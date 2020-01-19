#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "http_request_parser.hh"
#include "http_response_parser.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;
using t = Log::Category;

void program_body()
{
  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  FileDescriptor standard_output { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };
  standard_output.set_blocking( false );

  timer().start<t::DNS>();
  Address addr { "cs.stanford.edu", "443" };
  timer().stop<t::DNS>();

  timer().start<t::Nonblock>();
  tcp_sock.connect( addr );
  timer().stop<t::Nonblock>();

  SSLContext ssl_context;
  SSLSession ssl { ssl_context.make_SSL_handle(), move( tcp_sock ) };

  HTTPResponseParser responses;

  queue<HTTPRequest> requests;
  requests.push( { "GET /~keithw/ HTTP/1.1", { { "Host", "cs.stanford.edu" } }, "" } );
  responses.new_request_arrived( requests.back() );

  requests.push(
    { "GET /~keithw/nonexist HTTP/1.1", { { "Host", "cs.stanford.edu" }, { "Connection", "close" } }, "" } );
  responses.new_request_arrived( requests.back() );

  EventLoop event_loop;

  event_loop.add_rule(
    ssl.socket(),
    Direction::In,
    [&] { ssl.do_read(); },
    [&] { return ssl.want_read(); },
    [] {},
    [&] { ssl.socket().throw_if_error(); } );

  event_loop.add_rule(
    ssl.socket(),
    Direction::Out,
    [&] { ssl.do_write(); },
    [&] { return ssl.want_write(); },
    [] {},
    [&] { ssl.socket().throw_if_error(); } );

  string current_request;
  string_view current_request_unsent;

  do {
    while ( ( not ssl.outbound_plaintext().writable_region().empty() ) and
            ( ( not current_request_unsent.empty() ) or ( not requests.empty() ) ) ) {
      if ( current_request_unsent.empty() ) {
        assert( not requests.empty() );
        current_request = requests.front().serialize();
        requests.pop();
        current_request_unsent = current_request;
      }

      current_request_unsent.remove_prefix( ssl.outbound_plaintext().read_from( current_request_unsent ) );
    }

    if ( ( not ssl.inbound_plaintext().readable_region().empty() ) and ( responses.can_parse() ) ) {
      ssl.inbound_plaintext().pop( responses.parse( ssl.inbound_plaintext().readable_region() ) );
    }

    while ( not responses.empty() ) {
      cerr << "Response received: " << responses.front().first_line() << "\n";
      responses.pop();
    }
  } while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit );
}

int main()
{
  try {
    program_body();
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

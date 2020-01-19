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

  queue<HTTPRequest> requests;
  requests.emplace();
  requests.back().set_first_line( "GET /~keithw/ HTTP/1.1" );
  requests.back().add_header( { "Host", "cs.stanford.edu" } );
  requests.back().done_with_headers();
  requests.back().read_in_body( "" );

  requests.emplace();
  requests.back().set_first_line( "GET /~keithw/nonexist HTTP/1.1" );
  requests.back().add_header( { "Host", "cs.stanford.edu" } );
  requests.back().add_header( { "Connection", "close" } );
  requests.back().done_with_headers();
  requests.back().read_in_body( "" );

  string text_request = requests.front().str();
  requests.pop();
  text_request.append( requests.front().str() );

  string_view text_request_view { text_request };

  HTTPResponseParser responses;

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

  event_loop.add_rule(
    standard_output,
    Direction::Out,
    [&] { ssl.inbound_plaintext().write_to( standard_output ); },
    [&] { return !ssl.inbound_plaintext().readable_region().empty(); } );

  do {
    if ( ( not text_request_view.empty() ) and ( not ssl.outbound_plaintext().writable_region().empty() ) ) {
      text_request_view.remove_prefix( ssl.outbound_plaintext().read_from( text_request_view ) );
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

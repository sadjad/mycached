#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "http_client.hh"
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

  HTTPClient http;
  http.push_request( { "GET /~keithw/ HTTP/1.1", { { "Host", "cs.stanford.edu" } }, "" } );
  http.push_request( { "GET /~keithw/nonexist HTTP/1.1", { { "Host", "cs.stanford.edu" } }, "" } );
  http.push_request( { "GET /~keithw/ HTTP/1.1", { { "Host", "cs.stanford.edu" }, { "Connection", "close" } }, "" } );

  EventLoop event_loop;

  event_loop.add_rule(
    ssl.socket(), Direction::In, [&] { ssl.do_read(); }, [&] { return ssl.want_read(); } );

  event_loop.add_rule(
    ssl.socket(), Direction::Out, [&] { ssl.do_write(); }, [&] { return ssl.want_write(); } );

  do {
    while ( ( not ssl.outbound_plaintext().writable_region().empty() ) and ( not http.requests_empty() ) ) {
      http.write( ssl.outbound_plaintext() );
    }

    if ( ( not ssl.inbound_plaintext().readable_region().empty() ) ) {
      http.read( ssl.inbound_plaintext() );
    }

    while ( not http.responses_empty() ) {
      cerr << "Response received: " << http.responses_front().first_line() << "\n";
      http.pop_response();
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

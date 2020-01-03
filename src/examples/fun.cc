#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;
using t = Log::Category;

void program_body()
{
  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  FileDescriptor standard_input { STDIN_FILENO };
  standard_input.set_blocking( false );

  FileDescriptor standard_output { STDOUT_FILENO };
  standard_output.set_blocking( false );

  timer().start<t::DNS>();
  Address addr { "cs.stanford.edu", "443" };
  timer().stop<t::DNS>();

  timer().start<t::Nonblock>();
  tcp_sock.connect( addr );
  timer().stop<t::Nonblock>();

  SSLContext ssl_context;
  SSLSession ssl { ssl_context.make_SSL_handle() };

  {
    bool connected = false;
    EventLoop event_loop;
    event_loop.add_rule(
      tcp_sock,
      Direction::Out,
      [&] {
        tcp_sock.throw_if_error();
        tcp_sock.peer_address();
        connected = true;
      },
      [&] { return not connected; },
      [] {},
      [&] { tcp_sock.throw_if_error(); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
  }

  const string query = "GET /~keithw/ HTTP/1.1\r\nHost: cs.stanford.edu\r\n\r\nGET /~keithw/test.txt HTTP/1.1\r\nHost: "
                       "cs.stanford.edu\r\nConnection: close\r\n\r\n";

  ssl.outbound_plaintext().wrote( ssl.outbound_plaintext().writable_region().copy( query ) );

  {
    EventLoop event_loop;

    event_loop.add_rule(
      tcp_sock,
      Direction::In,
      [&] { ssl.inbound_ciphertext().wrote( tcp_sock.read( ssl.inbound_ciphertext().writable_region() ) ); },
      [&] { return !ssl.inbound_ciphertext().writable_region().empty(); },
      [] {},
      [&] { tcp_sock.throw_if_error(); } );

    event_loop.add_rule(
      tcp_sock,
      Direction::Out,
      [&] { ssl.outbound_ciphertext().pop( tcp_sock.write( ssl.outbound_ciphertext().readable_region() ) ); },
      [&] { return !ssl.outbound_ciphertext().readable_region().empty(); },
      [] {},
      [&] { tcp_sock.throw_if_error(); } );

    /*
    event_loop.add_rule(
      standard_input,
      Direction::In,
      [&] { ssl.outbound_plaintext().wrote( standard_input.read( ssl.outbound_plaintext().writable_region() ) ); },
      [&] { return !ssl.outbound_plaintext().writable_region().empty(); } );
    */

    event_loop.add_rule(
      standard_output,
      Direction::Out,
      [&] { ssl.inbound_plaintext().pop( standard_output.write( ssl.inbound_plaintext().readable_region() ) ); },
      [&] { return !ssl.inbound_plaintext().readable_region().empty(); } );

    do {
      ssl.do_work();
    } while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit );
  }
}

int main()
{
  try {
    program_body();
    cerr << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    cerr << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

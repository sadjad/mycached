#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "secure_socket.hh"
#include "timer.hh"

using namespace std;
using t = Log::Category;

void program_body()
{
  timer().start<t::SSL>();
  SSLContext ssl_context;
  timer().stop<t::SSL>();

  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  timer().start<t::DNS>();
  Address addr { "localhost", "9090" };
  timer().stop<t::DNS>();

  timer().start<t::Nonblock>();
  tcp_sock.connect( addr );
  timer().stop<t::Nonblock>();

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

  string buf;
  buf.resize( 256 );
  simple_string_span available_buffer_span { buf };

  {
    EventLoop event_loop;
    event_loop.add_rule(
      tcp_sock,
      Direction::In,
      [&] {
        size_t amount_read = tcp_sock.read( available_buffer_span );
        available_buffer_span.remove_prefix( amount_read );
        cerr << "Read " << amount_read << " bytes, available space now " << available_buffer_span.size() << "\n";
      },
      [&] { return available_buffer_span.size() > 0; },
      [] {},
      [&] { tcp_sock.throw_if_error(); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
  }
}

int main()
{
  try {
    program_body();
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    cerr << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "nb_secure_socket.hh"
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
  Address addr { "cs.stanford.edu", "443" };
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

    timer().start<t::WaitingToConnect>();
    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
    timer().stop<t::WaitingToConnect>();
  }

  timer().start<t::SSL>();
  NBSecureSocket ssl_sock { ssl_context.new_secure_socket( move( tcp_sock ) ) };
  timer().stop<t::SSL>();

  timer().start<t::Nonblock>();
  ssl_sock.connect();
  timer().stop<t::Nonblock>();

  {
    EventLoop event_loop;
    event_loop.add_rule(
      ssl_sock,
      Direction::Out,
      [&] { ssl_sock.continue_SSL_connect(); },
      [&] { return ssl_sock.needs_write() and not ssl_sock.connected(); } );

    event_loop.add_rule(
      ssl_sock,
      Direction::In,
      [&] { ssl_sock.continue_SSL_connect(); },
      [&] { return ssl_sock.needs_read() and not ssl_sock.connected(); } );

    timer().start<t::WaitingToConnect>();
    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
    timer().stop<t::WaitingToConnect>();

    if ( not ssl_sock.connected() ) {
      throw runtime_error( "did not connect SSL socket" );
    }
  }

  ssl_sock.ezwrite( "GET /~keithw/ HTTP/1.1\n\n" );
}

int main()
{
  try {
    timer();
    program_body();
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    cerr << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

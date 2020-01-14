#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

void program_body( const string& name )
{
  const uint64_t start_time = timestamp_ns();

  UDPSocket sock;
  sock.set_blocking( false );

  sock.bind( { "0", 5050 } );

  Address trolley { "171.67.76.46", 9090 };

  Address bound_to = sock.local_address();

  bool message_sent = false;
  bool reply_received = false;

  EventLoop event_loop;
  event_loop.add_rule(
    sock,
    Direction::Out,
    [&] {
      sock.sendto( trolley, "hello! I am bound to " + bound_to.to_string() );
      message_sent = true;
    },
    [&] { return message_sent == false; },
    [] {},
    [&] { sock.throw_if_error(); } );
  event_loop.add_rule(
    sock,
    Direction::In,
    [&] {
      auto rec = sock.recv();
      cout << "Datagram received from " << rec.source_address.to_string() << ": " << rec.payload << "\n";
      reply_received = true;
    },
    [&] { return reply_received == false; },
    [] {},
    [&] { sock.throw_if_error(); } );

  while ( event_loop.wait_next_event( 500 ) != EventLoop::Result::Exit ) {
    if ( timestamp_ns() - start_time > 5ULL * 1000 * 1000 * 1000 ) {
      cout << "Timeout\n";
      return;
    }
  }
}

int main( const int argc, const char* const argv[] )
{
  try {
    timer();

    if ( argc != 2 ) {
      throw runtime_error( "Usage: udp-reflect IDENTITY" );
    }

    program_body( argv[1] );
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

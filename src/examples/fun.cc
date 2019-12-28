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
  Address addr { "cs.stanford.edu", "https" };
  timer().stop<t::DNS>();

  timer().start<t::Nonblock>();
  tcp_sock.connect( addr );
  timer().stop<t::Nonblock>();

  bool connected = false;

  {
    EventLoop event_loop;
    event_loop.add_rule(
      tcp_sock,
      Direction::Out,
      [&] {
        tcp_sock.throw_if_error();
        connected = true;
      },
      [&] { return not connected; } );

    timer().start<t::WaitingToConnect>();
    event_loop.wait_next_event( -1 );
    timer().stop<t::WaitingToConnect>();
  }

  cout << tcp_sock.local_address().to_string() << " -> " << tcp_sock.peer_address().to_string() << "\n";
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
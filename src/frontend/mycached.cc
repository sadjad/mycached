#include <cstdlib>
#include <iostream>
#include <unordered_map>

#include "util/eventloop.hh"
#include "util/exception.hh"
#include "util/socket.hh"

using namespace std;

void usage( char* argv0 )
{
  cerr << "Usage: " << argv0 << " PORT" << endl;
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      usage( argv[0] );
      return EXIT_FAILURE;
    }

    EventLoop event_loop;

    const uint16_t port = static_cast<uint16_t>( stoi( argv[1] ) );
    const Address listen_address { "0.0.0.0", port };

    TCPSocket listen_sock;
    listen_sock.set_blocking( false );
    listen_sock.bind( listen_address );
    listen_sock.listen();

    event_loop.add_rule( "TCP Listener",
                         listen_sock,
                         Direction::In,
                         [&listen_sock]() {
                           TCPSocket sock = listen_sock.accept();
                           cerr << "incoming connection: "
                                << sock.peer_address().to_string() << endl;
                         },
                         []() { return true; },
                         []() { throw runtime_error( "error" ); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit )
      ;

  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}

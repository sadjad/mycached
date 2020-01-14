#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

void program_body()
{
  UDPSocket sock;
  sock.bind( { "0", 9090 } );

  while ( true ) {
    auto rec = sock.recv();
    cout << "Datagram received from " << rec.source_address.to_string() << ": " << rec.payload << "\n";

    sock.sendto( rec.source_address, "nice to meet you, " + rec.source_address.to_string() );
  }
}

int main()
{
  try {
    timer();
    program_body();
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

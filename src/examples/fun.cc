#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "exception.hh"
#include "nb_secure_socket.hh"
#include "timer.hh"

using namespace std;
using t = Log::Category;

void program_body()
{
  global_timer().start<t::SSL>();
  SSLContext ssl_context;
  global_timer().stop<t::SSL>();

  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  global_timer().start<t::DNS>();
  Address addr { "cs.stanford.edu", "https" };
  global_timer().stop<t::DNS>();
}

int main()
{
  try {
    global_timer();
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

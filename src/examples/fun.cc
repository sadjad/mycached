#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "exception.hh"
#include "nb_secure_socket.hh"

using namespace std;

void program_body()
{
  SSLContext ssl_context;

  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

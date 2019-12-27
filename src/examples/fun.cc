#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "exception.hh"

using namespace std;

void program_body()
{
  CheckSystemCall( "hello", write( STDOUT_FILENO, "hi", 3 ) );
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

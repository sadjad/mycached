#include <cstdlib>
#include <iostream>
#include <random>

#include "ring_buffer.hh"

using namespace std;

void program_body()
{
  RingBuffer rb { 65536 };

  for ( unsigned int iteration = 0; iteration < 2833; iteration++ ) {
    string data;
    data.resize( 641 );

    for ( auto& ch : data ) {
      ch = random_device()();
    }

    rb.wrote( rb.writable_region().copy( data ) );

    const string data2 { rb.readable_region() };
    rb.pop( data2.size() );

    if ( data != data2 ) {
      throw runtime_error( "mismatch" );
    }
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

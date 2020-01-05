#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>

#include "ring_buffer.hh"

using namespace std;

void rb_test( const size_t rb_size, const size_t iteration_count, const size_t block )
{
  RingBuffer rb { rb_size };

  for ( unsigned int iteration = 0; iteration < iteration_count; iteration++ ) {
    string data;
    data.resize( block );

    for ( auto& ch : data ) {
      ch = random_device()();
    }

    rb.push( rb.writable_region().copy( data ) );

    if ( block == rb_size ) {
      assert( rb.writable_region().size() == 0 );
    }

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
    rb_test( 65536, 800, 641 );
    rb_test( 4096, 32, 4096 );
    rb_test( 8192, 32, 4096 );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

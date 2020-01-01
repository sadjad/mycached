#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "ring_buffer.hh"
#include "timer.hh"

using namespace std;

void program_body()
{
  FileDescriptor in { STDIN_FILENO };
  in.set_blocking( false );

  FileDescriptor out { STDOUT_FILENO };
  out.set_blocking( false );

  RingBuffer buf { 1048576 };

  {
    EventLoop event_loop;
    event_loop.add_rule(
      in,
      Direction::In,
      [&] { buf.wrote( in.read( buf.writable_region() ) ); },
      [&] { return !buf.writable_region().empty(); } );

    event_loop.add_rule(
      out,
      Direction::Out,
      [&] { buf.pop( out.write( buf.readable_region() ) ); },
      [&] { return !buf.readable_region().empty(); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
  }
}

int main()
{
  try {
    program_body();
    cerr << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    cerr << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

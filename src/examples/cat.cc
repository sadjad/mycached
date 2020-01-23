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
  FileDescriptor in { CheckSystemCall( "dup", dup( STDIN_FILENO ) ) };
  in.set_blocking( false );

  FileDescriptor out { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };
  out.set_blocking( false );

  RingBuffer buf { 1048576 };

  {
    EventLoop event_loop;
    event_loop.add_rule(
      "read from stdin",
      in,
      Direction::In,
      [&] { buf.read_from( in ); },
      [&] { return !buf.writable_region().empty(); } );

    event_loop.add_rule(
      "write to stdout",
      out,
      Direction::Out,
      [&] { buf.write_to( out ); },
      [&] { return !buf.readable_region().empty(); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
  }
}

int main()
{
  try {
    timer();
    program_body();
    cerr << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    cerr << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

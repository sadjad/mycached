#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>

#include <unistd.h>

#include "build-id.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

const uint64_t BILLION { 1'000'000'000 };

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  bool in_double_quoted_string = false;
  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

void program_body( const string& id )
{
  const uint64_t start_time = timestamp_ns();

  cerr << "Build ID: " << build_id_hex() << "\n";

  UDPSocket sock;
  sock.set_blocking( false );

  Address trolley { "171.67.76.46", 9090 };
  optional<Address> other_address;

  const string other = ( id == "client" ? "server" : "client" );

  EventLoop event_loop;

  uint64_t next_announce_time = start_time;
  event_loop.add_rule(
    sock,
    Direction::Out,
    [&] {
      sock.sendto( trolley, "= " + id );
      next_announce_time = timestamp_ns() + BILLION;
    },
    [&] { return next_announce_time < timestamp_ns(); },
    [] {},
    [&] { sock.throw_if_error(); } );

  event_loop.add_rule(
    sock,
    Direction::In,
    [&] {
      auto rec = sock.recv();
      sock.sendto( trolley,
                   "INFO " + id + " received datagram from " + rec.source_address.to_string() + ": " + rec.payload );

      vector<string_view> fields;
      split_on_char( rec.payload, ' ', fields );
      if ( fields.size() == 4 and fields.at( 0 ) == "=" and fields.at( 1 ) == other ) {
        other_address.emplace( string( fields.at( 2 ) ), string( fields.at( 3 ) ) );
        sock.sendto( trolley, "INFO " + id + " learned mapping other => " + other_address.value().to_string() );
      }
    },
    [&] { return true; },
    [] {},
    [&] { sock.throw_if_error(); } );

  uint64_t next_call_time = start_time;
  event_loop.add_rule(
    sock,
    Direction::Out,
    [&] {
      sock.sendto( trolley, "INFO sending request to other" );
      sock.sendto( other_address.value(), "REQUEST from " + id );
      next_call_time = timestamp_ns() + BILLION / 2;
    },
    [&] { return next_call_time < timestamp_ns() and other_address.has_value(); },
    [] {},
    [&] { sock.throw_if_error(); } );

  while ( event_loop.wait_next_event( 500 ) != EventLoop::Result::Exit ) {
    if ( timestamp_ns() - start_time > 5ULL * 1000 * 1000 * 1000 ) {
      cout << id << " timeout\n";
      return;
    }
  }
}

int main( const int argc, const char* const argv[] )
{
  try {
    timer();

    if ( argc != 2 ) {
      throw runtime_error( "Usage: udp-reflect client|server" );
    }

    program_body( argv[1] );
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

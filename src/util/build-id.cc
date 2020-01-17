#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "build-id.hh"

extern const char build_id_start;
extern const char build_id_end;

using namespace std;

constexpr unsigned int HEADER_LENGTH = 16;
constexpr unsigned int SHA_1_LENGTH = 20;

#pragma GCC diagnostic ignored "-Warray-bounds"

string_view build_id()
{
  if ( &build_id_end != &build_id_start + HEADER_LENGTH + SHA_1_LENGTH ) {
    throw runtime_error( "no SHA-1 build-id available" );
  }
  return { &build_id_start + HEADER_LENGTH, SHA_1_LENGTH };
}

string build_id_hex()
{
  const auto id = build_id();
  stringstream out;
  out << hex << setw( 2 ) << setfill( '0' );
  for ( const auto x : id ) {
    out << static_cast<unsigned int>( static_cast<uint8_t>( x ) );
  }
  return out.str();
}

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "elf-info.hh"

extern const char __build_id_start;
extern const char __build_id_end;

extern const char __interp_start;
extern const char __interp_end;

using namespace std;

constexpr unsigned int HEADER_LENGTH = 16;
constexpr unsigned int SHA_1_LENGTH = 20;

#pragma GCC diagnostic ignored "-Warray-bounds"

string_view build_id()
{
  if ( &__build_id_end != &__build_id_start + HEADER_LENGTH + SHA_1_LENGTH ) {
    throw runtime_error( "no SHA-1 build-id available" );
  }
  return { &__build_id_start + HEADER_LENGTH, SHA_1_LENGTH };
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

string_view interpreter()
{
  if ( &__interp_start == &__interp_end ) {
    return {};
  }

  const size_t length = &__interp_end - &__interp_start;

  return { &__interp_start, length };
}

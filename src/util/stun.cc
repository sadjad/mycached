#include "stun.hh"
#include "random.hh"
#include "timer.hh"

#include <iomanip>
#include <iostream>

using namespace std;

void STUNClient::expire_pending_requests()
{
  const uint64_t now = Timer::timestamp_ns();

  for ( auto it = pending_requests_.begin(); it != pending_requests_.end(); ) {
    if ( it->second < now ) {
      it = pending_requests_.erase( it );
    } else {
      ++it;
    }
  }
}

string STUNClient::make_binding_request()
{
  expire_pending_requests();

  string transaction_id;

  for ( unsigned int i = 0; i < 16; i++ ) {
    transaction_id.push_back( dist_( rng_ ) );
  }

  pending_requests_.emplace( transaction_id, Timer::timestamp_ns() + uint64_t( 5 ) * 1000 * 1000 * 1000 );

  return string( "\x00\x01\x00\x00", 4 ) + transaction_id;
}

optional<Address> STUNClient::process_binding_response( const string_view buffer )
{
  expire_pending_requests();

  if ( buffer.size() != 32 ) {
    return {};
  }

  if ( buffer.at( 0 ) != 1 or buffer.at( 1 ) != 1 ) { /* binding response */
    return {};
  }

  if ( buffer.at( 2 ) != 0 or buffer.at( 3 ) != 12 ) { /* message length = 12 bytes */
    return {};
  }

  const string_view transaction_id = buffer.substr( 4, 16 );
  const auto it = pending_requests_.find( transaction_id );
  if ( it == pending_requests_.end() ) {
    return {};
  }

  pending_requests_.erase( it );

  if ( buffer.at( 20 ) != 0 or buffer.at( 21 ) != 1 ) { /* MAPPED-ADDRESS */
    return {};
  }

  if ( buffer.at( 22 ) != 0 or buffer.at( 23 ) != 8 ) { /* attribute length */
    return {};
  }

  if ( buffer.at( 24 ) != 0 or buffer.at( 25 ) != 1 ) { /* IPv4 */
    return {};
  }

  sockaddr_in address;
  address.sin_family = AF_INET;
  memcpy( &address.sin_port, buffer.data() + 26, 2 );
  memcpy( &address.sin_addr, buffer.data() + 28, 4 );

  return Address { reinterpret_cast<const sockaddr*>( &address ), sizeof( address ) };
}

bool STUNClient::has_pending_requests()
{
  expire_pending_requests();
  return not pending_requests_.empty();
}

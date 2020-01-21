#include "stun.hh"

extern "C"
{
#include <stun/usages/bind.h>
}

using namespace std;

STUNClient::STUNClient()
  : agent_()
{
  stun_agent_init(
    &agent_, STUN_ALL_KNOWN_ATTRIBUTES, STUN_COMPATIBILITY_RFC3489, STUN_AGENT_USAGE_IGNORE_CREDENTIALS );
}

string_view STUNClient::make_binding_request()
{
  StunMessage message;

  scratch_.resize( 64 );

  const size_t len
    = stun_usage_bind_create( &agent_, &message, reinterpret_cast<uint8_t*>( scratch_.data() ), scratch_.size() );

  return { scratch_.data(), len };
}

optional<Address> STUNClient::process_binding_response( const string_view buffer )
{
  StunMessage message;
  if ( STUN_VALIDATION_SUCCESS
       != stun_agent_validate(
         &agent_, &message, reinterpret_cast<const uint8_t*>( buffer.data() ), buffer.size(), nullptr, nullptr ) ) {
    return {};
  }

  sockaddr_storage address;
  socklen_t address_length = sizeof( address );

  sockaddr_storage alternate_server;
  socklen_t alternate_server_length = sizeof( address );

  if ( STUN_USAGE_BIND_RETURN_SUCCESS
       != stun_usage_bind_process( &message,
                                   reinterpret_cast<sockaddr*>( &address ),
                                   &address_length,
                                   reinterpret_cast<sockaddr*>( &alternate_server ),
                                   &alternate_server_length ) ) {
    return {};
  }

  /* don't allow IPv6 for now */
  if ( address_length != sizeof( sockaddr_in ) or address.ss_family != AF_INET ) {
    return {};
  }

  return Address { reinterpret_cast<sockaddr*>( &address ), address_length };
}

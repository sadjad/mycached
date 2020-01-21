#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C"
{
#include <stun/stunagent.h>
}

#include "address.hh"
#include "socket.hh"

class STUNClient
{
  StunAgent agent_;
  std::string scratch_ { 64, 'x' };

public:
  STUNClient();

  std::string_view make_binding_request();
  std::optional<Address> process_binding_response( const std::string_view buffer );
};

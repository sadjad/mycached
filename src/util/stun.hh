#pragma once

#include <map>
#include <optional>
#include <string>

#include "address.hh"
#include "random.hh"
#include "socket.hh"

class STUNClient
{
  std::mt19937 rng_ = get_random_generator();
  std::uniform_int_distribution<> dist_ { 0, 255 };

  struct compare_strings : public std::less<std::string_view>
  {
    using is_transparent = void;
  };

  std::map<std::string, uint64_t, compare_strings> pending_requests_ {};

  void expire_pending_requests();

public:
  std::string make_binding_request();
  std::optional<Address> process_binding_response( const std::string_view buffer );

  bool has_pending_requests();
};

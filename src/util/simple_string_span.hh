#pragma once

#include <string_view>

class simple_string_span : public std::string_view
{
public:
  simple_string_span( std::string_view sv )
    : std::string_view( sv )
  {}

  char* mutable_data() { return const_cast<char*>( data() ); }
};

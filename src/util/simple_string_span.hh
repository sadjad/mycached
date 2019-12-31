#pragma once

#include <string_view>

class simple_string_span : public std::string_view
{
public:
  using std::string_view::string_view;

  char* mutable_data() { return const_cast<char*>( data() ); }
};

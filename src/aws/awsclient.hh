#pragma once

#include <string_view>

#include "address.hh"
#include "eventloop.hh"
#include "http_client.hh"
#include "secure_socket.hh"

class AWSClient
{
  std::string endpoint_hostname_;
  Address endpoint_;
  SSLContext ssl_context_;
  SSLSession ssl_session_;
  HTTPClient http_;

public:
  AWSClient( const std::string& region );

  void install_rules( EventLoop& event_loop );

  void get_account_settings();
};

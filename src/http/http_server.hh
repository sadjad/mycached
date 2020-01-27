#pragma once

#include <queue>
#include <string>
#include <string_view>

#include "http_request_parser.hh"
#include "http_response.hh"
#include "ring_buffer.hh"

class HTTPServer
{
  HTTPRequestParser requests_ {};
  std::queue<HTTPResponse> responses_ {};

  std::string current_response_headers_ {};
  std::string_view current_response_unsent_headers_ {};
  std::string_view current_response_unsent_body_ {};

  void load()
  {
    if ( ( not current_response_unsent_headers_.empty() ) or ( not current_response_unsent_body_.empty() )
         or ( responses_.empty() ) ) {
      throw std::runtime_error( "HTTPServer cannot load new response" );
    }

    responses_.front().serialize_headers( current_response_headers_ );
    current_response_unsent_headers_ = current_response_headers_;
    current_response_unsent_body_ = responses_.front().body();
  }

public:
  void push_response( HTTPResponse&& res )
  {
    responses_.push( std::move( res ) );

    if ( current_response_unsent_headers_.empty() and current_response_unsent_body_.empty() ) {
      load();
    }
  }

  bool responses_empty() const
  {
    return current_response_unsent_headers_.empty() and current_response_unsent_body_.empty() and responses_.empty();
  }

  template<class Writable>
  void write( Writable& out )
  {
    if ( responses_empty() ) {
      throw std::runtime_error( "HTTPServer::write(): HTTPServer has no more responses" );
    }

    if ( not current_response_unsent_headers_.empty() ) {
      current_response_unsent_headers_.remove_prefix( out.write( current_response_unsent_headers_ ) );
    } else if ( not current_response_unsent_body_.empty() ) {
      current_response_unsent_body_.remove_prefix( out.write( current_response_unsent_body_ ) );
    } else {
      responses_.pop();
      if ( not responses_.empty() ) {
        load();
      }
    }
  }

  void read( RingBuffer& in ) { in.pop( requests_.parse( in.readable_region() ) ); }

  bool requests_empty() const { return requests_.empty(); }
  const HTTPRequest& requests_front() const { return requests_.front(); }
  void pop_request() { return requests_.pop(); }
};

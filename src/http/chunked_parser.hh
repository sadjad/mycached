#pragma once

#include "body_parser.hh"

class ChunkedBodyParser : public BodyParser
{
private:
  size_t compute_ack_size( const std::string_view haystack, const std::string_view needle, const size_t input_size );
  uint32_t get_chunk_size( const std::string_view chunk_hdr ) const;
  std::string parser_buffer_ { "" };
  uint32_t current_chunk_size_ { 0 };
  std::string::size_type acked_so_far_ { 0 };
  std::string::size_type parsed_so_far_ { 0 };
  enum
  {
    CHUNK_HDR,
    CHUNK,
    TRAILER
  } state_ { CHUNK_HDR };
  const bool trailers_enabled_ { false };

public:
  size_t read( const std::string_view input_buffer ) override;

  /* Follow item 2, Section 4.4 of RFC 2616 */
  bool eof() const override { return true; }

  ChunkedBodyParser( bool t_trailers_enabled )
    : trailers_enabled_( t_trailers_enabled )
  {}
};

#include <cassert>
#include <string>

#include "chunked_parser.hh"
#include "convert.hh"

using namespace std;

/* Take a chunk header and parse it assuming no folding */
uint32_t ChunkedBodyParser::get_chunk_size( const string_view chunk_hdr ) const
{
  /* Check that the chunk header ends with a CRLF */
  assert( chunk_hdr.substr( chunk_hdr.length() - 2, 2 ) == "\r\n" );

  /* If there are chunk extensions, ';' terminates chunk size */
  auto pos = chunk_hdr.find( ";" );

  /* There are no ';'s, and hence no chunk externsions, CRLF terminates chunk size */
  if ( pos == string::npos ) {
    pos = chunk_hdr.find( "\r\n" );
  }

  /* Can't be npos even now */
  assert( pos != string::npos );

  /* Parse hex string */
  auto hex_string = chunk_hdr.substr( 0, pos );
  return to_uint64( hex_string, 16 );
}

size_t ChunkedBodyParser::read( const std::string_view input_buffer )
{
  parser_buffer_ += input_buffer;

  while ( !parser_buffer_.empty() ) {
    switch ( state_ ) {
      case CHUNK_HDR: {
        auto it = parser_buffer_.find( "\r\n" );
        if ( it != string::npos ) {
          /* if you have CRLF, get chunk size & transition to CHUNK/TRAILER */
          current_chunk_size_ = get_chunk_size( parser_buffer_.substr( 0, it + 2 ) );

          /* Transition appropriately */
          state_ = ( current_chunk_size_ == 0 ) ? TRAILER : CHUNK;

          /* shrink parser_buffer_ */
          parsed_so_far_ += ( it + 2 );
          parser_buffer_ = parser_buffer_.substr( it + 2 );
          break;
        } else {
          /* if you haven't seen a CRLF so far, do nothing */
          acked_so_far_ += input_buffer.length();
          return string::npos;
        }
      }

      case CHUNK: {
        if ( parser_buffer_.length() >= current_chunk_size_ + 2 ) {
          /* accumulated enough bytes, check CRLF at the end of the chunk */
          assert( parser_buffer_.substr( current_chunk_size_, 2 ) == "\r\n" );

          /* Transition to next state */
          state_ = CHUNK_HDR;

          /* shrink parser_buffer_ */
          parsed_so_far_ += current_chunk_size_ + 2;
          parser_buffer_ = parser_buffer_.substr( current_chunk_size_ + 2 );
          break;
        } else {
          /* Haven't seen enough bytes so far, do nothing */
          acked_so_far_ += input_buffer.length();
          return string::npos;
        }
      }

      case TRAILER: {
        if ( trailers_enabled_ ) {
          /* We need two consecutive CRLFs */
          return compute_ack_size( parser_buffer_, "\r\n\r\n", input_buffer.length() );
        } else {
          /* We need only one CRLF now */
          return compute_ack_size( parser_buffer_, "\r\n", input_buffer.length() );
        }
      }

      default: {
        assert( false );
        return false;
      }
    }
  }
  acked_so_far_ += input_buffer.length();
  return string::npos;
}

/*
   Computes the acknowledgement from the BodyParser to its caller,
   telling it how much of the current input_buffer has been
   successfully parsed.
*/
size_t ChunkedBodyParser::compute_ack_size( const string_view haystack,
                                            const string_view needle,
                                            const size_t input_size )
{
  auto loc = haystack.find( needle );
  if ( loc != string::npos ) {
    /* Found it, eat up the whole buffer */
    parsed_so_far_ += loc + needle.length();
    assert( parsed_so_far_ > acked_so_far_ );
    return ( parsed_so_far_ - acked_so_far_ );
  } else {
    /* Find unacknowledged buffer so far, ack it, and be done */
    acked_so_far_ += input_size;
    return loc;
  }
}

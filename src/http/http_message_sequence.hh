#pragma once

#include <cassert>
#include <queue>
#include <string>

#include "http_message.hh"
#include "ring_buffer.hh"

template<class MessageType>
class HTTPMessageSequence
{
  static bool have_complete_line( const RingBuffer& rb )
  {
    size_t first_line_ending = rb.readable_region().find( CRLF );
    return first_line_ending != std::string::npos;
  }

  static std::string_view get_line( const RingBuffer& rb )
  {
    size_t first_line_ending = rb.readable_region().find( CRLF );
    assert( first_line_ending != std::string::npos );

    return rb.readable_region().substr( 0, first_line_ending );
  }

  /* complete messages ready to go */
  std::queue<MessageType> complete_messages_ {};

  /* one loop through the parser */
  /* returns whether to continue */
  bool parsing_step( RingBuffer& rb );

  /* what to do to create a new message.
     must be implemented by subclass */
  virtual void initialize_new_message() = 0;

protected:
  /* the current message we're working on */
  MessageType message_in_progress_ {};

public:
  HTTPMessageSequence() {}
  virtual ~HTTPMessageSequence() {}

  void parse( RingBuffer& rb )
  {
    if ( rb.readable_region().empty() ) { /* EOF */
      message_in_progress_.eof();
    }

    /* parse as much as we can */
    while ( parsing_step( rb ) ) {
    }
  }

  /* getters */
  bool empty() const { return complete_messages_.empty(); }
  const MessageType& front() const { return complete_messages_.front(); }

  /* pop one request */
  void pop() { complete_messages_.pop(); }
};

template<class MessageType>
bool HTTPMessageSequence<MessageType>::parsing_step( RingBuffer& rb )
{
  switch ( message_in_progress_.state() ) {
    case FIRST_LINE_PENDING:
      /* do we have a complete line? */
      if ( not have_complete_line( rb ) ) {
        return false;
      }

      /* supply status line to request/response initialization routine */
      initialize_new_message();

      message_in_progress_.set_first_line( get_line( rb ) );
      rb.pop( message_in_progress_.first_line().size() + 2 );

      return true;
    case HEADERS_PENDING:
      /* do we have a complete line? */
      if ( not have_complete_line( rb ) ) {
        return false;
      }

      /* is line blank? */
      {
        std::string_view line { get_line( rb ) };
        if ( line.empty() ) {
          message_in_progress_.done_with_headers();
        } else {
          message_in_progress_.add_header( line );
        }
        rb.pop( line.size() + 2 );
      }
      return true;

    case BODY_PENDING: {
      size_t bytes_read = message_in_progress_.read_in_body( rb.readable_region() );
      assert( bytes_read == rb.readable_region().size() or message_in_progress_.state() == COMPLETE );
      rb.pop( bytes_read );
    }
      return message_in_progress_.state() == COMPLETE;

    case COMPLETE:
      complete_messages_.emplace( std::move( message_in_progress_ ) );
      message_in_progress_ = MessageType();
      return true;
  }

  assert( false );
  return false;
}

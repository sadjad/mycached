#include "secure_socket.hh"
#include "exception.hh"

using namespace std;

void OpenSSL::check_errors( const std::string_view context, const bool must_have_error )
{
  unsigned long first_error = ERR_get_error();

  if ( first_error == 0 ) { // no error
    if ( must_have_error ) {
      throw runtime_error( "SSL error, but nothing on error queue" );
    } else {
      return;
    }
  }

  unsigned long next_error = ERR_get_error();

  if ( next_error == 0 ) {
    throw ssl_error( context, first_error );
  } else {
    string errors = ERR_error_string( first_error, nullptr );
    errors.append( ", " );
    errors.append( ERR_error_string( next_error, nullptr ) );

    while ( unsigned long another_error = ERR_get_error() ) {
      errors.append( ", " );
      errors.append( ERR_error_string( another_error, nullptr ) );
    }

    throw runtime_error( "multiple SSL errors: " + errors );
  }
}

SSL_CTX* initialize_new_client_context()
{
  SSL_CTX* ret = SSL_CTX_new( TLS_client_method() );
  if ( not ret ) {
    OpenSSL::throw_error( "SSL_CTL_new" );
  }
  return ret;
}

SSLContext::SSLContext()
  : ctx_( initialize_new_client_context() )
{}

SSL_handle SSLContext::make_SSL_handle()
{
  SSL_handle ssl { SSL_new( ctx_.get() ) };
  if ( not ssl ) {
    OpenSSL::throw_error( "SSL_new" );
  }
  return ssl;
}

int ringbuffer_BIO_write( BIO* bio, const char* const buf, const int len )
{
  RingBufferBIO* rb = reinterpret_cast<RingBufferBIO*>( BIO_get_data( bio ) );
  if ( not rb ) {
    throw runtime_error( "BIO_get_data returned nullptr" );
  }

  if ( len < 0 ) {
    throw runtime_error( "ringbuffer_BIO_write: len < 0" );
  }

  const string_view data_to_write( buf, len );
  const size_t bytes_written = rb->writable_region().copy( data_to_write );
  rb->wrote( bytes_written );

  if ( bytes_written == 0 ) {
    BIO_set_retry_write( bio );
  }

  return bytes_written;
}

int ringbuffer_BIO_read( BIO* bio, char* const buf, const int len )
{
  RingBufferBIO* rb = reinterpret_cast<RingBufferBIO*>( BIO_get_data( bio ) );
  if ( not rb ) {
    throw runtime_error( "BIO_get_data returned nullptr" );
  }

  if ( len < 0 ) {
    throw runtime_error( "ringbuffer_BIO_write: len < 0" );
  }

  simple_string_span data_to_read( buf, len );
  const size_t bytes_read = data_to_read.copy( rb->readable_region() );
  rb->pop( bytes_read );

  if ( bytes_read == 0 ) {
    BIO_set_retry_read( bio );
  }

  return bytes_read;
}

long ringbuffer_BIO_ctrl( BIO*, const int cmd, const long, void* const )
{
  if ( cmd == BIO_CTRL_FLUSH ) {
    return 1;
  }

  return 0;
}

BIO_METHOD* make_method( const string& name )
{
  const int new_type = BIO_get_new_index();
  if ( new_type == -1 ) {
    OpenSSL::throw_error( "BIO_get_new_index" );
  }

  BIO_METHOD* const ret = BIO_meth_new( new_type, name.c_str() );
  if ( not ret ) {
    OpenSSL::throw_error( "BIO_meth_new" );
  }

  return ret;
}

RingBufferBIO::Method::Method()
  : method_( make_method( "RingBuffer" ) )
{
  if ( not BIO_meth_set_write( method_.get(), ringbuffer_BIO_write ) ) {
    OpenSSL::throw_error( "BIO_meth_set_write" );
  }

  if ( not BIO_meth_set_read( method_.get(), ringbuffer_BIO_read ) ) {
    OpenSSL::throw_error( "BIO_meth_set_read" );
  }

  if ( not BIO_meth_set_ctrl( method_.get(), ringbuffer_BIO_ctrl ) ) {
    OpenSSL::throw_error( "BIO_meth_set_ctrl" );
  }
}

const BIO_METHOD* RingBufferBIO::Method::method()
{
  static Method method;
  return method.method_.get();
}

RingBufferBIO::RingBufferBIO( const size_t capacity )
  : RingBuffer( capacity )
  , bio_( BIO_new( Method::method() ) )
{
  if ( not bio_ ) {
    OpenSSL::throw_error( "BIO_new" );
  }

  BIO_set_data( bio_.get(), this );
  BIO_up_ref( bio_.get() );

  OpenSSL::check( "RingBufferBIO constructor" );
}

SSLSession::SSLSession( SSL_handle&& ssl )
  : ssl_( move( ssl ) )
{
  if ( not ssl_ ) {
    throw runtime_error( "SecureSocket: constructor must be passed valid SSL structure" );
  }

  SSL_set_mode( ssl_.get(), SSL_MODE_AUTO_RETRY );
  SSL_set_mode( ssl_.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER );
  SSL_set_mode( ssl_.get(), SSL_MODE_ENABLE_PARTIAL_WRITE );

  SSL_set0_rbio( ssl_.get(), inbound_ciphertext_ );
  SSL_set0_wbio( ssl_.get(), outbound_ciphertext_ );

  SSL_set_connect_state( ssl_.get() );

  OpenSSL::check( "SSLSession constructor" );
}

int SSLSession::get_error( const int return_value ) const
{
  return SSL_get_error( ssl_.get(), return_value );
}

bool SSLSession::should_read() const
{
  return ( not read_waiting_on_write_ ) and ( not inbound_ciphertext_.readable_region().empty() ) and
         ( not inbound_plaintext_.writable_region().empty() );
}

bool SSLSession::should_write() const
{
  return ( not write_waiting_on_read_ ) and ( not outbound_plaintext_.readable_region().empty() ) and
         ( not outbound_ciphertext_.writable_region().empty() );
}

void SSLSession::do_work()
{
  while ( pending_work() ) {
    bool forward_progress = false;

    for ( unsigned int i = 0; i < 2; i++ ) {
      if ( should_read() ) {
        const size_t input_data_before = inbound_ciphertext_.readable_region().size();
        const size_t output_data_before = inbound_plaintext_.readable_region().size();
        do_read();
        const size_t input_data_after = inbound_ciphertext_.readable_region().size();
        const size_t output_data_after = inbound_plaintext_.readable_region().size();

        if ( ( input_data_after < input_data_before ) or ( output_data_after > output_data_before ) ) {
          forward_progress = true;
          write_waiting_on_read_ = false;
        }
      }

      if ( should_write() ) {
        const size_t input_data_before = outbound_plaintext_.readable_region().size();
        const size_t output_data_before = outbound_ciphertext_.readable_region().size();
        do_write();
        const size_t input_data_after = outbound_plaintext_.readable_region().size();
        const size_t output_data_after = outbound_ciphertext_.readable_region().size();

        if ( ( input_data_after < input_data_before ) or ( output_data_after > output_data_before ) ) {
          forward_progress = true;
          read_waiting_on_write_ = false;
        }
      }
    }

    if ( not forward_progress ) {
      throw runtime_error( "SSL read/write/read/write cycle made no forward progress" );
    }
  }
}

void SSLSession::do_read()
{
  OpenSSL::check( "SSLSession::do_read()" );

  simple_string_span target = inbound_plaintext_.writable_region();
  const int bytes_read = SSL_read( ssl_.get(), target.mutable_data(), target.size() );

  if ( bytes_read > 0 ) {
    inbound_plaintext_.wrote( bytes_read );
    return;
  }

  const int error_return = get_error( bytes_read );

  if ( bytes_read == 0 and error_return == SSL_ERROR_ZERO_RETURN ) {
    return;
  }

  if ( error_return == SSL_ERROR_WANT_WRITE ) {
    read_waiting_on_write_ = true;
    return;
  }

  if ( error_return == SSL_ERROR_WANT_READ ) {
    return;
  }

  OpenSSL::check( "SSL_read check" );
  throw ssl_error( "SSL_read", error_return );
}

void SSLSession::do_write()
{
  OpenSSL::check( "SSLSession::do_write()" );

  const string_view source = outbound_plaintext_.readable_region();
  const int bytes_written = SSL_write( ssl_.get(), source.data(), source.size() );

  if ( bytes_written > 0 ) {
    outbound_plaintext_.pop( bytes_written );
    return;
  }

  const int error_return = get_error( bytes_written );

  if ( error_return == SSL_ERROR_WANT_READ ) {
    write_waiting_on_read_ = true;
    return;
  }

  if ( error_return == SSL_ERROR_WANT_WRITE ) {
    return;
  }

  OpenSSL::check( "SSL_write check" );
  throw ssl_error( "SSL_write", error_return );
}

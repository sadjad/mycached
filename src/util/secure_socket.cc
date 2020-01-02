#include "secure_socket.hh"
#include "exception.hh"

using namespace std;

OpenSSL::OpenSSL()
{
  /* SSL initialization: Needs to be done exactly once */
  /* load algorithms/ciphers */
  SSL_library_init();
  OpenSSL_add_all_algorithms();

  /* load error messages */
  SSL_load_error_strings();
}

OpenSSL& OpenSSL::global_context()
{
  static OpenSSL os;
  return os;
}

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
    throw runtime_error( "multiple SSL errors" );
  }
}

SSL_CTX* initialize_new_client_context()
{
  OpenSSL::global_context();
  SSL_CTX* ret = SSL_CTX_new( TLS_client_method() );
  if ( not ret ) {
    OpenSSL::throw_error( "SSL_CTL_new" );
  }
  return ret;
}

SSLContext::SSLContext() : ctx_( initialize_new_client_context() ) {}

SSL_handle SSLContext::make_SSL_handle()
{
  SSL_handle ssl { SSL_new( ctx_.get() ) };
  if ( not ssl ) {
    OpenSSL::throw_error( "SSL_new" );
  }
  return ssl;
}

SSLSession::SSLSession( SSL_handle&& ssl ) : ssl_( move( ssl ) )
{
  if ( not ssl_ ) {
    throw runtime_error( "SecureSocket: constructor must be passed valid SSL structure" );
  }

  SSL_set_mode( ssl_.get(), SSL_MODE_AUTO_RETRY );
  SSL_set_mode( ssl_.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER );
  SSL_set_mode( ssl_.get(), SSL_MODE_ENABLE_PARTIAL_WRITE );

  OpenSSL::check( "SSLSession constructor" );
}

int SSLSession::get_error( const int return_value ) const
{
  return SSL_get_error( ssl_.get(), return_value );
}

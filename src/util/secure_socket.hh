#pragma once

#include <memory>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "exception.hh"
#include "ring_buffer.hh"

/* global OpenSSL behavior */
class ssl_error_category : public std::error_category
{
public:
  const char* name() const noexcept override { return "SSL"; }
  std::string message( const int ssl_error ) const noexcept override { return ERR_error_string( ssl_error, nullptr ); }
};

class ssl_error : public tagged_error
{
public:
  ssl_error( const std::string_view attempt, const int error_code )
    : tagged_error( ssl_error_category(), attempt, error_code )
  {}
};

class OpenSSL
{
  static void check_errors( const std::string_view context, const bool must_have_error );

public:
  OpenSSL();
  static OpenSSL& global_context();

  static void check( const std::string_view context ) { return check_errors( context, false ); }
  static void throw_error( const std::string_view context ) { return check_errors( context, true ); }
};

struct SSL_deleter
{
  void operator()( SSL* x ) const { SSL_free( x ); }
};
typedef std::unique_ptr<SSL, SSL_deleter> SSL_handle;

class SSLContext
{
  struct CTX_deleter
  {
    void operator()( SSL_CTX* x ) const { SSL_CTX_free( x ); }
  };
  typedef std::unique_ptr<SSL_CTX, CTX_deleter> CTX_handle;
  CTX_handle ctx_;

public:
  SSLContext();

  SSL_handle make_SSL_handle();
};

/* SSL session */
class SSLSession
{
  static constexpr size_t storage_size = 65536;

  SSL_handle ssl_;

  RingBuffer outbound_plaintext_ { storage_size };
  RingBuffer outbound_ciphertext_ { storage_size };

  RingBuffer inbound_ciphertext_ { storage_size };
  RingBuffer inbound_plaintext_ { storage_size };

  int get_error( const int return_value ) const;

public:
  SSLSession( SSL_handle&& ssl );

  void tick();
};

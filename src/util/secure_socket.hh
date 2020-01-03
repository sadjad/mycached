#pragma once

#include <memory>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "exception.hh"
#include "ring_buffer.hh"
#include "socket.hh"

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

class TCPSocketBIO : public TCPSocket
{
  class Method
  {
    struct BIO_METHOD_deleter
    {
      void operator()( BIO_METHOD* x ) const { BIO_meth_free( x ); }
    };
    std::unique_ptr<BIO_METHOD, BIO_METHOD_deleter> method_;

    Method();

  public:
    static const BIO_METHOD* method();
  };

  struct BIO_deleter
  {
    void operator()( BIO* x ) const { BIO_vfree( x ); }
  };
  std::unique_ptr<BIO, BIO_deleter> bio_;

public:
  TCPSocketBIO( TCPSocket&& sock );

  operator BIO*() { return bio_.get(); }
};

/* SSL session */
class SSLSession
{
  static constexpr size_t storage_size = 65536;

  SSL_handle ssl_;

  TCPSocketBIO socket_;

  RingBuffer outbound_plaintext_ { storage_size };
  RingBuffer inbound_plaintext_ { storage_size };

  int get_error( const int return_value ) const;

  bool write_waiting_on_read_ {};
  bool read_waiting_on_write_ {};

public:
  SSLSession( SSL_handle&& ssl, TCPSocket&& sock );

  RingBuffer& outbound_plaintext() { return outbound_plaintext_; }
  RingBuffer& inbound_plaintext() { return inbound_plaintext_; }
  TCPSocket& socket() { return socket_; }

  void do_read();
  void do_write();

  bool want_read() const;
  bool want_write() const;
};

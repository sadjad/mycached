#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "address.hh"
#include "elf-info.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "http_client.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stun.hh"
#include "timer.hh"

using namespace std;
using t = Timer::Category;

const string usertrust_certificate = R"(
-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
)";

void program_body()
{
  ios::sync_with_stdio( false );

  cerr << "Build ID: " << build_id_hex() << "\n";

  cerr << "Interpreter: " << interpreter() << "\n";

  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  FileDescriptor standard_output { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };
  standard_output.set_blocking( false );

  global_timer().start<t::DNS>();
  Address addr { "cs.stanford.edu", "443" };
  Address stun_server { "stun.l.google.com", "19302" };
  global_timer().stop<t::DNS>();

  {
    GlobalScopeTimer<t::Nonblock> timer;
    tcp_sock.connect( addr );
  }

  SSLContext ssl_context;
  ssl_context.trust_certificate( usertrust_certificate );
  SSLSession ssl { ssl_context.make_SSL_handle(), move( tcp_sock ), "cs.stanford.edu" };

  HTTPClient http;
  http.push_request( { "GET /~keithw/ HTTP/1.1", { { "Host", "cs.stanford.edu" } }, "" } );
  http.push_request( { "GET /~keithw HTTP/1.1", { { "Host", "cs.stanford.edu" } }, "" } );
  http.push_request( { "GET /~keithw/ HTTP/1.1", { { "Host", "cs.stanford.edu" }, { "Connection", "close" } }, "" } );

  STUNClient stun;

  UDPSocket sock;
  sock.sendto( stun_server, stun.make_binding_request() );
  UDPSocket::received_datagram udp_scratch { { "0", 0 }, {} };

  EventLoop event_loop;

  event_loop.add_rule(
    "SSL read", ssl.socket(), Direction::In, [&] { ssl.do_read(); }, [&] { return ssl.want_read(); } );

  event_loop.add_rule(
    "SSL write", ssl.socket(), Direction::Out, [&] { ssl.do_write(); }, [&] { return ssl.want_write(); } );

  event_loop.add_rule(
    "UDP receive",
    sock,
    Direction::In,
    [&] {
      auto rec = sock.recv();
      cerr << "Got STUN reply: " << rec.source_address.to_string() << " says we are: ";
      auto addr = stun.process_binding_response( rec.payload );
      cerr << ( addr ? addr.value().to_string() : "(unknown)" );
      cerr << "\n";
    },
    [&] { return stun.has_pending_requests(); } );

  event_loop.add_rule(
    "HTTP write",
    [&] { http.write( ssl.outbound_plaintext() ); },
    [&] { return ( not ssl.outbound_plaintext().writable_region().empty() ) and ( not http.requests_empty() ); } );

  event_loop.add_rule(
    "HTTP read",
    [&] { http.read( ssl.inbound_plaintext() ); },
    [&] { return not ssl.inbound_plaintext().readable_region().empty(); } );

  event_loop.add_rule(
    "print HTTP response",
    [&] {
      cerr << "Response received: " << http.responses_front().first_line() << "\n";
      http.pop_response();
    },
    [&] { return not http.responses_empty(); } );

  while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << event_loop.summary() << "\n";
}

int main()
{
  try {
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

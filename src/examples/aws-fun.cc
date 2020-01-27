#include <iostream>
#include <string>

#include "elf-info.hh"
#include "eventloop.hh"
#include "http_client.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;
using t = Timer::Category;

/* https://www.amazontrust.com/repository/AmazonRootCA1.pem */
const string aws_root_ca_1 = R"(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)";

void program_body()
{
  ios::sync_with_stdio( false );

  cerr << "Build ID: " << build_id_hex() << "\n";

  const string endpoint_name { "lambda.us-west-2.amazonaws.com" };

  const auto interp = interpreter();
  if ( not interp.empty() ) {
    cerr << "WARNING: This is not a statically linked executable.\n";
  }

  TCPSocket tcp_sock;
  tcp_sock.set_blocking( false );

  global_timer().start<t::DNS>();
  Address lambda_endpoint { endpoint_name, "443" };
  global_timer().stop<t::DNS>();

  {
    GlobalScopeTimer<t::Nonblock> timer;
    tcp_sock.connect( lambda_endpoint );
  }

  SSLContext ssl_context;
  ssl_context.trust_certificate( aws_root_ca_1 );
  SSLSession ssl { ssl_context.make_SSL_handle(), move( tcp_sock ), endpoint_name };

  HTTPClient http;
  http.push_request( { { "GET /2016-08-19/account-settings/ HTTP/1.1" },
                       { { "Host", endpoint_name }, { "Connection", "close" } },
                       "" } );

  EventLoop event_loop;

  event_loop.add_rule(
    "SSL read", ssl.socket(), Direction::In, [&] { ssl.do_read(); }, [&] { return ssl.want_read(); } );

  event_loop.add_rule(
    "SSL write", ssl.socket(), Direction::Out, [&] { ssl.do_write(); }, [&] { return ssl.want_write(); } );

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
      cerr << http.responses_front().body() << "\n";
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

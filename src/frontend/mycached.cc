#include <cstdlib>
#include <iostream>
#include <unordered_map>

#include "http/http_server.hh"
#include "util/eventloop.hh"
#include "util/exception.hh"
#include "util/socket.hh"

using namespace std;

void usage( char* argv0 )
{
  cerr << "Usage: " << argv0 << " PORT" << endl;
}

struct Client
{
  uint64_t id;
  TCPSession session;
  HTTPServer http {};

  Client( const uint64_t id, TCPSocket&& socket )
    : id( id )
    , session( move( socket ) )
  {}
};

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      usage( argv[0] );
      return EXIT_FAILURE;
    }

    uint64_t client_id { 0 };
    unordered_map<uint64_t, Client> clients;

    EventLoop event_loop;

    const uint16_t port = static_cast<uint16_t>( stoi( argv[1] ) );
    const Address listen_address { "0.0.0.0", port };

    TCPSocket listen_sock;
    listen_sock.set_blocking( false );
    listen_sock.bind( listen_address );
    listen_sock.listen();

    event_loop.add_rule(
      "tcp listener",
      listen_sock,
      Direction::In,
      [&]() {
        clients.emplace(
          piecewise_construct,
          forward_as_tuple( client_id ),
          forward_as_tuple( client_id, move( listen_sock.accept() ) ) );

        Client& client = clients.at( client_id );
        client.session.socket().set_blocking( false );

        event_loop.add_rule( "client read",
                             client.session.socket(),
                             Direction::In,
                             [&]() { client.session.do_read(); },
                             [&]() { return client.session.want_read(); } );

        event_loop.add_rule( "client write",
                             client.session.socket(),
                             Direction::Out,
                             [&]() { client.session.do_write(); },
                             [&]() { return client.session.want_write(); } );

        event_loop.add_rule(
          "HTTP read",
          [&] { client.http.read( client.session.inbound_plaintext() ); },
          [&] {
            return not client.session.inbound_plaintext()
                         .readable_region()
                         .empty();
          } );

        event_loop.add_rule(
          "request",
          [&]() {
            auto& request = client.http.requests_front();
            cerr << "request received: " << request.first_line() << endl;
            client.http.pop_request();

            client.http.push_response(
              { "HTTP/1.1 200 OK",
                { { "Server", "mycached/0.0.1" }, { "Content-Length", "1" } },
                "A" } );
          },
          [&]() { return not client.http.requests_empty(); } );

        event_loop.add_rule(
          "HTTP write",
          [&] { client.http.write( client.session.outbound_plaintext() ); },
          [&]() {
            return not client.session.outbound_plaintext()
                         .writable_region()
                         .empty()
                   and not client.http.responses_empty();
          } );
        client_id++;
      },
      []() { return true; },
      []() { throw runtime_error( "listen socket cancelled" ); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit )
      ;

  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}

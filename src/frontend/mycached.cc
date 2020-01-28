#include <cstdlib>
#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>

#include "http/http_server.hh"
#include "util/eventloop.hh"
#include "util/exception.hh"
#include "util/socket.hh"
#include "util/tokenize.hh"

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
  list<EventLoop::Handle> handles {};

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

    unordered_map<string, string> data_store;

    uint64_t client_id { 0 };
    unordered_map<uint64_t, Client> clients;

    EventLoop event_loop;

    const uint16_t port = static_cast<uint16_t>( stoi( argv[1] ) );
    const Address listen_address { "0.0.0.0", port };

    TCPSocket listen_sock;
    listen_sock.set_reuseaddr();
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

        auto cancel_callback = [&] {
          event_loop.remove_rules( client.handles );
          clients.erase( client.id );
        };

        client.handles.push_back(
          event_loop.add_rule( "client read",
                               client.session.socket(),
                               Direction::In,
                               [&] { client.session.do_read(); },
                               [&] { return client.session.want_read(); },
                               cancel_callback ) );

        client.handles.push_back(
          event_loop.add_rule( "client write",
                               client.session.socket(),
                               Direction::Out,
                               [&] { client.session.do_write(); },
                               [&] { return client.session.want_write(); },
                               cancel_callback ) );

        client.handles.push_back( event_loop.add_rule(
          "HTTP read",
          [&] { client.http.read( client.session.inbound_plaintext() ); },
          [&] {
            return not client.session.inbound_plaintext()
                         .readable_region()
                         .empty();
          } ) );

        client.handles.push_back( event_loop.add_rule(
          "HTTP write",
          [&] { client.http.write( client.session.outbound_plaintext() ); },
          [&] {
            return not client.session.outbound_plaintext()
                         .writable_region()
                         .empty()
                   and not client.http.responses_empty();
          } ) );

        client.handles.push_back( event_loop.add_rule(
          "request",
          [&] {
            auto& request = client.http.requests_front();
            const auto tokens = split( request.first_line(), " " );

            const string& method = tokens.at( 0 );
            const string& key = tokens.at( 1 ).substr( 1 );

            if ( method == "GET" ) {
              client.http.push_response(
                { "HTTP/1.1 200 OK",
                  { { "Server", "mycached/0.0.1" },
                    { "X-Object-Key", key },
                    { "Content-Length",
                      to_string( data_store.at( key ).length() ) } },
                  move( data_store.at( key ) ) } );

              data_store.erase( key );
            } else if ( method == "PUT" ) {
              data_store.emplace( key, move( request.body() ) );

              client.http.push_response( { "HTTP/1.1 200 OK",
                                           { { "Server", "mycached/0.0.1" },
                                             { "X-Object-Key", key },
                                             { "Content-Length", "0" } },
                                           "" } );
            } else {
              throw runtime_error( "invalid request" );
            }

            client.http.pop_request();
          },
          [&] { return not client.http.requests_empty(); } ) );

        client_id++;
      },
      [] { return true; },
      [] { throw runtime_error( "listen socket cancelled" ); } );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit )
      ;

  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  bool in_double_quoted_string = false;
  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

void program_body()
{
  UDPSocket sock;
  sock.bind( { "0", 9090 } );

  optional<Address> client, server;

  while ( true ) {
    auto rec = sock.recv();
    cout << "Datagram received from " << rec.source_address.to_string() << ": " << rec.payload << "\n";

    vector<string_view> fields;
    split_on_char( rec.payload, ' ', fields );
    if ( fields.size() == 2 and fields.at( 0 ) == "=" ) {
      if ( fields.at( 1 ) == "server" ) {
        server.emplace( rec.source_address );
        cout << "Learned server = " << server.value().to_string() << "\n";

        if ( client.has_value() ) {
          sock.sendto( client.value(), "= server " + server->ip() + " " + to_string( server->port() ) );
          cout << "Informing client.\n";
        }

	sock.sendto( server.value(), "= trolley" );
      } else if ( fields.at( 1 ) == "client" ) {
        client.emplace( rec.source_address );
        cout << "Learned client = " << client.value().to_string() << "\n";

        if ( server.has_value() ) {
          sock.sendto( client.value(), "= server " + server->ip() + " " + to_string( server->port() ) );
          cout << "Informing client of server address.\n";
        }
      }
    }
  }
}

int main()
{
  try {
    timer();
    program_body();
    cout << timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

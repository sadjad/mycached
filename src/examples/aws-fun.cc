#include <iostream>
#include <string>

#include "awsclient.hh"
#include "eventloop.hh"
#include "timer.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  AWSClient aws { "us-west-2" };
  EventLoop event_loop;

  aws.install_rules( event_loop );
  aws.get_account_settings();

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

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

unsigned int EventLoop::FDRule::service_count() const
{
  return direction == Direction::In ? fd.read_count() : fd.write_count();
}

void EventLoop::add_rule( const string& name,
                          const FileDescriptor& fd,
                          const Direction direction,
                          const CallbackT& callback,
                          const InterestT& interest,
                          const CallbackT& cancel )
{
  _fd_rules.push_back( { name, fd.duplicate(), direction, callback, interest, cancel } );
}

void EventLoop::add_rule( const string& name, const CallbackT& callback, const InterestT& interest )
{
  _non_fd_rules.push_back( { name, callback, interest } );
}

EventLoop::Result EventLoop::wait_next_event( const int timeout_ms )
{
  // first, handle the non-file-descriptor-related rules
  {
    ScopeTimer<Log::Category::Nonblock> timer;
    for ( auto& this_rule : _non_fd_rules ) {
      unsigned int iterations = 0;
      while ( this_rule.interest() ) {
        this_rule.callback();
        ++iterations;
        if ( iterations > 255 ) {
          throw runtime_error( "EventLoop: busy wait detected: rule \"" + this_rule.name
                               + "\" is still interested after running callback " + to_string( iterations )
                               + " times" );
        }
      }
    }
  }

  // now the file-descriptor-related rules. poll any "interested" file descriptors
  vector<pollfd> pollfds {};
  pollfds.reserve( _fd_rules.size() );
  bool something_to_poll = false;

  // set up the pollfd for each rule
  for ( auto it = _fd_rules.cbegin(); it != _fd_rules.cend(); ) { // NOTE: it gets erased or incremented in loop body
    const auto& this_rule = *it;
    if ( this_rule.direction == Direction::In && this_rule.fd.eof() ) {
      // no more reading on this rule, it's reached eof
      this_rule.cancel();
      it = _fd_rules.erase( it );
      continue;
    }

    if ( this_rule.fd.closed() ) {
      this_rule.cancel();
      it = _fd_rules.erase( it );
      continue;
    }

    if ( this_rule.interest() ) {
      pollfds.push_back( { this_rule.fd.fd_num(), static_cast<short>( this_rule.direction ), 0 } );
      something_to_poll = true;
    } else {
      pollfds.push_back( { this_rule.fd.fd_num(), 0, 0 } ); // placeholder --- we still want errors
    }
    ++it;
  }

  // quit if there is nothing left to poll
  if ( not something_to_poll ) {
    return Result::Exit;
  }

  // call poll -- wait until one of the fds satisfies one of the rules (writeable/readable)
  {
    ScopeTimer<Log::Category::WaitingForEvent> timer;
    if ( 0 == CheckSystemCall( "poll", ::poll( pollfds.data(), pollfds.size(), timeout_ms ) ) ) {
      return Result::Timeout;
    }
  }

  // go through the poll results
  ScopeTimer<Log::Category::Nonblock> timer;

  for ( auto [it, idx] = make_pair( _fd_rules.begin(), size_t( 0 ) ); it != _fd_rules.end(); ++idx ) {
    const auto& this_pollfd = pollfds[idx];
    const auto& this_rule = *it;

    const auto poll_error = static_cast<bool>( this_pollfd.revents & ( POLLERR | POLLNVAL ) );
    if ( poll_error ) {
      /* see if fd is a socket */
      int socket_error = 0;
      socklen_t optlen = sizeof( socket_error );
      const int ret = getsockopt( this_rule.fd.fd_num(), SOL_SOCKET, SO_ERROR, &socket_error, &optlen );
      if ( ret == -1 and errno == ENOTSOCK ) {
        throw runtime_error( "error on polled file descriptor for rule \"" + this_rule.name + "\"" );
      } else if ( ret == -1 ) {
        throw unix_error( "getsockopt" );
      } else if ( optlen != sizeof( socket_error ) ) {
        throw runtime_error( "unexpected length from getsockopt: " + to_string( optlen ) );
      } else if ( socket_error ) {
        throw unix_error( "error on polled socket for rule \"" + this_rule.name + "\"", socket_error );
      }

      this_rule.cancel();
      it = _fd_rules.erase( it );
      continue;
    }

    const auto poll_ready = static_cast<bool>( this_pollfd.revents & this_pollfd.events );
    const auto poll_hup = static_cast<bool>( this_pollfd.revents & POLLHUP );
    if ( poll_hup && this_pollfd.events && !poll_ready ) {
      // if we asked for the status, and the _only_ condition was a hangup, this FD is defunct:
      //   - if it was POLLIN and nothing is readable, no more will ever be readable
      //   - if it was POLLOUT, it will not be writable again
      this_rule.cancel();
      it = _fd_rules.erase( it );
      continue;
    }

    if ( poll_ready ) {
      // we only want to call callback if revents includes the event we asked for
      const auto count_before = this_rule.service_count();
      this_rule.callback();

      // only check for busy wait if we're not canceling or exiting
      if ( count_before == this_rule.service_count() and this_rule.interest() ) {
        throw runtime_error( "EventLoop: busy wait detected: rule \"" + this_rule.name
                             + "\" did not read/write fd and is still interested" );
      }
    }

    ++it; // if we got here, it means we didn't call _fd_rules.erase()
  }

  return Result::Success;
}

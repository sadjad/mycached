#pragma once

#include <functional>
#include <list>
#include <string_view>

#include <poll.h>

#include "file_descriptor.hh"
#include "timer.hh"

//! Waits for events on file descriptors and executes corresponding callbacks.
class EventLoop
{
public:
  //! Indicates interest in reading (In) or writing (Out) a polled fd.
  enum class Direction : short
  {
    In = POLLIN,  //!< Callback will be triggered when Rule::fd is readable.
    Out = POLLOUT //!< Callback will be triggered when Rule::fd is writable.
  };

private:
  using CallbackT = std::function<void( void )>;
  using InterestT = std::function<bool( void )>;

  struct RuleCategory
  {
    std::string name;
    Timer::Record timer;
  };

  struct FDRule
  {
    FileDescriptor fd;   //!< FileDescriptor to monitor for activity.
    Direction direction; //!< Direction::In for reading from fd, Direction::Out for writing to fd.
    CallbackT callback;  //!< A callback that reads or writes fd.
    InterestT interest;  //!< A callback that returns `true` whenever fd should be polled.
    CallbackT cancel;    //!< A callback that is called when the rule is cancelled (e.g. on hangup)
    size_t category_id;

    //! Returns the number of times fd has been read or written, depending on the value of Rule::direction.
    //! \details This function is used internally by EventLoop; you will not need to call it
    unsigned int service_count() const;
  };

  struct NonFDRule
  {
    CallbackT callback;
    InterestT interest;
    size_t category_id;
  };

  std::vector<RuleCategory> _rule_categories {};
  std::list<FDRule> _fd_rules {};
  std::list<NonFDRule> _non_fd_rules {};

public:
  //! Returned by each call to EventLoop::wait_next_event.
  enum class Result
  {
    Success, //!< At least one Rule was triggered.
    Timeout, //!< No rules were triggered before timeout.
    Exit //!< All rules have been canceled or were uninterested; make no further calls to EventLoop::wait_next_event.
  };

  size_t add_category( const std::string& name );

  void add_rule(
    const size_t category_id,
    const FileDescriptor& fd,
    const Direction direction,
    const CallbackT& callback,
    const InterestT& interest = [] { return true; },
    const CallbackT& cancel = [] {} );

  void add_rule(
    const size_t category_id,
    const CallbackT& callback,
    const InterestT& interest = [] { return true; } );

  //! Calls [poll(2)](\ref man2::poll) and then executes callback for each ready fd.
  Result wait_next_event( const int timeout_ms );

  std::string summary() const;

  // convenience function to add category and rule at the same time
  template<typename... Targs>
  void add_rule( const std::string& name, Targs&&... Fargs )
  {
    add_rule( add_category( name ), std::forward<Targs>( Fargs )... );
  }
};

using Direction = EventLoop::Direction;

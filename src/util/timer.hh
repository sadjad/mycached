#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <type_traits>

inline uint64_t timestamp_ns()
{
  static_assert( std::is_same<std::chrono::steady_clock::duration, std::chrono::nanoseconds>::value );

  return std::chrono::steady_clock::now().time_since_epoch().count();
}

class Log
{
public:
  enum class Category
  {
    DNS,
    SSL,
    count
  };

  constexpr static size_t num_categories = static_cast<size_t>( Category::count );

  constexpr static std::array<const char*, num_categories> _category_names { { "DNS", "SSL" } };

private:
  uint64_t _beginning_timestamp = timestamp_ns();

  struct Record
  {
    uint64_t count;
    uint64_t total_ns;
    uint64_t max_ns;
  };

  std::array<Record, num_categories> _logs {};

public:
  inline void log( const Category category, const uint64_t time_ns )
  {
    auto& entry = _logs[static_cast<size_t>( category )];

    entry.count++;
    entry.total_ns += time_ns;
    entry.max_ns = std::max( entry.max_ns, time_ns );
  }

  std::string summary() const;
};

class Timer
{
private:
  Log _log {};
  std::optional<Log::Category> _current_category {};
  uint64_t _start_time {};

public:
  template<Log::Category category>
  void start()
  {
    if ( _current_category.has_value() ) {
      throw std::runtime_error( "timer started when already running" );
    }

    _current_category = category;
    _start_time = timestamp_ns();
  }

  template<Log::Category category>
  void stop()
  {
    if ( not _current_category.has_value() or _current_category.value() != category ) {
      throw std::runtime_error( "timer stopped when not running, or with mismatched category" );
    }

    _log.log( category, timestamp_ns() - _start_time );
    _current_category.reset();
  }

  std::string summary() const { return _log.summary(); }
};

inline Timer& global_timer()
{
  static Timer the_global_timer;
  return the_global_timer;
}

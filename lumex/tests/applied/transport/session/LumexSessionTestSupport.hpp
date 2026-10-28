// Helpers of the session tests: a factory of mock streams the test scripts
// (fails, throws, holds the open), a factory of scripted streams whose
// operations outlive their close, a recorder of the state changes, and waits
// with a bound.

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_SESSION_TEST_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_SESSION_TEST_SUPPORT_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/session/LumexMessageSession.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/LumexStreamSession.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace lumex_session_test
{
namespace tr = lumex::applied::transport;
namespace ss = tr::session;
using clock_type = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;

/// Waits until `condition` holds, polling every millisecond, at most
/// `limit`; returns the last value of the condition.
inline bool
eventually (std::function<bool ()> const &condition, ms limit = ms (3000))
{
  clock_type::time_point const end = clock_type::now () + limit;
  for (;;)
    {
      if (condition ())
        return true;
      if (clock_type::now () >= end)
        return condition ();
      std::this_thread::sleep_for (ms (1));
    }
}

/// The script and the record of a mock stream factory (thread-safe).
struct stream_factory_state
{
  std::mutex mutex;
  /// The first `throws` calls throw (std::runtime_error), the next
  /// `open_failures` make a stream whose open fails with `open_error`.
  std::size_t throws = 0;
  std::size_t open_failures = 0;
  std::error_code open_error
      = std::make_error_code (std::errc::no_such_device);
  /// The call `hold_call` (1-based; 0: none) makes a stream whose open never
  /// completes until cancelled.
  std::size_t hold_call = 0;
  std::vector<clock_type::time_point> calls;
  std::vector<tr::testing::mock_stream_control> controls;

  std::size_t
  call_count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls.size ();
  }

  std::vector<clock_type::time_point>
  call_times ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls;
  }

  /// The control of the last stream made (the session's current one).
  tr::testing::mock_stream_control
  last ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    if (controls.empty ())
      throw std::logic_error ("no stream made yet");
    return controls.back ();
  }
};

inline ss::stream_session::factory_t
make_stream_factory (std::shared_ptr<stream_factory_state> const &state)
{
  return [state] (tr::executor_type const &executor) -> tr::stream::byte_stream
           {
             std::lock_guard<std::mutex> const lock (state->mutex);
             state->calls.push_back (clock_type::now ());
             std::size_t const call = state->calls.size ();
             if (call <= state->throws)
               throw std::runtime_error ("no device");
             tr::testing::mock_stream made = tr::testing::make_mock_stream (
                 executor, "mock" + std::to_string (call));
             if (call <= state->throws + state->open_failures)
               made.control.fail_open (state->open_error);
             if (call == state->hold_call)
               made.control.hold_next_open ();
             state->controls.push_back (made.control);
             return std::move (made.stream);
           };
}

/// The script and the record of a mock datagram factory.
struct datagram_factory_state
{
  std::mutex mutex;
  std::size_t calls = 0;
  std::vector<tr::testing::mock_datagram_control> controls;

  tr::testing::mock_datagram_control
  last ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    if (controls.empty ())
      throw std::logic_error ("no transport made yet");
    return controls.back ();
  }

  std::size_t
  call_count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls;
  }
};

inline ss::message_session::factory_t
make_datagram_factory (std::shared_ptr<datagram_factory_state> const &state)
{
  return
      [state] (
          tr::executor_type const &executor) -> tr::datagram::message_transport
        {
          std::lock_guard<std::mutex> const lock (state->mutex);
          ++state->calls;
          tr::testing::mock_datagram made = tr::testing::make_mock_datagram (
              executor, "datagram" + std::to_string (state->calls));
          state->controls.push_back (made.control);
          return std::move (made.transport);
        };
}

/// A stream whose open and read stay pending until the test ends them; its
/// cancel () and close () leave them pending (a transport whose operations
/// outlive its close, as the bounded open of the serial stream does). Writes
/// succeed, or fail with the error `fail_writes` set.
class scripted_stream_impl : public tr::stream::stream_impl
{
public:
  scripted_stream_impl (tr::executor_type const &executor,
                        std::string const &name)
      : tr::stream::stream_impl (executor, name), open_pending_ (false),
        read_pending_ (false), closed_ (false)
  {
  }

  /// Ends the pending open with `error` (success marks the stream open).
  void
  finish_open (std::error_code error)
  {
    std::shared_ptr<scripted_stream_impl> const self = shared_self ();
    asio::post (strand (), [self, error] () { self->do_finish_open (error); });
  }

  /// Ends the pending read with `error` and `bytes`.
  void
  finish_read (std::error_code error, std::size_t bytes)
  {
    std::shared_ptr<scripted_stream_impl> const self = shared_self ();
    asio::post (strand (), [self, error, bytes] ()
                  { self->do_finish_read (error, bytes); });
  }

  /// The writes from now on fail with `error` (empty: they succeed).
  void
  fail_writes (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    write_error_ = error;
  }

  bool
  open_pending () const
  {
    return open_pending_.load ();
  }

  bool
  read_pending () const
  {
    return read_pending_.load ();
  }

  /// True once close () reached the stream.
  bool
  closed () const
  {
    return closed_.load ();
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    open_handler_ = std::move (handler);
    open_pending_.store (true);
  }

  void
  do_read_some (tr::mutable_bytes_t, tr::io_handler_t handler) override
  {
    read_handler_ = std::move (handler);
    read_pending_.store (true);
  }

  void
  do_write_some (tr::const_bytes_t data, tr::io_handler_t handler) override
  {
    std::error_code error;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      error = write_error_;
    }
    complete (std::move (handler), error, error ? 0 : data.size ());
  }

  void
  do_cancel () override
  {
  }

  void
  do_close () override
  {
    closed_.store (true);
  }

private:
  std::shared_ptr<scripted_stream_impl>
  shared_self ()
  {
    return std::static_pointer_cast<scripted_stream_impl> (
        shared_from_this ());
  }

  void
  do_finish_open (std::error_code error)
  {
    if (!open_pending_.exchange (false))
      return;
    if (!error)
      set_open (true);
    complete (std::move (open_handler_), error);
  }

  void
  do_finish_read (std::error_code error, std::size_t bytes)
  {
    if (!read_pending_.exchange (false))
      return;
    complete (std::move (read_handler_), error, bytes);
  }

  std::mutex mutex_;
  std::error_code write_error_;
  tr::status_handler_t open_handler_;
  tr::io_handler_t read_handler_;
  std::atomic<bool> open_pending_;
  std::atomic<bool> read_pending_;
  std::atomic<bool> closed_;
};

/// The record of a factory of scripted streams.
struct scripted_factory_state
{
  std::mutex mutex;
  std::vector<std::shared_ptr<scripted_stream_impl>> made;

  std::size_t
  count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return made.size ();
  }

  /// The stream of call `index` (0-based).
  std::shared_ptr<scripted_stream_impl>
  at (std::size_t index)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    if (index >= made.size ())
      throw std::logic_error ("no such stream yet");
    return made[index];
  }
};

inline ss::stream_session::factory_t
make_scripted_factory (std::shared_ptr<scripted_factory_state> const &state)
{
  return [state] (tr::executor_type const &executor) -> tr::stream::byte_stream
           {
             std::lock_guard<std::mutex> const lock (state->mutex);
             std::shared_ptr<scripted_stream_impl> const made
                 = std::make_shared<scripted_stream_impl> (
                     executor,
                     "scripted" + std::to_string (state->made.size () + 1));
             state->made.push_back (made);
             return tr::stream::byte_stream (made);
           };
}

/// Records the state changes a session reports.
struct state_recorder
{
  std::mutex mutex;
  std::vector<ss::state_change_t> changes;

  void
  add (ss::state_change_t const &change)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    changes.push_back (change);
  }

  std::vector<ss::state_change_t>
  all ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return changes;
  }

  std::vector<ss::channel_state>
  states ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    std::vector<ss::channel_state> result;
    for (std::size_t i = 0; i < changes.size (); ++i)
      result.push_back (changes[i].current);
    return result;
  }

  std::size_t
  count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return changes.size ();
  }
};

/// Options that record into `recorder`, with short delays and no jitter.
inline ss::session_options_t
recorded_options (std::shared_ptr<state_recorder> const &recorder,
                  ms initial = ms (5), ms max_delay = ms (20))
{
  ss::session_options_t options;
  options.name = "test";
  options.policy.initial_delay = initial;
  options.policy.max_delay = max_delay;
  options.policy.multiplier = 2.0;
  options.policy.jitter = 0.0;
  options.on_state_change = [recorder] (ss::state_change_t const &change)
    { recorder->add (change); };
  return options;
}

/// The result of an asynchronous operation, filled by its handler.
template <typename Value> struct outcome
{
  std::mutex mutex;
  std::condition_variable signal;
  bool done = false;
  std::error_code error;
  Value value = Value ();

  void
  set (std::error_code const &e, Value const &v)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    error = e;
    value = v;
    done = true;
    signal.notify_all ();
  }

  bool
  wait (ms limit = ms (3000))
  {
    std::unique_lock<std::mutex> lock (mutex);
    return signal.wait_for (lock, limit, [this] { return done; });
  }

  bool
  is_done ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return done;
  }
};
} // namespace lumex_session_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_SESSION_TEST_SUPPORT_HPP

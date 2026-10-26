// Every public header of the transport and every token style in one
// translation unit, built with strict warnings (see CMakeLists.txt). At run
// time a mock stream and a mock datagram answer each style once, directly and
// through a session (stream_session, message_session); exit code 0 when every
// exchange returned what was fed, 1 otherwise. With the logger adapter
// (LUMEX_TRANSPORT_WITH_LOGGER) the run goes through the LumexLogger sink and
// a lumex_logger_watch_timer.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
#include "lumex/applied/transport/log/adapter/LumexTransportLoggerAdapter.hpp"
#endif
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;

std::vector<std::uint8_t>
reply ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

template <typename Stream>
bool
by_callback (Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  stream.async_read_some (tr::mutable_bytes_t (buffer),
                          [&done] (std::error_code error, std::size_t bytes)
                            { done.set_value (error ? 0 : bytes); });
  return done.get_future ().get () == 3 && buffer == reply ();
}

template <typename Stream>
bool
by_future (Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::size_t const bytes
      = stream.async_read_some (tr::mutable_bytes_t (buffer), asio::use_future)
            .get ();
  stream.async_write_all (tr::const_bytes_t (buffer), asio::use_future).get ();
  return bytes == 3 && buffer == reply ();
}

// A deferred operation launched later, with a future.
bool
by_deferred (tr::stream::byte_stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  auto read
      = stream.async_read_some (tr::mutable_bytes_t (buffer), asio::deferred);
  std::size_t const bytes = std::move (read) (asio::use_future).get ();
  return bytes == 3 && buffer == reply ();
}

// A cancellation slot (asio::cancel_after) and a handler bound to a strand.
bool
by_cancel_after (tr::stream::io_runner &runner,
                 tr::stream::byte_stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (
          std::chrono::seconds (10),
          asio::bind_executor (
              asio::make_strand (runner.get_executor ()),
              [&done] (std::error_code error, std::size_t bytes)
                { done.set_value (error ? 0 : bytes); })));
  return done.get_future ().get () == 3 && buffer == reply ();
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
template <typename Stream>
bool
by_yield (tr::stream::io_runner &runner, Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  asio::spawn (
      runner.get_executor (),
      [&stream, &buffer, &done] (asio::yield_context yield)
        {
          std::error_code error;
          std::size_t const bytes = stream.async_read_some (
              tr::mutable_bytes_t (buffer), yield[error]);
          done.set_value (error ? 0 : bytes);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3 && buffer == reply ();
}
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
template <typename Stream>
bool
by_awaitable (tr::stream::io_runner &runner, Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  asio::co_spawn (
      runner.get_executor (),
      [&stream, &buffer, &done] () -> asio::awaitable<void>
        {
          std::size_t const bytes = co_await stream.async_read_some (
              tr::mutable_bytes_t (buffer), asio::use_awaitable);
          done.set_value (bytes);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3 && buffer == reply ();
}
#endif

bool
datagram_round (tr::stream::io_runner &runner)
{
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (runner.get_executor (), "datagram");
  if (!mock.transport.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::datagram::message_t message;
  message.payload = reply ();
  mock.control.deliver (message);
  tr::datagram::message_t const received
      = mock.transport.async_receive (asio::use_future).get ();
  mock.transport.async_send (received, asio::use_future).get ();
  return received.payload == reply ();
}
// The mock streams a session's factory made, for the script.
struct made_streams
{
  std::mutex mutex;
  std::vector<tr::testing::mock_stream_control> controls;
};

// Every token style again through a stream_session, then a state wait and a
// message_session round.
bool
session_round (tr::stream::io_runner &runner)
{
  std::shared_ptr<made_streams> const made = std::make_shared<made_streams> ();
  tr::session::stream_session session (
      runner.get_executor (),
      [made] (tr::executor_type const &executor) -> tr::stream::byte_stream
        {
          tr::testing::mock_stream mock
              = tr::testing::make_mock_stream (executor, "session");
          std::lock_guard<std::mutex> const lock (made->mutex);
          made->controls.push_back (mock.control);
          return std::move (mock.stream);
        });
  if (!session.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::testing::mock_stream_control control = made->controls.back ();
  bool fine = true;
  control.feed (reply ());
  fine = by_callback (session) && fine;
  control.feed (reply ());
  fine = by_future (session) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  control.feed (reply ());
  fine = by_yield (runner, session) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  control.feed (reply ());
  fine = by_awaitable (runner, session) && fine;
#endif
  // A read through asio::deferred, launched later.
  control.feed (reply ());
  std::vector<std::uint8_t> deferred_buffer (3);
  auto deferred_read = session.async_read_some (
      tr::mutable_bytes_t (deferred_buffer), asio::deferred);
  std::promise<std::size_t> deferred_done;
  std::move (deferred_read) (
      [&deferred_done] (std::error_code error, std::size_t bytes)
        { deferred_done.set_value (error ? 0 : bytes); });
  fine = deferred_done.get_future ().get () == 3 && fine;
  // The close ends the pending waits with cancelled (the wait from a
  // snapshot misses no change).
  std::promise<std::error_code> changed;
  session.async_wait_state_change (
      tr::session::channel_state::open,
      [&changed] (std::error_code error, tr::session::channel_state)
        { changed.set_value (error); });
  tr::session::state_snapshot_t const seen = session.state_snapshot ();
  std::promise<std::error_code> snapshot_changed;
  session.async_wait_state_change (
      seen, [&snapshot_changed] (std::error_code error,
                                 tr::session::state_snapshot_t)
        { snapshot_changed.set_value (error); });
  session.close ();
  fine = changed.get_future ().get () == tr::error::transport_errc::cancelled
         && snapshot_changed.get_future ().get ()
                == tr::error::transport_errc::cancelled
         && session.session_statistics ().attempts == 1 && fine;

  tr::session::message_session messages (
      runner.get_executor (),
      [] (tr::executor_type const &executor) -> tr::datagram::message_transport
        {
          tr::testing::mock_datagram mock
              = tr::testing::make_mock_datagram (executor, "messages");
          return std::move (mock.transport);
        });
  if (!messages.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::datagram::message_t message;
  message.payload = reply ();
  messages.async_send (message, asio::use_future).get ();
  return fine;
}
} // namespace

int
main ()
{
  tr::stream::io_runner runner (2);
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
  tr::log::install_lumex_logger_sink ();
  tr::log::lumex_logger_watch_timer const watch (
      runner, std::chrono::milliseconds (50));
  if (!watch.is_running ())
    return 1;
#endif
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "stream");
  if (!mock.stream.open (tr::deadline_t (1000)).has_value ())
    return 1;

  bool fine = true;
  mock.control.feed (reply ());
  fine = by_callback (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_future (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_deferred (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_cancel_after (runner, mock.stream) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  mock.control.feed (reply ());
  fine = by_yield (runner, mock.stream) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  mock.control.feed (reply ());
  fine = by_awaitable (runner, mock.stream) && fine;
#endif
  fine = datagram_round (runner) && fine;
  fine = session_round (runner) && fine;
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
  tr::log::uninstall_lumex_logger_sink ();
  fine = tr::log::get_log_sink () == nullptr && fine;
#endif

  // The counters: the exchanges above were counted (a snapshot and a reset
  // compile and run under the strict warnings too).
  tr::statistics::statistics_t const counted = mock.stream.statistics ();
  fine = counted.opens == 1 && counted.reads >= 4 && counted.writes >= 1
         && counted.total_errors () == 0 && fine;
  mock.stream.reset_statistics ();
  fine = mock.stream.statistics ().reads == 0 && fine;

  std::cout << "transport_warnings: " << (fine ? "fine" : "FAILED") << '\n';
  return fine ? 0 : 1;
}

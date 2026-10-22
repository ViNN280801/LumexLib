// Every public header of the transport and every token style in one
// translation unit, built with strict warnings (see CMakeLists.txt). At run
// time a mock stream and a mock datagram answer each style once; exit code 0
// when every exchange returned what was fed, 1 otherwise.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"
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

bool
by_callback (tr::stream::byte_stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  stream.async_read_some (tr::mutable_bytes_t (buffer),
                          [&done] (std::error_code error, std::size_t bytes)
                            { done.set_value (error ? 0 : bytes); });
  return done.get_future ().get () == 3 && buffer == reply ();
}

bool
by_future (tr::stream::byte_stream &stream)
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
bool
by_yield (tr::stream::io_runner &runner, tr::stream::byte_stream &stream)
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
bool
by_awaitable (tr::stream::io_runner &runner, tr::stream::byte_stream &stream)
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
} // namespace

int
main ()
{
  tr::stream::io_runner runner (2);
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

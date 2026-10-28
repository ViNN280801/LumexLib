// Cancelling and composing operations of lumex::transport, on a mock stream
// that the example drives (no hardware, exact results): a deadline on one
// asynchronous operation with asio::cancel_after, a cancellation slot and
// signal, what a terminal cancellation ends (every operation pending on the
// stream, not only the one whose slot was used), asio::deferred (build the
// operation now, choose the token later), a completion handler that runs on
// an executor of the program, and the synchronous call that refuses to wait
// for itself. The same code cancels a serial, TCP or UDP operation.
//
// The rules the example relies on (README, "Асинхронный API"): a handler with
// asio::cancel_after must run on the stream's strand or on its own strand,
// never on the bare executor of an io_runner with several threads; the
// transport holds no work on a handler's executor, so a context that only
// runs handlers gets an executor_work_guard.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
namespace
{
using ms = std::chrono::milliseconds;
using tr::error::transport_errc;
using read_outcome = std::pair<std::error_code, std::size_t>;

std::vector<std::uint8_t>
bytes_of (std::string const &text)
{
  return std::vector<std::uint8_t> (text.begin (), text.end ());
}

std::string
describe (read_outcome const &outcome)
{
  return outcome.first ? outcome.first.message ()
                       : std::to_string (outcome.second) + " B";
}

bool
cancel_after_on_one_operation (tr::stream::io_runner &runner)
{
  std::cout << "--- 1. asio::cancel_after: a deadline on one operation\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));
  std::vector<std::uint8_t> buffer (16);

  // The wrapped token keeps the handler as it is. The timer runs on the
  // stream's strand; the read ends with cancelled when the time is up.
  std::promise<read_outcome> expired;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (
          ms (50), [&expired] (std::error_code error, std::size_t bytes)
            { expired.set_value (read_outcome (error, bytes)); }));
  read_outcome const first = expired.get_future ().get ();
  std::cout << "  nothing arrived within 50 ms: " << describe (first)
            << " (== transport_errc::cancelled: "
            << (first.first == transport_errc::cancelled ? "yes" : "no")
            << ")\n";

  // With asio::use_future the error becomes an exception of the future.
  std::future<std::size_t> second = stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (ms (50), asio::use_future));
  try
    {
      second.get ();
    }
  catch (std::system_error const &error)
    {
      std::cout << "  the same with use_future: " << error.code ().message ()
                << '\n';
    }

  // Cancelling is not closing: the stream is open, and the next read gets
  // its bytes.
  mock.control.feed (bytes_of ("late"));
  std::promise<read_outcome> third;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (
          ms (500), [&third] (std::error_code error, std::size_t bytes)
            { third.set_value (read_outcome (error, bytes)); }));
  read_outcome const arrived = third.get_future ().get ();
  std::cout << "  a read that is in time: " << describe (arrived)
            << ", stream open: " << (stream.is_open () ? "yes" : "no") << '\n';

  tr::statistics::statistics_t const stats = stream.statistics ();
  std::cout << "  statistics: " << stats.cancellations << " cancellations, "
            << stats.timeouts << " timeouts\n";
  return first.first == transport_errc::cancelled && arrived.second == 4
         && stats.cancellations == 2 && stats.timeouts == 0;
}

// Two reads pending at once need a transport that accepts them: a mock keeps
// one pending read, a TCP stream queues both. The peer here accepts and says
// nothing.
bool
a_signal_cancels_the_whole_stream (tr::stream::io_runner &runner)
{
  std::cout << "--- 2. a cancellation signal ends every pending operation\n";
  asio::ip::tcp::acceptor acceptor (
      runner.context (),
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  asio::ip::tcp::socket silent_peer (runner.context ());
  acceptor.async_accept (silent_peer, [] (asio::error_code const &) {});
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  if (!stream.open (tr::deadline_t (2000)).has_value ())
    return false;
  std::vector<std::uint8_t> buffer_a (16);
  std::vector<std::uint8_t> buffer_b (16);

  std::promise<read_outcome> a_done;
  std::promise<read_outcome> b_done;
  asio::cancellation_signal signal;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer_a),
      [&a_done] (std::error_code error, std::size_t bytes)
        { a_done.set_value (read_outcome (error, bytes)); });
  stream.async_read_some (
      tr::mutable_bytes_t (buffer_b),
      asio::bind_cancellation_slot (
          signal.slot (), [&b_done] (std::error_code error, std::size_t bytes)
            { b_done.set_value (read_outcome (error, bytes)); }));

  // Asio wants a signal emitted on the executor of the handler's operation;
  // the runner has one thread, so a post to it is that executor.
  asio::post (runner.get_executor (),
              [&signal] { signal.emit (asio::cancellation_type::terminal); });
  read_outcome const b = b_done.get_future ().get ();
  read_outcome const a = a_done.get_future ().get ();
  // `partial` and `total` alone change nothing: only the terminal bit
  // cancels (asio::cancellation_type::all has it).
  std::cout << "  read B (the slot's): " << describe (b)
            << "\n  read A (no slot):    " << describe (a)
            << "\n  stream still open: " << (stream.is_open () ? "yes" : "no")
            << '\n';
  return a.first == transport_errc::cancelled
         && b.first == transport_errc::cancelled && stream.is_open ();
}

bool
deferred_operations (tr::stream::io_runner &runner)
{
  std::cout << "--- 3. asio::deferred: the operation now, the token later\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));
  mock.control.feed (bytes_of ("abc"));
  std::vector<std::uint8_t> buffer (16);

  // Nothing is started by this line: the bytes are still on the device.
  auto operation
      = stream.async_read_some (tr::mutable_bytes_t (buffer), asio::deferred);
  std::cout << "  reads counted before the operation is launched: "
            << stream.statistics ().reads << '\n';

  // The token is chosen at the launch: here a deadline around a callback.
  std::promise<read_outcome> done;
  std::move (operation) (asio::cancel_after (
      ms (500), [&done] (std::error_code error, std::size_t bytes)
        { done.set_value (read_outcome (error, bytes)); }));
  read_outcome const outcome = done.get_future ().get ();
  std::cout << "  launched later: " << describe (outcome) << ", reads counted "
            << stream.statistics ().reads << '\n';
  return outcome.second == 3 && stream.statistics ().reads == 1;
}

bool
completion_on_the_handlers_executor (tr::stream::io_runner &runner,
                                     asio::io_context &ui)
{
  std::cout << "--- 4. the handler runs on the executor it is bound to\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));
  mock.control.feed (bytes_of ("abc"));
  std::vector<std::uint8_t> buffer (16);

  // `ui` is the context of this thread (a GUI loop, say). The transport keeps
  // no work on it, so ui.run () would return at once and leave the handler
  // for a later run: hold a work guard until the handler is done.
  std::thread::id handler_thread;
  asio::executor_work_guard<asio::io_context::executor_type> guard
      = asio::make_work_guard (ui);
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_executor (
          ui.get_executor (),
          [&guard, &handler_thread] (std::error_code, std::size_t)
            {
              handler_thread = std::this_thread::get_id ();
              guard.reset (); // no more work: run () returns
            }));
  ui.run ();
  bool const here = handler_thread == std::this_thread::get_id ();
  std::cout << "  the handler ran on the thread that called run (): "
            << (here ? "yes" : "no") << '\n';
  ui.restart ();
  return here;
}

bool
a_synchronous_call_on_the_executor_thread (tr::stream::io_runner &runner)
{
  std::cout << "--- 5. a synchronous call from the executor's own thread\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));

  // The synchronous read waits for a handler that only this thread could
  // run: instead of hanging it returns would_deadlock at once.
  std::promise<std::error_code> done;
  std::vector<std::uint8_t> buffer (16);
  asio::post (runner.get_executor (),
              [&stream, &buffer, &done]
                {
                  tr::io_result_t const result = stream.read_some (
                      tr::mutable_bytes_t (buffer), tr::deadline_t (5000));
                  done.set_value (result.error ());
                });
  std::error_code const error = done.get_future ().get ();
  std::cout << "  read_some in a handler of the runner: " << error.message ()
            << '\n';
  return error == transport_errc::would_deadlock;
}
} // namespace

int
main ()
{
  // Declared before the runner: the context of the handlers outlives the
  // operations the runner still holds.
  asio::io_context ui;
  tr::stream::io_runner runner (1);
  if (!cancel_after_on_one_operation (runner))
    return 1;
  if (!a_signal_cancels_the_whole_stream (runner))
    return 1;
  if (!deferred_operations (runner))
    return 1;
  if (!completion_on_the_handlers_executor (runner, ui))
    return 1;
  if (!a_synchronous_call_on_the_executor_thread (runner))
    return 1;
  return 0;
}
#else
int
main ()
{
  std::cout << "built without the mock transports "
               "(LUMEX_TRANSPORT_WITH_TESTING): nothing to show\n";
  return 0;
}
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

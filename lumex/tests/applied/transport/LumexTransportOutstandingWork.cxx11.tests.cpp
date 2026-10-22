// The executor of the completion handler and outstanding work: byte_stream
// and message_transport keep no work on it (a handler that keeps its owner
// would keep that work forever, see LumexTransportHandlerCycle), so an
// io_context that only runs the handler may return from run () before the
// handler, and a later run () calls it; with an executor_work_guard, as the
// documentation tells, run () waits for the handler. An operation destroyed
// uncalled leaves nothing on that context. From the review of WP13 (TK10)
// and of WP12b (the tracked work of round 2 was dropped).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::datagram::message_t;
typedef std::chrono::steady_clock clock_type;
typedef std::chrono::milliseconds ms;
typedef asio::executor_work_guard<asio::io_context::executor_type> guard_type;

std::vector<std::uint8_t>
three ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

// Counts the call and, with a guard, ends the work the test keeps.
struct counting_flag
{
  std::atomic<int> *calls;
  guard_type *guard;

  template <typename... Args>
  void
  operator() (Args &&...)
  {
    calls->fetch_add (1);
    if (guard)
      guard->reset ();
  }
};

long long
elapsed_ms (clock_type::time_point start)
{
  return std::chrono::duration_cast<ms> (clock_type::now () - start).count ();
}

class LumexTransportOutstandingWorkTest : public ::testing::Test
{
protected:
  // Runs other_ until the handler was called (at most two seconds): its
  // completion reaches other_ from the runner's thread.
  void
  run_until_called (int expected)
  {
    clock_type::time_point const start = clock_type::now ();
    while (calls_.load () < expected && elapsed_ms (start) < 2000)
      {
        other_.restart ();
        other_.run_for (ms (10));
      }
  }

  // The context of the handlers outlives the runner: a handler the runner
  // still holds dies before it.
  asio::io_context other_;
  tr::stream::io_runner runner_{ 1 };
  tr::testing::mock_stream stream_
      = tr::testing::make_mock_stream (runner_.get_executor (), "stream");
  tr::testing::mock_datagram datagram_
      = tr::testing::make_mock_datagram (runner_.get_executor (), "datagram");
  std::atomic<int> calls_{ 0 };
};
} // namespace

TEST_F (
    LumexTransportOutstandingWorkTest,
    GivenAReadBoundToAnotherContextWithoutAGuard_WhenThatContextRunsBeforeTheBytes_ThenRunReturnsAndALaterRunCallsTheHandler)
{
  ASSERT_TRUE (stream_.stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  counting_flag const flag = { &calls_, nullptr };
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer),
                                  asio::bind_executor (other_, flag));
  clock_type::time_point const start = clock_type::now ();
  other_.run_for (std::chrono::seconds (5));
  EXPECT_LT (elapsed_ms (start), 4000) << "the stream kept work on other_";
  EXPECT_EQ (calls_.load (), 0);
  stream_.control.feed (three ());
  run_until_called (1);
  EXPECT_EQ (calls_.load (), 1);
}

TEST_F (
    LumexTransportOutstandingWorkTest,
    GivenAReadBoundToAnotherContextWithAWorkGuard_WhenThatContextRunsBeforeTheBytes_ThenRunWaitsForTheHandler)
{
  ASSERT_TRUE (stream_.stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  guard_type guard = asio::make_work_guard (other_);
  counting_flag const flag = { &calls_, &guard };
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer),
                                  asio::bind_executor (other_, flag));
  tr::testing::mock_stream_control control = stream_.control;
  std::thread feeder (
      [control] () mutable
        {
          std::this_thread::sleep_for (ms (100));
          control.feed (three ());
        });
  clock_type::time_point const start = clock_type::now ();
  other_.run_for (std::chrono::seconds (5));
  long long const ran = elapsed_ms (start);
  feeder.join ();
  EXPECT_EQ (calls_.load (), 1) << "run_for returned after " << ran << " ms";
  // The guard ended with the handler: run_for did not wait for its timeout.
  EXPECT_LT (ran, 4000);
}

TEST_F (
    LumexTransportOutstandingWorkTest,
    GivenAReceiveBoundToAnotherContextWithAWorkGuard_WhenThatContextRunsBeforeTheMessage_ThenRunWaitsForTheHandler)
{
  ASSERT_TRUE (datagram_.transport.open (tr::deadline_t (2000)).has_value ());
  guard_type guard = asio::make_work_guard (other_);
  counting_flag const flag = { &calls_, &guard };
  datagram_.transport.async_receive (asio::bind_executor (other_, flag));
  tr::testing::mock_datagram_control control = datagram_.control;
  std::thread sender (
      [control] () mutable
        {
          std::this_thread::sleep_for (ms (100));
          message_t message;
          message.payload = three ();
          control.deliver (message);
        });
  clock_type::time_point const start = clock_type::now ();
  other_.run_for (std::chrono::seconds (5));
  long long const ran = elapsed_ms (start);
  sender.join ();
  EXPECT_EQ (calls_.load (), 1) << "run_for returned after " << ran << " ms";
  EXPECT_LT (ran, 4000);
}

// Every operation, also those that complete at once: with a guard each the
// context of the handlers runs at once and still gets every handler.
TEST_F (
    LumexTransportOutstandingWorkTest,
    GivenEveryOperationBoundToAnotherContextWithAWorkGuard_WhenThatContextRunsAtOnce_ThenItRunsEveryHandler)
{
  std::vector<std::uint8_t> const data = three ();
  for (int operation = 0; operation < 6; ++operation)
    {
      guard_type guard = asio::make_work_guard (other_);
      counting_flag const flag = { &calls_, &guard };
      switch (operation)
        {
        case 0:
          stream_.stream.async_open (asio::bind_executor (other_, flag));
          break;
        case 1:
          stream_.stream.async_write_some (tr::const_bytes_t (data),
                                           asio::bind_executor (other_, flag));
          break;
        case 2:
          stream_.stream.async_write_all (tr::const_bytes_t (data),
                                          asio::bind_executor (other_, flag));
          break;
        case 3:
          datagram_.transport.async_open (asio::bind_executor (other_, flag));
          break;
        case 4:
          datagram_.transport.async_send (message_t (),
                                          asio::bind_executor (other_, flag));
          break;
        default:
          {
            message_t message;
            message.payload = three ();
            datagram_.control.deliver (message);
            datagram_.transport.async_receive (
                asio::bind_executor (other_, flag));
          }
          break;
        }
      other_.restart ();
      other_.run_for (std::chrono::seconds (5));
      ASSERT_EQ (calls_.load (), operation + 1) << "operation " << operation;
    }
}

// An operation destroyed uncalled (its context died with it) leaves nothing
// on the handler's context: that context's run () returns at once.
TEST (LumexTransportOutstandingWorkLifetime,
      GivenAReadDestroyedUncalled_WhenTheHandlerContextRuns_ThenItReturns)
{
  asio::io_context other;
  std::atomic<int> calls (0);
  std::vector<std::uint8_t> buffer (3);
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    counting_flag const flag = { &calls, nullptr };
    mock.stream.async_read_some (tr::mutable_bytes_t (buffer),
                                 asio::bind_executor (other, flag));
  }
  clock_type::time_point const start = clock_type::now ();
  other.run_for (std::chrono::seconds (5));
  EXPECT_EQ (calls.load (), 0);
  EXPECT_LT (elapsed_ms (start), 4000);
}

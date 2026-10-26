// byte_stream::statistics () over the mock stream: every counter is moved by
// the operation that causes it (open, read, write, write_all, each error
// code, deadline, cancel, close, would_deadlock), the wrapper around the
// completion handler keeps the handler's own executor, reset, a moved-from
// stream, and snapshots read from other threads during traffic.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <initializer_list>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::error::transport_errc;
using tr::statistics::statistics_t;
using tr::stream::byte_stream;
using tr::stream::io_runner;
using tr::testing::make_mock_stream;
using tr::testing::mock_stream;

typedef std::chrono::system_clock clock_type;

std::vector<std::uint8_t>
bytes (std::initializer_list<std::uint8_t> values)
{
  return std::vector<std::uint8_t> (values);
}

// A handler that only moves (it owns a unique_ptr).
struct move_only_handler
{
  std::unique_ptr<std::promise<std::size_t>> promise;
  void
  operator() (std::error_code, std::size_t count)
  {
    promise->set_value (count);
  }
};

// Runs `call` (it returns the error code of a synchronous call) on a thread
// of the runner, where the synchronous API must refuse.
template <typename Call>
std::error_code
called_on_runner (io_runner &runner, Call call)
{
  std::promise<std::error_code> result;
  asio::post (runner.get_executor (),
              [&result, &call] { result.set_value (call ()); });
  std::future<std::error_code> got = result.get_future ();
  if (got.wait_for (std::chrono::seconds (2)) != std::future_status::ready)
    return std::error_code ();
  return got.get ();
}

class LumexStreamStatisticsTest : public ::testing::Test
{
protected:
  LumexStreamStatisticsTest ()
      : runner_ (2), mock_ (make_mock_stream (runner_.get_executor (), "m"))
  {
  }

  void
  open ()
  {
    ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  }

  statistics_t
  stats () const
  {
    return mock_.stream.statistics ();
  }

  io_runner runner_;
  mock_stream mock_;
};
} // namespace

TEST_F (LumexStreamStatisticsTest, GivenAFreshStream_WhenAsked_ThenAllZero)
{
  statistics_t const s = stats ();
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.reads, 0u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
}

TEST_F (LumexStreamStatisticsTest,
        GivenAnOpen_WhenItSucceeds_ThenOpensAndLastActivityMove)
{
  clock_type::time_point const before = clock_type::now ();
  open ();
  clock_type::time_point const after = clock_type::now ();
  statistics_t const s = stats ();
  EXPECT_EQ (s.opens, 1u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_FALSE (s.last_activity < before);
  EXPECT_FALSE (after < s.last_activity);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAnOpenThatFailsWithASystemError_WhenOpened_ThenOtherErrorsMoves)
{
  mock_.control.fail_open (
      std::make_error_code (std::errc::permission_denied));
  ASSERT_FALSE (mock_.stream.open (deadline_t (500)).has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.other_errors, 1u);
  EXPECT_EQ (s.total_errors (), 1u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenBytesFed_WhenRead_ThenReadsAndBytesReadMove)
{
  open ();
  mock_.control.feed (bytes ({ 1, 2, 3 }));
  std::vector<std::uint8_t> buffer (8);
  ASSERT_TRUE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  mock_.control.feed (bytes ({ 4, 5 }));
  ASSERT_TRUE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.reads, 2u);
  EXPECT_EQ (s.bytes_read, 5u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenSmallWriteChunks_WhenWriteSomeAndWriteAll_ThenOneWriteEach)
{
  open ();
  mock_.control.set_write_chunk (3);
  std::vector<std::uint8_t> const data
      = bytes ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
  ASSERT_TRUE (
      mock_.stream.write_some (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  statistics_t s = stats ();
  EXPECT_EQ (s.writes, 1u);
  EXPECT_EQ (s.bytes_written, 3u);
  // write_all takes four writes of the mock, and counts as one.
  ASSERT_TRUE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  s = stats ();
  EXPECT_EQ (s.writes, 2u);
  EXPECT_EQ (s.bytes_written, 13u);
  EXPECT_EQ (s.total_errors (), 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAFailingWrite_WhenWriteAll_ThenTheErrorIsCountedNotAWrite)
{
  open ();
  mock_.control.fail_next_write (transport_errc::disconnected);
  std::vector<std::uint8_t> const data = bytes ({ 1, 2 });
  ASSERT_FALSE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::disconnected), 1u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenEveryTransportErrc_WhenAReadFailsWithIt_ThenItsSlotMoves)
{
  open ();
  std::vector<std::uint8_t> buffer (4);
  for (int value = 1; value <= 10; ++value)
    {
      transport_errc const code = static_cast<transport_errc> (value);
      mock_.stream.reset_statistics ();
      mock_.control.fail_next_read (code);
      ASSERT_FALSE (
          mock_.stream
              .read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
              .has_value ())
          << value;
      statistics_t const s = stats ();
      EXPECT_EQ (s.errors_of (code), 1u) << value;
      EXPECT_EQ (s.total_errors (), 1u) << value;
      EXPECT_EQ (s.other_errors, 0u) << value;
      EXPECT_EQ (s.reads, 0u) << value;
    }
}

TEST_F (LumexStreamStatisticsTest,
        GivenNoBytes_WhenTheDeadlinePasses_ThenOneTimeoutAndNoCancellation)
{
  open ();
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result
      = mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (30));
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error () == transport_errc::timeout);
  statistics_t const s = stats ();
  EXPECT_EQ (s.timeouts, 1u);
  EXPECT_EQ (s.errors_of (transport_errc::timeout), 1u);
  EXPECT_EQ (s.cancellations, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::cancelled), 0u);
  EXPECT_EQ (s.total_errors (), 1u);
  EXPECT_EQ (s.reads, 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenABlockedRead_WhenAnotherThreadCancels_ThenOneCancellation)
{
  open ();
  std::thread canceller (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          mock_.stream.cancel ();
        });
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (5000));
  canceller.join ();
  ASSERT_FALSE (result.has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.cancellations, 1u);
  EXPECT_EQ (s.errors_of (transport_errc::cancelled), 1u);
  EXPECT_EQ (s.timeouts, 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenABlockedRead_WhenAnotherThreadCloses_ThenOneCancellation)
{
  open ();
  std::thread closer (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          mock_.stream.close ();
        });
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (5000));
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (stats ().cancellations, 1u);
  EXPECT_EQ (stats ().timeouts, 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAClosedStream_WhenReadAndWritten_ThenNotOpenCountsTwice)
{
  std::vector<std::uint8_t> buffer (4);
  EXPECT_FALSE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (200))
          .has_value ());
  std::vector<std::uint8_t> const data = bytes ({ 1 });
  EXPECT_FALSE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (200))
          .has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.errors_of (transport_errc::not_open), 2u);
  EXPECT_EQ (s.total_errors (), 2u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenADisconnect_WhenReading_ThenDisconnectedCounts)
{
  open ();
  mock_.control.disconnect ();
  std::vector<std::uint8_t> buffer (4);
  EXPECT_FALSE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  EXPECT_EQ (stats ().errors_of (transport_errc::disconnected), 1u);
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenAHandlerOnTheRunnerThread_WhenItCallsTheSyncApi_ThenWouldDeadlockCounts)
{
  open ();
  mock_.control.feed (bytes ({ 1 }));
  std::vector<std::uint8_t> buffer (4);
  std::promise<void> inner;
  byte_stream &stream = mock_.stream;
  stream.async_read_some (tr::mutable_bytes_t (buffer),
                          [&stream, &inner] (std::error_code, std::size_t)
                            {
                              std::vector<std::uint8_t> other (4);
                              stream.read_some (tr::mutable_bytes_t (other),
                                                deadline_t (1000));
                              inner.set_value ();
                            });
  std::future<void> done = inner.get_future ();
  ASSERT_EQ (done.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAnAsyncRead_WhenTheHandlerRuns_ThenTheCountIsAlreadyInTheSnapshot)
{
  open ();
  mock_.control.feed (bytes ({ 9, 9 }));
  std::vector<std::uint8_t> buffer (4);
  std::promise<statistics_t> seen;
  byte_stream &stream = mock_.stream;
  stream.async_read_some (tr::mutable_bytes_t (buffer),
                          [&stream, &seen] (std::error_code, std::size_t)
                            { seen.set_value (stream.statistics ()); });
  std::future<statistics_t> got = seen.get_future ();
  ASSERT_EQ (got.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  statistics_t const s = got.get ();
  EXPECT_EQ (s.reads, 1u);
  EXPECT_EQ (s.bytes_read, 2u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAHandlerBoundToAnotherExecutor_WhenItCompletes_ThenItRunsThere)
{
  open ();
  mock_.control.feed (bytes ({ 1 }));
  asio::thread_pool other (1);
  std::promise<std::thread::id> other_id;
  asio::post (other, [&other_id]
                { other_id.set_value (std::this_thread::get_id ()); });
  std::thread::id const expected = other_id.get_future ().get ();

  std::vector<std::uint8_t> buffer (4);
  std::promise<std::thread::id> ran_on;
  mock_.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_executor (
          other, [&ran_on] (std::error_code, std::size_t)
            { ran_on.set_value (std::this_thread::get_id ()); }));
  std::future<std::thread::id> got = ran_on.get_future ();
  ASSERT_EQ (got.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_TRUE (got.get () == expected);
  EXPECT_EQ (stats ().reads, 1u);
  other.join ();
}

TEST_F (LumexStreamStatisticsTest,
        GivenAMoveOnlyHandler_WhenAWriteCompletes_ThenItIsCalledAndCounted)
{
  open ();
  std::vector<std::uint8_t> const data = bytes ({ 1, 2, 3 });
  move_only_handler handler = { std::unique_ptr<std::promise<std::size_t>> (
      new std::promise<std::size_t> ()) };
  std::future<std::size_t> got = handler.promise->get_future ();
  mock_.stream.async_write_all (tr::const_bytes_t (data), std::move (handler));
  ASSERT_EQ (got.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (got.get (), 3u);
  EXPECT_EQ (stats ().bytes_written, 3u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenCountedTraffic_WhenReset_ThenZeroAndCountingContinues)
{
  open ();
  mock_.control.feed (bytes ({ 1, 2 }));
  std::vector<std::uint8_t> buffer (4);
  ASSERT_TRUE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  std::vector<std::uint8_t> const data = bytes ({ 1 });
  ASSERT_TRUE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  EXPECT_FALSE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (20))
          .has_value ());
  mock_.stream.reset_statistics ();
  statistics_t s = stats ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.reads, 0u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.timeouts, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
  mock_.control.feed (bytes ({ 7 }));
  ASSERT_TRUE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  s = stats ();
  EXPECT_EQ (s.reads, 1u);
  EXPECT_EQ (s.bytes_read, 1u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAMovedFromStream_WhenAsked_ThenAnEmptySnapshotAndNoCrash)
{
  open ();
  byte_stream moved (std::move (mock_.stream));
  EXPECT_EQ (moved.statistics ().opens, 1u);
  statistics_t const s = mock_.stream.statistics ();
  EXPECT_EQ (s.opens, 0u);
  mock_.stream.reset_statistics ();
}

TEST_F (LumexStreamStatisticsTest,
        GivenTrafficOnTheRunner_WhenOtherThreadsTakeSnapshots_ThenTheyOnlyGrow)
{
  open ();
  std::atomic<bool> stop (false);
  std::atomic<int> violations (0);
  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i)
    readers.emplace_back (
        [this, &stop, &violations]
          {
            std::uint64_t reads = 0;
            std::uint64_t written = 0;
            while (!stop.load ())
              {
                statistics_t const s = mock_.stream.statistics ();
                if (s.reads < reads || s.bytes_written < written)
                  violations.fetch_add (1);
                reads = s.reads;
                written = s.bytes_written;
              }
          });
  std::size_t const rounds = 200;
  std::vector<std::uint8_t> const data = bytes ({ 1, 2, 3, 4 });
  std::vector<std::uint8_t> buffer (4);
  for (std::size_t round = 0; round < rounds; ++round)
    {
      ASSERT_TRUE (
          mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (1000))
              .has_value ());
      mock_.control.feed (data);
      ASSERT_TRUE (
          mock_.stream
              .read_some (tr::mutable_bytes_t (buffer), deadline_t (1000))
              .has_value ());
    }
  stop.store (true);
  for (std::thread &reader : readers)
    reader.join ();
  EXPECT_EQ (violations.load (), 0);
  statistics_t const s = stats ();
  EXPECT_EQ (s.reads, rounds);
  EXPECT_EQ (s.writes, rounds);
  EXPECT_EQ (s.bytes_read, rounds * 4);
  EXPECT_EQ (s.bytes_written, rounds * 4);
  EXPECT_EQ (s.total_errors (), 0u);
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenTheRunnerThread_WhenItCallsOpen_ThenWouldDeadlockCountsAndNothingElse)
{
  std::error_code const error = called_on_runner (
      runner_,
      [this]
        {
          tr::status_result_t const result
              = mock_.stream.open (deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().opens, 0u);
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenTheRunnerThread_WhenItCallsReadSome_ThenWouldDeadlockCountsAndNothingElse)
{
  open ();
  std::vector<std::uint8_t> buffer (4);
  std::error_code const error = called_on_runner (
      runner_,
      [this, &buffer]
        {
          tr::io_result_t const result = mock_.stream.read_some (
              tr::mutable_bytes_t (buffer), deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().reads, 0u);
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenTheRunnerThread_WhenItCallsWriteSome_ThenWouldDeadlockCountsAndNothingElse)
{
  open ();
  std::vector<std::uint8_t> const data = bytes ({ 1, 2 });
  std::error_code const error = called_on_runner (
      runner_,
      [this, &data]
        {
          tr::io_result_t const result = mock_.stream.write_some (
              tr::const_bytes_t (data), deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().writes, 0u);
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenTheRunnerThread_WhenItCallsWriteAll_ThenWouldDeadlockCountsAndNothingElse)
{
  open ();
  std::vector<std::uint8_t> const data = bytes ({ 1, 2 });
  std::error_code const error = called_on_runner (
      runner_,
      [this, &data]
        {
          tr::status_result_t const result = mock_.stream.write_all (
              tr::const_bytes_t (data), deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().writes, 0u);
}

TEST_F (LumexStreamStatisticsTest,
        GivenAFreshStream_WhenOnlyOpened_ThenLastActivityIsSet)
{
  ASSERT_TRUE (stats ().last_activity == clock_type::time_point ());
  open ();
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (LumexStreamStatisticsTest,
        GivenResetCounters_WhenOnlyAReadSucceeds_ThenLastActivityIsSet)
{
  open ();
  mock_.control.feed (bytes ({ 1 }));
  mock_.stream.reset_statistics ();
  ASSERT_TRUE (stats ().last_activity == clock_type::time_point ());
  std::vector<std::uint8_t> buffer (4);
  ASSERT_TRUE (
      mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (500))
          .has_value ());
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (LumexStreamStatisticsTest,
        GivenResetCounters_WhenOnlyAWriteSomeSucceeds_ThenLastActivityIsSet)
{
  open ();
  mock_.stream.reset_statistics ();
  std::vector<std::uint8_t> const data = bytes ({ 1 });
  ASSERT_TRUE (
      mock_.stream.write_some (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (LumexStreamStatisticsTest,
        GivenResetCounters_WhenOnlyAWriteAllSucceeds_ThenLastActivityIsSet)
{
  open ();
  mock_.stream.reset_statistics ();
  std::vector<std::uint8_t> const data = bytes ({ 1 });
  ASSERT_TRUE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (
    LumexStreamStatisticsTest,
    GivenAChunkedWriteAllThatFailsOnItsSecondWrite_WhenCounted_ThenTheFirstChunkCountsAsBytesOnly)
{
  open ();
  mock_.control.set_write_chunk (3);
  mock_.control.fail_write_after (1, transport_errc::disconnected);
  mock_.stream.reset_statistics ();
  std::vector<std::uint8_t> const data
      = bytes ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
  tr::status_result_t const result
      = mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::disconnected);
  statistics_t const s = stats ();
  EXPECT_EQ (s.bytes_written, 3u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::disconnected), 1u);
  EXPECT_EQ (s.total_errors (), 1u);
  // The bytes that went out are activity, though the write failed.
  EXPECT_FALSE (s.last_activity == clock_type::time_point ());
  EXPECT_EQ (mock_.control.written ().size (), 3u);
}

// byte_stream (lumex/applied/transport/stream) over the mock stream: the
// synchronous API with deadlines, cancel and close from another thread (the
// regression of a blocked read that close must end within 200 ms), write_all
// in chunks, the asynchronous API with callbacks and futures, completion never
// inside the initiating call, would_deadlock on the runner's thread, and the
// front destroyed inside its own handler.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::error::transport_errc;
using tr::stream::byte_stream;
using tr::stream::io_runner;
using tr::testing::make_mock_stream;
using tr::testing::mock_stream;

using clock_type = std::chrono::steady_clock;

std::vector<std::uint8_t>
bytes (std::initializer_list<std::uint8_t> values)
{
  return std::vector<std::uint8_t> (values);
}

class LumexByteStreamTest : public ::testing::Test
{
protected:
  LumexByteStreamTest ()
      : runner_ (2), mock_ (make_mock_stream (runner_.get_executor (), "m"))
  {
  }

  io_runner runner_;
  mock_stream mock_;
};

long long
elapsed_ms (clock_type::time_point start)
{
  return std::chrono::duration_cast<std::chrono::milliseconds> (
             clock_type::now () - start)
      .count ();
}
} // namespace

TEST_F (LumexByteStreamTest,
        GivenAnOpenStream_WhenBytesArriveLater_ThenTheSyncReadReturnsThem)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  EXPECT_TRUE (mock_.stream.is_open ());
  std::thread feeder (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (20));
          mock_.control.feed (bytes ({ 1, 2, 3 }));
        });
  std::vector<std::uint8_t> buffer (8);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (1000));
  feeder.join ();
  ASSERT_TRUE (result.has_value ());
  ASSERT_EQ (result.value (), 3u);
  EXPECT_EQ (buffer[0], 1);
  EXPECT_EQ (buffer[2], 3);
}

TEST_F (LumexByteStreamTest,
        GivenNoBytes_WhenTheDeadlinePasses_ThenTimeoutWithinTheBudget)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  clock_type::time_point const start = clock_type::now ();
  tr::io_result_t const result
      = mock_.stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (50));
  long long const took = elapsed_ms (start);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout);
  EXPECT_GE (took, 45);
  EXPECT_LT (took, 1000);
}

TEST_F (LumexByteStreamTest, GivenAClosedStream_WhenRead_ThenNotOpen)
{
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (200));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::not_open);
}

TEST_F (LumexByteStreamTest,
        GivenABlockedSyncRead_WhenAnotherThreadCancels_ThenCancelled)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
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
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_TRUE (mock_.stream.is_open ());
}

TEST_F (LumexByteStreamTest,
        GivenABlockedSyncRead_WhenAnotherThreadCloses_ThenItEndsWithin200Ms)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  clock_type::time_point closed_at;
  std::thread closer (
      [this, &closed_at]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          closed_at = clock_type::now ();
          mock_.stream.close ();
        });
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (10000));
  clock_type::time_point const returned_at = clock_type::now ();
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_LT (std::chrono::duration_cast<std::chrono::milliseconds> (
                 returned_at - closed_at)
                 .count (),
             200);
  EXPECT_FALSE (mock_.stream.is_open ());
}

TEST_F (LumexByteStreamTest,
        GivenAClosedStream_WhenClosedAgainAndUsed_ThenIdempotentAndNotOpen)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.stream.close ();
  mock_.stream.close ();
  std::vector<std::uint8_t> const data = bytes ({ 1 });
  tr::io_result_t const result
      = mock_.stream.write_some (tr::const_bytes_t (data), deadline_t (200));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::not_open);
}

TEST_F (LumexByteStreamTest,
        GivenSmallWriteChunks_WhenWriteAll_ThenEveryByteIsWrittenInOrder)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.control.set_write_chunk (3);
  std::vector<std::uint8_t> const data
      = bytes ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
  ASSERT_TRUE (
      mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500))
          .has_value ());
  EXPECT_EQ (mock_.control.written (), data);
  tr::io_result_t const one
      = mock_.stream.write_some (tr::const_bytes_t (data), deadline_t (500));
  ASSERT_TRUE (one.has_value ());
  EXPECT_EQ (one.value (), 3u);
}

TEST_F (LumexByteStreamTest,
        GivenAWriteThatFails_WhenWriteAll_ThenTheErrorIsReturned)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.control.fail_next_write (transport_errc::disconnected);
  std::vector<std::uint8_t> const data = bytes ({ 1, 2 });
  tr::status_result_t const result
      = mock_.stream.write_all (tr::const_bytes_t (data), deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::disconnected);
}

TEST_F (LumexByteStreamTest,
        GivenAnOpenThatFails_WhenOpened_ThenTheErrorIsReturned)
{
  mock_.control.fail_open (
      std::make_error_code (std::errc::permission_denied));
  tr::status_result_t const result = mock_.stream.open (deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == std::errc::permission_denied);
  EXPECT_FALSE (mock_.stream.is_open ());
}

TEST_F (LumexByteStreamTest, GivenADisconnect_WhenReading_ThenDisconnected)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.control.disconnect ();
  std::vector<std::uint8_t> buffer (4);
  tr::io_result_t const result = mock_.stream.read_some (
      tr::mutable_bytes_t (buffer), deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::disconnected);
}

TEST_F (LumexByteStreamTest,
        GivenACallback_WhenAnAsyncReadStarts_ThenItNeverRunsInsideTheCall)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.control.feed (bytes ({ 9 }));
  std::vector<std::uint8_t> buffer (4);
  std::promise<std::size_t> got;
  std::atomic<bool> inside (true);
  mock_.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      [&got, &inside] (std::error_code error, std::size_t count)
        {
          EXPECT_FALSE (inside.load ());
          EXPECT_FALSE (error);
          got.set_value (count);
        });
  inside.store (false);
  std::future<std::size_t> result = got.get_future ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), 1u);
}

TEST_F (LumexByteStreamTest,
        GivenUseFuture_WhenAnAsyncWriteCompletes_ThenTheFutureHasTheCount)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  std::vector<std::uint8_t> const data = bytes ({ 1, 2, 3, 4 });
  std::future<std::size_t> written = mock_.stream.async_write_all (
      tr::const_bytes_t (data), asio::use_future);
  ASSERT_EQ (written.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (written.get (), 4u);
}

TEST_F (LumexByteStreamTest,
        GivenAHandlerOnTheRunnerThread_WhenItCallsTheSyncApi_ThenWouldDeadlock)
{
  ASSERT_TRUE (mock_.stream.open (deadline_t (500)).has_value ());
  mock_.control.feed (bytes ({ 1 }));
  std::vector<std::uint8_t> buffer (4);
  std::promise<std::error_code> inner;
  byte_stream &stream = mock_.stream;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      [&stream, &inner] (std::error_code, std::size_t)
        {
          std::vector<std::uint8_t> other (4);
          tr::io_result_t const nested = stream.read_some (
              tr::mutable_bytes_t (other), deadline_t (1000));
          inner.set_value (nested.has_value () ? std::error_code ()
                                               : nested.error ());
        });
  std::future<std::error_code> result = inner.get_future ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_TRUE (result.get () == transport_errc::would_deadlock);
}

TEST_F (
    LumexByteStreamTest,
    GivenAFrontDestroyedInsideItsHandler_WhenTheHandlerRuns_ThenNothingBreaks)
{
  mock_stream local = make_mock_stream (runner_.get_executor (), "local");
  std::unique_ptr<byte_stream> owned (
      new byte_stream (std::move (local.stream)));
  ASSERT_TRUE (owned->open (deadline_t (500)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  std::promise<void> ran;
  owned->async_read_some (tr::mutable_bytes_t (buffer),
                          [&owned, &ran] (std::error_code, std::size_t)
                            {
                              owned.reset ();
                              ran.set_value ();
                            });
  local.control.feed (bytes ({ 5 }));
  std::future<void> done = ran.get_future ();
  ASSERT_EQ (done.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (owned, nullptr);
}

TEST (LumexIoRunnerTest,
      GivenFourThreads_WhenManyStreamsWorkAtOnce_ThenEveryOperationCompletes)
{
  io_runner runner (4);
  EXPECT_EQ (runner.thread_count (), 4u);
  std::vector<mock_stream> mocks;
  for (int i = 0; i < 8; ++i)
    mocks.push_back (make_mock_stream (runner.get_executor (), "s"));
  std::vector<std::thread> users;
  std::atomic<int> ok (0);
  for (mock_stream &mock : mocks)
    users.emplace_back (
        [&mock, &ok]
          {
            if (!mock.stream.open (deadline_t (1000)).has_value ())
              return;
            for (int round = 0; round < 50; ++round)
              {
                std::vector<std::uint8_t> const data
                    = bytes ({ static_cast<std::uint8_t> (round) });
                if (!mock.stream
                         .write_all (tr::const_bytes_t (data),
                                     deadline_t (1000))
                         .has_value ())
                  return;
                mock.control.feed (data);
                std::vector<std::uint8_t> buffer (1);
                if (!mock.stream
                         .read_some (tr::mutable_bytes_t (buffer),
                                     deadline_t (1000))
                         .has_value ())
                  return;
              }
            ok.fetch_add (1);
          });
  for (std::thread &user : users)
    user.join ();
  EXPECT_EQ (ok.load (), 8);
}

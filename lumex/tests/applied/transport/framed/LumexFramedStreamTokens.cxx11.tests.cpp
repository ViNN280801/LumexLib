// The completion token styles of framed_stream (callbacks, asio::use_future,
// asio::deferred, asio::yield_context with LUMEX_TRANSPORT_WITH_SPAWN), the
// synchronous API with deadlines (timeout, cancel and close from another
// thread, would_deadlock on the runner's thread), and a stress run of
// asio::cancel_after against frames that arrive at the same time, over the
// mock stream on an io_runner. The C++20 suite adds asio::use_awaitable
// (LumexFramedStreamAwaitable.cxx20.tests.cpp). Synthetic frames.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <random>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace
{
using namespace lumex_framed_test;
using tr::error::transport_errc;

class LumexFramedStreamTokenTest : public ::testing::Test
{
protected:
  LumexFramedStreamTokenTest () : rig_ (slip_case::limit ()) {}

  void
  SetUp () override
  {
    rig_.open ();
  }

  // The device answers a frame with the same frame.
  void
  echo_last_write (std::size_t from)
  {
    bytes_t const written = rig_.mock.control.written ();
    rig_.mock.control.feed (
        bytes_t (written.begin () + static_cast<std::ptrdiff_t> (from),
                 written.end ()));
  }

  log_capture log_;
  stream_rig<slip_case> rig_;
  bytes_t payload_ = bytes_t (5, 'q');
};
} // namespace

TEST_F (LumexFramedStreamTokenTest,
        GivenCallbacks_WhenWriteAndRead_ThenTheFrameComesBack)
{
  std::promise<std::error_code> written;
  rig_.link.async_write_frame (tr::const_bytes_t (payload_),
                               [&written] (std::error_code error)
                                 { written.set_value (error); });
  ASSERT_FALSE (written.get_future ().get ());
  echo_last_write (0);
  std::promise<bytes_t> read;
  rig_.link.async_read_frame (
      [&read] (std::error_code error, tr::framed::frame_t frame)
        {
          if (error)
            frame.clear ();
          read.set_value (frame);
        });
  EXPECT_EQ (read.get_future ().get (), payload_);
}

TEST_F (LumexFramedStreamTokenTest,
        GivenUseFuture_WhenWriteAndRead_ThenTheFrameComesBack)
{
  rig_.link.async_write_frame (tr::const_bytes_t (payload_), asio::use_future)
      .get ();
  echo_last_write (0);
  EXPECT_EQ (rig_.link.async_read_frame (asio::use_future).get (), payload_);
}

TEST_F (LumexFramedStreamTokenTest,
        GivenUseFuture_WhenTheOperationFails_ThenGetThrowsTheCode)
{
  bytes_t const big = slip_case::oversized ();
  try
    {
      rig_.link.async_write_frame (tr::const_bytes_t (big), asio::use_future)
          .get ();
      FAIL () << "a payload beyond the limit was written";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == framing_errc::payload_too_large);
    }
  rig_.link.close ();
  try
    {
      rig_.link.async_read_frame (asio::use_future).get ();
      FAIL () << "a read of a closed stream succeeded";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::not_open);
    }
}

TEST_F (LumexFramedStreamTokenTest,
        GivenDeferred_WhenTheOperationIsStartedLater_ThenItRunsThen)
{
  auto write = rig_.link.async_write_frame (tr::const_bytes_t (payload_),
                                            asio::deferred);
  // Nothing was written by creating the operation.
  std::this_thread::sleep_for (ms (20));
  EXPECT_TRUE (rig_.mock.control.written ().empty ());
  std::promise<std::error_code> written;
  std::move (write) ([&written] (std::error_code error)
                       { written.set_value (error); });
  ASSERT_FALSE (written.get_future ().get ());
  echo_last_write (0);
  auto read = rig_.link.async_read_frame (asio::deferred);
  std::promise<bytes_t> got;
  std::move (read) ([&got] (std::error_code, tr::framed::frame_t frame)
                      { got.set_value (frame); });
  EXPECT_EQ (got.get_future ().get (), payload_);
}

TEST_F (LumexFramedStreamTokenTest,
        GivenYieldContext_WhenWriteAndRead_ThenTheFrameComesBack)
{
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  std::promise<std::error_code> done;
  bytes_t frame;
  asio::spawn (
      rig_.runner.get_executor (),
      [&] (asio::yield_context yield)
        {
          std::error_code error;
          rig_.link.async_write_frame (tr::const_bytes_t (payload_),
                                       yield[error]);
          if (!error)
            {
              echo_last_write (0);
              frame = rig_.link.async_read_frame (yield[error]);
            }
          done.set_value (error);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  std::error_code const error = done.get_future ().get ();
  ASSERT_FALSE (error) << error.message ();
  EXPECT_EQ (frame, payload_);
#else
  GTEST_SKIP () << "LUMEX_TRANSPORT_WITH_SPAWN is off";
#endif
}

// --- the synchronous API
// -----------------------------------------------------

TEST_F (
    LumexFramedStreamTokenTest,
    GivenNoFrame_WhenTheDeadlinePasses_ThenTimeoutWithinTheBudgetAndTheAdapterWorksAfter)
{
  clock_type::time_point const start = clock_type::now ();
  tr::framed::frame_result_t const result
      = rig_.link.read_frame (tr::deadline_t (50));
  long long const elapsed
      = std::chrono::duration_cast<ms> (clock_type::now () - start).count ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout)
      << result.error ().message ();
  EXPECT_GE (elapsed, 40);
  EXPECT_LT (elapsed, 1000);
  // Counted once, as a timeout of the adapter.
  EXPECT_EQ (rig_.link.statistics ().timeouts, 1u);
  rig_.mock.control.feed (slip_wire (1, 4));
  tr::framed::frame_result_t const next
      = rig_.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (next.has_value ()) << next.error ().message ();
  EXPECT_EQ (next.value (), bytes_t (1, 4));
}

TEST_F (
    LumexFramedStreamTokenTest,
    GivenABlockedSyncRead_WhenCancelIsCalledFromAnotherThread_ThenItEndsCancelledWithinTheBudget)
{
  std::thread canceller (
      [this]
        {
          std::this_thread::sleep_for (ms (30));
          rig_.link.cancel ();
        });
  clock_type::time_point const start = clock_type::now ();
  tr::framed::frame_result_t const result
      = rig_.link.read_frame (tr::deadline_t (10000));
  long long const elapsed
      = std::chrono::duration_cast<ms> (clock_type::now () - start).count ();
  canceller.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled)
      << result.error ().message ();
  EXPECT_LT (elapsed, 1000);
}

TEST_F (
    LumexFramedStreamTokenTest,
    GivenABlockedSyncRead_WhenCloseIsCalledFromAnotherThread_ThenItEndsWithinTheBudget)
{
  std::thread closer (
      [this]
        {
          std::this_thread::sleep_for (ms (30));
          rig_.link.close ();
        });
  clock_type::time_point const start = clock_type::now ();
  tr::framed::frame_result_t const result
      = rig_.link.read_frame (tr::deadline_t (10000));
  long long const elapsed
      = std::chrono::duration_cast<ms> (clock_type::now () - start).count ();
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled)
      << result.error ().message ();
  EXPECT_LT (elapsed, 200);
  EXPECT_FALSE (rig_.link.is_open ());
}

TEST_F (
    LumexFramedStreamTokenTest,
    GivenASyncCallOnAThreadOfTheRunner_WhenMade_ThenWouldDeadlockComesBackAtOnce)
{
  std::promise<std::error_code> read_error;
  std::promise<std::error_code> write_error;
  asio::post (rig_.runner.get_executor (),
              [this, &read_error, &write_error]
                {
                  read_error.set_value (
                      rig_.link.read_frame (tr::deadline_t (5000)).error ());
                  write_error.set_value (
                      rig_.link
                          .write_frame (tr::const_bytes_t (payload_),
                                        tr::deadline_t (5000))
                          .error ());
                });
  EXPECT_TRUE (read_error.get_future ().get ()
               == transport_errc::would_deadlock);
  EXPECT_TRUE (write_error.get_future ().get ()
               == transport_errc::would_deadlock);
  // The refusal reached no operation: neither the transport nor the adapter
  // counts it.
  EXPECT_EQ (rig_.link.transport_statistics ().total_errors (), 0u);
  EXPECT_EQ (rig_.link.statistics ().total_errors (), 0u);
  EXPECT_EQ (rig_.link.statistics ().timeouts, 0u);
  EXPECT_TRUE (rig_.mock.control.written ().empty ());
}

TEST_F (LumexFramedStreamTokenTest,
        GivenAWriteThatFails_WhenWrittenSynchronously_ThenTheErrorComesBack)
{
  rig_.mock.control.fail_next_write (
      std::make_error_code (std::errc::io_error));
  tr::status_result_t const result = rig_.link.write_frame (
      tr::const_bytes_t (payload_), tr::deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == std::errc::io_error);
  EXPECT_EQ (rig_.link.statistics ().frames_out, 0u);
  EXPECT_TRUE (
      rig_.link
          .write_frame (tr::const_bytes_t (payload_), tr::deadline_t (2000))
          .has_value ());
}

TEST (LumexFramedStreamClosedTest,
      GivenAStreamThatIsNotOpen_WhenWritten_ThenNotOpenComesBack)
{
  log_capture log;
  stream_rig<slip_case> rig;
  bytes_t const payload (3, 'z');
  tr::status_result_t const result = rig.link.write_frame (
      tr::const_bytes_t (payload), tr::deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::not_open)
      << result.error ().message ();
  EXPECT_EQ (rig.link.statistics ().frames_out, 0u);
}

// --- asio::cancel_after against arriving frames
// ------------------------------

namespace
{
// A frame is two bytes: its number, big end first.
bytes_t
numbered_wire (std::uint32_t number)
{
  bytes_t payload;
  payload.push_back (static_cast<std::uint8_t> (number >> 8u));
  payload.push_back (static_cast<std::uint8_t> (number));
  return encode_direct (slip_case::make (slip_case::limit ()), payload);
}

struct stress_handler
{
  std::shared_ptr<std::promise<std::error_code>> done;
  std::shared_ptr<tr::framed::frame_t> frame;
  std::atomic<int> *doubles;
  std::shared_ptr<std::atomic<int>> calls;

  void
  operator() (std::error_code error, tr::framed::frame_t value)
  {
    if (calls->fetch_add (1) != 0)
      {
        doubles->fetch_add (1);
        return;
      }
    *frame = std::move (value);
    done->set_value (error);
  }
};
} // namespace

// Each read has a timer of 0..60 us and a frame arrives at the same time: the
// read ends with the frame or with cancelled, every handler runs once, and no
// frame is lost or repeated or reordered (a frame a cancelled read did not
// take waits in the queue for the next one).
TEST (
    LumexFramedStreamStress,
    GivenCancelAfterOnFourThreads_WhenFramesArriveAtTheSameTime_ThenNoFrameIsLostOrRepeated)
{
  log_capture log;
  tr::stream::io_runner runner (4);
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "stress");
  tr::framed::framed_options_t options;
  options.queue_bound = 8192;
  tr::framed::framed_stream<slip_case::codec_t> link (
      std::move (mock.stream), slip_case::make (slip_case::limit ()), options);
  ASSERT_TRUE (link.transport ().open (tr::deadline_t (2000)).has_value ());
  asio::strand<tr::executor_type> const strand
      = asio::make_strand (runner.get_executor ());
  std::mt19937 random (4242);
  std::uniform_int_distribution<int> delay (0, 60);
  std::atomic<int> doubles (0);
  std::vector<std::uint32_t> got;
  int const iterations = 2000;
  int hung = 0;
  for (int i = 0; i < iterations; ++i)
    {
      mock.control.feed (numbered_wire (static_cast<std::uint32_t> (i)));
      stress_handler handler
          = { std::make_shared<std::promise<std::error_code>> (),
              std::make_shared<tr::framed::frame_t> (), &doubles,
              std::make_shared<std::atomic<int>> (0) };
      std::future<std::error_code> ended = handler.done->get_future ();
      std::chrono::microseconds const timeout (delay (random));
      // asio::cancel_after must be started inside a strand (Asio's documented
      // thread safety): a step of the strand the timer's handler runs on,
      // the associated executor of the handler.
      asio::post (strand,
                  [&link, timeout, &strand, handler] ()
                    {
                      link.async_read_frame (asio::cancel_after (
                          timeout, asio::bind_executor (strand, handler)));
                    });
      if (ended.wait_for (std::chrono::seconds (5))
          != std::future_status::ready)
        {
          ++hung;
          break;
        }
      std::error_code const error = ended.get ();
      if (!error)
        {
          ASSERT_EQ (handler.frame->size (), 2u);
          got.push_back (static_cast<std::uint32_t> ((*handler.frame)[0]) << 8u
                         | (*handler.frame)[1]);
        }
      else
        ASSERT_TRUE (error == transport_errc::cancelled) << error.message ();
    }
  ASSERT_EQ (hung, 0);
  EXPECT_EQ (doubles.load (), 0);
  // Whatever the cancelled reads left is in the queue, in order.
  for (;;)
    {
      tr::framed::frame_result_t const frame
          = link.read_frame (tr::deadline_t (200));
      if (!frame.has_value ())
        break;
      got.push_back (static_cast<std::uint32_t> (frame.value ()[0]) << 8u
                     | frame.value ()[1]);
    }
  ASSERT_EQ (got.size (), static_cast<std::size_t> (iterations));
  for (std::size_t i = 0; i < got.size (); ++i)
    ASSERT_EQ (got[i], static_cast<std::uint32_t> (i)) << "position " << i;
  EXPECT_EQ (link.statistics ().dropped_frames, 0u);
}

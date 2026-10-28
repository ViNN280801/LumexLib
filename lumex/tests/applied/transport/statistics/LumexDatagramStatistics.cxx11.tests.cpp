// message_transport::statistics () over the mock datagram: opens, messages
// and payload bytes in both directions, each error code, deadline, cancel,
// close, would_deadlock, the handler's own executor, reset, a moved-from
// transport, and snapshots read from other threads during traffic.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::datagram::message_t;
using tr::datagram::message_transport;
using tr::error::transport_errc;
using tr::statistics::statistics_t;
using tr::stream::io_runner;
using tr::testing::make_mock_datagram;
using tr::testing::mock_datagram;

using clock_type = std::chrono::system_clock;

message_t
message (std::size_t payload_size)
{
  message_t result;
  result.payload.assign (payload_size, std::uint8_t (0x5A));
  return result;
}

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

class LumexDatagramStatisticsTest : public ::testing::Test
{
protected:
  LumexDatagramStatisticsTest ()
      : runner_ (2), mock_ (make_mock_datagram (runner_.get_executor (), "d"))
  {
  }

  void
  open ()
  {
    ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  }

  statistics_t
  stats () const
  {
    return mock_.transport.statistics ();
  }

  io_runner runner_;
  mock_datagram mock_;
};
} // namespace

TEST_F (LumexDatagramStatisticsTest,
        GivenAFreshTransport_WhenAsked_ThenAllZero)
{
  statistics_t const s = stats ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.messages_received, 0u);
  EXPECT_EQ (s.messages_sent, 0u);
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAnOpen_WhenItSucceeds_ThenOpensAndLastActivityMove)
{
  clock_type::time_point const before = clock_type::now ();
  open ();
  clock_type::time_point const after = clock_type::now ();
  statistics_t const s = stats ();
  EXPECT_EQ (s.opens, 1u);
  EXPECT_FALSE (s.last_activity < before);
  EXPECT_FALSE (after < s.last_activity);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAnOpenThatFails_WhenOpened_ThenTheErrorIsCountedNotTheOpen)
{
  mock_.control.fail_open (transport_errc::permission_denied);
  ASSERT_FALSE (mock_.transport.open (deadline_t (500)).has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::permission_denied), 1u);
  mock_.control.fail_open (std::make_error_code (std::errc::io_error));
  ASSERT_FALSE (mock_.transport.open (deadline_t (500)).has_value ());
  EXPECT_EQ (stats ().other_errors, 1u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenTwoMessages_WhenReceived_ThenMessagesAndPayloadBytesMove)
{
  open ();
  mock_.control.deliver (message (5));
  mock_.control.deliver (message (7));
  ASSERT_TRUE (mock_.transport.receive (deadline_t (500)).has_value ());
  ASSERT_TRUE (mock_.transport.receive (deadline_t (500)).has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.messages_received, 2u);
  EXPECT_EQ (s.bytes_read, 12u);
  EXPECT_EQ (s.reads, 0u);
  EXPECT_EQ (s.messages_sent, 0u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenTwoMessages_WhenSent_ThenMessagesAndPayloadBytesMove)
{
  open ();
  ASSERT_TRUE (
      mock_.transport.send (message (3), deadline_t (500)).has_value ());
  ASSERT_TRUE (
      mock_.transport.send (message (0), deadline_t (500)).has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.messages_sent, 2u);
  EXPECT_EQ (s.bytes_written, 3u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.messages_received, 0u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAFailingSend_WhenSent_ThenNoBytesAndTheErrorIsCounted)
{
  open ();
  mock_.control.fail_next_send (transport_errc::report_too_large);
  ASSERT_FALSE (
      mock_.transport.send (message (9), deadline_t (500)).has_value ());
  statistics_t const s = stats ();
  EXPECT_EQ (s.messages_sent, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::report_too_large), 1u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenEveryTransportErrc_WhenAReceiveFailsWithIt_ThenItsSlotMoves)
{
  open ();
  for (int value = 1; value <= 10; ++value)
    {
      transport_errc const code = static_cast<transport_errc> (value);
      mock_.transport.reset_statistics ();
      mock_.control.fail_next_receive (code);
      ASSERT_FALSE (mock_.transport.receive (deadline_t (500)).has_value ())
          << value;
      statistics_t const s = stats ();
      EXPECT_EQ (s.errors_of (code), 1u) << value;
      EXPECT_EQ (s.total_errors (), 1u) << value;
      EXPECT_EQ (s.messages_received, 0u) << value;
    }
}

TEST_F (LumexDatagramStatisticsTest,
        GivenNoMessage_WhenTheDeadlinePasses_ThenOneTimeoutAndNoCancellation)
{
  open ();
  tr::datagram::message_result_t const result
      = mock_.transport.receive (deadline_t (30));
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error () == transport_errc::timeout);
  statistics_t const s = stats ();
  EXPECT_EQ (s.timeouts, 1u);
  EXPECT_EQ (s.errors_of (transport_errc::timeout), 1u);
  EXPECT_EQ (s.cancellations, 0u);
  EXPECT_EQ (s.total_errors (), 1u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenABlockedReceive_WhenAnotherThreadCancelsOrCloses_ThenCancellations)
{
  open ();
  std::thread canceller (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          mock_.transport.cancel ();
        });
  ASSERT_FALSE (mock_.transport.receive (deadline_t (5000)).has_value ());
  canceller.join ();
  EXPECT_EQ (stats ().cancellations, 1u);
  EXPECT_EQ (stats ().timeouts, 0u);

  std::thread closer (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          mock_.transport.close ();
        });
  // The cancel left the transport open: the next receive blocks until closed.
  ASSERT_FALSE (mock_.transport.receive (deadline_t (5000)).has_value ());
  closer.join ();
  EXPECT_EQ (stats ().cancellations, 2u);
  EXPECT_EQ (stats ().timeouts, 0u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAClosedTransport_WhenReceivedAndSent_ThenNotOpenCountsTwice)
{
  EXPECT_FALSE (mock_.transport.receive (deadline_t (200)).has_value ());
  EXPECT_FALSE (
      mock_.transport.send (message (1), deadline_t (200)).has_value ());
  EXPECT_EQ (stats ().errors_of (transport_errc::not_open), 2u);
  EXPECT_EQ (stats ().total_errors (), 2u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenADisconnect_WhenReceiving_ThenDisconnectedCounts)
{
  open ();
  mock_.control.disconnect ();
  EXPECT_FALSE (mock_.transport.receive (deadline_t (500)).has_value ());
  EXPECT_EQ (stats ().errors_of (transport_errc::disconnected), 1u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenAHandlerOnTheRunnerThread_WhenItCallsTheSyncApi_ThenWouldDeadlockCounts)
{
  open ();
  mock_.control.deliver (message (1));
  std::promise<void> inner;
  message_transport &transport = mock_.transport;
  transport.async_receive (
      [&transport, &inner] (std::error_code, message_t)
        {
          transport.receive (deadline_t (1000));
          inner.set_value ();
        });
  std::future<void> done = inner.get_future ();
  ASSERT_EQ (done.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenAnAsyncSendOnAnotherExecutor_WhenItCompletes_ThenItRunsThereCounted)
{
  open ();
  asio::thread_pool other (1);
  std::promise<std::thread::id> other_id;
  asio::post (other, [&other_id]
                { other_id.set_value (std::this_thread::get_id ()); });
  std::thread::id const expected = other_id.get_future ().get ();

  std::promise<std::thread::id> ran_on;
  mock_.transport.async_send (
      message (4), asio::bind_executor (
                       other, [&ran_on] (std::error_code)
                         { ran_on.set_value (std::this_thread::get_id ()); }));
  std::future<std::thread::id> got = ran_on.get_future ();
  ASSERT_EQ (got.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_TRUE (got.get () == expected);
  EXPECT_EQ (stats ().messages_sent, 1u);
  EXPECT_EQ (stats ().bytes_written, 4u);
  other.join ();
}

TEST_F (LumexDatagramStatisticsTest,
        GivenCountedTraffic_WhenReset_ThenZeroAndCountingContinues)
{
  open ();
  mock_.control.deliver (message (2));
  ASSERT_TRUE (mock_.transport.receive (deadline_t (500)).has_value ());
  ASSERT_TRUE (
      mock_.transport.send (message (2), deadline_t (500)).has_value ());
  EXPECT_FALSE (mock_.transport.receive (deadline_t (20)).has_value ());
  mock_.transport.reset_statistics ();
  statistics_t s = stats ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.messages_received, 0u);
  EXPECT_EQ (s.messages_sent, 0u);
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.timeouts, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
  ASSERT_TRUE (
      mock_.transport.send (message (1), deadline_t (500)).has_value ());
  EXPECT_EQ (stats ().messages_sent, 1u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAMovedFromTransport_WhenAsked_ThenAnEmptySnapshotAndNoCrash)
{
  open ();
  message_transport moved (std::move (mock_.transport));
  EXPECT_EQ (moved.statistics ().opens, 1u);
  EXPECT_EQ (mock_.transport.statistics ().opens, 0u);
  mock_.transport.reset_statistics ();
}

TEST_F (LumexDatagramStatisticsTest,
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
            std::uint64_t received = 0;
            std::uint64_t sent = 0;
            while (!stop.load ())
              {
                statistics_t const s = mock_.transport.statistics ();
                if (s.messages_received < received || s.messages_sent < sent)
                  violations.fetch_add (1);
                received = s.messages_received;
                sent = s.messages_sent;
              }
          });
  std::size_t const rounds = 200;
  for (std::size_t round = 0; round < rounds; ++round)
    {
      ASSERT_TRUE (
          mock_.transport.send (message (4), deadline_t (1000)).has_value ());
      mock_.control.deliver (message (4));
      ASSERT_TRUE (mock_.transport.receive (deadline_t (1000)).has_value ());
    }
  stop.store (true);
  for (std::thread &reader : readers)
    reader.join ();
  EXPECT_EQ (violations.load (), 0);
  statistics_t const s = stats ();
  EXPECT_EQ (s.messages_received, rounds);
  EXPECT_EQ (s.messages_sent, rounds);
  EXPECT_EQ (s.bytes_read, rounds * 4);
  EXPECT_EQ (s.bytes_written, rounds * 4);
  EXPECT_EQ (s.total_errors (), 0u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenTheRunnerThread_WhenItCallsOpen_ThenWouldDeadlockCountsAndNothingElse)
{
  std::error_code const error = called_on_runner (
      runner_,
      [this]
        {
          tr::status_result_t const result
              = mock_.transport.open (deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().opens, 0u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenTheRunnerThread_WhenItCallsReceive_ThenWouldDeadlockCountsAndNothingElse)
{
  open ();
  std::error_code const error = called_on_runner (
      runner_,
      [this]
        {
          tr::datagram::message_result_t const result
              = mock_.transport.receive (deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().messages_received, 0u);
}

TEST_F (
    LumexDatagramStatisticsTest,
    GivenTheRunnerThread_WhenItCallsSend_ThenWouldDeadlockCountsAndNothingElse)
{
  open ();
  std::error_code const error = called_on_runner (
      runner_,
      [this]
        {
          tr::status_result_t const result
              = mock_.transport.send (message (2), deadline_t (500));
          return result.has_value () ? std::error_code () : result.error ();
        });
  EXPECT_TRUE (error == transport_errc::would_deadlock);
  EXPECT_EQ (stats ().errors_of (transport_errc::would_deadlock), 1u);
  EXPECT_EQ (stats ().total_errors (), 1u);
  EXPECT_EQ (stats ().messages_sent, 0u);
}

TEST_F (LumexDatagramStatisticsTest,
        GivenAFreshTransport_WhenOnlyOpened_ThenLastActivityIsSet)
{
  ASSERT_TRUE (stats ().last_activity == clock_type::time_point ());
  open ();
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (LumexDatagramStatisticsTest,
        GivenResetCounters_WhenOnlyAReceiveSucceeds_ThenLastActivityIsSet)
{
  open ();
  mock_.control.deliver (message (2));
  mock_.transport.reset_statistics ();
  ASSERT_TRUE (stats ().last_activity == clock_type::time_point ());
  ASSERT_TRUE (mock_.transport.receive (deadline_t (500)).has_value ());
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

TEST_F (LumexDatagramStatisticsTest,
        GivenResetCounters_WhenOnlyASendSucceeds_ThenLastActivityIsSet)
{
  open ();
  mock_.transport.reset_statistics ();
  ASSERT_TRUE (
      mock_.transport.send (message (2), deadline_t (500)).has_value ());
  EXPECT_FALSE (stats ().last_activity == clock_type::time_point ());
}

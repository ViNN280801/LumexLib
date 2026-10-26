// message_transport (lumex/applied/transport/datagram) over the mock: one
// receive is one message with its report ID, send, the deadline, close from
// another thread, not_open, a failed send, the asynchronous API and
// would_deadlock.

#include <chrono>
#include <cstdint>
#include <future>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::datagram::message_t;
using tr::datagram::message_transport;
using tr::error::transport_errc;
using tr::stream::io_runner;
using tr::testing::make_mock_datagram;
using tr::testing::mock_datagram;

message_t
message (std::uint8_t report_id, std::vector<std::uint8_t> const &payload)
{
  message_t result;
  result.report_id = report_id;
  result.payload = payload;
  return result;
}

class LumexMessageTransportTest : public ::testing::Test
{
protected:
  LumexMessageTransportTest ()
      : runner_ (2), mock_ (make_mock_datagram (runner_.get_executor (), "d"))
  {
  }

  io_runner runner_;
  mock_datagram mock_;
};
} // namespace

TEST_F (LumexMessageTransportTest,
        GivenTwoQueuedMessages_WhenReceived_ThenEachArrivesWhole)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  mock_.control.deliver (message (3, { 1, 2, 3 }));
  mock_.control.deliver (message (0, { 9 }));
  tr::datagram::message_result_t const first
      = mock_.transport.receive (deadline_t (500));
  tr::datagram::message_result_t const second
      = mock_.transport.receive (deadline_t (500));
  ASSERT_TRUE (first.has_value ());
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (first.value ().report_id, 3);
  EXPECT_EQ (first.value ().payload, (std::vector<std::uint8_t>{ 1, 2, 3 }));
  EXPECT_EQ (second.value ().report_id, 0);
  EXPECT_EQ (second.value ().payload, (std::vector<std::uint8_t>{ 9 }));
}

TEST_F (LumexMessageTransportTest, GivenAMessage_WhenSent_ThenTheMockHasIt)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  ASSERT_TRUE (mock_.transport.send (message (5, { 7, 8 }), deadline_t (500))
                   .has_value ());
  std::vector<message_t> const sent = mock_.control.sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 5);
  EXPECT_EQ (sent[0].payload, (std::vector<std::uint8_t>{ 7, 8 }));
}

TEST_F (LumexMessageTransportTest,
        GivenNoMessage_WhenTheDeadlinePasses_ThenTimeout)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  tr::datagram::message_result_t const result
      = mock_.transport.receive (deadline_t (40));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout);
}

TEST_F (LumexMessageTransportTest,
        GivenABlockedReceive_WhenAnotherThreadCloses_ThenCancelledAndClosed)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  std::thread closer (
      [this]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (30));
          mock_.transport.close ();
        });
  tr::datagram::message_result_t const result
      = mock_.transport.receive (deadline_t (5000));
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_FALSE (mock_.transport.is_open ());
}

TEST_F (LumexMessageTransportTest,
        GivenAClosedTransport_WhenSending_ThenNotOpen)
{
  tr::status_result_t const result
      = mock_.transport.send (message (0, { 1 }), deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::not_open);
}

TEST_F (LumexMessageTransportTest,
        GivenAFailingSend_WhenSending_ThenTheErrorIsReturned)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  mock_.control.fail_next_send (transport_errc::report_too_large);
  tr::status_result_t const result
      = mock_.transport.send (message (0, { 1 }), deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::report_too_large);
}

TEST_F (LumexMessageTransportTest,
        GivenADisconnect_WhenReceiving_ThenDisconnected)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  mock_.control.disconnect ();
  tr::datagram::message_result_t const result
      = mock_.transport.receive (deadline_t (500));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::disconnected);
}

TEST_F (LumexMessageTransportTest,
        GivenAnAsyncReceive_WhenAMessageArrives_ThenTheCallbackGetsIt)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  std::promise<message_t> got;
  mock_.transport.async_receive (
      [&got] (std::error_code error, message_t received)
        {
          EXPECT_FALSE (error);
          got.set_value (received);
        });
  mock_.control.deliver (message (2, { 4 }));
  std::future<message_t> result = got.get_future ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get ().report_id, 2);
}

TEST_F (LumexMessageTransportTest,
        GivenUseFuture_WhenAnAsyncSendCompletes_ThenTheFutureIsReady)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  std::future<void> sent
      = mock_.transport.async_send (message (0, { 1, 2 }), asio::use_future);
  ASSERT_EQ (sent.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_NO_THROW (sent.get ());
  EXPECT_EQ (mock_.control.sent ().size (), 1u);
}

TEST_F (
    LumexMessageTransportTest,
    GivenAHandlerOnTheRunnerThread_WhenItReceivesSynchronously_ThenWouldDeadlock)
{
  ASSERT_TRUE (mock_.transport.open (deadline_t (500)).has_value ());
  std::promise<std::error_code> inner;
  message_transport &transport = mock_.transport;
  transport.async_send (message (0, { 1 }),
                        [&transport, &inner] (std::error_code)
                          {
                            tr::datagram::message_result_t const nested
                                = transport.receive (deadline_t (1000));
                            inner.set_value (nested.has_value ()
                                                 ? std::error_code ()
                                                 : nested.error ());
                          });
  std::future<std::error_code> result = inner.get_future ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_TRUE (result.get () == transport_errc::would_deadlock);
}

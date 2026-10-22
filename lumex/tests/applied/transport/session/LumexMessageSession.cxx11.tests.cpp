// message_session over a factory of mock datagram transports: receive and
// send through the session, not_open while closed, a lost link reconnects.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
typedef ss::channel_state state;

class LumexMessageSessionTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  std::shared_ptr<datagram_factory_state> factory_
      = std::make_shared<datagram_factory_state> ();
  std::shared_ptr<state_recorder> recorder_
      = std::make_shared<state_recorder> ();

  std::unique_ptr<ss::message_session>
  make_session ()
  {
    return std::unique_ptr<ss::message_session> (new ss::message_session (
        runner_.get_executor (), make_datagram_factory (factory_),
        recorded_options (recorder_)));
  }
};
} // namespace

TEST_F (
    LumexMessageSessionTest,
    GivenAnOpenSession_WhenMessagesGoBothWays_ThenTheCurrentTransportCarriesThem)
{
  std::unique_ptr<ss::message_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  tr::datagram::message_t message;
  message.report_id = 3;
  message.payload = { 1, 2, 3 };
  session->async_send (message, asio::use_future).get ();
  std::vector<tr::datagram::message_t> const sent = factory_->last ().sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 3);
  EXPECT_EQ (sent[0].payload, message.payload);

  tr::datagram::message_t incoming;
  incoming.payload = { 7, 7 };
  factory_->last ().deliver (incoming);
  tr::datagram::message_t const received
      = session->async_receive (asio::use_future).get ();
  EXPECT_EQ (received.payload, incoming.payload);
  tr::statistics::statistics_t const stats = session->statistics ();
  EXPECT_EQ (stats.messages_sent, 1u);
  EXPECT_EQ (stats.messages_received, 1u);
}

TEST_F (LumexMessageSessionTest,
        GivenAClosedSession_WhenAMessageIsSentOrReceived_ThenItEndsWithNotOpen)
{
  std::unique_ptr<ss::message_session> session = make_session ();
  outcome<int> sent;
  session->async_send (tr::datagram::message_t (),
                       [&sent] (std::error_code error)
                         { sent.set (error, 0); });
  ASSERT_TRUE (sent.wait ());
  EXPECT_TRUE (sent.error == transport_errc::not_open);
  outcome<std::size_t> received;
  session->async_receive (
      [&received] (std::error_code error, tr::datagram::message_t message)
        { received.set (error, message.payload.size ()); });
  ASSERT_TRUE (received.wait ());
  EXPECT_TRUE (received.error == transport_errc::not_open);
  EXPECT_EQ (factory_->call_count (), 0u);
}

TEST_F (LumexMessageSessionTest,
        GivenAnOpenSession_WhenTheTransportDisconnects_ThenItReconnects)
{
  std::unique_ptr<ss::message_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  outcome<std::size_t> received;
  session->async_receive (
      [&received] (std::error_code error, tr::datagram::message_t message)
        { received.set (error, message.payload.size ()); });
  factory_->last ().disconnect ();
  ASSERT_TRUE (received.wait ());
  EXPECT_TRUE (received.error == transport_errc::disconnected);
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        }));
  EXPECT_EQ (session->state (), state::open);
  EXPECT_EQ (factory_->call_count (), 2u);
  std::vector<ss::channel_state> const states = recorder_->states ();
  ASSERT_EQ (states.size (), 4u);
  EXPECT_EQ (states[2], state::reconnecting);
  EXPECT_EQ (states[3], state::open);
}

TEST_F (LumexMessageSessionTest,
        GivenASendThatFailsWithReportTooLarge_WhenItEnds_ThenTheLinkStaysOpen)
{
  std::unique_ptr<ss::message_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  factory_->last ().fail_next_send (
      make_error_code (transport_errc::report_too_large));
  outcome<int> sent;
  session->async_send (tr::datagram::message_t (),
                       [&sent] (std::error_code error)
                         { sent.set (error, 0); });
  ASSERT_TRUE (sent.wait ());
  EXPECT_TRUE (sent.error == transport_errc::report_too_large);
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (session->state (), state::open);
  EXPECT_EQ (factory_->call_count (), 1u);
}

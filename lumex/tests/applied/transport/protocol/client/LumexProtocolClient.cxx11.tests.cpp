// The generated client<P> on a virtual clock: a method per row in the async
// form, what each reply form gives, the policy of the table, an argument that
// does not fit, the events, the session in the decoders. The synthetic mixer
// table.

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;
using bench_t = client_bench_t<mixer>;

/// Calls `method` and runs until it ended.
template <typename Row, typename Call>
std::shared_ptr<outcome_of<Row>>
finish (bench_t &bench, Call const &call)
{
  std::shared_ptr<outcome_of<Row>> const out = new_outcome<Row> ();
  call (into<Row> (out));
  EXPECT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (out->calls, 1);
  return out;
}
} // namespace

TEST (LumexProtocolClient, Client_MethodPerRowWithSpecName)
{
  log_capture_t const log;
  bench_t bench;
  bench.device->set_reply<mixer::PEEK_t> (std::uint8_t (42));
  auto const peek = finish<mixer::PEEK_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->PEEK (std::uint8_t (3), done); });
  ASSERT_FALSE (peek->error) << peek->error.message ();
  EXPECT_EQ (peek->value, 42);
  // The method carries the identifier, the log the specification name.
  EXPECT_TRUE (log_capture_t::any_holds ("mixer_t.PEEK-ONE"));
  EXPECT_FALSE (log_capture_t::any_holds ("mixer_t.PEEK "));
  auto const wipe
      = finish<mixer::WIPE_t> (bench, [&] (record_t<std::uint8_t> done)
                                 { bench.client->WIPE (pr::confirm, done); });
  ASSERT_FALSE (wipe->error) << wipe->error.message ();
  EXPECT_TRUE (log_capture_t::any_holds ("DELETE ALL"));
  EXPECT_FALSE (log_capture_t::any_holds ("WIPE"));
}

TEST (LumexProtocolClient, Client_LayerMethodsOfTheBaseAreTheOwnLayerRows)
{
  client_bench_t<mixer_v1> bench;
  bench.device->set_reply<mixer_v1::ONLY_V1_t> (std::uint8_t (7));
  bench.device->set_reply<mixer_v1::RATE_t> (std::uint8_t (8));
  auto const own = std::make_shared<outcome_of<mixer_v1::ONLY_V1_t>> ();
  bench.client->ONLY_V1 (into<mixer_v1::ONLY_V1_t> (own));
  auto const inherited = std::make_shared<outcome_of<mixer_v1::RATE_t>> ();
  bench.client->RATE (std::uint8_t (1), into<mixer_v1::RATE_t> (inherited));
  EXPECT_TRUE (bench.run_until (
      [&] () { return own->calls != 0 && inherited->calls != 0; }));
  EXPECT_EQ (own->value, 7);
  EXPECT_EQ (inherited->value, 8);
}

TEST (LumexProtocolClient, Client_EveryReplyFormGivesItsValue)
{
  bench_t bench;
  // No reply: an empty record, the exchange ends with the write.
  auto const run = finish<mixer::RUN_t> (
      bench, [&] (record_t<field::record<>> done)
        { bench.client->RUN (std::uint8_t (1), done); });
  EXPECT_FALSE (run->error) << run->error.message ();
  // A layout.
  bench.device->set_reply<mixer::LEVEL_t> (pr::reply_of<mixer::LEVEL_t> ());
  // reply_row<R>: the message of R.
  pr::reply_of<mixer::WRITE_t> change;
  field::get<0> (change) = 5;
  field::get<1> (change) = 600;
  bench.device->set_reply<mixer::WRITE_t> (change);
  auto const write = finish<mixer::WRITE_t> (
      bench,
      [&] (record_t<pr::reply_of<mixer::WRITE_t>> done)
        {
          bench.client->WRITE (std::uint8_t (5), std::uint16_t (600), done);
        });
  ASSERT_FALSE (write->error) << write->error.message ();
  EXPECT_EQ (field::get<0> (write->value), 5);
  EXPECT_EQ (field::get<1> (write->value), 600);
  // ack_nak: the value of the ACK.
  bench.device->on<mixer::MOVE_t> (
      [] (layout::request_of<mixer::MOVE_t>::type const &,
          index::session_context_t &) { return std::uint8_t (77); });
  auto const move = finish<mixer::MOVE_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->MOVE (std::uint8_t (1), done); });
  ASSERT_FALSE (move->error) << move->error.message ();
  EXPECT_EQ (move->value, 77);
  // two_phase: the value of the completion.
  bench.device->set_reply<mixer::PRIME_t> (std::uint16_t (900));
  auto const prime = finish<mixer::PRIME_t> (
      bench, [&] (record_t<std::uint16_t> done)
        { bench.client->PRIME (std::uint16_t (1), done); });
  ASSERT_FALSE (prime->error) << prime->error.message ();
  EXPECT_EQ (prime->value, 900);
  // The echo kinds: the echo.
  auto const flow = finish<mixer::FLOW_t> (
      bench, [&] (record_t<std::uint16_t> done)
        { bench.client->FLOW (std::uint16_t (321), done); });
  ASSERT_FALSE (flow->error) << flow->error.message ();
  EXPECT_EQ (flow->value, 321);
  auto const limit = finish<mixer::LIMIT_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->LIMIT (std::uint8_t (9), done); });
  ASSERT_FALSE (limit->error) << limit->error.message ();
  EXPECT_EQ (limit->value, 9);
  auto const mode = finish<mixer::MODE_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->MODE (std::uint8_t (4), done); });
  ASSERT_FALSE (mode->error) << mode->error.message ();
  EXPECT_EQ (mode->value, 4);
}

TEST (LumexProtocolClient, Client_TheMatcherOfTheRowDecides)
{
  bench_t bench;
  bench.device->set_reply<mixer::PICK_t> (std::uint8_t (6));
  auto const wrong = finish<mixer::PICK_t> (
      bench, [&] (record_t<std::uint8_t> done) { bench.client->PICK (done); });
  EXPECT_EQ (wrong->error, protocol_errc::timeout);
  bench.device->set_reply<mixer::PICK_t> (std::uint8_t (7));
  auto const right = finish<mixer::PICK_t> (
      bench, [&] (record_t<std::uint8_t> done) { bench.client->PICK (done); });
  EXPECT_FALSE (right->error) << right->error.message ();
  EXPECT_EQ (right->value, 7);
}

TEST (LumexProtocolClient, Client_ARefusedReplyGivesItsErrorAndValue)
{
  bench_t bench;
  bench.device->set_reply<mixer::FLOW_t> (std::uint16_t (11));
  auto const flow = finish<mixer::FLOW_t> (
      bench, [&] (record_t<std::uint16_t> done)
        { bench.client->FLOW (std::uint16_t (10), done); });
  EXPECT_EQ (flow->error, protocol_errc::not_confirmed);
  EXPECT_EQ (flow->value, 11);
  bench.device->set_reply<mixer::MODE_t> (std::uint8_t (2));
  auto const mode = finish<mixer::MODE_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->MODE (std::uint8_t (1), done); });
  EXPECT_EQ (mode->error, protocol_errc::refused);
  EXPECT_EQ (mode->value, 2);
  bench.device->refuse<mixer::MOVE_t> ();
  auto const move = finish<mixer::MOVE_t> (
      bench, [&] (record_t<std::uint8_t> done)
        { bench.client->MOVE (std::uint8_t (1), done); });
  EXPECT_EQ (move->error, protocol_errc::nak);
  EXPECT_EQ (move->value, 0);
}

TEST (LumexProtocolClient, Client_TheTimeoutAndRetriesOfTheRowCount)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = new_outcome<mixer::PING_t> ();
  bench.client->PING (into<mixer::PING_t> (out));
  bench.advance (ms (899));
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.client->statistics ().requests, 3u);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::retries_exhausted);
}

TEST (LumexProtocolClient, Client_ARowWithoutOptionsTakesTheDefaultOfTheTable)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (out));
  bench.advance (ms (199));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (bench.client->statistics ().requests, 1u);
}

TEST (LumexProtocolClient, Client_ExtraRetriesOfTheOptionsKeepTheirState)
{
  pr::client_options_t options = pr::client_options_for<mixer> ();
  std::shared_ptr<int> const asked = std::make_shared<int> (0);
  int grants = 3;
  options.dialogue.defaults.extra_retries
      = [asked, grants] (index::session_context_t const &,
                         std::error_code const &) mutable -> std::uint32_t
    {
      ++*asked;
      return grants-- > 0 ? 1 : 0;
    };
  bench_t bench (options);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (first));
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  // Three granted, the fourth question said no: 1 + 3 attempts.
  EXPECT_EQ (*asked, 4);
  EXPECT_EQ (bench.client->statistics ().requests, 4u);
  auto const second = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (second));
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  // The one object: what it counted stays, so no more grants.
  EXPECT_EQ (*asked, 5);
  EXPECT_EQ (bench.client->statistics ().requests, 5u);
}

TEST (LumexProtocolClient, Client_TheDialogueReadsTheSessionOfTheClient)
{
  pr::client_options_t options = pr::client_options_for<mixer> ();
  std::shared_ptr<std::string> const seen = std::make_shared<std::string> ();
  options.dialogue.defaults.extra_retries
      = [seen] (index::session_context_t const &session,
                std::error_code const &) -> std::uint32_t
    {
      *seen = session.model ();
      return 0;
    };
  bench_t bench (options);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  index::session_context_t session;
  session.set_model ("given");
  bench.client->set_session (session);
  auto const first = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (first));
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (*seen, "given");
  // What identify () learned as well.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<mixer::INFO_t> (mixer_info ("M100", 3, 1));
  pr::identity_t identity;
  bool identified = false;
  bench.client->async_identify (
      [&] (std::error_code error, pr::identity_t value)
        {
          EXPECT_FALSE (error) << error.message ();
          identity = std::move (value);
          identified = true;
        });
  ASSERT_TRUE (bench.run_until ([&] () { return identified; }));
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const second = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (second));
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_EQ (*seen, "M100");
}

TEST (LumexProtocolClient, Client_ArgumentNotFittingIsInvalidValue)
{
  bench_t bench;
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (300, into<mixer::RATE_t> (out));
  // Never inside the call.
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::invalid_value);
  EXPECT_EQ (bench.requests (), 0u);
  EXPECT_EQ (bench.client->statistics ().requests, 0u);
  // The second argument of a row of two.
  auto const pair = new_outcome<mixer::PAIR_t> ();
  bench.client->PAIR (std::uint8_t (1), -1, into<mixer::PAIR_t> (pair));
  bench.settle ();
  EXPECT_EQ (pair->error, protocol_errc::invalid_value);
  EXPECT_EQ (bench.requests (), 0u);
}

TEST (LumexProtocolClient, Client_ACompletionNeverRunsInsideTheCall)
{
  bench_t bench;
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (out));
  EXPECT_EQ (out->calls, 0);
  EXPECT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  bench.advance (ms (1000));
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexProtocolClient, Client_DangerousRowWithConfirmIsSent)
{
  bench_t bench;
  bench.device->set_reply<mixer::ERASE_t> (std::uint8_t (1));
  auto const out = new_outcome<mixer::ERASE_t> ();
  bench.client->ERASE (pr::confirm, std::uint8_t (3),
                       into<mixer::ERASE_t> (out));
  EXPECT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value, 1);
  EXPECT_EQ (bench.requests (), 1u);
}

TEST (LumexProtocolClient, Client_OnEventTyped)
{
  bench_t bench;
  std::vector<int> levels;
  pr::subscription_t const subscription = bench.client->on<mixer::LEVEL_t> (
      [&levels] (pr::reply_of<mixer::LEVEL_t> const &value)
        { levels.push_back (field::get<1> (value)); });
  EXPECT_NE (subscription.id, 0u);
  pr::reply_of<mixer::LEVEL_t> level;
  field::get<1> (level) = 9;
  EXPECT_TRUE (bench.device->start_stream<mixer::LEVEL_t> (level));
  bench.advance (ms (250));
  ASSERT_EQ (levels.size (), 2u);
  EXPECT_EQ (levels[0], 9);
  EXPECT_TRUE (bench.client->unsubscribe (subscription));
  EXPECT_FALSE (bench.client->unsubscribe (subscription));
  bench.advance (ms (250));
  EXPECT_EQ (levels.size (), 2u);
}

TEST (LumexProtocolClient, Client_AFrameThatDoesNotDecodeIsDroppedAndLogged)
{
  log_capture_t const log;
  bench_t bench;
  int calls = 0;
  bench.client->on<mixer::LEVEL_t> (
      [&calls] (pr::reply_of<mixer::LEVEL_t> const &) { ++calls; });
  std::vector<std::uint8_t> wire;
  std::vector<std::uint8_t> const cut = { 0x12, 0x01 };
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (cut.data (), cut.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  EXPECT_EQ (calls, 0);
  EXPECT_TRUE (log_capture_t::any_holds ("does not decode"));
  EXPECT_TRUE (log_capture_t::any_holds ("LEVEL"));
}

TEST (LumexProtocolClient, Client_OnUnknownHearsAFrameOfNoRow)
{
  bench_t bench;
  std::vector<std::vector<std::uint8_t>> frames;
  bench.client->on_unknown ([&frames] (pr::reply_frame_t const &frame)
                              { frames.push_back (frame); });
  std::vector<std::uint8_t> wire;
  std::vector<std::uint8_t> const lost = { 0xEE, 0x01 };
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (lost.data (), lost.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  ASSERT_EQ (frames.size (), 1u);
  EXPECT_EQ (frames[0], lost);
}

TEST (LumexProtocolClient, Client_TheSessionDecidesTheDecoding)
{
  bench_t bench;
  // The device answers with the wide form of the reply.
  std::vector<std::uint8_t> const wide = { 0x23, 0x34, 0x12 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (wide.data (), wide.size ()), wire)
          .has_value ());
  tr::testing::mock_stream_control *const control = bench.control.get ();
  bench.device.reset ();
  bench.settle ();
  control->on_write ([control, wire] (std::vector<std::uint8_t> const &)
                       { control->feed (wire); });
  // Before: no session value, the decoder cannot tell the form.
  auto const before = new_outcome<mixer::SHAPE_t> ();
  bench.client->SHAPE (into<mixer::SHAPE_t> (before));
  ASSERT_TRUE (bench.run_until ([&] () { return before->calls != 0; }));
  EXPECT_EQ (before->error, protocol_errc::unknown_case);
  // The application knows the form.
  index::session_context_t session;
  session.set ("mixer_form", 1);
  bench.client->set_session (session);
  std::int64_t form = 0;
  EXPECT_TRUE (bench.client->session ().find ("mixer_form", form));
  EXPECT_EQ (form, 1);
  auto const after = new_outcome<mixer::SHAPE_t> ();
  bench.client->SHAPE (into<mixer::SHAPE_t> (after));
  ASSERT_TRUE (bench.run_until ([&] () { return after->calls != 0; }));
  ASSERT_FALSE (after->error) << after->error.message ();
  EXPECT_EQ (field::get<0> (after->value).discriminant, 1);
  bench.control->on_write (tr::testing::write_observer_t ());
}

TEST (LumexProtocolClient, Client_ATableTheIndexRefusesAnswersEveryCall)
{
  // Two rows of one name: the methods compile, the index says no.
  client_bench_t<tables::lone_t, tables::twins_t> bench;
  auto const out = new_outcome<tables::twins_t::FIRST_t> ();
  bench.client->FIRST (into<tables::twins_t::FIRST_t> (out));
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::duplicate_key);
  EXPECT_EQ (bench.client->statistics ().requests, 0u);
}

TEST (LumexProtocolClient, Client_PlainOptionsStillTakeTheFeaturesOfTheTable)
{
  pr::client_options_t options;
  client_bench_t<tables::featured_t> bench (options);
  // The table names a keepalive of a row without fields.
  bench.advance (ms (250));
  EXPECT_EQ (bench.client->statistics ().keepalives, 2u);
}

TEST (LumexProtocolClient, Client_CancelEndsTheCallsAndCloseEndsThemForGood)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (first));
  bench.settle ();
  bench.client->cancel ();
  bench.settle ();
  EXPECT_EQ (first->error, transport_errc::cancelled);
  bench.client->close ();
  auto const second = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer::RATE_t> (second));
  bench.settle ();
  EXPECT_EQ (second->error, transport_errc::closed);
}

TEST (LumexProtocolClient, Client_ACancellationSlotEndsOneCall)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  asio::cancellation_signal signal;
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (std::uint8_t (1),
                      asio::bind_cancellation_slot (
                          signal.slot (), into<mixer::RATE_t> (out)));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
}

TEST (LumexProtocolClient, Client_AHandlerRunsOnItsOwnExecutor)
{
  bench_t bench;
  asio::io_context other;
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (
      std::uint8_t (1),
      asio::bind_executor (other.get_executor (), into<mixer::RATE_t> (out)));
  bench.settle ();
  bench.advance (ms (10));
  EXPECT_EQ (out->calls, 0);
  other.poll ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
}

TEST (LumexProtocolClient, Client_ARefusalRunsOnTheExecutorOfTheHandlerToo)
{
  bench_t bench;
  asio::io_context other;
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->RATE (300, asio::bind_executor (other.get_executor (),
                                                into<mixer::RATE_t> (out)));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  other.poll ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::invalid_value);
}

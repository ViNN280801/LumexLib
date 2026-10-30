// The reply kinds in the dialogue engine (ack_nak, two_phase, echo_set,
// echo_zero_on_error, state_instead_of_refusal through their readers),
// frames that answer no exchange (subscribers, on_unknown and the warning),
// the debug line of every frame (the describe line of the index) and the
// report ID of a datagram link. The pump table of the vocabulary tests; the
// device of the bench answers as each test says.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueTestSupport.hpp"

namespace
{
namespace dt = lumex_dialogue_test;
using dt::bench_t;
using dt::bytes;
using dt::bytes_t;
using dt::log_capture_t;
using dt::ms;
using dt::outcome_ptr;
using dt::protocol_errc;
using dt::pump;
using dt::transport_errc;
namespace tr = dt::tr;
namespace pr = dt::pr;
namespace fr = dt::fr;
using tr::log::log_level;

// A device that answers the n-th request with replies[n] (the last one for
// every later request), `delay` after it.
std::function<void (bytes_t const &)>
answers (bench_t &bench, std::vector<bytes_t> const &replies,
         ms delay = ms (0))
{
  return [&bench, replies, delay] (bytes_t const &)
    {
      std::size_t const number = bench.requests ().size ();
      std::size_t const at
          = number <= replies.size () ? number - 1 : replies.size () - 1;
      bench.reply (replies[at], delay);
    };
}

pr::exchange_t
with_policy (pr::exchange_t request, std::uint32_t timeout_ms,
             std::uint32_t retries)
{
  request.policy = dt::policy (timeout_ms, retries);
  return request;
}

outcome_ptr
run_one (bench_t &bench, pr::exchange_t request)
{
  outcome_ptr const out = bench.start_exchange (request);
  EXPECT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  return out;
}

struct frames_t
{
  std::vector<bytes_t> seen;
};
} // namespace

TEST (
    LumexDialogueReplyKindsTest,
    GivenAnAckNakOfFields_WhenTheDeviceAcksThenNaks_ThenTheAckCompletesAndTheNakEndsWithNak)
{
  bench_t bench;
  bench.start ();
  bench.on_request (answers (
      bench, { bytes ({ 0x01, 0x07 }), bytes ({ 0x01, 0x05, 0x34, 0x12 }) }));
  outcome_ptr const ack
      = run_one (bench, dt::make_exchange<pump::START_t> (std::uint8_t (3)));
  EXPECT_FALSE (ack->error) << ack->error.message ();
  EXPECT_EQ (ack->frame, bytes ({ 0x01, 0x07 }));
  outcome_ptr const nak
      = run_one (bench, dt::make_exchange<pump::START_t> (std::uint8_t (3)));
  EXPECT_EQ (nak->error, protocol_errc::nak);
  EXPECT_EQ (nak->frame, bytes_t ());
  EXPECT_EQ (bench.the_dialogue->statistics ().naks, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().replies, 2u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAnAckNakOfRows_WhenTheAckOrTheNakRowOrAnotherFrameComes_ThenEachEndsItsWay)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      answers (bench, { bytes ({ 0x06 }), bytes ({ 0x15, 0x05 }),
                        bytes ({ 0x02 }), bytes ({ 0x0C, 0x01 }) }));
  outcome_ptr const ack = run_one (bench, dt::make_exchange<pump::STOP_t> ());
  EXPECT_FALSE (ack->error);
  EXPECT_EQ (ack->frame, bytes ({ 0x06 }));
  outcome_ptr const nak = run_one (bench, dt::make_exchange<pump::STOP_t> ());
  EXPECT_EQ (nak->error, protocol_errc::nak);
  // The row's own key is no part when both parts are rows; another row is
  // no part either: the by_order exchange ends with unexpected_reply.
  outcome_ptr const own = run_one (bench, dt::make_exchange<pump::STOP_t> ());
  EXPECT_EQ (own->error, protocol_errc::unexpected_reply);
  outcome_ptr const other
      = run_one (bench, dt::make_exchange<pump::STOP_t> ());
  EXPECT_EQ (other->error, protocol_errc::unexpected_reply);
  EXPECT_EQ (other->frame, bytes ({ 0x0C, 0x01 }));
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenARowWithARetry_WhenTheFirstAnswerIsANak_ThenTheRequestIsSentAgainAndTheAckCompletes)
{
  bench_t bench;
  bench.start ();
  bench.on_request (answers (
      bench, { bytes ({ 0x01, 0x05, 0x01, 0x00 }), bytes ({ 0x01, 0x09 }) }));
  outcome_ptr const start = run_one (
      bench, with_policy (dt::make_exchange<pump::START_t> (std::uint8_t (1)),
                          100, 1));
  EXPECT_FALSE (start->error) << start->error.message ();
  EXPECT_EQ (start->frame, bytes ({ 0x01, 0x09 }));
  EXPECT_EQ (bench.requests ().size (), 2u);
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_EQ (counters.naks, 1u);
  EXPECT_EQ (counters.retries, 1u);
  EXPECT_EQ (counters.timeouts, 0u);
  // Every attempt NAKed: the last answer is the error.
  bench.on_request (answers (bench, { bytes ({ 0x01, 0x05, 0x01, 0x00 }) }));
  outcome_ptr const refused = run_one (
      bench, with_policy (dt::make_exchange<pump::START_t> (std::uint8_t (1)),
                          100, 1));
  EXPECT_EQ (refused->error, protocol_errc::nak);
  EXPECT_EQ (bench.requests ().size (), 4u);
  EXPECT_EQ (bench.the_dialogue->statistics ().retries_exhausted, 0u);
}

TEST (LumexDialogueReplyKindsTest,
      GivenAPolicyThatAsksTheSession_WhenANakComes_ThenThePredicateSeesTheNak)
{
  bench_t bench;
  bench.start ();
  std::vector<std::error_code> asked;
  pr::extra_retries_t const once
      = [&asked] (lumex::core::protocol::index::session_context_t const &,
                  std::error_code const &error) -> std::uint32_t
    {
      asked.push_back (error);
      return asked.size () == 1 ? 1u : 0u;
    };
  bench.on_request (answers (
      bench, { bytes ({ 0x01, 0x05, 0x01, 0x00 }), bytes ({ 0x01, 0x02 }) }));
  pr::exchange_t request = dt::make_exchange<pump::START_t> (std::uint8_t (1));
  request.policy = dt::policy (100, 0, once);
  outcome_ptr const start = run_one (bench, request);
  EXPECT_FALSE (start->error);
  ASSERT_EQ (asked.size (), 1u);
  EXPECT_EQ (asked[0], protocol_errc::nak);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenANakHandledJustAsTheTimeoutEnds_WhenTheRetryStarts_ThenTheEndedWaitDoesNotTimeItOut)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<dt::manual_link_t> const link
      = std::make_shared<dt::manual_link_t> (context);
  pr::dialogue_options_t options;
  options.timers = clock.source ();
  pr::dialogue_engine engine (pr::frame_link (link), dt::pump_index (),
                              options);
  pr::exchange_t request = dt::make_exchange<pump::START_t> (std::uint8_t (1));
  request.policy = dt::policy (100, 1);
  outcome_ptr const out = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { out, &clock };
  engine.async_exchange (request, handler);
  context.poll ();
  context.restart ();
  ASSERT_EQ (link->written ().size (), 1u);
  // The NAK reaches the strand first; the clock then ends the wait of the
  // first attempt, whose completion queues behind the NAK.
  ASSERT_TRUE (link->deliver (bytes ({ 0x01, 0x05, 0x01, 0x00 })));
  clock.advance (ms (100));
  context.poll ();
  context.restart ();
  ASSERT_EQ (link->written ().size (), 2u);
  EXPECT_EQ (out->calls, 0);
  ASSERT_TRUE (link->deliver (bytes ({ 0x01, 0x04 })));
  context.poll ();
  context.restart ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (engine.statistics ().timeouts, 0u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenATwoPhaseRow_WhenTheAckComes_ThenTheCompletionHasATimeoutOfItsOwnAndEndsTheExchange)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          bench.reply (bytes ({ 0x06 }), ms (500));
          bench.reply (bytes ({ 0x03, 0xE8, 0x03 }), ms (1400));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = run_one (
      bench,
      with_policy (dt::make_exchange<pump::PRIME_t> (std::uint16_t (1000)),
                   1000, 2));
  EXPECT_FALSE (prime->error) << prime->error.message ();
  EXPECT_EQ (prime->frame, bytes ({ 0x03, 0xE8, 0x03 }));
  EXPECT_EQ (std::chrono::duration_cast<ms> (prime->at - start).count (),
             1400);
  EXPECT_EQ (bench.requests ().size (), 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().replies, 2u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenATwoPhaseRowAcknowledged_WhenTheCompletionNeverComes_ThenItTimesOutWithoutARetry)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x06 }), ms (500)); });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = run_one (
      bench, with_policy (dt::make_exchange<pump::PRIME_t> (std::uint16_t (5)),
                          1000, 2));
  EXPECT_EQ (prime->error, protocol_errc::timeout);
  EXPECT_EQ (std::chrono::duration_cast<ms> (prime->at - start).count (),
             1500);
  EXPECT_EQ (bench.requests ().size (), 1u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenATwoPhaseAckHandledJustAsTheTimeoutEnds_WhenTheCompletionWaitStarts_ThenTheEndedWaitDoesNotEndIt)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<dt::manual_link_t> const link
      = std::make_shared<dt::manual_link_t> (context);
  pr::dialogue_options_t options;
  options.timers = clock.source ();
  pr::dialogue_engine engine (pr::frame_link (link), dt::pump_index (),
                              options);
  pr::exchange_t request
      = dt::make_exchange<pump::PRIME_t> (std::uint16_t (10));
  request.policy = dt::policy (100, 0);
  outcome_ptr const out = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { out, &clock };
  engine.async_exchange (request, handler);
  context.poll ();
  context.restart ();
  ASSERT_TRUE (link->deliver (bytes ({ 0x06 })));
  clock.advance (ms (100));
  context.poll ();
  context.restart ();
  EXPECT_EQ (out->calls, 0);
  ASSERT_TRUE (link->deliver (bytes ({ 0x03, 0x0A, 0x00 })));
  context.poll ();
  context.restart ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAnEchoSet_WhenTheEchoDiffers_ThenItIsNotConfirmedWithBothValuesInTheLog)
{
  log_capture_t const capture;
  bench_t bench;
  bench.start ();
  bench.on_request (answers (
      bench, { bytes ({ 0x04, 0x64, 0x00 }), bytes ({ 0x04, 0x63, 0x00 }) }));
  outcome_ptr const same = run_one (
      bench, dt::make_exchange<pump::SET_FLOW_t> (std::uint16_t (100)));
  EXPECT_FALSE (same->error);
  outcome_ptr const other = run_one (
      bench, dt::make_exchange<pump::SET_FLOW_t> (std::uint16_t (100)));
  EXPECT_EQ (other->error, protocol_errc::not_confirmed);
  EXPECT_EQ (other->frame, bytes ({ 0x04, 0x63, 0x00 }));
  EXPECT_EQ (
      log_capture_t::count (
          log_level::warning,
          "SET_FLOW failed: the device did not confirm the value it was "
          "sent; sent: TX pump_t.SET_FLOW (Set the flow) flow=100; "
          "got: RX pump_t.SET_FLOW (Set the flow) ECHO value=99"),
      1u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAnEchoZeroOnError_WhenTheEchoIsZero_ThenItIsRefusedAndAnyOtherEchoCompletes)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      answers (bench, { bytes ({ 0x05, 0x00 }), bytes ({ 0x05, 0x07 }) }));
  outcome_ptr const refused = run_one (
      bench, dt::make_exchange<pump::SET_LIMIT_t> (std::uint8_t (9)));
  EXPECT_EQ (refused->error, protocol_errc::refused);
  // The device took another value than the one sent: still taken.
  outcome_ptr const taken = run_one (
      bench, dt::make_exchange<pump::SET_LIMIT_t> (std::uint8_t (9)));
  EXPECT_FALSE (taken->error);
  EXPECT_EQ (taken->frame, bytes ({ 0x05, 0x07 }));
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAStateInsteadOfRefusal_WhenTheStateDiffersFromTheValueSent_ThenItIsRefusedWithBothInTheLog)
{
  log_capture_t const capture;
  bench_t bench;
  bench.start ();
  bench.on_request (
      answers (bench, { bytes ({ 0x07, 0x01 }), bytes ({ 0x07, 0x02 }) }));
  outcome_ptr const echoed = run_one (
      bench,
      dt::make_exchange<pump::SET_MODE_t> (dt::tables::pump_state::running));
  EXPECT_FALSE (echoed->error);
  outcome_ptr const state = run_one (
      bench,
      dt::make_exchange<pump::SET_MODE_t> (dt::tables::pump_state::running));
  EXPECT_EQ (state->error, protocol_errc::refused);
  EXPECT_EQ (log_capture_t::count (
                 log_level::warning,
                 "SET_MODE failed: the device refused the request; sent: TX "
                 "pump_t.SET_MODE (Set the mode) mode=1; got: RX "
                 "pump_t.SET_MODE (Set the mode) ECHO value=2"),
             1u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAReplyOfAKindThatDoesNotDecode_WhenItComes_ThenTheExchangeEndsWithTheDecodingError)
{
  bench_t bench;
  bench.start ();
  bench.on_request (answers (bench, { bytes ({ 0x04, 0x64 }) }));
  outcome_ptr const flow = run_one (
      bench, dt::make_exchange<pump::SET_FLOW_t> (std::uint16_t (100)));
  EXPECT_EQ (flow->error, protocol_errc::truncated);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenSubscribers_WhenFramesAnswerNoExchange_ThenEachSubscriberGetsItsRowsInOrder)
{
  bench_t bench;
  bench.start ();
  std::shared_ptr<frames_t> const status = std::make_shared<frames_t> ();
  std::shared_ptr<frames_t> const change = std::make_shared<frames_t> ();
  pr::subscription_t const on_status = bench.the_dialogue->subscribe (
      dt::pump_index ().find_name ("STATUS"),
      [status] (pr::reply_frame_t const &frame)
        { status->seen.push_back (frame); });
  pr::subscription_t const on_change = bench.the_dialogue->subscribe (
      dt::pump_index ().find_name ("CHANGE"),
      [change] (pr::reply_frame_t const &frame)
        { change->seen.push_back (frame); });
  EXPECT_NE (on_status.id, 0u);
  EXPECT_NE (on_change.id, on_status.id);
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.reply (bytes ({ 0x30, 0x02, 0x05, 0x00 }));
  bench.reply (bytes ({ 0x12, 0x00, 0x00, 0x11, 0x00 }));
  bench.settle ();
  ASSERT_EQ (status->seen.size (), 2u);
  EXPECT_EQ (status->seen[0], bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  EXPECT_EQ (status->seen[1], bytes ({ 0x12, 0x00, 0x00, 0x11, 0x00 }));
  ASSERT_EQ (change->seen.size (), 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().events, 2u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 1u);
  // Unsubscribed: no more frames.
  EXPECT_TRUE (bench.the_dialogue->unsubscribe (on_status));
  EXPECT_FALSE (bench.the_dialogue->unsubscribe (on_status));
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.settle ();
  EXPECT_EQ (status->seen.size (), 2u);
  // A row the index has not, an empty handler: no subscription.
  EXPECT_EQ (
      bench.the_dialogue->subscribe (nullptr, pr::frame_handler_t ()).id, 0u);
  EXPECT_EQ (bench.the_dialogue
                 ->subscribe (dt::pump_index ().find_name ("STATUS"),
                              pr::frame_handler_t ())
                 .id,
             0u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenASubscriber_WhenAFrameAnswersAnExchangeOrComesLate_ThenTheSubscriberDoesNotGetIt)
{
  bench_t bench;
  bench.start ();
  std::shared_ptr<frames_t> const polls = std::make_shared<frames_t> ();
  bench.the_dialogue->subscribe (dt::pump_index ().find_name ("POLL"),
                                 [polls] (pr::reply_frame_t const &frame)
                                   { polls->seen.push_back (frame); });
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          std::size_t const number = bench.requests ().size ();
          bench.reply (bytes ({ 0x0C, static_cast<int> (number) }),
                       ms (number == 1 ? 10 : 150));
        });
  outcome_ptr const answered
      = run_one (bench, dt::make_exchange<pump::POLL_t> ());
  EXPECT_FALSE (answered->error);
  outcome_ptr const late = run_one (
      bench, with_policy (dt::make_exchange<pump::POLL_t> (), 100, 0));
  EXPECT_EQ (late->error, protocol_errc::timeout);
  bench.advance (ms (100));
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_TRUE (polls->seen.empty ());
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenASubscriberThatThrowsAndOneThatDestroysTheEngine_WhenAFrameComes_ThenBothRunAndTheExceptionLeavesRun)
{
  bench_t bench;
  bench.start ();
  int second_calls = 0;
  bench.the_dialogue->subscribe (
      dt::pump_index ().find_name ("STATUS"), [] (pr::reply_frame_t const &)
        { throw std::runtime_error ("subscriber"); });
  bench.the_dialogue->subscribe (dt::pump_index ().find_name ("STATUS"),
                                 [&second_calls] (pr::reply_frame_t const &)
                                   { ++second_calls; });
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  EXPECT_THROW (bench.settle (), std::runtime_error);
  EXPECT_EQ (second_calls, 1);
  // The reading goes on.
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  EXPECT_THROW (bench.settle (), std::runtime_error);
  EXPECT_EQ (second_calls, 2);
  EXPECT_EQ (bench.the_dialogue->statistics ().events, 2u);
  // A subscriber may destroy the engine it subscribed to.
  bench_t other;
  other.start ();
  int calls = 0;
  other.the_dialogue->subscribe (dt::pump_index ().find_name ("STATUS"),
                                 [&other, &calls] (pr::reply_frame_t const &)
                                   {
                                     ++calls;
                                     other.the_dialogue.reset ();
                                   });
  outcome_ptr const pending = other.start_exchange (
      with_policy (dt::make_exchange<pump::POLL_t> (), 0, 0));
  other.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  other.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  other.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (other.the_dialogue, nullptr);
  ASSERT_EQ (pending->calls, 1);
  EXPECT_EQ (pending->error, transport_errc::cancelled);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenASubscriberThatOwnsTheEngine_WhenTheIoContextIsDestroyed_ThenTheEngineIsFreed)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watch;
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    std::shared_ptr<dt::manual_link_t> const link
        = std::make_shared<dt::manual_link_t> (context);
    pr::dialogue_options_t options;
    options.timers = clock.source ();
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine.reset (new pr::dialogue_engine (pr::frame_link (link),
                                                  dt::pump_index (), options));
    watch = owner;
    owner->engine->subscribe (dt::pump_index ().find_name ("STATUS"),
                              [owner] (pr::reply_frame_t const &) {});
    context.poll ();
    owner.reset ();
    EXPECT_FALSE (watch.expired ());
  }
  EXPECT_TRUE (watch.expired ());
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenUnknownFrames_WhenTheyCome_ThenOnUnknownGetsEachAndOneWarningPerBurstIsLogged)
{
  log_capture_t const capture;
  bench_t bench;
  bench.start ();
  std::shared_ptr<frames_t> const unknown = std::make_shared<frames_t> ();
  bench.the_dialogue->on_unknown ([unknown] (pr::reply_frame_t const &frame)
                                    { unknown->seen.push_back (frame); });
  bench.reply (bytes ({ 0x7E, 0x01 }));
  bench.reply (bytes ({ 0x7D }));
  bench.settle ();
  ASSERT_EQ (unknown->seen.size (), 2u);
  EXPECT_EQ (unknown->seen[0], bytes ({ 0x7E, 0x01 }));
  EXPECT_EQ (log_capture_t::count (log_level::warning, "a frame of no row: "
                                                       "RX unknown key=0x7E "
                                                       "len=2: 7E 01"),
             1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "a frame of no row"),
             1u);
  EXPECT_EQ (log_capture_t::count (log_level::debug,
                                   "a frame of no row: RX unknown key=0x7D"),
             1u);
  // A known frame ends the burst.
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.reply (bytes ({ 0x7C }));
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::warning, "a frame of no row"),
             2u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 3u);
  // Removed: no more calls.
  bench.the_dialogue->on_unknown (pr::frame_handler_t ());
  bench.reply (bytes ({ 0x7E }));
  bench.settle ();
  EXPECT_EQ (unknown->seen.size (), 3u);
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenAnExchange_WhenTheFramesGoBothWays_ThenEachDebugLineIsTheDescribeLineOfTheIndex)
{
  log_capture_t const capture;
  bench_t bench;
  bench.start ();
  bench.on_request (answers (bench, { bytes ({ 0x01, 0x05, 0x34, 0x12 }) }));
  outcome_ptr const start
      = run_one (bench, dt::make_exchange<pump::START_t> (std::uint8_t (2)));
  EXPECT_EQ (start->error, protocol_errc::nak);
  std::vector<std::string> const lines
      = log_capture_t::messages (log_level::debug);
  std::vector<std::string> frames;
  for (std::size_t i = 0; i < lines.size (); ++i)
    if (lines[i].compare (0, 3, "TX ") == 0
        || lines[i].compare (0, 3, "RX ") == 0)
      frames.push_back (lines[i]);
  ASSERT_EQ (frames.size (), 2u);
  EXPECT_EQ (frames[0], "TX pump_t.START (Start) channel=2");
  EXPECT_EQ (frames[1], "RX pump_t.START (Start) NAK code=5 detail=4660");
  bytes_t const request = bytes ({ 0x01, 0x02 });
  EXPECT_EQ (frames[0],
             dt::pump_index ().describe (
                 dt::layout::bytes_view_t (request.data (), request.size ()),
                 dt::index::frame_direction::tx));
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenADatagramLinkWithAReportId_WhenRequestsAndRetriesAreSent_ThenEachKeepsTheReportId)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t mock_options;
  mock_options.timers = clock.source ();
  tr::testing::mock_datagram mock = tr::testing::make_mock_datagram (
      context.get_executor (), mock_options);
  mock.transport.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  tr::testing::mock_datagram_control control = mock.control;
  std::shared_ptr<int> sends = std::make_shared<int> (0);
  control.on_send (
      [control, sends] (tr::datagram::message_t const &) mutable
        {
          // The second request is answered, in a report of another number.
          if (++*sends < 2)
            return;
          tr::datagram::message_t reply;
          reply.report_id = 7;
          bytes_t const message = bytes ({ 0x0C, 0x2A });
          fr::slip::slip_codec ().encode (
              fr::codec::bytes_view_t (message.data (), message.size ()),
              reply.payload);
          control.deliver (reply);
        });
  pr::dialogue_options_t options;
  options.timers = clock.source ();
  options.name = "report";
  std::unique_ptr<pr::dialogue_engine> engine (new pr::dialogue_engine (
      pr::frame_link (tr::framed::framed_datagram<fr::slip::slip_codec> (
                          std::move (mock.transport), fr::slip::slip_codec ()),
                      std::uint8_t (3)),
      dt::pump_index (), options));
  outcome_ptr const out = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { out, &clock };
  engine->async_exchange (
      with_policy (dt::make_exchange<pump::POLL_t> (), 100, 1), handler);
  ASSERT_TRUE (tr::testing::run_until (
      context, clock, [&] () { return out->calls != 0; }, ms (10000)));
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, bytes ({ 0x0C, 0x2A }));
  std::vector<tr::datagram::message_t> const sent = control.sent ();
  ASSERT_EQ (sent.size (), 2u);
  EXPECT_EQ (sent[0].report_id, 3);
  EXPECT_EQ (sent[1].report_id, 3);
  control.on_send (tr::testing::send_observer_t ());
  engine.reset ();
  tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
}

TEST (
    LumexDialogueReplyKindsTest,
    GivenSubscribersAndAnUnknownHandler_WhenTheDialogueIsClosed_ThenTheyAreDroppedAndNoLongerCalled)
{
  bench_t bench;
  bench.start ();
  std::shared_ptr<int> const kept = std::make_shared<int> (0);
  bench.the_dialogue->subscribe (dt::pump_index ().find_name ("STATUS"),
                                 [kept] (pr::reply_frame_t const &)
                                   { ++*kept; });
  bench.the_dialogue->on_unknown ([kept] (pr::reply_frame_t const &)
                                    { ++*kept; });
  EXPECT_EQ (kept.use_count (), 3);
  bench.the_dialogue->close ();
  bench.settle ();
  EXPECT_EQ (kept.use_count (), 1);
  EXPECT_EQ (*kept, 0);
}

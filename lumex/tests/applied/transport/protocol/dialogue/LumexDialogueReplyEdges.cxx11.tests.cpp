// Edges of the part B of the dialogue engine: the value sent of the echo
// kinds, the acknowledgements of a two-phase row, the warning rate of the
// unknown frames, handlers after close (), a reader that throws, a
// subscription with the row of another index, and the late replies, the
// window and the next exchange after a timeout that followed an
// acknowledgement. The bench of the other dialogue suites; the device
// answers as each test says.

#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
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
namespace tr = dt::tr;
namespace pr = dt::pr;
using tr::log::log_level;

pr::exchange_t
with_policy (pr::exchange_t request, std::uint32_t timeout_ms,
             std::uint32_t retries)
{
  request.policy = dt::policy (timeout_ms, retries);
  return request;
}

bool
accept_everything (lumex::core::protocol::index::row_t const *,
                   lumex::core::protocol::layout::bytes_view_t,
                   lumex::core::protocol::layout::bytes_view_t)
{
  return true;
}

// A reader that throws, as a decoder out of memory would.
pr::reply_reading_t
throwing_reader (lumex::core::protocol::layout::bytes_view_t,
                 lumex::core::protocol::field::decode_context_t const *,
                 lumex::core::protocol::layout::bytes_view_t)
{
  throw std::bad_alloc ();
}
} // namespace

// --- the value sent of the echo kinds ---------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAnEchoSetOfARowOfTwoFields_WhenTheEchoIsTheLastFieldOrTheFirst_ThenOnlyTheLastConfirms)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          std::size_t const number = bench.requests ().size ();
          // The slot is 1, the value is 7: the first answer echoes the value,
          // the second the slot.
          bench.reply (number == 1 ? bytes ({ 0x19, 0x07 })
                                   : bytes ({ 0x19, 0x01 }));
        });
  outcome_ptr const confirmed
      = bench.start_exchange (dt::make_exchange<pump::PAIR_SET_t> (
          std::uint8_t (1), std::uint8_t (7)));
  ASSERT_TRUE (bench.run_until ([&] () { return confirmed->calls != 0; }));
  EXPECT_FALSE (confirmed->error) << confirmed->error.message ();
  outcome_ptr const other
      = bench.start_exchange (dt::make_exchange<pump::PAIR_SET_t> (
          std::uint8_t (1), std::uint8_t (7)));
  ASSERT_TRUE (bench.run_until ([&] () { return other->calls != 0; }));
  EXPECT_EQ (other->error, protocol_errc::not_confirmed);
}

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAnEchoZeroOnErrorOfARowOfTwoFields_WhenTheLastFieldSentIsZero_ThenAZeroEchoConfirmsIt)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x1A, 0x00 })); });
  // Slot 5, value 0: the value sent is the last field, 0.
  outcome_ptr const zero
      = bench.start_exchange (dt::make_exchange<pump::PAIR_ZERO_t> (
          std::uint8_t (5), std::uint8_t (0)));
  ASSERT_TRUE (bench.run_until ([&] () { return zero->calls != 0; }));
  EXPECT_FALSE (zero->error) << zero->error.message ();
  EXPECT_EQ (zero->frame, bytes ({ 0x1A, 0x00 }));
  // Slot 0, value 5: the zero echo is a refusal.
  outcome_ptr const refused
      = bench.start_exchange (dt::make_exchange<pump::PAIR_ZERO_t> (
          std::uint8_t (0), std::uint8_t (5)));
  ASSERT_TRUE (bench.run_until ([&] () { return refused->calls != 0; }));
  EXPECT_EQ (refused->error, protocol_errc::refused);
}

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAnEchoZeroOnError_WhenZeroIsSentAndEchoed_ThenItSucceedsForEveryValueType)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          int const key = dt::key_of (request);
          if (key == 0x05)
            bench.reply (bytes ({ 0x05, 0x00 }));
          else if (key == 0x16)
            bench.reply (bytes ({ 0x16, 0x00 }));
          else if (key == 0x17)
            bench.reply (bytes ({ 0x17, 0x00, 0x00 }));
          else
            bench.reply (bytes ({ 0x18, 0x00, 0x00, 0x00, 0x00 }));
        });
  outcome_ptr const limit = bench.start_exchange (
      dt::make_exchange<pump::SET_LIMIT_t> (std::uint8_t (0)));
  outcome_ptr const state = bench.start_exchange (
      dt::make_exchange<pump::SET_STATE_t> (dt::tables::pump_state::idle));
  ASSERT_TRUE (bench.run_until (
      [&] () { return limit->calls != 0 && state->calls != 0; }));
  EXPECT_FALSE (limit->error) << limit->error.message ();
  EXPECT_FALSE (state->error) << state->error.message ();
  outcome_ptr const rate
      = bench.start_exchange (dt::make_exchange<pump::SET_RATE_t> (0.0f));
  ASSERT_TRUE (bench.run_until ([&] () { return rate->calls != 0; }));
  EXPECT_FALSE (rate->error) << rate->error.message ();
  // The same zero echo to a write of 3 is a refusal.
  outcome_ptr const sent = bench.start_exchange (
      dt::make_exchange<pump::SET_LIMIT_t> (std::uint8_t (3)));
  ASSERT_TRUE (bench.run_until ([&] () { return sent->calls != 0; }));
  EXPECT_EQ (sent->error, protocol_errc::refused);
}

// --- a two-phase row --------------------------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenATwoPhaseRow_WhenAcknowledgementsComeFasterThanTheTimeout_ThenTheFirstOneArmsTheTimerAndTheExchangeEndsAtThatTimePlusTheTimeout)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          // At 300, 600, 900 and 1200: every one before the end of the
          // timeout the first started (1300).
          for (int at = 300; at <= 1200; at += 300)
            bench.reply (bytes ({ 0x06 }), ms (at));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = bench.start_exchange (with_policy (
      dt::make_exchange<pump::PRIME_t> (std::uint16_t (5)), 1000, 0));
  ASSERT_TRUE (bench.run_until ([&] () { return prime->calls != 0; }));
  EXPECT_EQ (prime->error, protocol_errc::timeout);
  EXPECT_EQ (std::chrono::duration_cast<ms> (prime->at - start).count (),
             1300);
  EXPECT_EQ (bench.the_dialogue->statistics ().replies, 4u);
}

TEST (
    LumexDialogueReplyEdgesTest,
    GivenATwoPhaseRow_WhenTheAcknowledgementIsRepeated_ThenTheExchangeKeepsWaitingForTheCompletion)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          bench.reply (bytes ({ 0x06 }), ms (100));
          bench.reply (bytes ({ 0x06 }), ms (200));
          bench.reply (bytes ({ 0x03, 0xE8, 0x03 }), ms (800));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = bench.start_exchange (with_policy (
      dt::make_exchange<pump::PRIME_t> (std::uint16_t (1000)), 1000, 0));
  ASSERT_TRUE (bench.run_until ([&] () { return prime->calls != 0; }));
  EXPECT_FALSE (prime->error) << prime->error.message ();
  EXPECT_EQ (prime->frame, bytes ({ 0x03, 0xE8, 0x03 }));
  EXPECT_EQ (std::chrono::duration_cast<ms> (prime->at - start).count (), 800);
}

// --- the unknown frames -----------------------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenNoiseBetweenGoodFrames_WhenTheClockMoves_ThenOneWarningPerSecondCarriesTheNumberSkipped)
{
  log_capture_t const capture;
  bench_t bench;
  bench.start ();
  std::shared_ptr<std::vector<bytes_t>> const seen
      = std::make_shared<std::vector<bytes_t>> ();
  bench.the_dialogue->on_unknown ([seen] (pr::reply_frame_t const &frame)
                                    { seen->push_back (frame); });
  bytes_t const good = bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 });
  // t = 0: the first is warned about, the second is not.
  bench.reply (bytes ({ 0x7E, 0x01 }));
  bench.reply (bytes ({ 0x7D }));
  bench.settle ();
  // A good frame in between changes nothing.
  bench.advance (ms (100));
  bench.reply (good);
  bench.advance (ms (400));
  bench.reply (bytes ({ 0x7C }));
  bench.advance (ms (499));
  bench.reply (bytes ({ 0x7B }));
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::warning, "a frame of no row"),
             1u);
  // t = 1000: a second after the first warning.
  bench.advance (ms (1));
  bench.reply (bytes ({ 0x7A }));
  bench.settle ();
  bench.reply (good);
  bench.advance (ms (1000));
  // t = 2000: nothing was skipped since the last warning.
  bench.reply (bytes ({ 0x79 }));
  bench.settle ();
  std::vector<std::string> const warnings
      = log_capture_t::messages (log_level::warning);
  std::vector<std::string> noise;
  for (std::size_t i = 0; i < warnings.size (); ++i)
    if (warnings[i].compare (0, 17, "a frame of no row") == 0)
      noise.push_back (warnings[i]);
  ASSERT_EQ (noise.size (), 3u);
  EXPECT_EQ (noise[0], "a frame of no row: RX unknown key=0x7E len=2: 7E 01");
  EXPECT_EQ (noise[1],
             "a frame of no row: RX unknown key=0x7A len=1: 7A (3 more since "
             "the last warning, logged at debug)");
  EXPECT_EQ (noise[2], "a frame of no row: RX unknown key=0x79 len=1: 79");
  EXPECT_EQ (
      log_capture_t::count (log_level::debug, "a frame of no row: RX unknown"),
      3u);
  EXPECT_EQ (seen->size (), 6u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 6u);
}

TEST (
    LumexDialogueReplyEdgesTest,
    GivenTheWindowOfALateByOrderReply_WhenAnUnknownFrameComes_ThenOnUnknownDoesNotGetItAndItIsCountedLate)
{
  log_capture_t const capture;
  pr::dialogue_options_t options;
  options.late_reply_window_ms = 100;
  bench_t bench (options);
  bench.start ();
  std::shared_ptr<int> const calls = std::make_shared<int> (0);
  bench.the_dialogue->on_unknown ([calls] (pr::reply_frame_t const &)
                                    { ++*calls; });
  outcome_ptr const read = bench.start_exchange (
      with_policy (dt::make_exchange<pump::READ_t> (std::uint8_t (1)), 50, 0));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_EQ (read->error, protocol_errc::timeout);
  bench.reply (bytes ({ 0x7E }));
  bench.settle ();
  EXPECT_EQ (*calls, 0);
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 0u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "a frame of no row"),
             0u);
  // After the window the frame is an unknown one.
  bench.advance (ms (100));
  bench.reply (bytes ({ 0x7E }));
  bench.settle ();
  EXPECT_EQ (*calls, 1);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 1u);
}

// --- handlers after close () ------------------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAClosedDialogue_WhenSubscribeIsCalled_ThenItRefusesAndKeepsNoHandler)
{
  bench_t bench;
  bench.start ();
  bench.the_dialogue->close ();
  bench.settle ();
  std::shared_ptr<int> const kept = std::make_shared<int> (0);
  pr::subscription_t const made = bench.the_dialogue->subscribe (
      dt::pump_index ().find_name ("STATUS"),
      [kept] (pr::reply_frame_t const &) { ++*kept; });
  EXPECT_EQ (made.id, 0u);
  EXPECT_EQ (kept.use_count (), 1);
  EXPECT_FALSE (bench.the_dialogue->unsubscribe (made));
}

TEST (LumexDialogueReplyEdgesTest,
      GivenAClosedDialogue_WhenOnUnknownIsCalled_ThenTheHandlerIsIgnored)
{
  bench_t bench;
  bench.start ();
  bench.the_dialogue->close ();
  bench.settle ();
  std::shared_ptr<int> const kept = std::make_shared<int> (0);
  bench.the_dialogue->on_unknown ([kept] (pr::reply_frame_t const &)
                                    { ++*kept; });
  EXPECT_EQ (kept.use_count (), 1);
}

// --- a reader that throws ---------------------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAReaderThatThrows_WhenAFrameAnswers_ThenTheExchangeEndsOutOfMemoryAndTheNextRuns)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x01 })); });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.reader = &throwing_reader;
  outcome_ptr const failed = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return failed->calls != 0; }));
  EXPECT_EQ (failed->error, std::errc::not_enough_memory);
  outcome_ptr const next
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return next->calls != 0; }));
  EXPECT_FALSE (next->error);
}

// --- subscribing with the row of another index ------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenARowOfAnotherIndex_WhenItIsSubscribed_ThenTheRowOfThisIndexByNameGetsTheFramesAndAnUnknownNameIsRefused)
{
  bench_t bench;
  bench.start ();
  std::shared_ptr<int> const calls = std::make_shared<int> (0);
  lumex::core::protocol::index::row_t const *const other
      = dt::index::protocol_index<dt::tables::pump_v2_t> ()->find_name (
          "STATUS");
  ASSERT_NE (other, nullptr);
  ASSERT_NE (other, dt::pump_index ().find_name ("STATUS"));
  pr::subscription_t const found = bench.the_dialogue->subscribe (
      other, [calls] (pr::reply_frame_t const &) { ++*calls; });
  EXPECT_NE (found.id, 0u);
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.settle ();
  EXPECT_EQ (*calls, 1);
  // A row whose name the index of this dialogue has not.
  lumex::core::protocol::index::row_t const *const foreign
      = dt::index::protocol_index<dt::tables::foreign_t> ()->find_name ("ODD");
  ASSERT_NE (foreign, nullptr);
  pr::subscription_t const refused = bench.the_dialogue->subscribe (
      foreign, [calls] (pr::reply_frame_t const &) { ++*calls; });
  EXPECT_EQ (refused.id, 0u);
}

// --- the order in which a frame is offered ----------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenACustomExchangeAheadOfAByReplyKeyOne_WhenTheReplyRowComes_ThenTheByReplyKeyExchangeGetsIt)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 500;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x0F)
            bench.reply (bytes ({ 0x30, 0x01, 0x05, 0x00 }), ms (10));
        });
  pr::exchange_t query = dt::make_exchange<pump::QUERY_t> ();
  query.matcher = &accept_everything;
  outcome_ptr const custom = bench.start_exchange (query);
  outcome_ptr const write = bench.start_exchange (
      dt::make_exchange<pump::WRITE_NAMED_t> (std::uint8_t (1)));
  bench.advance (ms (100));
  ASSERT_EQ (write->calls, 1);
  EXPECT_FALSE (write->error);
  EXPECT_EQ (write->frame, bytes ({ 0x30, 0x01, 0x05, 0x00 }));
  EXPECT_EQ (custom->calls, 0);
}

// --- a timeout that followed an acknowledgement -----------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAByKeyTwoPhaseRowAcknowledgedAndTimedOut_WhenTheCompletionComesLate_ThenItIsALateReplyAndTheQueuedExchangeStartedAtTheTimeout)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 1;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x1B)
            bench.reply (bytes ({ 0x1B, 0x01 }), ms (50));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = bench.start_exchange (with_policy (
      dt::make_exchange<pump::PRIME_KEY_t> (std::uint16_t (1000)), 100, 0));
  outcome_ptr const poll = bench.start_exchange (
      with_policy (dt::make_exchange<pump::POLL_t> (), 1000, 0));
  ASSERT_TRUE (bench.run_until ([&] () { return prime->calls != 0; }));
  EXPECT_EQ (prime->error, protocol_errc::timeout);
  // The queued exchange was written when the timeout (100 ms after the
  // acknowledgement at 50) ended the first.
  ASSERT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (dt::key_of (bench.requests ()[1].message), 0x0C);
  EXPECT_EQ (std::chrono::duration_cast<ms> (bench.requests ()[1].at - start)
                 .count (),
             150);
  bench.reply (bytes ({ 0x1B, 0xE8, 0x03 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 0u);
  EXPECT_EQ (poll->calls, 0);
  bench.reply (bytes ({ 0x0C, 0x02 }));
  bench.settle ();
  ASSERT_EQ (poll->calls, 1);
  EXPECT_FALSE (poll->error);
}

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAByOrderTwoPhaseRowAcknowledgedAndTimedOut_WhenTheCompletionComesLate_ThenTheWindowDropsItAndDelaysTheNextRequest)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 1;
  options.late_reply_window_ms = 100;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x03)
            bench.reply (bytes ({ 0x06 }), ms (50));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const prime = bench.start_exchange (with_policy (
      dt::make_exchange<pump::PRIME_t> (std::uint16_t (1000)), 100, 0));
  outcome_ptr const poll = bench.start_exchange (
      with_policy (dt::make_exchange<pump::POLL_t> (), 1000, 0));
  ASSERT_TRUE (bench.run_until ([&] () { return prime->calls != 0; }));
  EXPECT_EQ (prime->error, protocol_errc::timeout);
  bench.settle ();
  // The window is open: the next request waits for it.
  EXPECT_EQ (bench.requests ().size (), 1u);
  bench.reply (bytes ({ 0x03, 0xE8, 0x03 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 0u);
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.requests ().size () == 2u; }));
  EXPECT_EQ (std::chrono::duration_cast<ms> (bench.requests ()[1].at - start)
                 .count (),
             250);
  EXPECT_EQ (poll->calls, 0);
}

// --- a failed write ---------------------------------------------------------

TEST (
    LumexDialogueReplyEdgesTest,
    GivenAWriteThatFailsWithAnExchangeQueued_WhenItEnds_ThenTheQueuedExchangeStartsAndIsAnswered)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 1;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x01 })); });
  bench.control->fail_next_write (make_error_code (std::errc::io_error));
  outcome_ptr const first
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_EQ (first->error, std::errc::io_error);
  EXPECT_FALSE (second->error) << second->error.message ();
}

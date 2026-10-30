// The dialogue engine (lumex/applied/transport/protocol/dialogue): the
// correlations, the in-flight limit and the queue, the timeouts and the
// retries on the virtual clock, the late replies, the rows without a reply,
// the errors of the link and the counters. Each exchange runs over a mock
// stream framed with SLIP; the device of the bench answers as each test
// says, or through a behaviour of the simulator.

#include <chrono>
#include <cstdint>
#include <memory>
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
using dt::ms;
using dt::outcome_ptr;
using dt::protocol_errc;
using dt::pump;
using dt::transport_errc;
namespace tr = dt::tr;
namespace pr = dt::pr;

bool
all_done (std::vector<outcome_ptr> const &outcomes)
{
  for (std::size_t i = 0; i < outcomes.size (); ++i)
    if (outcomes[i]->calls == 0)
      return false;
  return true;
}

// A predicate with its own state: `left` extra attempts in all.
struct budget_t
{
  explicit budget_t (int count) : left (count) {}

  std::uint32_t
  operator() (lumex::core::protocol::index::session_context_t const &,
              std::error_code const &)
  {
    if (left == 0)
      return 0;
    --left;
    return 1;
  }

  int left;
};

std::int64_t
since (tr::timing::time_point_t start, tr::timing::time_point_t at)
{
  return std::chrono::duration_cast<ms> (at - start).count ();
}
} // namespace

TEST (
    LumexDialogueTest,
    GivenTwoByKeyExchangesInFlight_WhenTheRepliesComeSwapped_ThenEachGetsItsOwn)
{
  bench_t bench;
  bench.link_options ().max_in_flight = 2;
  bench.start ();
  bytes_t const info_reply
      = bytes ({ 0x10, 'A', 'B', 'C', 0, 0, 0, 0, 0, 0x02, 0x01 });
  bench.on_request (
      [&bench, &info_reply] (bytes_t const &)
        {
          if (bench.requests ().size () == 2)
            {
              bench.reply (info_reply);
              bench.reply (bytes ({ 0x0C, 0x2A }));
            }
        });
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const info
      = bench.start_exchange (dt::make_exchange<pump::INFO_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return all_done ({ poll, info }); }));
  EXPECT_FALSE (poll->error) << poll->error.message ();
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x2A }));
  EXPECT_FALSE (info->error) << info->error.message ();
  EXPECT_EQ (info->frame, info_reply);
  EXPECT_EQ (poll->calls, 1);
  EXPECT_EQ (info->calls, 1);
  EXPECT_EQ (bench.the_dialogue->statistics ().replies, 2u);
}

TEST (LumexDialogueTest,
      GivenAByOrderExchange_WhenAnotherIsStarted_ThenItWaitsUntilTheFirstEnded)
{
  bench_t bench;
  bench.link_options ().max_in_flight = 2;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x09)
            bench.reply (bytes ({ 0x09, 0x34, 0x12 }), ms (10));
          else
            bench.reply (bytes ({ 0x0C, 0x01 }), ms (10));
        });
  outcome_ptr const read = bench.start_exchange (
      dt::make_exchange<pump::READ_t> (std::uint8_t (4)));
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  // The by_order request is alone on the wire.
  ASSERT_EQ (bench.requests ().size (), 1u);
  EXPECT_EQ (dt::key_of (bench.requests ()[0].message), 0x09);
  ASSERT_TRUE (bench.run_until ([&] () { return all_done ({ read, poll }); }));
  EXPECT_EQ (read->frame, bytes ({ 0x09, 0x34, 0x12 }));
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x01 }));
  ASSERT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (since (bench.requests ()[0].at, bench.requests ()[1].at), 10);
}

TEST (LumexDialogueTest,
      GivenAByKeyExchangeInFlight_WhenAByOrderOneIsStarted_ThenItWaitsForIt)
{
  bench_t bench;
  bench.link_options ().max_in_flight = 4;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          bench.reply (dt::key_of (request) == 0x0C
                           ? bytes ({ 0x0C, 0x01 })
                           : bytes ({ 0x09, 0x02, 0x00 }),
                       ms (20));
        });
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const read = bench.start_exchange (
      dt::make_exchange<pump::READ_t> (std::uint8_t (1)));
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 1u);
  ASSERT_TRUE (
      bench.run_until ([&] () { return all_done ({ poll, read, second }); }));
  ASSERT_EQ (bench.requests ().size (), 3u);
  EXPECT_EQ (dt::key_of (bench.requests ()[1].message), 0x09);
  EXPECT_EQ (dt::key_of (bench.requests ()[2].message), 0x0C);
  EXPECT_EQ (since (bench.requests ()[0].at, bench.requests ()[2].at), 40);
  EXPECT_EQ (read->frame, bytes ({ 0x09, 0x02, 0x00 }));
}

TEST (
    LumexDialogueTest,
    GivenAByReplyKeyRow_WhenItsOwnKeyAndThenTheReplyRowAnswer_ThenOnlyTheReplyRowEndsIt)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          bench.reply (bytes ({ 0x08, 0x03, 0xF4, 0x01 }));
          bench.reply (bytes ({ 0x30, 0x03, 0xF4, 0x01 }), ms (5));
        });
  outcome_ptr const write
      = bench.start_exchange (dt::make_exchange<pump::WRITE_t> (
          std::uint8_t (3), std::uint16_t (500)));
  ASSERT_TRUE (bench.run_until ([&] () { return write->calls != 0; }));
  EXPECT_FALSE (write->error) << write->error.message ();
  EXPECT_EQ (write->frame, bytes ({ 0x30, 0x03, 0xF4, 0x01 }));
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_EQ (counters.unexpected_replies, 1u);
  EXPECT_EQ (counters.replies, 1u);
}

TEST (LumexDialogueTest,
      GivenACustomRow_WhenAFrameComes_ThenTheMatcherOfTheTableTakesIt)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x7E, 0x01, 0x02 })); });
  outcome_ptr const query
      = bench.start_exchange (dt::make_exchange<pump::QUERY_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return query->calls != 0; }));
  EXPECT_FALSE (query->error);
  EXPECT_EQ (query->frame, bytes ({ 0x7E, 0x01, 0x02 }));
}

TEST (
    LumexDialogueTest,
    GivenASilentDevice_WhenTheRowHasRetries_ThenEachAttemptTimesOutThenRetriesAreExhausted)
{
  bench_t bench;
  bench.start ();
  // A device that never answers (the behaviour of the simulator).
  tr::testing::behavior_applier silent (tr::testing::behavior::silent (),
                                        bench.clock.source ());
  bench.on_request (
      [&bench, &silent] (bytes_t const &request)
        { silent.respond (*bench.control, request, bytes ({ 0x0C, 0x01 })); });
  tr::timing::time_point_t const start = bench.clock.now ();
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 2);
  outcome_ptr const poll = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_EQ (poll->error, protocol_errc::retries_exhausted);
  std::vector<dt::request_t> const sent = bench.requests ();
  ASSERT_EQ (sent.size (), 3u);
  EXPECT_EQ (since (start, sent[0].at), 0);
  EXPECT_EQ (since (start, sent[1].at), 100);
  EXPECT_EQ (since (start, sent[2].at), 200);
  EXPECT_EQ (since (start, poll->at), 300);
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_EQ (counters.requests, 3u);
  EXPECT_EQ (counters.retries, 2u);
  EXPECT_EQ (counters.timeouts, 3u);
  EXPECT_EQ (counters.retries_exhausted, 1u);
}

TEST (
    LumexDialogueTest,
    GivenARowWithoutRetries_WhenNoReplyComes_ThenItEndsWithTimeoutAfterOneAttempt)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 250;
  bench_t bench (options);
  bench.start ();
  tr::timing::time_point_t const start = bench.clock.now ();
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_EQ (poll->error, protocol_errc::timeout);
  EXPECT_EQ (bench.requests ().size (), 1u);
  EXPECT_EQ (since (start, poll->at), 250);
  EXPECT_EQ (bench.the_dialogue->statistics ().retries_exhausted, 0u);
  EXPECT_EQ (bench.the_dialogue->statistics ().timeouts, 1u);
}

TEST (
    LumexDialogueTest,
    GivenARetryThatIsAnswered_WhenTheFirstAttemptTimedOut_ThenTheExchangeSucceeds)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          if (bench.requests ().size () == 2)
            bench.reply (bytes ({ 0x0C, 0x05 }), ms (30));
        });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 3);
  outcome_ptr const poll = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_FALSE (poll->error);
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x05 }));
  EXPECT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (bench.the_dialogue->statistics ().retries, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().timeouts, 1u);
}

TEST (
    LumexDialogueTest,
    GivenADeviceThatInitializes_WhenTheAttemptsRunOut_ThenTheSessionGrantsMoreUntilItIsReady)
{
  bench_t bench;
  bench.start ();
  bench.the_dialogue->session ().set ("initializing", 1);
  int asked = 0;
  pr::extra_retries_t const while_initializing =
      [&asked] (lumex::core::protocol::index::session_context_t const &session,
                std::error_code const &error) -> std::uint32_t
    {
      ++asked;
      EXPECT_EQ (error, protocol_errc::timeout);
      std::int64_t value = 0;
      return session.find ("initializing", value) && value == 1 ? 1u : 0u;
    };
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          if (bench.requests ().size () == 4)
            bench.reply (bytes ({ 0x0C, 0x09 }));
        });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 1, while_initializing);
  outcome_ptr const poll = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_FALSE (poll->error) << poll->error.message ();
  EXPECT_EQ (bench.requests ().size (), 4u);
  EXPECT_EQ (asked, 2);

  // Once the device says it is ready, the attempts end with the policy's.
  bench.the_dialogue->session ().set ("initializing", 0);
  bench.on_request (std::function<void (bytes_t const &)> ());
  pr::exchange_t again = dt::make_exchange<pump::POLL_t> ();
  again.policy = request.policy;
  outcome_ptr const second = bench.start_exchange (again);
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_EQ (second->error, protocol_errc::retries_exhausted);
  EXPECT_EQ (bench.requests ().size (), 6u);
  EXPECT_EQ (asked, 3);
}

TEST (
    LumexDialogueTest,
    GivenAStatefulPredicate_WhenTwoExchangesShareThePolicy_ThenTheOneObjectKeepsItsState)
{
  bench_t bench;
  bench.start ();
  // Two extra attempts in all, whatever exchange asks: the state is in the
  // function object itself, so a copy per exchange would grant them twice.
  std::shared_ptr<pr::retry_policy_t const> const one
      = dt::policy (50, 0, budget_t (2));
  pr::exchange_t first = dt::make_exchange<pump::POLL_t> ();
  first.policy = one;
  pr::exchange_t second = dt::make_exchange<pump::POLL_t> ();
  second.policy = one;
  outcome_ptr const a = bench.start_exchange (first);
  outcome_ptr const b = bench.start_exchange (second);
  ASSERT_TRUE (bench.run_until ([&] () { return all_done ({ a, b }); }));
  // 1 + 2 extra for the first, 1 for the second.
  EXPECT_EQ (bench.requests ().size (), 4u);
  EXPECT_EQ (a->error, protocol_errc::retries_exhausted);
  EXPECT_EQ (b->error, protocol_errc::timeout);
}

TEST (
    LumexDialogueTest,
    GivenAReplyThatComesAfterTheTimeout_WhenNothingWaits_ThenItIsLateAndTheNextExchangeGetsItsOwn)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          if (bench.requests ().size () == 1)
            bench.reply (bytes ({ 0x0C, 0x01 }), ms (150));
          else
            bench.reply (bytes ({ 0x0C, 0x02 }), ms (10));
        });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 0);
  outcome_ptr const first = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (first->error, protocol_errc::timeout);
  bench.advance (ms (100));
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 0u);
  outcome_ptr const second = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_FALSE (second->error) << second->error.message ();
  EXPECT_EQ (second->frame, bytes ({ 0x0C, 0x02 }));
}

TEST (
    LumexDialogueTest,
    GivenALateReplyThatComesAfterTheNextRequest_WhenItIsTaken_ThenTheReplyAfterItIsLateAndNothingShifts)
{
  bench_t bench;
  bench.start ();
  // The first request is answered after 150 ms, every other after 10 ms;
  // each reply carries the number of its request.
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          std::size_t const number = bench.requests ().size ();
          bench.reply (bytes ({ 0x0C, static_cast<int> (number) }),
                       ms (number == 1 ? 150 : 60));
        });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 0);
  outcome_ptr const first = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (first->error, protocol_errc::timeout);
  // The second request goes out at 100; the late reply of the first comes at
  // 150 and is taken (one row cannot tell them apart), its own at 160 is the
  // late one.
  outcome_ptr const second = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_EQ (second->frame, bytes ({ 0x0C, 1 }));
  bench.advance (ms (20));
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  outcome_ptr const third = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return third->calls != 0; }));
  EXPECT_FALSE (third->error);
  EXPECT_EQ (third->frame, bytes ({ 0x0C, 3 }));
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 0u);
}

TEST (
    LumexDialogueTest,
    GivenAByOrderRowAndALateReplyWindow_WhenAnAttemptTimesOut_ThenTheWindowDropsTheLateReplyAndDelaysTheRetry)
{
  pr::dialogue_options_t options;
  options.late_reply_window_ms = 50;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          std::size_t const number = bench.requests ().size ();
          bench.reply (bytes ({ 0x09, static_cast<int> (number), 0x00 }),
                       ms (number == 1 ? 120 : 10));
        });
  tr::timing::time_point_t const start = bench.clock.now ();
  pr::exchange_t request = dt::make_exchange<pump::READ_t> (std::uint8_t (7));
  request.policy = dt::policy (100, 1);
  outcome_ptr const read = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->frame, bytes ({ 0x09, 2, 0x00 }));
  std::vector<dt::request_t> const sent = bench.requests ();
  ASSERT_EQ (sent.size (), 2u);
  EXPECT_EQ (since (start, sent[1].at), 150);
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
}

TEST (
    LumexDialogueTest,
    GivenAByOrderRowWithoutAWindow_WhenItsLateReplyComes_ThenTheNextByOrderExchangeTakesIt)
{
  // The known limit of a reply without a key (late_reply_window_ms 0).
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          if (bench.requests ().size () == 1)
            bench.reply (bytes ({ 0x09, 1, 0x00 }), ms (120));
        });
  pr::exchange_t request = dt::make_exchange<pump::READ_t> (std::uint8_t (7));
  request.policy = dt::policy (100, 0);
  outcome_ptr const first = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (first->error, protocol_errc::timeout);
  outcome_ptr const second = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_FALSE (second->error);
  EXPECT_EQ (second->frame, bytes ({ 0x09, 1, 0x00 }));
}

TEST (
    LumexDialogueTest,
    GivenARowWithoutAReply_WhenItIsWritten_ThenTheExchangeEndsAtOnceWithoutATimer)
{
  bench_t bench;
  bench.start ();
  outcome_ptr const clock_set = bench.start_exchange (
      dt::make_exchange<pump::CLOCK_t> (std::uint32_t (12345)));
  // Queued behind it: the end of the write starts it.
  outcome_ptr const next = bench.start_exchange (
      dt::make_exchange<pump::CLOCK_t> (std::uint32_t (1)));
  bench.settle ();
  ASSERT_EQ (clock_set->calls, 1);
  EXPECT_FALSE (clock_set->error);
  EXPECT_TRUE (clock_set->frame.empty ());
  ASSERT_EQ (next->calls, 1);
  EXPECT_FALSE (next->error);
  EXPECT_EQ (bench.clock.pending (), 0u);
  ASSERT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (bench.requests ()[0].message,
             bytes ({ 0x11, 0x39, 0x30, 0x00, 0x00 }));
}

TEST (
    LumexDialogueTest,
    GivenAWriteThatFails_WhenTheExchangeIsInFlight_ThenItEndsWithTheErrorOfTheWrite)
{
  bench_t bench;
  bench.start ();
  bench.control->fail_next_write (make_error_code (std::errc::io_error));
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_EQ (poll->error, std::errc::io_error);
  // The dialogue goes on.
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x01 })); });
  outcome_ptr const next
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return next->calls != 0; }));
  EXPECT_FALSE (next->error);
}

TEST (
    LumexDialogueTest,
    GivenTheWriteOfAnEarlierAttemptFailsLate_WhenARetryIsOnItsWay_ThenTheRetryDecides)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x03 })); });
  // The first write is held (a frozen line); it times out, the retry waits
  // behind it, then the first write fails.
  bench.control->hold_writes ();
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 1);
  outcome_ptr const poll = bench.start_exchange (request);
  bench.advance (ms (100));
  EXPECT_EQ (poll->calls, 0);
  bench.control->fail_next_write (make_error_code (std::errc::io_error));
  bench.control->release_writes ();
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_FALSE (poll->error) << poll->error.message ();
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x03 }));
}

TEST (LumexDialogueTest,
      GivenALinkThatHangsUp_WhenAnExchangeIsInFlight_ThenItEndsWithLinkLost)
{
  bench_t bench;
  bench.start ();
  // Whole replies, then a hang-up instead of the second one.
  tr::testing::behavior_applier device (
      tr::testing::behavior::works_then_disconnects (1),
      bench.clock.source ());
  bench.on_request (
      [&bench, &device] (bytes_t const &request)
        {
          bytes_t wire;
          lumex::core::framing::slip::slip_codec const codec;
          bytes_t const reply = bytes ({ 0x0C, 0x04 });
          codec.encode (lumex::core::framing::codec::bytes_view_t (
                            reply.data (), reply.size ()),
                        wire);
          device.respond (*bench.control, request, wire);
        });
  outcome_ptr const first
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_FALSE (first->error);
  EXPECT_EQ (first->frame, bytes ({ 0x0C, 0x04 }));
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_EQ (second->error, protocol_errc::link_lost);
}

TEST (
    LumexDialogueTest,
    GivenARepeatingDevice_WhenTheReplyComesThreeTimes_ThenTheFirstAnswersAndTheCopiesAreCounted)
{
  bench_t bench;
  bench.start ();
  tr::testing::behavior_applier device (
      tr::testing::behavior::repeating (2, ms (5)), bench.clock.source ());
  bench.on_request (
      [&bench, &device] (bytes_t const &request)
        {
          bytes_t wire;
          lumex::core::framing::slip::slip_codec const codec;
          bytes_t const reply = bytes ({ 0x0C, 0x06 });
          codec.encode (lumex::core::framing::codec::bytes_view_t (
                            reply.data (), reply.size ()),
                        wire);
          device.respond (*bench.control, request, wire);
        });
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x06 }));
  bench.advance (ms (50));
  EXPECT_EQ (poll->calls, 1);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 2u);
  EXPECT_EQ (bench.the_dialogue->statistics ().replies, 1u);
}

TEST (
    LumexDialogueTest,
    GivenABoundedQueue_WhenMoreExchangesThanItHoldsWait_ThenTheNextOneEndsWithQueueFull)
{
  pr::dialogue_options_t options;
  options.queue_bound = 2;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  std::vector<outcome_ptr> outcomes;
  for (int i = 0; i < 4; ++i)
    outcomes.push_back (
        bench.start_exchange (dt::make_exchange<pump::POLL_t> ()));
  bench.settle ();
  EXPECT_EQ (outcomes[0]->calls, 0);
  EXPECT_EQ (outcomes[1]->calls, 0);
  EXPECT_EQ (outcomes[2]->calls, 0);
  ASSERT_EQ (outcomes[3]->calls, 1);
  EXPECT_EQ (outcomes[3]->error, protocol_errc::queue_full);
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_EQ (counters.queue_refusals, 1u);
  EXPECT_EQ (counters.max_queued, 2u);
  EXPECT_EQ (bench.requests ().size (), 1u);
  // The device answers each in turn; the queue drains in order.
  for (int i = 0; i < 3; ++i)
    {
      bench.reply (bytes ({ 0x0C, i }));
      bench.settle ();
    }
  ASSERT_TRUE (all_done (outcomes));
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ (outcomes[static_cast<std::size_t> (i)]->frame,
               bytes ({ 0x0C, i }));
}

TEST (LumexDialogueTest,
      GivenAQueueBoundOfZero_WhenTheFlightIsFull_ThenAnotherExchangeIsRefused)
{
  pr::dialogue_options_t options;
  options.queue_bound = 0;
  options.max_in_flight = 0;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const first
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  EXPECT_EQ (first->calls, 0);
  ASSERT_EQ (second->calls, 1);
  EXPECT_EQ (second->error, protocol_errc::queue_full);
}

TEST (
    LumexDialogueTest,
    GivenFramesThatAnswerNothing_WhenTheyCome_ThenEachKindIsCountedAndResetClearsThem)
{
  bench_t bench;
  bench.start ();
  // An unknown key, an event row, a row nobody asked for.
  bench.reply (bytes ({ 0x7E, 0x01 }));
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.settle ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x02 })); });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  outcome_ptr const poll = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_EQ (counters.unknown_frames, 1u);
  EXPECT_EQ (counters.events, 1u);
  EXPECT_EQ (counters.unexpected_replies, 1u);
  EXPECT_EQ (counters.requests, 1u);
  EXPECT_EQ (counters.replies, 1u);
  EXPECT_EQ (counters.naks, 0u);
  EXPECT_EQ (counters.stream_gaps, 0u);
  EXPECT_EQ (counters.rate_limited, 0u);
  EXPECT_EQ (counters.reconnects, 0u);
  bench.the_dialogue->reset_statistics ();
  pr::dialogue_statistics_t const cleared = bench.the_dialogue->statistics ();
  EXPECT_EQ (cleared.unknown_frames, 0u);
  EXPECT_EQ (cleared.events, 0u);
  EXPECT_EQ (cleared.unexpected_replies, 0u);
  EXPECT_EQ (cleared.requests, 0u);
  EXPECT_EQ (cleared.replies, 0u);
}

TEST (
    LumexDialogueTest,
    GivenEveryCounterOfTheEngine_WhenItsEventHappens_ThenEachMovedAndResetClearsAll)
{
  pr::dialogue_options_t options;
  options.queue_bound = 1;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          std::size_t const number = bench.requests ().size ();
          // The first request times out and its reply comes late, the second
          // is answered.
          bench.reply (bytes ({ 0x0C, static_cast<int> (number) }),
                       ms (number == 1 ? 150 : 10));
        });
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 1);
  outcome_ptr const first = bench.start_exchange (request);
  outcome_ptr const queued = bench.start_exchange (request);
  outcome_ptr const refused = bench.start_exchange (request);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  ASSERT_TRUE (bench.run_until ([&] () { return queued->calls != 0; }));
  bench.reply (bytes ({ 0x7E }));
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.reply (bytes ({ 0x0B }));
  bench.advance (ms (500));
  pr::exchange_t lonely = dt::make_exchange<pump::POLL_t> ();
  lonely.policy = dt::policy (10, 1);
  bench.on_request (std::function<void (bytes_t const &)> ());
  outcome_ptr const exhausted = bench.start_exchange (lonely);
  ASSERT_TRUE (bench.run_until ([&] () { return exhausted->calls != 0; }));
  EXPECT_EQ (refused->error, protocol_errc::queue_full);
  EXPECT_EQ (exhausted->error, protocol_errc::retries_exhausted);
  pr::dialogue_statistics_t const counters = bench.the_dialogue->statistics ();
  EXPECT_NE (counters.requests, 0u);
  EXPECT_NE (counters.replies, 0u);
  EXPECT_NE (counters.retries, 0u);
  EXPECT_NE (counters.timeouts, 0u);
  EXPECT_NE (counters.retries_exhausted, 0u);
  EXPECT_NE (counters.unexpected_replies, 0u);
  EXPECT_NE (counters.late_replies, 0u);
  EXPECT_NE (counters.unknown_frames, 0u);
  EXPECT_NE (counters.events, 0u);
  EXPECT_NE (counters.queue_refusals, 0u);
  EXPECT_NE (counters.max_queued, 0u);
  bench.the_dialogue->reset_statistics ();
  pr::dialogue_statistics_t const cleared = bench.the_dialogue->statistics ();
  EXPECT_EQ (cleared.requests, 0u);
  EXPECT_EQ (cleared.replies, 0u);
  EXPECT_EQ (cleared.retries, 0u);
  EXPECT_EQ (cleared.timeouts, 0u);
  EXPECT_EQ (cleared.retries_exhausted, 0u);
  EXPECT_EQ (cleared.unexpected_replies, 0u);
  EXPECT_EQ (cleared.late_replies, 0u);
  EXPECT_EQ (cleared.unknown_frames, 0u);
  EXPECT_EQ (cleared.events, 0u);
  EXPECT_EQ (cleared.queue_refusals, 0u);
  EXPECT_EQ (cleared.max_queued, 0u);
}

TEST (
    LumexDialogueTest,
    GivenARequestTheIndexCannotPlace_WhenStarted_ThenItEndsAtOnceWithoutAWrite)
{
  bench_t bench;
  bench.start ();
  // A row of another protocol.
  {
    lumex::core::protocol::index::index_result_t const &other
        = lumex::core::protocol::index::protocol_index<dt::tables::fixed_t> ();
    ASSERT_TRUE (other.has_value ());
    pr::exchange_t foreign;
    foreign.message = bytes ({ 0x20, 0x01, 0x02 });
    foreign.row = other->find_name ("SET_VALVE");
    outcome_ptr const not_ours = bench.start_exchange (foreign);
    // by_reply_key without its reply row, custom without a matcher, a
    // correlation outside the enumeration.
    pr::exchange_t no_reply_row = dt::make_exchange<pump::POLL_t> ();
    no_reply_row.how = dt::dialogue::correlation::by_reply_key;
    outcome_ptr const missing_row = bench.start_exchange (no_reply_row);
    pr::exchange_t no_matcher = dt::make_exchange<pump::POLL_t> ();
    no_matcher.how = dt::dialogue::correlation::custom;
    outcome_ptr const missing_matcher = bench.start_exchange (no_matcher);
    pr::exchange_t odd = dt::make_exchange<pump::POLL_t> ();
    odd.how = static_cast<dt::dialogue::correlation> (9);
    outcome_ptr const out_of_range = bench.start_exchange (odd);
    pr::exchange_t keyless;
    keyless.message = bytes ({ 0x0C });
    outcome_ptr const no_row = bench.start_exchange (keyless);
    bench.settle ();
    EXPECT_EQ (not_ours->error, protocol_errc::not_supported);
    EXPECT_EQ (missing_row->error, std::errc::invalid_argument);
    EXPECT_EQ (missing_matcher->error, std::errc::invalid_argument);
    EXPECT_EQ (out_of_range->error, std::errc::invalid_argument);
    EXPECT_EQ (no_row->error, std::errc::invalid_argument);
    EXPECT_TRUE (bench.requests ().empty ());
  }
}

TEST (
    LumexDialogueTest,
    GivenTheDeferredToken_WhenTheOperationIsLaunchedLater_ThenTheExchangeStartsThen)
{
  bench_t bench;
  bench.start ();
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x08 })); });
  auto deferred = bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::POLL_t> (), asio::deferred);
  bench.settle ();
  EXPECT_TRUE (bench.requests ().empty ());
  outcome_ptr const out = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { out, &bench.clock };
  std::move (deferred) (handler);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->frame, bytes ({ 0x0C, 0x08 }));
  EXPECT_EQ (bench.requests ().size (), 1u);
}

TEST (
    LumexDialogueTest,
    GivenAByOrderExchange_WhenAnEventComesFirst_ThenTheEventIsNoReplyAndTheNextFrameIs)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &)
        {
          bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
          bench.reply (bytes ({ 0x7F, 0x34, 0x12 }), ms (5));
        });
  outcome_ptr const read = bench.start_exchange (
      dt::make_exchange<pump::READ_t> (std::uint8_t (2)));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error);
  // A reply without a key of the protocol is the reply of a by_order row.
  EXPECT_EQ (read->frame, bytes ({ 0x7F, 0x34, 0x12 }));
  EXPECT_EQ (bench.the_dialogue->statistics ().events, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 0u);
}

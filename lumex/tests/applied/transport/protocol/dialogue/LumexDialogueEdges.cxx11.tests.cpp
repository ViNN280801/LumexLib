// Edges of the dialogue engine (lumex/applied/transport/protocol/dialogue):
// which exchange a frame goes to first, late replies against by_order
// exchanges and the window, the bounded late-reply ring, the queue after a
// cancellation, a lost link and a full flight, partial cancellations, the
// io_context shutdown with queued exchanges and a policy that owns the owner,
// a link of one's own that cancels its reads or fails a write, and the order
// of a completion against the next exchange.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
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
namespace fr = dt::fr;

bool
accept_everything (lumex::core::protocol::index::row_t const *,
                   lumex::core::protocol::layout::bytes_view_t,
                   lumex::core::protocol::layout::bytes_view_t)
{
  return true;
}

bytes_t const kinfo
    = bytes ({ 0x10, 'A', 'B', 'C', 0, 0, 0, 0, 0, 0x02, 0x01 });

struct owner_t
{
  std::unique_ptr<pr::dialogue_engine> engine;
};

// An engine over a manual link, on a context and a clock of the test.
struct manual_rig_t
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<dt::manual_link_t> link;
  std::unique_ptr<pr::dialogue_engine> engine;

  explicit manual_rig_t (pr::dialogue_options_t options
                         = pr::dialogue_options_t ())
      : link (std::make_shared<dt::manual_link_t> (context))
  {
    options.timers = clock.source ();
    engine.reset (new pr::dialogue_engine (pr::frame_link (link),
                                           dt::pump_index (), options));
    poll ();
  }

  ~manual_rig_t ()
  {
    engine.reset ();
    poll ();
  }

  void
  poll ()
  {
    context.poll ();
    context.restart ();
  }

  outcome_ptr
  start (pr::exchange_t request)
  {
    outcome_ptr const out = std::make_shared<dt::outcome_t> ();
    dt::record_t handler = { out, &clock };
    engine->async_exchange (request, handler);
    return out;
  }
};
} // namespace

TEST (
    LumexDialogueEdgesTest,
    GivenACustomExchangeAheadOfAByKeyOne_WhenTheByKeyReplyComes_ThenTheByKeyExchangeGetsIt)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 500;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x0C)
            bench.reply (bytes ({ 0x0C, 0x05 }), ms (10));
        });
  pr::exchange_t query = dt::make_exchange<pump::QUERY_t> ();
  query.matcher = &accept_everything;
  outcome_ptr const custom = bench.start_exchange (query);
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.advance (ms (100));
  ASSERT_EQ (poll->calls, 1);
  EXPECT_FALSE (poll->error);
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x05 }));
  EXPECT_EQ (custom->calls, 0);
  // A frame no exact match wants still goes to the custom exchange.
  bench.reply (bytes ({ 0x7E }));
  bench.settle ();
  ASSERT_EQ (custom->calls, 1);
  EXPECT_EQ (custom->frame, bytes ({ 0x7E }));
}

TEST (
    LumexDialogueEdgesTest,
    GivenTheLateReplyOfAByKeyRow_WhenAByOrderExchangeWaits_ThenItIsCountedLateAndTheByOrderOneGetsItsOwn)
{
  bench_t bench;
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x0C)
            bench.reply (bytes ({ 0x0C, 0x01 }), ms (150));
          else
            bench.reply (bytes ({ 0x09, 0x34, 0x12 }), ms (300));
        });
  pr::exchange_t poll = dt::make_exchange<pump::POLL_t> ();
  poll.policy = dt::policy (100, 0);
  outcome_ptr const first = bench.start_exchange (poll);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (first->error, protocol_errc::timeout);
  pr::exchange_t read = dt::make_exchange<pump::READ_t> (std::uint8_t (7));
  read.policy = dt::policy (1000, 0);
  outcome_ptr const second = bench.start_exchange (read);
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  EXPECT_FALSE (second->error);
  EXPECT_EQ (second->frame, bytes ({ 0x09, 0x34, 0x12 }));
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
}

TEST (
    LumexDialogueEdgesTest,
    GivenAQueuedByOrderExchangeAtTheHead_WhenItIsCancelled_ThenTheExchangesBehindItStart)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  asio::cancellation_signal on_read;
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const read = std::make_shared<dt::outcome_t> ();
  dt::record_t read_handler = { read, &bench.clock };
  bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::READ_t> (std::uint8_t (1)),
      asio::bind_cancellation_slot (on_read.slot (),
                                    std::move (read_handler)));
  outcome_ptr const info
      = bench.start_exchange (dt::make_exchange<pump::INFO_t> ());
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 1u);
  asio::post (bench.the_dialogue->get_strand (), [&on_read] ()
                { on_read.emit (asio::cancellation_type::terminal); });
  bench.settle ();
  EXPECT_EQ (read->error, transport_errc::cancelled);
  ASSERT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (dt::key_of (bench.requests ()[1].message), 0x10);
  EXPECT_EQ (poll->calls, 0);
  EXPECT_EQ (info->calls, 0);
}

TEST (
    LumexDialogueEdgesTest,
    GivenTheLateReplyWindowOfAByOrderRow_WhenAnotherExchangeIsStarted_ThenItWaitsForTheWindowToEnd)
{
  pr::dialogue_options_t options;
  options.late_reply_window_ms = 100;
  bench_t bench (options);
  bench.start ();
  tr::timing::time_point_t const start = bench.clock.now ();
  pr::exchange_t read = dt::make_exchange<pump::READ_t> (std::uint8_t (1));
  read.policy = dt::policy (50, 0);
  outcome_ptr const first = bench.start_exchange (read);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_EQ (first->error, protocol_errc::timeout);
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x01 })); });
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  EXPECT_EQ (bench.requests ().size (), 1u);
  // An event in the window is an event; a known frame is a late reply.
  bench.reply (bytes ({ 0x12, 0x01, 0x01, 0x10, 0x00 }));
  bench.reply (bytes ({ 0x0C, 0x09 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().events, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  ASSERT_EQ (bench.requests ().size (), 2u);
  EXPECT_EQ (std::chrono::duration_cast<ms> (bench.requests ()[1].at - start)
                 .count (),
             150);
  EXPECT_EQ (poll->frame, bytes ({ 0x0C, 0x01 }));
}

TEST (LumexDialogueEdgesTest,
      GivenQueuedExchanges_WhenTheLinkIsLost_ThenEachOfThemEndsToo)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const a
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const b
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const c
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  bench.control->disconnect ();
  bench.settle ();
  ASSERT_EQ (a->calls, 1);
  ASSERT_EQ (b->calls, 1);
  ASSERT_EQ (c->calls, 1);
  EXPECT_EQ (a->error, protocol_errc::link_lost);
  EXPECT_TRUE (b->error);
  EXPECT_TRUE (c->error);
}

TEST (
    LumexDialogueEdgesTest,
    GivenAPartialCancellation_WhenItIsEmittedOnAWrittenExchange_ThenThatExchangeEnds)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  asio::cancellation_signal signal;
  outcome_ptr const poll = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { poll, &bench.clock };
  bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::POLL_t> (),
      asio::bind_cancellation_slot (signal.slot (), std::move (handler)));
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 1u);
  asio::post (bench.the_dialogue->get_strand (), [&signal] ()
                { signal.emit (asio::cancellation_type::partial); });
  bench.settle ();
  ASSERT_EQ (poll->calls, 1);
  EXPECT_EQ (poll->error, transport_errc::cancelled);
}

TEST (
    LumexDialogueEdgesTest,
    GivenAQueuedExchangeWhoseHandlerOwnsTheEngine_WhenTheIoContextIsDestroyed_ThenTheEngineIsFreed)
{
  std::weak_ptr<owner_t> watch;
  {
    manual_rig_t rig;
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine = std::move (rig.engine);
    watch = owner;
    // One in flight, one queued that owns the owner.
    owner->engine->async_exchange (dt::make_exchange<pump::POLL_t> (),
                                   [] (std::error_code, pr::reply_frame_t) {});
    owner->engine->async_exchange (
        dt::make_exchange<pump::POLL_t> (),
        [owner] (std::error_code, pr::reply_frame_t) {});
    rig.poll ();
    owner.reset ();
    EXPECT_FALSE (watch.expired ());
  }
  EXPECT_TRUE (watch.expired ());
}

TEST (
    LumexDialogueEdgesTest,
    GivenPoliciesThatOwnTheOwner_WhenTheIoContextIsDestroyed_ThenTheEngineIsFreed)
{
  std::weak_ptr<owner_t> watch_defaults;
  std::weak_ptr<owner_t> watch_policy;
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    std::shared_ptr<owner_t> other = std::make_shared<owner_t> ();
    pr::dialogue_options_t options;
    options.timers = clock.source ();
    options.defaults.extra_retries
        = [owner] (lumex::core::protocol::index::session_context_t const &,
                   std::error_code const &) -> std::uint32_t { return 0; };
    owner->engine.reset (new pr::dialogue_engine (
        pr::frame_link (std::make_shared<dt::manual_link_t> (context)),
        dt::pump_index (), options));
    other->engine.reset (new pr::dialogue_engine (
        pr::frame_link (std::make_shared<dt::manual_link_t> (context)),
        dt::pump_index (), options));
    watch_defaults = owner;
    watch_policy = other;
    owner->engine->async_exchange (dt::make_exchange<pump::POLL_t> (),
                                   [] (std::error_code, pr::reply_frame_t) {});
    // The policy of one exchange owns the other owner.
    pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
    request.policy = dt::policy (
        1000, 0,
        [other] (lumex::core::protocol::index::session_context_t const &,
                 std::error_code const &) -> std::uint32_t { return 0; });
    other->engine->async_exchange (request,
                                   [] (std::error_code, pr::reply_frame_t) {});
    context.poll ();
    owner.reset ();
    other.reset ();
    EXPECT_FALSE (watch_defaults.expired ());
    EXPECT_FALSE (watch_policy.expired ());
  }
  EXPECT_TRUE (watch_defaults.expired ());
  EXPECT_TRUE (watch_policy.expired ());
}

TEST (
    LumexDialogueEdgesTest,
    GivenMoreAbandonedRowsThanTheRingHolds_WhenTheOldestLateReplyComes_ThenItIsNoLongerLate)
{
  bench_t bench;
  bench.start ();
  pr::exchange_t info = dt::make_exchange<pump::INFO_t> ();
  info.policy = dt::policy (10, 0);
  outcome_ptr const first = bench.start_exchange (info);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  pr::exchange_t poll = dt::make_exchange<pump::POLL_t> ();
  poll.policy = dt::policy (10, 0);
  for (int i = 0; i < 32; ++i)
    {
      outcome_ptr const out = bench.start_exchange (poll);
      ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
    }
  // The note of INFO was the oldest of 33: it is gone.
  bench.reply (kinfo);
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 0u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 1u);
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
}

TEST (LumexDialogueEdgesTest,
      GivenOneAbandonedAttempt_WhenItsRowAnswersTwice_ThenOnlyTheFirstIsLate)
{
  bench_t bench;
  bench.start ();
  pr::exchange_t poll = dt::make_exchange<pump::POLL_t> ();
  poll.policy = dt::policy (100, 0);
  outcome_ptr const first = bench.start_exchange (poll);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 1u);
}

TEST (LumexDialogueEdgesTest,
      GivenAByReplyKeyRowThatTimedOut_WhenTheReplyRowComesLate_ThenItIsLate)
{
  bench_t bench;
  bench.start ();
  pr::exchange_t write
      = dt::make_exchange<pump::WRITE_t> (std::uint8_t (1), std::uint16_t (2));
  write.policy = dt::policy (100, 0);
  outcome_ptr const out = bench.start_exchange (write);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (out->error, protocol_errc::timeout);
  bench.reply (bytes ({ 0x30, 0x01, 0x02, 0x00 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 0u);
}

TEST (
    LumexDialogueEdgesTest,
    GivenSeveralExchangesBehindAByOrderOne_WhenItEnds_ThenEveryOneTheFlightTakesStarts)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 3;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const read = bench.start_exchange (
      dt::make_exchange<pump::READ_t> (std::uint8_t (1)));
  for (int i = 0; i < 3; ++i)
    bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 1u);
  bench.reply (bytes ({ 0x09, 0x01, 0x00 }));
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (bench.requests ().size (), 4u);
}

TEST (
    LumexDialogueEdgesTest,
    GivenALinkOfOnesOwnThatCancelsItsReadsWhileOpen_WhenAReplyComesAfter_ThenTheReadingWentOn)
{
  manual_rig_t rig;
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (0, 0);
  outcome_ptr const out = rig.start (request);
  rig.poll ();
  ASSERT_EQ (rig.link->pending_reads (), 1u);
  rig.link->cancel_reads ();
  rig.poll ();
  EXPECT_EQ (out->calls, 0);
  ASSERT_EQ (rig.link->pending_reads (), 1u);
  ASSERT_TRUE (rig.link->deliver (bytes ({ 0x0C, 0x03 })));
  rig.poll ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (
    LumexDialogueEdgesTest,
    GivenALinkWhoseWriteThrows_WhenAnExchangeStarts_ThenItEndsWithTheErrorAndTheNextOneRuns)
{
  manual_rig_t rig;
  rig.link->throw_on_write (true);
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (100, 0);
  outcome_ptr const failed = rig.start (request);
  outcome_ptr const queued = rig.start (request);
  rig.poll ();
  ASSERT_EQ (failed->calls, 1);
  EXPECT_EQ (failed->error, std::errc::not_enough_memory);
  ASSERT_EQ (queued->calls, 1);
  EXPECT_EQ (queued->error, std::errc::not_enough_memory);
  rig.link->throw_on_write (false);
  outcome_ptr const next = rig.start (request);
  rig.poll ();
  ASSERT_EQ (rig.link->written ().size (), 1u);
  ASSERT_TRUE (rig.link->deliver (bytes ({ 0x0C, 0x04 })));
  rig.poll ();
  ASSERT_EQ (next->calls, 1);
  EXPECT_FALSE (next->error);
  EXPECT_EQ (rig.clock.pending (), 0u);
}

TEST (
    LumexDialogueEdgesTest,
    GivenAQueuedExchange_WhenTheFirstEnds_ThenItsHandlerRunsAfterTheEngineStartedTheNext)
{
  bench_t bench;
  bench.start ();
  std::uint64_t requests_seen = 0;
  pr::dialogue_engine *const engine = bench.the_dialogue.get ();
  engine->async_exchange (
      dt::make_exchange<pump::POLL_t> (),
      [&requests_seen, engine] (std::error_code, pr::reply_frame_t)
        { requests_seen = engine->statistics ().requests; });
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.settle ();
  // Posted, not run inside the engine: the next exchange was written first.
  EXPECT_EQ (requests_seen, 2u);
  EXPECT_EQ (second->calls, 0);
}

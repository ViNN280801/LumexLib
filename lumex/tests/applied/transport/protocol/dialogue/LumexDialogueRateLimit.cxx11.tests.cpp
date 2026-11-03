// The rate limits of the dialogue engine: of one row and of the protocol, who
// waits and who goes past, the attempts after a NAK, the counter and the
// warning, a cancellation and a close while a request waits, a timer that
// cannot be armed.

#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace tr = ft::tr;
using ft::bytes;
using ft::log_capture_t;
using ft::log_level;
using ft::manual_bench_t;
using ft::ms;
using ft::outcome_ptr;
using ft::protocol_errc;
using ft::rated_t;
using ft::transport_errc;
using ft::wide_t;

// Two in flight; rate limits of the rows only.
template <typename P> struct wide_bench_t : manual_bench_t<P>
{
  wide_bench_t () { this->options.max_in_flight = 4; }
};

TEST (LumexDialogueRateLimitTest, RateLimit_SecondRequestWaits)
{
  log_capture_t const log;
  wide_bench_t<wide_t> bench;
  bench.start ();
  outcome_ptr const first
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x34), 1u);
  bench.deliver (bytes ({ 0x34, 0x01 }));
  EXPECT_EQ (first->calls, 1);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 0u);
  outcome_ptr const second
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x34), 1u);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "SLOW"), 1u);
  bench.advance (ms (49));
  EXPECT_EQ (bench.count (0x34), 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.count (0x34), 2u);
  bench.deliver (bytes ({ 0x34, 0x02 }));
  EXPECT_EQ (second->calls, 1);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 1u);
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_WarnsAtMostOnceASecondOfTheEngineWithTheNumberSkipped)
{
  log_capture_t const log;
  wide_bench_t<wide_t> bench;
  // Nothing answers: the requests stay in flight, without a timeout.
  bench.options.max_in_flight = 64;
  bench.options.defaults.timeout_ms = 0;
  bench.start ();
  std::string const delays = "a rate limit delays";
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  // Three of the row and one of another: one warning, three at debug, also
  // for the other row (the limit of the warning is the engine's).
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 4u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, delays), 1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "more since"), 0u);
  EXPECT_EQ (log_capture_t::count (log_level::debug, delays), 3u);
  // Just before the second is over nothing is warned about.
  bench.advance (ms (999));
  EXPECT_EQ (bench.count (0x34), 4u);
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 5u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, delays), 1u);
  EXPECT_EQ (log_capture_t::count (log_level::debug, delays), 4u);
  // A second after the first: a warning with the four that went to debug.
  bench.advance (ms (1));
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 6u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, delays), 2u);
  EXPECT_EQ (log_capture_t::count (log_level::warning,
                                   "(4 more since the last warning, logged at "
                                   "debug)"),
             1u);
  // The second counts from this warning: one right after it goes to debug.
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 7u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, delays), 2u);
  EXPECT_EQ (log_capture_t::count (log_level::debug, delays), 5u);
  // The count starts over: the one that went to debug, and no other.
  bench.advance (ms (1000));
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::warning, delays), 3u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "more since"), 2u);
  EXPECT_EQ (log_capture_t::count (log_level::warning,
                                   "(1 more since the last warning, logged at "
                                   "debug)"),
             1u);
}

TEST (LumexDialogueRateLimitTest, RateLimit_AfterTheIntervalNothingWaits)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  bench.deliver (bytes ({ 0x34, 0x01 }));
  bench.advance (ms (50));
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x34), 2u);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 0u);
}

TEST (LumexDialogueRateLimitTest, RateLimit_OfARowDoesNotHoldAnotherRow)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  bench.deliver (bytes ({ 0x34, 0x01 }));
  // The second of the row waits; the query behind it goes past.
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  outcome_ptr const query
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x34, 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (query->calls, 1);
  bench.advance (ms (50));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x34, 0x33, 0x34 }));
}

TEST (LumexDialogueRateLimitTest, RateLimit_OneRowStaysInOrder)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  outcome_ptr const out[] = {
    bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ()),
    bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ()),
    bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ())
  };
  bench.settle ();
  EXPECT_EQ (bench.count (0x34), 1u);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x34), 2u);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x34), 3u);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x34), 3u);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 2u);
  bench.deliver (bytes ({ 0x34, 0x07 }));
  // by_key: the first exchange of the row takes the reply.
  EXPECT_EQ (out[0]->calls, 1);
  EXPECT_EQ (out[1]->calls, 0);
}

TEST (LumexDialogueRateLimitTest, RateLimit_OfTheProtocolHoldsEveryRow)
{
  manual_bench_t<rated_t> bench;
  bench.start ();
  outcome_ptr const first
      = bench.start_exchange (ft::make_exchange<rated_t, rated_t::QUERY_t> ());
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (first->calls, 1);
  bench.start_exchange (ft::make_exchange<rated_t, rated_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 1u);
  bench.advance (ms (40));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33, 0x31 }));
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_ByOrderHeadThatMayGoStillHoldsTheOthers)
{
  // The first in the queue is no longer limited but needs an empty flight:
  // what stands behind it must wait, or it would never get one.
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUERY_t> ());
  outcome_ptr const ordered
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::ORDERED_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33, 0x37 }));
  bench.deliver (bytes ({ 0x37, 0x01 }));
  EXPECT_EQ (ordered->calls, 1);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33, 0x37, 0x31 }));
}

TEST (LumexDialogueRateLimitTest, RateLimit_ARepeatAfterANakWaitsToo)
{
  manual_bench_t<wide_t> bench;
  bench.start ();
  pr::exchange_t request = ft::make_exchange<wide_t, wide_t::RETRIED_t> ();
  request.policy = lumex_dialogue_test::policy (1000, 1);
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  EXPECT_EQ (bench.count (0x36), 1u);
  // The NAK: the second part is two bytes long.
  bench.deliver (bytes ({ 0x36, 0xFB, 0x00 }));
  EXPECT_EQ (bench.engine->statistics ().naks, 1u);
  bench.settle ();
  EXPECT_EQ (bench.count (0x36), 1u);
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().rate_limited, 1u);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x36), 2u);
  bench.deliver (bytes ({ 0x36, 0x05 }));
  EXPECT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueRateLimitTest, RateLimit_TwoRepeatsWithDifferentTimesBothGo)
{
  // Two exchanges wait for their limits in flight: the end of the nearer wait
  // must arm the one for the farther.
  wide_bench_t<wide_t> bench;
  bench.start ();
  pr::exchange_t slow = ft::make_exchange<wide_t, wide_t::RETRIED_t> ();
  slow.policy = lumex_dialogue_test::policy (1000, 1);
  pr::exchange_t quick = ft::make_exchange<wide_t, wide_t::RETRIED_QUICK_t> ();
  quick.policy = lumex_dialogue_test::policy (1000, 1);
  bench.start_exchange (std::move (slow));
  bench.start_exchange (std::move (quick));
  bench.settle ();
  bench.deliver (bytes ({ 0x36, 0xFB, 0x00 }));
  bench.deliver (bytes ({ 0x3E, 0xFB, 0x00 }));
  EXPECT_EQ (bench.count (0x36), 1u);
  EXPECT_EQ (bench.count (0x3E), 1u);
  bench.advance (ms (10));
  EXPECT_EQ (bench.count (0x3E), 2u);
  EXPECT_EQ (bench.count (0x36), 1u);
  bench.advance (ms (40));
  EXPECT_EQ (bench.count (0x36), 2u);
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_AWaitingRequestEndsWithCancelAndClose)
{
  {
    wide_bench_t<wide_t> bench;
    bench.start ();
    bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
    outcome_ptr const second
        = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
    bench.settle ();
    bench.engine->cancel ();
    bench.settle ();
    EXPECT_EQ (second->calls, 1);
    EXPECT_EQ (second->error, transport_errc::cancelled);
    bench.advance (ms (1000));
    EXPECT_EQ (bench.count (0x34), 1u);
  }
  {
    wide_bench_t<wide_t> bench;
    bench.start ();
    bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
    outcome_ptr const second
        = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
    bench.settle ();
    bench.engine->close ();
    bench.settle ();
    EXPECT_EQ (second->calls, 1);
    EXPECT_EQ (second->error, transport_errc::cancelled);
    bench.advance (ms (1000));
    EXPECT_EQ (bench.count (0x34), 1u);
  }
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_ACancelledWaitLeavesTheOthersOnTime)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  asio::cancellation_signal signal;
  outcome_ptr const second = std::make_shared<ft::outcome_t> ();
  ft::record_t handler = { second, &bench.clock };
  bench.engine->async_exchange (
      ft::make_exchange<wide_t, wide_t::SLOW_t> (),
      asio::bind_cancellation_slot (signal.slot (), handler));
  outcome_ptr const third
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  EXPECT_EQ (second->error, transport_errc::cancelled);
  EXPECT_EQ (bench.count (0x34), 1u);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x34), 2u);
  EXPECT_EQ (third->calls, 0);
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_ADueWaitRacingANewRequestStartsEachOnce)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x34), 1u);
  // The wait is due and its completion posted; a request of another row and
  // one more of the row come before it runs.
  bench.clock.advance (ms (50));
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUERY_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x33), 1u);
  EXPECT_EQ (bench.count (0x34), 2u);
  bench.advance (ms (49));
  EXPECT_EQ (bench.count (0x34), 2u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.count (0x34), 3u);
}

TEST (LumexDialogueRateLimitTest, RateLimit_ANearerDeadlineRearmsTheWait)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  // The wait is armed for the row of 50 ms; the row of 10 ms must not wait
  // for it.
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUICK_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x3A), 1u);
  bench.advance (ms (10));
  EXPECT_EQ (bench.count (0x3A), 2u);
  EXPECT_EQ (bench.count (0x34), 1u);
  bench.advance (ms (40));
  EXPECT_EQ (bench.count (0x34), 2u);
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_ByOrderWaitingForItsLimitLetsOthersPastThenHoldsThem)
{
  wide_bench_t<wide_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::ORDERED_SLOW_t> ());
  bench.settle ();
  bench.deliver (bytes ({ 0x3B, 0x01 }));
  // The second by_order waits for its limit: the query goes past it.
  outcome_ptr const ordered = bench.start_exchange (
      ft::make_exchange<wide_t, wide_t::ORDERED_SLOW_t> ());
  outcome_ptr const query
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3B, 0x33 }));
  // Its limit passed, the query is still in flight: it needs an empty flight,
  // so a request behind it waits.
  bench.advance (ms (50));
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3B, 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (query->calls, 1);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3B, 0x33, 0x3B }));
  bench.deliver (bytes ({ 0x3B, 0x01 }));
  EXPECT_EQ (ordered->calls, 1);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3B, 0x33, 0x3B, 0x31 }));
}

TEST (LumexDialogueRateLimitTest,
      RateLimit_ATimerThatCannotBeArmedEndsTheWaitingRequest)
{
  wide_bench_t<wide_t> bench;
  std::shared_ptr<ft::flaky_timers_t> const timers
      = std::make_shared<ft::flaky_timers_t> (bench.clock.source ());
  bench.options.timers = tr::timing::timer_source (timers);
  bench.start ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  timers->set_throwing (true);
  outcome_ptr const second
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::SLOW_t> ());
  bench.settle ();
  timers->set_throwing (false);
  ASSERT_EQ (second->calls, 1);
  EXPECT_EQ (second->error,
             std::make_error_code (std::errc::not_enough_memory));
  // The engine goes on.
  bench.advance (ms (50));
  outcome_ptr const third
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x33), 1u);
  EXPECT_EQ (third->calls, 0);
}
} // namespace

// The keepalive of the dialogue engine: its period, the arguments of each
// request, a request still pending, a generator that fails, throws or destroys
// the engine, and what it may own.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace tr = ft::tr;
namespace dialogue = ft::dialogue;
namespace layout = ft::layout;
using ft::beat_t;
using ft::bytes;
using ft::bytes_t;
using ft::log_capture_t;
using ft::log_level;
using ft::manual_bench_t;
using ft::ms;
using ft::quiet_t;
using context_t = lumex::core::protocol::field::decode_context_t;

using ft::answered_t;
using ft::clocked_t;

TEST (LumexDialogueKeepaliveTest, Keepalive_SentEveryPeriodWithFreshArgs)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  std::uint32_t seconds = 100;
  EXPECT_TRUE (
      bench.engine->set_keepalive (dialogue::keepalive_from<clocked_t> (
          [&seconds] () { return std::make_tuple (seconds++); })));
  bench.settle ();
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.advance (ms (999));
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.advance (ms (1));
  ASSERT_EQ (bench.link->written ().size (), 1u);
  bench.advance (ms (2000));
  std::vector<bytes_t> const written = bench.link->written ();
  ASSERT_EQ (written.size (), 3u);
  EXPECT_EQ (written[0], bytes ({ 0x11, 100, 0, 0, 0 }));
  EXPECT_EQ (written[1], bytes ({ 0x11, 101, 0, 0, 0 }));
  EXPECT_EQ (written[2], bytes ({ 0x11, 102, 0, 0, 0 }));
  EXPECT_EQ (bench.engine->statistics ().keepalives, 3u);
  EXPECT_EQ (bench.engine->statistics ().requests, 3u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_WithAFieldWaitsForItsGenerator)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  bench.advance (ms (5000));
  EXPECT_TRUE (bench.link->written ().empty ());
  EXPECT_EQ (bench.engine->statistics ().keepalives, 0u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_WithoutFieldsNeedsNoGenerator)
{
  manual_bench_t<beat_t> bench;
  bench.start ();
  bench.advance (ms (199));
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.advance (ms (1));
  bench.settle ();
  EXPECT_EQ (bench.count (0x39), 1u);
  bench.advance (ms (400));
  EXPECT_EQ (bench.count (0x39), 3u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_ATableWithoutOneRefusesAGenerator)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_FALSE (bench.engine->set_keepalive (ft::tank_keepalive_none ()));
  bench.advance (ms (5000));
  EXPECT_TRUE (bench.link->written ().empty ());
}

TEST (LumexDialogueKeepaliveTest, Keepalive_AnEmptyGeneratorEndsIt)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  bench.engine->set_keepalive (dialogue::keepalive_from<clocked_t> (
      [] () { return std::make_tuple (std::uint32_t (1)); }));
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
  bench.engine->set_keepalive (pr::keepalive_generator_t ());
  bench.settle ();
  bench.advance (ms (5000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
  // And one set again goes on.
  bench.engine->set_keepalive (dialogue::keepalive_from<clocked_t> (
      [] () { return std::make_tuple (std::uint32_t (2)); }));
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 2u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_AGeneratorSetTwiceRunsOnePeriod)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  for (int i = 0; i < 3; ++i)
    {
      bench.engine->set_keepalive (dialogue::keepalive_from<clocked_t> (
          [] () { return std::make_tuple (std::uint32_t (1)); }));
      bench.settle ();
    }
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_ADueWaitRacingASetSendsOnceAndKeepsOnePeriod)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  pr::keepalive_generator_t const generator
      = dialogue::keepalive_from<clocked_t> (
          [] () { return std::make_tuple (std::uint32_t (1)); });
  bench.engine->set_keepalive (generator);
  bench.settle ();
  // The wait is due and posted; a set comes before it runs.
  bench.clock.advance (ms (1000));
  bench.engine->set_keepalive (generator);
  bench.settle ();
  EXPECT_EQ (bench.link->written ().size (), 1u);
  bench.advance (ms (999));
  EXPECT_EQ (bench.link->written ().size (), 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.link->written ().size (), 2u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_SkippedWhileThePreviousOneIsPending)
{
  manual_bench_t<answered_t> bench;
  bench.options.defaults.timeout_ms = 1000;
  bench.start ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x33), 1u);
  // No reply: the next periods send nothing.
  bench.advance (ms (500));
  EXPECT_EQ (bench.count (0x33), 1u);
  EXPECT_EQ (bench.engine->statistics ().keepalives, 1u);
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (bench.engine->statistics ().replies, 1u);
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x33), 2u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_TakesTheTimeAndTheRetriesOfItsRowAndGoesOnAfterATimeout)
{
  manual_bench_t<answered_t> bench;
  bench.options.defaults.timeout_ms = 150;
  bench.start ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x33), 1u);
  bench.advance (ms (150));
  EXPECT_EQ (bench.engine->statistics ().timeouts, 1u);
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x33), 2u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_GoesThroughTheQueueBehindARequest)
{
  manual_bench_t<answered_t> bench;
  bench.options.defaults.timeout_ms = 500;
  bench.start ();
  ft::outcome_ptr const first = bench.start_exchange (
      ft::make_exchange<answered_t, answered_t::FILL_t> (1));
  bench.settle ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31 }));
  bench.deliver (bytes ({ 0x31, 0x00 }));
  EXPECT_EQ (first->calls, 1);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31, 0x33 }));
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_AGeneratorErrorSkipsThatOneAndTheNextGoes)
{
  log_capture_t const log;
  manual_bench_t<clocked_t> bench;
  bench.start ();
  int calls = 0;
  bench.engine->set_keepalive (
      [&calls] (layout::bytes_t &out, context_t const &)
        {
          ++calls;
          if (calls == 1)
            return std::make_error_code (std::errc::invalid_argument);
          out = bytes ({ 0x11, 9, 0, 0, 0 });
          return std::error_code ();
        });
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_TRUE (bench.link->written ().empty ());
  EXPECT_EQ (bench.engine->statistics ().keepalives, 0u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "keepalive"), 1u);
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_AGeneratorThatThrowsLeavesTheRunAndThePeriodGoesOn)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  int calls = 0;
  bench.engine->set_keepalive (
      [&calls] (layout::bytes_t &out, context_t const &)
        {
          ++calls;
          if (calls == 1)
            throw std::runtime_error ("generator");
          out = bytes ({ 0x11, 9, 0, 0, 0 });
          return std::error_code ();
        });
  bench.settle ();
  bench.clock.advance (ms (1000));
  EXPECT_THROW (bench.context.poll (), std::runtime_error);
  bench.settle ();
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_AGeneratorMayDestroyTheEngine)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  bench.engine->set_keepalive (
      [&bench] (layout::bytes_t &out, context_t const &)
        {
          bench.engine.reset ();
          out = bytes ({ 0x11, 9, 0, 0, 0 });
          return std::error_code ();
        });
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.engine, nullptr);
  bench.settle ();
  bench.advance (ms (5000));
  EXPECT_LE (bench.link->written ().size (), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_AClosedEngineKeepsNoHandlerOfAFeature)
{
  manual_bench_t<clocked_t> bench;
  bench.start ();
  bench.engine->close ();
  bench.settle ();
  std::shared_ptr<int> const owned = std::make_shared<int> (0);
  EXPECT_FALSE (bench.engine->set_keepalive (
      [owned] (layout::bytes_t &, context_t const &)
        { return std::error_code (); }));
  EXPECT_EQ (bench.engine
                 ->on_edge (ft::row_of<clocked_t, clocked_t::STATUS_t> (),
                            [owned] (pr::edge_t const &) {})
                 .id,
             0u);
  bench.engine->on_link_state (
      [owned] (pr::link_state, std::error_code const &) {});
  EXPECT_EQ (owned.use_count (), 1);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_StopsWithClose)
{
  manual_bench_t<beat_t> bench;
  bench.start ();
  bench.advance (ms (200));
  EXPECT_EQ (bench.count (0x39), 1u);
  bench.engine->close ();
  bench.settle ();
  bench.advance (ms (5000));
  EXPECT_EQ (bench.count (0x39), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_GoesOnAfterCancelButNotOnALinkThatIsClosed)
{
  manual_bench_t<beat_t> bench;
  bench.start ();
  bench.advance (ms (200));
  bench.engine->cancel ();
  bench.settle ();
  bench.advance (ms (200));
  EXPECT_EQ (bench.count (0x39), 2u);
  // The transport was closed from outside: the reads fail, the link is lost,
  // and a keepalive would only fail its write.
  bench.link->close ();
  bench.settle ();
  EXPECT_FALSE (bench.engine->link_is_up ());
  bench.advance (ms (1000));
  EXPECT_EQ (bench.count (0x39), 2u);
}

TEST (LumexDialogueKeepaliveTest, Keepalive_GoesOnOnALostLinkThatIsOpen)
{
  // A device that talks only when spoken to comes back through the keepalive.
  manual_bench_t<beat_t> bench;
  bench.options.read_restart_ms = 0;
  bench.start ();
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  EXPECT_FALSE (bench.engine->link_is_up ());
  bench.advance (ms (200));
  EXPECT_EQ (bench.count (0x39), 1u);
  EXPECT_EQ (bench.link->pending_reads (), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_ATimerThatCannotBeArmedIsLoggedAndASetArmsItAgain)
{
  manual_bench_t<clocked_t> bench;
  std::shared_ptr<ft::flaky_timers_t> const timers
      = std::make_shared<ft::flaky_timers_t> (bench.clock.source ());
  bench.options.timers = tr::timing::timer_source (timers);
  bench.start ();
  timers->set_throwing (true);
  pr::keepalive_generator_t const generator
      = dialogue::keepalive_from<clocked_t> (
          [] () { return std::make_tuple (std::uint32_t (1)); });
  bench.engine->set_keepalive (generator);
  bench.settle ();
  timers->set_throwing (false);
  bench.advance (ms (5000));
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.engine->set_keepalive (generator);
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->written ().size (), 1u);
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_AGeneratorThatOwnsTheEngineDiesWithTheIoContext)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watch;
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    std::shared_ptr<ft::manual_link_t> const link
        = std::make_shared<ft::manual_link_t> (context);
    pr::dialogue_options_t options = dialogue::options_for<clocked_t> ();
    options.timers = clock.source ();
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine.reset (new pr::dialogue_engine (
        pr::frame_link (link), *ft::index::protocol_index<clocked_t> (),
        options));
    owner->engine->set_keepalive (
        [owner] (layout::bytes_t &, context_t const &)
          { return std::error_code (); });
    watch = owner;
    context.poll ();
    owner.reset ();
    EXPECT_FALSE (watch.expired ());
  }
  EXPECT_TRUE (watch.expired ());
}

TEST (LumexDialogueKeepaliveTest,
      Keepalive_AGeneratorThatOwnsTheEngineDiesWithClose)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watch;
  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<ft::manual_link_t> const link
      = std::make_shared<ft::manual_link_t> (context);
  pr::dialogue_options_t options = dialogue::options_for<clocked_t> ();
  options.timers = clock.source ();
  std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
  owner->engine.reset (new pr::dialogue_engine (
      pr::frame_link (link), *ft::index::protocol_index<clocked_t> (),
      options));
  owner->engine->set_keepalive ([owner] (layout::bytes_t &, context_t const &)
                                  { return std::error_code (); });
  watch = owner;
  pr::dialogue_engine *const engine = owner->engine.get ();
  owner.reset ();
  engine->close ();
  context.poll ();
  EXPECT_TRUE (watch.expired ());
  context.restart ();
  context.poll ();
}
} // namespace

// The edge events of the dialogue engine and the last known state read from a
// row: when an edge fires, who hears it, a handler that throws or destroys the
// engine, a frame that does not decode, the end of the link.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace tr = ft::tr;
namespace dialogue = ft::dialogue;
using ft::bytes;
using ft::bytes_t;
using ft::manual_bench_t;
using ft::ms;
using ft::outcome_ptr;
using ft::quiet_t;
using ft::status_frame;
using ft::tank_state;

using seen_edges_t = std::vector<pr::edge_t>;

pr::subscription_t
watch (manual_bench_t<quiet_t> &bench, seen_edges_t &seen)
{
  return bench.engine->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                                [&seen] (pr::edge_t const &edge)
                                  { seen.push_back (edge); });
}

TEST (LumexDialogueEdgesTest, EdgeEvent_FiresOnZeroToOneOnly)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  EXPECT_NE (watch (bench, seen).id, 0u);
  bench.deliver (status_frame (0));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (0));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (1));
  ASSERT_EQ (seen.size (), 1u);
  EXPECT_EQ (seen[0].field, 0u);
  EXPECT_EQ (seen[0].from, 0);
  EXPECT_EQ (seen[0].to, 1);
  EXPECT_STREQ (seen[0].row->entry->identifier, "STATUS");
  // Staying at the level fires nothing.
  bench.deliver (status_frame (1));
  bench.deliver (status_frame (1));
  EXPECT_EQ (seen.size (), 1u);
  EXPECT_EQ (bench.engine->statistics ().edge_events, 1u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_TheFirstFrameIsNoEdge)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (1));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (0));
  ASSERT_EQ (seen.size (), 1u);
  EXPECT_EQ (seen[0].from, 1);
  EXPECT_EQ (seen[0].to, 0);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_ATransitionThatIsNotNamedFiresNothing)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (2));
  bench.deliver (status_frame (1));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (3));
  EXPECT_TRUE (seen.empty ());
  EXPECT_EQ (bench.engine->statistics ().edge_events, 0u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_AReplyOfTheRowCountsAsAFrameOfIt)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (0));
  outcome_ptr const out = bench.start_exchange (
      ft::make_exchange<quiet_t, quiet_t::STATUS_t> ());
  bench.settle ();
  bench.deliver (status_frame (1));
  EXPECT_EQ (out->calls, 1);
  EXPECT_EQ (seen.size (), 1u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_AFrameThatDoesNotDecodeLeavesTheLevel)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (0));
  // Too short for the layout of the row.
  bench.deliver (bytes ({ 0x12, 0x01 }));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (1));
  EXPECT_EQ (seen.size (), 1u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_TheLevelIsForgottenWhenTheLinkIsLost)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (0));
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  bench.advance (ms (100));
  bench.deliver (status_frame (1));
  EXPECT_TRUE (seen.empty ());
  bench.deliver (status_frame (0));
  EXPECT_EQ (seen.size (), 1u);
}

TEST (LumexDialogueEdgesTest,
      EdgeEvent_UnsubscribedHearsNothingAndTheCountGoesOn)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  pr::subscription_t const made = watch (bench, seen);
  EXPECT_TRUE (bench.engine->unsubscribe (made));
  EXPECT_FALSE (bench.engine->unsubscribe (made));
  bench.deliver (status_frame (0));
  bench.deliver (status_frame (1));
  EXPECT_TRUE (seen.empty ());
  EXPECT_EQ (bench.engine->statistics ().edge_events, 1u);
}

TEST (LumexDialogueEdgesTest,
      EdgeEvent_ARowOfAnotherTableOrAnEmptyHandlerIsRefused)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_EQ (bench.engine
                 ->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                            pr::edge_handler_t ())
                 .id,
             0u);
  EXPECT_EQ (bench.engine->on_edge (nullptr, [] (pr::edge_t const &) {}).id,
             0u);
  // A row without an edge has nothing to hear.
  EXPECT_NE (bench.engine
                 ->on_edge (ft::row_of<quiet_t, quiet_t::QUERY_t> (),
                            [] (pr::edge_t const &) {})
                 .id,
             0u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_WithoutFeaturesNothingFires)
{
  manual_bench_t<quiet_t> bench;
  bench.options.features = nullptr;
  bench.start ();
  seen_edges_t seen;
  watch (bench, seen);
  bench.deliver (status_frame (0));
  bench.deliver (status_frame (1));
  EXPECT_TRUE (seen.empty ());
}

TEST (LumexDialogueEdgesTest,
      EdgeEvent_AHandlerThatThrowsLeavesTheRunTheOthersHearAndReadingGoesOn)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  seen_edges_t seen;
  bench.engine->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                         [] (pr::edge_t const &)
                           { throw std::runtime_error ("edge"); });
  watch (bench, seen);
  int frames = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                           [&frames] (bytes_t const &) { ++frames; });
  bench.deliver (status_frame (0));
  ASSERT_TRUE (bench.link->deliver (status_frame (1)));
  EXPECT_THROW (bench.context.poll (), std::runtime_error);
  bench.settle ();
  EXPECT_EQ (seen.size (), 1u);
  EXPECT_EQ (frames, 2);
  EXPECT_EQ (bench.link->pending_reads (), 1u);
}

TEST (LumexDialogueEdgesTest, EdgeEvent_AHandlerMayDestroyTheEngine)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.engine->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                         [&bench] (pr::edge_t const &)
                           { bench.engine.reset (); });
  bench.deliver (status_frame (0));
  bench.deliver (status_frame (1));
  EXPECT_EQ (bench.engine, nullptr);
  bench.settle ();
  EXPECT_EQ (bench.link->closes (), 1u);
}

TEST (LumexDialogueEdgesTest,
      EdgeEvent_AHandlerThatOwnsTheEngineDiesWithTheIoContextAndClose)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watched;
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    std::shared_ptr<ft::manual_link_t> const link
        = std::make_shared<ft::manual_link_t> (context);
    pr::dialogue_options_t options = dialogue::options_for<quiet_t> ();
    options.timers = clock.source ();
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine.reset (new pr::dialogue_engine (
        pr::frame_link (link), *ft::index::protocol_index<quiet_t> (),
        options));
    owner->engine->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                            [owner] (pr::edge_t const &) {});
    owner->engine->on_link_state (
        [owner] (pr::link_state, std::error_code const &) {});
    watched = owner;
    context.poll ();
    owner.reset ();
    EXPECT_FALSE (watched.expired ());
  }
  EXPECT_TRUE (watched.expired ());

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<ft::manual_link_t> const link
      = std::make_shared<ft::manual_link_t> (context);
  pr::dialogue_options_t options = dialogue::options_for<quiet_t> ();
  options.timers = clock.source ();
  std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
  owner->engine.reset (new pr::dialogue_engine (
      pr::frame_link (link), *ft::index::protocol_index<quiet_t> (), options));
  owner->engine->on_edge (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                          [owner] (pr::edge_t const &) {});
  owner->engine->on_link_state (
      [owner] (pr::link_state, std::error_code const &) {});
  watched = owner;
  pr::dialogue_engine *const engine = owner->engine.get ();
  owner.reset ();
  engine->close ();
  context.poll ();
  EXPECT_TRUE (watched.expired ());
  context.restart ();
  context.poll ();
}

// --- the last known state
// ----------------------------------------------------

TEST (LumexDialogueStateTest, State_ReadFromEveryFrameOfItsRow)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_FALSE (bench.engine->state_known ());
  EXPECT_EQ (bench.engine->last_state (), 0);
  bench.deliver (status_frame (0, tank_state::filling));
  EXPECT_TRUE (bench.engine->state_known ());
  EXPECT_EQ (bench.engine->last_state (),
             static_cast<std::int64_t> (tank_state::filling));
  outcome_ptr const out = bench.start_exchange (
      ft::make_exchange<quiet_t, quiet_t::STATUS_t> ());
  bench.settle ();
  bench.deliver (status_frame (0, tank_state::fault));
  EXPECT_EQ (out->calls, 1);
  EXPECT_EQ (bench.engine->last_state (),
             static_cast<std::int64_t> (tank_state::fault));
}

TEST (LumexDialogueStateTest, State_AFrameThatDoesNotDecodeChangesNothing)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::filling));
  bench.deliver (bytes ({ 0x12, 0x00 }));
  EXPECT_EQ (bench.engine->last_state (),
             static_cast<std::int64_t> (tank_state::filling));
}

TEST (LumexDialogueStateTest, State_SetByHandAndClearedByHandFromAnyThread)
{
  manual_bench_t<ft::wide_t> bench;
  bench.start ();
  EXPECT_FALSE (bench.engine->state_known ());
  bench.engine->set_state (7);
  EXPECT_TRUE (bench.engine->state_known ());
  EXPECT_EQ (bench.engine->last_state (), 7);
  bench.engine->clear_state ();
  EXPECT_FALSE (bench.engine->state_known ());
  bench.settle ();
}

TEST (LumexDialogueStateTest, State_FromAReplyOfOneFieldOnARowWithoutEdges)
{
  manual_bench_t<ft::one_field_t> bench;
  bench.start ();
  bench.deliver (bytes ({ 0x33, 0x02 }));
  EXPECT_TRUE (bench.engine->state_known ());
  EXPECT_EQ (bench.engine->last_state (), 2);
}

TEST (LumexDialogueStateTest, State_TheRowIsNotReadWithoutTheOption)
{
  manual_bench_t<ft::wide_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::fault));
  EXPECT_FALSE (bench.engine->state_known ());
}
} // namespace

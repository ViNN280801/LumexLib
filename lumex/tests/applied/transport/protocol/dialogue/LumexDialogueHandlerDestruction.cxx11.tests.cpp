// A handler of the user whose destruction calls back into the engine: the
// engine destroys what it replaces or drops outside its lock. Each test runs
// under a time limit: the fault it looks for is a thread waiting for itself.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace tr = ft::tr;
namespace layout = ft::layout;
using ft::bytes_t;
using ft::manual_bench_t;
using ft::quiet_t;
using context_t = lumex::core::protocol::field::decode_context_t;

// A capture that runs a function when the last copy dies.
struct on_destroy_t
{
  std::function<void ()> call;

  ~on_destroy_t ()
  {
    if (call)
      call ();
  }
};

using guard_ptr = std::shared_ptr<on_destroy_t>;

guard_ptr
guard (std::function<void ()> call)
{
  guard_ptr made = std::make_shared<on_destroy_t> ();
  made->call = std::move (call);
  return made;
}

// Runs `body` on a thread; a thread that waits for itself ends the binary.
void
within_limit (std::function<void ()> const &body)
{
  std::promise<void> finished;
  std::future<void> ended = finished.get_future ();
  std::thread worker (
      [&body, &finished] ()
        {
          body ();
          finished.set_value ();
        });
  if (ended.wait_for (std::chrono::seconds (20)) != std::future_status::ready)
    {
      std::fprintf (stderr, "the engine waits for its own lock\n");
      std::fflush (stderr);
      std::_Exit (3);
    }
  worker.join ();
}

lumex::core::protocol::index::row_t const *
level_row ()
{
  return ft::row_of<quiet_t, quiet_t::LEVEL_t> ();
}

lumex::core::protocol::index::row_t const *
query_row ()
{
  return ft::row_of<quiet_t, quiet_t::QUERY_t> ();
}

lumex::core::protocol::index::row_t const *
status_row ()
{
  return ft::row_of<quiet_t, quiet_t::STATUS_t> ();
}

TEST (LumexDialogueHandlerDestructionTest,
      Unsubscribe_ADestructorThatUnsubscribesAnother)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          pr::subscription_t const other
              = engine->subscribe (query_row (), [] (bytes_t const &) {});
          bool called = false;
          guard_ptr keep = guard (
              [engine, other, &called] ()
                {
                  called = true;
                  EXPECT_TRUE (engine->unsubscribe (other));
                });
          pr::subscription_t const first
              = engine->subscribe (level_row (), [keep] (bytes_t const &) {});
          keep.reset ();
          EXPECT_TRUE (engine->unsubscribe (first));
          EXPECT_TRUE (called);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      Unsubscribe_ADestructorThatSubscribes)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          pr::subscription_t made;
          guard_ptr keep = guard (
              [engine, &made] ()
                {
                  made = engine->subscribe (query_row (),
                                            [] (bytes_t const &) {});
                });
          pr::subscription_t const first
              = engine->subscribe (level_row (), [keep] (bytes_t const &) {});
          keep.reset ();
          EXPECT_TRUE (engine->unsubscribe (first));
          EXPECT_NE (made.id, 0u);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      Unsubscribe_AnEdgeHandlerWhoseDestructorCallsOnEdgeAndUnsubscribe)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          pr::subscription_t const other
              = engine->on_edge (status_row (), [] (pr::edge_t const &) {});
          pr::subscription_t made;
          bool removed = false;
          guard_ptr keep = guard (
              [engine, other, &made, &removed] ()
                {
                  removed = engine->unsubscribe (other);
                  made = engine->on_edge (status_row (),
                                          [] (pr::edge_t const &) {});
                });
          pr::subscription_t const first = engine->on_edge (
              status_row (), [keep] (pr::edge_t const &) {});
          keep.reset ();
          EXPECT_TRUE (engine->unsubscribe (first));
          EXPECT_TRUE (removed);
          EXPECT_NE (made.id, 0u);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      OnUnknown_AReplacedHandlerWhoseDestructorCallsOnUnknown)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          bool called = false;
          guard_ptr keep = guard (
              [engine, &called] ()
                {
                  called = true;
                  engine->on_unknown (pr::frame_handler_t ());
                });
          engine->on_unknown ([keep] (bytes_t const &) {});
          keep.reset ();
          engine->on_unknown ([] (bytes_t const &) {});
          EXPECT_TRUE (called);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      OnUnknown_ARemovedHandlerWhoseDestructorSubscribes)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          pr::subscription_t made;
          guard_ptr keep = guard (
              [engine, &made] ()
                {
                  made = engine->subscribe (level_row (),
                                            [] (bytes_t const &) {});
                });
          engine->on_unknown ([keep] (bytes_t const &) {});
          keep.reset ();
          engine->on_unknown (pr::frame_handler_t ());
          EXPECT_NE (made.id, 0u);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      OnLinkState_AReplacedHandlerWhoseDestructorCallsOnLinkState)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          bool called = false;
          guard_ptr keep = guard (
              [engine, &called] ()
                {
                  called = true;
                  engine->on_link_state (pr::link_state_handler_t ());
                });
          engine->on_link_state (
              [keep] (pr::link_state, std::error_code const &) {});
          keep.reset ();
          engine->on_link_state (
              [] (pr::link_state, std::error_code const &) {});
          EXPECT_TRUE (called);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      SetKeepalive_AReplacedGeneratorWhoseDestructorSetsOne)
{
  within_limit (
      [] ()
        {
          manual_bench_t<ft::clocked_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          bool called = false;
          guard_ptr keep = guard (
              [engine, &called] ()
                {
                  called = true;
                  engine->set_keepalive (pr::keepalive_generator_t ());
                });
          engine->set_keepalive ([keep] (layout::bytes_t &, context_t const &)
                                   { return std::error_code (); });
          keep.reset ();
          engine->set_keepalive (pr::keepalive_generator_t ());
          EXPECT_TRUE (called);
          bench.settle ();
        });
}

// Every handler at once, each calling back when it dies.
void
hold_every_handler (pr::dialogue_engine &engine, guard_ptr const &keep)
{
  engine.subscribe (level_row (), [keep] (bytes_t const &) {});
  engine.on_edge (status_row (), [keep] (pr::edge_t const &) {});
  engine.on_unknown ([keep] (bytes_t const &) {});
  engine.on_link_state ([keep] (pr::link_state, std::error_code const &) {});
}

void
call_back_everything (pr::dialogue_engine *engine)
{
  engine->subscribe (level_row (), [] (bytes_t const &) {});
  engine->on_edge (status_row (), [] (pr::edge_t const &) {});
  engine->on_unknown (pr::frame_handler_t ());
  engine->on_link_state (pr::link_state_handler_t ());
  engine->set_keepalive (pr::keepalive_generator_t ());
  engine->unsubscribe (pr::subscription_t ());
}

TEST (LumexDialogueHandlerDestructionTest,
      Close_HandlersWhoseDestructorsCallEveryMember)
{
  within_limit (
      [] ()
        {
          manual_bench_t<quiet_t> bench;
          bench.start ();
          pr::dialogue_engine *const engine = bench.engine.get ();
          bool called = false;
          guard_ptr keep = guard (
              [engine, &called] ()
                {
                  called = true;
                  call_back_everything (engine);
                });
          hold_every_handler (*engine, keep);
          keep.reset ();
          engine->close ();
          bench.settle ();
          EXPECT_TRUE (called);
        });
}

TEST (LumexDialogueHandlerDestructionTest,
      Shutdown_HandlersWhoseDestructorsCallEveryMember)
{
  within_limit (
      [] ()
        {
          struct owner_t
          {
            std::unique_ptr<pr::dialogue_engine> engine;
          };
          std::shared_ptr<bool> const called = std::make_shared<bool> (false);
          {
            asio::io_context context;
            tr::testing::virtual_clock clock;
            std::shared_ptr<ft::manual_link_t> const link
                = std::make_shared<ft::manual_link_t> (context);
            pr::dialogue_options_t options
                = ft::dialogue::options_for<quiet_t> ();
            options.timers = clock.source ();
            std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
            owner->engine.reset (new pr::dialogue_engine (
                pr::frame_link (link), *ft::index::protocol_index<quiet_t> (),
                options));
            // The guard owns the owner of the engine: the shutdown frees both.
            guard_ptr keep = guard (
                [owner, called] ()
                  {
                    *called = true;
                    call_back_everything (owner->engine.get ());
                  });
            hold_every_handler (*owner->engine, keep);
            keep.reset ();
            owner.reset ();
            context.poll ();
          }
          EXPECT_TRUE (*called);
        });
}
} // namespace

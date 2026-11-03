// The features of the dialogue engine under threads: real timers on a pool of
// threads, the calls of the front from other threads, frames coming, and the
// engine destroyed meanwhile. For TSan, and each operation ends exactly once.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
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
using ft::bytes;
using ft::bytes_t;
using ft::ms;
using ft::tank_t;

LUMEX_PROTOCOL_SCENARIO (threads_scenario, tank_t,
                         (expect<tank_t::LEVEL_t> (5)),
                         (send<tank_t::QUERY_t> ()));

/// Waits until `done`, at most ten seconds.
template <typename Done>
bool
eventually (Done done)
{
  for (int i = 0; i < 10000 && !done (); ++i)
    std::this_thread::sleep_for (ms (1));
  return done ();
}

TEST (LumexDialogueFeatureThreadsTest,
      Threads_FrontCallsAndFramesWhileTheEngineRunsAndDies)
{
  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> guard
      = asio::make_work_guard (context);
  std::vector<std::thread> pool;
  for (int i = 0; i < 3; ++i)
    pool.emplace_back ([&context] () { context.run (); });

  std::shared_ptr<ft::manual_link_t> const link
      = std::make_shared<ft::manual_link_t> (context);
  pr::dialogue_options_t options = dialogue::options_for<tank_t> ();
  options.name = "tank";
  options.defaults.timeout_ms = 5;
  options.defaults.retries = 0;
  options.max_in_flight = 1;
  options.read_restart_ms = 1;
  std::unique_ptr<pr::dialogue_engine> engine (new pr::dialogue_engine (
      pr::frame_link (link), *ft::index::protocol_index<tank_t> (), options));

  std::atomic<std::size_t> finished (0);
  std::atomic<std::size_t> started (0);
  std::atomic<std::size_t> edges (0);
  std::atomic<std::size_t> generated (0);
  std::atomic<bool> stop (false);
  std::vector<std::thread> callers;
  // The state and the handlers change from one thread, scenarios and streams
  // start from another, frames come from a third.
  pr::dialogue_engine *const front = engine.get ();
  callers.emplace_back (
      [&, front] ()
        {
          std::int64_t state = 0;
          while (!stop.load ())
            {
              front->set_state (state++ % 4);
              (void)front->last_state ();
              (void)front->state_known ();
              (void)front->link_is_up ();
              pr::subscription_t const made = front->on_edge (
                  ft::row_of<tank_t, tank_t::STATUS_t> (),
                  [&edges] (pr::edge_t const &) { ++edges; });
              front->set_keepalive (
                  [&generated] (
                      ft::layout::bytes_t &out,
                      lumex::core::protocol::field::decode_context_t const &)
                    {
                      ++generated;
                      out = bytes ({ 0x11, 1, 0, 0, 0 });
                      return std::error_code ();
                    });
              front->on_link_state (
                  [] (pr::link_state, std::error_code const &) {});
              front->unsubscribe (made);
              std::this_thread::sleep_for (std::chrono::microseconds (200));
            }
        });
  callers.emplace_back (
      [&, front] ()
        {
          while (!stop.load ())
            {
              ++started;
              pr::async_run_scenario<threads_scenario> (
                  *front,
                  [&finished] (std::error_code, std::size_t) { ++finished; });
              ++started;
              front->async_enable_streams (
                  [&finished] (std::error_code, std::size_t) { ++finished; });
              std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
        });
  callers.emplace_back (
      [&] ()
        {
          int valve = 0;
          while (!stop.load ())
            {
              link->deliver (ft::status_frame (valve++ % 2));
              link->deliver (bytes ({ 0x13, 0x01, 0x00 }));
              if (valve % 16 == 0)
                link->fail_read (std::make_error_code (std::errc::io_error));
              std::this_thread::sleep_for (std::chrono::microseconds (500));
            }
        });
  std::this_thread::sleep_for (ms (150));
  stop.store (true);
  for (std::size_t i = 0; i < callers.size (); ++i)
    callers[i].join ();
  engine.reset ();
  EXPECT_TRUE (
      eventually ([&] () { return finished.load () == started.load (); }))
      << finished.load () << " of " << started.load ();
  guard.reset ();
  context.stop ();
  for (std::size_t i = 0; i < pool.size (); ++i)
    pool[i].join ();
  EXPECT_EQ (finished.load (), started.load ());
  EXPECT_GT (started.load (), 0u);
}

TEST (LumexDialogueFeatureThreadsTest,
      Threads_RealTimersRunTheFeaturesToTheirEnd)
{
  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> guard
      = asio::make_work_guard (context);
  std::thread runner ([&context] () { context.run (); });
  {
    std::shared_ptr<ft::manual_link_t> const link
        = std::make_shared<ft::manual_link_t> (context);
    pr::dialogue_options_t options = dialogue::options_for<ft::watched_t> ();
    pr::dialogue_engine engine (pr::frame_link (link),
                                *ft::index::protocol_index<ft::watched_t> (),
                                options);
    // On the steady source: the watchdog of 300 ms ends the link.
    EXPECT_TRUE (eventually ([&engine] () { return !engine.link_is_up (); }));
    EXPECT_EQ (engine.statistics ().watchdog_expiries, 1u);
    EXPECT_EQ (link->closes (), 1u);
  }
  guard.reset ();
  context.stop ();
  runner.join ();
}
} // namespace

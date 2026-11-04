// The engine under random interleavings of everything the front can do and
// the link and the clock can bring, over the tank table with every feature:
// frames (random keys, right and wrong shapes), requests of every correlation
// and reply kind with their own policies, scenarios, the enabling of streams,
// cancellation by slot and by cancel (), read failures, the state, a
// keepalive, timers and writes that throw, handlers that throw or close the
// engine. In every sequence each exchange and each run ends exactly once, and
// after the end nothing waits on the clock. The test also says what the
// sequences reached, so that it cannot pass by reaching nothing.
//
// The seeds are fixed. To widen it for a hunt, raise kseeds (three sequences
// of ksteps per seed; 3600 sequences take about 10 s on a Debug tree without a
// sanitizer) or change kfirst_seed. At 100 it stays under 5 s under ASan.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
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
using ft::bytes;
using ft::bytes_t;
using ft::manual_bench_t;
using ft::ms;
using ft::status_frame;
using ft::tank_state;
using ft::tank_t;
using status_value_t
    = lumex::core::protocol::field::value_of<ft::tank_status_t>::type;

int const kseeds = 100;
int const ksteps = 400;
std::uint64_t const kfirst_seed = 1;

bool
valve_open (status_value_t const &status)
{
  return lumex::core::protocol::field::get<0> (status) == 1;
}

LUMEX_PROTOCOL_SCENARIO (
    long_scenario, tank_t, (send<tank_t::FILL_t> (5)),
    (expect<tank_t::LEVEL_t> (200)), (wait_state (tank_state::draining, 1000)),
    (send<tank_t::STATUS_t> ()),
    (branch_on_reply<tank_t::STATUS_t> (&valve_open, "o")),
    (send<tank_t::QUERY_t> ()), (jump ("e")), (label ("o")),
    (send<tank_t::DRAIN_t> (9)), (label ("e")));
LUMEX_PROTOCOL_SCENARIO (kinds_scenario, tank_t, (send<tank_t::STOP_t> ()),
                         (send<tank_t::PRIME_t> (100)),
                         (expect<tank_t::STATUS_t> ()),
                         (send<tank_t::ECHOED_t> (3)));
LUMEX_PROTOCOL_SCENARIO (for_ever_scenario, tank_t, (send<tank_t::QUERY_t> ()),
                         (expect<tank_t::LEVEL_t> (0)));
LUMEX_PROTOCOL_SCENARIO (state_scenario, tank_t,
                         (wait_state (tank_state::fault, 0)));
LUMEX_PROTOCOL_SCENARIO (slow_scenario, tank_t, (expect<tank_t::LEVEL_t> ()),
                         (send<tank_t::SLOW_t> ()),
                         (send<tank_t::ORDERED_SLOW_t> ()),
                         (send<tank_t::ERASE_t> (confirm, 3)));

struct random_t
{
  std::uint64_t state;

  std::uint32_t
  below (std::uint32_t limit)
  {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<std::uint32_t> (state >> 33) % limit;
  }
};

/// What the sequences reached, over the whole run.
struct reached_t
{
  std::uint64_t watchdog = 0;
  std::uint64_t keepalives = 0;
  std::uint64_t gaps = 0;
  std::uint64_t rate_limited = 0;
  std::uint64_t state_refusals = 0;
  std::uint64_t edges = 0;
  std::uint64_t late = 0;
  std::uint64_t naks = 0;
  std::uint64_t retries = 0;
  std::uint64_t queue_refusals = 0;
  std::uint64_t exchanges = 0;
  std::uint64_t exchanges_fine = 0;
  std::uint64_t runs = 0;
  std::uint64_t runs_fine = 0;
};

bytes_t
random_frame (random_t &r)
{
  static int const keys[]
      = { 0x06, 0x15, 0x12, 0x13, 0x21, 0x22, 0x31, 0x32, 0x33,
          0x34, 0x35, 0x36, 0x37, 0x38, 0x3A, 0x3B, 0x3C, 0x3D,
          0x41, 0x42, 0x43, 0x44, 0x7E, 0x40, 0x11, 0x39 };
  int const key = keys[r.below (sizeof (keys) / sizeof (keys[0]))];
  switch (key)
    {
    case 0x12:
      return status_frame (static_cast<int> (r.below (2)),
                           static_cast<tank_state> (r.below (4)),
                           static_cast<int> (r.below (300)));
    case 0x13:
      return bytes ({ key, static_cast<int> (r.below (256)),
                      static_cast<int> (r.below (2)) });
    case 0x41:
      return bytes ({ key, static_cast<int> (r.below (256)), 0 });
    case 0x36:
      return r.below (2) != 0
                 ? bytes ({ key, static_cast<int> (r.below (256)) })
                 : bytes ({ key, static_cast<int> (r.below (256)), 0 });
    case 0x06:
      return bytes ({ key });
    default:
      break;
    }
  if (r.below (10) == 0)
    return bytes ({ key });
  return bytes ({ key, static_cast<int> (r.below (4)) });
}

pr::exchange_t
random_request (random_t &r)
{
  using namespace lumex_dialogue_feature_test;
  switch (r.below (18))
    {
    case 0:
      return make_exchange<tank_t, tank_t::FILL_t> (
          static_cast<std::uint8_t> (r.below (4)));
    case 1:
      return make_exchange<tank_t, tank_t::DRAIN_t> (std::uint8_t (1));
    case 2:
      return make_exchange<tank_t, tank_t::QUERY_t> ();
    case 3:
      return make_exchange<tank_t, tank_t::SLOW_t> ();
    case 4:
      return make_exchange<tank_t, tank_t::RETRIED_t> ();
    case 5:
      return make_exchange<tank_t, tank_t::ORDERED_t> ();
    case 6:
      return make_exchange<tank_t, tank_t::TIMED_t> ();
    case 7:
      return make_exchange<tank_t, tank_t::NOTHING_t> ();
    case 8:
      return make_exchange<tank_t, tank_t::STOP_t> ();
    case 9:
      return make_exchange<tank_t, tank_t::PRIME_t> (std::uint16_t (100));
    case 10:
      return make_exchange<tank_t, tank_t::ECHOED_t> (std::uint8_t (3));
    case 11:
      return make_exchange<tank_t, tank_t::ZEROED_t> (
          static_cast<std::uint8_t> (r.below (2)));
    case 12:
      return make_exchange<tank_t, tank_t::STATED_t> (std::uint8_t (3));
    case 13:
      return make_exchange<tank_t, tank_t::ORDERED_SLOW_t> ();
    case 14:
      return make_exchange<tank_t, tank_t::QUICK_t> ();
    case 15:
      return make_exchange<tank_t, tank_t::RETRIED_QUICK_t> ();
    case 16:
      return make_exchange<tank_t, tank_t::SET_VALUE_t> (std::uint8_t (4));
    default:
      return make_exchange<tank_t, tank_t::ERASE_t> (std::uint8_t (1));
    }
}

using signal_ptr = std::shared_ptr<asio::cancellation_signal>;

template <typename Scenario>
void
start_scenario (pr::dialogue_engine &engine, bool bound,
                signal_ptr const &signal, ft::sequence_record_t const &record)
{
  if (bound)
    pr::async_run_scenario<Scenario> (
        engine, asio::bind_cancellation_slot (signal->slot (), record));
  else
    pr::async_run_scenario<Scenario> (engine, record);
}

// A handler that throws leaves the run of the context: go on, as an
// application whose run () loop catches would.
void
guarded (std::function<void ()> const &call)
{
  try
    {
      call ();
    }
  catch (std::exception const &)
    {
    }
}

// One sequence; mode 0 ends it by close (), 1 may also close it on the way, 2
// destroys the engine. False when an invariant broke.
bool
drive (std::uint64_t seed, int mode, reached_t &reached)
{
  random_t r = { seed * 2654435761ULL + 12345U };
  manual_bench_t<tank_t> bench;
  std::shared_ptr<ft::flaky_timers_t> const timers
      = std::make_shared<ft::flaky_timers_t> (bench.clock.source ());
  bench.options.timers = tr::timing::timer_source (timers);
  bench.options.max_in_flight = 1 + r.below (3);
  bench.options.queue_bound = 3 + r.below (6);
  bench.options.late_reply_window_ms = r.below (3) * 30;
  bench.start ();
  std::vector<ft::outcome_ptr> exchanges;
  std::vector<ft::sequence_outcome_ptr> runs;
  std::vector<signal_ptr> exchange_signals;
  std::vector<signal_ptr> run_signals;
  bool generator_throws = false;
  auto advance = [&bench] (ms by)
    {
      for (int i = 0; i < 50; ++i)
        try
          {
            bench.advance (by);
            return;
          }
        catch (std::exception const &)
          {
          }
    };
  auto settle = [&bench] ()
    {
      for (int i = 0; i < 50; ++i)
        try
          {
            bench.settle ();
            return;
          }
        catch (std::exception const &)
          {
          }
    };
  for (int step = 0; step < ksteps && bench.engine; ++step)
    {
      pr::dialogue_engine *const engine = bench.engine.get ();
      switch (r.below (24))
        {
        case 0:
        case 1:
        case 2:
        case 3:
          bench.link->deliver (random_frame (r));
          settle ();
          break;
        case 4:
        case 5:
          {
            static int const steps[]
                = { 0, 1, 7, 29, 31, 50, 99, 100, 399, 400, 401, 1000, 2500 };
            advance (ms (steps[r.below (sizeof (steps) / sizeof (steps[0]))]));
            break;
          }
        case 6:
        case 7:
        case 8:
          {
            ft::outcome_ptr const out = std::make_shared<ft::outcome_t> ();
            exchanges.push_back (out);
            pr::exchange_t request = random_request (r);
            if (r.below (3) == 0)
              request.policy = lumex_dialogue_test::policy (r.below (3) * 200,
                                                            r.below (3));
            signal_ptr const signal
                = std::make_shared<asio::cancellation_signal> ();
            exchange_signals.push_back (signal);
            ft::record_t const record = { out, &bench.clock };
            if (r.below (2) == 0)
              engine->async_exchange (
                  std::move (request),
                  asio::bind_cancellation_slot (signal->slot (), record));
            else
              engine->async_exchange (std::move (request), record);
            break;
          }
        case 9:
          if (!exchange_signals.empty ())
            {
              asio::cancellation_type const types[]
                  = { asio::cancellation_type::terminal,
                      asio::cancellation_type::partial,
                      asio::cancellation_type::total };
              exchange_signals[r.below (static_cast<std::uint32_t> (
                                   exchange_signals.size ()))]
                  ->emit (types[r.below (3)]);
            }
          break;
        case 10:
        case 11:
          {
            ft::sequence_outcome_ptr const out
                = std::make_shared<ft::sequence_outcome_t> ();
            runs.push_back (out);
            ft::sequence_record_t const record = { out };
            signal_ptr const signal
                = std::make_shared<asio::cancellation_signal> ();
            run_signals.push_back (signal);
            bool const bound = r.below (2) == 0;
            switch (r.below (5))
              {
              case 0:
                start_scenario<long_scenario> (*engine, bound, signal, record);
                break;
              case 1:
                start_scenario<kinds_scenario> (*engine, bound, signal,
                                                record);
                break;
              case 2:
                start_scenario<for_ever_scenario> (*engine, bound, signal,
                                                   record);
                break;
              case 3:
                start_scenario<state_scenario> (*engine, bound, signal,
                                                record);
                break;
              default:
                start_scenario<slow_scenario> (*engine, bound, signal, record);
                break;
              }
            break;
          }
        case 12:
          if (!run_signals.empty ())
            run_signals[r.below (
                            static_cast<std::uint32_t> (run_signals.size ()))]
                ->emit (r.below (2) != 0 ? asio::cancellation_type::terminal
                                         : asio::cancellation_type::partial);
          break;
        case 13:
          {
            ft::sequence_outcome_ptr const out
                = std::make_shared<ft::sequence_outcome_t> ();
            runs.push_back (out);
            ft::sequence_record_t const record = { out };
            engine->async_enable_streams (record);
            break;
          }
        case 14:
          if (r.below (2) != 0)
            bench.link->fail_read (std::make_error_code (std::errc::io_error));
          else
            bench.link->cancel_reads ();
          settle ();
          break;
        case 15:
          if (r.below (3) == 0)
            engine->clear_state ();
          else
            engine->set_state (static_cast<std::int64_t> (r.below (4)));
          break;
        case 16:
          if (r.below (6) == 0)
            engine->cancel ();
          break;
        case 17:
          if (r.below (2) != 0)
            engine->set_keepalive (dialogue::keepalive_from<tank_t> (
                [&generator_throws] () -> std::tuple<std::uint32_t>
                  {
                    if (generator_throws)
                      throw std::runtime_error ("generator");
                    return std::make_tuple (std::uint32_t (1));
                  }));
          else
            engine->set_keepalive (pr::keepalive_generator_t ());
          break;
        case 18:
          generator_throws = r.below (2) == 0;
          bench.link->throw_on_write (r.below (8) == 0);
          timers->set_throwing (r.below (8) == 0);
          break;
        case 19:
          switch (r.below (5))
            {
            case 0:
              engine->on_link_state (
                  [engine] (pr::link_state, std::error_code const &)
                    { engine->close (); });
              break;
            case 1:
              engine->on_link_state (
                  [] (pr::link_state, std::error_code const &)
                    { throw std::runtime_error ("link"); });
              break;
            case 2:
              engine->on_edge (ft::row_of<tank_t, tank_t::STATUS_t> (),
                               [engine] (pr::edge_t const &)
                                 { engine->cancel (); });
              break;
            case 3:
              engine->on_edge (ft::row_of<tank_t, tank_t::STATUS_t> (),
                               [] (pr::edge_t const &)
                                 { throw std::runtime_error ("edge"); });
              break;
            default:
              engine->subscribe (ft::row_of<tank_t, tank_t::LEVEL_t> (),
                                 [] (bytes_t const &)
                                   { throw std::runtime_error ("frame"); });
              break;
            }
          break;
        case 20:
          if (mode == 1 && r.below (30) == 0)
            engine->close ();
          break;
        default:
          advance (ms (r.below (120)));
          break;
        }
      for (std::size_t i = 0; i < exchanges.size (); ++i)
        if (exchanges[i]->calls > 1)
          return false;
      for (std::size_t i = 0; i < runs.size (); ++i)
        if (runs[i]->calls > 1)
          return false;
    }
  timers->set_throwing (false);
  bench.link->throw_on_write (false);
  pr::dialogue_statistics_t const counted = bench.engine->statistics ();
  reached.watchdog += counted.watchdog_expiries;
  reached.keepalives += counted.keepalives;
  reached.gaps += counted.stream_gaps;
  reached.rate_limited += counted.rate_limited;
  reached.state_refusals += counted.state_refusals;
  reached.edges += counted.edge_events;
  reached.late += counted.late_replies;
  reached.naks += counted.naks;
  reached.retries += counted.retries;
  reached.queue_refusals += counted.queue_refusals;
  if (mode == 2)
    bench.engine.reset ();
  else
    bench.engine->close ();
  settle ();
  advance (ms (20000));
  settle ();
  bool fine = bench.clock.pending () == 0;
  for (std::size_t i = 0; i < exchanges.size (); ++i)
    {
      fine = fine && exchanges[i]->calls == 1;
      ++reached.exchanges;
      if (!exchanges[i]->error)
        ++reached.exchanges_fine;
    }
  for (std::size_t i = 0; i < runs.size (); ++i)
    {
      fine = fine && runs[i]->calls == 1;
      ++reached.runs;
      if (!runs[i]->error)
        ++reached.runs_fine;
    }
  return fine;
}

TEST (LumexDialogueRandomDriverTest,
      Random_EveryCompletionOnceAndNoWaitLeftInEveryInterleaving)
{
  // The log of 400 steps per sequence is no part of the test.
  tr::log::log_level const level = tr::log::get_log_level ();
  tr::log::set_log_level (tr::log::log_level::off);
  reached_t reached;
  for (int mode = 0; mode < 3; ++mode)
    for (int seed = 0; seed < kseeds; ++seed)
      {
        std::uint64_t const number
            = kfirst_seed + static_cast<std::uint64_t> (seed)
              + 1000U * static_cast<std::uint64_t> (mode);
        EXPECT_TRUE (drive (number, mode, reached))
            << "seed " << number << " mode " << mode;
      }
  tr::log::set_log_level (level);
  std::printf (
      "reached in %d sequences: exchanges %llu (fine %llu), runs %llu "
      "(fine %llu), watchdog %llu, keepalives %llu, gaps %llu, rate "
      "limited %llu, state refusals %llu, edges %llu, late %llu, naks "
      "%llu, retries %llu, queue refusals %llu\n",
      3 * kseeds, static_cast<unsigned long long> (reached.exchanges),
      static_cast<unsigned long long> (reached.exchanges_fine),
      static_cast<unsigned long long> (reached.runs),
      static_cast<unsigned long long> (reached.runs_fine),
      static_cast<unsigned long long> (reached.watchdog),
      static_cast<unsigned long long> (reached.keepalives),
      static_cast<unsigned long long> (reached.gaps),
      static_cast<unsigned long long> (reached.rate_limited),
      static_cast<unsigned long long> (reached.state_refusals),
      static_cast<unsigned long long> (reached.edges),
      static_cast<unsigned long long> (reached.late),
      static_cast<unsigned long long> (reached.naks),
      static_cast<unsigned long long> (reached.retries),
      static_cast<unsigned long long> (reached.queue_refusals));
  // Every path it is there for was reached.
  EXPECT_GT (reached.exchanges_fine, 0u);
  EXPECT_GT (reached.runs_fine, 0u);
  EXPECT_GT (reached.watchdog, 0u);
  EXPECT_GT (reached.keepalives, 0u);
  EXPECT_GT (reached.gaps, 0u);
  EXPECT_GT (reached.rate_limited, 0u);
  EXPECT_GT (reached.state_refusals, 0u);
  EXPECT_GT (reached.edges, 0u);
  EXPECT_GT (reached.late, 0u);
  EXPECT_GT (reached.naks, 0u);
  EXPECT_GT (reached.retries, 0u);
  EXPECT_GT (reached.queue_refusals, 0u);
}
} // namespace

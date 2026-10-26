// Many sessions on one io_runner with two threads: the test thread closes,
// starts, cancels, flips the device presence, disconnects, starts I/O and
// state waits, and destroys sessions at random while they connect and
// reconnect (factories that fail and throw, a short policy with jitter, an
// open timeout). Twenty rounds, each with its own seed. After a round: every
// session that is left closes and stays closed (no factory call after it),
// every handler ran exactly once, and every implementation was released.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
typedef ss::channel_state state;

struct slot
{
  std::shared_ptr<stream_factory_state> factory;
  std::unique_ptr<ss::stream_session> session;
  std::weak_ptr<int> sentinel;
  std::vector<std::uint8_t> buffer = std::vector<std::uint8_t> (8);
  // One read at a time per stream, as for any Asio stream (the mock keeps
  // one pending read).
  std::shared_ptr<std::atomic<bool>> reading
      = std::make_shared<std::atomic<bool>> (false);
};

struct handler_counts
{
  std::atomic<int> issued;
  std::atomic<int> completed;
  std::atomic<int> twice;
  handler_counts () : issued (0), completed (0), twice (0) {}
};

// Completes once; a second call would be a defect.
struct once_flag_handler
{
  std::shared_ptr<handler_counts> counts;
  std::shared_ptr<std::atomic<bool>> called;

  void
  mark ()
  {
    if (called->exchange (true))
      counts->twice.fetch_add (1);
    else
      counts->completed.fetch_add (1);
  }
};

struct state_wait_handler : once_flag_handler
{
  void
  operator() (std::error_code, state)
  {
    mark ();
  }
};

struct read_handler : once_flag_handler
{
  std::shared_ptr<std::atomic<bool>> reading;

  void
  operator() (std::error_code, std::size_t)
  {
    reading->store (false);
    mark ();
  }
};

ss::stream_session::factory_t
sentinel_factory (std::shared_ptr<stream_factory_state> const &state,
                  std::shared_ptr<int> const &sentinel)
{
  ss::stream_session::factory_t const inner = make_stream_factory (state);
  return [inner, sentinel] (tr::executor_type const &executor)
    { return inner (executor); };
}

void
make_slot (slot &target, tr::stream::io_runner &runner, std::mt19937 &random)
{
  target.factory = std::make_shared<stream_factory_state> ();
  target.factory->open_failures = random () % 4;
  target.factory->throws = random () % 2;
  if (random () % 4 == 0)
    target.factory->hold_call = 1 + random () % 3;
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  target.sentinel = sentinel;
  ss::session_options_t options;
  options.policy.initial_delay = ms (1);
  options.policy.max_delay = ms (4);
  options.policy.jitter = 0.5;
  options.policy.max_attempts = random () % 2 == 0 ? 0u : 5u;
  options.open_timeout = ms (20);
  std::shared_ptr<std::atomic<int>> const callbacks
      = std::make_shared<std::atomic<int>> (0);
  options.on_state_change
      = [callbacks] (ss::state_change_t const &) { callbacks->fetch_add (1); };
  target.session.reset (new ss::stream_session (
      runner.get_executor (), sentinel_factory (target.factory, sentinel),
      options));
  target.session->start ();
}
} // namespace

TEST (
    LumexSessionStressTest,
    GivenManySessionsOnTwoThreads_WhenTheyAreDrivenAtRandom_ThenEveryHandlerEndsOnceAndNothingLeaks)
{
  for (unsigned round = 0; round < 20; ++round)
    {
      SCOPED_TRACE (round);
      std::mt19937 random (round + 1);
      std::shared_ptr<handler_counts> const counts
          = std::make_shared<handler_counts> ();
      std::vector<std::weak_ptr<int>> released;
      {
        tr::stream::io_runner runner (2);
        // After the runner: the mock streams the factories keep (and the
        // read buffers) die before their context.
        std::vector<slot> slots (24);
        for (std::size_t i = 0; i < slots.size (); ++i)
          make_slot (slots[i], runner, random);

        for (int step = 0; step < 300; ++step)
          {
            slot &target = slots[random () % slots.size ()];
            if (!target.session)
              {
                released.push_back (target.sentinel);
                make_slot (target, runner, random);
                continue;
              }
            switch (random () % 9)
              {
              case 0:
                target.session->close ();
                break;
              case 1:
                target.session->start ();
                break;
              case 2:
                target.session->cancel ();
                break;
              case 3:
                target.session->set_device_present (random () % 2 == 0);
                break;
              case 4:
                {
                  state_wait_handler handler;
                  handler.counts = counts;
                  handler.called = std::make_shared<std::atomic<bool>> (false);
                  counts->issued.fetch_add (1);
                  target.session->async_wait_state_change (
                      target.session->state (), handler);
                  break;
                }
              case 5:
                if (!target.reading->exchange (true))
                  {
                    read_handler handler;
                    handler.reading = target.reading;
                    handler.counts = counts;
                    handler.called
                        = std::make_shared<std::atomic<bool>> (false);
                    counts->issued.fetch_add (1);
                    target.session->async_read_some (
                        tr::mutable_bytes_t (target.buffer), handler);
                  }
                break;
              case 6:
                try
                  {
                    target.factory->last ().disconnect ();
                  }
                catch (std::logic_error const &)
                  {
                    // No stream made yet (or every call threw).
                  }
                break;
              case 7:
                target.session.reset ();
                break;
              default:
                target.session->set_device_present (true);
                target.session->start ();
                break;
              }
            // Let the strands and the timers (1 to 4 ms) move; a real sleep
            // is rare (a Windows tick is about 15 ms).
            if (random () % 64 == 0)
              std::this_thread::sleep_for (ms (1));
            else if (random () % 4 == 0)
              std::this_thread::yield ();
          }

        // Close what is left: each ends closed and its factory stays still.
        for (std::size_t i = 0; i < slots.size (); ++i)
          if (slots[i].session)
            slots[i].session->close ();
        bool const all_closed = eventually (
            [&slots] ()
              {
                for (std::size_t i = 0; i < slots.size (); ++i)
                  if (slots[i].session
                      && slots[i].session->state () != state::closed)
                    return false;
                return true;
              });
        for (std::size_t i = 0; i < slots.size () && !all_closed; ++i)
          if (slots[i].session)
            EXPECT_EQ (slots[i].session->state (), state::closed)
                << "slot " << i;
        ASSERT_TRUE (all_closed);
        std::vector<std::size_t> calls;
        for (std::size_t i = 0; i < slots.size (); ++i)
          calls.push_back (slots[i].factory->call_count ());
        std::this_thread::sleep_for (ms (30));
        for (std::size_t i = 0; i < slots.size (); ++i)
          EXPECT_EQ (slots[i].factory->call_count (), calls[i])
              << "slot " << i;

        for (std::size_t i = 0; i < slots.size (); ++i)
          {
            released.push_back (slots[i].sentinel);
            slots[i].session.reset ();
          }
        ASSERT_TRUE (eventually (
            [&counts]
              { return counts->completed.load () == counts->issued.load (); }))
            << counts->completed.load () << " of " << counts->issued.load ();
        EXPECT_EQ (counts->twice.load (), 0);
        EXPECT_TRUE (eventually (
            [&released] ()
              {
                for (std::size_t i = 0; i < released.size (); ++i)
                  if (!released[i].expired ())
                    return false;
                return true;
              }))
            << "an implementation is still alive";
      }
    }
}

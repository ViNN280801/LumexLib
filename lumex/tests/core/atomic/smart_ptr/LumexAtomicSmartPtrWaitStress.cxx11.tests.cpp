// wait () and notify_*() under load. A lost wake-up is a thread that sleeps
// forever, so every test runs under a watchdog that aborts with a message;
// the shapes are the ones that expose a lost wake-up: a token passed around a
// ring of threads (every hand-over is a store, a notify and a wait that may
// start before, during or after it), a notify_one per sleeper, and more
// atomics than the 64 stripes of the pre-C++20 wait table so that several
// waiters share a stripe.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
typedef std::shared_ptr<int> IntPtr;

/// Passes a token around @p ring_size threads for @p rounds laps; thread i
/// takes the turns whose value is i modulo the ring size.
void
pass_token (int ring_size, int rounds, bool notify_all,
            lumex_test::ScheduleKind kind, std::uint64_t seed)
{
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::string const error = lumex_test::run_threads (
      ring_size,
      [&] (int i)
        {
          lumex_test::Schedule schedule (
              kind,
              lumex_test::derive_seed (seed, static_cast<std::uint64_t> (i)));
          for (int lap = 0; lap < rounds; ++lap)
            {
              IntPtr current = a.load ();
              while (*current % ring_size != i)
                {
                  a.wait (current);
                  current = a.load ();
                }
              schedule.point ();
              a.store (std::make_shared<int> (*current + 1));
              if (notify_all)
                a.notify_all ();
              else
                a.notify_one ();
            }
        });
  EXPECT_TRUE (error.empty ()) << error;
  EXPECT_EQ (*a.load (), ring_size * rounds)
      << "ring=" << ring_size << " rounds=" << rounds;
}
} // namespace

TEST (
    LumexAtomicSmartPtrWaitStressTest,
    GivenATokenPassedAroundARing_WhenEveryHandOverNotifiesAll_ThenNoWakeUpIsLost)
{
  lumex_test::TestWatchdog const dog ("GivenATokenPassedAroundARing");
  int const sizes[] = { 2, 3, 5, 9 };
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  for (std::size_t s = 0; s < sizeof (sizes) / sizeof (sizes[0]); ++s)
    for (std::size_t k = 0; k < kinds.size (); ++k)
      {
        int rounds = lumex_test::scaled (150);
        if (kinds[k] == lumex_test::ScheduleKind::cold)
          rounds = std::max (4, rounds / 8);
        pass_token (
            sizes[s], rounds, true, kinds[k],
            lumex_test::derive_seed (lumex_test::base_seed (), s * 8 + k));
      }
}

TEST (
    LumexAtomicSmartPtrWaitStressTest,
    GivenATokenPassedBetweenTwoThreads_WhenEveryHandOverNotifiesOne_ThenNoWakeUpIsLost)
{
  lumex_test::TestWatchdog const dog ("GivenATokenPassedBetweenTwoThreads");
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  for (std::size_t k = 0; k < kinds.size (); ++k)
    pass_token (2, lumex_test::scaled (300), false, kinds[k],
                lumex_test::derive_seed (lumex_test::base_seed (), 100 + k));
}

TEST (
    LumexAtomicSmartPtrWaitStressTest,
    GivenMoreAtomicsThanWaitStripes_WhenEachWaiterIsNotifiedOnce_ThenEveryWaiterWakes)
{
  lumex_test::TestWatchdog const dog ("GivenMoreAtomicsThanWaitStripes");
  int const count = 100; // more than the 64 stripes of the wait table
  for (int variant = 0; variant < 2; ++variant)
    {
      // variant 0: waiters sleep first, then the change; variant 1: the
      // change comes first and the waiters must not sleep at all.
      std::vector<std::unique_ptr<atomic_shared_ptr<int>>> atoms;
      std::vector<IntPtr> olds;
      for (int i = 0; i < count; ++i)
        {
          olds.push_back (std::make_shared<int> (i));
          atoms.push_back (std::unique_ptr<atomic_shared_ptr<int>> (
              new atomic_shared_ptr<int> (olds.back ())));
        }
      std::atomic<int> woken (0);
      std::atomic<int> started (0);
      if (variant == 1)
        for (int i = 0; i < count; ++i)
          {
            atoms[static_cast<std::size_t> (i)]->store (
                std::make_shared<int> (-i - 1));
            atoms[static_cast<std::size_t> (i)]->notify_one ();
          }
      std::vector<std::thread> waiters;
      for (int i = 0; i < count; ++i)
        waiters.push_back (std::thread (
            [&, i]
              {
                started.fetch_add (1);
                atoms[static_cast<std::size_t> (i)]->wait (
                    olds[static_cast<std::size_t> (i)]);
                woken.fetch_add (1);
              }));
      while (started.load () < count)
        std::this_thread::yield ();
      if (variant == 0)
        {
          sleep_ms (30);
          EXPECT_EQ (woken.load (), 0) << "nobody was notified yet";
          for (int i = 0; i < count; ++i)
            {
              atoms[static_cast<std::size_t> (i)]->store (
                  std::make_shared<int> (-i - 1));
              atoms[static_cast<std::size_t> (i)]->notify_one ();
            }
        }
      join_all (waiters);
      EXPECT_EQ (woken.load (), count) << "variant=" << variant;
    }
}

TEST (LumexAtomicSmartPtrWaitStressTest,
      GivenWaitersOfTwoAtomics_WhenOneIsNotified_ThenOnlyItsWaitersReturn)
{
  // Two atomics, many waiters on each; changing and notifying one of them
  // must not release the waiters of the other (a stripe of the pre-C++20
  // wait table may be shared, the value is not).
  lumex_test::TestWatchdog const dog ("GivenWaitersOfTwoAtomics");
  IntPtr const old_a = std::make_shared<int> (1);
  IntPtr const old_b = std::make_shared<int> (2);
  atomic_shared_ptr<int> a (old_a);
  atomic_shared_ptr<int> b (old_b);
  std::atomic<int> returned_a (0);
  std::atomic<int> returned_b (0);
  std::atomic<int> started (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < 6; ++i)
    {
      waiters.push_back (std::thread (
          [&]
            {
              started.fetch_add (1);
              a.wait (old_a);
              returned_a.fetch_add (1);
            }));
      waiters.push_back (std::thread (
          [&]
            {
              started.fetch_add (1);
              b.wait (old_b);
              returned_b.fetch_add (1);
            }));
    }
  while (started.load () < 12)
    std::this_thread::yield ();
  sleep_ms (30);
  a.store (std::make_shared<int> (10));
  a.notify_all ();
  wait_for_count (returned_a, 6);
  sleep_ms (30);
  EXPECT_EQ (returned_a.load (), 6);
  EXPECT_EQ (returned_b.load (), 0)
      << "the waiters of the other atomic stay asleep";
  b.store (std::make_shared<int> (20));
  b.notify_all ();
  join_all (waiters);
  EXPECT_EQ (returned_b.load (), 6);
}

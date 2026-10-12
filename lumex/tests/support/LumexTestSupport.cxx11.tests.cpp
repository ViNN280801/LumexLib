// Tests of the header-only helpers of lumex/tests/support that the concurrency
// tests build on: the configuration, the seeded random schedule, the thread
// helpers, the reuse-address allocator, the object ledger, the history
// recorder with its Wing and Gong checker, and the subprocess runner. The
// helpers decide whether a concurrency test can see a bug, so each is tested
// on a case it must accept and a case it must reject.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/support/LumexTestConfig.hpp"
#include "lumex/tests/support/LumexTestHistory.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"
#include "lumex/tests/support/LumexTestReplay.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

namespace
{
void
set_env (char const *name, char const *value)
{
#if defined(_WIN32)
  _putenv_s (name, value == nullptr ? "" : value);
#else
  if (value == nullptr)
    unsetenv (name);
  else
    setenv (name, value, 1);
#endif
}

/// Restores an environment variable when the test ends.
class EnvGuard
{
public:
  explicit EnvGuard (char const *name) : name_ (name), had_ (false)
  {
    char const *old = std::getenv (name);
    if (old != nullptr)
      {
        had_ = true;
        old_ = old;
      }
  }

  ~EnvGuard () { set_env (name_, had_ ? old_.c_str () : nullptr); }

  EnvGuard (EnvGuard const &) = delete;
  EnvGuard &operator= (EnvGuard const &) = delete;

private:
  char const *name_;
  bool had_;
  std::string old_;
};

lumex_test::HistoryOp
op (int thread, int kind, std::uint64_t invoke, std::uint64_t respond,
    std::int64_t arg0, std::int64_t arg1, std::int64_t res0, std::int64_t res1)
{
  lumex_test::HistoryOp o;
  o.thread = thread;
  o.kind = kind;
  o.arg0 = arg0;
  o.arg1 = arg1;
  o.res0 = res0;
  o.res1 = res1;
  o.invoke = invoke;
  o.respond = respond;
  return o;
}
} // namespace

// --- configuration
// -----------------------------------------------------------

TEST (LumexTestConfigTest, GivenNoVariables_WhenRead_ThenDefaultsApply)
{
  EnvGuard seed ("LUMEX_TEST_SEED");
  EnvGuard threads ("LUMEX_TEST_THREADS");
  EnvGuard scale ("LUMEX_TEST_SCALE");
  EnvGuard soak ("LUMEX_TEST_SOAK");
  EnvGuard soak_seconds ("LUMEX_TEST_SOAK_SECONDS");
  set_env ("LUMEX_TEST_SEED", nullptr);
  set_env ("LUMEX_TEST_THREADS", nullptr);
  set_env ("LUMEX_TEST_SCALE", nullptr);
  set_env ("LUMEX_TEST_SOAK", nullptr);
  set_env ("LUMEX_TEST_SOAK_SECONDS", nullptr);
  EXPECT_EQ (lumex_test::base_seed (), 20261008u);
  EXPECT_FALSE (lumex_test::seed_is_pinned ());
  EXPECT_EQ (lumex_test::scale_factor (), 1);
  EXPECT_FALSE (lumex_test::soak_requested ());
  EXPECT_EQ (lumex_test::soak_seconds (), 20);
  std::vector<int> const counts = lumex_test::thread_counts ();
  ASSERT_EQ (counts.size (), 5u);
  EXPECT_EQ (counts[0], 1);
  EXPECT_EQ (counts[1], 2);
  EXPECT_EQ (counts[2], 4);
  EXPECT_EQ (counts[3], 8);
  EXPECT_GT (counts[4], static_cast<int> (lumex_test::hardware_threads ()))
      << "the last default count is above the number of cores";
  EXPECT_EQ (lumex_test::thread_counts (false).size (), 4u);
}

TEST (LumexTestConfigTest, GivenVariables_WhenRead_ThenTheyOverrideTheDefaults)
{
  EnvGuard seed ("LUMEX_TEST_SEED");
  EnvGuard threads ("LUMEX_TEST_THREADS");
  EnvGuard scale ("LUMEX_TEST_SCALE");
  EnvGuard soak ("LUMEX_TEST_SOAK");
  EnvGuard soak_seconds ("LUMEX_TEST_SOAK_SECONDS");
  set_env ("LUMEX_TEST_SEED", "777");
  set_env ("LUMEX_TEST_THREADS", "3, 5;9");
  set_env ("LUMEX_TEST_SCALE", "4");
  set_env ("LUMEX_TEST_SOAK", "1");
  set_env ("LUMEX_TEST_SOAK_SECONDS", "5");
  EXPECT_EQ (lumex_test::base_seed (), 777u);
  EXPECT_TRUE (lumex_test::seed_is_pinned ());
  std::vector<int> const counts = lumex_test::thread_counts ();
  ASSERT_EQ (counts.size (), 3u);
  EXPECT_EQ (counts[0], 3);
  EXPECT_EQ (counts[1], 5);
  EXPECT_EQ (counts[2], 9);
  EXPECT_EQ (lumex_test::scale_factor (), 4);
  EXPECT_TRUE (lumex_test::soak_requested ());
  EXPECT_EQ (lumex_test::soak_seconds (), 5);
}

TEST (LumexTestConfigTest, GivenMalformedVariables_WhenRead_ThenDefaultsApply)
{
  EnvGuard seed ("LUMEX_TEST_SEED");
  EnvGuard scale ("LUMEX_TEST_SCALE");
  EnvGuard soak ("LUMEX_TEST_SOAK");
  set_env ("LUMEX_TEST_SEED", "abc");
  set_env ("LUMEX_TEST_SCALE", "0");
  set_env ("LUMEX_TEST_SOAK", "0");
  EXPECT_EQ (lumex_test::base_seed (), 20261008u);
  EXPECT_EQ (lumex_test::scale_factor (), 1);
  EXPECT_FALSE (lumex_test::soak_requested ()) << "\"0\" is off";
  set_env ("LUMEX_TEST_SCALE", "100000");
  EXPECT_EQ (lumex_test::scale_factor (), 1) << "out of range";
}

TEST (LumexTestConfigTest, GivenAScale_WhenScaled_ThenTheWorkFollowsIt)
{
  EnvGuard scale ("LUMEX_TEST_SCALE");
  set_env ("LUMEX_TEST_SCALE", "1");
  int const base = lumex_test::scaled (600);
  set_env ("LUMEX_TEST_SCALE", "3");
  EXPECT_EQ (lumex_test::scaled (600), 3 * base);
  EXPECT_GE (lumex_test::scaled (1), 1) << "never zero";
}

TEST (LumexTestConfigTest,
      GivenSeedsOfDifferentStreams_WhenDerived_ThenTheyDiffer)
{
  std::set<std::uint64_t> seen;
  for (std::uint64_t a = 0; a < 20; ++a)
    for (std::uint64_t b = 0; b < 20; ++b)
      seen.insert (lumex_test::derive_seed (1234, a, b));
  EXPECT_EQ (seen.size (), 400u);
  EXPECT_EQ (lumex_test::derive_seed (1234, 5, 6),
             lumex_test::derive_seed (1234, 5, 6))
      << "derivation is deterministic";
  EXPECT_NE (lumex_test::derive_seed (1, 5, 6),
             lumex_test::derive_seed (2, 5, 6));
}

TEST (LumexTestConfigTest,
      GivenASeedAndThreads_WhenReplayText_ThenBothVariablesAppear)
{
  std::string const text = lumex_test::replay_text (42, 8);
  EXPECT_NE (text.find ("LUMEX_TEST_SEED=42"), std::string::npos);
  EXPECT_NE (text.find ("LUMEX_TEST_THREADS=8"), std::string::npos);
}

TEST (LumexTestConfigTest,
      GivenTheBuild_WhenAskedForSlowdown_ThenItMatchesTheSanitizer)
{
  int const divisor = lumex_test::slowdown_divisor ();
  EXPECT_GE (divisor, 1);
  if (lumex_test::instrumented_build ())
    {
      EXPECT_GE (divisor, 3);
    }
#if LUMEX_TEST_HAS_TSAN
  EXPECT_GE (divisor, 6);
#endif
}

// --- seeded random and the schedule
// -------------------------------------------

TEST (LumexTestScheduleTest, GivenOneSeed_WhenTwoStreamsRun_ThenTheyAreEqual)
{
  lumex_test::SeededRandom a (99);
  lumex_test::SeededRandom b (99);
  lumex_test::SeededRandom c (100);
  bool differ = false;
  for (int i = 0; i < 100; ++i)
    {
      std::uint64_t const x = a.next ();
      EXPECT_EQ (x, b.next ());
      if (x != c.next ())
        differ = true;
    }
  EXPECT_TRUE (differ);
}

TEST (LumexTestScheduleTest,
      GivenABound_WhenBelow_ThenValuesStayInsideAndCoverIt)
{
  lumex_test::SeededRandom random (7);
  std::set<std::uint32_t> seen;
  for (int i = 0; i < 2000; ++i)
    {
      std::uint32_t const v = random.below (10);
      ASSERT_LT (v, 10u);
      seen.insert (v);
    }
  EXPECT_EQ (seen.size (), 10u);
}

TEST (LumexTestScheduleTest, GivenPercentages_WhenChance_ThenTheRateIsClose)
{
  lumex_test::SeededRandom random (11);
  int hits = 0;
  for (int i = 0; i < 20000; ++i)
    if (random.chance (25))
      ++hits;
  EXPECT_GT (hits, 4000);
  EXPECT_LT (hits, 6000);
  for (int i = 0; i < 100; ++i)
    {
      EXPECT_FALSE (random.chance (0));
      EXPECT_TRUE (random.chance (100));
    }
}

TEST (LumexTestScheduleTest, GivenEveryKind_WhenPointsRun_ThenTheyReturn)
{
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  ASSERT_EQ (kinds.size (), 4u);
  std::set<std::string> names;
  for (std::size_t i = 0; i < kinds.size (); ++i)
    {
      lumex_test::Schedule schedule (kinds[i], 5);
      EXPECT_EQ (schedule.kind (), kinds[i]);
      for (int p = 0; p < 40; ++p)
        schedule.point ();
      names.insert (lumex_test::schedule_name (kinds[i]));
    }
  EXPECT_EQ (names.size (), 4u) << "every kind has its own name";
}

TEST (LumexTestScheduleTest,
      GivenEveryKind_WhenPointsRun_ThenOnlyTheColdKindFlushesTheCache)
{
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  for (std::size_t i = 0; i < kinds.size (); ++i)
    {
      lumex_test::Schedule schedule (kinds[i], 3);
      unsigned long const before = lumex_test::cache_flush_count ().load ();
      for (int p = 0; p < 20; ++p)
        schedule.point ();
      unsigned long const flushes
          = lumex_test::cache_flush_count ().load () - before;
      if (kinds[i] == lumex_test::ScheduleKind::cold)
        EXPECT_EQ (flushes, 20ul) << "every point of a cold schedule flushes";
      else
        EXPECT_EQ (flushes, 0ul) << lumex_test::schedule_name (kinds[i]);
    }
}

// --- threads
// --------------------------------------------------------------------

TEST (LumexTestThreadsTest,
      GivenABarrier_WhenThreadsArrive_ThenNoneLeavesEarly)
{
  int const n = 6;
  lumex_test::StartBarrier barrier (n);
  std::atomic<int> arrived (0);
  std::atomic<int> early (0);
  std::vector<std::thread> threads;
  for (int i = 0; i < n; ++i)
    threads.push_back (std::thread (
        [&]
          {
            arrived.fetch_add (1);
            barrier.arrive_and_wait ();
            if (arrived.load () != n)
              early.fetch_add (1);
          }));
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (early.load (), 0);
}

TEST (LumexTestThreadsTest,
      GivenABarrier_WhenUsedForSeveralRounds_ThenEveryRoundSynchronizes)
{
  int const n = 4;
  int const rounds = 50;
  lumex_test::StartBarrier barrier (n);
  std::atomic<int> counter (0);
  std::atomic<int> wrong (0);
  std::vector<std::thread> threads;
  for (int i = 0; i < n; ++i)
    threads.push_back (std::thread (
        [&]
          {
            for (int r = 1; r <= rounds; ++r)
              {
                counter.fetch_add (1);
                barrier.arrive_and_wait ();
                if (counter.load () != r * n)
                  wrong.fetch_add (1);
                barrier.arrive_and_wait ();
              }
          }));
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (wrong.load (), 0);
}

TEST (LumexTestThreadsTest,
      GivenAGate_WhenOpened_ThenWaitersLeaveAndItStaysOpen)
{
  lumex_test::Gate gate;
  EXPECT_FALSE (gate.is_open ());
  EXPECT_FALSE (gate.wait_for_ms (5));
  std::atomic<int> through (0);
  std::vector<std::thread> threads;
  for (int i = 0; i < 3; ++i)
    threads.push_back (std::thread (
        [&]
          {
            gate.wait ();
            through.fetch_add (1);
          }));
  gate.open ();
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (through.load (), 3);
  EXPECT_TRUE (gate.is_open ());
  EXPECT_TRUE (gate.wait_for_ms (1));
  gate.close ();
  EXPECT_FALSE (gate.is_open ());
}

TEST (LumexTestThreadsTest,
      GivenAStallPoint_WhenAThreadParks_ThenItWaitsForTheRelease)
{
  lumex_test::StallPoint point;
  point.arm (true);
  std::atomic<int> state (0);
  std::thread worker (
      [&]
        {
          state.store (1);
          EXPECT_TRUE (point.park ());
          state.store (2);
        });
  ASSERT_TRUE (point.wait_until_parked (1));
  EXPECT_EQ (point.parked (), 1);
  std::this_thread::sleep_for (std::chrono::milliseconds (20));
  EXPECT_EQ (state.load (), 1) << "the thread is held at the stall point";
  point.release ();
  ASSERT_TRUE (point.wait_until_passed (1));
  worker.join ();
  EXPECT_EQ (state.load (), 2);
  EXPECT_EQ (point.parked (), 0);
}

TEST (LumexTestThreadsTest,
      GivenADisarmedStallPoint_WhenParking_ThenItReturnsAtOnce)
{
  lumex_test::StallPoint point;
  EXPECT_TRUE (point.park ());
  point.arm (true);
  point.arm (false);
  EXPECT_TRUE (point.park ());
}

TEST (LumexTestThreadsTest,
      GivenAStallPointNeverReleased_WhenParking_ThenTheTimeoutReports)
{
  lumex_test::StallPoint point;
  point.arm (true);
  EXPECT_FALSE (point.park (30));
}

TEST (LumexTestThreadsTest,
      GivenARendezvous_WhenAllPartiesArrive_ThenMeetReturnsTrue)
{
  lumex_test::Rendezvous rendezvous (3);
  std::atomic<int> met (0);
  std::vector<std::thread> threads;
  for (int i = 0; i < 3; ++i)
    threads.push_back (std::thread (
        [&]
          {
            if (rendezvous.meet (10000))
              met.fetch_add (1);
          }));
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (met.load (), 3);
  EXPECT_EQ (rendezvous.arrived (), 3);
  rendezvous.reset ();
  EXPECT_EQ (rendezvous.arrived (), 0);
}

TEST (LumexTestThreadsTest,
      GivenARendezvous_WhenAPartnerNeverComes_ThenMeetGivesUp)
{
  lumex_test::Rendezvous rendezvous (2);
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  EXPECT_FALSE (rendezvous.meet (30));
  EXPECT_GE (std::chrono::steady_clock::now () - start,
             std::chrono::milliseconds (25));
}

TEST (LumexTestThreadsTest,
      GivenThreads_WhenRunThreads_ThenEachIndexRunsOnceAndErrorsAreReturned)
{
  std::vector<int> ran (5, 0);
  std::string error = lumex_test::run_threads (
      5, [&] (int i) { ran[static_cast<std::size_t> (i)] += 1; });
  EXPECT_TRUE (error.empty ());
  for (std::size_t i = 0; i < ran.size (); ++i)
    EXPECT_EQ (ran[i], 1);

  error = lumex_test::run_threads (3,
                                   [] (int i)
                                     {
                                       if (i == 1)
                                         throw std::runtime_error ("boom");
                                     });
  EXPECT_NE (error.find ("boom"), std::string::npos);
  EXPECT_NE (error.find ("thread 1"), std::string::npos);
}

TEST (LumexTestThreadsTest,
      GivenAVerdict_WhenFailed_ThenItKeepsTheMessagesAndTheCount)
{
  lumex_test::Verdict verdict;
  EXPECT_TRUE (verdict.ok ());
  verdict.require (true, "fine");
  EXPECT_TRUE (verdict.ok ());
  for (int i = 0; i < 10; ++i)
    verdict.fail ("problem " + std::to_string (i));
  EXPECT_FALSE (verdict.ok ());
  EXPECT_EQ (verdict.violations (), 10L);
  std::string const text = verdict.text ();
  EXPECT_NE (text.find ("10 violation"), std::string::npos);
  EXPECT_NE (text.find ("problem 0"), std::string::npos);
  EXPECT_EQ (text.find ("problem 9"), std::string::npos)
      << "only the first messages are kept";
  verdict.require (false, "required");
  EXPECT_EQ (verdict.violations (), 11L);
}

TEST (LumexTestThreadsTest,
      GivenAWatchdog_WhenDestroyedInTime_ThenNothingHappens)
{
  lumex_test::TestWatchdog const dog ("GivenAWatchdog");
  std::this_thread::sleep_for (std::chrono::milliseconds (5));
}

// --- the reuse-address allocator
// -------------------------------------------------

TEST (LumexTestReusePoolTest,
      GivenARelease_WhenAllocatingTheSameSize_ThenTheAddressComesBack)
{
  lumex_test::ReusePool pool;
  void *first = pool.allocate (48);
  void *second = pool.allocate (48);
  EXPECT_NE (first, second);
  pool.release (first, 48);
  pool.release (second, 48);
  EXPECT_EQ (pool.allocate (48), second) << "last in, first out";
  EXPECT_EQ (pool.allocate (48), first);
  EXPECT_EQ (pool.reuses (), 2L);
  EXPECT_EQ (pool.allocations (), 4L);
  EXPECT_EQ (pool.releases (), 2L);
  EXPECT_EQ (pool.live (), 2L);
  pool.release (first, 48);
  pool.release (second, 48);
  EXPECT_EQ (pool.leaked (), 0L);
}

TEST (LumexTestReusePoolTest,
      GivenDifferentSizes_WhenReleased_ThenEachSizeKeepsItsOwnList)
{
  lumex_test::ReusePool pool;
  void *small = pool.allocate (16);
  void *large = pool.allocate (64);
  pool.release (small, 16);
  void *other = pool.allocate (64);
  EXPECT_NE (other, small);
  EXPECT_NE (other, large);
  EXPECT_EQ (pool.allocate (16), small);
  pool.release (other, 64);
  pool.release (large, 64);
  pool.release (small, 16);
}

TEST (LumexTestReusePoolTest,
      GivenAReleasedBlock_WhenPoisoningIsOn_ThenItIsFilledWithThePattern)
{
#if LUMEX_TEST_HAS_ASAN
  GTEST_SKIP () << "under AddressSanitizer a released block is poisoned, "
                   "reading it is the report";
#else
  lumex_test::ReusePool pool;
  unsigned char *block = static_cast<unsigned char *> (pool.allocate (32));
  for (int i = 0; i < 32; ++i)
    block[i] = 0x11;
  pool.release (block, 32);
  for (int i = 0; i < 32; ++i)
    EXPECT_EQ (block[i],
               static_cast<unsigned char> (lumex_test::pool_poison_byte));
  unsigned char *again = static_cast<unsigned char *> (pool.allocate (32));
  EXPECT_EQ (again, block);
  pool.release (again, 32);

  pool.set_poisoning (false);
  again = static_cast<unsigned char *> (pool.allocate (32));
  for (int i = 0; i < 32; ++i)
    again[i] = 0x22;
  pool.release (again, 32);
  for (int i = 0; i < 32; ++i)
    EXPECT_EQ (again[i], 0x22) << "no pattern when poisoning is off";
#endif
}

TEST (
    LumexTestReusePoolTest,
    GivenAReleasedBlock_WhenItIsReadUnderAddressSanitizer_ThenTheChildIsStopped)
{
#if LUMEX_TEST_HAS_ASAN
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      []
        {
          lumex_test::ReusePool pool;
          volatile unsigned char *block
              = static_cast<unsigned char *> (pool.allocate (32));
          block[0] = 1;
          pool.release (const_cast<unsigned char *> (block), 32);
          return block[0] == 1 ? 0 : 0; // the read is the report
        });
  EXPECT_TRUE (lumex_test::child_was_caught (result))
      << "a read of a released block was not reported: "
      << lumex_test::describe_child (result);
  // A block that is allocated again is readable.
  lumex_test::ReusePool pool;
  unsigned char *block = static_cast<unsigned char *> (pool.allocate (32));
  pool.release (block, 32);
  unsigned char *again = static_cast<unsigned char *> (pool.allocate (32));
  EXPECT_EQ (again, block);
  again[0] = 2;
  EXPECT_EQ (again[0], 2);
  pool.release (again, 32);
#else
  GTEST_SKIP () << "needs AddressSanitizer; the other build reads the pattern "
                   "(GivenAReleasedBlock_WhenPoisoningIsOn_...)";
#endif
}

TEST (LumexTestReusePoolTest,
      GivenAHook_WhenAllocatingAndReleasing_ThenItSeesTheFourEventsInOrder)
{
  lumex_test::ReusePool pool;
  std::vector<int> events;
  std::vector<void *> addresses;
  pool.set_hook (
      [&] (lumex_test::PoolEvent event, void *address)
        {
          events.push_back (static_cast<int> (event));
          addresses.push_back (address);
        });
  void *block = pool.allocate (24);
  pool.release (block, 24);
  ASSERT_EQ (events.size (), 4u);
  EXPECT_EQ (events[0],
             static_cast<int> (lumex_test::PoolEvent::before_allocate));
  EXPECT_EQ (events[1],
             static_cast<int> (lumex_test::PoolEvent::after_allocate));
  EXPECT_EQ (events[2],
             static_cast<int> (lumex_test::PoolEvent::before_release));
  EXPECT_EQ (events[3],
             static_cast<int> (lumex_test::PoolEvent::after_release));
  EXPECT_EQ (addresses[0], static_cast<void *> (nullptr));
  EXPECT_EQ (addresses[1], block);
  EXPECT_EQ (addresses[2], block);
  EXPECT_EQ (addresses[3], block);
}

TEST (LumexTestReusePoolTest,
      GivenAllocateShared_WhenAnObjectDies_ThenTheNextObjectTakesItsAddress)
{
  lumex_test::ReusePool pool;
  void *first_address = nullptr;
  {
    std::shared_ptr<int> first = lumex_test::make_pooled<int> (pool, 5);
    first_address = first.get ();
    EXPECT_EQ (*first, 5);
  }
  EXPECT_EQ (pool.live (), 0L);
  std::shared_ptr<int> second = lumex_test::make_pooled<int> (pool, 6);
  EXPECT_EQ (static_cast<void *> (second.get ()), first_address)
      << "object and control block are reused at once";
  EXPECT_EQ (*second, 6);
  EXPECT_EQ (pool.reuses (), 1L);
  std::weak_ptr<int> weak (second);
  second.reset ();
  EXPECT_EQ (pool.live (), 1L) << "a weak reference keeps the block";
  weak.reset ();
  EXPECT_EQ (pool.live (), 0L);
}

TEST (LumexTestReusePoolTest,
      GivenManyThreads_WhenAllocatingAndReleasing_ThenNothingLeaks)
{
  lumex_test::ReusePool pool;
  std::string const error = lumex_test::run_threads (
      6,
      [&] (int)
        {
          for (int i = 0; i < 2000; ++i)
            {
              std::shared_ptr<int> p = lumex_test::make_pooled<int> (pool, i);
              if (*p != i)
                throw std::runtime_error ("payload clobbered");
            }
        });
  EXPECT_TRUE (error.empty ()) << error;
  EXPECT_EQ (pool.live (), 0L);
  EXPECT_EQ (pool.allocations (), pool.releases ());
  EXPECT_GT (pool.reuses (), 0L);
}

// --- the ledger
// -------------------------------------------------------------------

TEST (LumexTestLedgerTest,
      GivenEveryObjectDestroyedOnce_WhenChecked_ThenItIsBalanced)
{
  lumex_test::ObjectLedger ledger (8);
  {
    std::vector<std::unique_ptr<lumex_test::LedgerEntry>> entries;
    for (int i = 0; i < 5; ++i)
      entries.push_back (std::unique_ptr<lumex_test::LedgerEntry> (
          new lumex_test::LedgerEntry (ledger)));
    EXPECT_EQ (ledger.constructed (), 5u);
    EXPECT_EQ (ledger.alive (), 5u);
    EXPECT_FALSE (ledger.balanced ()) << "five are still alive";
    for (std::size_t i = 0; i < entries.size (); ++i)
      EXPECT_EQ (entries[i]->id (), i);
  }
  EXPECT_EQ (ledger.alive (), 0u);
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexTestLedgerTest,
      GivenALeakAndADoubleDestruction_WhenChecked_ThenBothAreNamed)
{
  lumex_test::ObjectLedger ledger (4);
  std::size_t const a = ledger.construct ();
  std::size_t const b = ledger.construct ();
  std::size_t const c = ledger.construct ();
  ledger.destroy (a);
  ledger.destroy (b);
  ledger.destroy (b); // twice
  EXPECT_EQ (c, 2u);
  EXPECT_EQ (ledger.destroyed_twice (), 1u);
  EXPECT_EQ (ledger.never_destroyed (), 1u);
  EXPECT_EQ (ledger.destructions (), 3u);
  EXPECT_EQ (ledger.alive (), 0u)
      << "one leak and one extra release cancel in a bare counter";
  EXPECT_FALSE (ledger.balanced ()) << "the ledger is not fooled by that";
  EXPECT_NE (ledger.report ().find ("destroyed_twice=1"), std::string::npos);
}

TEST (LumexTestLedgerTest,
      GivenAnExtraDestruction_WhenEveryIdWasDestroyed_ThenItIsNotBalanced)
{
  lumex_test::ObjectLedger ledger (2);
  std::size_t const a = ledger.construct ();
  std::size_t const b = ledger.construct ();
  ledger.destroy (a);
  ledger.destroy (b);
  EXPECT_TRUE (ledger.balanced ());
  ledger.destroy (a); // one release too many
  EXPECT_EQ (ledger.destroyed_twice (), 1u);
  EXPECT_EQ (ledger.never_destroyed (), 0u);
  EXPECT_FALSE (ledger.balanced ());
}

TEST (LumexTestLedgerTest,
      GivenMoreObjectsThanTheCapacity_WhenChecked_ThenItIsNotBalanced)
{
  lumex_test::ObjectLedger ledger (2);
  for (int i = 0; i < 3; ++i)
    ledger.destroy (ledger.construct ());
  EXPECT_EQ (ledger.overflow (), 1u);
  EXPECT_FALSE (ledger.balanced ());
}

TEST (LumexTestLedgerTest, GivenAnEntry_WhenDestroyed_ThenItsCanaryChanges)
{
  lumex_test::ObjectLedger ledger (1);
  void *storage = ::operator new (sizeof (lumex_test::LedgerEntry));
  lumex_test::LedgerEntry *entry
      = new (storage) lumex_test::LedgerEntry (ledger);
  EXPECT_TRUE (entry->intact ());
  entry->~LedgerEntry ();
  EXPECT_FALSE (entry->intact ()) << "a destroyed object is not intact";
  ::operator delete (storage);
}

TEST (LumexTestLedgerTest,
      GivenManyThreads_WhenConstructingAndDestroying_ThenTheIdsAreUnique)
{
  lumex_test::ObjectLedger ledger (6 * 500);
  std::string const error = lumex_test::run_threads (
      6,
      [&] (int)
        {
          for (int i = 0; i < 500; ++i)
            {
              lumex_test::LedgerEntry entry (ledger);
              if (!entry.intact ())
                throw std::runtime_error ("not intact");
            }
        });
  EXPECT_TRUE (error.empty ());
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
  EXPECT_EQ (ledger.constructed (), 3000u);
}

// --- the history and the Wing and Gong checker
// ------------------------------------

namespace
{
using lumex_test::Linearizability;
using lumex_test::RegisterModel;

Linearizability
check_register (std::vector<lumex_test::HistoryOp> const &history,
                std::int64_t initial)
{
  return lumex_test::is_linearizable<RegisterModel> (history, initial);
}
} // namespace

TEST (LumexTestHistoryTest,
      GivenASequentialRegisterHistory_WhenChecked_ThenItIsLinearizable)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_store, 1, 2, 5, 0, 0, 0));
  h.push_back (op (0, lumex_test::register_load, 3, 4, 0, 0, 5, 0));
  h.push_back (op (0, lumex_test::register_exchange, 5, 6, 7, 0, 5, 0));
  h.push_back (op (0, lumex_test::register_cas_strong, 7, 8, 7, 9, 1, 0));
  h.push_back (op (0, lumex_test::register_load, 9, 10, 0, 0, 9, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable);
}

TEST (LumexTestHistoryTest,
      GivenASequentialHistoryWithAWrongLoad_WhenChecked_ThenItIsRejected)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_store, 1, 2, 5, 0, 0, 0));
  h.push_back (op (0, lumex_test::register_load, 3, 4, 0, 0, 6, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable);
}

TEST (LumexTestHistoryTest,
      GivenOverlappingOperations_WhenEitherOrderWorks_ThenItIsLinearizable)
{
  // store 1 overlaps a load that returns the old value 0: the load can be
  // ordered first.
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_store, 1, 10, 1, 0, 0, 0));
  h.push_back (op (1, lumex_test::register_load, 2, 5, 0, 0, 0, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable);
  // the load may also return the new value
  h[1].res0 = 1;
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable);
}

TEST (LumexTestHistoryTest,
      GivenALoadThatMissesAFinishedStore_WhenChecked_ThenItIsRejected)
{
  // The load starts after the store responded and still sees the old value:
  // a stale read.
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_store, 1, 2, 1, 0, 0, 0));
  h.push_back (op (1, lumex_test::register_load, 3, 4, 0, 0, 0, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable);
}

TEST (
    LumexTestHistoryTest,
    GivenTwoCompareExchangesFromTheSameValue_WhenBothSucceed_ThenItIsRejected)
{
  // Both threads expect 0 and both report success: a lost update.
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_cas_strong, 1, 10, 0, 1, 1, 0));
  h.push_back (op (1, lumex_test::register_cas_strong, 2, 9, 0, 2, 1, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable);
  // One of them failing and reporting the other's value is fine.
  h[1].res0 = 0;
  h[1].res1 = 1;
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable);
}

TEST (
    LumexTestHistoryTest,
    GivenAStrongFailureWithEqualValues_WhenChecked_ThenItIsRejectedButAWeakOneAccepted)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_cas_strong, 1, 2, 0, 1, 0, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable)
      << "a strong compare-exchange never fails spuriously";
  h[0].kind = lumex_test::register_cas_weak;
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable)
      << "a weak one may";
  h[0].res1 = 3;
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable)
      << "but it must report the real value";
}

TEST (LumexTestHistoryTest,
      GivenExchangesThatBothReturnTheSameValue_WhenChecked_ThenItIsRejected)
{
  // exchange chain 0 -> 1 -> 2: the second must return 1; returning 0 twice
  // duplicates a value.
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::register_exchange, 1, 10, 1, 0, 0, 0));
  h.push_back (op (1, lumex_test::register_exchange, 2, 11, 2, 0, 0, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::not_linearizable);
  h[1].res0 = 1;
  EXPECT_EQ (check_register (h, 0), Linearizability::linearizable);
}

TEST (LumexTestHistoryTest, GivenAStackHistory_WhenChecked_ThenLifoIsRequired)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::stack_push, 1, 2, 1, 0, 0, 0));
  h.push_back (op (0, lumex_test::stack_push, 3, 4, 2, 0, 0, 0));
  h.push_back (op (1, lumex_test::stack_pop, 5, 6, 0, 0, 2, 0));
  h.push_back (op (1, lumex_test::stack_pop, 7, 8, 0, 0, 1, 0));
  h.push_back (op (1, lumex_test::stack_pop, 9, 10, 0, 0, -1, 0));
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::StackModel> (
                 h, lumex_test::StackModel::state_type ()),
             Linearizability::linearizable);
  h[2].res0 = 1; // FIFO order is not a stack
  h[3].res0 = 2;
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::StackModel> (
                 h, lumex_test::StackModel::state_type ()),
             Linearizability::not_linearizable);
}

TEST (LumexTestHistoryTest, GivenAQueueHistory_WhenChecked_ThenFifoIsRequired)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::queue_enqueue, 1, 2, 1, 0, 0, 0));
  h.push_back (op (0, lumex_test::queue_enqueue, 3, 4, 2, 0, 0, 0));
  h.push_back (op (1, lumex_test::queue_dequeue, 5, 6, 0, 0, 1, 0));
  h.push_back (op (1, lumex_test::queue_dequeue, 7, 8, 0, 0, 2, 0));
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::QueueModel> (
                 h, lumex_test::QueueModel::state_type ()),
             Linearizability::linearizable);
  h[2].res0 = 2;
  h[3].res0 = 1;
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::QueueModel> (
                 h, lumex_test::QueueModel::state_type ()),
             Linearizability::not_linearizable);
}

TEST (LumexTestHistoryTest,
      GivenASetHistory_WhenChecked_ThenMembershipIsRequired)
{
  std::vector<lumex_test::HistoryOp> h;
  h.push_back (op (0, lumex_test::set_insert, 1, 2, 3, 0, 1, 0));
  h.push_back (op (0, lumex_test::set_insert, 3, 4, 3, 0, 0, 0)); // already in
  h.push_back (op (1, lumex_test::set_contains, 5, 6, 3, 0, 1, 0));
  h.push_back (op (1, lumex_test::set_remove, 7, 8, 3, 0, 1, 0));
  h.push_back (
      op (1, lumex_test::set_remove, 9, 10, 3, 0, 0, 0)); // already out
  h.push_back (op (0, lumex_test::set_contains, 11, 12, 4, 0, 0, 0));
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::SetModel> (h, 0),
             Linearizability::linearizable);
  h[1].res0 = 1; // a second successful insert of the same key
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::SetModel> (h, 0),
             Linearizability::not_linearizable);
  h[1].res0 = 0;
  h[2].res0 = 0; // contains misses a key inserted and not yet removed
  EXPECT_EQ (lumex_test::is_linearizable<lumex_test::SetModel> (h, 0),
             Linearizability::not_linearizable);
  EXPECT_NE (
      lumex_test::describe_history<lumex_test::SetModel> (h).find ("insert 3"),
      std::string::npos);
}

TEST (LumexTestHistoryTest,
      GivenATooLongHistory_WhenChecked_ThenItIsReportedAsTooLong)
{
  std::vector<lumex_test::HistoryOp> h;
  for (int i = 0; i < 63; ++i)
    h.push_back (op (0, lumex_test::register_load,
                     2u * static_cast<unsigned> (i) + 1,
                     2u * static_cast<unsigned> (i) + 2, 0, 0, 0, 0));
  EXPECT_EQ (check_register (h, 0), Linearizability::too_long);
}

TEST (LumexTestHistoryTest,
      GivenAnEmptyHistory_WhenChecked_ThenItIsLinearizable)
{
  EXPECT_EQ (check_register (std::vector<lumex_test::HistoryOp> (), 0),
             Linearizability::linearizable);
}

TEST (LumexTestHistoryTest,
      GivenARecorder_WhenThreadsRecord_ThenTicketsOrderTheOperations)
{
  lumex_test::HistoryRecorder recorder (3);
  std::string const error = lumex_test::run_threads (
      3,
      [&] (int t)
        {
          for (int i = 0; i < 20; ++i)
            {
              std::uint64_t const invoke = recorder.begin ();
              recorder.end (t, lumex_test::register_load, invoke, 0, 0, i, 0);
            }
        });
  EXPECT_TRUE (error.empty ());
  std::vector<lumex_test::HistoryOp> const history = recorder.history ();
  ASSERT_EQ (history.size (), 60u);
  std::set<std::uint64_t> tickets;
  for (std::size_t i = 0; i < history.size (); ++i)
    {
      EXPECT_LT (history[i].invoke, history[i].respond);
      tickets.insert (history[i].invoke);
      tickets.insert (history[i].respond);
      if (i > 0)
        {
          EXPECT_LE (history[i - 1].invoke, history[i].invoke);
        }
    }
  EXPECT_EQ (tickets.size (), 120u) << "every ticket is taken once";
  EXPECT_NE (
      lumex_test::describe_history<RegisterModel> (history).find ("load"),
      std::string::npos);
}

// --- the subprocess runner
// -----------------------------------------------------------

TEST (LumexTestSubprocessTest,
      GivenAChild_WhenItEndsInEachWay_ThenTheResultTellsHow)
{
  if (!lumex_test::subprocess_available ())
    {
      lumex_test::ChildResult const none
          = lumex_test::run_in_child ([] { return 0; });
      EXPECT_EQ (none.end, lumex_test::ChildEnd::unavailable);
      EXPECT_FALSE (lumex_test::child_was_caught (none));
      return;
    }
  lumex_test::ChildResult result = lumex_test::run_in_child ([] { return 0; });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::clean);
  EXPECT_FALSE (lumex_test::child_was_caught (result));

  result = lumex_test::run_in_child ([] { return 7; });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::failed_exit);
  EXPECT_EQ (result.code, 7);
  EXPECT_TRUE (lumex_test::child_was_caught (result));

  result = lumex_test::run_in_child ([] () -> int { throw 1; });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::failed_exit);
  EXPECT_EQ (result.code, 99);

  result = lumex_test::run_in_child ([] () -> int { std::abort (); });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled);
  EXPECT_TRUE (lumex_test::child_was_caught (result));
  EXPECT_NE (lumex_test::describe_child (result).find ("signal"),
             std::string::npos);
}

TEST (LumexTestSubprocessTest,
      GivenAChildThatHangs_WhenTheAlarmFires_ThenItIsKilled)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      [] () -> int
        {
          for (;;)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        },
      1);
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled);
}

TEST (LumexTestSubprocessTest,
      GivenThePlatform_WhenAsked_ThenAvailabilityMatchesFork)
{
#if defined(__unix__) || defined(__APPLE__)
  EXPECT_TRUE (lumex_test::subprocess_available ());
#else
  EXPECT_FALSE (lumex_test::subprocess_available ());
#endif
}

TEST (LumexTestReplayTest,
      GivenAPassingTest_WhenTheNoteDies_ThenNothingIsPrinted)
{
  testing::internal::CaptureStderr ();
  {
    lumex_test::ReplayNote const note ("Replay.Pass", 1, 2);
  }
  EXPECT_TRUE (testing::internal::GetCapturedStderr ().empty ());
}

TEST (LumexTestReplayTest,
      GivenAScenario_WhenTheLineIsBuilt_ThenItNamesTheSeedAndTheThreads)
{
  EXPECT_EQ (
      lumex_test::replay_line ("Aba.Stress", 20261008, 8),
      "REPLAY Aba.Stress: LUMEX_TEST_SEED=20261008 LUMEX_TEST_THREADS=8");
}

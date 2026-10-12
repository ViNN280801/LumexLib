// Concurrency of atomic_shared_ptr and atomic_weak_ptr. The first two cases
// port util.smartptr.shared.atomic/atomic_shared_ptr_stress.pass.cpp of
// llvm-project pull request 194215 (the author's own libc++ implementation of
// P0718R2); the others count objects, deletions and control blocks, so they
// also catch a leaked or doubly released reference, and they run the access
// patterns that exposed the compare-exchange livelock and the reference leak
// of the lock-free libc++ variant. Random choices use a fixed seed
// (LUMEX_ATOMIC_STRESS_SEED overrides it); thread counts come from
// LUMEX_ATOMIC_STRESS_THREADS (default 2,4,8) and the amount of work from
// LUMEX_ATOMIC_STRESS_SCALE (default 1). Races are not deterministic: run the
// binary several times with several thread counts before trusting a pass.

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
int const candidate_count = 4;

/// Deleter that loads from the atomic object that may have held the pointer.
struct LoadingDeleter
{
  atomic_shared_ptr<int> *watch;
  std::atomic<long> *broken;

  void
  operator() (int *p) const
  {
    std::shared_ptr<int> const current = watch->load ();
    if (current && *current < 0)
      broken->fetch_add (1);
    delete p;
  }
};
} // namespace

// --- atomic_shared_ptr_stress.pass.cpp
// ----------------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenWritersAndReaders_WhenSharingAValue_ThenReadersSeeOnlyStoredOnes)
{
  Watchdog const dog ("GivenWritersAndReaders_WhenSharingAValue");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const writers = std::max (1, counts[c] / 2);
      int const readers = std::max (1, counts[c] - writers);
      int const iterations = stress_iterations (4000);
      std::vector<std::shared_ptr<int>> pool;
      for (int i = 0; i < candidate_count; ++i)
        pool.push_back (std::make_shared<int> (i + 1));
      {
        atomic_shared_ptr<int> a (pool[0]);
        std::atomic<bool> stop (false);
        std::atomic<long> bad (0);
        std::vector<std::thread> writer_threads;
        std::vector<std::thread> reader_threads;
        for (int w = 0; w < writers; ++w)
          writer_threads.push_back (std::thread (
              [&, w]
                {
                  for (int i = 0; i < iterations; ++i)
                    a.store (pool[static_cast<std::size_t> (
                        (w + i) % candidate_count)]);
                }));
        for (int r = 0; r < readers; ++r)
          reader_threads.push_back (std::thread (
              [&]
                {
                  while (!stop.load (std::memory_order_relaxed))
                    {
                      std::shared_ptr<int> const v = a.load ();
                      if (!v || *v < 1 || *v > candidate_count)
                        bad.fetch_add (1);
                      std::this_thread::yield ();
                    }
                }));
        join_all (writer_threads);
        stop.store (true, std::memory_order_relaxed);
        join_all (reader_threads);
        EXPECT_EQ (bad.load (), 0L) << "threads=" << counts[c];
        // A replaced value that a reader was copying when it was replaced is
        // destroyed by a later pass of a lock-free engine: settle it before
        // the exact count.
        EngineUnderTest::quiesce ();
        long total = 0;
        for (int i = 0; i < candidate_count; ++i)
          total += pool[static_cast<std::size_t> (i)].use_count ();
        EXPECT_EQ (total, candidate_count + 1L)
            << "the pool plus the one stored owner";
      }
      for (int i = 0; i < candidate_count; ++i)
        EXPECT_EQ (pool[static_cast<std::size_t> (i)].use_count (), 1L);
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenWritersAndReaders_WhenSharingAWeakValue_ThenReadersSeeStoredOnes)
{
  Watchdog const dog ("GivenWritersAndReaders_WhenSharingAWeakValue");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const writers = std::max (1, counts[c] / 2);
      int const readers = std::max (1, counts[c] - writers);
      int const iterations = stress_iterations (4000);
      std::vector<std::shared_ptr<int>> pool;
      for (int i = 0; i < candidate_count; ++i)
        pool.push_back (std::make_shared<int> (i + 1));
      atomic_weak_ptr<int> a ((std::weak_ptr<int> (pool[0])));
      std::atomic<bool> stop (false);
      std::atomic<long> bad (0);
      std::vector<std::thread> writer_threads;
      std::vector<std::thread> reader_threads;
      for (int w = 0; w < writers; ++w)
        writer_threads.push_back (std::thread (
            [&, w]
              {
                for (int i = 0; i < iterations; ++i)
                  a.store (std::weak_ptr<int> (pool[static_cast<std::size_t> (
                      (w + i) % candidate_count)]));
              }));
      for (int r = 0; r < readers; ++r)
        reader_threads.push_back (std::thread (
            [&]
              {
                while (!stop.load (std::memory_order_relaxed))
                  {
                    std::shared_ptr<int> const v = a.load ().lock ();
                    if (!v || *v < 1 || *v > candidate_count)
                      bad.fetch_add (1);
                    std::this_thread::yield ();
                  }
              }));
      join_all (writer_threads);
      stop.store (true, std::memory_order_relaxed);
      join_all (reader_threads);
      EXPECT_EQ (bad.load (), 0L) << "threads=" << counts[c];
      for (int i = 0; i < candidate_count; ++i)
        EXPECT_EQ (pool[static_cast<std::size_t> (i)].use_count (), 1L);
    }
}

// --- every operation at once, seeded
// ---------------------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenSeededMixedOperations_WhenRunConcurrently_ThenEveryCountBalances)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenSeededMixedOperations_WhenRunConcurrently");
  std::uint32_t const seed = stress_seed ();
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const threads_count = counts[c];
      int const iterations = stress_iterations (3000);
      long const allocations_before = live_allocations ().load ();
      int const alive_before = Tracker::alive ().load ();
      std::atomic<long> broken (0);
      {
        atomic_shared_ptr<Tracker> a (make_counted_tracker (0));
        atomic_weak_ptr<Tracker> w ((std::weak_ptr<Tracker> (a.load ())));
        std::vector<std::thread> threads;
        for (int t = 0; t < threads_count; ++t)
          threads.push_back (std::thread (
              [&, t]
                {
                  Random random (seed + static_cast<std::uint32_t> (t));
                  for (int i = 0; i < iterations; ++i)
                    {
                      int const value = t * iterations + i;
                      switch (random.next () % 9u)
                        {
                        case 0:
                          a.store (make_counted_tracker (value));
                          break;
                        case 1:
                          {
                            std::shared_ptr<Tracker> const old
                                = a.exchange (make_counted_tracker (value));
                            if (old && !old->intact ())
                              broken.fetch_add (1);
                            break;
                          }
                        case 2:
                          {
                            std::shared_ptr<Tracker> expected = a.load ();
                            a.compare_exchange_strong (
                                expected, make_counted_tracker (value));
                            if (expected && !expected->intact ())
                              broken.fetch_add (1);
                            break;
                          }
                        case 3:
                          {
                            std::shared_ptr<Tracker> expected = a.load ();
                            for (int k = 0; k < 4; ++k)
                              if (a.compare_exchange_weak (
                                      expected, make_counted_tracker (value),
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire))
                                break;
                            break;
                          }
                        case 4:
                          {
                            std::shared_ptr<Tracker> const v = a.load ();
                            if (v && !v->intact ())
                              broken.fetch_add (1);
                            w.store (std::weak_ptr<Tracker> (v));
                            break;
                          }
                        case 5:
                          {
                            std::shared_ptr<Tracker> const v
                                = w.load ().lock ();
                            if (v && !v->intact ())
                              broken.fetch_add (1);
                            break;
                          }
                        case 6:
                          {
                            std::weak_ptr<Tracker> expected = w.load ();
                            w.compare_exchange_strong (
                                expected, std::weak_ptr<Tracker> (a.load ()));
                            break;
                          }
                        case 7:
                          {
                            std::weak_ptr<Tracker> const old = w.exchange (
                                std::weak_ptr<Tracker> (a.load ()),
                                std::memory_order_acq_rel);
                            std::shared_ptr<Tracker> const v = old.lock ();
                            if (v && !v->intact ())
                              broken.fetch_add (1);
                            break;
                          }
                        default:
                          a.notify_all ();
                          w.notify_one ();
                          break;
                        }
                    }
                }));
        join_all (threads);
      }
      EXPECT_EQ (broken.load (), 0L)
          << "threads=" << threads_count << " seed=" << seed;
      EXPECT_EQ (Tracker::alive ().load (), alive_before)
          << "threads=" << threads_count << " seed=" << seed;
      EXPECT_EQ (live_allocations ().load (), allocations_before)
          << "threads=" << threads_count << " seed=" << seed;
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenLedgerObjects_WhenReplacedConcurrently_ThenEachIsDeletedOnce)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenLedgerObjects_WhenReplacedConcurrently");
  std::uint32_t const seed = stress_seed ();
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (1500);
      DeletionLedger ledger (
          static_cast<std::size_t> (counts[c]) * iterations * 2 + 1);
      {
        atomic_shared_ptr<int> a (ledger.make ());
        std::vector<std::thread> threads;
        for (int t = 0; t < counts[c]; ++t)
          threads.push_back (std::thread (
              [&, t]
                {
                  Random random (seed + 31u * static_cast<std::uint32_t> (t));
                  for (int i = 0; i < iterations; ++i)
                    switch (random.next () % 3u)
                      {
                      case 0:
                        a.store (ledger.make ());
                        break;
                      case 1:
                        {
                          std::shared_ptr<int> const old
                              = a.exchange (ledger.make ());
                          break;
                        }
                      default:
                        {
                          std::shared_ptr<int> expected = a.load ();
                          a.compare_exchange_strong (expected, ledger.make ());
                          break;
                        }
                      }
                }));
        join_all (threads);
      }
      EXPECT_EQ (ledger.deleted_twice (), 0u)
          << "threads=" << counts[c] << " seed=" << seed;
      EXPECT_EQ (ledger.not_deleted (), 0u)
          << "threads=" << counts[c] << " seed=" << seed;
    }
}

// --- the livelock and leak regressions of the libc++ investigation
// ------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenEveryThreadLoadingThenComparing_WhenContended_ThenProgressIsMade)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  // The contended compare-exchange pattern of the libc++ benchmarks: every
  // thread loads, then swaps between two owners with a strong
  // compare-exchange. It made the lock-free libc++ variant livelock (a
  // compare-exchange that waited for in-flight loads never found the
  // moment). Each failed attempt of one thread needs a success of another
  // thread inside its window, so there are at least `iterations` successes;
  // the watchdog turns a livelock into a failure.
  Watchdog const dog ("GivenEveryThreadLoadingThenComparing_WhenContended");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (5000);
      std::shared_ptr<Tracker> const keep_a = make_counted_tracker (1);
      std::shared_ptr<Tracker> const keep_b = make_counted_tracker (2);
      std::atomic<long> swaps (0);
      {
        atomic_shared_ptr<Tracker> a (keep_a);
        std::vector<std::thread> threads;
        for (int t = 0; t < counts[c]; ++t)
          threads.push_back (std::thread (
              [&]
                {
                  for (int i = 0; i < iterations; ++i)
                    {
                      std::shared_ptr<Tracker> expected
                          = a.load (std::memory_order_relaxed);
                      std::shared_ptr<Tracker> const desired
                          = expected == keep_a ? keep_b : keep_a;
                      if (a.compare_exchange_strong (expected, desired))
                        swaps.fetch_add (1);
                    }
                }));
        join_all (threads);
        std::shared_ptr<Tracker> const last = a.load ();
        EXPECT_TRUE (last == keep_a || last == keep_b);
      }
      EXPECT_GE (swaps.load (), static_cast<long> (iterations))
          << "threads=" << counts[c];
      EXPECT_EQ (keep_a.use_count (), 1L) << "threads=" << counts[c];
      EXPECT_EQ (keep_b.use_count (), 1L) << "threads=" << counts[c];
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenLoadersBesideCompareExchangeSwappers_WhenContended_ThenBalanced)
{
  Watchdog const dog ("GivenLoadersBesideCompareExchangeSwappers");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const swappers = std::max (1, counts[c] / 2);
      int const loaders = std::max (1, counts[c] - swappers);
      int const iterations = stress_iterations (5000);
      std::shared_ptr<Tracker> const keep_a = make_counted_tracker (1);
      std::shared_ptr<Tracker> const keep_b = make_counted_tracker (2);
      std::atomic<long> bad (0);
      std::atomic<long> swaps (0);
      {
        atomic_shared_ptr<Tracker> a (keep_a);
        std::atomic<bool> stop (false);
        std::vector<std::thread> swapper_threads;
        std::vector<std::thread> loader_threads;
        for (int s = 0; s < swappers; ++s)
          swapper_threads.push_back (std::thread (
              [&]
                {
                  for (int i = 0; i < iterations; ++i)
                    {
                      std::shared_ptr<Tracker> expected = a.load ();
                      std::shared_ptr<Tracker> const desired
                          = expected == keep_a ? keep_b : keep_a;
                      if (a.compare_exchange_strong (expected, desired))
                        swaps.fetch_add (1);
                    }
                }));
        for (int l = 0; l < loaders; ++l)
          loader_threads.push_back (std::thread (
              [&]
                {
                  while (!stop.load (std::memory_order_relaxed))
                    {
                      std::shared_ptr<Tracker> const v = a.load ();
                      if (v != keep_a && v != keep_b)
                        bad.fetch_add (1);
                    }
                }));
        join_all (swapper_threads);
        stop.store (true);
        join_all (loader_threads);
        EngineUnderTest::quiesce (); // see the test above
        EXPECT_EQ (keep_a.use_count () + keep_b.use_count (), 3L)
            << "two locals plus the one stored owner";
      }
      EXPECT_EQ (bad.load (), 0L);
      EXPECT_GE (swaps.load (), static_cast<long> (iterations));
      EXPECT_EQ (keep_a.use_count (), 1L);
      EXPECT_EQ (keep_b.use_count (), 1L);
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenOneWriterAlternatingOwnersUnderReaders_WhenDone_ThenAllAreReleased)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  // The readers-heavy pattern in which the lock-free libc++ variant leaked
  // one reference per control block. Nothing outside the atomic keeps the
  // objects at the end, so every object and every control block must be gone
  // once the atomic is destroyed.
  Watchdog const dog ("GivenOneWriterAlternatingOwnersUnderReaders");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const readers = std::max (1, counts[c] - 1);
      int const iterations = stress_iterations (6000);
      int const alive_before = Tracker::alive ().load ();
      long const allocations_before = live_allocations ().load ();
      std::atomic<long> broken (0);
      {
        atomic_shared_ptr<Tracker> a (make_counted_tracker (0));
        std::atomic<bool> stop (false);
        std::vector<std::thread> reader_threads;
        for (int r = 0; r < readers; ++r)
          reader_threads.push_back (std::thread (
              [&]
                {
                  while (!stop.load (std::memory_order_relaxed))
                    {
                      std::shared_ptr<Tracker> const v = a.load ();
                      if (!v || !v->intact ())
                        broken.fetch_add (1);
                    }
                }));
        {
          std::shared_ptr<Tracker> owner_a = make_counted_tracker (1);
          std::shared_ptr<Tracker> owner_b = make_counted_tracker (2);
          for (int i = 0; i < iterations; ++i)
            a.store ((i & 1) == 0 ? owner_a : owner_b);
          owner_a.reset ();
          owner_b.reset ();
          for (int i = 0; i < iterations; ++i)
            a.store (make_counted_tracker (i));
        }
        stop.store (true);
        join_all (reader_threads);
      }
      EXPECT_EQ (broken.load (), 0L) << "threads=" << counts[c];
      EXPECT_EQ (Tracker::alive ().load (), alive_before)
          << "threads=" << counts[c];
      EXPECT_EQ (live_allocations ().load (), allocations_before)
          << "threads=" << counts[c];
    }
}

// --- ABA
// -------------------------------------------------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenAStaleExpectedAfterABA_WhenCompareExchange_ThenIdentityDecides)
{
  // A held expected keeps its object alive, so a new object can never take
  // its address: A -> B -> A succeeds, A -> B -> A' (a new object) fails.
  std::shared_ptr<int> const a_value = std::make_shared<int> (1);
  atomic_shared_ptr<int> a (a_value);

  std::shared_ptr<int> stale = a.load ();
  a.store (std::make_shared<int> (2));
  a.store (a_value);
  EXPECT_TRUE (a.compare_exchange_strong (stale, std::make_shared<int> (3)))
      << "the same object came back";

  std::shared_ptr<int> stale_again = a.load ();
  a.store (std::make_shared<int> (4));
  a.store (std::make_shared<int> (3)); // equal value, new object
  EXPECT_FALSE (
      a.compare_exchange_strong (stale_again, std::make_shared<int> (5)));
  EXPECT_EQ (*stale_again, 3);
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenThreadsCyclingThreeOwners_WhenComparingWithStaleValues_ThenBalanced)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenThreadsCyclingThreeOwners");
  std::uint32_t const seed = stress_seed ();
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (4000);
      std::vector<std::shared_ptr<Tracker>> owners;
      for (int i = 0; i < 3; ++i)
        owners.push_back (make_counted_tracker (i));
      std::atomic<long> successes (0);
      {
        atomic_shared_ptr<Tracker> a (owners[0]);
        std::vector<std::thread> threads;
        for (int t = 0; t < counts[c]; ++t)
          threads.push_back (std::thread (
              [&, t]
                {
                  Random random (seed + 7u * static_cast<std::uint32_t> (t));
                  std::shared_ptr<Tracker> stale = a.load ();
                  for (int i = 0; i < iterations; ++i)
                    {
                      std::shared_ptr<Tracker> const next
                          = owners[random.next () % 3u];
                      // A deliberately stale expected: ABA makes it succeed
                      // now and then, and a failure refreshes it.
                      if (a.compare_exchange_strong (stale, next))
                        {
                          successes.fetch_add (1);
                          stale = next;
                        }
                    }
                }));
        join_all (threads);
      }
      EXPECT_GT (successes.load (), 0L);
      for (int i = 0; i < 3; ++i)
        EXPECT_EQ (owners[static_cast<std::size_t> (i)].use_count (), 1L)
            << "threads=" << counts[c] << " seed=" << seed;
    }
}

// --- weak pointers and dying objects
// ------------------------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenObjectsDyingWhileLocked_WhenLoadingWeakly_ThenLocksAreIntactOrEmpty)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  // The last owner dies at a seeded moment while readers keep locking the
  // weak pointer: every lock is either empty or an intact live object.
  Watchdog const dog ("GivenObjectsDyingWhileLocked");
  std::uint32_t const seed = stress_seed ();
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const rounds = stress_iterations (200);
      int const readers = std::max (1, counts[c] - 1);
      long const allocations_before = live_allocations ().load ();
      std::atomic<long> broken (0);
      std::atomic<long> empties (0);
      Random random (seed + static_cast<std::uint32_t> (counts[c]));
      for (int r = 0; r < rounds; ++r)
        {
          std::shared_ptr<Tracker> owner = make_counted_tracker (r);
          atomic_weak_ptr<Tracker> w ((std::weak_ptr<Tracker> (owner)));
          std::atomic<bool> stop (false);
          std::vector<std::thread> threads;
          for (int t = 0; t < readers; ++t)
            threads.push_back (std::thread (
                [&]
                  {
                    while (!stop.load (std::memory_order_relaxed))
                      {
                        std::shared_ptr<Tracker> const v = w.load ().lock ();
                        if (!v)
                          empties.fetch_add (1);
                        else if (!v->intact ())
                          broken.fetch_add (1);
                        std::weak_ptr<Tracker> expected = w.load ();
                        w.compare_exchange_weak (expected, expected);
                      }
                  }));
          for (std::uint32_t spin = random.next () % 200u; spin > 0; --spin)
            std::this_thread::yield ();
          owner.reset (); // the last owner dies while the readers lock
          for (std::uint32_t spin = random.next () % 50u; spin > 0; --spin)
            std::this_thread::yield ();
          stop.store (true);
          join_all (threads);
          EXPECT_TRUE (w.load ().expired ());
        }
      EXPECT_EQ (broken.load (), 0L)
          << "threads=" << counts[c] << " seed=" << seed;
      EXPECT_EQ (live_allocations ().load (), allocations_before);
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenExpiringTargets_WhenWeakCompareExchangeRuns_ThenBlocksAreFreed)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenExpiringTargets_WhenWeakCompareExchangeRuns");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const producers = std::max (1, counts[c] / 2);
      int const consumers = std::max (1, counts[c] - producers);
      int const iterations = stress_iterations (3000);
      long const allocations_before = live_allocations ().load ();
      std::atomic<long> broken (0);
      {
        atomic_weak_ptr<Tracker> w;
        std::atomic<bool> stop (false);
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        for (int p = 0; p < producers; ++p)
          producer_threads.push_back (std::thread (
              [&, p]
                {
                  for (int i = 0; i < iterations; ++i)
                    {
                      // The object dies at the end of the iteration, so the
                      // stored weak pointer expires while others use it.
                      std::shared_ptr<Tracker> const object
                          = make_counted_tracker (p * iterations + i);
                      w.store (std::weak_ptr<Tracker> (object));
                      if ((i & 3) == 0)
                        w.notify_all ();
                    }
                }));
        for (int q = 0; q < consumers; ++q)
          consumer_threads.push_back (std::thread (
              [&]
                {
                  while (!stop.load (std::memory_order_relaxed))
                    {
                      std::shared_ptr<Tracker> const v = w.load ().lock ();
                      if (v && !v->intact ())
                        broken.fetch_add (1);
                      std::weak_ptr<Tracker> expected = w.load ();
                      w.compare_exchange_strong (expected,
                                                 std::weak_ptr<Tracker> ());
                    }
                }));
        join_all (producer_threads);
        stop.store (true);
        join_all (consumer_threads);
      }
      EXPECT_EQ (broken.load (), 0L) << "threads=" << counts[c];
      EXPECT_EQ (live_allocations ().load (), allocations_before)
          << "threads=" << counts[c];
    }
}

// --- deleters that use the atomic, under contention
// ------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenDeletersThatUseTheAtomic_WhenReleasedConcurrently_ThenNoDeadlock)
{
  Watchdog const dog (
      "GivenDeletersThatUseTheAtomic_WhenReleasedConcurrently");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (2000);
      std::atomic<long> broken (0);
      atomic_shared_ptr<int> a;
      LoadingDeleter const deleter = { &a, &broken };
      std::vector<std::thread> threads;
      for (int t = 0; t < counts[c]; ++t)
        threads.push_back (std::thread (
            [&, t]
              {
                for (int i = 0; i < iterations; ++i)
                  {
                    std::shared_ptr<int> const value (new int (t + i),
                                                      deleter);
                    if ((i & 1) == 0)
                      a.store (value);
                    else
                      {
                        std::shared_ptr<int> expected = a.load ();
                        a.compare_exchange_strong (expected, value);
                      }
                  }
              }));
      join_all (threads);
      a.store (nullptr);
      EXPECT_EQ (broken.load (), 0L);
    }
}

// --- producers and consumers with wait and notify
// ----------------------------------

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenAProducerWithNotifyAll_WhenConsumersWait_ThenTheyFollowEveryVersion)
{
  Watchdog const dog ("GivenAProducerWithNotifyAll_WhenConsumersWait");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const consumers = std::max (1, counts[c] - 1);
      int const versions = stress_iterations (500);
      atomic_shared_ptr<int> a (std::make_shared<int> (0));
      std::atomic<long> non_monotonic (0);
      std::vector<std::thread> consumer_threads;
      for (int q = 0; q < consumers; ++q)
        consumer_threads.push_back (std::thread (
            [&]
              {
                int last = -1;
                for (;;)
                  {
                    std::shared_ptr<int> const current = a.load ();
                    if (*current < last)
                      non_monotonic.fetch_add (1);
                    last = *current;
                    if (*current == versions)
                      break;
                    a.wait (current);
                  }
              }));
      for (int v = 1; v <= versions; ++v)
        {
          a.store (std::make_shared<int> (v));
          a.notify_all ();
          if (v % 50 == 0)
            sleep_ms (1); // let the consumers fall asleep now and then
        }
      join_all (consumer_threads);
      EXPECT_EQ (non_monotonic.load (), 0L);
    }
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenAProducerWithNotifyOne_WhenConsumersWait_ThenEveryOneFinishes)
{
  Watchdog const dog ("GivenAProducerWithNotifyOne_WhenConsumersWait");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const consumers = std::max (1, counts[c] - 1);
      int const versions = stress_iterations (300);
      std::vector<std::shared_ptr<int>> keep;
      keep.push_back (std::make_shared<int> (0));
      atomic_weak_ptr<int> a ((std::weak_ptr<int> (keep.back ())));
      std::atomic<int> done (0);
      std::vector<std::thread> consumer_threads;
      for (int q = 0; q < consumers; ++q)
        consumer_threads.push_back (std::thread (
            [&]
              {
                for (;;)
                  {
                    std::weak_ptr<int> const current = a.load ();
                    std::shared_ptr<int> const locked = current.lock ();
                    if (locked && *locked == versions)
                      break;
                    a.wait (current);
                  }
                done.fetch_add (1);
              }));
      for (int v = 1; v <= versions; ++v)
        {
          keep.push_back (std::make_shared<int> (v));
          a.store (std::weak_ptr<int> (keep.back ()));
          a.notify_one ();
          if (v % 50 == 0)
            sleep_ms (1);
        }
      // notify_one wakes at least one sleeper per call; keep calling until
      // every consumer has seen the final version.
      while (done.load () < consumers)
        {
          a.notify_one ();
          std::this_thread::yield ();
        }
      join_all (consumer_threads);
      EXPECT_EQ (done.load (), consumers);
    }
}

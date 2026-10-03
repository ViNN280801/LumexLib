// The waiting layer under the lock-based atomic smart pointers: the
// address-keyed wait (std::atomic::wait from C++20, the striped mutex and
// condition variable table otherwise or when forced) and the two-bit lock.
// The lock tests include the lost wake-up found in the author's own libc++
// implementation (llvm-project pull request 194215): a woken thread that
// finds the lock taken again must set the sleeper bit again before it
// sleeps.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/sync/LumexAtomicWait.hpp"
#include "lumex/core/atomic/sync/LumexBitLock.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace sync_detail = lumex::core::atomic::sync::Detail;

TEST (LumexAtomicWaitTest,
      GivenTheWaitSelection_WhenNamingTheLock_ThenTheAbiNamespaceMatches)
{
#if LUMEX_ATOMIC_WAIT_USES_STD
  static_assert (
      std::is_same<
          sync_detail::BitLock,
          lumex::core::atomic::sync::std_wait::Detail::BitLock>::value,
      "std::atomic::wait");
#else
  static_assert (
      std::is_same<
          sync_detail::BitLock,
          lumex::core::atomic::sync::table_wait::Detail::BitLock>::value,
      "striped table");
  std::atomic<std::uint32_t> word (0u);
  EXPECT_EQ (&sync_detail::stripe_for (&word),
             &sync_detail::stripe_for (&word))
      << "a word always maps to the same stripe";
#endif
  SUCCEED ();
}

TEST (LumexAtomicWaitTest, GivenCpuRelax_WhenCalledRepeatedly_ThenItReturns)
{
  for (int i = 0; i < 1000; ++i)
    sync_detail::cpu_relax ();
  static_assert (noexcept (sync_detail::cpu_relax ()), "noexcept");
  SUCCEED ();
}

TEST (LumexAtomicWaitTest,
      GivenAChangedWord_WhenWaitingUntilChanged_ThenReturnsAtOnce)
{
  Watchdog const dog ("GivenAChangedWord_WhenWaitingUntilChanged");
  std::atomic<std::uint32_t> word (5u);
  sync_detail::wait_until_changed (word, 4u);
  sync_detail::wait_until_changed (word, 0u);
  word.store (0xffffffffu);
  sync_detail::wait_until_changed (word, 0u);
  SUCCEED ();
}

TEST (LumexAtomicWaitTest, GivenSleepers_WhenNotifyAll_ThenEveryOneWakes)
{
  Watchdog const dog ("GivenSleepers_WhenNotifyAll");
  std::atomic<std::uint32_t> word (0u);
  std::atomic<int> started (0);
  std::atomic<int> woke (0);
  std::vector<std::thread> sleepers;
  for (int i = 0; i < 3; ++i)
    sleepers.push_back (std::thread (
        [&]
          {
            started.fetch_add (1);
            sync_detail::wait_until_changed (word, 0u);
            woke.fetch_add (1);
          }));
  wait_for_count (started, 3);
  sleep_ms (20);
  EXPECT_EQ (woke.load (), 0);
  word.store (1u);
  sync_detail::notify_all (word);
  join_all (sleepers);
  EXPECT_EQ (woke.load (), 3);
}

TEST (LumexAtomicWaitTest, GivenASleeper_WhenNotifyOne_ThenItWakes)
{
  Watchdog const dog ("GivenASleeper_WhenNotifyOne");
  std::atomic<std::uint32_t> word (0u);
  std::atomic<bool> started (false);
  std::thread sleeper (
      [&]
        {
          started.store (true);
          sync_detail::wait_until_changed (word, 0u);
        });
  wait_for_flag (started);
  sleep_ms (10);
  word.store (7u);
  sync_detail::notify_one (word);
  sleeper.join ();
  EXPECT_EQ (word.load (), 7u);
}

TEST (LumexAtomicWaitTest,
      GivenNotifiesWithoutAChange_WhenSleeping_ThenTheSleeperStays)
{
  Watchdog const dog ("GivenNotifiesWithoutAChange_WhenSleeping");
  std::atomic<std::uint32_t> word (0u);
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread sleeper (
      [&]
        {
          started.store (true);
          sync_detail::wait_until_changed (word, 0u);
          woke.store (true);
        });
  wait_for_flag (started);
  for (int i = 0; i < 50; ++i)
    {
      sync_detail::notify_all (word);
      sync_detail::notify_one (word);
      std::this_thread::yield ();
    }
  sleep_ms (20);
  EXPECT_FALSE (woke.load ());
  word.store (1u);
  sync_detail::notify_all (word);
  sleeper.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicWaitTest,
      GivenMoreWordsThanStripes_WhenWokenOneByOne_ThenOnlyChangedOnesWake)
{
  // More words than stripes: several words share a stripe, and waking one
  // word must neither lose nor finish the sleepers of the others.
  Watchdog const dog ("GivenMoreWordsThanStripes_WhenWokenOneByOne");
  int const word_count = 96;
  std::unique_ptr<std::atomic<std::uint32_t>[]> words (
      new std::atomic<std::uint32_t>[word_count]);
  std::unique_ptr<std::atomic<bool>[]> woke (
      new std::atomic<bool>[word_count]);
  for (int i = 0; i < word_count; ++i)
    {
      words[i].store (0u);
      woke[i].store (false);
    }
  std::atomic<int> started (0);
  std::vector<std::thread> sleepers;
  for (int i = 0; i < word_count; ++i)
    sleepers.push_back (std::thread (
        [&, i]
          {
            started.fetch_add (1);
            sync_detail::wait_until_changed (words[i], 0u);
            woke[i].store (true);
          }));
  wait_for_count (started, word_count);
  sleep_ms (20);
  for (int i = word_count - 1; i >= 0; i -= 2)
    {
      words[i].store (1u);
      sync_detail::notify_one (words[i]);
    }
  for (int i = word_count - 1; i >= 0; i -= 2)
    while (!woke[i].load ())
      std::this_thread::yield ();
  for (int i = word_count - 2; i >= 0; i -= 2)
    EXPECT_FALSE (woke[i].load ()) << "word " << i << " was not changed";
  for (int i = word_count - 2; i >= 0; i -= 2)
    {
      words[i].store (1u);
      sync_detail::notify_all (words[i]);
    }
  join_all (sleepers);
}

TEST (LumexBitLockTest,
      GivenAReleasedLock_WhenLockedAndUnlocked_ThenTheLockBitToggles)
{
  sync_detail::BitLock lock;
  EXPECT_EQ (lock.state (), 0u);
  lock.lock ();
  EXPECT_EQ (lock.state (),
             static_cast<std::uint32_t> (sync_detail::BitLock::lock_bit));
  lock.unlock ();
  EXPECT_EQ (lock.state (), 0u);

  static_assert (
      std::is_nothrow_default_constructible<sync_detail::BitLock>::value, "");
  static_assert (!std::is_copy_constructible<sync_detail::BitLock>::value, "");
  static_assert (!std::is_copy_assignable<sync_detail::BitLock>::value, "");
  static_assert (noexcept (lock.lock ()), "noexcept");
  static_assert (noexcept (lock.unlock ()), "noexcept");
}

TEST (LumexBitLockTest, GivenAGuard_WhenItsScopeEnds_ThenTheLockIsReleased)
{
  sync_detail::BitLock const lock;
  {
    sync_detail::BitLockGuard const guard (lock);
    EXPECT_NE (lock.state () & sync_detail::BitLock::lock_bit, 0u);
  }
  EXPECT_EQ (lock.state (), 0u);
}

TEST (LumexBitLockTest,
      GivenAHeldLock_WhenAContenderSleeps_ThenItSetsTheNotifyBitAndWakes)
{
  Watchdog const dog ("GivenAHeldLock_WhenAContenderSleeps");
  sync_detail::BitLock lock;
  lock.lock ();
  std::atomic<bool> acquired (false);
  std::thread contender (
      [&]
        {
          lock.lock ();
          acquired.store (true);
          lock.unlock ();
        });
  std::uint32_t const armed
      = sync_detail::BitLock::lock_bit | sync_detail::BitLock::notify_bit;
  std::chrono::steady_clock::time_point const deadline
      = std::chrono::steady_clock::now () + std::chrono::seconds (30);
  while (lock.state () != armed
         && std::chrono::steady_clock::now () < deadline)
    sleep_ms (1);
  EXPECT_EQ (lock.state (), armed) << "the contender sleeps with the bit set";
  EXPECT_FALSE (acquired.load ());
  lock.unlock ();
  contender.join ();
  EXPECT_TRUE (acquired.load ());
  EXPECT_EQ (lock.state (), 0u) << "the release clears both bits";
}

TEST (LumexBitLockTest,
      GivenManyThreads_WhenIncrementingUnderTheLock_ThenNoIncrementIsLost)
{
  Watchdog const dog ("GivenManyThreads_WhenIncrementingUnderTheLock");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (20000);
      sync_detail::BitLock lock;
      long counter = 0; // plain: only the lock protects it
      std::vector<std::thread> threads;
      for (int t = 0; t < counts[c]; ++t)
        threads.push_back (std::thread (
            [&]
              {
                for (int i = 0; i < iterations; ++i)
                  {
                    sync_detail::BitLockGuard const guard (lock);
                    ++counter;
                  }
              }));
      join_all (threads);
      EXPECT_EQ (counter, static_cast<long> (counts[c]) * iterations);
      EXPECT_EQ (lock.state (), 0u);
    }
}

TEST (LumexBitLockTest,
      GivenSleepingContenders_WhenTheLockChangesHands_ThenNoneSleepsForever)
{
  // The holder sleeps inside the lock now and then, so contenders run out of
  // spin rounds and sleep; with three or more threads, a woken thread often
  // finds the lock taken by a third one. Sleeping again without setting the
  // sleeper bit leaves it asleep forever, which the watchdog reports.
  Watchdog const dog ("GivenSleepingContenders_WhenTheLockChangesHands");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const threads_count = std::max (3, counts[c]);
      int const iterations = stress_iterations (400);
      sync_detail::BitLock lock;
      long counter = 0;
      std::vector<std::thread> threads;
      for (int t = 0; t < threads_count; ++t)
        threads.push_back (std::thread (
            [&, t]
              {
                for (int i = 0; i < iterations; ++i)
                  {
                    sync_detail::BitLockGuard const guard (lock);
                    ++counter;
                    if ((i + t) % 16 == 0)
                      std::this_thread::sleep_for (
                          std::chrono::microseconds (200));
                  }
              }));
      join_all (threads);
      EXPECT_EQ (counter, static_cast<long> (threads_count) * iterations);
      EXPECT_EQ (lock.state (), 0u);
    }
}

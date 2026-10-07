// wait / notify_one / notify_all of atomic_shared_ptr and atomic_weak_ptr:
// what ends a wait (a notified change of the stored pointer or of the owner)
// and what does not (a store without notify, a notify without a change, a
// change back to the old value, the expiry of a weakly referenced object),
// many waiters, and a notification that arrives between the value check of
// wait and its sleep, which must not be lost. libstdc++ 13's own
// std::atomic<std::shared_ptr<T>>::wait fails two of these (it returns
// without a change and misses a change of the stored pointer alone).

#include <atomic>
#include <cstddef>
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
struct Pair
{
  int a;
  int b;
};

int g_shared_int = 5;
} // namespace

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAStoreWithoutNotify_WhenWaiting_ThenTheWaiterKeepsSleeping)
{
  Watchdog const dog ("GivenAStoreWithoutNotify_WhenWaiting");
  std::shared_ptr<int> const first = std::make_shared<int> (1);
  atomic_shared_ptr<int> a (first);
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (first);
          woke.store (true);
        });
  wait_for_flag (started);
  sleep_ms (10);
  a.store (std::make_shared<int> (2)); // as in the standard: no wake-up
  sleep_ms (30);
  EXPECT_FALSE (woke.load ()) << "store alone does not notify";
  a.notify_one ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenNotifiesWithoutAChange_WhenWaiting_ThenTheWaiterKeepsSleeping)
{
  // libstdc++ 13's own wait returns here.
  Watchdog const dog ("GivenNotifiesWithoutAChange_WhenWaiting");
  std::shared_ptr<int> const p = std::make_shared<int> (1);
  atomic_shared_ptr<int> a (p);
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (p);
          woke.store (true);
        });
  wait_for_flag (started);
  for (int i = 0; i < 300; ++i)
    {
      std::shared_ptr<int> const loaded = a.load ();
      a.store (loaded); // an equivalent value
      if ((i & 1) == 0)
        a.notify_all ();
      else
        a.notify_one ();
      std::this_thread::yield ();
    }
  sleep_ms (20);
  EXPECT_FALSE (woke.load ()) << "wait returned while the value was unchanged";
  a.store (std::make_shared<int> (2));
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAChangeOfTheStoredPointerAlone_WhenNotified_ThenTheWaiterWakes)
{
  // Same owner, another stored pointer (aliasing): libstdc++ 13's own wait
  // never wakes for this change.
  Watchdog const dog ("GivenAChangeOfTheStoredPointerAlone_WhenNotified");
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  atomic_shared_ptr<int> a (view_a);
  std::atomic<bool> started (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (view_a);
        });
  wait_for_flag (started);
  sleep_ms (20);
  a.store (view_b);
  a.notify_all ();
  waiter.join ();
  EXPECT_EQ (a.load ().get (), &owner->b);
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAChangeOfTheOwnerAlone_WhenNotified_ThenTheWaiterWakes)
{
  Watchdog const dog ("GivenAChangeOfTheOwnerAlone_WhenNotified");
  std::shared_ptr<int> const owner1 = std::make_shared<int> (0);
  std::shared_ptr<int> const owner2 = std::make_shared<int> (0);
  std::shared_ptr<int> const v1 (owner1, &g_shared_int);
  std::shared_ptr<int> const v2 (owner2, &g_shared_int);
  atomic_shared_ptr<int> a (v1);
  std::atomic<bool> started (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (v1);
        });
  wait_for_flag (started);
  sleep_ms (20);
  a.store (v2);
  a.notify_one ();
  waiter.join ();
  EXPECT_TRUE (same_owner_and_pointer (a.load (), v2));
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAnEmptyValue_WhenWaitingForNullptr_ThenSleepsUntilAValueArrives)
{
  Watchdog const dog ("GivenAnEmptyValue_WhenWaitingForNullptr");
  atomic_shared_ptr<int> a;
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (nullptr);
          woke.store (true);
        });
  wait_for_flag (started);
  sleep_ms (20);
  a.notify_all ();
  sleep_ms (10);
  EXPECT_FALSE (woke.load ());
  a.store (std::make_shared<int> (1));
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAChangeBackToTheOldValue_WhenNotified_ThenTheWaiterSleepsOn)
{
  // ABA during a wait: the value is equivalent to the old one again by the
  // time the waiter looks, so it goes back to sleep, as the standard allows.
  Watchdog const dog ("GivenAChangeBackToTheOldValue_WhenNotified");
  std::shared_ptr<int> const a_value = std::make_shared<int> (1);
  atomic_shared_ptr<int> a (a_value);
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (a_value);
          woke.store (true);
        });
  wait_for_flag (started);
  sleep_ms (10);
  a.store (std::make_shared<int> (2));
  a.store (a_value);
  a.notify_all ();
  sleep_ms (30);
  EXPECT_FALSE (woke.load ());
  a.store (std::make_shared<int> (3));
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenEveryLoadOrder_WhenWaitingForAChange_ThenTheWaiterWakes)
{
  Watchdog const dog ("GivenEveryLoadOrder_WhenWaitingForAChange");
  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      atomic_shared_ptr<int> a (std::make_shared<int> (0));
      std::atomic<bool> started (false);
      std::memory_order const order = orders[i];
      std::thread waiter (
          [&a, &started, order]
            {
              std::shared_ptr<int> const old = a.load ();
              started.store (true);
              a.wait (old, order);
            });
      wait_for_flag (started);
      sleep_ms (5);
      a.store (std::make_shared<int> (1));
      a.notify_all ();
      waiter.join ();
    }
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenManyWaiters_WhenNotifyAll_ThenEveryOneWakes)
{
  Watchdog const dog ("GivenManyWaiters_WhenNotifyAll");
  int const waiter_count = 32;
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::atomic<int> ready (0);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            std::shared_ptr<int> const old = a.load ();
            ready.fetch_add (1);
            a.wait (old);
            woke.fetch_add (1);
          }));
  wait_for_count (ready, waiter_count);
  sleep_ms (20);
  a.store (std::make_shared<int> (1));
  a.notify_all ();
  join_all (waiters);
  EXPECT_EQ (woke.load (), waiter_count);
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenSeveralWaiters_WhenNotifyOneRepeatedly_ThenEveryOneEventuallyWakes)
{
  Watchdog const dog ("GivenSeveralWaiters_WhenNotifyOneRepeatedly");
  int const waiter_count = 6;
  atomic_shared_ptr<int> a (std::make_shared<int> (1));
  std::atomic<int> ready (0);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            std::shared_ptr<int> const old = a.load ();
            ready.fetch_add (1);
            a.wait (old);
            woke.fetch_add (1);
          }));
  wait_for_count (ready, waiter_count);
  sleep_ms (10);
  a.store (std::make_shared<int> (2));
  while (woke.load () < waiter_count)
    {
      a.notify_one ();
      std::this_thread::yield ();
    }
  join_all (waiters);
  EXPECT_EQ (woke.load (), waiter_count);
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenANotifyBetweenCheckAndSleep_WhenPingPonging_ThenNoWakeUpIsLost)
{
  // Each round the consumer takes the old value, tells the producer, and
  // waits; the producer changes the value and notifies once, at once. The
  // notification therefore often lands between the value check of wait and
  // its sleep. A lost one leaves the consumer asleep for good, because the
  // producer does not notify again before the consumer acknowledges.
  Watchdog const dog ("GivenANotifyBetweenCheckAndSleep_WhenPingPonging");
  int const rounds = stress_iterations (3000);
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::atomic<int> acknowledged (-1);
  std::atomic<long> mismatches (0);
  std::thread consumer (
      [&]
        {
          for (int r = 1; r <= rounds; ++r)
            {
              std::shared_ptr<int> const old = a.load ();
              acknowledged.store (r - 1, std::memory_order_release);
              a.wait (old);
              if (*a.load () != r)
                mismatches.fetch_add (1);
            }
        });
  for (int r = 1; r <= rounds; ++r)
    {
      while (acknowledged.load (std::memory_order_acquire) != r - 1)
        std::this_thread::yield ();
      a.store (std::make_shared<int> (r));
      if ((r & 1) == 0)
        a.notify_one ();
      else
        a.notify_all ();
    }
  consumer.join ();
  EXPECT_EQ (mismatches.load (), 0L);
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAWeakNotifyBetweenCheckAndSleep_WhenPingPonging_ThenNoneIsLost)
{
  Watchdog const dog ("GivenAWeakNotifyBetweenCheckAndSleep_WhenPingPonging");
  int const rounds = stress_iterations (2000);
  std::vector<std::shared_ptr<int>> values;
  for (int r = 0; r <= rounds; ++r)
    values.push_back (std::make_shared<int> (r));
  atomic_weak_ptr<int> a ((std::weak_ptr<int> (values[0])));
  std::atomic<int> acknowledged (-1);
  std::atomic<long> mismatches (0);
  std::thread consumer (
      [&]
        {
          for (int r = 1; r <= rounds; ++r)
            {
              std::weak_ptr<int> const old = a.load ();
              acknowledged.store (r - 1, std::memory_order_release);
              a.wait (old);
              std::shared_ptr<int> const now = a.load ().lock ();
              if (!now || *now != r)
                mismatches.fetch_add (1);
            }
        });
  for (int r = 1; r <= rounds; ++r)
    {
      while (acknowledged.load (std::memory_order_acquire) != r - 1)
        std::this_thread::yield ();
      a.store (std::weak_ptr<int> (values[static_cast<std::size_t> (r)]));
      a.notify_one ();
    }
  consumer.join ();
  EXPECT_EQ (mismatches.load (), 0L);
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAWeaklyReferencedObjectThatExpires_WhenNotified_ThenWaitContinues)
{
  Watchdog const dog ("GivenAWeaklyReferencedObjectThatExpires");
  std::shared_ptr<int> owner = std::make_shared<int> (1);
  std::weak_ptr<int> const watched = owner;
  atomic_weak_ptr<int> a (watched);
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (watched);
          woke.store (true);
        });
  wait_for_flag (started);
  sleep_ms (10);
  owner.reset (); // the stored weak pointer expires; the value is unchanged
  a.notify_all ();
  sleep_ms (20);
  EXPECT_FALSE (woke.load ());
  std::shared_ptr<int> const other = std::make_shared<int> (2);
  a.store (std::weak_ptr<int> (other));
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAWaitOnAnEmptyWeakPointer_WhenAValueArrives_ThenTheWaiterWakes)
{
  Watchdog const dog ("GivenAWaitOnAnEmptyWeakPointer");
  atomic_weak_ptr<int> a;
  std::shared_ptr<int> const value = std::make_shared<int> (3);
  std::atomic<bool> started (false);
  std::thread waiter (
      [&]
        {
          started.store (true);
          a.wait (std::weak_ptr<int> ());
        });
  wait_for_flag (started);
  sleep_ms (10);
  a.store (std::weak_ptr<int> (value));
  a.notify_one ();
  waiter.join ();
  EXPECT_TRUE (refers_to (a.load (), value));
}

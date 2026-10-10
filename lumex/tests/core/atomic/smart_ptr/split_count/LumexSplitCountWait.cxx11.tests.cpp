/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// wait and notify of the split-count engine: `wait (old)` returns only when
// the stored value is not equivalent to `old` AND a notify has followed the
// change (a modifying operation wakes nobody); a notify without a change, or
// a change to an equivalent value, leaves the waiter asleep. The same sources
// run in the variant that forces the wait table (C++20 executable), where the
// waiting goes through the table instead of the futex word.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_shared_ptr_lock_free_split_count<int> shared_atomic;
typedef asp::atomic_weak_ptr_lock_free_split_count<int> weak_atomic;

void
settle_a_while ()
{
  std::this_thread::sleep_for (std::chrono::milliseconds (80));
}

/// Waiters on one atomic object; the destructor wakes and joins them.
template <typename Atomic, typename Value> class waiters_t
{
public:
  waiters_t (Atomic &a, Value const &old, Value const &release_value)
      : a_ (a), old_ (old), release_ (release_value), woke_ (0), started_ (0),
        threads_ ()
  {
  }

  ~waiters_t ()
  {
    a_.store (release_);
    a_.notify_all ();
    for (std::size_t i = 0; i < threads_.size (); ++i)
      threads_[i].join ();
  }

  void
  start (int count)
  {
    for (int i = 0; i < count; ++i)
      threads_.push_back (std::thread (
          [this]
            {
              started_.fetch_add (1);
              a_.wait (old_);
              woke_.fetch_add (1);
            }));
    spin_until ([this, count] { return started_.load () >= count; });
    settle_a_while ();
  }

  int
  woke () const
  {
    return woke_.load ();
  }

  bool
  woke_within (int count, int milliseconds) const
  {
    return spin_until ([this, count] { return woke_.load () >= count; },
                       milliseconds);
  }

private:
  Atomic &a_;
  Value old_;
  Value release_;
  std::atomic<int> woke_;
  std::atomic<int> started_;
  std::vector<std::thread> threads_;
};

TEST (LumexSplitCountWaitTest,
      GivenADifferentValue_WhenWaiting_ThenItReturnsAtOnce)
{
  Watchdog const dog ("wait returns");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  a.wait (q);
  a.wait (sp::shared_ptr<Obj> ());
  weak_atomic w;
  sp::shared_ptr<int> i = sp::make_shared<int> (1);
  w.wait (sp::weak_ptr<int> (i));
}

TEST (
    LumexSplitCountWaitTest,
    GivenAWaiter_WhenTheValueChangesWithoutNotify_ThenItStaysAsleepUntilNotified)
{
  Watchdog const dog ("no wake without notify");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<Obj>,
            sp::shared_ptr<Obj>>
      waiters (a, p, q);
  waiters.start (1);
  a.store (q);
  EXPECT_FALSE (waiters.woke_within (1, 150))
      << "a store does not wake a waiter";
  a.notify_one ();
  EXPECT_TRUE (waiters.woke_within (1, 20000));
}

TEST (LumexSplitCountWaitTest,
      GivenAWaiter_WhenNotifiedWithoutAChange_ThenItGoesBackToSleep)
{
  Watchdog const dog ("notify without change");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<Obj>,
            sp::shared_ptr<Obj>>
      waiters (a, p, q);
  waiters.start (1);
  a.notify_all ();
  a.notify_one ();
  EXPECT_FALSE (waiters.woke_within (1, 150));
  a.store (q);
  a.notify_all ();
  EXPECT_TRUE (waiters.woke_within (1, 20000));
}

TEST (LumexSplitCountWaitTest,
      GivenAWaiter_WhenAnEquivalentValueIsStored_ThenItStaysAsleep)
{
  Watchdog const dog ("equivalent store");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<Obj>,
            sp::shared_ptr<Obj>>
      waiters (a, p, q);
  waiters.start (1);
  a.store (p);
  a.notify_all ();
  EXPECT_FALSE (waiters.woke_within (1, 150))
      << "the same pointer and owner is equivalent";
  a.store (q);
  a.notify_all ();
  EXPECT_TRUE (waiters.woke_within (1, 20000));
}

/// Waits on @p old and changes the value to @p changed: the waiter wakes.
template <typename Value>
void
changed_value_wakes (Value const &old, Value const &changed,
                     Value const &release)
{
  Watchdog const dog ("aliasing change");
  asp::atomic_shared_ptr_lock_free_split_count<int> a (old);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<int>, Value> waiters (
      a, old, release);
  waiters.start (2);
  a.store (changed);
  a.notify_all ();
  EXPECT_TRUE (waiters.woke_within (2, 20000));
}

TEST (LumexSplitCountWaitTest,
      GivenWaitersOnAnAlias_WhenTheSameOwnerStoresAnotherPointer_ThenTheyWake)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const first (owner, &owner->v);
  sp::shared_ptr<int> const second (owner, &owner->pad[1]);
  changed_value_wakes<sp::shared_ptr<int>> (first, second,
                                            sp::shared_ptr<int> ());
}

TEST (
    LumexSplitCountWaitTest,
    GivenWaitersOnAHolderWord_WhenTheSameOwnerStoresAnotherFarPointer_ThenTheyWake)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  std::int64_t const far = std::int64_t (1) << 41;
  sp::shared_ptr<int> const first (owner, shifted (owner.get (), far));
  sp::shared_ptr<int> const second (owner, shifted (owner.get (), far + 8));
  changed_value_wakes<sp::shared_ptr<int>> (first, second,
                                            sp::shared_ptr<int> ());
}

TEST (LumexSplitCountWaitTest,
      GivenWaitersOnAHolderWord_WhenAnInWindowAliasIsStored_ThenTheyWake)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const first (
      owner, shifted (owner.get (), std::int64_t (1) << 41));
  sp::shared_ptr<int> const second (owner, &owner->v);
  changed_value_wakes<sp::shared_ptr<int>> (first, second,
                                            sp::shared_ptr<int> ());
}

TEST (LumexSplitCountWaitTest, GivenWaiters_WhenNotifyAll_ThenEveryOneWakes)
{
  Watchdog const dog ("notify_all");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<Obj>,
            sp::shared_ptr<Obj>>
      waiters (a, p, q);
  waiters.start (4);
  a.store (q);
  a.notify_all ();
  EXPECT_TRUE (waiters.woke_within (4, 20000));
}

TEST (LumexSplitCountWaitTest,
      GivenTwoWaiters_WhenNotifyOne_ThenAtLeastOneWakes)
{
  Watchdog const dog ("notify_one");
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  asp::atomic_shared_ptr_lock_free_split_count<Obj> a (p);
  waiters_t<asp::atomic_shared_ptr_lock_free_split_count<Obj>,
            sp::shared_ptr<Obj>>
      waiters (a, p, q);
  waiters.start (2);
  a.store (q);
  a.notify_one ();
  EXPECT_TRUE (waiters.woke_within (1, 20000));
  a.notify_all ();
  EXPECT_TRUE (waiters.woke_within (2, 20000));
}

TEST (LumexSplitCountWaitTest,
      GivenAWeakWaiter_WhenTheValueChangesAndNotify_ThenItWakesAndNotBefore)
{
  Watchdog const dog ("weak wait");
  sp::shared_ptr<int> p = sp::make_shared<int> (1);
  sp::shared_ptr<int> q = sp::make_shared<int> (2);
  sp::weak_ptr<int> const pw (p);
  sp::weak_ptr<int> const qw (q);
  weak_atomic a (pw);
  waiters_t<weak_atomic, sp::weak_ptr<int>> waiters (a, pw, qw);
  waiters.start (1);
  a.store (pw);
  a.notify_all ();
  EXPECT_FALSE (waiters.woke_within (1, 150)) << "an equivalent weak pointer";
  a.store (qw);
  EXPECT_FALSE (waiters.woke_within (1, 150)) << "no notify yet";
  a.notify_one ();
  EXPECT_TRUE (waiters.woke_within (1, 20000));
}
} // namespace

#else

TEST (LumexSplitCountWaitTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

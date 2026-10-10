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

// Deleters that use the atomic object they are destroyed from. The engine
// runs a user deleter only from the read-modify-write that makes a counter
// zero, after the word was updated, with no pin and no deposit of the running
// thread on that block, so the deleter may call `load`, `store`, `exchange`
// and `compare_exchange_*` on the same atomic object. Every place where an
// object can die inside a call is covered: the drop after a store, the drop
// of a holder, the replacement of `expected` by a failed compare-exchange,
// the retirement of `desired`, the one-step payment of a writer whose block
// was swapped out (S5), the destructor of the atomic object; and a loader
// held by the gate, which must not destroy anything before it returns.

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountHold.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountScenarios.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

/// An object whose destruction runs a hook (a deleter that uses the atomic).
struct reentrant_t
{
  explicit reentrant_t (int x) : v (x), on_destroy ()
  {
    alive_objects ().fetch_add (1);
  }

  reentrant_t (reentrant_t const &) = delete;
  reentrant_t &operator= (reentrant_t const &) = delete;

  ~reentrant_t ()
  {
    if (on_destroy)
      on_destroy ();
    alive_objects ().fetch_sub (1);
  }

  int v;
  std::function<void ()> on_destroy;
};

typedef sp::shared_ptr<reentrant_t> rptr;

rptr
make_r (int x)
{
  return sp::make_shared<reentrant_t> (x);
}

/// A far alias of @p owner as a `rptr` (stored through a holder).
rptr
far_r (rptr const &owner)
{
  return rptr (owner, reinterpret_cast<reentrant_t *> (
                          reinterpret_cast<std::uintptr_t> (owner.get ())
                          + (std::uintptr_t (1) << 41)));
}

struct tag_a
{
};
struct tag_b
{
};
struct tag_c
{
};
struct tag_d
{
};

/// The hook of the scenarios: calls every operation on @p a with @p r and
/// leaves @p r in the slot.
template <typename Atomic>
std::function<void ()>
reenter_with (Atomic &a, rptr const &r, std::atomic<int> &calls)
{
  return [&a, r, &calls]
    {
      calls.fetch_add (1);
      rptr seen = a.load ();
      a.store (r);
      rptr old = a.exchange (r);
      rptr expected = r;
      EXPECT_TRUE (a.compare_exchange_strong (expected, r));
      expected = seen;
      a.compare_exchange_weak (expected, r);
      a.store (r);
    };
}

template <typename Policy>
void
store_runs_the_deleter (bool holder)
{
  reset_ledger<Policy> ();
  long const alive = alive_objects ().load ();
  {
    rptr r = make_r (3);
    rptr q = make_r (2);
    std::atomic<int> calls (0);
    shared_cell<reentrant_t, Policy> a;
    {
      rptr p = make_r (1);
      p->on_destroy = reenter_with (a, r, calls);
      a.store (holder ? far_r (p) : p);
    }
    EXPECT_EQ (calls.load (), 0) << "the slot owns it";
    a.store (q);
    EXPECT_EQ (calls.load (), 1) << "the deleter ran once, inside store";
    EXPECT_EQ (a.load ().get (), r.get ()) << "and its store won";
    expect_exact (q, 1, "q was replaced by the deleter's store");
    expect_exact (r, 2, "r: r and the slot; the hook died with its object");
    check_balanced<Policy> ("deleter in store");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenStoreDestroysTheValue_ThenItMayCallEveryOperation)
{
  Watchdog const dog ("deleter in store");
  store_runs_the_deleter<ledger_policy_t<tag_a>> (false);
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenStoreDropsAHolderOfTheOwner_ThenItMayCallEveryOperation)
{
  Watchdog const dog ("deleter in store, holder");
  store_runs_the_deleter<ledger_policy_t<tag_b>> (true);
}

TEST (LumexSplitCountDeleterTest,
      GivenADeleterThatUsesTheAtomic_WhenTheTornPolicyRuns_ThenItStillWorks)
{
  Watchdog const dog ("deleter in store, torn");
  typedef torn_policy_t<tag_c> policy;
  for (std::uint32_t k = 1; k <= 5; ++k)
    {
      policy::set_period (k);
      store_runs_the_deleter<policy> (false);
      store_runs_the_deleter<policy> (true);
    }
  policy::set_period (0);
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenExchangeHandsTheValueOver_ThenTheDeleterRunsAfterTheCall)
{
  typedef ledger_policy_t<tag_d> policy;
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    rptr r = make_r (3);
    rptr q = make_r (2);
    std::atomic<int> calls (0);
    shared_cell<reentrant_t, policy> a;
    {
      rptr p = make_r (1);
      p->on_destroy = reenter_with (a, r, calls);
      a.store (p);
    }
    rptr old = a.exchange (q);
    EXPECT_EQ (calls.load (), 0) << "the result owns the old value";
    old.reset ();
    EXPECT_EQ (calls.load (), 1) << "the deleter ran with the last owner";
    EXPECT_EQ (a.load ().get (), r.get ());
    check_balanced<policy> ("deleter after exchange");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

struct tag_e
{
};
struct tag_f
{
};
struct tag_g
{
};
struct tag_h
{
};

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenAFailedCompareExchangeReplacesExpected_ThenItRunsInsideTheCall)
{
  Watchdog const dog ("deleter in cas, expected");
  typedef ledger_policy_t<tag_e> policy;
  for (int holder = 0; holder < 2; ++holder)
    {
      reset_ledger<policy> ();
      long const alive = alive_objects ().load ();
      {
        rptr r = make_r (3);
        rptr q = make_r (2);
        std::atomic<int> calls (0);
        shared_cell<reentrant_t, policy> a (holder != 0 ? far_r (q) : q);
        rptr expected = make_r (1);
        expected->on_destroy = reenter_with (a, r, calls);
        EXPECT_FALSE (a.compare_exchange_strong (expected, make_r (9)));
        EXPECT_EQ (calls.load (), 1)
            << "expected was the last owner of the object";
        EXPECT_EQ (a.load ().get (), r.get ()) << "the hook's store won";
        EXPECT_EQ (expected.get (), (holder != 0 ? far_r (q) : q).get ())
            << "expected received the value the slot had";
        check_balanced<policy> ("deleter in cas, expected");
      }
      EXPECT_EQ (alive_objects ().load (), alive);
    }
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenAFailedCompareExchangeRetiresDesired_ThenItRunsInsideTheCall)
{
  Watchdog const dog ("deleter in cas, desired");
  typedef ledger_policy_t<tag_f> policy;
  for (int holder = 0; holder < 2; ++holder)
    {
      reset_ledger<policy> ();
      long const alive = alive_objects ().load ();
      {
        rptr r = make_r (3);
        rptr q = make_r (2);
        std::atomic<int> calls (0);
        shared_cell<reentrant_t, policy> a (q);
        rptr expected = r;
        {
          rptr desired = make_r (1);
          desired->on_destroy = reenter_with (a, r, calls);
          rptr alias = holder != 0 ? far_r (desired) : desired;
          desired.reset ();
          // The only owner moves into the call; on failure the call drops it.
          EXPECT_FALSE (
              a.compare_exchange_strong (expected, std::move (alias)));
        }
        EXPECT_EQ (calls.load (), 1) << "desired died inside the call";
        EXPECT_EQ (a.load ().get (), r.get ());
        check_balanced<policy> ("deleter in cas, desired");
      }
      EXPECT_EQ (alive_objects ().load (), alive);
    }
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenTheAtomicObjectIsDestroyed_ThenTheValueDiesInTheDestructor)
{
  typedef ledger_policy_t<tag_g> policy;
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    rptr r = make_r (3);
    std::atomic<int> calls (0);
    shared_cell<reentrant_t, policy> other (r);
    {
      shared_cell<reentrant_t, policy> a;
      {
        rptr p = make_r (1);
        p->on_destroy = [&]
          {
            calls.fetch_add (1);
            other.store (r);
            rptr seen = other.load ();
          };
        a.store (p);
      }
      EXPECT_EQ (calls.load (), 0);
    }
    EXPECT_EQ (calls.load (), 1) << "the destructor dropped the last owner";
    check_balanced<policy> ("deleter in the destructor");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

typedef gate_policy_t<tag_h> gate_t;

/// W2 is held before its swap with a pin outstanding; another writer swaps
/// the block out; the last payer destroys the old object, which runs a hook
/// that uses the atomic object.
void
payer_runs_the_deleter (bool holder)
{
  gate_t::arrived ().store (0);
  reset_ledger<gate_t> ();
  long const alive = alive_objects ().load ();
  {
    rptr r = make_r (3);
    rptr q = make_r (2);
    rptr z = make_r (4);
    std::atomic<int> calls (0);
    shared_cell<reentrant_t, gate_t> a;
    {
      rptr p = make_r (1);
      p->on_destroy = reenter_with (a, z, calls);
      a.store (holder ? far_r (p) : p);
    }
    rptr loaded;
    hold_group_t<gate_t> load;
    load.start (1, [&] (int) { loaded = a.load (); });
    ASSERT_TRUE (load.wait_arrived (1));
    hold_group_t<gate_t> second;
    second.start (1, [&] (int) { a.store (r); }, split_point_t::swap, 1);
    ASSERT_TRUE (second.wait_arrived (2));
    a.store (q);
    EXPECT_EQ (calls.load (), 0) << "pins are outstanding";
    load.release ();
    loaded.reset ();
    EXPECT_EQ (calls.load (), 0) << "W2's pin is still outstanding";
    second.release ();
    EXPECT_EQ (calls.load (), 1) << "W2 paid last and ran the deleter";
    EXPECT_EQ (a.load ().get (), r.get ())
        << "W2's own store came after the deleter's, as it restarted";
    check_balanced<gate_t> ("payer runs the deleter");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenTheLastPayerOfASwappedOutBlockDestroysIt_ThenItMayCallEveryOperation)
{
  Watchdog const dog ("payer runs the deleter");
  payer_runs_the_deleter (false);
}

TEST (
    LumexSplitCountDeleterTest,
    GivenADeleterThatUsesTheAtomic_WhenTheLastPayerOfASwappedOutHolderDestroysTheOwner_ThenItMayCallEveryOperation)
{
  Watchdog const dog ("payer runs the deleter, holder");
  payer_runs_the_deleter (true);
}

struct tag_i
{
};

TEST (
    LumexSplitCountDeleterTest,
    GivenALoadHeldAfterItsTick_WhenTheValueIsReplacedAndItsOwnersDie_ThenNothingIsDestroyedBeforeTheLoadReturns)
{
  Watchdog const dog ("load held");
  typedef gate_policy_t<tag_i> policy;
  for (int holder = 0; holder < 2; ++holder)
    {
      policy::arrived ().store (0);
      reset_ledger<policy> ();
      long const alive = alive_objects ().load ();
      {
        rptr r = make_r (3);
        rptr q = make_r (2);
        std::atomic<int> calls (0);
        shared_cell<reentrant_t, policy> a;
        {
          rptr p = make_r (1);
          p->on_destroy = reenter_with (a, r, calls);
          a.store (holder != 0 ? far_r (p) : p);
        }
        rptr got;
        hold_group_t<policy> load;
        load.start (1, [&] (int) { got = a.load (); });
        ASSERT_TRUE (load.wait_arrived (1));
        a.store (q);
        EXPECT_EQ (calls.load (), 0)
            << "the held load is an owner: the object cannot die";
        EXPECT_EQ (alive_objects ().load (), alive + 3);
        load.release ();
        ASSERT_TRUE (static_cast<bool> (got));
        EXPECT_EQ (calls.load (), 0)
            << "load returned it, nothing died in load";
        got.reset ();
        EXPECT_EQ (calls.load (), 1) << "the last owner ran the deleter";
        check_balanced<policy> ("load held");
      }
      EXPECT_EQ (alive_objects ().load (), alive);
    }
}

struct tag_j
{
};

/// Threads store fresh objects whose destructor loads the atomic object (and
/// stores once more, to a bounded depth).
void
reentrant_stress (int threads, int rep)
{
  typedef ledger_policy_t<tag_j> policy;
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    shared_cell<reentrant_t, policy> a (make_r (0));
    int const work = lumex_test::scaled (18000);
    run_workers (threads,
                 [&] (int index)
                   {
                     rng_t rng (lumex_test::derive_seed (
                         lumex_test::base_seed (),
                         static_cast<std::uint64_t> (index),
                         static_cast<std::uint64_t> (rep) + 500));
                     std::function<void ()> const hook = [&a]
                       {
                         static thread_local int depth = 0;
                         if (depth < 3)
                           {
                             ++depth;
                             rptr seen = a.load ();
                             if (seen && seen->v % 7 == 0)
                               {
                                 rptr again = make_r (-1);
                                 a.store (again);
                               }
                             --depth;
                           }
                       };
                     for (int k = 0; k < work / threads; ++k)
                       {
                         rptr fresh = make_r (k);
                         fresh->on_destroy = hook;
                         switch (rng.next () % 3)
                           {
                           case 0:
                             a.store (fresh);
                             break;
                           case 1:
                             a.exchange (fresh);
                             break;
                           default:
                             {
                               rptr e = a.load ();
                               a.compare_exchange_strong (e, fresh);
                               break;
                             }
                           }
                       }
                   });
    check_balanced<policy> ("reentrant stress");
    a.store (rptr ());
  }
  EXPECT_EQ (alive_objects ().load (), alive) << "every object died once";
}

TEST (
    LumexSplitCountDeleterTest,
    GivenDeletersThatLoadTheAtomic_WhenThreadsReplaceItsValue_ThenEveryObjectDiesOnce)
{
  Watchdog const dog ("reentrant stress");
  for_each_shape ([] (int threads, int rep)
                    { reentrant_stress (threads, rep); });
}
} // namespace

#else

TEST (LumexSplitCountDeleterTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

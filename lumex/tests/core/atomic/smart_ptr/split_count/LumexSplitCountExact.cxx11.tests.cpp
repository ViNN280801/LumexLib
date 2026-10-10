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

// The exactness oracles of the use_count protocol (plan section 1.3, G1 and
// G2) as scenario tests. A gate policy holds loads after their tick and
// before their count, and writers at a point of their swap, so every window
// of the guarantee is observed at a fixed place:
//
//   (a) K loads held, the value replaced by store, exchange or compare-
//       exchange: an observer on the old object reads K + 1 owners at once
//       (no waiting) and the same after the loads finish;
//   (b) the same with a holder word, observed on the owner (the mirror);
//   (c) a live installation: the held loads are not linearized yet;
//   (d) the same block re-installed with another delta;
//   (e) the weak engine: held weak loads change no use_count;
//   (f) a randomized never-below checker;
//   (g) exact at quiescence (also asserted by every stress shape).
// Plus the windows (i) and (ii), the waiting accessor, and the reserve.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountHold.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

enum replace_t
{
  by_store,
  by_exchange_dropped,
  by_exchange_kept,
  by_cas
};

struct exact_tag
{
};
struct window_tag
{
};
struct reserve0_tag
{
};
struct reinstall_tag
{
};
struct weak_tag
{
};

typedef gate_policy_t<exact_tag> exact_policy;

/// The plan's observer: `use_count ()` of @p object from another thread, which
/// must return without waiting for the held threads.
template <typename Pointer>
long
observe_without_waiting (Pointer const &object, bool &returned)
{
  std::atomic<long> seen (-1);
  std::thread observer (
      [&] { seen.store (static_cast<long> (object.use_count ())); });
  returned = spin_until ([&] { return seen.load () >= 0; }, 5000);
  if (!returned)
    {
      // Never join a thread that waits for the held ones: report and let the
      // caller release the gate first.
      observer.detach ();
      return -1;
    }
  observer.join ();
  return seen.load ();
}

/// Plan item 8 (a) and (b): K loads held after their tick, the value
/// replaced, the owner observed.
void
held_then_replaced (int k, replace_t how, bool holder)
{
  typedef exact_policy policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (7);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (8);
    sp::shared_ptr<int> pv (
        p, holder ? shifted (p.get (), std::int64_t (1) << 41) : &p->v);
    sp::shared_ptr<int> qv (q, &q->v);
    shared_cell<int, policy> a (pv);
    ASSERT_EQ (needs_holder (pv), holder);
    EXPECT_EQ (p.use_count (), 3) << "p, pv, the slot";
    std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (k));
    {
      hold_group_t<policy> held;
      held.start (k, [&] (int i)
                    { got[static_cast<std::size_t> (i)] = a.load (); });
      ASSERT_TRUE (held.wait_arrived (k)) << "the loads must reach the gate";
      EXPECT_EQ (p.use_count (), 3)
          << "(c) held loads are not linearized yet, nothing in transit";
      sp::shared_ptr<int> kept;
      bool ok = true;
      switch (how)
        {
        case by_store:
          a.store (qv);
          break;
        case by_exchange_dropped:
          a.exchange (qv);
          break;
        case by_exchange_kept:
          kept = a.exchange (qv);
          break;
        case by_cas:
          kept = pv;
          ok = a.compare_exchange_strong (kept, qv);
          break;
        }
      EXPECT_TRUE (ok);
      long const extra = (how == by_exchange_kept || how == by_cas) ? 1 : 0;
      // The owners: p, pv, the K linearized loads, the kept copy.
      long const owners = 2 + k + extra;
      bool returned = false;
      long const seen = observe_without_waiting (p, returned);
      if (!returned)
        held.release ();
      EXPECT_TRUE (returned) << "use_count must not wait";
      EXPECT_GE (seen, owners) << "G1: never below the owners";
      if (!holder)
        {
          EXPECT_EQ (seen, owners) << "G2: exact outside the windows";
          words_t const w = words_of (p);
          EXPECT_EQ (w.count, 2 + extra)
              << "nothing was counted for the loads";
          EXPECT_EQ (w.ext, k) << "the held loads are in transit";
        }
      else
        {
          // A holder word keeps its reference on the owner until the last
          // pin has paid, so the owner shows one more than the owners (the
          // report lists this as a finding on G2).
          EXPECT_LE (seen, owners + 1);
        }
      if (how == by_store && !::testing::Test::HasFailure ())
        {
          // The waiting twin does wait for the loads in transit.
          std::atomic<long> settled (-1);
          std::thread waiter ([&] { settled.store (p.use_count_settled ()); });
          std::this_thread::sleep_for (std::chrono::milliseconds (50));
          EXPECT_EQ (settled.load (), -1)
              << "use_count_settled waits while owners are in transit";
          held.release ();
          waiter.join ();
          if (holder)
            {
              EXPECT_GE (settled.load (), owners);
              EXPECT_LE (settled.load (), owners + 1)
                  << "the holder may still hold its reference when ext "
                     "reaches zero";
            }
          else
            EXPECT_EQ (settled.load (), owners);
        }
      else
        held.release ();
      EXPECT_EQ (p.use_count (), owners) << "exact again after the loads";
      expect_exact (p, owners, "after the loads finished");
      for (int i = 0; i < k; ++i)
        EXPECT_TRUE (same_value (got[static_cast<std::size_t> (i)], pv));
      EXPECT_EQ (a.load ().get (), qv.get ());
    }
    got.clear ();
    expect_exact (p, 2, "all results dropped");
    expect_exact (q, 3, "q: q, qv, the slot");
    check_balanced<policy> ("held then replaced");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (
    LumexSplitCountExactTest,
    GivenHeldLoadsOnABlockWord_WhenReplacedByStore_ThenTheObserverReadsKPlusOne)
{
  Watchdog const dog ("held loads, store");
  for (int k = 1; k <= 3; ++k)
    {
      SCOPED_TRACE (k);
      held_then_replaced (k, by_store, false);
    }
}

TEST (
    LumexSplitCountExactTest,
    GivenHeldLoadsOnABlockWord_WhenReplacedByExchange_ThenTheObserverReadsKPlusOne)
{
  Watchdog const dog ("held loads, exchange");
  for (int k = 1; k <= 3; ++k)
    {
      SCOPED_TRACE (k);
      held_then_replaced (k, by_exchange_dropped, false);
      held_then_replaced (k, by_exchange_kept, false);
    }
}

TEST (
    LumexSplitCountExactTest,
    GivenHeldLoadsOnABlockWord_WhenReplacedByCompareExchange_ThenTheObserverReadsKPlusTwo)
{
  Watchdog const dog ("held loads, cas");
  for (int k = 1; k <= 3; ++k)
    {
      SCOPED_TRACE (k);
      held_then_replaced (k, by_cas, false);
    }
}

TEST (LumexSplitCountExactTest,
      GivenHeldLoadsOnAHolderWord_WhenReplaced_ThenTheOwnerIsNeverBelow)
{
  Watchdog const dog ("held loads, holder");
  replace_t const modes[]
      = { by_store, by_exchange_dropped, by_exchange_kept, by_cas };
  for (int k = 1; k <= 3; ++k)
    for (std::size_t m = 0; m < 4; ++m)
      {
        SCOPED_TRACE (k);
        SCOPED_TRACE (static_cast<int> (modes[m]));
        held_then_replaced (k, modes[m], true);
      }
}

TEST (LumexSplitCountExactTest,
      GivenHeldLoadsOnALiveWord_WhenObserved_ThenTheyCountOnlyAfterTheyFinish)
{
  Watchdog const dog ("held loads, live");
  typedef exact_policy policy;
  for (int holder = 0; holder < 2; ++holder)
    for (int k = 1; k <= 3; ++k)
      {
        policy::arrived ().store (0);
        reset_ledger<policy> ();
        sp::shared_ptr<Obj> p = sp::make_shared<Obj> (7);
        sp::shared_ptr<int> pv (
            p,
            holder != 0 ? shifted (p.get (), std::int64_t (1) << 41) : &p->v);
        {
          shared_cell<int, policy> a (pv);
          std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (k));
          hold_group_t<policy> held;
          held.start (k, [&] (int i)
                        { got[static_cast<std::size_t> (i)] = a.load (); });
          ASSERT_TRUE (held.wait_arrived (k));
          bool returned = false;
          long const seen = observe_without_waiting (p, returned);
          if (!returned)
            held.release ();
          EXPECT_TRUE (returned);
          EXPECT_EQ (seen, 3) << "(c) p, pv and the slot; the held loads "
                                 "are not linearized";
          held.release ();
          EXPECT_EQ (p.use_count (), 3 + k) << "K more after they finish";
          expect_exact (p, 3 + k, "live loads finished");
          got.clear ();
          check_balanced<policy> ("live held loads");
        }
        expect_exact (p, 2, "slot destroyed");
      }
}

/// Plan item 8 (d): the same block re-installed with another delta while a
/// pin of the old alias is in flight.
TEST (
    LumexSplitCountExactTest,
    GivenTheSameBlockReinstalledWithAnotherDelta_WhenLoadsOverlap_ThenValuesAreRightAndTheObserverIsInTheWindow)
{
  Watchdog const dog ("reinstall");
  typedef gate_policy_t<reinstall_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (5);
    sp::shared_ptr<int> a1 (owner, &owner->v);
    sp::shared_ptr<int> a2 (owner, &owner->pad[0]);
    shared_cell<int, policy> a (a1);
    EXPECT_EQ (owner.use_count (), 4) << "owner, a1, a2, the slot";
    sp::shared_ptr<int> r1;
    sp::shared_ptr<int> r2;
    hold_group_t<policy> first;
    hold_group_t<policy> second;
    first.start (1, [&] (int) { r1 = a.load (); });
    ASSERT_TRUE (first.wait_arrived (1));
    a.store (a2);
    second.start (1, [&] (int) { r2 = a.load (); });
    ASSERT_TRUE (second.wait_arrived (2));
    first.release ();
    // L1 returned a1 (its pin was of a1) and took the tick of L2 back from
    // the word; L2 has not counted. Owners: owner, a1, a2, the slot, L1.
    ASSERT_TRUE (static_cast<bool> (r1));
    EXPECT_EQ (r1.get (), &owner->v) << "the value of the first pin";
    bool returned = false;
    long const seen = observe_without_waiting (owner, returned);
    if (!returned)
      second.release ();
    EXPECT_TRUE (returned);
    EXPECT_EQ (seen, 5 + 1)
        << "inside window (iii): the owners (5) plus the unit deposited for "
           "a load that has already counted itself";
    second.release ();
    EXPECT_EQ (r2.get (), &owner->pad[0]) << "the value of the second pin";
    EXPECT_EQ (owner.use_count (), 6)
        << "exactly the owners after L2 finished";
    expect_exact (owner, 6, "after both loads");
    r1.reset ();
    r2.reset ();
    check_balanced<policy> ("reinstall");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

/// Window (i): a store between its swap and its drop shows the slot's unit
/// once more (+1), and window (ii): a writer between its deposit and the end
/// of its call shows the deposit (ticks seen plus the reserve at most).
TEST (LumexSplitCountExactTest,
      GivenAStoreHeldBetweenSwapAndDrop_WhenObserved_ThenTheSlotUnitLagsByOne)
{
  Watchdog const dog ("window i");
  typedef gate_policy_t<window_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  {
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<int> pv (p, &p->v);
    sp::shared_ptr<int> qv (q, &q->v);
    shared_cell<int, policy> a (pv);
    {
      hold_group_t<policy> writer;
      writer.start (1, [&] (int) { a.store (qv); }, split_point_t::release, 0);
      ASSERT_TRUE (writer.wait_arrived (1));
      // The swap happened: the slot names q. The owners of p: p and pv.
      EXPECT_EQ (a.load ().get (), qv.get ());
      EXPECT_EQ (p.use_count (), 3) << "owners 2, plus the lagging decrement";
      writer.release ();
    }
    expect_exact (p, 2, "after the drop");
    expect_exact (q, 3, "q: q, qv, the slot");
    check_balanced<policy> ("window i");
  }
}

TEST (
    LumexSplitCountExactTest,
    GivenAWriterHeldAfterItsDeposit_WhenObserved_ThenTheDepositIsAtMostTicksPlusReserve)
{
  Watchdog const dog ("window ii");
  typedef gate_policy_t<window_tag, 0xFFFFFFu, 8u> policy8;
  typedef gate_policy_t<reserve0_tag, 0xFFFFFFu, 0u> policy0;
  policy8::arrived ().store (0);
  policy0::arrived ().store (0);
  // reserve 8 and 0; two held loads; the writer is held on its second `swap`
  // point, i.e. after the pin and the deposit and before the swap.
  for (int reserve = 0; reserve < 2; ++reserve)
    {
      for (int k = 1; k <= 3; ++k)
        {
          sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
          sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
          sp::shared_ptr<int> pv (p, &p->v);
          sp::shared_ptr<int> qv (q, &q->v);
          long seen_during = -1;
          long k_max = 0;
#define SPLIT_WINDOW_II(Policy)                                               \
  do                                                                          \
    {                                                                         \
      Policy::arrived ().store (0);                                           \
      reset_ledger<Policy> ();                                                \
      shared_cell<int, Policy> a (pv);                                        \
      std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (k));    \
      hold_group_t<Policy> loads;                                             \
      loads.start (k, [&] (int i)                                             \
                     { got[static_cast<std::size_t> (i)] = a.load (); });     \
      ASSERT_TRUE (loads.wait_arrived (k));                                   \
      hold_group_t<Policy> writer;                                            \
      writer.start (1, [&] (int) { a.store (qv); }, split_point_t::swap, 1);  \
      ASSERT_TRUE (writer.wait_arrived (k + 1));                              \
      seen_during = static_cast<long> (p.use_count ());                       \
      k_max = static_cast<long> (Policy::reserve ());                         \
      writer.release ();                                                      \
      loads.release ();                                                       \
      expect_exact (p, 2 + k, "window ii finished");                          \
      got.clear ();                                                           \
      check_balanced<Policy> ("window ii");                                   \
    }                                                                         \
  while (0)
          if (reserve == 0)
            SPLIT_WINDOW_II (policy0);
          else
            SPLIT_WINDOW_II (policy8);
#undef SPLIT_WINDOW_II
          // Owners at that instant: p, pv, the slot (still p), and the K
          // loads that the writer's deposit will cover. G1 and G2 (ii):
          // between the owners and the owners plus the ticks and the
          // reserve.
          EXPECT_GE (seen_during, 3 + k) << "G1 with reserve " << k_max;
          EXPECT_LE (seen_during, 3 + k + k_max)
              << "G2 (ii): at most the reserve more, reserve " << k_max;
          if (k_max == 0)
            EXPECT_EQ (seen_during, 3 + k) << "no reserve, no excess";
          else
            EXPECT_EQ (seen_during, 3 + k + k_max)
                << "the whole reserve is deposited until the writer ends";
        }
    }
}

TEST (
    LumexSplitCountExactTest,
    GivenHeldWeakLoads_WhenTheValueIsReplacedAndTheOwnersDie_ThenNoUseCountChangesAndExpiredAgrees)
{
  Watchdog const dog ("weak held");
  typedef gate_policy_t<weak_tag> policy;
  for (int holder = 0; holder < 2; ++holder)
    for (int k = 1; k <= 3; ++k)
      {
        SCOPED_TRACE (holder);
        SCOPED_TRACE (k);
        policy::arrived ().store (0);
        reset_ledger<policy> ();
        long const alive = alive_objects ().load ();
        long const live = live_allocations ().load ();
        {
          sp::shared_ptr<Obj> p = sp::make_shared<Obj> (7);
          sp::shared_ptr<Obj> q = sp::make_shared<Obj> (8);
          sp::shared_ptr<int> pv (
              p, holder != 0 ? shifted (p.get (), std::int64_t (1) << 41)
                             : &p->v);
          sp::shared_ptr<int> qv (q, &q->v);
          sp::weak_ptr<int> pw (pv);
          sp::weak_ptr<int> qw (qv);
          weak_cell<int, policy> a (pw);
          EXPECT_EQ (p.use_count (), 2)
              << "p and pv; weak pointers do not own";
          std::vector<sp::weak_ptr<int>> got (static_cast<std::size_t> (k));
          hold_group_t<policy> held;
          held.start (k, [&] (int i)
                        { got[static_cast<std::size_t> (i)] = a.load (); });
          ASSERT_TRUE (held.wait_arrived (k));
          a.store (qw);
          EXPECT_EQ (p.use_count (), 2)
              << "held weak loads change no use_count";
          EXPECT_FALSE (pw.expired ());
          pv.reset ();
          p.reset ();
          EXPECT_EQ (alive_objects ().load (), alive + 1)
              << "the object died with its last owner; the held loads keep "
                 "only the block";
          EXPECT_TRUE (pw.expired ());
          held.release ();
          for (int i = 0; i < k; ++i)
            {
              sp::weak_ptr<int> &w = got[static_cast<std::size_t> (i)];
              EXPECT_TRUE (w.expired ()) << "expired () is consistent";
              EXPECT_EQ (w.use_count (), 0);
              EXPECT_FALSE (static_cast<bool> (w.lock ()));
            }
          sp::weak_ptr<int> now = a.load ();
          EXPECT_FALSE (now.expired ());
          EXPECT_EQ (now.lock ().get (), &q->v);
          check_balanced<policy> ("weak held");
        }
        EXPECT_EQ (alive_objects ().load (), alive);
        EXPECT_EQ (live_allocations ().load (), live)
            << "the dead block and the weak holder were freed";
      }
}

/// The reserve: ticks that arrive between a writer's deposit and its swap are
/// absorbed by the reserve without another access to the counter; when more
/// arrive than the reserve covers, the failed swap tops the deposit up.
template <std::uint32_t Reserve>
void
reserve_scenario (int arriving)
{
  typedef gate_policy_t<struct reserve_tag, 0xFFFFFFu, Reserve> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<int> const pv (p, &p->v);
    sp::shared_ptr<int> const qv (q, &q->v);
    shared_cell<int, policy> a (pv);
    // A first load is held after its tick, so that the writer finds a tick
    // and pins; it is released with the others at the end.
    std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (arriving)
                                          + 1);
    hold_group_t<policy> first;
    first.start (1, [&] (int) { got[0] = a.load (); });
    ASSERT_TRUE (first.wait_arrived (1));
    hold_group_t<policy> writer;
    writer.start (1, [&] (int) { a.store (qv); }, split_point_t::swap, 1);
    ASSERT_TRUE (writer.wait_arrived (2)) << "the writer deposited and waits";
    // The writer saw one tick besides its own: it deposited 1 + the reserve.
    EXPECT_EQ (policy::counters ().transfers.load (), 1ul + Reserve);
    hold_group_t<policy> later;
    later.start (arriving, [&] (int i)
                   { got[static_cast<std::size_t> (i) + 1] = a.load (); });
    ASSERT_TRUE (later.wait_arrived (2 + arriving));
    writer.release ();
    long const loads = 1 + arriving;
    // The owners now: p, pv, and every load that the successful swap covered.
    EXPECT_EQ (p.use_count (), 2 + loads)
        << "G1 and exactness after the writer returned";
    unsigned long const covered = static_cast<unsigned long> (arriving);
    if (static_cast<std::uint32_t> (arriving) > Reserve)
      EXPECT_EQ (policy::counters ().transfers.load (),
                 1ul + Reserve + covered)
          << "the ticks beyond the reserve were topped up";
    else
      EXPECT_EQ (policy::counters ().transfers.load (), 1ul + Reserve)
          << "the reserve absorbed the arriving ticks: no top-up";
    later.release ();
    first.release ();
    expect_exact (p, 2 + loads, "after all the loads");
    got.clear ();
    expect_exact (p, 2, "p and pv");
    check_balanced<policy> ("reserve");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (
    LumexSplitCountExactTest,
    GivenTicksArrivingAfterTheDeposit_WhenTheWriterSwaps_ThenTheReserveAbsorbsOrTheDepositIsToppedUp)
{
  Watchdog const dog ("reserve");
  reserve_scenario<0u> (0);
  reserve_scenario<0u> (1);
  reserve_scenario<0u> (3);
  reserve_scenario<1u> (1);
  reserve_scenario<1u> (2);
  reserve_scenario<4u> (3);
  reserve_scenario<4u> (4);
  reserve_scenario<4u> (5);
}

/// An object whose only owners are loads in transit is alive: `expired ()` is
/// false and a `weak_ptr::lock ()` succeeds (a count of zero with an `ext`
/// above zero is alive).
TEST (
    LumexSplitCountExactTest,
    GivenOnlyLoadsInTransitOwnTheObject_WhenItIsLockedAndAskedIfExpired_ThenItIsAlive)
{
  Watchdog const dog ("loads in transit own the object");
  typedef gate_policy_t<struct transit_tag> policy;
  for (int holder = 0; holder < 2; ++holder)
    for (int k = 1; k <= 3; ++k)
      {
        policy::arrived ().store (0);
        reset_ledger<policy> ();
        long const alive = alive_objects ().load ();
        {
          sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
          sp::shared_ptr<int> const qv (q, &q->v);
          sp::weak_ptr<int> weak;
          shared_cell<int, policy> a;
          {
            sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
            sp::shared_ptr<int> const pv (
                p, holder != 0 ? shifted (p.get (), std::int64_t (1) << 41)
                               : &p->v);
            a.store (pv);
            weak = sp::weak_ptr<int> (pv);
          }
          // Only the slot owns the object. K loads are held after their tick.
          std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (k));
          hold_group_t<policy> held;
          held.start (k, [&] (int i)
                        { got[static_cast<std::size_t> (i)] = a.load (); });
          ASSERT_TRUE (held.wait_arrived (k));
          a.store (qv);
          EXPECT_FALSE (weak.expired ())
              << "the loads in transit are owners (G1): not expired";
          {
            sp::shared_ptr<int> const locked = weak.lock ();
            EXPECT_GT (locked.use_count (), 0)
                << "lock () succeeds on count 0 with owners in transit";
            EXPECT_EQ (alive_objects ().load (), alive + 2);
            held.release ();
            for (int i = 0; i < k; ++i)
              EXPECT_EQ (got[static_cast<std::size_t> (i)].get (),
                         locked.get ());
          }
          got.clear ();
          EXPECT_TRUE (weak.expired ()) << "the last owner is gone";
          EXPECT_EQ (alive_objects ().load (), alive + 1) << "only q is left";
          check_balanced<policy> ("loads in transit own the object");
        }
        EXPECT_EQ (alive_objects ().load (), alive);
      }
}

// -- (f) the randomized never-below checker --

struct alignas (64) thread_counters_t
{
  std::atomic<long> returned;
  std::atomic<long> released;
};

/// One thread holds P and samples `use_count ()`; loaders keep what they
/// load in small slots with counters around them. Every sample must be at
/// least `1 + (loads returned before it started and not released before it
/// ended)`; the upper bound is a sanity bound only.
void
never_below_once (int threads, int rep)
{
  typedef ledger_policy_t<struct never_below_tag> policy;
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> const P = sp::make_shared<Obj> (1);
    std::vector<sp::shared_ptr<Obj>> others;
    for (int i = 0; i < 3; ++i)
      others.push_back (sp::make_shared<Obj> (2 + i));
    shared_cell<Obj, policy> a (P);
    int const writers = threads == 1 ? 0 : std::max (1, threads / 4);
    int const loaders = threads == 1 ? 1 : threads - writers;
    int const work = lumex_test::scaled (72000);
    std::vector<thread_counters_t> counters (
        static_cast<std::size_t> (loaders));
    for (std::size_t i = 0; i < counters.size (); ++i)
      {
        counters[i].returned = 0;
        counters[i].released = 0;
      }
    std::atomic<bool> stop (false);
    std::atomic<long> violations (0);
    std::atomic<long> samples (0);
    std::atomic<long> max_excess (0);
    auto sum = [&] (bool released)
      {
        long total = 0;
        for (std::size_t i = 0; i < counters.size (); ++i)
          total += (released ? counters[i].released : counters[i].returned)
                       .load ();
        return total;
      };
    auto sample = [&] (long extra_owners)
      {
        long const returned_before = sum (false);
        long const released_before = sum (true);
        long const value = static_cast<long> (P.use_count ());
        long const released_after = sum (true);
        long const returned_after = sum (false);
        long const lower = 1 + returned_before - released_after;
        long const upper
            = 2 + extra_owners + (returned_after - released_before)
              + 2 * loaders
              + writers
                    * (static_cast<long> (policy::reserve ()) + threads + 2);
        samples.fetch_add (1);
        if (value < lower)
          violations.fetch_add (1);
        if (value > upper)
          violations.fetch_add (1);
        long const excess = value - lower;
        long seen = max_excess.load ();
        while (excess > seen
               && !max_excess.compare_exchange_weak (seen, excess))
          {
          }
      };
    std::thread sampler;
    if (threads > 1)
      sampler = std::thread (
          [&]
            {
              while (!stop.load ())
                {
                  sample (0);
                  std::this_thread::yield ();
                }
            });
    run_workers (
        threads,
        [&] (int index)
          {
            rng_t rng (lumex_test::derive_seed (
                lumex_test::base_seed (), static_cast<std::uint64_t> (index),
                static_cast<std::uint64_t> (rep) + 300));
            if (threads == 1 || index >= writers)
              {
                thread_counters_t &mine = counters[static_cast<std::size_t> (
                    threads == 1 ? 0 : index - writers)];
                sp::shared_ptr<Obj> slots[3];
                int next = 0;
                int const loads = work / loaders;
                for (int k = 0; k < loads; ++k)
                  {
                    sp::shared_ptr<Obj> p = a.load ();
                    std::size_t const s = static_cast<std::size_t> (next);
                    next = (next + 1) % 3;
                    if (slots[s] && slots[s] == P)
                      mine.released.fetch_add (1);
                    slots[s].reset ();
                    if (p == P)
                      mine.returned.fetch_add (1);
                    slots[s] = std::move (p);
                    if (threads == 1 && k % 4 == 0)
                      {
                        // One thread: the writes come from here, and the
                        // sample is exact.
                        switch (rng.next () % 3)
                          {
                          case 0:
                            a.store (others[rng.next () % 3]);
                            break;
                          case 1:
                            a.store (P);
                            break;
                          default:
                            a.exchange (P);
                            break;
                          }
                        sp::shared_ptr<Obj> now = a.load ();
                        long held_here = 0;
                        for (int i = 0; i < 3; ++i)
                          if (slots[i] && slots[i] == P)
                            ++held_here;
                        long const expected
                            = 1 + held_here + (now == P ? 2 : 0);
                        EXPECT_EQ (static_cast<long> (P.use_count ()),
                                   expected)
                            << "one thread: exact";
                      }
                  }
                for (int i = 0; i < 3; ++i)
                  if (slots[i] && slots[i] == P)
                    mine.released.fetch_add (1);
              }
            else
              {
                int const writes = work / (4 * writers);
                for (int k = 0; k < writes; ++k)
                  {
                    switch (rng.next () % 5)
                      {
                      case 0:
                        a.store (P);
                        break;
                      case 1:
                        a.store (others[rng.next () % 3]);
                        break;
                      case 2:
                        a.exchange (P);
                        break;
                      case 3:
                        {
                          sp::shared_ptr<Obj> e = a.load ();
                          a.compare_exchange_strong (e, P);
                          break;
                        }
                      default:
                        {
                          sp::shared_ptr<Obj> e = a.load ();
                          a.compare_exchange_strong (e,
                                                     others[rng.next () % 3]);
                          break;
                        }
                      }
                  }
              }
          });
    stop.store (true);
    if (sampler.joinable ())
      sampler.join ();
    EXPECT_EQ (violations.load (), 0)
        << "G1 (or the sanity bound) failed; samples " << samples.load ()
        << " max excess " << max_excess.load ();
    // (g) exact at quiescence.
    sp::shared_ptr<Obj> cur = a.load ();
    expect_exact (cur, 3, "final value: its variable, the slot and cur");
    if (!(cur == P))
      expect_exact (P, 1, "P");
    check_balanced<policy> ("never below");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (LumexSplitCountExactTest,
      GivenLoadersAndWriters_WhenSampled_ThenUseCountIsNeverBelowTheHeldLoads)
{
  Watchdog const dog ("never below");
  std::vector<int> const counts = shape_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    for (int rep = 0; rep < shape_repetitions (); ++rep)
      {
        SCOPED_TRACE (
            lumex_test::replay_text (lumex_test::base_seed (), counts[c])
            + " repetition " + std::to_string (rep));
        never_below_once (counts[c], rep);
        if (::testing::Test::HasFatalFailure ())
          return;
      }
}
} // namespace

#else

TEST (LumexSplitCountExactTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

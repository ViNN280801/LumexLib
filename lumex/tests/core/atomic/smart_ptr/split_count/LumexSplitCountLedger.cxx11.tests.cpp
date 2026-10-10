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

// The tick ledger of the split-count engine under load and under a fixed
// schedule. After every multi-thread run the identities hold:
//   ticks == unticks + settles + consumed          (every tick ends once)
//   transfers - untransfers == settles             (every deposit is paid or
//                                                   returned)
// the objects are all destroyed, every block is back to its expected count
// with `ext == 0`, and the peak of `L` never exceeded the policy's limit. The
// shapes (shared engine, tick limit 2, holders of two owners mixed with
// in-window aliases, the weak engine with weak holders) run at 1, 2, 4, 8 and
// 24 threads, five repetitions each, with reserve 0, 4 and 8. The fixed
// schedules cover the undo of a writer whose block was swapped out, the tick
// limit at each of its three checks, and the reentrancy of deleters.

#include <atomic>
#include <chrono>
#include <cstdint>
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

template <int N> struct tag_t
{
};

#define SPLIT_SHAPE_TEST(Name, Shape, Limit, Reserve, N)                      \
  TEST (LumexSplitCountLedgerTest, Name)                                      \
  {                                                                           \
    Watchdog const dog (#Name);                                               \
    typedef ledger_policy_t<tag_t<N>, Limit, Reserve> policy;                 \
    for_each_shape ([] (int threads, int rep)                                 \
                      { Shape<policy> (threads, rep); });                     \
  }

SPLIT_SHAPE_TEST (GivenTheSharedShape_WhenReserveIsEight_ThenTheLedgerBalances,
                  stress_shared, 0xFFFFFFu, 8u, 1)
SPLIT_SHAPE_TEST (GivenTheSharedShape_WhenReserveIsFour_ThenTheLedgerBalances,
                  stress_shared, 0xFFFFFFu, 4u, 2)
SPLIT_SHAPE_TEST (GivenTheSharedShape_WhenReserveIsZero_ThenTheLedgerBalances,
                  stress_shared, 0xFFFFFFu, 0u, 3)
SPLIT_SHAPE_TEST (
    GivenTheSharedShape_WhenTheTickLimitIsTwo_ThenTheLedgerBalances,
    stress_shared, 2u, 8u, 4)
SPLIT_SHAPE_TEST (
    GivenTheSharedShape_WhenTheTickLimitIsTwoAndReserveZero_ThenTheLedgerBalances,
    stress_shared, 2u, 0u, 5)
SPLIT_SHAPE_TEST (GivenTheHolderShape_WhenReserveIsEight_ThenTheLedgerBalances,
                  stress_holder, 0xFFFFFFu, 8u, 6)
SPLIT_SHAPE_TEST (GivenTheHolderShape_WhenReserveIsFour_ThenTheLedgerBalances,
                  stress_holder, 0xFFFFFFu, 4u, 7)
SPLIT_SHAPE_TEST (GivenTheHolderShape_WhenReserveIsZero_ThenTheLedgerBalances,
                  stress_holder, 0xFFFFFFu, 0u, 8)
SPLIT_SHAPE_TEST (
    GivenTheHolderShape_WhenTheTickLimitIsTwo_ThenTheLedgerBalances,
    stress_holder, 2u, 8u, 9)
SPLIT_SHAPE_TEST (GivenTheWeakShape_WhenReserveIsEight_ThenTheLedgerBalances,
                  stress_weak, 0xFFFFFFu, 8u, 10)
SPLIT_SHAPE_TEST (GivenTheWeakShape_WhenReserveIsZero_ThenTheLedgerBalances,
                  stress_weak, 0xFFFFFFu, 0u, 11)
SPLIT_SHAPE_TEST (
    GivenTheWeakShape_WhenTheTickLimitIsTwo_ThenTheLedgerBalances, stress_weak,
    2u, 4u, 12)

#undef SPLIT_SHAPE_TEST

// -- Fixed schedules --

struct undo_tag
{
};
struct limit_load_tag
{
};
struct limit_store_tag
{
};
struct limit_cas_tag
{
};
struct cas_undo_tag
{
};
struct cas_holder_tag
{
};
struct cas_success_tag
{
};

/// The value of a slot that holds @p owner: an in-window alias or a far one.
sp::shared_ptr<int>
alias_of (sp::shared_ptr<Obj> const &owner, bool holder)
{
  return sp::shared_ptr<int> (
      owner,
      holder ? shifted (owner.get (), std::int64_t (1) << 41) : &owner->v);
}

/// Two writers and a load: the load is held after its tick, so the word
/// carries a tick and W2's store pins it and deposits; W2 is held before its
/// swap. W1 stores another value and swaps the block out with the ticks of
/// the load and of W2 in the word. W2's swap then fails and it pays its own
/// tick together with its deposit in one step (S5), which destroys the old
/// object inside W2's store when it is the last to pay.
void
two_writers_undo (bool holder)
{
  typedef gate_policy_t<undo_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  long const live = live_allocations ().load ();
  {
    sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<Obj> r = sp::make_shared<Obj> (3);
    sp::shared_ptr<int> const qv (q, &q->v);
    sp::shared_ptr<int> const rv (r, &r->v);
    shared_cell<int, policy> a (alias_of (owner, holder));
    owner.reset ();
    EXPECT_EQ (alive_objects ().load (), alive + 3)
        << "the slot owns the first object";
    sp::shared_ptr<int> loaded;
    hold_group_t<policy> load;
    load.start (1, [&] (int) { loaded = a.load (); });
    ASSERT_TRUE (load.wait_arrived (1)) << "the load has ticked";
    hold_group_t<policy> second;
    second.start (1, [&] (int) { a.store (rv); }, split_point_t::swap, 1);
    ASSERT_TRUE (second.wait_arrived (2)) << "W2 must reach its swap";
    a.store (qv);
    EXPECT_EQ (a.load ().get (), qv.get ());
    load.release ();
    ASSERT_TRUE (static_cast<bool> (loaded));
    loaded.reset ();
    EXPECT_EQ (alive_objects ().load (), alive + 3)
        << "the old object lives: the pin of W2 is outstanding";
    second.release ();
    EXPECT_EQ (alive_objects ().load (), alive + 2)
        << "W2 paid the last unit and destroyed the old object";
    sp::shared_ptr<int> const now = a.load ();
    EXPECT_EQ (now.get (), rv.get ()) << "W2 stored after W1";
    expect_exact (q, 2, "q: W1's value was swapped out by W2");
    expect_exact (r, 4, "r: r, rv, the slot, now");
    check_balanced<policy> ("two writers");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
  EXPECT_EQ (live_allocations ().load (), live) << "the holder was freed";
}

TEST (
    LumexSplitCountLedgerTest,
    GivenAWriterHeldBeforeItsSwap_WhenAnotherWriterSwapsTheBlockOut_ThenItPaysAndRestarts)
{
  Watchdog const dog ("two writers");
  two_writers_undo (false);
}

TEST (
    LumexSplitCountLedgerTest,
    GivenAWriterHeldBeforeItsSwap_WhenAnotherWriterSwapsAHolderOut_ThenItPaysAndRestarts)
{
  Watchdog const dog ("two writers, holder");
  two_writers_undo (true);
}

struct settle_tag
{
};

/// A load counted itself on the live path and is held before it takes its
/// tick back; the block is swapped out (the writer deposits a unit for the
/// tick) and installed again with no ticks. The tick of the load is not in
/// the word any more, so the load must pay the deposit, never take a tick
/// from a word that has none (that would borrow from the offset field).
TEST (
    LumexSplitCountLedgerTest,
    GivenALoadHeldBeforeItsSettle_WhenTheBlockIsSwappedOutAndInstalledAgain_ThenItPaysAndDoesNotUntick)
{
  Watchdog const dog ("settle on a re-installed block");
  typedef gate_policy_t<settle_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<int> const pv (p, &p->v);
    sp::shared_ptr<int> const qv (q, &q->v);
    shared_cell<int, policy> a (pv);
    sp::shared_ptr<int> got;
    hold_group_t<policy> load;
    load.start (
        1, [&] (int) { got = a.load (); }, split_point_t::settle_read, 0);
    ASSERT_TRUE (load.wait_arrived (1)) << "the load counted and is held";
    a.store (qv);
    a.store (pv);
    load.release ();
    EXPECT_EQ (policy::counters ().unticks.load (), 0u)
        << "no tick was left in the word for the load to take back";
    EXPECT_EQ (policy::counters ().settles.load (), 1u)
        << "the load paid the unit that the writer deposited";
    EXPECT_EQ (got.get (), pv.get ());
    check_balanced<policy> ("settle after re-install");
    expect_exact (p, 4, "p, pv, the slot and got");
    expect_exact (q, 2, "q and qv");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

/// A compare-exchange on a block word deposits the ticks it sees without a
/// pin; held before its swap, the word is replaced by a store, so its swap
/// fails and it takes the deposit back (S10).
void
cas_deposit_returned (int held_loads)
{
  typedef gate_policy_t<cas_undo_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<Obj> r = sp::make_shared<Obj> (3);
    sp::shared_ptr<int> const pv (p, &p->v);
    sp::shared_ptr<int> const qv (q, &q->v);
    sp::shared_ptr<int> const rv (r, &r->v);
    shared_cell<int, policy> a (pv);
    std::vector<sp::shared_ptr<int>> got (
        static_cast<std::size_t> (held_loads));
    hold_group_t<policy> loads;
    loads.start (held_loads, [&] (int i)
                   { got[static_cast<std::size_t> (i)] = a.load (); });
    ASSERT_TRUE (loads.wait_arrived (held_loads));
    sp::shared_ptr<int> expected = pv;
    bool result = true;
    hold_group_t<policy> comparer;
    comparer.start (
        1, [&] (int) { result = a.compare_exchange_strong (expected, rv); },
        split_point_t::cas, 0);
    ASSERT_TRUE (comparer.wait_arrived (held_loads + 1));
    a.store (qv);
    comparer.release ();
    EXPECT_FALSE (result) << "the slot changed under the compare-exchange";
    EXPECT_EQ (expected.get (), qv.get ())
        << "expected receives the current value";
    loads.release ();
    for (int i = 0; i < held_loads; ++i)
      EXPECT_EQ (got[static_cast<std::size_t> (i)].get (), pv.get ());
    got.clear ();
    expect_exact (p, 2, "p after everything finished");
    expect_exact (q, 4, "q: q, qv, the slot, expected");
    expect_exact (r, 2, "r was never stored");
    check_balanced<policy> ("compare-exchange undo");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (
    LumexSplitCountLedgerTest,
    GivenACompareExchangeHeldBeforeItsSwap_WhenTheWordIsReplaced_ThenItTakesItsDepositBack)
{
  Watchdog const dog ("cas undo");
  for (int k = 0; k <= 3; ++k)
    {
      SCOPED_TRACE (k);
      cas_deposit_returned (k);
    }
}

/// S12: a compare-exchange whose expected value is a holder pins the holder
/// word; held before its swap, the holder is swapped out by a store, so the
/// compare-exchange pays its own tick (and the mirror) and fails.
TEST (
    LumexSplitCountLedgerTest,
    GivenAHolderCompareExchangeHeldBeforeItsSwap_WhenTheHolderIsSwappedOut_ThenItPaysItsTickAndFails)
{
  Watchdog const dog ("holder cas undo");
  typedef gate_policy_t<cas_holder_tag> policy;
  policy::arrived ().store (0);
  reset_ledger<policy> ();
  long const alive = alive_objects ().load ();
  long const live = live_allocations ().load ();
  {
    sp::shared_ptr<Obj> o = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
    sp::shared_ptr<Obj> r = sp::make_shared<Obj> (3);
    sp::shared_ptr<int> const ov = alias_of (o, true);
    sp::shared_ptr<int> const qv (q, &q->v);
    sp::shared_ptr<int> const rv (r, &r->v);
    shared_cell<int, policy> a (ov);
    sp::shared_ptr<int> expected = ov;
    bool result = true;
    hold_group_t<policy> comparer;
    comparer.start (
        1, [&] (int) { result = a.compare_exchange_strong (expected, rv); },
        split_point_t::cas, 0);
    ASSERT_TRUE (comparer.wait_arrived (1)) << "pinned the holder word";
    a.store (qv);
    comparer.release ();
    EXPECT_FALSE (result);
    EXPECT_EQ (expected.get (), qv.get ());
    expect_exact (o, 2, "o: o and ov; the holder died");
    expect_exact (r, 2, "r was never stored");
    check_balanced<policy> ("holder compare-exchange undo");
  }
  EXPECT_EQ (alive_objects ().load (), alive);
  EXPECT_EQ (live_allocations ().load (), live) << "the holder was freed";
}

/// S12 success with ticks present: the own tick is consumed, not paid.
TEST (
    LumexSplitCountLedgerTest,
    GivenAHolderCompareExchangeWithLoadsHeld_WhenItSucceeds_ThenItsTickIsConsumed)
{
  Watchdog const dog ("holder cas success");
  typedef gate_policy_t<cas_success_tag> policy;
  for (int k = 0; k <= 3; ++k)
    {
      SCOPED_TRACE (k);
      policy::arrived ().store (0);
      reset_ledger<policy> ();
      long const alive = alive_objects ().load ();
      long const live = live_allocations ().load ();
      {
        sp::shared_ptr<Obj> o = sp::make_shared<Obj> (1);
        sp::shared_ptr<Obj> r = sp::make_shared<Obj> (3);
        sp::shared_ptr<int> const ov = alias_of (o, true);
        sp::shared_ptr<int> const rv (r, &r->v);
        shared_cell<int, policy> a (ov);
        std::vector<sp::shared_ptr<int>> got (static_cast<std::size_t> (k));
        hold_group_t<policy> loads;
        loads.start (k, [&] (int i)
                       { got[static_cast<std::size_t> (i)] = a.load (); });
        ASSERT_TRUE (loads.wait_arrived (k));
        sp::shared_ptr<int> expected = ov;
        EXPECT_TRUE (a.compare_exchange_strong (expected, rv));
        loads.release ();
        got.clear ();
        expect_exact (o, 3, "o: o, ov, expected");
        EXPECT_EQ (a.load ().get (), rv.get ());
        check_balanced<policy> ("holder compare-exchange success");
      }
      EXPECT_EQ (alive_objects ().load (), alive);
      EXPECT_EQ (live_allocations ().load (), live);
    }
}

/// The tick limit at each of its three checks: a load, a writer's pin and the
/// pin of a holder compare-exchange back off while the word carries `limit`
/// ticks, and none of them raises `L` above the limit.
template <typename Policy> struct limit_fixture_t
{
  limit_fixture_t (bool holder)
      : owner (sp::make_shared<Obj> (1)), other (sp::make_shared<Obj> (2)),
        value (alias_of (owner, holder)), replacement (other, &other->v),
        cell (value), got (2), loads (), finished (false)
  {
    Policy::arrived ().store (0);
    reset_ledger<Policy> ();
    loads.start (2, [this] (int i)
                   { got[static_cast<std::size_t> (i)] = cell.load (); });
  }

  sp::shared_ptr<Obj> owner;
  sp::shared_ptr<Obj> other;
  sp::shared_ptr<int> value;
  sp::shared_ptr<int> replacement;
  shared_cell<int, Policy> cell;
  std::vector<sp::shared_ptr<int>> got;
  hold_group_t<Policy> loads;
  std::atomic<bool> finished;
};

TEST (
    LumexSplitCountLedgerTest,
    GivenTheWordAtTheTickLimit_WhenAnotherLoadComes_ThenItBacksOffUntilATickIsGone)
{
  Watchdog const dog ("limit, load");
  typedef gate_policy_t<limit_load_tag, 2u> policy;
  for (int holder = 0; holder < 2; ++holder)
    {
      limit_fixture_t<policy> f (holder != 0);
      ASSERT_TRUE (f.loads.wait_arrived (2));
      EXPECT_EQ (policy::counters ().peak.load (), 2u);
      sp::shared_ptr<int> third;
      std::thread loader (
          [&]
            {
              third = f.cell.load ();
              f.finished.store (true);
            });
      EXPECT_FALSE (flag_set_within (f.finished, 150))
          << "the third load must wait: two ticks are in the word";
      EXPECT_EQ (policy::counters ().peak.load (), 2u)
          << "L never exceeds the limit";
      f.loads.release ();
      loader.join ();
      EXPECT_TRUE (f.finished.load ());
      EXPECT_EQ (third.get (), f.value.get ());
      EXPECT_LE (policy::counters ().peak.load (), 2u);
      f.got.clear ();
      third.reset ();
      check_balanced<policy> ("limit load");
    }
}

TEST (
    LumexSplitCountLedgerTest,
    GivenTheWordAtTheTickLimit_WhenAWriterComes_ThenItBacksOffUntilATickIsGone)
{
  Watchdog const dog ("limit, writer");
  typedef gate_policy_t<limit_store_tag, 2u> policy;
  for (int holder = 0; holder < 2; ++holder)
    for (int how = 0; how < 2; ++how)
      {
        limit_fixture_t<policy> f (holder != 0);
        ASSERT_TRUE (f.loads.wait_arrived (2));
        std::thread writer (
            [&]
              {
                if (how == 0)
                  f.cell.store (f.replacement);
                else
                  f.cell.exchange (f.replacement);
                f.finished.store (true);
              });
        EXPECT_FALSE (flag_set_within (f.finished, 150))
            << "the writer's pin must wait: two ticks are in the word";
        EXPECT_EQ (policy::counters ().peak.load (), 2u);
        f.loads.release ();
        writer.join ();
        EXPECT_TRUE (f.finished.load ());
        EXPECT_LE (policy::counters ().peak.load (), 2u);
        EXPECT_EQ (f.cell.load ().get (), f.replacement.get ());
        f.got.clear ();
        check_balanced<policy> ("limit writer");
      }
}

TEST (LumexSplitCountLedgerTest,
      GivenAHolderWordAtTheTickLimit_WhenACompareExchangePins_ThenItBacksOff)
{
  Watchdog const dog ("limit, holder cas");
  typedef gate_policy_t<limit_cas_tag, 2u> policy;
  limit_fixture_t<policy> f (true);
  ASSERT_TRUE (f.loads.wait_arrived (2));
  sp::shared_ptr<int> expected = f.value;
  bool result = false;
  std::thread comparer (
      [&]
        {
          result = f.cell.compare_exchange_strong (expected, f.replacement);
          f.finished.store (true);
        });
  EXPECT_FALSE (flag_set_within (f.finished, 150))
      << "the pin of the compare-exchange must wait";
  EXPECT_EQ (policy::counters ().peak.load (), 2u);
  f.loads.release ();
  comparer.join ();
  EXPECT_TRUE (result);
  EXPECT_LE (policy::counters ().peak.load (), 2u);
  f.got.clear ();
  check_balanced<policy> ("limit holder compare-exchange");
}
} // namespace

#else

TEST (LumexSplitCountLedgerTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

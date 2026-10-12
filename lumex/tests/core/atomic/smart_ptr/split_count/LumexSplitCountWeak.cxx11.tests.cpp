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

// The weak engine on a single thread: `atomic_weak_ptr_lock_free_split_count`
// over every kind of weak value (empty, in-window aliases, far aliases that
// need a weak holder, a null alias, aliases of an owner that already died)
// crossed with load, store, exchange and compare-exchange. A weak pointer
// owns a weak reference only: the strong count of an owner never changes, the
// weak ledger counts the slot and the weak holder, and a weak holder keeps the
// control block, never the object.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_weak_ptr_lock_free_split_count<int> weak_atomic;
typedef sp::weak_ptr<int> wptr;

struct weak_kind_t
{
  wptr value;
  int owner; // -1: none
  bool expired;
  char const *name;
};

struct weak_set_t
{
  std::vector<sp::shared_ptr<Obj>> owners; // the two live owners
  std::vector<sp::shared_ptr<int>> strong; // aliases that keep them alive
  std::vector<weak_kind_t> kinds;
  int alive_owners;

  /// The strong count of live owner @p index while no atomic exists.
  long
  strong_base (int index) const
  {
    long n = 1;
    for (std::size_t i = 0; i < strong.size (); ++i)
      if (block_of (strong[i])
          == block_of (owners[static_cast<std::size_t> (index)]))
        ++n;
    return n;
  }
};

bool
weak_needs_holder (wptr const &w)
{
  sp::detail::ctl_base *const block = block_of (w);
  if (block == nullptr)
    return false;
  std::int64_t const d = static_cast<std::int64_t> (
      reinterpret_cast<std::uintptr_t> (sp::detail::access::stored (w))
      - block->anchor ());
  std::int64_t const half = std::int64_t (1) << 39;
  return d < -half || d >= half;
}

/// Weak values of two live owners and of a third that is dead (its block
/// lives on through two weak pointers). Index 8 and 9 are the dead ones.
weak_set_t
make_weak_set ()
{
  weak_set_t set;
  set.alive_owners = 2;
  set.owners.push_back (sp::make_shared<Obj> (11));
  set.owners.push_back (sp::make_shared<Obj> (22));
  Obj *const o1 = set.owners[0].get ();
  Obj *const o2 = set.owners[1].get ();
  std::int64_t const far = std::int64_t (1) << 41;
  sp::shared_ptr<int> const a1 (set.owners[0], &o1->v);
  sp::shared_ptr<int> const a2 (set.owners[0], &o1->pad[1]);
  sp::shared_ptr<int> const a3 (set.owners[0], shifted (o1, far));
  sp::shared_ptr<int> const a4 (set.owners[0], shifted (o1, far + 8));
  sp::shared_ptr<int> const a5 (set.owners[0], static_cast<int *> (nullptr));
  sp::shared_ptr<int> const b1 (set.owners[1], &o2->v);
  sp::shared_ptr<int> const b2 (set.owners[1], shifted (o2, far));
  set.strong.push_back (a1);
  set.strong.push_back (a2);
  set.strong.push_back (a3);
  set.strong.push_back (a4);
  set.strong.push_back (a5);
  set.strong.push_back (b1);
  set.strong.push_back (b2);
  weak_kind_t const kinds[] = { { wptr (), -1, false, "empty" },
                                { wptr (a1), 0, false, "delta zero" },
                                { wptr (a2), 0, false, "member" },
                                { wptr (a3), 0, false, "far" },
                                { wptr (a4), 0, false, "another far" },
                                { wptr (a5), 0, false, "null alias" },
                                { wptr (b1), 1, false, "other owner" },
                                { wptr (b2), 1, false, "other owner far" } };
  for (std::size_t i = 0; i < sizeof kinds / sizeof kinds[0]; ++i)
    set.kinds.push_back (kinds[i]);
  {
    sp::shared_ptr<Obj> dead = sp::make_shared<Obj> (33);
    sp::shared_ptr<int> const d1 (dead, &dead->v);
    sp::shared_ptr<int> const d2 (dead, shifted (dead.get (), far));
    weak_kind_t const expired[] = { { wptr (d1), 2, true, "expired" },
                                    { wptr (d2), 2, true, "expired far" } };
    set.kinds.push_back (expired[0]);
    set.kinds.push_back (expired[1]);
  }
  return set;
}

/// The weak ledger of the block of kind @p which while no atomic exists: the
/// implicit unit of the strong group while the object lives, plus one per
/// weak value naming it.
long
weak_base (weak_set_t const &set, int owner)
{
  long n = owner == 2 ? 0 : 1;
  for (std::size_t i = 0; i < set.kinds.size (); ++i)
    if (set.kinds[i].owner == owner)
      ++n;
  return n;
}

long
slot_units (weak_set_t const &set, int slot, int owner)
{
  return slot >= 0 && set.kinds[static_cast<std::size_t> (slot)].owner == owner
             ? 1
             : 0;
}

/// The block of the kind set's owner @p owner.
sp::detail::ctl_base *
owner_block (weak_set_t const &set, int owner)
{
  if (owner == 2)
    return block_of (set.kinds[8].value);
  return block_of (set.owners[static_cast<std::size_t> (owner)]);
}

void
expect_weak_counts (weak_set_t const &set, int slot,
                    std::vector<int> const &held, std::string const &what)
{
  for (int o = 0; o < 3; ++o)
    {
      long want = weak_base (set, o) + slot_units (set, slot, o);
      for (std::size_t h = 0; h < held.size (); ++h)
        want += slot_units (set, held[h], o);
      sp::detail::ctl_base *const block = owner_block (set, o);
      counter_t::word_type const word = block->weak_counter ().load ();
      EXPECT_EQ (counter_t::count_of (word), want)
          << what << ": weak count of " << o;
      EXPECT_EQ (counter_t::ext_of (word), 0) << what << ": weak ext of " << o;
      EXPECT_EQ (counter_t::ext_of (block->strong_counter ().load ()), 0);
      if (o < 2)
        EXPECT_EQ (static_cast<long> (
                       set.owners[static_cast<std::size_t> (o)].use_count ()),
                   set.strong_base (o))
            << what << ": weak operations never change the strong count";
      else
        EXPECT_EQ (counter_t::count_of (block->strong_counter ().load ()), 0)
            << what << ": the dead owner stays dead";
    }
}

std::string
title (weak_set_t const &set, char const *op, int a, int b = -1, int c = -1)
{
  std::string text = op;
  text += ": ";
  text += set.kinds[static_cast<std::size_t> (a)].name;
  if (b >= 0)
    text += std::string (" / ") + set.kinds[static_cast<std::size_t> (b)].name;
  if (c >= 0)
    text += std::string (" / ") + set.kinds[static_cast<std::size_t> (c)].name;
  return text;
}

bool
same_kind (weak_set_t const &set, int a, int b)
{
  return same_weak (set.kinds[static_cast<std::size_t> (a)].value,
                    set.kinds[static_cast<std::size_t> (b)].value);
}

TEST (
    LumexSplitCountWeakTest,
    GivenTheKindSet_WhenBuilt_ThenTheDeadOwnerIsDeadAndTheHoldersAreWhereExpected)
{
  weak_set_t const set = make_weak_set ();
  // The null alias is a holder only where the heap is more than 2^39 bytes
  // away from address zero; its expectation is computed, not listed.
  bool const holder[]
      = { false, false, false, true, true, false, false, true, false, true };
  for (std::size_t i = 0; i < set.kinds.size (); ++i)
    {
      if (i != 5)
        {
          EXPECT_EQ (weak_needs_holder (set.kinds[i].value), holder[i])
              << set.kinds[i].name;
        }
      EXPECT_EQ (set.kinds[i].value.expired (),
                 set.kinds[i].expired || set.kinds[i].owner < 0)
          << set.kinds[i].name;
    }
}

TEST (LumexSplitCountWeakTest,
      GivenEveryKind_WhenLoaded_ThenTheWeakPointerComesBackWithTheRightCounts)
{
  warm_up_framework ();
  weak_set_t const set = make_weak_set ();
  long const live = live_allocations ().load ();
  for (std::size_t i = 0; i < set.kinds.size (); ++i)
    {
      int const k = static_cast<int> (i);
      SCOPED_TRACE (title (set, "load", k));
      wptr const &v = set.kinds[i].value;
      unsigned long const before = new_calls ();
      weak_atomic a (v);
      expect_allocations (before, weak_needs_holder (v) ? 1ul : 0ul);
      expect_weak_counts (set, k, std::vector<int> (), "constructed");
      unsigned long const loads = new_calls ();
      {
        wptr r = a.load ();
        expect_allocations (loads, 0ul);
        EXPECT_TRUE (same_weak (r, v));
        EXPECT_EQ (r.expired (), v.expired ());
        EXPECT_EQ (r.lock ().use_count () > 0, !v.expired ());
        expect_weak_counts (set, k, std::vector<int> (1, k), "loaded");
        wptr converted = a;
        EXPECT_TRUE (same_weak (converted, v));
      }
      expect_weak_counts (set, k, std::vector<int> (), "after the loads");
    }
  EXPECT_EQ (live_allocations ().load (), live);
}

TEST (LumexSplitCountWeakTest,
      GivenEveryPairOfKinds_WhenStoredAndExchanged_ThenValuesAndCountsAreRight)
{
  warm_up_framework ();
  weak_set_t const set = make_weak_set ();
  long const live = live_allocations ().load ();
  std::size_t const n = set.kinds.size ();
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j)
      {
        int const x = static_cast<int> (i);
        int const y = static_cast<int> (j);
        wptr const &v = set.kinds[i].value;
        wptr const &w = set.kinds[j].value;
        {
          SCOPED_TRACE (title (set, "store", x, y));
          weak_atomic a (v);
          unsigned long const before = new_calls ();
          a.store (w);
          expect_allocations (before, weak_needs_holder (w) ? 1ul : 0ul);
          expect_weak_counts (set, y, std::vector<int> (), "stored");
          wptr r = a.load ();
          EXPECT_TRUE (same_weak (r, w));
        }
        {
          SCOPED_TRACE (title (set, "exchange", x, y));
          weak_atomic a (v);
          unsigned long const before = new_calls ();
          wptr old = a.exchange (w);
          expect_allocations (before, weak_needs_holder (w) ? 1ul : 0ul);
          EXPECT_TRUE (same_weak (old, v));
          expect_weak_counts (set, y, std::vector<int> (1, x), "exchanged");
        }
        expect_weak_counts (set, -1, std::vector<int> (), "after both");
      }
  EXPECT_EQ (live_allocations ().load (), live);
}

TEST (
    LumexSplitCountWeakTest,
    GivenEveryTripleOfKinds_WhenCompareExchanged_ThenItSucceedsExactlyOnEquivalence)
{
  warm_up_framework ();
  weak_set_t const set = make_weak_set ();
  long const live = live_allocations ().load ();
  std::size_t const n = set.kinds.size ();
  for (int form = 0; form < 2; ++form)
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t e = 0; e < n; ++e)
        for (std::size_t d = 0; d < n; ++d)
          {
            int const x = static_cast<int> (i);
            int const y = static_cast<int> (e);
            int const z = static_cast<int> (d);
            SCOPED_TRACE (
                title (set, form == 0 ? "cas strong" : "cas weak", x, y, z));
            wptr const &v = set.kinds[i].value;
            wptr expected = set.kinds[e].value;
            wptr const &desired = set.kinds[d].value;
            bool const match = same_kind (set, x, y);
            weak_atomic a (v);
            unsigned long const before = new_calls ();
            bool const ok = form == 0
                                ? a.compare_exchange_strong (expected, desired)
                                : a.compare_exchange_weak (expected, desired);
            EXPECT_EQ (ok, match);
            if (ok)
              {
                expect_allocations (before,
                                    weak_needs_holder (desired) ? 1ul : 0ul);
                expect_weak_counts (set, z, std::vector<int> (1, y),
                                    "success");
                wptr r = a.load ();
                EXPECT_TRUE (same_weak (r, desired));
              }
            else
              {
                expect_allocations (before, 0ul);
                EXPECT_TRUE (same_weak (expected, v));
                expect_weak_counts (set, x, std::vector<int> (1, x),
                                    "failure");
              }
          }
  EXPECT_EQ (live_allocations ().load (), live);
}

TEST (
    LumexSplitCountWeakTest,
    GivenAWeakHolder_WhenTheOwnerDies_ThenTheHolderKeepsTheBlockAndNotTheObject)
{
  long const alive = alive_objects ().load ();
  warm_up_framework ();
  long const live_before = live_allocations ().load ();
  weak_atomic a;
  {
    sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
    sp::shared_ptr<int> const alias (
        owner, shifted (owner.get (), std::int64_t (1) << 41));
    a.store (wptr (alias));
    EXPECT_EQ (owner.use_count (), 2) << "owner and alias: the slot is weak";
  }
  EXPECT_EQ (alive_objects ().load (), alive)
      << "the weak holder does not keep the object alive";
  if (alloc_hooks_active ())
    {
      EXPECT_GT (live_allocations ().load (), live_before)
          << "the block and the holder are still allocated";
    }
  wptr r = a.load ();
  EXPECT_TRUE (r.expired ());
  EXPECT_EQ (r.lock ().use_count (), 0);
  r = wptr ();
  a.store (wptr ());
  EXPECT_EQ (live_allocations ().load (), live_before)
      << "clearing the slot freed the holder and the block";
}

TEST (
    LumexSplitCountWeakTest,
    GivenAnExpiredAlias_WhenCompareExchangedWithTheSameBlockAndAnotherPointer_ThenItIsNotEquivalent)
{
  weak_set_t const set = make_weak_set ();
  weak_atomic a (set.kinds[8].value);
  wptr expected = set.kinds[9].value;
  EXPECT_FALSE (a.compare_exchange_strong (expected, wptr ()))
      << "the same dead block, another stored pointer";
  EXPECT_TRUE (same_weak (expected, set.kinds[8].value));
  EXPECT_TRUE (a.compare_exchange_strong (expected, wptr ()))
      << "equivalent: the same block and the same stored pointer";
  EXPECT_TRUE (a.load ().expired ());
}
} // namespace

#else

TEST (LumexSplitCountWeakTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

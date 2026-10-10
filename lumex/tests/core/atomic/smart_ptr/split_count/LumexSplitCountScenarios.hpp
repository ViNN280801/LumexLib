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

// The scenarios that more than one split-count test file runs, as templates on
// an engine or a policy: the single-thread semantics table (every operation
// crossed with every value kind) and the multi-thread stress shapes with
// their oracles. The semantics file runs the table on the production policy,
// the torn file runs the same templates on a policy whose speculative reads
// are torn, and the ledger file runs the shapes on ledger policies.
//
// The oracles come from the plan, never from what the engine returns:
//   - two pointers are equivalent when they store the same pointer and either
//     share ownership or are both empty;
//   - a stored pointer goes through a holder exactly when its offset from the
//     anchor of its block is outside [-2^39, 2^39);
//   - at quiescence a block's strong `count` is the number of owners that the
//     test can count, `ext` is zero, and the ledger identities hold;
//   - a pointer in a stress shape may be seen with fewer owners than the
//     test holds never.
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SCENARIOS_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SCENARIOS_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace split_test
{
/// An engine for the scenario templates: the cell with a policy.
template <typename Policy> struct policy_engine_t
{
  /// Whether the allocations of holders can be counted exactly.
  static bool const strict_allocations = !tears_t<Policy>::value;

  template <typename T> using shared = shared_cell<T, Policy>;
  template <typename T> using weak = weak_cell<T, Policy>;
};

/// The engine of the public classes (the production policy).
struct public_engine_t
{
  static bool const strict_allocations = true;

  template <typename T>
  using shared = asp::atomic_shared_ptr_lock_free_split_count<T>;
  template <typename T>
  using weak = asp::atomic_weak_ptr_lock_free_split_count<T>;
};

/// One value of the semantics table: a pointer and the index of its owner
/// (-1: none).
struct kind_t
{
  sp::shared_ptr<int> value;
  int owner;
  char const *name;
};

/// The values of the table over two owners, and the counts they imply.
struct kind_set_t
{
  std::vector<sp::shared_ptr<Obj>> owners;
  std::vector<kind_t> kinds;

  /// The owners of owner @p index while no atomic exists: the owner variable
  /// and every value naming it.
  long
  base_owners (int index) const
  {
    long n = 1;
    for (std::size_t i = 0; i < kinds.size (); ++i)
      if (kinds[i].owner == index)
        ++n;
    return n;
  }
};

inline int &
sink_a ()
{
  static int value = 0;
  return value;
}

inline int &
sink_b ()
{
  static int value = 0;
  return value;
}

/// The kinds of value of plan item 1, with the boundary aliases at
/// `anchor + 2^39 - 1` (a block word) and `anchor + 2^39` (a holder), and the
/// same two below the anchor.
inline kind_set_t
make_kind_set ()
{
  kind_set_t set;
  set.owners.push_back (sp::make_shared<Obj> (11));
  set.owners.push_back (sp::make_shared<Obj> (22));
  Obj *const o1 = set.owners[0].get ();
  Obj *const o2 = set.owners[1].get ();
  sp::shared_ptr<int> const none;
  std::int64_t const half = std::int64_t (1) << 39;
  std::int64_t const far = std::int64_t (1) << 41;
  kind_t const kinds[]
      = { { sp::shared_ptr<int> (), -1, "empty" },
          { sp::shared_ptr<int> (none, &sink_a ()), -1, "empty alias a" },
          { sp::shared_ptr<int> (none, &sink_b ()), -1, "empty alias b" },
          { sp::shared_ptr<int> (set.owners[0], &o1->v), 0, "delta zero" },
          { sp::shared_ptr<int> (set.owners[0], &o1->pad[1]), 0, "member" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, -64)), 0,
            "below the anchor" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, far)), 0, "far" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, far + 8)), 0,
            "another far" },
          { sp::shared_ptr<int> (set.owners[0], static_cast<int *> (nullptr)),
            0, "null alias" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, half - 1)), 0,
            "last in window" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, half)), 0,
            "first out above" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, -half)), 0,
            "lowest in window" },
          { sp::shared_ptr<int> (set.owners[0], shifted (o1, -half - 1)), 0,
            "first out below" },
          { sp::shared_ptr<int> (set.owners[1], &o2->v), 1, "other owner" },
          { sp::shared_ptr<int> (set.owners[1], shifted (o2, far)), 1,
            "other owner far" } };
  for (std::size_t i = 0; i < sizeof kinds / sizeof kinds[0]; ++i)
    set.kinds.push_back (kinds[i]);
  return set;
}

/**
 * @brief The semantics table on one engine.
 * @details Single thread. Every check is derived from the plan: equivalence,
 * owner counts (the slot owns one unit of its value, a holder word one unit of
 * the owner, a loaded pointer one more), allocations of holders (one per
 * install of an out-of-window value, none otherwise), and prompt destruction.
 */
template <typename Engine> class semantics_t
{
public:
  typedef sp::shared_ptr<int> ptr;
  typedef typename Engine::template shared<int> atomic_type;

  /// The expected strong count of owner @p owner: the base, the slot if it
  /// holds a value of that owner, and one per held value.
  static long
  expected_use (kind_set_t const &set, int owner, int slot,
                std::vector<int> const &held)
  {
    long n = set.base_owners (owner);
    if (slot >= 0 && set.kinds[static_cast<std::size_t> (slot)].owner == owner)
      ++n;
    for (std::size_t i = 0; i < held.size (); ++i)
      if (set.kinds[static_cast<std::size_t> (held[i])].owner == owner)
        ++n;
    return n;
  }

  /// Checks the counts of both owners and their quiescence.
  static void
  expect_counts (kind_set_t const &set, int slot, std::vector<int> const &held,
                 std::string const &what)
  {
    for (std::size_t o = 0; o < set.owners.size (); ++o)
      {
        long const want = expected_use (set, static_cast<int> (o), slot, held);
        expect_exact (set.owners[o], want, (what + " owner").c_str ());
      }
  }

  /// The weak form may fail although the values are equivalent (the
  /// standard allows it; a policy that tears every guess provokes it again
  /// and again, because the stale guess of the next attempt fails its
  /// whole-word compare); the caller of a weak form retries a bounded number
  /// of times while the current value is still the expected one.
  template <typename Call>
  static bool
  weak_decided (ptr &expected, Call const &call, int &retries)
  {
    ptr const original = expected;
    for (int i = 0; i < 64; ++i)
      {
        if (call (expected))
          return true;
        if (!same_value (expected, original))
          return false;
        ++retries;
      }
    return false;
  }

  static bool
  strict ()
  {
    return Engine::strict_allocations;
  }

  static bool
  equivalent (kind_set_t const &set, int a, int b)
  {
    return same_value (set.kinds[static_cast<std::size_t> (a)].value,
                       set.kinds[static_cast<std::size_t> (b)].value);
  }

  static std::string
  title (kind_set_t const &set, char const *op, int a, int b = -1, int c = -1)
  {
    std::string text = op;
    text += ": ";
    text += set.kinds[static_cast<std::size_t> (a)].name;
    if (b >= 0)
      {
        text += " / ";
        text += set.kinds[static_cast<std::size_t> (b)].name;
      }
    if (c >= 0)
      {
        text += " / ";
        text += set.kinds[static_cast<std::size_t> (c)].name;
      }
    return text;
  }

  /// Construct, load, drop: the result, the counts, the allocations.
  static void
  load_table ()
  {
    warm_up_framework ();
    kind_set_t set = make_kind_set ();
    long const live = live_allocations ().load ();
    for (std::size_t i = 0; i < set.kinds.size (); ++i)
      {
        int const k = static_cast<int> (i);
        SCOPED_TRACE (title (set, "load", k));
        ptr const &v = set.kinds[i].value;
        unsigned long const before = new_calls ();
        atomic_type a (v);
        expect_allocations (
            before, needs_holder (v) ? 1ul : 0ul,
            "a holder is allocated exactly for an out-of-window value");
        expect_counts (set, k, std::vector<int> (), "after construction");
        unsigned long const loads = new_calls ();
        {
          ptr r = a.load ();
          expect_allocations (loads, 0ul, "a load allocates nothing");
          EXPECT_EQ (r.get (), v.get ());
          EXPECT_TRUE (same_value (r, v));
          expect_counts (set, k, std::vector<int> (1, k), "while loaded");
          ptr converted = a;
          EXPECT_TRUE (same_value (converted, v));
          std::vector<int> two (2, k);
          expect_counts (set, k, two, "loaded twice");
        }
        expect_counts (set, k, std::vector<int> (), "after the loads");
      }
    EXPECT_EQ (live_allocations ().load (), live) << "no holder leaked";
  }

  /// Every ordered pair for store and exchange, and operator=.
  static void
  replace_table ()
  {
    warm_up_framework ();
    kind_set_t set = make_kind_set ();
    long const live = live_allocations ().load ();
    std::size_t const n = set.kinds.size ();
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j)
        {
          int const x = static_cast<int> (i);
          int const y = static_cast<int> (j);
          ptr const &v = set.kinds[i].value;
          ptr const &w = set.kinds[j].value;
          {
            SCOPED_TRACE (title (set, "store", x, y));
            atomic_type a (v);
            unsigned long const before = new_calls ();
            a.store (w);
            expect_allocations (before, needs_holder (w) ? 1ul : 0ul);
            expect_counts (set, y, std::vector<int> (), "after the store");
            ptr r = a.load ();
            EXPECT_TRUE (same_value (r, w));
            r.reset ();
            a = v;
            r = a.load ();
            EXPECT_TRUE (same_value (r, v)) << "operator=";
            r.reset ();
            expect_counts (set, x, std::vector<int> (), "after operator=");
          }
          {
            SCOPED_TRACE (title (set, "exchange", x, y));
            atomic_type a (v);
            unsigned long const before = new_calls ();
            ptr old = a.exchange (w);
            expect_allocations (before, needs_holder (w) ? 1ul : 0ul);
            EXPECT_TRUE (same_value (old, v)) << "the previous value";
            expect_counts (set, y, std::vector<int> (1, x),
                           "after the exchange");
            old.reset ();
            expect_counts (set, y, std::vector<int> (), "result dropped");
            ptr now = a.load ();
            EXPECT_TRUE (same_value (now, w));
          }
          expect_counts (set, -1, std::vector<int> (), "after both");
        }
    EXPECT_EQ (live_allocations ().load (), live) << "no holder leaked";
  }

  /// Every triple (slot, expected, desired) for the four compare-exchange
  /// forms.
  static void
  cas_table ()
  {
    warm_up_framework ();
    kind_set_t set = make_kind_set ();
    long const live = live_allocations ().load ();
    std::size_t const n = set.kinds.size ();
    for (int form = 0; form < 4; ++form)
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t e = 0; e < n; ++e)
          for (std::size_t d = 0; d < n; ++d)
            {
              int const x = static_cast<int> (i);
              int const y = static_cast<int> (e);
              int const z = static_cast<int> (d);
              SCOPED_TRACE (title (set,
                                   form == 0   ? "cas strong"
                                   : form == 1 ? "cas weak"
                                   : form == 2 ? "cas strong one order"
                                               : "cas weak one order",
                                   x, y, z));
              ptr const &v = set.kinds[i].value;
              ptr expected = set.kinds[e].value;
              ptr const &desired = set.kinds[d].value;
              bool const match = equivalent (set, x, y);
              atomic_type a (v);
              unsigned long const before = new_calls ();
              bool ok = false;
              int retries = 0;
              if (form == 0)
                ok = a.compare_exchange_strong (expected, desired,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
              else if (form == 1)
                ok = weak_decided (
                    expected,
                    [&] (ptr &current_expected)
                      {
                        return a.compare_exchange_weak (
                            current_expected, desired,
                            std::memory_order_seq_cst,
                            std::memory_order_seq_cst);
                      },
                    retries);
              else if (form == 2)
                ok = a.compare_exchange_strong (expected, desired,
                                                std::memory_order_acq_rel);
              else
                ok = weak_decided (
                    expected,
                    [&] (ptr &current_expected)
                      {
                        return a.compare_exchange_weak (current_expected,
                                                        desired);
                      },
                    retries);
              if (strict () || form == 0 || form == 2)
                {
                  EXPECT_EQ (ok, match)
                      << "succeeds exactly when the slot is equivalent to "
                         "expected";
                }
              else if (ok)
                {
                  EXPECT_TRUE (match) << "a weak form never succeeds wrongly";
                }
              if (ok)
                {
                  if (strict ())
                    expect_allocations (before,
                                        needs_holder (desired) ? 1ul : 0ul);
                  EXPECT_TRUE (same_value (expected, set.kinds[e].value))
                      << "expected is left alone on success";
                  expect_counts (set, z, std::vector<int> (1, y),
                                 "after a success");
                  ptr r = a.load ();
                  EXPECT_TRUE (same_value (r, desired));
                }
              else
                {
                  if (strict ())
                    expect_allocations (
                        before, 0ul,
                        "a failed compare-exchange allocates nothing");
                  EXPECT_TRUE (same_value (expected, v))
                      << "expected receives the current value";
                  EXPECT_EQ (expected.get (), v.get ());
                  expect_counts (set, x, std::vector<int> (1, x),
                                 "after a failure");
                  ptr r = a.load ();
                  EXPECT_TRUE (same_value (r, v)) << "the slot is unchanged";
                }
            }
    EXPECT_EQ (live_allocations ().load (), live) << "no holder leaked";
  }

  /// The failure order @p failure with the success order @p success under the
  /// rule of C++11 to C++14: no stronger than the success order, never
  /// `release` or `acq_rel` (the caller passes the four load orders).
  static bool
  failure_order_allowed (std::memory_order success, std::memory_order failure)
  {
    int rank_failure = failure == std::memory_order_relaxed   ? 0
                       : failure == std::memory_order_consume ? 1
                       : failure == std::memory_order_acquire ? 2
                                                              : 3;
    int limit = success == std::memory_order_relaxed   ? 0
                : success == std::memory_order_consume ? 1
                : success == std::memory_order_acquire ? 2
                : success == std::memory_order_release ? 0
                : success == std::memory_order_acq_rel ? 2
                                                       : 3;
    return rank_failure <= limit;
  }

  /// Every legal memory-order overload of every operation, on a block word
  /// and on a holder word.
  static void
  order_table ()
  {
    std::memory_order const all[]
        = { std::memory_order_relaxed, std::memory_order_consume,
            std::memory_order_acquire, std::memory_order_release,
            std::memory_order_acq_rel, std::memory_order_seq_cst };
    std::memory_order const loads[]
        = { std::memory_order_relaxed, std::memory_order_consume,
            std::memory_order_acquire, std::memory_order_seq_cst };
    std::memory_order const stores[]
        = { std::memory_order_relaxed, std::memory_order_release,
            std::memory_order_seq_cst };
    kind_set_t set = make_kind_set ();
    int const picks[] = { 3, 6 };
    for (std::size_t m = 0; m < 2; ++m)
      {
        ptr const &v = set.kinds[static_cast<std::size_t> (picks[m])].value;
        ptr const &w = set.kinds[static_cast<std::size_t> (13)].value;
        atomic_type a (v);
        for (std::size_t o = 0; o < 4; ++o)
          {
            ptr r = a.load (loads[o]);
            EXPECT_TRUE (same_value (r, v)) << "load order " << o;
          }
        for (std::size_t o = 0; o < 3; ++o)
          {
            a.store (w, stores[o]);
            ptr r = a.load ();
            EXPECT_TRUE (same_value (r, w)) << "store order " << o;
            a.store (v, stores[o]);
          }
        for (std::size_t o = 0; o < 6; ++o)
          {
            ptr old = a.exchange (w, all[o]);
            EXPECT_TRUE (same_value (old, v)) << "exchange order " << o;
            old = a.exchange (v, all[o]);
            EXPECT_TRUE (same_value (old, w));
          }
        for (std::size_t s = 0; s < 6; ++s)
          {
            for (std::size_t f = 0; f < 4; ++f)
              {
                // The failure order may not be stronger than the success
                // order before C++17 (and never release or acq_rel).
                if (!failure_order_allowed (all[s], loads[f]))
                  continue;
                ptr expected = v;
                EXPECT_TRUE (
                    a.compare_exchange_strong (expected, w, all[s], loads[f]))
                    << s << "/" << f;
                expected = v;
                EXPECT_FALSE (
                    a.compare_exchange_strong (expected, w, all[s], loads[f]));
                EXPECT_TRUE (same_value (expected, w));
                int retries = 0;
                if (!weak_decided (
                        expected,
                        [&] (ptr &current_expected)
                          {
                            return a.compare_exchange_weak (
                                current_expected, v, all[s], loads[f]);
                          },
                        retries))
                  {
                    EXPECT_FALSE (strict ()) << "the weak form failed";
                    expected = w;
                    EXPECT_TRUE (a.compare_exchange_strong (expected, v));
                  }
              }
            ptr expected = w;
            EXPECT_FALSE (a.compare_exchange_strong (expected, w, all[s]));
            EXPECT_TRUE (same_value (expected, v));
            EXPECT_TRUE (a.compare_exchange_strong (expected, w, all[s]));
            expected = w;
            int retries = 0;
            if (!weak_decided (
                    expected,
                    [&] (ptr &current_expected)
                      {
                        return a.compare_exchange_weak (current_expected, v,
                                                        all[s]);
                      },
                    retries))
              {
                EXPECT_FALSE (strict ()) << "the weak form failed";
                expected = w;
                EXPECT_TRUE (a.compare_exchange_strong (expected, v));
              }
          }
        ptr r = a.load ();
        EXPECT_TRUE (same_value (r, v));
      }
  }

  /// Construction, assignment, conversion, nullptr, lock-freedom.
  static void
  object_table ()
  {
    atomic_type empty;
    EXPECT_FALSE (static_cast<bool> (empty.load ()));
    EXPECT_TRUE (empty.is_lock_free ());
    // An ODR-use of the constant (needs the definition before C++17).
    bool const &always = atomic_type::is_always_lock_free;
    EXPECT_TRUE (always);
    atomic_type from_null (nullptr);
    EXPECT_FALSE (static_cast<bool> (from_null.load ()));
    sp::shared_ptr<Obj> p = sp::make_shared<Obj> (5);
    atomic_type held (ptr (p, &p->v));
    from_null = ptr (p, &p->v);
    EXPECT_EQ (p.use_count (), 3) << "p, held and from_null";
    ptr converted = held;
    EXPECT_EQ (converted.get (), &p->v);
    EXPECT_EQ (p.use_count (), 4);
    from_null = nullptr;
    EXPECT_FALSE (static_cast<bool> (from_null.load ()));
    EXPECT_EQ (p.use_count (), 3) << "assigning nullptr releases the value";
  }

  /// A replaced value dies in the call that replaces it when no load is in
  /// flight, and the destructor drops the value.
  static void
  destruction_table ()
  {
    long const alive = alive_objects ().load ();
    {
      sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
      atomic_type a (ptr (p, &p->v));
      p.reset ();
      EXPECT_EQ (alive_objects ().load (), alive + 1) << "the slot owns it";
      a.store (ptr ());
      EXPECT_EQ (alive_objects ().load (), alive)
          << "store destroyed the old value in the call";
    }
    {
      sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
      atomic_type a (ptr (p, shifted (p.get (), std::int64_t (1) << 41)));
      p.reset ();
      EXPECT_EQ (alive_objects ().load (), alive + 1)
          << "the holder owns the owner";
      a.store (ptr ());
      EXPECT_EQ (alive_objects ().load (), alive)
          << "store of a holder word destroyed the owner in the call";
    }
    {
      sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
      atomic_type a (ptr (p, &p->v));
      p.reset ();
      ptr old = a.exchange (ptr ());
      EXPECT_EQ (alive_objects ().load (), alive + 1)
          << "the result owns the old value";
      old.reset ();
      EXPECT_EQ (alive_objects ().load (), alive);
    }
    {
      sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
      sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
      atomic_type a (ptr (p, &p->v));
      ptr expected (q, &q->v);
      q.reset ();
      EXPECT_EQ (alive_objects ().load (), alive + 2);
      EXPECT_FALSE (a.compare_exchange_strong (expected, ptr ()));
      EXPECT_EQ (alive_objects ().load (), alive + 1)
          << "the failed compare-exchange replaced expected, the old "
             "expected was the last owner of q";
      static_cast<void> (p);
    }
    EXPECT_EQ (alive_objects ().load (), alive) << "the destructor dropped it";
    {
      sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
      sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
      atomic_type a (ptr (p, &p->v));
      ptr expected (p, &p->v);
      p.reset ();
      // desired is the only owner of q here: a successful compare-exchange
      // moves it into the slot, nothing dies; the old value is still held by
      // expected.
      EXPECT_TRUE (a.compare_exchange_strong (expected, ptr (q, &q->v)));
      q.reset ();
      EXPECT_EQ (alive_objects ().load (), alive + 2);
      expected.reset ();
      EXPECT_EQ (alive_objects ().load (), alive + 1)
          << "the old value died with the last owner, expected";
    }
    EXPECT_EQ (alive_objects ().load (), alive);
  }

  /// Everything of this class.
  static void
  run_all ()
  {
    object_table ();
    load_table ();
    replace_table ();
    cas_table ();
    order_table ();
    destruction_table ();
  }
};

/// The base-conversion kind: a `Base2` view of a `Derived` object, whose
/// offset from the anchor is not zero.
struct Base1
{
  virtual ~Base1 () {}
  long first;
};

struct Base2
{
  virtual ~Base2 () {}
  long second;
};

struct Derived : Base1, Base2
{
  Derived () { alive_objects ().fetch_add (1); }
  ~Derived () override { alive_objects ().fetch_sub (1); }
};

/// A `Base2` pointer converted from a `Derived` owner stores a pointer a few
/// bytes from the anchor: a block word with a nonzero delta.
template <typename Engine>
inline void
base_conversion_table ()
{
  typedef typename Engine::template shared<Base2> atomic_type;
  long const alive = alive_objects ().load ();
  {
    sp::shared_ptr<Derived> d = sp::make_shared<Derived> ();
    sp::shared_ptr<Base2> b2 = d;
    sp::shared_ptr<Base1> b1 = d;
    EXPECT_NE (static_cast<void *> (b2.get ()), static_cast<void *> (d.get ()))
        << "the test needs a nonzero base offset";
    EXPECT_FALSE (needs_holder (b2));
    atomic_type a (b2);
    EXPECT_EQ (d.use_count (), 4) << "d, b2, b1, the slot";
    sp::shared_ptr<Base2> r = a.load ();
    EXPECT_EQ (r.get (), b2.get ());
    EXPECT_EQ (d.use_count (), 5);
    sp::shared_ptr<Base2> e = b2;
    EXPECT_TRUE (a.compare_exchange_strong (e, sp::shared_ptr<Base2> ()));
    EXPECT_EQ (d.use_count (), 5) << "d, b2, b1, r, e";
    a.store (b2);
    sp::shared_ptr<Base2> x = a.exchange (sp::shared_ptr<Base2> ());
    EXPECT_EQ (x.get (), b2.get ());
    EXPECT_FALSE (static_cast<bool> (a.load ()));
  }
  EXPECT_EQ (alive_objects ().load (), alive);
}

// -- The stress shapes --

/// Work of one run of a shape: loads over all loader threads.
inline int
shape_work ()
{
  return lumex_test::scaled (72000);
}

/// The writers and loaders of a run on @p threads threads; one thread does
/// both.
inline int
shape_writers (int threads)
{
  return threads == 1 ? 0 : std::max (1, threads / 4);
}

/// Runs @p shape (threads, repetition) at every thread count of the shapes,
/// five times each.
template <typename Shape>
inline void
for_each_shape (Shape const &shape)
{
  std::vector<int> const counts = shape_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    for (int rep = 0; rep < shape_repetitions (); ++rep)
      {
        SCOPED_TRACE (
            lumex_test::replay_text (lumex_test::base_seed (), counts[c])
            + " repetition " + std::to_string (rep));
        shape (counts[c], rep);
        if (::testing::Test::HasFatalFailure ())
          return;
      }
}

/**
 * @brief The shared engine over plain objects: a pool of three values that
 * are stored again and again (A, B, A, the same block re-installed), fresh
 * values, loads that keep a few pointers.
 * @details Oracle at quiescence: the final value has `slot + cur (+ pool)`
 * owners, every other pool object exactly one, `ext` zero, the ledger
 * balanced, the peak of `L` at most the policy's limit, no object leaked.
 */
template <typename Policy>
inline void
stress_shared (int threads, int rep)
{
  reset_ledger<Policy> ();
  long const alive_before = alive_objects ().load ();
  {
    std::vector<sp::shared_ptr<Obj>> pool;
    for (int i = 0; i < 3; ++i)
      pool.push_back (sp::make_shared<Obj> (i));
    shared_cell<Obj, Policy> a (pool[0]);
    int const writers = shape_writers (threads);
    int const loaders = threads - writers;
    int const work = shape_work ();
    run_workers (
        threads,
        [&] (int index)
          {
            rng_t rng (lumex_test::derive_seed (
                lumex_test::base_seed (), static_cast<std::uint64_t> (index),
                static_cast<std::uint64_t> (rep)));
            bool const write = index < writers || threads == 1;
            bool const load = index >= writers || threads == 1;
            int const loads = load ? work / (threads == 1 ? 1 : loaders) : 0;
            int const writes
                = write ? work / (4 * (writers == 0 ? 1 : writers)) : 0;
            sp::shared_ptr<Obj> ring[3];
            int done_writes = 0;
            for (int k = 0; k < std::max (loads, writes); ++k)
              {
                if (k < loads)
                  {
                    sp::shared_ptr<Obj> p = a.load ();
                    EXPECT_TRUE (static_cast<bool> (p));
                    if (p)
                      {
                        EXPECT_GE (p->v, 0);
                      }
                    EXPECT_GE (p.use_count (), 1);
                    if (rng.next () % 3 == 0)
                      ring[rng.next () % 3] = std::move (p);
                  }
                if (write && done_writes < writes
                    && (threads == 1 ? k % 4 == 0 : true))
                  {
                    ++done_writes;
                    switch (rng.next () % 7)
                      {
                      case 0:
                        a.store (sp::make_shared<Obj> (static_cast<int> (k)));
                        break;
                      case 1:
                        a.store (pool[rng.next () % 3]);
                        break;
                      case 2:
                        a.exchange (
                            sp::make_shared<Obj> (static_cast<int> (k)));
                        break;
                      case 3:
                        a.exchange (pool[rng.next () % 3]);
                        break;
                      case 4:
                        {
                          sp::shared_ptr<Obj> e = a.load ();
                          a.compare_exchange_strong (
                              e, sp::make_shared<Obj> (static_cast<int> (k)));
                          break;
                        }
                      case 5:
                        {
                          sp::shared_ptr<Obj> e = pool[rng.next () % 3];
                          a.compare_exchange_strong (e, pool[rng.next () % 3]);
                          break;
                        }
                      default:
                        {
                          sp::shared_ptr<Obj> e = a.load ();
                          a.compare_exchange_weak (e, pool[rng.next () % 3]);
                          break;
                        }
                      }
                  }
              }
          });
    sp::shared_ptr<Obj> cur = a.load ();
    bool in_pool = false;
    for (std::size_t i = 0; i < pool.size (); ++i)
      if (pool[i] == cur)
        in_pool = true;
    expect_exact (cur, in_pool ? 3 : 2, "final value");
    for (std::size_t i = 0; i < pool.size (); ++i)
      if (!(pool[i] == cur))
        expect_exact (pool[i], 1, "pool value");
    check_balanced<Policy> ("shared shape");
    EXPECT_LE (Policy::counters ().peak.load (),
               static_cast<unsigned long> (Policy::tick_limit ()));
  }
  EXPECT_EQ (alive_objects ().load (), alive_before) << "no object leaked";
}

/**
 * @brief The shared engine over aliases: holders of two owners mixed with
 * in-window aliases, and observers of `ext` and `use_count`.
 * @details An observer samples both owners while the stress runs: `ext` is
 * never negative (invariant I2) and `use_count ()` is never below the owners
 * the test holds forever (G1). At quiescence the counts are exact.
 */
template <typename Policy>
inline void
stress_holder (int threads, int rep)
{
  warm_up_framework ();
  reset_ledger<Policy> ();
  long const alive_before = alive_objects ().load ();
  long const live_before = live_allocations ().load ();
  {
    sp::shared_ptr<Obj> o1 = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> o2 = sp::make_shared<Obj> (2);
    sp::shared_ptr<int> const values[6] = {
      sp::shared_ptr<int> (o1, shifted (o1.get (), std::int64_t (1) << 41)),
      sp::shared_ptr<int> (o2, shifted (o2.get (), std::int64_t (1) << 41)),
      sp::shared_ptr<int> (o1, &o1->v),
      sp::shared_ptr<int> (o2, &o2->v),
      sp::shared_ptr<int> (o1, static_cast<int *> (nullptr)),
      sp::shared_ptr<int> (o2, shifted (o2.get (), -128))
    };
    // Owners that exist whatever the slot holds: the variable and the three
    // values that name the owner.
    long const forever = 4;
    shared_cell<int, Policy> a (values[0]);
    int const writers = shape_writers (threads);
    int const loaders = threads - writers;
    int const work = shape_work ();
    std::atomic<bool> stop (false);
    std::atomic<long> min_use1 (1000000);
    std::atomic<long> min_use2 (1000000);
    std::atomic<long> min_ext (0);
    std::thread observer (
        [&]
          {
            while (!stop.load ())
              {
                words_t const w1 = words_of (o1);
                words_t const w2 = words_of (o2);
                long const u1 = static_cast<long> (o1.use_count ());
                long const u2 = static_cast<long> (o2.use_count ());
                if (w1.ext < min_ext.load ())
                  min_ext.store (w1.ext);
                if (w2.ext < min_ext.load ())
                  min_ext.store (w2.ext);
                if (u1 < min_use1.load ())
                  min_use1.store (u1);
                if (u2 < min_use2.load ())
                  min_use2.store (u2);
                std::this_thread::yield ();
              }
          });
    run_workers (
        threads,
        [&] (int index)
          {
            rng_t rng (lumex_test::derive_seed (
                lumex_test::base_seed (), static_cast<std::uint64_t> (index),
                static_cast<std::uint64_t> (rep) + 100));
            bool const write = index < writers || threads == 1;
            bool const load = index >= writers || threads == 1;
            int const loads = load ? work / (threads == 1 ? 1 : loaders) : 0;
            int const writes
                = write ? work / (4 * (writers == 0 ? 1 : writers)) : 0;
            sp::shared_ptr<int> ring[3];
            int done_writes = 0;
            for (int k = 0; k < std::max (loads, writes); ++k)
              {
                if (k < loads)
                  {
                    sp::shared_ptr<int> p = a.load ();
                    bool known = false;
                    for (int i = 0; i < 6; ++i)
                      known = known || (same_value (p, values[i]));
                    EXPECT_TRUE (known) << "a load returns one of the values";
                    if (p.get () == &o1->v)
                      {
                        EXPECT_EQ (*p, 1);
                      }
                    if (p.get () == &o2->v)
                      {
                        EXPECT_EQ (*p, 2);
                      }
                    if (rng.next () % 3 == 0)
                      ring[rng.next () % 3] = std::move (p);
                  }
                if (write && done_writes < writes
                    && (threads == 1 ? k % 4 == 0 : true))
                  {
                    ++done_writes;
                    sp::shared_ptr<int> const &v = values[rng.next () % 6];
                    switch (rng.next () % 4)
                      {
                      case 0:
                        a.store (v);
                        break;
                      case 1:
                        a.exchange (v);
                        break;
                      case 2:
                        {
                          sp::shared_ptr<int> e = a.load ();
                          a.compare_exchange_strong (e, v);
                          break;
                        }
                      default:
                        {
                          sp::shared_ptr<int> e = values[rng.next () % 6];
                          a.compare_exchange_strong (e, v);
                          break;
                        }
                      }
                  }
              }
          });
    stop.store (true);
    observer.join ();
    EXPECT_GE (min_ext.load (), 0) << "ext is never negative";
    EXPECT_GE (min_use1.load (), forever)
        << "use_count never below the owners";
    EXPECT_GE (min_use2.load (), forever)
        << "use_count never below the owners";
    sp::shared_ptr<int> cur = a.load ();
    long slot1 = 0;
    long slot2 = 0;
    // Which owner the final value names: a pointer into o1 or o2 by block.
    if (block_of (cur) == block_of (o1))
      slot1 = 1;
    else
      slot2 = 1;
    expect_exact (o1, forever + slot1 * 2, "owner one");
    expect_exact (o2, forever + slot2 * 2, "owner two");
    check_balanced<Policy> ("holder shape");
    EXPECT_LE (Policy::counters ().peak.load (),
               static_cast<unsigned long> (Policy::tick_limit ()));
    cur.reset ();
    a.store (nullptr);
    expect_exact (o1, forever, "owner one, slot empty");
    expect_exact (o2, forever, "owner two, slot empty");
  }
  EXPECT_EQ (alive_objects ().load (), alive_before) << "no object leaked";
  EXPECT_EQ (live_allocations ().load (), live_before)
      << "no holder or block leaked";
}

/**
 * @brief The weak engine over weak pointers made from aliases, with weak
 * holders for the out-of-window ones.
 * @details Loads lock the result (the owners are alive, so the lock
 * succeeds). The strong counts of the owners never change except by the
 * temporary locks; the weak ledger is exact at quiescence.
 */
template <typename Policy>
inline void
stress_weak (int threads, int rep)
{
  warm_up_framework ();
  reset_ledger<Policy> ();
  long const alive_before = alive_objects ().load ();
  long const live_before = live_allocations ().load ();
  {
    sp::shared_ptr<Obj> o1 = sp::make_shared<Obj> (1);
    sp::shared_ptr<Obj> o2 = sp::make_shared<Obj> (2);
    sp::shared_ptr<int> const shared_values[6] = {
      sp::shared_ptr<int> (o1, shifted (o1.get (), std::int64_t (1) << 41)),
      sp::shared_ptr<int> (o2, shifted (o2.get (), std::int64_t (1) << 41)),
      sp::shared_ptr<int> (o1, &o1->v),
      sp::shared_ptr<int> (o2, &o2->v),
      sp::shared_ptr<int> (o1, static_cast<int *> (nullptr)),
      sp::shared_ptr<int> (o2, shifted (o2.get (), -128))
    };
    sp::weak_ptr<int> const values[6]
        = { sp::weak_ptr<int> (shared_values[0]),
            sp::weak_ptr<int> (shared_values[1]),
            sp::weak_ptr<int> (shared_values[2]),
            sp::weak_ptr<int> (shared_values[3]),
            sp::weak_ptr<int> (shared_values[4]),
            sp::weak_ptr<int> (shared_values[5]) };
    long const strong_forever = 4;
    weak_cell<int, Policy> a (values[0]);
    int const writers = shape_writers (threads);
    int const loaders = threads - writers;
    int const work = shape_work ();
    std::atomic<bool> stop (false);
    std::atomic<long> min_ext (0);
    std::atomic<long> min_use (1000000);
    std::thread observer (
        [&]
          {
            while (!stop.load ())
              {
                words_t const w1 = words_of (o1);
                words_t const w2 = words_of (o2);
                long const u = static_cast<long> (o1.use_count ());
                if (w1.ext < min_ext.load ())
                  min_ext.store (w1.ext);
                if (w2.ext < min_ext.load ())
                  min_ext.store (w2.ext);
                if (w1.weak_ext < min_ext.load ())
                  min_ext.store (w1.weak_ext);
                if (w2.weak_ext < min_ext.load ())
                  min_ext.store (w2.weak_ext);
                if (u < min_use.load ())
                  min_use.store (u);
                std::this_thread::yield ();
              }
          });
    run_workers (
        threads,
        [&] (int index)
          {
            rng_t rng (lumex_test::derive_seed (
                lumex_test::base_seed (), static_cast<std::uint64_t> (index),
                static_cast<std::uint64_t> (rep) + 200));
            bool const write = index < writers || threads == 1;
            bool const load = index >= writers || threads == 1;
            int const loads = load ? work / (threads == 1 ? 1 : loaders) : 0;
            int const writes
                = write ? work / (4 * (writers == 0 ? 1 : writers)) : 0;
            sp::weak_ptr<int> ring[3];
            int done_writes = 0;
            for (int k = 0; k < std::max (loads, writes); ++k)
              {
                if (k < loads)
                  {
                    sp::weak_ptr<int> w = a.load ();
                    sp::shared_ptr<int> p = w.lock ();
                    EXPECT_GE (p.use_count (), 1) << "the owners are alive";
                    if (p && p.get () == &o1->v)
                      {
                        EXPECT_EQ (*p, 1);
                      }
                    if (rng.next () % 3 == 0)
                      ring[rng.next () % 3] = std::move (w);
                  }
                if (write && done_writes < writes
                    && (threads == 1 ? k % 4 == 0 : true))
                  {
                    ++done_writes;
                    sp::weak_ptr<int> const &v = values[rng.next () % 6];
                    switch (rng.next () % 4)
                      {
                      case 0:
                        a.store (v);
                        break;
                      case 1:
                        a.exchange (v);
                        break;
                      case 2:
                        {
                          sp::weak_ptr<int> e = a.load ();
                          a.compare_exchange_strong (e, v);
                          break;
                        }
                      default:
                        {
                          sp::weak_ptr<int> e = values[rng.next () % 6];
                          a.compare_exchange_strong (e, v);
                          break;
                        }
                      }
                  }
              }
          });
    stop.store (true);
    observer.join ();
    EXPECT_GE (min_ext.load (), 0) << "ext is never negative";
    EXPECT_GE (min_use.load (), strong_forever);
    sp::weak_ptr<int> cur = a.load ();
    bool const slot_is_o1 = block_of (cur) == block_of (o1);
    // Weak ledger at quiescence: the implicit unit, the three weak values of
    // the owner, the slot (a block word, or the weak reference of its
    // holder) and `cur`.
    words_t const w1 = words_of (o1);
    words_t const w2 = words_of (o2);
    EXPECT_EQ (w1.count, strong_forever);
    EXPECT_EQ (w2.count, strong_forever);
    EXPECT_EQ (w1.ext, 0);
    EXPECT_EQ (w2.ext, 0);
    EXPECT_EQ (w1.weak_ext, 0);
    EXPECT_EQ (w2.weak_ext, 0);
    EXPECT_EQ (w1.weak_count, 1 + 3 + (slot_is_o1 ? 2 : 0));
    EXPECT_EQ (w2.weak_count, 1 + 3 + (slot_is_o1 ? 0 : 2));
    check_balanced<Policy> ("weak shape");
    EXPECT_LE (Policy::counters ().peak.load (),
               static_cast<unsigned long> (Policy::tick_limit ()));
    cur.reset ();
    a.store (sp::weak_ptr<int> ());
    EXPECT_EQ (words_of (o1).weak_count, 1 + 3);
    EXPECT_EQ (words_of (o2).weak_count, 1 + 3);
  }
  EXPECT_EQ (alive_objects ().load (), alive_before) << "no object leaked";
  EXPECT_EQ (live_allocations ().load (), live_before)
      << "no holder or block leaked";
}
} // namespace split_test

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SCENARIOS_HPP

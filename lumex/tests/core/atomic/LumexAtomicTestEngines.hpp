/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexAtomicTestEngines.hpp
 * @brief Test-side atomic shared pointers with the API of `atomic_shared_ptr`:
 * a correct reference, deliberately broken copies of it, and a lock-free
 * box engine in a correct and a naive form.
 * @details The checkers of the concurrency suites (`LumexAtomicScenarios*`)
 * are falsified here without touching the library: each `broken_*` engine is
 * a copy of a correct implementation with one defect, and a self-test runs
 * the checkers against it and expects them to report it. The engines are
 * written on `std::shared_ptr` and share the API of `atomic_shared_ptr`
 * that the checkers use (`load`, `store`, `exchange`, `compare_exchange_*`).
 *
 * Mutex-based engines (`flawed_atomic_shared_ptr`): every access to the value
 * is under one mutex, so a defect is a logic error and not a data race, and
 * running the checker on it is safe in every build (except `cas_unlocked`,
 * whose point is the missing lock; its self-test runs in a child process):
 *
 * | Flaw | What is wrong | Who must see it |
 * | --- | --- | --- |
 * | `none` | nothing: the correct reference | nobody (the checkers must pass)
 * | | `cas_split_lock` | the compare-exchange locks twice, once to compare and
 * once to store | lost updates, two winners, linearizability | |
 * `cas_unlocked` | the compare-exchange does not take the lock at all: a data
 * race, unsafe, run in a child process | a crash, a sanitizer report or a lost
 * update | | `cas_pointer_only` | equivalence compares the stored pointer only
 * | the address-reuse and aliasing identity checks | | `cas_owner_only` |
 * equivalence compares the owner only | the aliasing identity checks | |
 * `exchange_split` | the exchange is a load followed by a store | the exchange
 * permutation, linearizability | | `store_keeps_old` | the store forgets to
 * release the replaced value | the lifetime and ledger checks | | `stale_load`
 * | every fourth load returns the value before the last store | the
 * monotone-load and linearizability checks |
 *
 * Box engines (`box_atomic_shared_ptr`): an immutable heap box behind one
 * `std::atomic<Box*>`, the shape of the lock-free engine, with the replaced
 * box either kept until the engine dies (`leak_until_destruction`: correct,
 * but the replaced value outlives the call, like a deferred reclamation),
 * kept and published with relaxed orders (`leak_with_relaxed_publication`: a
 * load that returns without acquiring, a data race that only
 * ThreadSanitizer reports) or deleted at once (`delete_at_once`: the naive
 * ABA victim). Boxes come from
 * a `ReusePool`, so the address of a deleted box is the address of the next
 * box. The pool of the naive engine does not poison, so the unsafe reads of
 * the self-tests hit live memory and not a sanitizer report.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_ENGINES_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_ENGINES_HPP

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#include "lumex/tests/support/LumexTestReusePool.hpp"

namespace lumex_atomic_test
{
/// Points of the engines where a test may stall or perturb a thread.
struct EngineHooks
{
  /// Called by the engines between the check and the act of a
  /// compare-exchange (only the engines that have a gap there).
  static std::function<void ()> &
  cas_gap ()
  {
    static std::function<void ()> hook;
    return hook;
  }

  static void
  run_cas_gap ()
  {
    std::function<void ()> &hook = cas_gap ();
    if (hook)
      hook ();
  }
};

/// True for the empty shared pointer that owns nothing and stores null.
template <typename T>
bool
is_plain_empty (std::shared_ptr<T> const &p)
{
  std::shared_ptr<T> const none;
  return p.get () == nullptr && !p.owner_before (none)
         && !none.owner_before (p);
}

/// Same stored pointer and same owner (the equivalence of the standard).
template <typename T>
bool
equivalent_values (std::shared_ptr<T> const &lhs,
                   std::shared_ptr<T> const &rhs)
{
  return lhs.get () == rhs.get () && !lhs.owner_before (rhs)
         && !rhs.owner_before (lhs);
}

/**
 * @brief Equality of the pointees, for the engine that (wrongly) compares by
 * content. The primary template says "different"; the checkers specialize it
 * for the types they store.
 */
template <typename T> struct ContentEqual
{
  static bool
  equal (T const &, T const &)
  {
    return false;
  }
};

template <> struct ContentEqual<int>
{
  static bool
  equal (int const &lhs, int const &rhs)
  {
    return lhs == rhs;
  }
};

// --- the mutex-based engines ------------------------------------------------

enum class Flaw
{
  none,
  cas_split_lock,
  cas_unlocked,
  cas_pointer_only,
  cas_owner_only,
  cas_content_equal,
  strong_fails_spuriously,
  exchange_split,
  store_keeps_old,
  store_leaks_forever,
  stale_load
};

/// Name of a flaw, for messages.
inline char const *
flaw_name (Flaw flaw)
{
  switch (flaw)
    {
    case Flaw::none:
      return "reference";
    case Flaw::cas_split_lock:
      return "broken_cas_split_lock";
    case Flaw::cas_unlocked:
      return "broken_cas_unlocked";
    case Flaw::cas_pointer_only:
      return "broken_cas_pointer_only";
    case Flaw::cas_owner_only:
      return "broken_cas_owner_only";
    case Flaw::cas_content_equal:
      return "broken_cas_content_equal";
    case Flaw::strong_fails_spuriously:
      return "broken_strong_fails_spuriously";
    case Flaw::exchange_split:
      return "broken_exchange_split";
    case Flaw::store_keeps_old:
      return "broken_store_keeps_old";
    case Flaw::store_leaks_forever:
      return "broken_store_leaks_forever";
    case Flaw::stale_load:
      return "broken_stale_load";
    }
  return "?";
}

/**
 * @brief A mutex-guarded atomic shared pointer with one chosen defect.
 * @tparam T Element type.
 * @tparam F The defect (`Flaw::none` is the correct reference).
 */
template <typename T, Flaw F> class flawed_atomic_shared_ptr
{
public:
  typedef std::shared_ptr<T> value_type;

  flawed_atomic_shared_ptr () : loads_ (0) {}
  flawed_atomic_shared_ptr (value_type desired)
      : value_ (std::move (desired)), loads_ (0)
  {
  }

  flawed_atomic_shared_ptr (flawed_atomic_shared_ptr const &) = delete;
  flawed_atomic_shared_ptr &operator= (flawed_atomic_shared_ptr const &)
      = delete;

  bool
  is_lock_free () const
  {
    return false;
  }

  value_type
  load (std::memory_order = std::memory_order_seq_cst) const
  {
    std::lock_guard<std::mutex> guard (mutex_);
    if (F == Flaw::stale_load && (++loads_ % 4u) == 0u && previous_)
      return previous_;
    return value_;
  }

  void
  store (value_type desired, std::memory_order = std::memory_order_seq_cst)
  {
    value_type old;
    {
      std::lock_guard<std::mutex> guard (mutex_);
      old = value_;
      value_ = std::move (desired);
      remember (old);
      if (F == Flaw::store_keeps_old)
        graveyard_.push_back (old);
      if (F == Flaw::store_leaks_forever)
        leak_one_reference (old);
    }
    // old is released here, after the lock, like the real engines do.
  }

  value_type
  exchange (value_type desired, std::memory_order = std::memory_order_seq_cst)
  {
    if (F == Flaw::exchange_split)
      {
        value_type seen = load ();
        EngineHooks::run_cas_gap ();
        store (std::move (desired));
        return seen;
      }
    std::lock_guard<std::mutex> guard (mutex_);
    value_type old = value_;
    value_ = std::move (desired);
    remember (old);
    return old;
  }

  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order, std::memory_order)
  {
    return compare_exchange (expected, std::move (desired), true);
  }

  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order = std::memory_order_seq_cst)
  {
    return compare_exchange (expected, std::move (desired), true);
  }

  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order, std::memory_order)
  {
    return compare_exchange (expected, std::move (desired), false);
  }

  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order = std::memory_order_seq_cst)
  {
    return compare_exchange (expected, std::move (desired), false);
  }

private:
  /// Strong calls of every object of this type (the spurious failure of
  /// every third one needs a count that outlives short-lived objects).
  static std::atomic<unsigned> &
  strong_calls ()
  {
    static std::atomic<unsigned> calls (0);
    return calls;
  }

  /// Takes a reference of @p old that is never released (a real leak).
  static void
  leak_one_reference (value_type const &old)
  {
    if (old)
      new value_type (old);
  }

  /// Keeps the replaced value for the stale load (and for nothing else).
  void
  remember (value_type const &old)
  {
    if (F == Flaw::stale_load)
      previous_ = old;
  }

  bool
  same (value_type const &lhs, value_type const &rhs) const
  {
    if (F == Flaw::cas_pointer_only)
      return lhs.get () == rhs.get ();
    if (F == Flaw::cas_owner_only)
      return !lhs.owner_before (rhs) && !rhs.owner_before (lhs);
    if (F == Flaw::cas_content_equal)
      return (!lhs && !rhs)
             || (lhs && rhs && ContentEqual<T>::equal (*lhs, *rhs));
    return equivalent_values (lhs, rhs);
  }

  bool
  compare_exchange (value_type &expected, value_type desired, bool strong)
  {
    if (F == Flaw::cas_unlocked)
      {
        // The defect: no lock. Concurrent callers race on value_.
        if (same (value_, expected))
          {
            value_type old = value_;
            value_ = std::move (desired);
            remember (old);
            return true;
          }
        expected = value_;
        return false;
      }
    if (F == Flaw::cas_split_lock)
      {
        value_type seen;
        {
          std::lock_guard<std::mutex> guard (mutex_);
          seen = value_;
        }
        if (!same (seen, expected))
          {
            expected = std::move (seen);
            return false;
          }
        // The defect: the lock is dropped between the comparison and the
        // store, so another thread can change the value in the gap.
        EngineHooks::run_cas_gap ();
        value_type old;
        {
          std::lock_guard<std::mutex> guard (mutex_);
          old = value_;
          value_ = std::move (desired);
          remember (old);
        }
        return true;
      }
    value_type old;
    value_type current;
    bool done = false;
    {
      std::lock_guard<std::mutex> guard (mutex_);
      bool matches = same (value_, expected);
      if (matches && F == Flaw::strong_fails_spuriously && strong
          && (strong_calls ().fetch_add (1u) % 3u) == 2u)
        matches = false; // the defect: a strong form that fails spuriously
      if (matches)
        {
          old = value_;
          value_ = std::move (desired);
          remember (old);
          done = true;
        }
      else
        current = value_;
    }
    if (!done)
      expected = std::move (current);
    return done;
  }

  mutable std::mutex mutex_;
  value_type value_;
  value_type previous_;
  std::vector<value_type> graveyard_;
  mutable unsigned loads_;
};

// --- the box engines --------------------------------------------------------

enum class BoxReclaim
{
  /// The replaced box is kept until the engine is destroyed. Safe, and the
  /// replaced value is destroyed late, as with a deferred reclamation.
  leak_until_destruction,
  /// As `leak_until_destruction`, but the box is published and read with
  /// relaxed orders: no release/acquire pair orders the construction of a
  /// box before the reads of it. Correct on x86 hardware, a data race for
  /// ThreadSanitizer; only that sanitizer can see it.
  leak_with_relaxed_publication,
  /// The replaced box is deleted at once, its address goes back to the pool
  /// and the next box takes it: the textbook ABA victim.
  delete_at_once
};

/// The pool that supplies the boxes of one reclamation mode. The naive mode
/// does not poison a freed box: its self-tests read the box on purpose.
template <BoxReclaim R> struct BoxPool
{
  struct Holder
  {
    Holder () { pool.set_poisoning (R != BoxReclaim::delete_at_once); }
    lumex_test::ReusePool pool;
  };

  static lumex_test::ReusePool &
  get ()
  {
    static Holder holder;
    return holder.pool;
  }
};

/**
 * @brief Atomic shared pointer as an immutable box behind one pointer word.
 * @tparam T Element type.
 * @tparam R When the replaced box is freed.
 * @details Null in the word means "no owner, null pointer". The retired
 * boxes of `leak_until_destruction` are freed by the destructor.
 */
template <typename T, BoxReclaim R> class box_atomic_shared_ptr
{
  struct Box
  {
    explicit Box (std::shared_ptr<T> v) : value (std::move (v)) {}
    std::shared_ptr<T> value;
  };

public:
  typedef std::shared_ptr<T> value_type;

  box_atomic_shared_ptr () : word_ (nullptr) {}
  box_atomic_shared_ptr (value_type desired) : word_ (make_box (desired)) {}

  box_atomic_shared_ptr (box_atomic_shared_ptr const &) = delete;
  box_atomic_shared_ptr &operator= (box_atomic_shared_ptr const &) = delete;

  ~box_atomic_shared_ptr ()
  {
    destroy_box (word_.load (std::memory_order_relaxed));
    for (std::size_t i = 0; i < retired_.size (); ++i)
      destroy_box (retired_[i]);
  }

  bool
  is_lock_free () const
  {
    return true;
  }

  value_type
  load (std::memory_order = std::memory_order_seq_cst) const
  {
    Box *box = word_.load (order ());
    return box == nullptr ? value_type () : box->value;
  }

  void
  store (value_type desired, std::memory_order = std::memory_order_seq_cst)
  {
    Box *fresh = make_box (desired);
    Box *old = word_.exchange (fresh, order ());
    retire (old);
  }

  value_type
  exchange (value_type desired, std::memory_order = std::memory_order_seq_cst)
  {
    Box *fresh = make_box (desired);
    Box *old = word_.exchange (fresh, order ());
    value_type previous = old == nullptr ? value_type () : old->value;
    retire (old);
    return previous;
  }

  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order, std::memory_order)
  {
    return compare_exchange (expected, std::move (desired));
  }

  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order = std::memory_order_seq_cst)
  {
    return compare_exchange (expected, std::move (desired));
  }

  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order, std::memory_order)
  {
    return compare_exchange (expected, std::move (desired));
  }

  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order = std::memory_order_seq_cst)
  {
    return compare_exchange (expected, std::move (desired));
  }

private:
  static std::memory_order
  order ()
  {
    return R == BoxReclaim::leak_with_relaxed_publication
               ? std::memory_order_relaxed
               : std::memory_order_seq_cst;
  }

  static Box *
  make_box (value_type const &value)
  {
    if (is_plain_empty (value))
      return nullptr;
    void *memory = BoxPool<R>::get ().allocate (sizeof (Box));
    return new (memory) Box (value);
  }

  static void
  destroy_box (Box *box)
  {
    if (box == nullptr)
      return;
    box->~Box ();
    BoxPool<R>::get ().release (box, sizeof (Box));
  }

  void
  retire (Box *old)
  {
    if (old == nullptr)
      return;
    if (R == BoxReclaim::delete_at_once)
      {
        destroy_box (old);
        return;
      }
    std::lock_guard<std::mutex> guard (retired_mutex_);
    retired_.push_back (old);
  }

  bool
  compare_exchange (value_type &expected, value_type desired)
  {
    for (;;)
      {
        Box *box = word_.load (order ());
        value_type current = box == nullptr ? value_type () : box->value;
        if (!equivalent_values (current, expected))
          {
            expected = std::move (current);
            return false;
          }
        // The gap between reading the box and swapping the word is where an
        // unprotected engine falls to ABA.
        EngineHooks::run_cas_gap ();
        Box *fresh = make_box (desired);
        if (word_.compare_exchange_strong (box, fresh, order ()))
          {
            retire (box);
            return true;
          }
        destroy_box (fresh);
      }
  }

  std::atomic<Box *> word_;
  std::mutex retired_mutex_;
  std::vector<Box *> retired_;
};

// --- engine descriptors
// --------------------------------------------------------

/**
 * @brief Describes an engine to the checkers: its shared pointer template and
 * what it promises about destruction.
 * @details `shared<T>` is the atomic shared pointer,
 * `replaced_value_dies_in_call` tells whether the value that a store, exchange
 * or successful compare-exchange replaces is destroyed before the call returns
 * (the lock-based and std-backed engines) or later (deferred reclamation), and
 * `quiesce ()` runs whatever makes a deferred engine release what it holds
 * (nothing for the current engines).
 */
template <Flaw F> struct FlawedEngine
{
  template <typename T> using shared = flawed_atomic_shared_ptr<T, F>;
  static bool
  replaced_value_dies_in_call ()
  {
    return true;
  }
  static void
  quiesce ()
  {
  }
  static char const *
  name ()
  {
    return flaw_name (F);
  }
};

/// The correct reference engine.
typedef FlawedEngine<Flaw::none> ReferenceEngine;

template <BoxReclaim R> struct BoxEngine
{
  template <typename T> using shared = box_atomic_shared_ptr<T, R>;
  static bool
  replaced_value_dies_in_call ()
  {
    return R == BoxReclaim::delete_at_once;
  }
  static void
  quiesce ()
  {
  }
  static char const *
  name ()
  {
    return R == BoxReclaim::delete_at_once
               ? "naive_box"
               : (R == BoxReclaim::leak_until_destruction ? "leaky_box"
                                                          : "relaxed_box");
  }
};

/// Correct lock-free-shaped engine with deferred destruction.
typedef BoxEngine<BoxReclaim::leak_until_destruction> LeakyBoxEngine;

/// The same with relaxed publication: a data race only ThreadSanitizer sees.
typedef BoxEngine<BoxReclaim::leak_with_relaxed_publication> RelaxedBoxEngine;

/// Lock-free-shaped engine that frees a box at once: the ABA victim.
typedef BoxEngine<BoxReclaim::delete_at_once> NaiveBoxEngine;
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_ENGINES_HPP

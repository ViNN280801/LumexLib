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

// What the split-count test files share: the names under test, a counting
// object, and the policies that make the real engine observable. The
// library has none of this: it is a template on a policy, and the policies
// below hook it from the tests.
//
// - `ledger_policy_t<Tag, Limit, Reserve>`: counts the six events of the
//   tick accounting in per-tag static counters and records the peak of `L`.
//   At quiescence `ticks == unticks + settles + consumed` and
//   `transfers - untransfers == settles` (`check_balanced`).
// - `gate_policy_t<Tag>`: a ledger policy whose `at (point)` blocks a thread
//   that armed its thread-local gate (`arm_gate`) at the n-th occurrence of a
//   point (by default the first `load_count`) until the gate opens, so a
//   test can hold a load after its tick and before its count, or a writer
//   after its deposit and before its swap. The hold is one-shot.
// - `torn_policy_t<Tag>`: a ledger policy whose `guess` returns, every k-th
//   call, the low half of the current word with the high half of the value
//   the thread guessed before, or (alternately, as the hardware does when
//   `speculative_load` reads `lo` first) the old low half with the current
//   high half; every use of a guess must survive that.
//
// Every test file wraps its tests in `#if
// LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` and has a placeholder test
// otherwise, so the directory also builds on a target without the engine.
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/smart_ptr/LumexSmartPtr"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestConfig.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace split_test
{
namespace sp = lumex::core::smart_ptr;
namespace asp = lumex::core::atomic::smart_ptr;
using asp::Detail::split_event_t;
using asp::Detail::split_native_policy_t;
using asp::Detail::split_point_t;
using asp::Detail::split_value_t;
using lumex_atomic_test::Watchdog;

/// The engine over `sp::shared_ptr<T>` with the policy @p Policy.
template <typename T, typename Policy>
using shared_cell = asp::Detail::basic_atomic_shared_ptr<
    T, asp::Detail::split_count_cell<sp::shared_ptr<T>, Policy>>;

/// The engine over `sp::weak_ptr<T>` with the policy @p Policy.
template <typename T, typename Policy>
using weak_cell = asp::Detail::basic_atomic_weak_ptr<
    T, asp::Detail::split_count_cell<sp::weak_ptr<T>, Policy>>;

/// Live objects of `Obj`: the destructor count of every test.
inline std::atomic<long> &
alive_objects ()
{
  static std::atomic<long> count (0);
  return count;
}

/// A counting object: `alive_objects ()` is its number of live instances.
struct Obj
{
  explicit Obj (int x) : v (x) { alive_objects ().fetch_add (1); }
  Obj (Obj const &) = delete;
  Obj &operator= (Obj const &) = delete;
  ~Obj () { alive_objects ().fetch_sub (1); }

  int v;
  int pad[3];
};

/// A pointer 2^41 bytes past @p base: an alias whose offset does not fit in
/// 40 bits, so the engine stores it through a holder. Never dereferenced.
inline int *
far_of (void const *base)
{
  return reinterpret_cast<int *> (reinterpret_cast<std::uintptr_t> (base)
                                  + (std::uintptr_t (1) << 41));
}

/// The counters of one tag of `ledger_policy_t`.
struct ledger_counters_t
{
  std::atomic<unsigned long> ticks;
  std::atomic<unsigned long> unticks;
  std::atomic<unsigned long> consumed;
  std::atomic<unsigned long> transfers;
  std::atomic<unsigned long> untransfers;
  std::atomic<unsigned long> settles;
  std::atomic<unsigned long> peak;

  void
  reset ()
  {
    ticks = 0;
    unticks = 0;
    consumed = 0;
    transfers = 0;
    untransfers = 0;
    settles = 0;
    peak = 0;
  }
};

/**
 * @brief A policy that counts the events of the engine (see the file text).
 * @tparam Tag Names the counters; one tag per test.
 * @tparam Limit The tick limit of the policy.
 * @tparam Reserve The reserve of the policy.
 */
template <typename Tag, std::uint32_t Limit = 0xFFFFFFu,
          std::uint32_t Reserve = 8u>
struct ledger_policy_t : split_native_policy_t
{
  static ledger_counters_t &
  counters ()
  {
    static ledger_counters_t instance;
    return instance;
  }

  static void
  note (split_event_t event, std::uint32_t n) noexcept
  {
    ledger_counters_t &c = counters ();
    switch (event)
      {
      case split_event_t::tick:
        c.ticks.fetch_add (n);
        break;
      case split_event_t::untick:
        c.unticks.fetch_add (n);
        break;
      case split_event_t::consumed:
        c.consumed.fetch_add (n);
        break;
      case split_event_t::transfer:
        c.transfers.fetch_add (n);
        break;
      case split_event_t::untransfer:
        c.untransfers.fetch_add (n);
        break;
      case split_event_t::settle_ext:
        c.settles.fetch_add (n);
        break;
      }
  }

  static void
  ticked (std::uint32_t ticks) noexcept
  {
    std::atomic<unsigned long> &peak = counters ().peak;
    unsigned long seen = peak.load ();
    while (seen < ticks && !peak.compare_exchange_weak (seen, ticks))
      {
      }
  }

  static std::uint32_t
  tick_limit () noexcept
  {
    return Limit;
  }

  static std::uint32_t
  reserve () noexcept
  {
    return Reserve;
  }
};

/// Resets the counters of @p Policy.
template <typename Policy>
inline void
reset_ledger ()
{
  Policy::counters ().reset ();
}

/// Readable form of the counters of @p Policy, for failure messages.
template <typename Policy>
inline std::string
describe_ledger ()
{
  ledger_counters_t &c = Policy::counters ();
  char buffer[192];
  std::snprintf (buffer, sizeof buffer,
                 "ticks %lu unticks %lu consumed %lu settles %lu transfers "
                 "%lu untransfers %lu peak %lu",
                 c.ticks.load (), c.unticks.load (), c.consumed.load (),
                 c.settles.load (), c.transfers.load (), c.untransfers.load (),
                 c.peak.load ());
  return buffer;
}

/// Asserts the two ledger identities of the protocol at quiescence.
template <typename Policy>
inline void
check_balanced (char const *what)
{
  ledger_counters_t &c = Policy::counters ();
  EXPECT_EQ (c.ticks.load (),
             c.unticks.load () + c.settles.load () + c.consumed.load ())
      << what << ": every tick ends once; " << describe_ledger<Policy> ();
  EXPECT_EQ (c.transfers.load () - c.untransfers.load (), c.settles.load ())
      << what << ": every deposit is paid or returned; "
      << describe_ledger<Policy> ();
}

/// The gate of the calling thread (null: the thread is not gated).
inline std::atomic<bool> *&
this_thread_gate ()
{
  static thread_local std::atomic<bool> *gate = nullptr;
  return gate;
}

/// Where the gate of the calling thread holds: the point and how many of its
/// occurrences are let through first.
struct hold_t
{
  split_point_t point;
  int skip;
};

inline hold_t &
this_thread_hold ()
{
  static thread_local hold_t hold = { split_point_t::load_count, 0 };
  return hold;
}

/// Arms the gate of the calling thread: the thread blocks at the
/// (@p skip + 1)-th `at (@p point)` until @p gate is true. One-shot.
inline void
arm_gate (std::atomic<bool> &gate,
          split_point_t point = split_point_t::load_count, int skip = 0)
{
  this_thread_hold ().point = point;
  this_thread_hold ().skip = skip;
  this_thread_gate () = &gate;
}

/// Disarms the gate of the calling thread.
inline void
disarm_gate ()
{
  this_thread_gate () = nullptr;
}

/**
 * @brief A ledger policy whose `at (point)` blocks the calling thread while
 * its gate is closed (see the file text). A load is then held after its tick
 * and before its count (the default hold); a writer can be held after its
 * pin and deposit and before its swap (`swap`, skip 1).
 */
template <typename Tag, std::uint32_t Limit = 0xFFFFFFu,
          std::uint32_t Reserve = 8u>
struct gate_policy_t : ledger_policy_t<Tag, Limit, Reserve>
{
  /// The number of threads that have reached their hold.
  static std::atomic<int> &
  arrived ()
  {
    static std::atomic<int> count (0);
    return count;
  }

  static void
  at (split_point_t point) noexcept
  {
    std::atomic<bool> *const gate = this_thread_gate ();
    if (gate == nullptr || point != this_thread_hold ().point)
      return;
    if (this_thread_hold ().skip > 0)
      {
        --this_thread_hold ().skip;
        return;
      }
    this_thread_gate () = nullptr;
    arrived ().fetch_add (1);
    while (!gate->load ())
      std::this_thread::yield ();
  }
};

/**
 * @brief A ledger policy whose `guess` is torn every k-th call (see the file
 * text). `set_period (k)` sets k (0: never); the counters of the tag are
 * shared by all threads, the period too.
 */
template <typename Tag, std::uint32_t Limit = 0xFFFFFFu,
          std::uint32_t Reserve = 8u>
struct torn_policy_t : ledger_policy_t<Tag, Limit, Reserve>
{
  static std::atomic<std::uint32_t> &
  period ()
  {
    static std::atomic<std::uint32_t> value (0);
    return value;
  }

  /// Sets k: every k-th guess of a thread is torn; 0 turns tearing off.
  static void
  set_period (std::uint32_t k)
  {
    period ().store (k);
  }

  static split_value_t
  guess (::lumex::core::atomic::dwcas::dwcas_word const &word) noexcept
  {
    static thread_local split_value_t previous = { 0u, 0u };
    static thread_local std::uint32_t calls = 0u;
    split_value_t const current = word.speculative_load ();
    std::uint32_t const k = period ().load (std::memory_order_relaxed);
    ++calls;
    split_value_t result = current;
    if (k != 0u && calls % k == 0u)
      {
        // Two directions, alternating: the old high half under the current
        // low half, and the hardware tear (`speculative_load` reads `lo`
        // first): the old low half under the new high half.
        if (((calls / k) & 1u) != 0u)
          {
            result.lo = current.lo;
            result.hi = previous.hi;
          }
        else
          {
            result.lo = previous.lo;
            result.hi = current.hi;
          }
      }
    previous = current;
    return result;
  }
};

/// Whether a policy tears its guesses (then a compare-exchange may build and
/// retire an image that a clean run would not, and the weak form may fail
/// although the values are equivalent).
template <typename Policy> struct tears_t
{
  static bool const value = false;
};

template <typename Tag, std::uint32_t Limit, std::uint32_t Reserve>
struct tears_t<torn_policy_t<Tag, Limit, Reserve>>
{
  static bool const value = true;
};

// -- What the scenario tests share --

/// The counter words and the combined form of the engine's counters.
typedef sp::detail::split_counter counter_t;

/// The control block of a smart pointer (null for an empty owner).
template <typename Pointer>
inline sp::detail::ctl_base *
block_of (Pointer const &pointer)
{
  return sp::detail::access::control (pointer);
}

/// The four counter fields of a block, read as plain numbers.
struct words_t
{
  long count;
  long ext;
  long weak_count;
  long weak_ext;
};

/// The counter fields of the block of @p pointer (all zero for no block).
template <typename Pointer>
inline words_t
words_of (Pointer const &pointer)
{
  words_t result = { 0, 0, 0, 0 };
  sp::detail::ctl_base *const block = block_of (pointer);
  if (block == nullptr)
    return result;
  counter_t::word_type const strong = block->strong_counter ().load ();
  counter_t::word_type const weak = block->weak_counter ().load ();
  result.count = counter_t::count_of (strong);
  result.ext = counter_t::ext_of (strong);
  result.weak_count = counter_t::count_of (weak);
  result.weak_ext = counter_t::ext_of (weak);
  return result;
}

/**
 * @brief Asserts that the block of @p pointer is quiescent and has exactly
 * @p owners strong owners (invariant I4 and exactness at quiescence): `count`
 * equals the owners, `ext` is zero on both ledgers and `use_count ()` agrees.
 */
template <typename Pointer>
inline void
expect_exact (Pointer const &pointer, long owners, char const *what)
{
  words_t const w = words_of (pointer);
  EXPECT_EQ (w.count, owners) << what << ": strong count";
  EXPECT_EQ (w.ext, 0) << what << ": strong ext at quiescence";
  EXPECT_EQ (w.weak_ext, 0) << what << ": weak ext at quiescence";
  EXPECT_EQ (static_cast<long> (pointer.use_count ()), owners)
      << what << ": use_count";
}

/// The same stored pointer and the same owner (or both without an owner).
template <typename A, typename B>
inline bool
same_value (A const &a, B const &b)
{
  return a.get () == b.get () && !a.owner_before (b) && !b.owner_before (a);
}

/// Equivalence of two weak pointers by the control block and the stored
/// pointer (`access::stored`).
template <typename T>
inline bool
same_weak (sp::weak_ptr<T> const &a, sp::weak_ptr<T> const &b)
{
  return sp::detail::access::stored (a) == sp::detail::access::stored (b)
         && !a.owner_before (b) && !b.owner_before (a);
}

/// The offset of the stored pointer of @p pointer from the anchor of its
/// block, as the engine would pack it; 0 for a pointer without an owner.
template <typename Pointer>
inline std::int64_t
offset_of_value (Pointer const &pointer)
{
  sp::detail::ctl_base *const block = block_of (pointer);
  if (block == nullptr)
    return 0;
  return static_cast<std::int64_t> (
      reinterpret_cast<std::uintptr_t> (pointer.get ()) - block->anchor ());
}

/// Whether the engine must store @p pointer through a holder: its offset does
/// not fit in 40 bits (plan section 1.1: `-2^39 <= d < 2^39` fits).
template <typename Pointer>
inline bool
needs_holder (Pointer const &pointer)
{
  if (block_of (pointer) == nullptr)
    return false;
  std::int64_t const d = offset_of_value (pointer);
  std::int64_t const half = std::int64_t (1) << 39;
  return d < -half || d >= half;
}

/// A pointer @p delta bytes from @p base. Never dereferenced.
inline int *
shifted (void const *base, std::int64_t delta)
{
  return reinterpret_cast<int *> (static_cast<std::uintptr_t> (
      static_cast<std::int64_t> (reinterpret_cast<std::uintptr_t> (base))
      + delta));
}

// -- The allocation hooks (LumexSplitCountAllocHooks.cxx11.tests.cpp) --

/// Whether the test executable replaces the global `operator new` (see
/// LumexSplitCountAllocHooks.cxx11.tests.cpp). ThreadSanitizer links its own
/// allocation functions, so under it nothing is counted and nothing can be
/// made to fail; the checks that depend on the counts are skipped there.
inline bool
alloc_hooks_active ()
{
  return !LUMEX_TEST_HAS_TSAN;
}

/// The `operator new` calls made by the calling thread.
inline unsigned long &
new_calls ()
{
  static thread_local unsigned long calls = 0;
  return calls;
}

/// When true, the next `operator new` of the calling thread throws
/// `std::bad_alloc` (and clears the flag).
inline bool &
fail_next_new ()
{
  static thread_local bool fail = false;
  return fail;
}

/// Checks that the calling thread made @p want `operator new` calls since
/// @p mark (a value of `new_calls ()`); skipped where the hooks are off.
inline void
expect_allocations (unsigned long mark, unsigned long want,
                    char const *what = "")
{
  if (!alloc_hooks_active ())
    return;
  EXPECT_EQ (new_calls () - mark, want) << what;
}

/// Allocations not yet freed, all threads (live blocks of `operator new`).
inline std::atomic<long> &
live_allocations ()
{
  static std::atomic<long> live (0);
  return live;
}

/// Makes the allocations of the test framework that persist (the trace stack)
/// happen now, so a count of live allocations taken after it is stable.
inline void
warm_up_framework ()
{
  SCOPED_TRACE ("warm up one");
  SCOPED_TRACE ("warm up two");
  SCOPED_TRACE ("warm up three");
  std::string text = "warm up";
  text += std::to_string (live_allocations ().load ());
}

/// The thread counts of the stress shapes: 1, 2, 4, 8, 24 (or the list of
/// `LUMEX_TEST_THREADS`).
inline std::vector<int>
shape_thread_counts ()
{
  if (std::getenv ("LUMEX_TEST_THREADS") != nullptr)
    return lumex_test::thread_counts (false);
  std::vector<int> counts;
  counts.push_back (1);
  counts.push_back (2);
  counts.push_back (4);
  counts.push_back (8);
  counts.push_back (24);
  return counts;
}

/// Repetitions of every stress shape at every thread count.
inline int
shape_repetitions ()
{
  return 5;
}

/// Runs @p body (index) on @p count threads that start together.
template <typename Body>
inline void
run_workers (int count, Body const &body)
{
  std::vector<std::thread> threads;
  std::atomic<bool> go (false);
  std::atomic<int> ready (0);
  for (int i = 0; i < count; ++i)
    threads.push_back (std::thread (
        [&, i]
          {
            ready.fetch_add (1);
            while (!go.load ())
              std::this_thread::yield ();
            body (i);
          }));
  while (ready.load () < count)
    std::this_thread::yield ();
  go.store (true);
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
}

/// Spins until @p predicate holds or @p milliseconds pass; returns whether
/// it held.
template <typename Predicate>
inline bool
spin_until (Predicate const &predicate, int milliseconds = 20000)
{
  auto const end = std::chrono::steady_clock::now ()
                   + std::chrono::milliseconds (milliseconds);
  while (!predicate ())
    {
      if (std::chrono::steady_clock::now () > end)
        return false;
      std::this_thread::yield ();
    }
  return true;
}

/// Waits up to @p milliseconds for @p flag; true when it was set.
inline bool
flag_set_within (std::atomic<bool> const &flag, int milliseconds)
{
  return spin_until ([&flag] { return flag.load (); }, milliseconds);
}

/// A small xorshift generator: a stress thread's own random stream.
struct rng_t
{
  explicit rng_t (std::uint64_t seed) : state (seed | 1u) {}

  std::uint32_t
  next ()
  {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return static_cast<std::uint32_t> (state >> 16);
  }

  std::uint64_t state;
};
} // namespace split_test

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP

// What the split-count test files share: the names under test, a counting
// object, and the policies that make the real engine observable. The
// library has none of this: it is a template on a policy, and the policies
// below hook it from the tests.
//
// - `ledger_policy_t<Tag, Limit, Reserve>`: counts the six events of the
//   tick accounting in per-tag static counters and records the peak of `L`.
//   At quiescence `ticks == unticks + settles + consumed` and
//   `transfers - untransfers == settles` (`check_balanced`).
// - `gate_policy_t<Tag>`: a ledger policy whose `at (load_count)` blocks a
//   thread that set its thread-local gate until the gate opens, so a test can
//   hold a load after its tick and before its count.
// - `torn_policy_t<Tag>`: a ledger policy whose `guess` returns, every k-th
//   call, the low half of the current word with the high half of the value
//   the thread guessed before; every use of a guess must survive that.
//
// Every test file wraps its tests in `#if
// LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` and has a placeholder test
// otherwise, so the directory also builds on a target without the engine.
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/smart_ptr/LumexSmartPtr"
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

/**
 * @brief A ledger policy whose `at (load_count)` blocks the calling thread
 * while its gate is closed (see the file text). A load is then held after
 * its tick and before its count.
 */
template <typename Tag, std::uint32_t Limit = 0xFFFFFFu,
          std::uint32_t Reserve = 8u>
struct gate_policy_t : ledger_policy_t<Tag, Limit, Reserve>
{
  /// The number of threads that have reached the gate.
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
    if (point == split_point_t::load_count && gate != nullptr)
      {
        arrived ().fetch_add (1);
        while (!gate->load ())
          std::this_thread::yield ();
      }
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
        result.lo = current.lo;
        result.hi = previous.hi;
      }
    previous = current;
    return result;
  }
};
} // namespace split_test

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_TEST_SUPPORT_HPP

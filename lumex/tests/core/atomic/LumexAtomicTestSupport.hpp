/**
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
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexAtomicTestSupport.hpp
 * @brief Shared fixtures of the atomic smart pointer tests.
 * @details The value types and their three distinct states follow the test
 * types of the libc++ atomic smart pointer tests (llvm-project pull request
 * 194215, the author's own libc++ implementation of P0718R2): a built-in
 * integer, a floating-point type, a standard library type, a user aggregate,
 * a user class and a scoped enumeration. The counters, the counting
 * allocator and the deletion ledger make the lifetime of managed objects and
 * control blocks observable, so a test can prove that no reference is leaked
 * or dropped twice. The detectors check member availability on const objects
 * with C++11 SFINAE. The stress helpers read the thread counts, the work
 * scale and the random seed from the environment, so one binary can be run
 * with several thread counts and reproduced with a fixed seed. The engine
 * under test is named once, by the alias templates `atomic_shared_ptr` and
 * `atomic_weak_ptr` below, so the same suites run on every engine (the
 * checkers of the concurrency suites take the `EngineUnderTest` descriptor).
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_SUPPORT_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/support/LumexTestConfig.hpp"

#define LUMEX_ATOMIC_TEST_STRINGIZE_IMPL(text) #text
#define LUMEX_ATOMIC_TEST_STRINGIZE(text)                                     \
  LUMEX_ATOMIC_TEST_STRINGIZE_IMPL (text)

// --- The engine under test ------------------------------------------------
//
// The only place that names the atomic smart pointer the suites run on.
// Every test (and every checker template) reaches it through the two alias
// templates below, so a new engine plugs in without touching a test body: a
// suite variant defines these macros with the template names of the engine
// (`VARIANT lock_free DEFINITIONS
// LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_lock_free
// LUMEX_ATOMIC_TEST_WEAK_ENGINE=atomic_weak_ptr_lock_free` in the
// CMakeLists.txt of the directory, the way `lock_based` defines
// LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED). Without them the suites run on
// the common name `atomic_shared_ptr` / `atomic_weak_ptr`, whatever the
// library resolves it to. An engine that destroys a replaced value later
// than the call (a deferred reclamation) says so with
// LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION=1 and provides the call that
// makes it release what it holds in LUMEX_ATOMIC_TEST_ENGINE_QUIESCE (for
// example `lumex::core::hazard_pointer::clean_up ()`).
#if !defined(LUMEX_ATOMIC_TEST_SHARED_ENGINE)
#define LUMEX_ATOMIC_TEST_SHARED_ENGINE atomic_shared_ptr
#endif
#if !defined(LUMEX_ATOMIC_TEST_WEAK_ENGINE)
#define LUMEX_ATOMIC_TEST_WEAK_ENGINE atomic_weak_ptr
#endif
#if !defined(LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION)
#define LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION 0
#endif
#if !defined(LUMEX_ATOMIC_TEST_ENGINE_QUIESCE)
#define LUMEX_ATOMIC_TEST_ENGINE_QUIESCE() ((void)0)
#endif

namespace lumex_atomic_test
{
// LumexLib declares the templates only in their namespace. The tests use the
// short names the way a consumer would: through these alias templates, which
// every test file brings in with `using namespace lumex_atomic_test`.
template <typename T>
using atomic_shared_ptr
    = lumex::core::atomic::smart_ptr::LUMEX_ATOMIC_TEST_SHARED_ENGINE<T>;
template <typename T>
using atomic_weak_ptr
    = lumex::core::atomic::smart_ptr::LUMEX_ATOMIC_TEST_WEAK_ENGINE<T>;

/// The engine under test as the checkers see it (see
/// LumexAtomicTestEngines.hpp for the descriptors of the test-side engines,
/// which have the same shape).
struct EngineUnderTest
{
  template <typename T> using shared = atomic_shared_ptr<T>;
  template <typename T> using weak = atomic_weak_ptr<T>;

  /// True when a value replaced by store, exchange or compare-exchange is
  /// destroyed before the call returns.
  static bool
  replaced_value_dies_in_call ()
  {
    return LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION == 0;
  }

  /// Makes a deferring engine release what it holds back.
  static void
  quiesce ()
  {
    LUMEX_ATOMIC_TEST_ENGINE_QUIESCE ();
  }

  static char const *
  name ()
  {
    return LUMEX_ATOMIC_TEST_STRINGIZE (LUMEX_ATOMIC_TEST_SHARED_ENGINE);
  }
};

// --- Value types -----------------------------------------------------------

struct TrackedPod
{
  std::uint32_t gen;
  std::int64_t salt;
};

inline bool
operator== (TrackedPod const &lhs, TrackedPod const &rhs)
{
  return lhs.gen == rhs.gen && lhs.salt == rhs.salt;
}

class Handle
{
public:
  explicit Handle (double c = 0.0) : coeff_ (c) {}
  double
  coeff () const
  {
    return coeff_;
  }
  friend bool
  operator== (Handle const &lhs, Handle const &rhs)
  {
    return lhs.coeff_ == rhs.coeff_;
  }

private:
  double coeff_;
};

enum class Flag : std::uint16_t
{
  Off = 0,
  Stale = 4099,
  On = 60000
};

/// Three distinct shared states per value type.
template <typename T> struct SpValues;

template <> struct SpValues<int>
{
  static int
  a ()
  {
    return -90210;
  }
  static int
  b ()
  {
    return 404;
  }
  static int
  c ()
  {
    return 7331;
  }
};

template <> struct SpValues<double>
{
  static double
  a ()
  {
    return 1.4142135623730951;
  }
  static double
  b ()
  {
    return 2.7182818284590452;
  }
  static double
  c ()
  {
    return 3.1415926535897932;
  }
};

template <> struct SpValues<std::string>
{
  static std::string
  a ()
  {
    return "kappa";
  }
  static std::string
  b ()
  {
    return "lambda";
  }
  static std::string
  c ()
  {
    return "mu";
  }
};

template <> struct SpValues<TrackedPod>
{
  static TrackedPod
  a ()
  {
    TrackedPod p = { 3u, -77 };
    return p;
  }
  static TrackedPod
  b ()
  {
    TrackedPod p = { 101u, 1 << 20 };
    return p;
  }
  static TrackedPod
  c ()
  {
    TrackedPod p = { 255u, -1 };
    return p;
  }
};

template <> struct SpValues<Handle>
{
  static Handle
  a ()
  {
    return Handle (0.125);
  }
  static Handle
  b ()
  {
    return Handle (-4096.5);
  }
  static Handle
  c ()
  {
    return Handle (8192.25);
  }
};

template <> struct SpValues<Flag>
{
  static Flag
  a ()
  {
    return Flag::Off;
  }
  static Flag
  b ()
  {
    return Flag::Stale;
  }
  static Flag
  c ()
  {
    return Flag::On;
  }
};

typedef ::testing::Types<int, double, std::string, TrackedPod, Handle, Flag>
    ValueTypes;

/// Shared pointers to the three states of T.
template <typename T>
std::shared_ptr<T>
state_a ()
{
  return std::make_shared<T> (SpValues<T>::a ());
}

template <typename T>
std::shared_ptr<T>
state_b ()
{
  return std::make_shared<T> (SpValues<T>::b ());
}

template <typename T>
std::shared_ptr<T>
state_c ()
{
  return std::make_shared<T> (SpValues<T>::c ());
}

/// Every valid order, and the subsets the standard allows per operation.
inline std::vector<std::memory_order>
all_orders ()
{
  std::memory_order const orders[]
      = { std::memory_order_relaxed, std::memory_order_consume,
          std::memory_order_acquire, std::memory_order_release,
          std::memory_order_acq_rel, std::memory_order_seq_cst };
  return std::vector<std::memory_order> (orders, orders + 6);
}

/// Orders allowed for load, wait and the failure side of a comparison.
inline std::vector<std::memory_order>
load_orders ()
{
  std::memory_order const orders[]
      = { std::memory_order_relaxed, std::memory_order_consume,
          std::memory_order_acquire, std::memory_order_seq_cst };
  return std::vector<std::memory_order> (orders, orders + 4);
}

/// Orders allowed for store.
inline std::vector<std::memory_order>
store_orders ()
{
  std::memory_order const orders[]
      = { std::memory_order_relaxed, std::memory_order_release,
          std::memory_order_seq_cst };
  return std::vector<std::memory_order> (orders, orders + 3);
}

// --- Ownership helpers -----------------------------------------------------

/// True when both store the same pointer and share ownership (or are empty).
template <typename T, typename U>
bool
same_owner_and_pointer (std::shared_ptr<T> const &lhs,
                        std::shared_ptr<U> const &rhs)
{
  return static_cast<void const *> (lhs.get ())
             == static_cast<void const *> (rhs.get ())
         && !lhs.owner_before (rhs) && !rhs.owner_before (lhs);
}

/// True when @p weak shares ownership with @p owner and locks to its pointer.
template <typename T>
bool
refers_to (std::weak_ptr<T> const &weak, std::shared_ptr<T> const &owner)
{
  std::shared_ptr<T> const locked = weak.lock ();
  return locked.get () == owner.get () && !weak.owner_before (owner)
         && !owner.owner_before (weak);
}

/// True when two weak pointers share ownership (or are both empty).
template <typename T>
bool
same_owner (std::weak_ptr<T> const &lhs, std::weak_ptr<T> const &rhs)
{
  return !lhs.owner_before (rhs) && !rhs.owner_before (lhs);
}

// --- Lifetime accounting ---------------------------------------------------

/// Counts live objects; each test reads the count before and after.
struct Tracker
{
  static std::atomic<int> &
  alive ()
  {
    static std::atomic<int> count (0);
    return count;
  }

  explicit Tracker (int v = 0) : value (v), check (~v)
  {
    alive ().fetch_add (1, std::memory_order_relaxed);
  }
  ~Tracker ()
  {
    check = 0;
    alive ().fetch_sub (1, std::memory_order_relaxed);
  }
  Tracker (Tracker const &) = delete;
  Tracker &operator= (Tracker const &) = delete;

  /// True while the object is alive and not overwritten.
  bool
  intact () const
  {
    return check == ~value;
  }

  int value;
  int check;
};

/// Counts live allocations of every CountingAllocator.
inline std::atomic<long> &
live_allocations ()
{
  static std::atomic<long> count (0);
  return count;
}

/**
 * @brief Allocator that counts its live allocations.
 * @details `std::allocate_shared` puts the object and its control block in
 * one allocation, which is freed only when the last shared and the last weak
 * reference are gone. A count back at its start value proves that every weak
 * reference was released.
 */
template <typename T> struct CountingAllocator
{
  typedef T value_type;

  CountingAllocator () {}
  template <typename U> CountingAllocator (CountingAllocator<U> const &) {}

  T *
  allocate (std::size_t n)
  {
    live_allocations ().fetch_add (1, std::memory_order_relaxed);
    return static_cast<T *> (::operator new (n * sizeof (T)));
  }

  void
  deallocate (T *p, std::size_t)
  {
    live_allocations ().fetch_sub (1, std::memory_order_relaxed);
    ::operator delete (static_cast<void *> (p));
  }
};

template <typename T, typename U>
bool
operator== (CountingAllocator<T> const &, CountingAllocator<U> const &)
{
  return true;
}

template <typename T, typename U>
bool
operator!= (CountingAllocator<T> const &, CountingAllocator<U> const &)
{
  return false;
}

/// A shared Tracker whose control block is counted.
inline std::shared_ptr<Tracker>
make_counted_tracker (int value)
{
  return std::allocate_shared<Tracker> (CountingAllocator<Tracker> (), value);
}

/**
 * @brief Records every deletion of the objects it hands out, without
 * freeing them.
 * @details The objects are slots of an array that outlives the test, and
 * the deleter only counts. A slot deleted twice therefore shows up as a count
 * of 2 instead of undefined behaviour, and a slot never deleted shows up as
 * a count of 0.
 */
class DeletionLedger
{
public:
  explicit DeletionLedger (std::size_t size)
      : slots_ (new std::atomic<int>[size]), payload_ (new int[size]),
        size_ (size), next_ (0)
  {
    for (std::size_t i = 0; i < size; ++i)
      {
        slots_[i].store (0);
        payload_[i] = static_cast<int> (i);
      }
  }

  DeletionLedger (DeletionLedger const &) = delete;
  DeletionLedger &operator= (DeletionLedger const &) = delete;

  /// Deleter that counts the deletion of one slot.
  struct Deleter
  {
    DeletionLedger *ledger;
    std::size_t slot;

    void
    operator() (int *) const
    {
      ledger->slots_[slot].fetch_add (1, std::memory_order_relaxed);
    }
  };

  /// A new owner of the next free slot.
  std::shared_ptr<int>
  make ()
  {
    std::size_t const slot = next_.fetch_add (1);
    if (slot >= size_)
      std::abort ();
    Deleter const deleter = { this, slot };
    return std::shared_ptr<int> (&payload_[slot], deleter);
  }

  /// A unique owner of the next free slot, with the counting deleter.
  std::unique_ptr<int, Deleter>
  make_unique_owner ()
  {
    std::size_t const slot = next_.fetch_add (1);
    if (slot >= size_)
      std::abort ();
    Deleter const deleter = { this, slot };
    return std::unique_ptr<int, Deleter> (&payload_[slot], deleter);
  }

  /// Slots handed out so far.
  std::size_t
  used () const
  {
    return next_.load ();
  }

  /// Number of deletions of @p slot.
  int
  deletions (std::size_t slot) const
  {
    return slots_[slot].load ();
  }

  /// Number of slots deleted more than once.
  std::size_t
  deleted_twice () const
  {
    std::size_t count = 0;
    for (std::size_t i = 0; i < used (); ++i)
      if (slots_[i].load () > 1)
        ++count;
    return count;
  }

  /// Number of slots that have not been deleted.
  std::size_t
  not_deleted () const
  {
    std::size_t count = 0;
    for (std::size_t i = 0; i < used (); ++i)
      if (slots_[i].load () == 0)
        ++count;
    return count;
  }

  /// The payload of @p slot (its index), to check what a pointer refers to.
  int
  payload (std::size_t slot) const
  {
    return payload_[slot];
  }

private:
  std::unique_ptr<std::atomic<int>[]> slots_;
  std::unique_ptr<int[]> payload_;
  std::size_t size_;
  std::atomic<std::size_t> next_;
};

// --- Member detection on const objects (C++11 SFINAE) --------------------

template <typename...> struct make_void
{
  typedef void type;
};

#define LUMEX_ATOMIC_TEST_DETECTOR(name, expression)                          \
  template <typename A, typename = void> struct name : std::false_type        \
  {                                                                           \
  };                                                                          \
  template <typename A>                                                       \
  struct name<A, typename make_void<decltype (expression)>::type>             \
      : std::true_type                                                        \
  {                                                                           \
  }

LUMEX_ATOMIC_TEST_DETECTOR (
    can_store,
    std::declval<A &> ().store (std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (can_load, std::declval<A &> ().load ());
LUMEX_ATOMIC_TEST_DETECTOR (
    can_exchange,
    std::declval<A &> ().exchange (std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (can_compare_exchange_strong,
                            std::declval<A &> ().compare_exchange_strong (
                                std::declval<typename A::value_type &> (),
                                std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (can_compare_exchange_weak,
                            std::declval<A &> ().compare_exchange_weak (
                                std::declval<typename A::value_type &> (),
                                std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (can_compare_exchange_with_rvalue_expected,
                            std::declval<A &> ().compare_exchange_strong (
                                std::declval<typename A::value_type> (),
                                std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (
    can_wait,
    std::declval<A &> ().wait (std::declval<typename A::value_type> ()));
LUMEX_ATOMIC_TEST_DETECTOR (can_notify_one,
                            std::declval<A &> ().notify_one ());
LUMEX_ATOMIC_TEST_DETECTOR (can_notify_all,
                            std::declval<A &> ().notify_all ());
LUMEX_ATOMIC_TEST_DETECTOR (can_is_lock_free,
                            std::declval<A &> ().is_lock_free ());
LUMEX_ATOMIC_TEST_DETECTOR (can_assign_value,
                            std::declval<A &> ()
                            = std::declval<typename A::value_type> ());

#undef LUMEX_ATOMIC_TEST_DETECTOR

// --- Threads ---------------------------------------------------------------

/// Sleeps for @p ms milliseconds.
inline void
sleep_ms (int ms)
{
  std::this_thread::sleep_for (std::chrono::milliseconds (ms));
}

/// Spins (with yields) until @p flag becomes true.
inline void
wait_for_flag (std::atomic<bool> const &flag)
{
  while (!flag.load (std::memory_order_acquire))
    std::this_thread::yield ();
}

/// Spins (with yields) until @p counter reaches @p value.
inline void
wait_for_count (std::atomic<int> const &counter, int value)
{
  while (counter.load (std::memory_order_acquire) < value)
    std::this_thread::yield ();
}

/**
 * @brief Watchdog limit of one test, in seconds.
 * @details `LUMEX_ATOMIC_WATCHDOG_SECONDS` (1 to 3600) overrides the default
 * of 120, for example to let a deliberately broken build fail fast.
 */
inline int
watchdog_seconds ()
{
  char const *env = std::getenv ("LUMEX_ATOMIC_WATCHDOG_SECONDS");
  if (env != nullptr)
    {
      long const v = std::strtol (env, nullptr, 10);
      if (v >= 1 && v <= 3600)
        return static_cast<int> (v);
    }
  return 120;
}

/**
 * @brief Aborts the test program with a message if it is not disarmed in
 * time.
 * @details A lost wake-up makes a thread sleep forever, which would hang the
 * test binary. The watchdog turns that into a failure with a clear message,
 * also when the binary runs outside CTest.
 */
class Watchdog
{
public:
  explicit Watchdog (char const *what)
      : what_ (what), seconds_ (watchdog_seconds ()), done_ (false), mutex_ (),
        condition_ (), thread_ ()
  {
    thread_ = std::thread (&Watchdog::run, this);
  }

  ~Watchdog ()
  {
    {
      std::lock_guard<std::mutex> guard (mutex_);
      done_ = true;
    }
    condition_.notify_all ();
    thread_.join ();
  }

  Watchdog (Watchdog const &) = delete;
  Watchdog &operator= (Watchdog const &) = delete;

private:
  void
  run ()
  {
    std::unique_lock<std::mutex> guard (mutex_);
    if (!condition_.wait_for (guard, std::chrono::seconds (seconds_),
                              [this] { return done_; }))
      {
        std::fprintf (stderr,
                      "\nWATCHDOG: '%s' did not finish within %d s "
                      "(a thread is probably asleep forever)\n",
                      what_, seconds_);
        std::fflush (stderr);
        std::abort ();
      }
  }

  char const *what_;
  int seconds_;
  bool done_;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::thread thread_;
};

// --- Stress configuration --------------------------------------------------

/**
 * @brief Thread counts of the stress tests.
 * @details `LUMEX_TEST_THREADS` (shared with the other concurrency tests) or
 * `LUMEX_ATOMIC_STRESS_THREADS`, a comma-separated list such as "2,4,8,16",
 * overrides the default {2, 4, 8}.
 */
inline std::vector<int>
stress_thread_counts ()
{
  // LUMEX_TEST_THREADS (shared with the other concurrency tests) wins.
  if (std::getenv ("LUMEX_TEST_THREADS") != nullptr)
    return lumex_test::thread_counts (false);
  std::vector<int> counts;
  char const *env = std::getenv ("LUMEX_ATOMIC_STRESS_THREADS");
  if (env != nullptr)
    {
      char const *p = env;
      while (*p != '\0')
        {
          char *end = nullptr;
          long const v = std::strtol (p, &end, 10);
          if (end == p)
            {
              ++p;
              continue;
            }
          if (v >= 1 && v <= 256)
            counts.push_back (static_cast<int> (v));
          p = end;
        }
    }
  if (counts.empty ())
    {
      counts.push_back (2);
      counts.push_back (4);
      counts.push_back (8);
    }
  return counts;
}

/**
 * @brief Scales an iteration count by `LUMEX_ATOMIC_STRESS_SCALE` and
 * `LUMEX_TEST_SCALE` (positive integers, default 1).
 */
inline int
stress_iterations (int base)
{
  char const *env = std::getenv ("LUMEX_ATOMIC_STRESS_SCALE");
  // LUMEX_TEST_SCALE (shared with the other concurrency tests) multiplies on
  // top.
  long scale = lumex_test::scale_factor ();
  if (env != nullptr)
    {
      long const v = std::strtol (env, nullptr, 10);
      if (v >= 1 && v <= 1000)
        scale *= v;
    }
  return static_cast<int> (base * scale);
}

/**
 * @brief Seed of the pseudo-random choices of the stress tests.
 * @details Fixed by default (20261003) so a run is repeatable;
 * `LUMEX_TEST_SEED` (shared with the other concurrency tests) or
 * `LUMEX_ATOMIC_STRESS_SEED` overrides it. Failure messages print it.
 */
inline std::uint32_t
stress_seed ()
{
  // LUMEX_TEST_SEED (shared with the other concurrency tests) wins.
  if (lumex_test::seed_is_pinned ())
    return static_cast<std::uint32_t> (lumex_test::base_seed ());
  char const *env = std::getenv ("LUMEX_ATOMIC_STRESS_SEED");
  if (env != nullptr)
    {
      unsigned long const v = std::strtoul (env, nullptr, 10);
      if (v != 0)
        return static_cast<std::uint32_t> (v);
    }
  return 20261003u;
}

/// Small deterministic pseudo-random generator (xorshift32).
class Random
{
public:
  explicit Random (std::uint32_t seed) : state_ (seed * 2654435761u + 1u) {}

  std::uint32_t
  next ()
  {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
  }

private:
  std::uint32_t state_;
};

/// Joins every thread of @p threads.
inline void
join_all (std::vector<std::thread> &threads)
{
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
}

} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_SUPPORT_HPP

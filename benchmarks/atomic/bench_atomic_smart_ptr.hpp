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
 * @file bench_atomic_smart_ptr.hpp
 * @brief Measurement core of LumexAtomicBenchmark.
 *
 * The benchmark compares atomic shared pointers that cannot share one
 * translation unit: LumexLib's lock-based engine built at C++11 (sleeping on
 * the striped wait table) and at C++20 (sleeping in std::atomic::wait),
 * LumexLib's lock-free engine at C++11 and C++20 (the replaced value
 * destroyed immediately, and deferred), the common name at C++20, the
 * wrapper of the standard library's type, the standard library's
 * std::atomic<std::shared_ptr<T>> and, where Boost is found, its
 * boost::atomic_shared_ptr (a spinlock). Each unit instantiates the
 * templates below for its own type and hands main () an implementation_t:
 * plain data and a function pointer, so units built with different standards
 * meet only through types that mean the same in every standard. An engine
 * that a build does not have is handed over as an implementation_t with a
 * null `measure`, and main () leaves it out.
 *
 * The method is the one the author used for the libc++ implementation of
 * llvm-project pull request 194215: N threads run one operation on one
 * shared object for a fixed time, and every result is divided by
 * std::atomic<std::uint64_t>::compare_exchange_strong timed in the same
 * process, at the same thread count, right before.
 *
 * The split-count engine (the series `lumex_split_count*`, over the
 * library's own `shared_ptr`) and the lock-based engine over the same
 * pointers are measured by the same harness. Beyond the four operations it
 * has: a read-heavy run that also reports the writer's stores, a load plus
 * store pair with the cycles and instructions of the thread (perf), the
 * `lock ()` rate of the weak engines, the latency between a replacing
 * store and the destructor of the replaced object, and the distribution of
 * the time of every single `store ()` of a writer under a read load.
 */

#ifndef LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP
#define LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

/** The language mode of the unit; MSVC reports it in _MSVC_LANG. */
#if defined(_MSVC_LANG)
#define LUMEX_ATOMIC_BENCH_STANDARD _MSVC_LANG
#else
#define LUMEX_ATOMIC_BENCH_STANDARD __cplusplus
#endif

#define LUMEX_ATOMIC_BENCH_STRINGIFY_IMPL(name) #name
/** Turns a macro holding a name (an ABI namespace) into a string literal. */
#define LUMEX_ATOMIC_BENCH_STRINGIFY(name)                                    \
  LUMEX_ATOMIC_BENCH_STRINGIFY_IMPL (name)

namespace lumex_atomic_bench
{
/** The timed operations, in the order of the CSV. */
enum operation_t
{
  operation_load = 0,
  operation_store = 1,
  operation_exchange = 2,
  operation_compare_exchange_strong = 3,
  /**
   * load () on all threads but one while that one thread stores in a loop:
   * the readers-heavy pattern in which the libc++ double-width method
   * crashed. The time is per load of a reader thread. One thread: plain
   * load (); two threads: one reader and the writer.
   */
  operation_load_one_writer = 4,
  /**
   * One load () and one store () of the same value per iteration: the
   * uncontended cost of the pair (run on one thread only). The time is per
   * pair.
   */
  operation_load_store = 5,
  /** weak atomic: load () of the weak pointer, destroyed at once. */
  operation_weak_load = 6,
  /** weak atomic: load () followed by weak_ptr::lock (). */
  operation_weak_load_lock = 7,
  operation_count = 8
};

/** The kind of an implementation: which operations it supports. */
enum kind_t
{
  kind_shared = 0, ///< an atomic shared pointer
  kind_weak = 1    ///< an atomic weak pointer
};

/** One timed run of one operation. */
struct measurement_t
{
  double ns_per_op;              ///< wall time per operation and thread
  unsigned long long operations; ///< all threads together
  unsigned long long successes;  ///< successful compare_exchange_strong
  double seconds;                ///< mean timed wall time of a thread
  bool valid;                    ///< no reference left behind afterwards
  double writer_ns_per_op;       ///< load_one_writer: ns per store of the
                                 ///< writer thread (0: not measured)
  unsigned long long writer_operations; ///< stores of the writer
  double cycles_per_op;       ///< one thread: CPU cycles per operation, 0 when
                              ///< the counter is not available
  double instructions_per_op; ///< one thread: instructions per operation
};

/** The outcome of the destruction-latency scenario of one implementation. */
struct latency_result_t
{
  double median_ns;                ///< store call start to destructor, median
  double p99_ns;                   ///< the same, 99th percentile
  double max_ns;                   ///< the same, maximum
  unsigned long long objects;      ///< replaced objects
  unsigned long long in_call;      ///< destroyed before the store returned
  unsigned long long pending_end;  ///< not yet destroyed when the loop ended
  unsigned long long peak_backlog; ///< most replaced, not yet destroyed
  bool valid;                      ///< every object destroyed exactly once
};

/**
 * The outcome of the store-wait scenario of one implementation: the time of
 * every single `store ()` of one writer while the other threads load.
 */
struct store_wait_result_t
{
  double median_ns;                ///< the time of one store, median
  double p90_ns;                   ///< 90th percentile
  double p99_ns;                   ///< 99th percentile
  double p999_ns;                  ///< 99.9th percentile
  double p9999_ns;                 ///< 99.99th percentile
  double max_ns;                   ///< the longest store
  double timer_ns;                 ///< median of an empty pair of clock reads
  unsigned long long stores;       ///< timed stores
  unsigned long long over_1ms;     ///< stores that took more than 1 ms
  unsigned long long switches;     ///< involuntary context switches of the
                                   ///< writer during the timed stores
  unsigned long long over_1ms_switched; ///< stores over 1 ms after which the
                                        ///< writer's switch count had
                                        ///< advanced since the last reading
  bool pinned;                     ///< every thread was pinned as requested
  bool valid;                      ///< no reference left behind afterwards
};

/**
 * Times `operation` with `threads` threads for `seconds`; on one object
 * shared by all threads when `contended`, otherwise on a private one (one
 * thread).
 */
typedef measurement_t (*measure_fn) (int operation, int threads,
                                     double seconds, bool contended);

/**
 * Replaces an object `objects` times, `threads - 1` further threads loading
 * all the while, and times from each replacing store to the destructor of
 * the object it replaced.
 */
typedef latency_result_t (*latency_fn) (int threads, long objects);

/**
 * One writer thread times `stores` single `store ()` calls of alternating
 * values on one shared object while `threads - 1` further threads `load ()`
 * all the while. With `pin` every thread is pinned to a CPU of its own.
 */
typedef store_wait_result_t (*store_wait_fn) (int threads, long stores,
                                              bool pin);

/** An implementation as main () sees it. */
struct implementation_t
{
  char const *key;    ///< CSV key, for example "lumex_lock_based_cxx11"
  char const *label;  ///< chart label
  char const *path;   ///< what the build selected: ABI namespace or library
  long standard;      ///< __cplusplus (MSVC: _MSVC_LANG) of the unit
  bool lock_free;     ///< is_lock_free () of the measured object
  measure_fn measure; ///< null: the build has no such engine, leave it out
  latency_fn latency; ///< null: no destruction-latency scenario
  int kind;           ///< kind_t
  store_wait_fn store_wait; ///< null: no store-wait scenario
};

/** The common name atomic_shared_ptr, built at C++20. */
implementation_t lumex_default_implementation ();

/** LumexLib's lock-based engine, the common name forced, built at C++20. */
implementation_t lumex_lock_based_implementation ();

/** LumexLib's lock-based engine built at C++11 (MSVC: C++14). */
implementation_t lumex_lock_based_cxx11_implementation ();

/** LumexLib's lock-free engine, immediate destruction, built at C++11. */
implementation_t lumex_lock_free_cxx11_implementation ();

/** LumexLib's lock-free engine, immediate destruction, built at C++20. */
implementation_t lumex_lock_free_implementation ();

/** LumexLib's lock-free engine, deferred destruction, built at C++20. */
implementation_t lumex_lock_free_deferred_implementation ();

/** The wrapper of the standard library's atomic smart pointer, C++20. */
implementation_t lumex_std_backed_implementation ();

/** boost::atomic_shared_ptr (a spinlock), when Boost is found. */
implementation_t boost_implementation ();

/**
 * The split-count engine over the library's own shared_ptr, reserve 8 (the
 * production value), built at C++11.
 */
implementation_t lumex_split_count_cxx11_implementation ();

/** The lock-based engine over the library's own shared_ptr, C++11. */
implementation_t lumex_lock_based_lumex_cxx11_implementation ();

/** The split-count engine, reserve 8, built at C++20. */
implementation_t lumex_split_count_implementation ();

/** The split-count engine, reserve 4, built at C++20. */
implementation_t lumex_split_count_r4_implementation ();

/** The lock-based engine over the library's own shared_ptr, C++20. */
implementation_t lumex_lock_based_lumex_implementation ();

/** The weak split-count engine (own weak_ptr), reserve 8, C++20. */
implementation_t lumex_split_count_weak_implementation ();

/** The lock-based weak engine over the library's own weak_ptr, C++20. */
implementation_t lumex_lock_based_lumex_weak_implementation ();

/** The lock-based weak engine over std::weak_ptr, C++20. */
implementation_t lumex_lock_based_weak_implementation ();

/** The lock-free (box, immediate) weak engine over std::weak_ptr, C++20. */
implementation_t lumex_lock_free_weak_implementation ();

/** The wrapper of std::atomic<std::weak_ptr>, C++20. */
implementation_t lumex_std_backed_weak_implementation ();

/** std::atomic<std::weak_ptr<int>> of the standard library, C++20. */
implementation_t std_weak_implementation ();

/**
 * Runs a reclamation pass of the hazard domain when the build has one (a
 * no-op otherwise): what the lock-free engine replaced is destroyed by then.
 */
void settle_hazard_domain ();

/** An implementation_t for an engine the build does not have. */
inline implementation_t
unavailable_implementation ()
{
  implementation_t const none
      = { "", "", "", 0, false, nullptr, nullptr, kind_shared, nullptr };
  return none;
}

namespace detail
{
/**
 * CPU cycles and instructions retired by the calling thread in user mode,
 * from perf_event_open (Linux). Where the counters cannot be opened (no
 * permission, no PMU, another system) every reading is zero.
 */
class thread_counters_t
{
public:
  thread_counters_t () : cycles_ (-1), instructions_ (-1)
  {
#if defined(__linux__)
    cycles_ = open_counter (PERF_COUNT_HW_CPU_CYCLES);
    instructions_ = open_counter (PERF_COUNT_HW_INSTRUCTIONS);
#endif
  }

  ~thread_counters_t ()
  {
#if defined(__linux__)
    if (cycles_ >= 0)
      ::close (cycles_);
    if (instructions_ >= 0)
      ::close (instructions_);
#endif
  }

  /** Starts counting. */
  void
  start ()
  {
#if defined(__linux__)
    reset_and_enable (cycles_);
    reset_and_enable (instructions_);
#endif
  }

  /** Stops counting; the counts are available after this call. */
  void
  stop ()
  {
#if defined(__linux__)
    cycle_count_ = finish (cycles_);
    instruction_count_ = finish (instructions_);
#endif
  }

  unsigned long long
  cycles () const
  {
    return cycle_count_;
  }
  unsigned long long
  instructions () const
  {
    return instruction_count_;
  }

private:
  thread_counters_t (thread_counters_t const &);
  thread_counters_t &operator= (thread_counters_t const &);

#if defined(__linux__)
  static int
  open_counter (unsigned long long config)
  {
    perf_event_attr attributes = perf_event_attr ();
    attributes.type = PERF_TYPE_HARDWARE;
    attributes.size = sizeof (attributes);
    attributes.config = config;
    attributes.disabled = 1;
    attributes.exclude_kernel = 1;
    attributes.exclude_hv = 1;
    return static_cast<int> (
        ::syscall (SYS_perf_event_open, &attributes, 0, -1, -1, 0ul));
  }

  static void
  reset_and_enable (int descriptor)
  {
    if (descriptor < 0)
      return;
    ::ioctl (descriptor, PERF_EVENT_IOC_RESET, 0);
    ::ioctl (descriptor, PERF_EVENT_IOC_ENABLE, 0);
  }

  static unsigned long long
  finish (int descriptor)
  {
    if (descriptor < 0)
      return 0u;
    ::ioctl (descriptor, PERF_EVENT_IOC_DISABLE, 0);
    unsigned long long value = 0u;
    if (::read (descriptor, &value, sizeof (value))
        != static_cast<long> (sizeof (value)))
      return 0u;
    return value;
  }
#endif

  int cycles_;
  int instructions_;
  unsigned long long cycle_count_ = 0u;
  unsigned long long instruction_count_ = 0u;
};

/** Start and stop of one timed run; the loops only read `stop`. */
struct run_flags_t
{
  std::atomic<int> ready;
  std::atomic<bool> go;
  std::atomic<bool> stop;
  bool count_cycles; ///< read the thread counters (one thread runs only)
};

/** What one thread reports after its timed loop. */
struct thread_result_t
{
  unsigned long long operations;
  unsigned long long successes;
  std::uintptr_t checksum; ///< keeps the loaded values observable
  double seconds;
  unsigned long long cycles;
  unsigned long long instructions;
};

/** Runs `body` on one thread between the start and the stop flag. */
template <typename Body>
void
run_worker (run_flags_t *flags, Body const *body, thread_result_t *result)
{
  thread_counters_t counters;
  flags->ready.fetch_add (1);
  while (!flags->go.load (std::memory_order_acquire))
    std::this_thread::yield ();
  if (flags->count_cycles)
    counters.start ();
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  (*body) (flags->stop, *result);
  std::chrono::duration<double> const elapsed
      = std::chrono::steady_clock::now () - start;
  if (flags->count_cycles)
    {
      counters.stop ();
      result->cycles = counters.cycles ();
      result->instructions = counters.instructions ();
    }
  result->seconds = elapsed.count ();
}

/**
 * Starts `threads` threads, releases them together, lets them run for
 * `seconds` and returns the wall time per operation and thread (the figure
 * Google Benchmark reports for Threads (N)->UseRealTime ()).
 */
template <typename Body>
measurement_t
timed_run (int threads, double seconds, Body const &body)
{
  run_flags_t flags;
  flags.ready.store (0);
  flags.go.store (false);
  flags.stop.store (false);
  flags.count_cycles = threads == 1;
  std::size_t const count = static_cast<std::size_t> (threads);
  thread_result_t const zero = { 0u, 0u, 0u, 0.0, 0u, 0u };
  std::vector<thread_result_t> results (count, zero);
  std::vector<std::thread> workers;
  workers.reserve (count);
  for (std::size_t t = 0; t < count; ++t)
    workers.push_back (
        std::thread (&run_worker<Body>, &flags, &body, &results[t]));
  while (flags.ready.load () < threads)
    std::this_thread::yield ();
  flags.go.store (true, std::memory_order_release);
  std::this_thread::sleep_for (std::chrono::duration<double> (seconds));
  flags.stop.store (true);
  for (std::size_t t = 0; t < count; ++t)
    workers[t].join ();

  measurement_t measurement = { 0.0, 0u, 0u, 0.0, true, 0.0, 0u, 0.0, 0.0 };
  double seconds_sum = 0.0;
  std::uintptr_t checksum = 0u;
  for (std::size_t t = 0; t < count; ++t)
    {
      measurement.operations += results[t].operations;
      measurement.successes += results[t].successes;
      seconds_sum += results[t].seconds;
      checksum += results[t].checksum;
    }
  measurement.seconds = seconds_sum / static_cast<double> (threads);
  if (measurement.operations != 0u)
    measurement.ns_per_op = measurement.seconds * 1e9
                            * static_cast<double> (threads)
                            / static_cast<double> (measurement.operations);
  if (threads == 1 && measurement.operations != 0u)
    {
      measurement.cycles_per_op
          = static_cast<double> (results[0].cycles)
            / static_cast<double> (measurement.operations);
      measurement.instructions_per_op
          = static_cast<double> (results[0].instructions)
            / static_cast<double> (measurement.operations);
    }
  // A checksum that no value can produce never happens; the comparison only
  // keeps the loaded pointers observable to the optimizer.
  if (checksum == 1u)
    measurement.valid = false;
  return measurement;
}

/**
 * How to make a shared pointer to an int of the family `Pointer` belongs to.
 * The default is for a family whose pointer is built from a raw pointer; the
 * std::shared_ptr and the library's own shared_ptr specialize it to use the
 * allocation of their make function.
 */
template <typename Pointer> struct pointer_maker_t
{
  static Pointer
  make (int value)
  {
    return Pointer (new int (value));
  }
};

template <> struct pointer_maker_t<std::shared_ptr<int>>
{
  static std::shared_ptr<int>
  make (int value)
  {
    return std::make_shared<int> (value);
  }
};

/**
 * The value type of an atomic and how to make a value. The default suits the
 * atomics over std::shared_ptr and over the library's own shared_ptr; an
 * adapter over another shared pointer specializes it.
 */
template <typename Atomic> struct atomic_traits_t
{
  typedef typename Atomic::value_type value_type;

  static value_type
  make (int value)
  {
    return pointer_maker_t<value_type>::make (value);
  }

  /** A pointer whose deleter is `deleter` (called with the raw pointer). */
  template <typename Deleter>
  static value_type
  make_with_deleter (int value, Deleter deleter)
  {
    return value_type (new int (value), deleter);
  }

  /**
   * Called after the atomic is gone, before the reference counts are checked:
   * an engine that destroys what it replaced later (the deferred lock-free
   * engine, or a box a reader held) runs its reclamation pass here.
   */
  static void
  settle ()
  {
    settle_hazard_domain ();
  }
};

/** Storage for one object on a cache line of its own. */
template <typename T> class padded_storage_t
{
public:
  padded_storage_t () : raw_ (new unsigned char[sizeof (T) + 2 * line]) {}

  /**
   * The first line boundary in the block: the lines the object covers hold
   * nothing else, because the block reaches at least to the end of the last
   * of them.
   */
  void *
  address () const
  {
    std::uintptr_t const base = reinterpret_cast<std::uintptr_t> (raw_.get ());
    return reinterpret_cast<void *> ((base + line - 1u) & ~(line - 1u));
  }

private:
  static std::uintptr_t const line = 64u;
  std::unique_ptr<unsigned char[]> raw_;
};

/** load (): copies the shared value out. */
template <typename Atomic> struct load_body_t
{
  Atomic *atom;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    std::uintptr_t checksum = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.get ());
        ++operations;
      }
    out.operations = operations;
    out.checksum = checksum;
  }
};

/** store (): always the same value, as in the libc++ benchmark. */
template <typename Atomic> struct store_body_t
{
  Atomic *atom;
  typename Atomic::value_type const *keep;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        atom->store (*keep);
        ++operations;
      }
    out.operations = operations;
  }
};

/** exchange (): the same value in, the previous value out. */
template <typename Atomic> struct exchange_body_t
{
  Atomic *atom;
  typename Atomic::value_type const *keep;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    std::uintptr_t checksum = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const previous = atom->exchange (*keep);
        checksum += reinterpret_cast<std::uintptr_t> (previous.get ());
        ++operations;
      }
    out.operations = operations;
    out.checksum = checksum;
  }
};

/**
 * load () then compare_exchange_strong () towards the other of two values:
 * the client pattern that exposed the livelock of the libc++ lock-free
 * path (a load changes the word that the compare-exchange replaces).
 */
template <typename Atomic> struct compare_exchange_body_t
{
  Atomic *atom;
  typename Atomic::value_type const *keep_a;
  typename Atomic::value_type const *keep_b;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    typedef typename Atomic::value_type pointer_t;
    unsigned long long operations = 0u;
    unsigned long long successes = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        pointer_t expected = atom->load (std::memory_order_relaxed);
        pointer_t desired = expected == *keep_a ? *keep_b : *keep_a;
        if (atom->compare_exchange_strong (expected, desired))
          ++successes;
        ++operations;
      }
    out.operations = operations;
    out.successes = successes;
  }
};

/**
 * The readers-heavy pattern: the thread with index 0 stores in a loop and
 * counts nothing, the others load and count. `stop` ends all of them.
 */
template <typename Atomic> struct readers_and_writer_body_t
{
  Atomic *atom;
  typename atomic_traits_t<Atomic>::value_type const *keep_a;
  typename atomic_traits_t<Atomic>::value_type const *keep_b;
  std::atomic<int> *next_index;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    int const index = next_index->fetch_add (1);
    unsigned long long operations = 0u;
    std::uintptr_t checksum = 0u;
    if (index == 0)
      {
        // The writer: alternating values, so that every store replaces a box.
        // Its stores go into `successes`, the readers' loads into
        // `operations`.
        bool flip = false;
        unsigned long long stores = 0u;
        while (!stop.load (std::memory_order_relaxed))
          {
            atom->store (flip ? *keep_a : *keep_b);
            flip = !flip;
            ++stores;
          }
        out.operations = 0u;
        out.successes = stores;
        return;
      }
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.get ());
        ++operations;
      }
    out.operations = operations;
    out.checksum = checksum;
  }
};

/** load () and store () of the same value: the uncontended pair. */
template <typename Atomic> struct load_store_body_t
{
  Atomic *atom;
  typename Atomic::value_type const *keep;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    std::uintptr_t checksum = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.get ());
        atom->store (*keep);
        ++operations;
      }
    out.operations = operations;
    out.checksum = checksum;
  }
};

/** weak atomic: load () and let the weak pointer die. */
template <typename Atomic> struct weak_load_body_t
{
  Atomic *atom;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        (void)snapshot;
        ++operations;
      }
    out.operations = operations;
  }
};

/** weak atomic: load () and weak_ptr::lock (); the strong pointer dies. */
template <typename Atomic> struct weak_lock_body_t
{
  Atomic *atom;

  void
  operator() (std::atomic<bool> const &stop, thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    std::uintptr_t checksum = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.lock ().get ());
        ++operations;
      }
    out.operations = operations;
    out.checksum = checksum;
  }
};

/** Placement-constructs an Atomic on its own cache line; destroys it. */
template <typename Atomic> class atomic_holder_t
{
public:
  explicit atomic_holder_t (typename Atomic::value_type const &value)
      : storage_ (), atom_ (new (storage_.address ()) Atomic (value))
  {
  }

  ~atomic_holder_t () { atom_->~Atomic (); }

  Atomic *
  get () const
  {
    return atom_;
  }

private:
  atomic_holder_t (atomic_holder_t const &);
  atomic_holder_t &operator= (atomic_holder_t const &);

  padded_storage_t<Atomic> storage_;
  Atomic *atom_;
};

/**
 * Times one operation of `Atomic` (an atomic over std::shared_ptr<int>)
 * and checks afterwards that the two values the threads passed around hold
 * no reference except their own.
 */
template <typename Atomic>
measurement_t
measure (int operation, int threads, double seconds, bool contended)
{
  typedef atomic_traits_t<Atomic> traits_t;
  typedef typename traits_t::value_type pointer_t;
  if (!contended)
    threads = 1;
  pointer_t const keep_a (traits_t::make (1));
  pointer_t const keep_b (traits_t::make (2));
  measurement_t measurement = { 0.0, 0u, 0u, 0.0, false, 0.0, 0u, 0.0, 0.0 };
  {
    atomic_holder_t<Atomic> const holder (keep_a);
    Atomic *const atom = holder.get ();
    switch (operation)
      {
      case operation_load:
        {
          load_body_t<Atomic> const body = { atom };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_store:
        {
          store_body_t<Atomic> const body = { atom, &keep_a };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_exchange:
        {
          exchange_body_t<Atomic> const body = { atom, &keep_a };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_compare_exchange_strong:
        {
          compare_exchange_body_t<Atomic> const body
              = { atom, &keep_a, &keep_b };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_load_store:
        {
          load_store_body_t<Atomic> const body = { atom, &keep_a };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_load_one_writer:
        {
          if (threads < 2)
            {
              // No writer to compete with: the plain load, and the plain
              // store as the writer's figure.
              load_body_t<Atomic> const body = { atom };
              measurement = timed_run (threads, seconds, body);
              store_body_t<Atomic> const writer = { atom, &keep_a };
              measurement_t const stored = timed_run (1, seconds, writer);
              measurement.writer_ns_per_op = stored.ns_per_op;
              measurement.writer_operations = stored.operations;
              break;
            }
          std::atomic<int> next_index (0);
          readers_and_writer_body_t<Atomic> const body
              = { atom, &keep_a, &keep_b, &next_index };
          measurement = timed_run (threads, seconds, body);
          // The time per load of a reader thread: the writer is one of the
          // threads that timed_run divided by.
          if (measurement.operations != 0u)
            measurement.ns_per_op
                = measurement.seconds * 1e9 * static_cast<double> (threads - 1)
                  / static_cast<double> (measurement.operations);
          // The writer's stores (one thread): the time per store.
          measurement.writer_operations = measurement.successes;
          if (measurement.writer_operations != 0u)
            measurement.writer_ns_per_op
                = measurement.seconds * 1e9
                  / static_cast<double> (measurement.writer_operations);
          measurement.successes = 0u;
          break;
        }
      default:
        return measurement;
      }
  }
  // The atomic is gone: each value must be back to its own reference, after
  // the engine settled what it still held back.
  traits_t::settle ();
  measurement.valid = measurement.valid && keep_a.use_count () == 1
                      && keep_b.use_count () == 1;
  return measurement;
}

/**
 * The weak atomics: times `load ()` of the weak pointer or `load ()` and
 * `weak_ptr::lock ()` on a weak pointer to an object that one strong pointer
 * keeps alive, and checks afterwards that this pointer holds its own
 * reference only.
 */
template <typename Atomic>
measurement_t
measure_weak (int operation, int threads, double seconds, bool contended)
{
  typedef typename Atomic::value_type weak_type;
  typedef decltype (std::declval<weak_type const &> ().lock ()) strong_type;
  if (!contended)
    threads = 1;
  strong_type const keep = pointer_maker_t<strong_type>::make (1);
  measurement_t measurement = { 0.0, 0u, 0u, 0.0, false, 0.0, 0u, 0.0, 0.0 };
  {
    weak_type const initial (keep);
    atomic_holder_t<Atomic> const holder (initial);
    Atomic *const atom = holder.get ();
    switch (operation)
      {
      case operation_weak_load:
        {
          weak_load_body_t<Atomic> const body = { atom };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      case operation_weak_load_lock:
        {
          weak_lock_body_t<Atomic> const body = { atom };
          measurement = timed_run (threads, seconds, body);
          break;
        }
      default:
        return measurement;
      }
  }
  settle_hazard_domain ();
  measurement.valid = measurement.valid && keep.use_count () == 1;
  return measurement;
}

/** Collects the destruction times of the objects of the latency scenario. */
class destruction_log_t
{
public:
  explicit destruction_log_t (std::size_t objects)
      : destroyed_ (new std::atomic<std::int64_t>[objects]),
        destroyed_count_ (0), size_ (objects)
  {
    for (std::size_t i = 0; i < objects; ++i)
      destroyed_[i].store (0, std::memory_order_relaxed);
  }

  static std::int64_t
  now ()
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds> (
               std::chrono::steady_clock::now ().time_since_epoch ())
        .count ();
  }

  /** Called by the deleter of object `index`. */
  void
  destroyed (std::size_t index)
  {
    destroyed_[index].store (now (), std::memory_order_relaxed);
    destroyed_count_.fetch_add (1, std::memory_order_relaxed);
  }

  std::int64_t
  destroyed_at (std::size_t index) const
  {
    return destroyed_[index].load (std::memory_order_relaxed);
  }

  unsigned long long
  destroyed_count () const
  {
    return destroyed_count_.load (std::memory_order_relaxed);
  }

  std::size_t
  size () const
  {
    return size_;
  }

private:
  destruction_log_t (destruction_log_t const &);
  destruction_log_t &operator= (destruction_log_t const &);

  std::unique_ptr<std::atomic<std::int64_t>[]> destroyed_;
  std::atomic<unsigned long long> destroyed_count_;
  std::size_t size_;
};

/** The deleter of the objects of the latency scenario. */
struct logging_deleter_t
{
  destruction_log_t *log;
  std::size_t index;

  void
  operator() (int *object) const
  {
    log->destroyed (index);
    delete object;
  }
};

/** Loads in a loop until `stop`; the readers of the latency scenario. */
template <typename Atomic> struct latency_reader_t
{
  Atomic *atom;
  std::atomic<bool> *stop;

  void
  operator() () const
  {
    std::uintptr_t checksum = 0u;
    while (!stop->load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.get ());
      }
    if (checksum == 1u)
      stop->store (true);
  }
};

/**
 * Replaces the value `objects` times with a new object that has a logging
 * deleter, `threads - 1` readers loading all the while. For object i, the
 * latency is the time from the start of the store that replaced it (store
 * i + 1) to its destructor; "in call" means the destructor ran before that
 * store returned. An object whose destructor had not run when the loop ended
 * is counted in `pending_end` and left out of the percentiles; the atomic is
 * then destroyed, the engine settles, and every object must be destroyed
 * exactly once.
 */
template <typename Atomic>
latency_result_t
measure_latency (int threads, long objects)
{
  typedef atomic_traits_t<Atomic> traits_t;
  std::size_t const count = static_cast<std::size_t> (objects) + 1u;
  destruction_log_t log (count);
  std::vector<std::int64_t> started (count, 0);
  std::vector<std::int64_t> returned (count, 0);
  latency_result_t result = { 0.0, 0.0, 0.0, 0u, 0u, 0u, 0u, false };
  std::int64_t loop_end = 0;
  {
    logging_deleter_t const first = { &log, 0u };
    atomic_holder_t<Atomic> const holder (
        traits_t::make_with_deleter (0, first));
    Atomic *const atom = holder.get ();
    std::atomic<bool> stop (false);
    std::vector<std::thread> readers;
    for (int t = 1; t < threads; ++t)
      {
        latency_reader_t<Atomic> const reader = { atom, &stop };
        readers.push_back (std::thread (reader));
      }
    // Let the readers start loading.
    std::this_thread::sleep_for (std::chrono::milliseconds (5));
    for (std::size_t i = 1; i < count; ++i)
      {
        logging_deleter_t const deleter = { &log, i };
        typename Atomic::value_type fresh
            = traits_t::make_with_deleter (static_cast<int> (i), deleter);
        started[i] = destruction_log_t::now ();
        atom->store (std::move (fresh));
        returned[i] = destruction_log_t::now ();
        unsigned long long const alive = i - log.destroyed_count ();
        // `alive` counts the current value too.
        if (alive > 1u && alive - 1u > result.peak_backlog)
          result.peak_backlog = alive - 1u;
      }
    loop_end = destruction_log_t::now ();
    stop.store (true);
    for (std::size_t t = 0; t < readers.size (); ++t)
      readers[t].join ();
  }
  traits_t::settle ();
  result.valid = log.destroyed_count () == count;

  // Object i (i < count - 1) was replaced by store i + 1.
  std::vector<double> latencies;
  latencies.reserve (count);
  for (std::size_t i = 0; i + 1 < count; ++i)
    {
      std::int64_t const at = log.destroyed_at (i);
      if (at > loop_end)
        {
          ++result.pending_end;
          continue;
        }
      if (at <= returned[i + 1])
        ++result.in_call;
      double const latency = static_cast<double> (at - started[i + 1]);
      latencies.push_back (latency < 0.0 ? 0.0 : latency);
    }
  result.objects = count - 1u;
  if (!latencies.empty ())
    {
      std::sort (latencies.begin (), latencies.end ());
      result.median_ns = latencies[latencies.size () / 2];
      result.p99_ns = latencies[latencies.size () * 99u / 100u];
      result.max_ns = latencies.back ();
    }
  return result;
}

/** Pins the calling thread to CPU `index` (modulo the CPUs); false: could not. */
inline bool
pin_current_thread (int index)
{
#if defined(__linux__)
  unsigned const cpus = std::thread::hardware_concurrency ();
  if (cpus == 0u)
    return false;
  cpu_set_t set;
  CPU_ZERO (&set);
  CPU_SET (static_cast<unsigned> (index) % cpus, &set);
  return ::pthread_setaffinity_np (::pthread_self (), sizeof (set), &set)
         == 0;
#else
  (void)index;
  return false;
#endif
}

/** The nanoseconds of the monotonic clock. */
inline std::int64_t
clock_ns ()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds> (
             std::chrono::steady_clock::now ().time_since_epoch ())
      .count ();
}

/**
 * The involuntary context switches of the calling thread so far (the thread
 * was taken off its CPU by the scheduler); 0 where the system does not say.
 */
inline unsigned long long
involuntary_switches ()
{
#if defined(__linux__) && defined(RUSAGE_THREAD)
  rusage usage;
  if (::getrusage (RUSAGE_THREAD, &usage) != 0)
    return 0u;
  return static_cast<unsigned long long> (usage.ru_nivcsw);
#else
  return 0u;
#endif
}

/** The value of quantile `fraction` of the sorted `values`. */
inline double
quantile_of_sorted (std::vector<std::int64_t> const &values, double fraction)
{
  std::size_t index = static_cast<std::size_t> (
      fraction * static_cast<double> (values.size ()));
  if (index >= values.size ())
    index = values.size () - 1u;
  return static_cast<double> (values[index]);
}

/** Loads in a loop until `stop`; a reader of the store-wait scenario. */
template <typename Atomic> struct store_wait_reader_t
{
  Atomic *atom;
  std::atomic<bool> *stop;
  std::atomic<int> *started;
  int index;
  bool pin;
  std::atomic<int> *pin_failures;

  void
  operator() () const
  {
    if (pin && !pin_current_thread (index))
      pin_failures->fetch_add (1);
    started->fetch_add (1);
    std::uintptr_t checksum = 0u;
    while (!stop->load (std::memory_order_relaxed))
      {
        typename Atomic::value_type const snapshot = atom->load ();
        checksum += reinterpret_cast<std::uintptr_t> (snapshot.get ());
      }
    if (checksum == 1u)
      stop->store (true);
  }
};

/** The writer of the store-wait scenario; times every store. */
template <typename Atomic> struct store_wait_writer_t
{
  Atomic *atom;
  typename Atomic::value_type const *keep_a;
  typename Atomic::value_type const *keep_b;
  std::atomic<int> *started;
  int readers;
  long stores;
  bool pin;
  std::atomic<int> *pin_failures;
  std::vector<std::int64_t> *waits;
  double *timer_ns;
  unsigned long long *switches;
  unsigned long long *over_1ms_switched;

  void
  operator() () const
  {
    if (pin && !pin_current_thread (0))
      pin_failures->fetch_add (1);
    while (started->load () < readers)
      std::this_thread::yield ();
    // Every reader has run its first loads; the stores that follow are the
    // measurement, after a few that bring the caches to their steady state.
    bool flip = false;
    for (int i = 0; i < 2000; ++i)
      {
        atom->store (flip ? *keep_a : *keep_b);
        flip = !flip;
      }
    // The cost of the timer: an empty pair of clock reads, with the readers
    // running.
    std::vector<std::int64_t> empty (20000u, 0);
    for (std::size_t i = 0; i < empty.size (); ++i)
      {
        std::int64_t const before = clock_ns ();
        empty[i] = clock_ns () - before;
      }
    std::sort (empty.begin (), empty.end ());
    *timer_ns = static_cast<double> (empty[empty.size () / 2u]);

    // A safety stop: a run that has not finished in a minute is cut short
    // (the number of stores timed tells).
    std::int64_t const give_up = clock_ns () + 60000000000ll;
    std::size_t const count = static_cast<std::size_t> (stores);
    waits->reserve (count);
    // The scheduler's counter is read only after a store over 1 ms, never
    // inside the timed loop of the ordinary stores (a system call per store
    // would change what is measured).
    unsigned long long const switches_first = involuntary_switches ();
    unsigned long long switches_seen = switches_first;
    for (std::size_t i = 0; i < count; ++i)
      {
        typename Atomic::value_type const &value = flip ? *keep_a : *keep_b;
        std::int64_t const before = clock_ns ();
        atom->store (value);
        std::int64_t const after = clock_ns ();
        waits->push_back (after - before);
        flip = !flip;
        if (after - before > 1000000)
          {
            unsigned long long const now_seen = involuntary_switches ();
            if (now_seen != switches_seen)
              ++*over_1ms_switched;
            switches_seen = now_seen;
          }
        if ((i & 1023u) == 1023u && after > give_up)
          break;
      }
    *switches = involuntary_switches () - switches_first;
  }
};

/**
 * Times every single `store ()` of one writer (the thread with index 0)
 * while `threads - 1` readers `load ()` all the while, `stores` times with
 * alternating values, so that every store replaces the value. Reports the
 * median, the 90th, 99th, 99.9th and 99.99th percentile and the longest
 * time, and how many stores took more than a millisecond (a preempted
 * writer shows there). The clock is std::chrono::steady_clock; its own cost
 * (an empty pair of reads) is measured next to the stores and reported.
 */
template <typename Atomic>
store_wait_result_t
measure_store_wait (int threads, long stores, bool pin)
{
  typedef atomic_traits_t<Atomic> traits_t;
  typedef typename traits_t::value_type pointer_t;
  pointer_t const keep_a (traits_t::make (1));
  pointer_t const keep_b (traits_t::make (2));
  store_wait_result_t result = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0u, 0u,
                                 0u,  0u,  false, false };
  std::vector<std::int64_t> waits;
  std::atomic<int> pin_failures (0);
  {
    atomic_holder_t<Atomic> const holder (keep_a);
    Atomic *const atom = holder.get ();
    std::atomic<bool> stop (false);
    std::atomic<int> started (0);
    std::vector<std::thread> readers;
    int const reader_count = threads > 1 ? threads - 1 : 0;
    for (int t = 1; t <= reader_count; ++t)
      {
        store_wait_reader_t<Atomic> const reader
            = { atom, &stop, &started, t, pin, &pin_failures };
        readers.push_back (std::thread (reader));
      }
    store_wait_writer_t<Atomic> const writer
        = { atom,   &keep_a, &keep_b, &started,      reader_count,
            stores, pin,     &pin_failures, &waits, &result.timer_ns,
            &result.switches, &result.over_1ms_switched };
    std::thread writer_thread (writer);
    writer_thread.join ();
    stop.store (true);
    for (std::size_t t = 0; t < readers.size (); ++t)
      readers[t].join ();
  }
  traits_t::settle ();
  result.valid = keep_a.use_count () == 1 && keep_b.use_count () == 1
                 && !waits.empty ();
  result.pinned = pin && pin_failures.load () == 0;
  result.stores = waits.size ();
  for (std::size_t i = 0; i < waits.size (); ++i)
    if (waits[i] > 1000000)
      ++result.over_1ms;
  if (!waits.empty ())
    {
      std::sort (waits.begin (), waits.end ());
      result.median_ns = quantile_of_sorted (waits, 0.5);
      result.p90_ns = quantile_of_sorted (waits, 0.9);
      result.p99_ns = quantile_of_sorted (waits, 0.99);
      result.p999_ns = quantile_of_sorted (waits, 0.999);
      result.p9999_ns = quantile_of_sorted (waits, 0.9999);
      result.max_ns = static_cast<double> (waits.back ());
    }
  return result;
}

/** Fills an implementation_t for `Atomic`. */
template <typename Atomic>
implementation_t
make_implementation (char const *key, char const *label, char const *path,
                     long standard)
{
  Atomic const probe;
  implementation_t const implementation = { key,
                                            label,
                                            path,
                                            standard,
                                            probe.is_lock_free (),
                                            &measure<Atomic>,
                                            &measure_latency<Atomic>,
                                            kind_shared,
                                            &measure_store_wait<Atomic> };
  return implementation;
}

/** Fills an implementation_t for the weak atomic `Atomic`. */
template <typename Atomic>
implementation_t
make_weak_implementation (char const *key, char const *label, char const *path,
                          long standard)
{
  Atomic const probe;
  implementation_t const implementation = { key,
                                            label,
                                            path,
                                            standard,
                                            probe.is_lock_free (),
                                            &measure_weak<Atomic>,
                                            nullptr,
                                            kind_weak,
                                            nullptr };
  return implementation;
}
} // namespace detail
} // namespace lumex_atomic_bench

#endif // LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP

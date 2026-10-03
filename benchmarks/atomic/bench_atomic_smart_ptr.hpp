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
 * translation unit: LumexLib's lock-based implementation built at C++11
 * (sleeping on the striped wait table) and at C++20 (sleeping in
 * std::atomic::wait), LumexLib's default selection at C++20, and the standard
 * library's std::atomic<std::shared_ptr<T>>. Each unit instantiates the
 * templates below for its own type and hands main () an implementation_t:
 * plain data and a function pointer, so units built with different standards
 * meet only through types that mean the same in every standard.
 *
 * The method is the one the author used for the libc++ implementation of
 * llvm-project pull request 194215: N threads run one operation on one
 * shared object for a fixed time, and every result is divided by
 * std::atomic<std::uint64_t>::compare_exchange_strong timed in the same
 * process, at the same thread count, right before.
 */

#ifndef LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP
#define LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <vector>

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
/** The four timed operations, in the order of the CSV. */
enum operation_t
{
  operation_load = 0,
  operation_store = 1,
  operation_exchange = 2,
  operation_compare_exchange_strong = 3,
  operation_count = 4
};

/** One timed run of one operation. */
struct measurement_t
{
  double ns_per_op;              ///< wall time per operation and thread
  unsigned long long operations; ///< all threads together
  unsigned long long successes;  ///< successful compare_exchange_strong
  double seconds;                ///< mean timed wall time of a thread
  bool valid;                    ///< no reference left behind afterwards
};

/**
 * Times `operation` with `threads` threads for `seconds`; on one object
 * shared by all threads when `contended`, otherwise on a private one (one
 * thread).
 */
typedef measurement_t (*measure_fn) (int operation, int threads,
                                     double seconds, bool contended);

/** An implementation as main () sees it. */
struct implementation_t
{
  char const *key;   ///< CSV key, for example "lumex_lock_based_cxx11"
  char const *label; ///< chart label
  char const *path;  ///< what the build selected: ABI namespace or library
  long standard;     ///< __cplusplus (MSVC: _MSVC_LANG) of the unit
  bool lock_free;    ///< is_lock_free () of the measured object
  measure_fn measure;
};

/** LumexLib's default selection, built at C++20. */
implementation_t lumex_default_implementation ();

/** LumexLib's lock-based implementation, forced, built at C++20. */
implementation_t lumex_lock_based_implementation ();

/** LumexLib's lock-based implementation built at C++11 (MSVC: C++14). */
implementation_t lumex_lock_based_cxx11_implementation ();

namespace detail
{
/** Start and stop of one timed run; the loops only read `stop`. */
struct run_flags_t
{
  std::atomic<int> ready;
  std::atomic<bool> go;
  std::atomic<bool> stop;
};

/** What one thread reports after its timed loop. */
struct thread_result_t
{
  unsigned long long operations;
  unsigned long long successes;
  std::uintptr_t checksum; ///< keeps the loaded values observable
  double seconds;
};

/** Runs `body` on one thread between the start and the stop flag. */
template <typename Body>
void
run_worker (run_flags_t *flags, Body const *body, thread_result_t *result)
{
  flags->ready.fetch_add (1);
  while (!flags->go.load (std::memory_order_acquire))
    std::this_thread::yield ();
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  (*body) (flags->stop, *result);
  std::chrono::duration<double> const elapsed
      = std::chrono::steady_clock::now () - start;
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
  std::size_t const count = static_cast<std::size_t> (threads);
  thread_result_t const zero = { 0u, 0u, 0u, 0.0 };
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

  measurement_t measurement = { 0.0, 0u, 0u, 0.0, true };
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
  // A checksum that no value can produce never happens; the comparison only
  // keeps the loaded pointers observable to the optimizer.
  if (checksum == 1u)
    measurement.valid = false;
  return measurement;
}

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
  typedef typename Atomic::value_type pointer_t;
  if (!contended)
    threads = 1;
  pointer_t const keep_a (std::make_shared<int> (1));
  pointer_t const keep_b (std::make_shared<int> (2));
  measurement_t measurement = { 0.0, 0u, 0u, 0.0, false };
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
      default:
        return measurement;
      }
  }
  // The atomic is gone: each value must be back to its own reference.
  measurement.valid = measurement.valid && keep_a.use_count () == 1
                      && keep_b.use_count () == 1;
  return measurement;
}

/** Fills an implementation_t for `Atomic`. */
template <typename Atomic>
implementation_t
make_implementation (char const *key, char const *label, char const *path,
                     long standard)
{
  Atomic const probe;
  implementation_t const implementation = {
    key, label, path, standard, probe.is_lock_free (), &measure<Atomic>
  };
  return implementation;
}
} // namespace detail
} // namespace lumex_atomic_bench

#endif // LUMEX_BENCHMARKS_ATOMIC_BENCH_ATOMIC_SMART_PTR_HPP

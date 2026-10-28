// compile_time.cpp
// The translation unit run_benchmark.py --compile-time-only compiles with
// -fsyntax-only to time what it costs to include and use an atomic shared
// pointer. BENCH_ATOMIC_IMPL is
//   1  atomic_shared_ptr of this library, the common name (the lock-free
//      engine where the build has it, the lock-based one otherwise),
//   2  atomic_shared_ptr_lock_based,
//   3  atomic_shared_ptr_lock_free (the build must have it),
//   4  atomic_shared_ptr_std_backed (the build must have the standard type),
//   5  std::atomic<std::shared_ptr<int>> (C++20, where the library has it),
//   6  boost::atomic_shared_ptr<int> (header-only, a spinlock),
//   0  a stub: a std::shared_ptr behind a std::mutex, which is the cost of
//      the headers every variant shares.
// BENCH_COMPILE_MODE is 0 to include the header and declare one object, 1 to
// use the operations the benchmark measures (load, store, exchange,
// compare_exchange_strong) and is_lock_free.
#include <cstddef>
#include <memory>
#include <mutex>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_ATOMIC_IMPL >= 1 && BENCH_ATOMIC_IMPL <= 4
#include "lumex/core/atomic/LumexAtomic"
#endif
#if BENCH_ATOMIC_IMPL == 6
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#endif

namespace
{
#if BENCH_ATOMIC_IMPL == 0
class cell_t
{
public:
  using value_type = std::shared_ptr<int>;
  cell_t () {}
  explicit cell_t (value_type desired) : value_ (desired) {}
  bool
  is_lock_free () const
  {
    return false;
  }
  value_type
  load () const
  {
    std::lock_guard<std::mutex> guard (mutex_);
    return value_;
  }
  void
  store (value_type desired)
  {
    std::lock_guard<std::mutex> guard (mutex_);
    value_ = desired;
  }
  value_type
  exchange (value_type desired)
  {
    std::lock_guard<std::mutex> guard (mutex_);
    value_.swap (desired);
    return desired;
  }
  bool
  compare_exchange_strong (value_type &expected, value_type desired)
  {
    std::lock_guard<std::mutex> guard (mutex_);
    if (value_ == expected)
      {
        value_ = desired;
        return true;
      }
    expected = value_;
    return false;
  }

private:
  mutable std::mutex mutex_;
  value_type value_;
};
#elif BENCH_ATOMIC_IMPL == 1
using cell_t = lumex::core::atomic::smart_ptr::atomic_shared_ptr<int>;
#elif BENCH_ATOMIC_IMPL == 2
using cell_t
    = lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based<int>;
#elif BENCH_ATOMIC_IMPL == 3
#if !LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
#error "this build has no lock-free engine"
#endif
using cell_t
    = lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free<int>;
#elif BENCH_ATOMIC_IMPL == 4
#if !LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
#error "this build has no std::atomic<std::shared_ptr>"
#endif
using cell_t
    = lumex::core::atomic::smart_ptr::atomic_shared_ptr_std_backed<int>;
#elif BENCH_ATOMIC_IMPL == 5
#if !defined(__cpp_lib_atomic_shared_ptr)
#error "this standard library has no std::atomic<std::shared_ptr>"
#endif
using cell_t = std::atomic<std::shared_ptr<int>>;
#elif BENCH_ATOMIC_IMPL == 6
class cell_t
{
public:
  using value_type = boost::shared_ptr<int>;
  cell_t () {}
  explicit cell_t (value_type desired) : atom_ (desired) {}
  bool
  is_lock_free () const
  {
    return atom_.is_lock_free ();
  }
  value_type
  load () const
  {
    return atom_.load ();
  }
  void
  store (value_type desired)
  {
    atom_.store (desired);
  }
  value_type
  exchange (value_type desired)
  {
    return atom_.exchange (desired);
  }
  bool
  compare_exchange_strong (value_type &expected, value_type desired)
  {
    return atom_.compare_exchange_strong (expected, desired);
  }

private:
  boost::atomic_shared_ptr<int> atom_;
};
#else
#error "BENCH_ATOMIC_IMPL must be 0 to 6"
#endif

using pointer_t = cell_t::value_type;
} // namespace

int
main ()
{
  std::size_t result = 0;
#if BENCH_COMPILE_MODE == 0
  cell_t cell;
  result += cell.is_lock_free () ? 1 : 0;
#else
  cell_t cell;
  cell_t other (pointer_t (new int (1)));
  pointer_t first = cell.load ();
  cell.store (pointer_t (new int (2)));
  pointer_t second = cell.exchange (first);
  pointer_t expected = second;
  bool const swapped = cell.compare_exchange_strong (expected, first);
  pointer_t third = other.load ();
  result += (swapped ? 1 : 0) + (third ? 1 : 0) + (second ? 1 : 0)
            + (cell.is_lock_free () ? 1 : 0);
#endif
  return result == 0 ? 1 : 0;
}

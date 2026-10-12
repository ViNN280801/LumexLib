// compile_time.cpp
// The translation unit run_benchmark.py --compile-time-only compiles with
// -fsyntax-only to time what it costs to include and use a way of reading a
// read-mostly pointer that another thread replaces. BENCH_HP_IMPL is
//   1  the hazard pointers of this library (the reader side is header-only),
//   2  a std::shared_mutex read lock around the read (C++17),
//   3  std::atomic_load of a std::shared_ptr,
//   0  a stub: an unprotected std::atomic<T *>, which is the cost of the
//      headers every variant shares.
// BENCH_COMPILE_MODE is 0 to include the header and declare one object, 1 to
// do what the benchmark does: read the pointer under protection, replace it
// and retire (delete) the old one.
#include <atomic>
#include <cstddef>
#include <memory>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_HP_IMPL == 1
#include "lumex/core/hazard_pointer/LumexHazardPointer"
#elif BENCH_HP_IMPL == 2
#include <mutex>
#include <shared_mutex>
#endif

namespace
{
#if BENCH_HP_IMPL == 1
namespace hp = lumex::core::hazard_pointer;
struct node : hp::hazard_pointer_obj_base<node>
{
  explicit node (std::size_t v) : value (v) {}
  std::size_t value;
};
#else
struct node
{
  explicit node (std::size_t v) : value (v) {}
  std::size_t value;
};
#endif
} // namespace

int
main ()
{
  std::size_t result = 0;
#if BENCH_HP_IMPL == 0
  std::atomic<node *> source (new node (1));
#if BENCH_COMPILE_MODE == 0
  result += source.load () != nullptr ? 1 : 0;
#else
  result += source.load (std::memory_order_acquire)->value;
  node *old = source.exchange (new node (2));
  result += old->value;
  delete old;
#endif
  delete source.load ();
#elif BENCH_HP_IMPL == 1
  std::atomic<node *> source (new node (1));
#if BENCH_COMPILE_MODE == 0
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  result += holder.empty () ? 1 : 0;
#else
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    node *seen = holder.protect (source);
    result += seen->value;
    node *probe = source.load (std::memory_order_relaxed);
    result += holder.try_protect (probe, source) ? 1 : 0;
    holder.reset_protection ();
    hp::hazard_pointer other = hp::make_hazard_pointer ();
    swap (holder, other);
  }
  node *old = source.exchange (new node (2));
  old->retire ();
#endif
  source.load ()->retire ();
#elif BENCH_HP_IMPL == 2
  std::shared_mutex mutex;
  node *current = new node (1);
#if BENCH_COMPILE_MODE == 0
  {
    std::shared_lock<std::shared_mutex> read (mutex);
    result += current != nullptr ? 1 : 0;
  }
#else
  {
    std::shared_lock<std::shared_mutex> read (mutex);
    result += current->value;
  }
  {
    std::unique_lock<std::shared_mutex> write (mutex);
    node *old = current;
    current = new node (2);
    result += old->value;
    delete old;
  }
#endif
  delete current;
#else
  std::shared_ptr<node> current (new node (1));
#if BENCH_COMPILE_MODE == 0
  result += std::atomic_load (&current) ? 1 : 0;
#else
  std::shared_ptr<node> seen = std::atomic_load (&current);
  result += seen->value;
  std::atomic_store (&current, std::shared_ptr<node> (new node (2)));
  result += current->value;
#endif
#endif
  return result == 0 ? 1 : 0;
}

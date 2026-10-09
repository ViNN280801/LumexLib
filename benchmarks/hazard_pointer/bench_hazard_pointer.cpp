// Benchmark of the hazard pointer module of LumexLib.
//
// The method is the one of benchmarks/atomic: every number is a ratio to a
// std::atomic<std::uint64_t> baseline (a relaxed load followed by a
// compare-exchange on one shared word) measured in the same process, with the
// same threads and at the same moment. The series are the module's own
// engine, the unprotected load of the same pointer (the lower bound of a read),
// a std::shared_mutex read lock and std::atomic_load of a std::shared_ptr (the
// usual alternatives for a read-mostly pointer), and, when the build points at
// a local copy, libcds HP and a port of the libc++ pull request engine.
//
// One run of the program is one repetition (run_benchmark.py runs many and takes
// medians). Output: --csv <file>, one row per series, mode, thread count and
// operation; --list prints the series.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "lumex/core/hazard_pointer/LumexHazardPointer"

#if defined(LUMEX_HP_BENCH_LIBCDS)
#include "adapter_libcds.hpp"
#endif
#if defined(LUMEX_HP_BENCH_LLVM)
#include "adapter_llvm.hpp"
#endif

namespace
{
namespace hp = lumex::core::hazard_pointer;
typedef std::chrono::steady_clock clock_type;

// Keeps a value alive without a store the compiler could reorder.
template <class T>
inline void
keep (T const &value)
{
#if defined(_MSC_VER)
  _ReadWriteBarrier ();
  static_cast<void> (&value);
#else
  asm volatile ("" : : "g"(&value) : "memory");
#endif
}

// ---------------------------------------------------------------------------
// Series
// ---------------------------------------------------------------------------

enum operation_id
{
  op_baseline,
  op_make_destroy,
  op_protect_reset,
  op_protect_kept,
  op_make_protect_destroy,
  op_retire,
  op_new_delete,
  op_read,
  op_list_walk,
  op_readers_writer,
  op_retire_p50,
  op_retire_p99,
  op_retire_p999,
  op_retire_max,
  operation_count
};

char const *const operation_names[operation_count] = {
  "uint64_cas",  "make_destroy", "protect_reset", "protect_kept",
  "make_protect_destroy", "retire", "new_delete", "read", "list_walk",
  "readers_writer", "retire_p50", "retire_p99", "retire_p999", "retire_max"
};

struct measurement
{
  std::uint64_t operations;
  std::uint64_t successes;
  bool valid;
};

struct stop_flag
{
  std::atomic<bool> stop;
  stop_flag () : stop (false) {}
};

// The payload every series protects.
struct payload
{
  std::uint64_t value;
};

// --- the module's engine ------------------------------------------------------

struct lumex_series
{
  static char const *key () { return "lumex"; }
  static char const *label () { return "LumexLib hazard_pointer"; }
  static char const *detail () { return "own engine, seq_cst fences"; }

  struct node : hp::hazard_pointer_obj_base<node>
  {
    explicit node (std::uint64_t v) : value (v) {}
    std::uint64_t value;
  };

  struct holder_type
  {
    hp::hazard_pointer holder;
    holder_type () : holder (hp::make_hazard_pointer ()) {}
    node *protect (std::atomic<node *> const &source) { return holder.protect (source); }
    void reset () { holder.reset_protection (); }
  };

  static void settle () { hp::clean_up (); }
};

// --- libcds and the libc++ port (local copies only) -------------------------------

// The adapters define series with the same shape as lumex_series; they come
// from the headers above.

// --- alternatives for a read-mostly pointer (operation read only) --------------

struct unprotected_series
{
  static char const *key () { return "unprotected"; }
  static char const *label () { return "plain acquire load (unsafe)"; }
  static char const *detail () { return "lower bound of a read"; }
};

struct shared_mutex_series
{
  static char const *key () { return "shared_mutex"; }
  static char const *label () { return "std::shared_mutex read lock"; }
  static char const *detail () { return "lock_shared / unlock_shared"; }
};

struct shared_ptr_series
{
  static char const *key () { return "shared_ptr"; }
  static char const *label () { return "std::atomic_load (shared_ptr)"; }
  static char const *detail () { return "copy and release a shared_ptr"; }
};

// ---------------------------------------------------------------------------
// The timing frame
// ---------------------------------------------------------------------------

struct options
{
  std::string csv;
  int repetition;
  int duration_ms;
  int warmup_ms;
  std::vector<int> threads;
  bool list;
  bool only_baseline_check;
};

// Runs `body (thread index, stop flag, result)` on `threads` threads that start
// together for `duration_ms`; returns the wall time per operation and thread
// in nanoseconds (the figure Google Benchmark reports with UseRealTime) and
// fills `total` with the sums.
template <class Body>
double
run_threads (int threads, int duration_ms, Body body, measurement &total)
{
  std::atomic<int> ready (0);
  std::atomic<bool> go (false);
  stop_flag stop;
  std::vector<measurement> results (static_cast<std::size_t> (threads));
  std::vector<std::thread> workers;
  for (int t = 0; t < threads; ++t)
    {
      workers.emplace_back ([&, t] {
        ready.fetch_add (1);
        while (!go.load (std::memory_order_acquire))
          {
          }
        body (t, stop, results[static_cast<std::size_t> (t)]);
      });
    }
  while (ready.load () < threads)
    {
      std::this_thread::yield ();
    }
  clock_type::time_point const begin = clock_type::now ();
  go.store (true, std::memory_order_release);
  std::this_thread::sleep_for (std::chrono::milliseconds (duration_ms));
  stop.stop.store (true, std::memory_order_release);
  for (std::thread &worker : workers)
    {
      worker.join ();
    }
  double const seconds
      = std::chrono::duration<double> (clock_type::now () - begin).count ();
  total.operations = 0;
  total.successes = 0;
  total.valid = true;
  for (measurement const &result : results)
    {
      total.operations += result.operations;
      total.successes += result.successes;
      total.valid = total.valid && result.valid;
    }
  if (total.operations == 0)
    {
      return 0.0;
    }
  // Per thread: every thread ran for the whole window.
  return seconds * 1e9 * threads / static_cast<double> (total.operations);
}

// The baseline of the libc++ benchmark: a relaxed load followed by a
// compare-exchange on one shared word.
double
baseline (int threads, int duration_ms, measurement &total)
{
  std::atomic<std::uint64_t> word (0);
  return run_threads (
      threads, duration_ms,
      [&word] (int, stop_flag &stop, measurement &out) {
        std::uint64_t operations = 0;
        std::uint64_t successes = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                std::uint64_t expected = word.load (std::memory_order_relaxed);
                if (word.compare_exchange_strong (expected, expected + 1))
                  {
                    ++successes;
                  }
                ++operations;
              }
          }
        out.operations = operations;
        out.successes = successes;
        out.valid = true;
      },
      total);
}

// --- operations over a hazard pointer series ------------------------------------

template <class S>
double
make_destroy (int threads, int duration_ms, measurement &total)
{
  return run_threads (
      threads, duration_ms,
      [] (int, stop_flag &stop, measurement &out) {
        std::uint64_t operations = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                typename S::holder_type holder;
                keep (holder);
                ++operations;
              }
          }
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
}

template <class S>
double
protect_reset (int threads, int duration_ms, measurement &total, bool keep_protection)
{
  typename S::node *shared = new typename S::node (1);
  std::atomic<typename S::node *> source (shared);
  double const ns = run_threads (
      threads, duration_ms,
      [&source, keep_protection] (int, stop_flag &stop, measurement &out) {
        typename S::holder_type holder;
        std::uint64_t operations = 0;
        std::uint64_t sum = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                typename S::node *node = holder.protect (source);
                sum += node->value;
                if (!keep_protection)
                  {
                    holder.reset ();
                  }
                ++operations;
              }
          }
        keep (sum);
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
  shared->retire ();
  S::settle ();
  return ns;
}

template <class S>
double
make_protect_destroy (int threads, int duration_ms, measurement &total)
{
  typename S::node *shared = new typename S::node (1);
  std::atomic<typename S::node *> source (shared);
  double const ns = run_threads (
      threads, duration_ms,
      [&source] (int, stop_flag &stop, measurement &out) {
        std::uint64_t operations = 0;
        std::uint64_t sum = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                typename S::holder_type holder;
                sum += holder.protect (source)->value;
                ++operations;
              }
          }
        keep (sum);
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
  shared->retire ();
  S::settle ();
  return ns;
}

// Allocate, retire and let the module reclaim: includes the inline passes.
template <class S>
double
retire_cost (int threads, int duration_ms, measurement &total)
{
  double const ns = run_threads (
      threads, duration_ms,
      [] (int, stop_flag &stop, measurement &out) {
        std::uint64_t operations = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                (new typename S::node (operations))->retire ();
                ++operations;
              }
          }
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
  S::settle ();
  return ns;
}

double
new_delete_cost (int threads, int duration_ms, measurement &total)
{
  return run_threads (
      threads, duration_ms,
      [] (int, stop_flag &stop, measurement &out) {
        std::uint64_t operations = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                payload *object = new payload;
                object->value = operations;
                keep (object);
                delete object;
                ++operations;
              }
          }
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
}

// Per-call latency of retire (one thread): percentiles in nanoseconds.
template <class S>
void
retire_latency (int duration_ms, double percentiles[4], measurement &total)
{
  std::vector<std::uint32_t> samples;
  samples.reserve (1 << 22);
  clock_type::time_point const end
      = clock_type::now () + std::chrono::milliseconds (duration_ms);
  std::uint64_t count = 0;
  while (clock_type::now () < end && samples.size () < (1u << 22))
    {
      typename S::node *node = new typename S::node (count);
      clock_type::time_point const start = clock_type::now ();
      node->retire ();
      clock_type::time_point const stop = clock_type::now ();
      samples.push_back (static_cast<std::uint32_t> (
          std::chrono::duration_cast<std::chrono::nanoseconds> (stop - start)
              .count ()));
      ++count;
    }
  S::settle ();
  std::sort (samples.begin (), samples.end ());
  auto at = [&samples] (double fraction) {
    if (samples.empty ())
      {
        return 0.0;
      }
    std::size_t index = static_cast<std::size_t> (
        fraction * static_cast<double> (samples.size () - 1));
    return static_cast<double> (samples[index]);
  };
  percentiles[0] = at (0.5);
  percentiles[1] = at (0.99);
  percentiles[2] = at (0.999);
  percentiles[3] = samples.empty () ? 0.0 : static_cast<double> (samples.back ());
  total.operations = count;
  total.successes = count;
  total.valid = true;
}

// A static list of 64 nodes, walked from the head; ops are nodes visited.
// The hazard walk is hand-over-hand with two holders (the single-writer list
// of P2530R3, section 3.3); the plain walk only loads the links.
struct list_node : hp::hazard_pointer_obj_base<list_node>
{
  explicit list_node (std::uint64_t v) : next (nullptr), value (v) {}
  std::atomic<list_node *> next;
  std::uint64_t value;
};

struct list_t
{
  std::atomic<list_node *> head;
  std::vector<list_node *> nodes;

  list_t () : head (nullptr)
  {
    list_node *previous = nullptr;
    for (int i = 63; i >= 0; --i)
      {
        list_node *node = new list_node (static_cast<std::uint64_t> (i));
        node->next.store (previous);
        previous = node;
        nodes.push_back (node);
      }
    head.store (previous);
  }

  ~list_t ()
  {
    for (list_node *node : nodes)
      {
        delete node;
      }
  }
};

double
list_walk_cost (int threads, int duration_ms, measurement &total, bool protect)
{
  list_t list;
  return run_threads (
      threads, duration_ms,
      [&list, protect] (int, stop_flag &stop, measurement &out) {
        hp::hazard_pointer first = hp::make_hazard_pointer ();
        hp::hazard_pointer second = hp::make_hazard_pointer ();
        std::uint64_t operations = 0;
        std::uint64_t sum = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            if (protect)
              {
                list_node *node = first.protect (list.head);
                while (node != nullptr)
                  {
                    sum += node->value;
                    ++operations;
                    list_node *next = second.protect (node->next);
                    swap (first, second);
                    node = next;
                  }
                first.reset_protection ();
                second.reset_protection ();
              }
            else
              {
                for (list_node *node = list.head.load (std::memory_order_acquire);
                     node != nullptr;
                     node = node->next.load (std::memory_order_acquire))
                  {
                    sum += node->value;
                    ++operations;
                  }
              }
          }
        keep (sum);
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
}

// The read operation over every way of reading a read-mostly pointer.
// A series with hazard pointers (S::node, S::holder_type, S::settle).
template <class S> struct reader
{
  struct state
  {
    std::atomic<typename S::node *> source;
    state () : source (new typename S::node (7)) {}
    ~state ()
    {
      source.load ()->retire ();
      S::settle ();
    }
  };

  struct local
  {
    typename S::holder_type holder;
  };

  static std::uint64_t
  read (state &s, local &l)
  {
    std::uint64_t const value = l.holder.protect (s.source)->value;
    l.holder.reset ();
    return value;
  }

  static void
  write (state &s)
  {
    typename S::node *old = s.source.exchange (new typename S::node (7));
    old->retire ();
  }
};

template <> struct reader<unprotected_series>
{
  struct state
  {
    std::atomic<payload *> source;
    state () : source (new payload) { source.load ()->value = 7; }
    ~state () { delete source.load (); }
  };
  struct local {};

  static std::uint64_t
  read (state &s, local &)
  {
    return s.source.load (std::memory_order_acquire)->value;
  }

  static void write (state &) {}
};

template <> struct reader<shared_mutex_series>
{
  struct state
  {
    std::shared_mutex lock;
    payload object;
    state () { object.value = 7; }
  };
  struct local {};

  static std::uint64_t
  read (state &s, local &)
  {
    s.lock.lock_shared ();
    std::uint64_t const value = s.object.value;
    s.lock.unlock_shared ();
    return value;
  }

  static void
  write (state &s)
  {
    s.lock.lock ();
    s.object.value = 7;
    s.lock.unlock ();
  }
};

template <> struct reader<shared_ptr_series>
{
  struct state
  {
    std::shared_ptr<payload> pointer;
    state () : pointer (new payload) { pointer->value = 7; }
  };
  struct local {};

  static std::uint64_t
  read (state &s, local &)
  {
    std::shared_ptr<payload> copy = std::atomic_load (&s.pointer);
    return copy->value;
  }

  static void
  write (state &s)
  {
    std::shared_ptr<payload> fresh (new payload);
    fresh->value = 7;
    std::atomic_store (&s.pointer, fresh);
  }
};

template <class S>
double
read_cost (int threads, int duration_ms, measurement &total)
{
  typename reader<S>::state state;
  return run_threads (
      threads, duration_ms,
      [&state] (int, stop_flag &stop, measurement &out) {
        typename reader<S>::local local;
        std::uint64_t operations = 0;
        std::uint64_t sum = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                sum += reader<S>::read (state, local);
                ++operations;
              }
          }
        keep (sum);
        out.operations = operations;
        out.successes = operations;
        out.valid = true;
      },
      total);
}

// Readers and one writer that replaces the object at full speed; the time is
// per read.
template <class S>
double
readers_writer (int threads, int duration_ms, measurement &total)
{
  typename reader<S>::state state;
  std::atomic<std::uint64_t> reads (0);
  std::atomic<std::uint64_t> writes (0);
  measurement all = { 0, 0, true };
  double const ns = run_threads (
      threads, duration_ms,
      [&state, &reads, &writes] (int index, stop_flag &stop, measurement &out) {
        out.operations = 0;
        out.successes = 0;
        out.valid = true;
        if (index == 0)
          {
            std::uint64_t count = 0;
            while (!stop.stop.load (std::memory_order_relaxed))
              {
                reader<S>::write (state);
                ++count;
              }
            writes.fetch_add (count);
            return;
          }
        typename reader<S>::local local;
        std::uint64_t operations = 0;
        std::uint64_t sum = 0;
        while (!stop.stop.load (std::memory_order_relaxed))
          {
            for (int i = 0; i < 32; ++i)
              {
                sum += reader<S>::read (state, local);
                ++operations;
              }
          }
        keep (sum);
        out.operations = operations;
        out.successes = operations;
        reads.fetch_add (operations);
      },
      all);
  total = all;
  (void) ns;
  // Time per read: the readers ran for the whole window.
  double const seconds_total = static_cast<double> (duration_ms) * 1e6;
  return reads.load () == 0
             ? 0.0
             : seconds_total * (threads - 1) / static_cast<double> (reads.load ());
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

struct row_writer
{
  std::FILE *file;
  int repetition;

  void
  put (char const *mode, int threads, char const *implementation,
       char const *operation, double ratio, double ns,
       measurement const &m)
  {
    std::fprintf (file, "%s,%d,%s,%s,%d,%.6f,%.3f,%llu,%llu,%d\n", mode,
                  threads, implementation, operation, repetition, ratio, ns,
                  static_cast<unsigned long long> (m.operations),
                  static_cast<unsigned long long> (m.successes),
                  m.valid ? 1 : 0);
  }
};

template <class S> struct has_holder
{
  static void
  list (std::vector<std::string> &lines)
  {
    lines.push_back (std::string (S::key ()) + "|" + S::label () + "|"
                     + S::detail () + "|11|lock-free reader");
  }
};

template <class S>
void
measure_hazard_series (row_writer &out, char const *mode, int threads,
                       options const &opt, double base_ns)
{
  measurement total = { 0, 0, true };
  double ns;
  ns = make_destroy<S> (threads, opt.duration_ms, total);
  out.put (mode, threads, S::key (), operation_names[op_make_destroy], ns / base_ns, ns, total);
  ns = protect_reset<S> (threads, opt.duration_ms, total, false);
  out.put (mode, threads, S::key (), operation_names[op_protect_reset], ns / base_ns, ns, total);
  ns = protect_reset<S> (threads, opt.duration_ms, total, true);
  out.put (mode, threads, S::key (), operation_names[op_protect_kept], ns / base_ns, ns, total);
  ns = make_protect_destroy<S> (threads, opt.duration_ms, total);
  out.put (mode, threads, S::key (), operation_names[op_make_protect_destroy], ns / base_ns, ns, total);
  ns = retire_cost<S> (threads, opt.duration_ms, total);
  out.put (mode, threads, S::key (), operation_names[op_retire], ns / base_ns, ns, total);
}

template <class S>
void
measure_read_series (row_writer &out, char const *mode, int threads,
                     options const &opt, double base_ns)
{
  measurement total = { 0, 0, true };
  double ns = read_cost<S> (threads, opt.duration_ms, total);
  out.put (mode, threads, S::key (), operation_names[op_read], ns / base_ns, ns, total);
  if (threads >= 2)
    {
      ns = readers_writer<S> (threads, opt.duration_ms, total);
      out.put (mode, threads, S::key (), operation_names[op_readers_writer], ns / base_ns, ns, total);
    }
}

std::vector<int>
default_threads ()
{
  int const cpus = static_cast<int> (std::max (1u, std::thread::hardware_concurrency ()));
  std::vector<int> counts;
  counts.push_back (1);
  for (int t = 2; t <= cpus; t += 2)
    {
      counts.push_back (t);
    }
  if (counts.back () != cpus && cpus > 1)
    {
      counts.push_back (cpus);
    }
  return counts;
}

void
print_series ()
{
  std::vector<std::string> lines;
  has_holder<lumex_series>::list (lines);
  lines.push_back (std::string (unprotected_series::key ()) + "|" + unprotected_series::label () + "|" + unprotected_series::detail () + "|11|no protection");
  lines.push_back (std::string (shared_mutex_series::key ()) + "|" + shared_mutex_series::label () + "|" + shared_mutex_series::detail () + "|17|lock");
  lines.push_back (std::string (shared_ptr_series::key ()) + "|" + shared_ptr_series::label () + "|" + shared_ptr_series::detail () + "|11|lock-free or locked");
  lines.push_back ("memory|new + delete of a small object|the allocator baseline of retire|11|allocator");
#if defined(LUMEX_HP_BENCH_LIBCDS)
  has_holder<libcds_series>::list (lines);
#endif
#if defined(LUMEX_HP_BENCH_LLVM)
  has_holder<llvm_series>::list (lines);
#endif
  for (std::string const &line : lines)
    {
      std::printf ("%s\n", line.c_str ());
    }
}

int
run (options const &opt)
{
  std::FILE *file = std::fopen (opt.csv.c_str (), "w");
  if (file == nullptr)
    {
      std::fprintf (stderr, "cannot write %s\n", opt.csv.c_str ());
      return 2;
    }
  #if defined(__VERSION__)
  std::fprintf (file, "# compiler=%s\n", __VERSION__);
#else
  std::fprintf (file, "# compiler=unknown\n");
#endif
  std::fprintf (file, "# library=LumexLib hazard_pointer\n");
#if defined(NDEBUG)
  std::fprintf (file, "# build=Release (NDEBUG)\n");
#else
  std::fprintf (file, "# build=without NDEBUG\n");
#endif
  std::vector<std::string> listed;
  has_holder<lumex_series>::list (listed);
  listed.push_back (std::string (unprotected_series::key ()) + "|" + unprotected_series::label () + "|" + unprotected_series::detail () + "|11|no protection");
  listed.push_back (std::string (shared_mutex_series::key ()) + "|" + shared_mutex_series::label () + "|" + shared_mutex_series::detail () + "|17|lock");
  listed.push_back (std::string (shared_ptr_series::key ()) + "|" + shared_ptr_series::label () + "|" + shared_ptr_series::detail () + "|11|lock-free or locked");
  listed.push_back ("memory|new + delete of a small object|the allocator baseline of retire|11|allocator");
#if defined(LUMEX_HP_BENCH_LIBCDS)
  has_holder<libcds_series>::list (listed);
#endif
#if defined(LUMEX_HP_BENCH_LLVM)
  has_holder<llvm_series>::list (listed);
#endif
  for (std::string const &line : listed)
    {
      std::fprintf (file, "# implementation=%s\n", line.c_str ());
    }
  std::fprintf (file, "mode,threads,implementation,operation,repetition,ratio,ns_per_op,operations,successes,valid\n");
  row_writer out = { file, opt.repetition };

  // Warm up every core.
  {
    measurement ignored = { 0, 0, true };
    int const cpus = static_cast<int> (std::max (1u, std::thread::hardware_concurrency ()));
    baseline (cpus, opt.warmup_ms, ignored);
  }

  std::vector<int> threads = opt.threads.empty () ? default_threads () : opt.threads;
  bool all_valid = true;
  for (int count : threads)
    {
      char const *mode = count == 1 ? "uncontended" : "contended";
      // Series take turns; the first one differs from repetition to repetition.
      for (int turn = 0; turn < 4; ++turn)
        {
          int const which = (turn + opt.repetition) % 4;
          measurement total = { 0, 0, true };
          double const base_ns = baseline (count, opt.duration_ms, total);
          if (base_ns <= 0.0)
            {
              all_valid = false;
              continue;
            }
          // The baseline row goes under the key of every series of the block.
          auto base_row = [&] (char const *key) {
            out.put (mode, count, key, operation_names[op_baseline], 1.0,
                     base_ns, total);
          };
          switch (which)
            {
            case 0:
              base_row (lumex_series::key ());
              base_row ("memory");
              measure_hazard_series<lumex_series> (out, mode, count, opt, base_ns);
              {
                double ns = new_delete_cost (count, opt.duration_ms, total);
                out.put (mode, count, "memory", operation_names[op_new_delete], ns / base_ns, ns, total);
              }
              break;
            case 1:
              base_row (lumex_series::key ());
              measure_read_series<lumex_series> (out, mode, count, opt, base_ns);
              {
                double ns = list_walk_cost (count, opt.duration_ms, total, true);
                out.put (mode, count, lumex_series::key (), operation_names[op_list_walk], ns / base_ns, ns, total);
              }
              break;
            case 2:
              base_row (unprotected_series::key ());
              base_row (shared_mutex_series::key ());
              measure_read_series<unprotected_series> (out, mode, count, opt, base_ns);
              {
                double ns = list_walk_cost (count, opt.duration_ms, total, false);
                out.put (mode, count, unprotected_series::key (), operation_names[op_list_walk], ns / base_ns, ns, total);
              }
              measure_read_series<shared_mutex_series> (out, mode, count, opt, base_ns);
              break;
            default:
              base_row (shared_ptr_series::key ());
              measure_read_series<shared_ptr_series> (out, mode, count, opt, base_ns);
#if defined(LUMEX_HP_BENCH_LIBCDS)
              base_row (libcds_series::key ());
              measure_hazard_series<libcds_series> (out, mode, count, opt, base_ns);
              measure_read_series<libcds_series> (out, mode, count, opt, base_ns);
#endif
#if defined(LUMEX_HP_BENCH_LLVM)
              base_row (llvm_series::key ());
              measure_hazard_series<llvm_series> (out, mode, count, opt, base_ns);
              measure_read_series<llvm_series> (out, mode, count, opt, base_ns);
#endif
              break;
            }
        }
    }

  // Tail latency of retire, one thread.
  {
    measurement total = { 0, 0, true };
    measurement ignored = { 0, 0, true };
    double const base_ns = baseline (1, opt.duration_ms, ignored);
    double percentiles[4];
    retire_latency<lumex_series> (opt.duration_ms * 5, percentiles, total);
    operation_id const ids[4] = { op_retire_p50, op_retire_p99, op_retire_p999, op_retire_max };
    for (int i = 0; i < 4; ++i)
      {
        out.put ("uncontended", 1, lumex_series::key (), operation_names[ids[i]],
                 base_ns > 0.0 ? percentiles[i] / base_ns : 0.0, percentiles[i], total);
      }
  }
  std::fclose (file);
  return all_valid ? 0 : 3;
}
} // namespace

int
main (int argc, char **argv)
{
  options opt;
  opt.repetition = 0;
  opt.duration_ms = 100;
  opt.warmup_ms = 300;
  opt.list = false;
  opt.only_baseline_check = false;
  for (int i = 1; i < argc; ++i)
    {
      std::string const arg = argv[i];
      auto value = [&] () -> char const * {
        return i + 1 < argc ? argv[++i] : "";
      };
      if (arg == "--list")
        {
          opt.list = true;
        }
      else if (arg == "--csv")
        {
          opt.csv = value ();
        }
      else if (arg == "--repetition")
        {
          opt.repetition = std::atoi (value ());
        }
      else if (arg == "--duration-ms")
        {
          opt.duration_ms = std::atoi (value ());
        }
      else if (arg == "--warmup-ms")
        {
          opt.warmup_ms = std::atoi (value ());
        }
      else if (arg == "--threads")
        {
          std::string list = value ();
          std::size_t at = 0;
          while (at < list.size ())
            {
              std::size_t comma = list.find (',', at);
              if (comma == std::string::npos)
                {
                  comma = list.size ();
                }
              opt.threads.push_back (std::atoi (list.substr (at, comma - at).c_str ()));
              at = comma + 1;
            }
        }
      else
        {
          std::fprintf (stderr, "unknown argument %s\n", arg.c_str ());
          return 2;
        }
    }
  if (opt.list)
    {
      print_series ();
      return 0;
    }
  if (opt.csv.empty ())
    {
      std::fprintf (stderr, "usage: %s --csv <file> [--repetition n] [--duration-ms n] [--warmup-ms n] [--threads 1,2,4] | --list\n", argv[0]);
      return 2;
    }
  return run (opt);
}

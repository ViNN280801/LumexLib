// bench_circular_buffer.cpp
// One source, five bounded sequences: the scenarios below are compiled against
// circular_buffer of this library (BENCH_RING_IMPL=1), boost::circular_buffer
// (2), std::deque used as a bounded queue (3), std::vector with manual index
// math (4) and the same with `%` for the wrap (5); every build is its own
// executable (see bench_circular_buffer_impl.hpp and CMakeLists.txt). The
// numbers are nanoseconds per operation, an operation being what the
// description of the scenario says (one push, one pass over `size` elements,
// ...).
//
// Equal work: before timing, every scenario is run for a few iterations and
// its checksum is compared with the same scenario run on a reference
// container (a std::vector that erases its first element when full, so
// obviously correct); the checksums are written to the CSV so that
// run_benchmark.py can compare them across the executables (a scenario with
// different checksums stops the run). The compiler is kept from deleting or
// hoisting the work with empty asm statements that read the objects and
// clobber memory.
//
// Usage: LumexCircularBufferBench_<impl>_cxx<std> [output.csv] [--quick]
//        [--filter text] [--list] [--traits footprint.csv]
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bench_circular_buffer_impl.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BENCH_ALWAYS_INLINE inline __attribute__ ((always_inline))
#elif defined(_MSC_VER)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_ALWAYS_INLINE inline
#endif

// -- Allocation counter (used by --traits only; one branch per allocation in
// every variant alike) --

namespace
{
bool g_count_allocations = false;
std::size_t g_allocation_bytes = 0;
std::size_t g_allocation_count = 0;

void *
counted_allocate (std::size_t size)
{
  if (g_count_allocations)
    {
      g_allocation_bytes += size;
      ++g_allocation_count;
    }
  void *const memory = std::malloc (size == 0 ? 1 : size);
  if (memory == nullptr)
    throw std::bad_alloc ();
  return memory;
}
} // namespace

void *
operator new (std::size_t size)
{
  return counted_allocate (size);
}
void *
operator new[] (std::size_t size)
{
  return counted_allocate (size);
}
void
operator delete (void *memory) noexcept
{
  std::free (memory);
}
void
operator delete[] (void *memory) noexcept
{
  std::free (memory);
}
void
operator delete (void *memory, std::size_t) noexcept
{
  std::free (memory);
}
void
operator delete[] (void *memory, std::size_t) noexcept
{
  std::free (memory);
}

namespace
{
using u64 = std::uint64_t;

// -- Optimizer barriers --

#if defined(__GNUC__) || defined(__clang__)
/** The object is read from memory here, and memory may change after it. */
template <typename T>
BENCH_ALWAYS_INLINE void
keep_object (T const &object)
{
  asm volatile ("" : : "m"(object) : "memory");
}

/** The scalar is used here, in a register or in memory. */
template <typename T>
BENCH_ALWAYS_INLINE void
keep_value (T const &value)
{
  asm volatile ("" : : "r,m"(value) : "memory");
}
#else
std::uint64_t volatile g_barrier = 0;

template <typename T>
BENCH_ALWAYS_INLINE void
keep_object (T const &object)
{
  _ReadWriteBarrier ();
  g_barrier = g_barrier
              + static_cast<std::uint64_t> (
                  reinterpret_cast<std::uintptr_t> (&object));
}

template <typename T>
BENCH_ALWAYS_INLINE void
keep_value (T const &value)
{
  _ReadWriteBarrier ();
  g_barrier = g_barrier + static_cast<std::uint64_t> (value);
}
#endif

// -- The reference container: obviously correct, never timed --

template <typename T> class model_ring
{
public:
  typedef typename std::vector<T>::iterator iterator;

  explicit model_ring (std::size_t capacity) : m_capacity (capacity) {}
  std::size_t
  size () const
  {
    return m_items.size ();
  }
  std::size_t
  capacity () const
  {
    return m_capacity;
  }
  void
  clear ()
  {
    m_items.clear ();
  }
  void
  push_back (T const &value)
  {
    if (m_items.size () == m_capacity)
      m_items.erase (m_items.begin ());
    m_items.push_back (value);
  }
  void
  pop_front ()
  {
    m_items.erase (m_items.begin ());
  }
  T &
  front ()
  {
    return m_items.front ();
  }
  T &
  back ()
  {
    return m_items.back ();
  }
  T &
  operator[] (std::size_t index)
  {
    return m_items[index];
  }
  iterator
  begin ()
  {
    return m_items.begin ();
  }
  iterator
  end ()
  {
    return m_items.end ();
  }

private:
  std::vector<T> m_items;
  std::size_t m_capacity;
};

// -- The element kinds --

struct pod32
{
  u64 a;
  u64 b;
  u64 c;
  u64 d;
};

/** A kind: a make function (the i-th sample) and a digest of a value. */
struct kind_int
{
  typedef int type;
  static char const *
  tag ()
  {
    return "int";
  }
  static type
  make (std::size_t i)
  {
    return static_cast<int> (i * 7u + 3u);
  }
  static u64
  digest (type const &v)
  {
    return static_cast<u64> (static_cast<std::int64_t> (v));
  }
};

struct kind_str_short
{
  typedef std::string type;
  static char const *
  tag ()
  {
    return "str_short";
  }
  static type
  make (std::size_t i)
  {
    return "s-" + std::to_string (i % 1000u);
  }
  static u64
  digest (type const &v)
  {
    return v.size () * 131u + static_cast<unsigned char> (v[v.size () - 1]);
  }
};

struct kind_str_long
{
  typedef std::string type;
  static char const *
  tag ()
  {
    return "str_long";
  }
  static type
  make (std::size_t i)
  {
    return std::string (70, 'x') + std::to_string (i % 1000u);
  }
  static u64
  digest (type const &v)
  {
    return v.size () * 131u + static_cast<unsigned char> (v[v.size () - 1]);
  }
};

constexpr std::size_t k_pool = 1024; // a power of two

/** The samples of a kind, made once in prepare_data. */
template <typename K> struct pool
{
  static std::vector<typename K::type> v;
  static typename K::type const &
  at (std::size_t i)
  {
    return v[i & (k_pool - 1)];
  }
};
template <typename K> std::vector<typename K::type> pool<K>::v;

template <typename K>
void
prepare_kind ()
{
  for (std::size_t i = 0; i < k_pool; ++i)
    pool<K>::v.push_back (K::make (i));
}

void
prepare_data ()
{
  prepare_kind<kind_int> ();
  prepare_kind<kind_str_short> ();
  prepare_kind<kind_str_long> ();
}

template <typename K> using ring_of = bench::ring<typename K::type>;
template <typename K> using model_of = model_ring<typename K::type>;

/** A buffer of `capacity` whose head is not at the start of the storage:
 * capacity * 3 / 2 pushes (the first half is overwritten), then a quarter
 * popped, so that it holds 3/4 capacity elements across the wrap point. */
template <typename K, typename C>
C
make_wrapped (std::size_t capacity, std::size_t first_sample)
{
  C buf (capacity);
  for (std::size_t j = 0; j < capacity + capacity / 2; ++j)
    buf.push_back (pool<K>::at (first_sample + j));
  for (std::size_t j = 0; j < capacity / 4; ++j)
    buf.pop_front ();
  return buf;
}

// -- Scenarios: run (iterations, size) on the container under test and on the
// reference --

typedef u64 (*run_t) (std::size_t iterations, std::size_t size);

struct scenario_t
{
  std::string name;
  char const *description;
  std::size_t size;
  run_t run;
  run_t reference;
};

/** push_back into a full buffer: overwrite (or drop the oldest). */
template <typename K, typename C>
u64
run_push_overwrite (std::size_t iterations, std::size_t size)
{
  C buf (size);
  for (std::size_t j = 0; j < size; ++j)
    buf.push_back (pool<K>::at (j));
  for (std::size_t i = 0; i < iterations; ++i)
    {
      buf.push_back (pool<K>::at (i));
      keep_object (buf);
    }
  return K::digest (buf.front ()) + K::digest (buf.back ()) + buf.size ();
}

/** The steady state of a queue: push_back, read front, pop_front. */
template <typename K, typename C>
u64
run_push_pop_steady (std::size_t iterations, std::size_t size)
{
  C buf (size);
  for (std::size_t j = 0; j < size / 2; ++j)
    buf.push_back (pool<K>::at (j));
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      buf.push_back (pool<K>::at (i));
      acc += K::digest (buf.front ());
      buf.pop_front ();
      keep_object (buf);
    }
  return acc + buf.size ();
}

/** clear, then `size` push_back into a buffer that never gets full. */
template <typename K, typename C>
u64
run_fill (std::size_t iterations, std::size_t size)
{
  C buf (size);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      buf.clear ();
      for (std::size_t j = 0; j < size; ++j)
        buf.push_back (pool<K>::at (j));
      keep_object (buf);
      acc += K::digest (buf.back ());
    }
  return acc + buf.size ();
}

/** fill (as above), then read front and pop_front until empty. */
template <typename K, typename C>
u64
run_fill_drain (std::size_t iterations, std::size_t size)
{
  C buf (size);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      buf.clear ();
      for (std::size_t j = 0; j < size; ++j)
        buf.push_back (pool<K>::at (j));
      keep_object (buf);
      while (buf.size () != 0)
        {
          acc += K::digest (buf.front ());
          buf.pop_front ();
        }
    }
  return acc;
}

/** front () and back () of a buffer whose head is in the middle. */
template <typename K, typename C>
u64
run_front_back (std::size_t iterations, std::size_t size)
{
  C buf = make_wrapped<K, C> (size, 0);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      acc += K::digest (buf.front ()) + K::digest (buf.back ());
      keep_object (buf);
    }
  return acc;
}

/** Sum over a wrapped buffer by operator[]. */
template <typename K, typename C>
u64
run_index_sum (std::size_t iterations, std::size_t size)
{
  C buf = make_wrapped<K, C> (size, 0);
  std::size_t const count = buf.size ();
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      for (std::size_t k = 0; k < count; ++k)
        acc += K::digest (buf[k]);
      keep_object (buf);
    }
  return acc;
}

/** Sum over a wrapped buffer by iterators. */
template <typename K, typename C>
u64
run_iterate_sum (std::size_t iterations, std::size_t size)
{
  C buf = make_wrapped<K, C> (size, 0);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      typename C::iterator const last = buf.end ();
      for (typename C::iterator it = buf.begin (); it != last; ++it)
        acc += K::digest (*it);
      keep_object (buf);
    }
  return acc;
}

/** One thread playing producer and consumer: per step a burst of 1 to 8
 * push_back, then up to 7 reads of front () with pop_front (when not empty);
 * it produces more than it consumes, so the buffer fills up and overwrites.
 * One operation is a pass of 1 024 steps. */
template <typename K, typename C>
u64
run_producer_consumer (std::size_t iterations, std::size_t)
{
  C buf (256);
  std::uint32_t state = 2463534242u;
  std::size_t next = 0;
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      for (std::size_t step = 0; step < 1024; ++step)
        {
          state = state * 1664525u + 1013904223u;
          std::size_t const produce = 1 + (state >> 16) % 8u;
          std::size_t const consume = (state >> 8) % 8u;
          for (std::size_t j = 0; j < produce; ++j)
            buf.push_back (pool<K>::at (next++));
          for (std::size_t j = 0; j < consume; ++j)
            {
              if (buf.size () == 0)
                break;
              acc += K::digest (buf.front ());
              buf.pop_front ();
            }
        }
      keep_object (buf);
    }
  return acc + buf.size ();
}

/** Construct an empty buffer of `size` elements and destroy it. */
template <typename K, typename C>
u64
run_ctor_dtor (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      C buf (size);
      keep_object (buf);
      acc += buf.capacity () + buf.size ();
    }
  return acc;
}

/** Copy construction of a wrapped buffer of 3/4 `size` elements. */
template <typename K, typename C>
u64
run_copy_ctor (std::size_t iterations, std::size_t size)
{
  C const source = make_wrapped<K, C> (size, 0);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      C copy (source);
      keep_object (copy);
      acc += K::digest (copy.back ()) + copy.size ();
    }
  return acc;
}

/** Copy assignment into a buffer of the same capacity with other content. */
template <typename K, typename C>
u64
run_copy_assign (std::size_t iterations, std::size_t size)
{
  C const source = make_wrapped<K, C> (size, 0);
  C target = make_wrapped<K, C> (size, 500);
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      target = source;
      keep_object (target);
      acc += K::digest (target.back ()) + target.size ();
    }
  return acc;
}

/** One move construction and one move assignment back, of a buffer of 3/4
 * `size` elements. */
template <typename K, typename C>
u64
run_move_pair (std::size_t iterations, std::size_t size)
{
  C a = make_wrapped<K, C> (size, 0);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      C b (std::move (a));
      keep_object (b);
      a = std::move (b);
      keep_object (a);
    }
  return K::digest (a.back ()) + a.size ();
}

#define BENCH_ENTRY(list, function, K, size_value, text)                      \
  (list).push_back (scenario_t{                                               \
      std::string (#function).substr (4) + "_" + K::tag (), text, size_value, \
      &function<K, ring_of<K>>, &function<K, model_of<K>> })

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> s;
  BENCH_ENTRY (s, run_push_overwrite, kind_int, 1024,
               "one push_back into a full buffer (overwrite the oldest)");
  BENCH_ENTRY (s, run_push_overwrite, kind_str_short, 1024,
               "one push_back into a full buffer (overwrite the oldest)");
  BENCH_ENTRY (s, run_push_overwrite, kind_str_long, 1024,
               "one push_back into a full buffer (overwrite the oldest)");
  BENCH_ENTRY (s, run_push_pop_steady, kind_int, 1024,
               "push_back, read front, pop_front on a half-full buffer");
  BENCH_ENTRY (s, run_push_pop_steady, kind_str_short, 1024,
               "push_back, read front, pop_front on a half-full buffer");
  BENCH_ENTRY (s, run_push_pop_steady, kind_str_long, 1024,
               "push_back, read front, pop_front on a half-full buffer");
  BENCH_ENTRY (s, run_fill, kind_int, 1024,
               "clear, then push_back of `size` elements (never full)");
  BENCH_ENTRY (s, run_fill, kind_str_short, 256,
               "clear, then push_back of `size` elements (never full)");
  BENCH_ENTRY (s, run_fill, kind_str_long, 256,
               "clear, then push_back of `size` elements (never full)");
  BENCH_ENTRY (
      s, run_fill_drain, kind_int, 1024,
      "fill with `size` elements, then front and pop_front until empty");
  BENCH_ENTRY (
      s, run_fill_drain, kind_str_short, 256,
      "fill with `size` elements, then front and pop_front until empty");
  BENCH_ENTRY (s, run_front_back, kind_int, 1024,
               "front () and back () of a wrapped buffer");
  BENCH_ENTRY (s, run_front_back, kind_str_short, 1024,
               "front () and back () of a wrapped buffer");
  BENCH_ENTRY (s, run_index_sum, kind_int, 1024,
               "sum of the 768 elements of a wrapped buffer by operator[]");
  BENCH_ENTRY (s, run_index_sum, kind_str_short, 1024,
               "sum of the 768 elements of a wrapped buffer by operator[]");
  BENCH_ENTRY (s, run_iterate_sum, kind_int, 1024,
               "sum of the 768 elements of a wrapped buffer by iterators");
  BENCH_ENTRY (s, run_iterate_sum, kind_str_short, 1024,
               "sum of the 768 elements of a wrapped buffer by iterators");
  BENCH_ENTRY (
      s, run_producer_consumer, kind_int, 1024,
      "producer and consumer on one thread, 1 024 steps, capacity 256");
  BENCH_ENTRY (
      s, run_producer_consumer, kind_str_short, 1024,
      "producer and consumer on one thread, 1 024 steps, capacity 256");
  BENCH_ENTRY (s, run_ctor_dtor, kind_int, 1024,
               "construct an empty buffer of capacity `size` and destroy it");
  BENCH_ENTRY (s, run_ctor_dtor, kind_str_short, 1024,
               "construct an empty buffer of capacity `size` and destroy it");
  BENCH_ENTRY (s, run_copy_ctor, kind_int, 1024,
               "copy construction of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_copy_ctor, kind_str_short, 256,
               "copy construction of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_copy_ctor, kind_str_long, 128,
               "copy construction of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_copy_assign, kind_int, 1024,
               "copy assignment of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_copy_assign, kind_str_short, 256,
               "copy assignment of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_copy_assign, kind_str_long, 128,
               "copy assignment of a wrapped buffer of 3/4 `size` elements");
  BENCH_ENTRY (s, run_move_pair, kind_int, 1024,
               "move construction and move assignment back of a buffer");
  BENCH_ENTRY (s, run_move_pair, kind_str_short, 256,
               "move construction and move assignment back of a buffer");
  return s;
}

// -- Footprint and traits (--traits) --

template <typename T>
void
footprint_row (std::ofstream &out, char const *name)
{
  typedef bench::ring<T> ring_t;
  std::size_t const capacity = 1024;
  T const sample = T ();
  g_count_allocations = true;
  g_allocation_bytes = 0;
  g_allocation_count = 0;
  ring_t *ring = new ring_t (capacity);
  std::size_t const empty_bytes = g_allocation_bytes - sizeof (ring_t);
  std::size_t const empty_allocs = g_allocation_count - 1;
  for (std::size_t i = 0; i < 10; ++i)
    ring->push_back (sample);
  std::size_t const ten_bytes = g_allocation_bytes - sizeof (ring_t);
  std::size_t const ten_allocs = g_allocation_count - 1;
  for (std::size_t i = 10; i < capacity; ++i)
    ring->push_back (sample);
  std::size_t const full_bytes = g_allocation_bytes - sizeof (ring_t);
  std::size_t const full_allocs = g_allocation_count - 1;
  g_count_allocations = false;
  delete ring;
  out << name << ',' << sizeof (T) << ',' << sizeof (ring_t) << ','
      << empty_bytes << ',' << empty_allocs << ',' << ten_bytes << ','
      << ten_allocs << ',' << full_bytes << ',' << full_allocs << ','
      << (std::is_nothrow_move_constructible<ring_t>::value ? 1 : 0) << ','
      << (std::is_nothrow_move_assignable<ring_t>::value ? 1 : 0) << ','
      << (std::is_copy_constructible<ring_t>::value ? 1 : 0) << '\n';
}

void
write_footprint (std::string const &path)
{
  std::ofstream out (path.c_str ());
  out << "type,sizeof_t,sizeof_ring,empty_bytes,empty_allocs,ten_bytes,"
         "ten_allocs,full_bytes,full_allocs,nothrow_move_constructible,"
         "nothrow_move_assignable,copy_constructible\n";
  footprint_row<char> (out, "char");
  footprint_row<int> (out, "int");
  footprint_row<double> (out, "double");
  footprint_row<pod32> (out, "pod32");
  footprint_row<std::string> (out, "std::string");
}

// -- Harness --

struct result_t
{
  double median_ns;
  double min_ns;
  double max_ns;
  std::size_t iterations;
  int repetitions;
  std::uint64_t checksum;
};

double
seconds_of (scenario_t const &scenario, std::size_t iterations)
{
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  std::uint64_t const checksum = scenario.run (iterations, scenario.size);
  std::chrono::duration<double> const elapsed
      = std::chrono::steady_clock::now () - start;
  keep_value (checksum);
  return elapsed.count ();
}

result_t
measure (scenario_t const &scenario, double target_seconds, int repetitions)
{
  // Calibrate: double the iteration count until one run lasts long enough.
  std::size_t iterations = 1;
  while (seconds_of (scenario, iterations) < target_seconds
         && iterations < (std::size_t (1) << 40))
    iterations *= 2;
  // Warm-up run with the chosen count.
  seconds_of (scenario, iterations);

  std::vector<double> samples;
  for (int r = 0; r < repetitions; ++r)
    samples.push_back (seconds_of (scenario, iterations) * 1e9
                       / static_cast<double> (iterations));
  std::sort (samples.begin (), samples.end ());
  result_t result;
  result.median_ns = samples[samples.size () / 2];
  result.min_ns = samples.front ();
  result.max_ns = samples.back ();
  result.iterations = iterations;
  result.repetitions = repetitions;
  result.checksum = scenario.run (8, scenario.size);
  return result;
}

bool
check_against_reference (scenario_t const &scenario)
{
  std::uint64_t const got = scenario.run (8, scenario.size);
  std::uint64_t const want = scenario.reference (8, scenario.size);
  if (got != want)
    {
      std::cerr << scenario.name << " (size " << scenario.size
                << "): checksum " << got << ", the reference container "
                << want << '\n';
      return false;
    }
  return true;
}

std::string
csv_quoted (std::string const &text)
{
  std::string result;
  for (char const c : text)
    {
      if (c == '"')
        result += '"';
      result += c;
    }
  return result;
}

std::string
compiler_name ()
{
  char buffer[64];
#if defined(__clang__)
  std::snprintf (buffer, sizeof (buffer), "Clang %d.%d.%d", __clang_major__,
                 __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
  std::snprintf (buffer, sizeof (buffer), "MSVC %d", _MSC_VER);
#elif defined(__GNUC__)
  std::snprintf (buffer, sizeof (buffer), "GCC %d.%d.%d", __GNUC__,
                 __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
  std::snprintf (buffer, sizeof (buffer), "unknown");
#endif
  return buffer;
}

std::string
library_name ()
{
#if defined(_LIBCPP_VERSION)
  return "libc++";
#elif defined(__GLIBCXX__)
  return "libstdc++";
#elif defined(_MSVC_STL_VERSION)
  return "MSVC STL";
#else
  return "unknown";
#endif
}
} // namespace

int
main (int argc, char **argv)
{
  std::string output = "circular_buffer_benchmark.csv";
  std::string filter;
  std::string traits;
  bool quick = false;
  bool list = false;
  for (int i = 1; i < argc; ++i)
    {
      if (std::strcmp (argv[i], "--quick") == 0)
        quick = true;
      else if (std::strcmp (argv[i], "--list") == 0)
        list = true;
      else if (std::strcmp (argv[i], "--filter") == 0 && i + 1 < argc)
        filter = argv[++i];
      else if (std::strcmp (argv[i], "--traits") == 0 && i + 1 < argc)
        traits = argv[++i];
      else
        output = argv[i];
    }
  if (!traits.empty ())
    {
      write_footprint (traits);
      return 0;
    }
  prepare_data ();

  std::vector<scenario_t> scenarios;
  for (scenario_t const &scenario : make_scenarios ())
    {
      if (!filter.empty () && scenario.name.find (filter) == std::string::npos)
        continue;
      scenarios.push_back (scenario);
    }
  if (list)
    {
      for (scenario_t const &scenario : scenarios)
        std::cout << scenario.name << ' ' << scenario.size << '\n';
      return 0;
    }

  bool same = true;
  for (scenario_t const &scenario : scenarios)
    same = check_against_reference (scenario) && same;
  if (!same)
    {
      std::cerr << "a scenario disagrees with the reference container; "
                   "the numbers would compare different work\n";
      return 1;
    }

  double const target_seconds = quick ? 0.002 : 0.02;
  int const repetitions = quick ? 3 : 15;
  std::ofstream csv (output.c_str ());
  csv << "# impl=" << BENCH_RING_IMPL_NAME << " compiler=" << compiler_name ()
      << " library=" << library_name () << " cplusplus=" << __cplusplus
#if defined(NDEBUG)
      << " build=release"
#else
      << " build=debug"
#endif
      << '\n';
  csv << "scenario,description,size,median_ns,min_ns,max_ns,iterations,"
         "repetitions,checksum\n";
  for (scenario_t const &scenario : scenarios)
    {
      result_t const result = measure (scenario, target_seconds, repetitions);
      csv << scenario.name << ",\"" << csv_quoted (scenario.description)
          << "\"," << scenario.size << ',' << result.median_ns << ','
          << result.min_ns << ',' << result.max_ns << ',' << result.iterations
          << ',' << result.repetitions << ',' << result.checksum << '\n';
      std::printf ("%-32s %8zu %10.3f ns/op (min %.3f, max %.3f)\n",
                   scenario.name.c_str (), scenario.size, result.median_ns,
                   result.min_ns, result.max_ns);
    }
  std::cout << "written " << output << '\n';
  return 0;
}

// bench_span.cpp
// One source, three spans: the scenarios below are compiled against the span
// of this library (BENCH_SPAN_IMPL=1), the std::span of the standard library
// in use (2) or boost::span of Boost.Core (3), and every build is its own
// executable (see bench_span_impl.hpp and CMakeLists.txt). The numbers are
// nanoseconds per operation, an operation being what the description of the
// scenario says (one construction, one pass over `size` elements, ...).
//
// Equal work: before timing, every scenario is run for a few iterations and
// its checksum is compared with a reference that uses raw pointers only; the
// checksums are written to the CSV so that run_benchmark.py can compare them
// across the executables (a scenario with different checksums is dropped).
// The compiler is kept from deleting or hoisting the work with empty asm
// statements that read the spans and clobber memory.
//
// Usage: LumexSpanBench_<impl>_cxx<std> [output.csv] [--quick] [--filter text]
//        [--list]
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bench_span_impl.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BENCH_NOINLINE __attribute__ ((noinline))
#define BENCH_ALWAYS_INLINE inline __attribute__ ((always_inline))
#elif defined(_MSC_VER)
#define BENCH_NOINLINE __declspec (noinline)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_NOINLINE
#define BENCH_ALWAYS_INLINE inline
#endif

namespace
{
using bench::span;

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

/** The compiler loses what it knows about the value. */
template <typename T>
BENCH_ALWAYS_INLINE T
hide (T value)
{
  asm volatile ("" : "+r"(value));
  return value;
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

template <typename T>
BENCH_ALWAYS_INLINE T
hide (T value)
{
  T volatile copy = value;
  _ReadWriteBarrier ();
  return copy;
}
#endif

// -- Data --

constexpr std::size_t k_max_size = 1000000;
constexpr std::size_t k_small = 16;

std::vector<int> g_data;
int g_small_array[k_small];
std::array<int, k_small> g_small_std_array;
std::vector<int> g_small_vector;

void
prepare_data ()
{
  g_data.resize (k_max_size);
  for (std::size_t i = 0; i < g_data.size (); ++i)
    g_data[i] = static_cast<int> ((i * 37u + 11u) % 251u);
  for (std::size_t i = 0; i < k_small; ++i)
    {
      g_small_array[i] = static_cast<int> (i * 3 + 1);
      g_small_std_array[i] = static_cast<int> (i * 3 + 1);
    }
  g_small_vector.assign (g_small_array, g_small_array + k_small);
}

int *
data_base ()
{
  return hide (g_data.data ());
}

// -- Functions the scenarios call --

BENCH_NOINLINE std::uint64_t
consume_noinline (span<int> view)
{
  return view.size () + (view.empty () ? 0u : static_cast<unsigned> (view[0]));
}

BENCH_ALWAYS_INLINE std::uint64_t
consume_inline (span<int> view)
{
  return view.size () + (view.empty () ? 0u : static_cast<unsigned> (view[0]));
}

BENCH_NOINLINE std::uint64_t
consume_static_noinline (span<int, k_small> view)
{
  return view.size () + static_cast<unsigned> (view[3]);
}

BENCH_NOINLINE std::uint64_t
divide_and_conquer (span<int const> view)
{
  if (view.size () <= 1)
    return view.empty () ? 0u : static_cast<unsigned> (view[0]);
  std::size_t const half = view.size () / 2;
  return divide_and_conquer (view.first (half))
         + divide_and_conquer (view.subspan (half));
}

std::uint64_t
divide_and_conquer_reference (int const *begin, std::size_t size)
{
  if (size <= 1)
    return size == 0 ? 0u : static_cast<unsigned> (begin[0]);
  std::size_t const half = size / 2;
  return divide_and_conquer_reference (begin, half)
         + divide_and_conquer_reference (begin + half, size - half);
}

// -- Scenarios: run (iterations, size) and the reference with raw pointers --

typedef std::uint64_t (*run_t) (std::size_t iterations, std::size_t size);

struct scenario_t
{
  char const *name;
  char const *description;
  std::size_t size;
  run_t run;
  run_t reference;
  bool needs_as_bytes;
  bool needs_std_conversion;
};

// construction

std::uint64_t
run_ctor_ptr_size (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  int *const base = data_base ();
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int> const view (hide (base), hide (size));
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
ref_ctor_ptr_size (std::size_t iterations, std::size_t size)
{
  return iterations * size;
}

std::uint64_t
run_ctor_c_array (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int> const view (g_small_array);
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
ref_ctor_small (std::size_t iterations, std::size_t)
{
  return iterations * k_small;
}

std::uint64_t
run_ctor_std_array (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int> const view (g_small_std_array);
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
run_ctor_vector (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  std::vector<int> &source = *hide (&g_small_vector);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int> const view (source);
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
run_ctor_static_array (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int, k_small> const view (g_small_array);
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
run_copy (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const original (data_base (), hide (size));
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (original);
      span<int> const copy (original);
      keep_object (copy);
      acc += copy.size ();
    }
  return acc;
}

// iteration

std::uint64_t
reference_sum (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    for (std::size_t j = 0; j < size; ++j)
      acc += static_cast<unsigned> (g_data[j]);
  return acc;
}

std::uint64_t
run_sum_range_for (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int const> const view (data_base (), hide (size));
      keep_object (view);
      for (int const element : view)
        acc += static_cast<unsigned> (element);
    }
  return acc;
}

std::uint64_t
run_sum_index (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int const> const view (data_base (), hide (size));
      keep_object (view);
      for (std::size_t j = 0; j < view.size (); ++j)
        acc += static_cast<unsigned> (view[j]);
    }
  return acc;
}

std::uint64_t
run_sum_iterators (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int const> const view (data_base (), hide (size));
      keep_object (view);
      for (span<int const>::iterator it = view.begin (); it != view.end ();
           ++it)
        acc += static_cast<unsigned> (*it);
    }
  return acc;
}

std::uint64_t
run_sum_static_range_for (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int const, k_small> const view (g_small_array);
      keep_object (view);
      for (int const element : view)
        acc += static_cast<unsigned> (element);
    }
  return acc;
}

std::uint64_t
ref_sum_small (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    for (std::size_t j = 0; j < k_small; ++j)
      acc += static_cast<unsigned> (g_small_array[j]);
  return acc;
}

// dynamic subviews

std::uint64_t
run_first (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int> const part = view.first (hide (size) / 2);
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[0]);
    }
  return acc;
}

std::uint64_t
ref_first (std::size_t iterations, std::size_t size)
{
  return iterations * (size / 2 + static_cast<unsigned> (g_data[0]));
}

std::uint64_t
run_last (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int> const part = view.last (hide (size) / 2);
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[0]);
    }
  return acc;
}

std::uint64_t
ref_last (std::size_t iterations, std::size_t size)
{
  return iterations
         * (size / 2 + static_cast<unsigned> (g_data[size - size / 2]));
}

std::uint64_t
run_subspan (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int> const part = view.subspan (hide (size) / 4, size / 2);
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[0]);
    }
  return acc;
}

std::uint64_t
ref_subspan (std::size_t iterations, std::size_t size)
{
  return iterations * (size / 2 + static_cast<unsigned> (g_data[size / 4]));
}

std::uint64_t
run_subspan_rest (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int> const part = view.subspan (hide (size) / 4);
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[0]);
    }
  return acc;
}

std::uint64_t
ref_subspan_rest (std::size_t iterations, std::size_t size)
{
  return iterations
         * (size - size / 4 + static_cast<unsigned> (g_data[size / 4]));
}

// static subviews

std::uint64_t
run_static_first (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  span<int, k_small> const view (g_small_array);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int, 4> const part = view.first<4> ();
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[1]);
    }
  return acc;
}

std::uint64_t
ref_static_first (std::size_t iterations, std::size_t)
{
  return iterations * (4 + static_cast<unsigned> (g_small_array[1]));
}

std::uint64_t
run_static_subspan (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  span<int, k_small> const view (g_small_array);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int, 4> const part = view.subspan<2, 4> ();
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[1]);
    }
  return acc;
}

std::uint64_t
ref_static_subspan (std::size_t iterations, std::size_t)
{
  return iterations * (4 + static_cast<unsigned> (g_small_array[3]));
}

std::uint64_t
run_static_first_of_dynamic (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int, 4> const part = view.first<4> ();
      keep_object (part);
      acc += part.size () + static_cast<unsigned> (part[1]);
    }
  return acc;
}

std::uint64_t
ref_static_first_of_dynamic (std::size_t iterations, std::size_t)
{
  return iterations * (4 + static_cast<unsigned> (g_data[1]));
}

// conversions

std::uint64_t
run_static_to_dynamic (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  span<int, k_small> const fixed (g_small_array);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (fixed);
      span<int> const dynamic = fixed;
      keep_object (dynamic);
      acc += dynamic.size ();
    }
  return acc;
}

std::uint64_t
run_dynamic_to_static (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  span<int> const dynamic (g_small_array, hide (k_small));
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (dynamic);
      span<int, k_small> const fixed (dynamic);
      keep_object (fixed);
      acc += fixed.size ();
    }
  return acc;
}

std::uint64_t
run_mutable_to_const (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      span<int const> const read_only = view;
      keep_object (read_only);
      acc += read_only.size ();
    }
  return acc;
}

// conversions to and from std::span (the span of this library at C++20 only)

#if BENCH_SPAN_HAS_STD_CONVERSION
std::uint64_t
run_from_std_span (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  std::span<int> const standard (data_base (), hide (size));
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (standard);
      span<int> const view = standard;
      keep_object (view);
      acc += view.size ();
    }
  return acc;
}

std::uint64_t
run_to_std_span (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), hide (size));
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      std::span<int> const standard = view;
      keep_object (standard);
      acc += standard.size ();
    }
  return acc;
}

std::uint64_t
run_bytes_to_std_span (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int const> const view (data_base (), hide (size));
  bench::byte_span const bytes = bench::as_bytes (view);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (bytes);
      std::span<std::byte const> const standard = bytes;
      keep_object (standard);
      acc += standard.size ();
    }
  return acc;
}

std::uint64_t
ref_bytes_to_std_span (std::size_t iterations, std::size_t size)
{
  return iterations * size * sizeof (int);
}
#else
std::uint64_t
run_from_std_span (std::size_t, std::size_t)
{
  return 0;
}

std::uint64_t
run_to_std_span (std::size_t, std::size_t)
{
  return 0;
}

std::uint64_t
run_bytes_to_std_span (std::size_t, std::size_t)
{
  return 0;
}

std::uint64_t
ref_bytes_to_std_span (std::size_t, std::size_t)
{
  return 0;
}
#endif

// passing

std::uint64_t
run_pass_noinline (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      acc += consume_noinline (view);
    }
  return acc;
}

std::uint64_t
run_pass_inline (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      acc += consume_inline (view);
    }
  return acc;
}

std::uint64_t
run_pass_static_noinline (std::size_t iterations, std::size_t)
{
  std::uint64_t acc = 0;
  span<int, k_small> const view (g_small_array);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      acc += consume_static_noinline (view);
    }
  return acc;
}

std::uint64_t
ref_pass (std::size_t iterations, std::size_t size)
{
  return iterations * (size + static_cast<unsigned> (g_data[0]));
}

std::uint64_t
ref_pass_static (std::size_t iterations, std::size_t)
{
  return iterations * (k_small + static_cast<unsigned> (g_small_array[3]));
}

// divide and conquer

std::uint64_t
run_divide_and_conquer (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    acc += divide_and_conquer (span<int const> (data_base (), hide (size)));
  return acc;
}

std::uint64_t
ref_divide_and_conquer (std::size_t iterations, std::size_t size)
{
  return iterations * divide_and_conquer_reference (g_data.data (), size);
}

// bytes

#if BENCH_SPAN_HAS_AS_BYTES
std::uint64_t
run_bytes_sum (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      span<int const> const view (data_base (), hide (size));
      keep_object (view);
      for (bench::byte const element : bench::as_bytes (view))
        acc += static_cast<unsigned char> (element);
    }
  return acc;
}

std::uint64_t
ref_bytes_sum (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  unsigned char const *bytes
      = reinterpret_cast<unsigned char const *> (g_data.data ());
  for (std::size_t i = 0; i < iterations; ++i)
    for (std::size_t j = 0; j < size * sizeof (int); ++j)
      acc += bytes[j];
  return acc;
}
#else
std::uint64_t
run_bytes_sum (std::size_t, std::size_t)
{
  return 0;
}

std::uint64_t
ref_bytes_sum (std::size_t, std::size_t)
{
  return 0;
}
#endif

std::uint64_t
run_size_bytes (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int const> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      acc += view.size_bytes ();
    }
  return acc;
}

std::uint64_t
ref_size_bytes (std::size_t iterations, std::size_t size)
{
  return iterations * size * sizeof (int);
}

// element access

std::uint64_t
run_front_back (std::size_t iterations, std::size_t size)
{
  std::uint64_t acc = 0;
  span<int const> const view (data_base (), size);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (view);
      acc += static_cast<unsigned> (view.front ())
             + static_cast<unsigned> (view.back ());
    }
  return acc;
}

std::uint64_t
ref_front_back (std::size_t iterations, std::size_t size)
{
  return iterations
         * (static_cast<unsigned> (g_data[0])
            + static_cast<unsigned> (g_data[size - 1]));
}

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> scenarios;
  scenarios.push_back ({ "ctor_ptr_size", "span (pointer, count)", 1024,
                         run_ctor_ptr_size, ref_ctor_ptr_size, false, false });
  scenarios.push_back ({ "ctor_c_array", "span from a built-in array", 0,
                         run_ctor_c_array, ref_ctor_small, false, false });
  scenarios.push_back ({ "ctor_std_array", "span from a std::array", 0,
                         run_ctor_std_array, ref_ctor_small, false, false });
  scenarios.push_back ({ "ctor_vector",
                         "span from a std::vector (data (), size ())", 0,
                         run_ctor_vector, ref_ctor_small, false, false });
  scenarios.push_back (
      { "ctor_static_array", "span<int, 16> from a built-in array", 0,
        run_ctor_static_array, ref_ctor_small, false, false });
  scenarios.push_back ({ "copy", "copy of a span", 1024, run_copy,
                         ref_ctor_ptr_size, false, false });
  for (std::size_t const size :
       { std::size_t (8), std::size_t (1024), std::size_t (1000000) })
    {
      scenarios.push_back ({ "sum_range_for", "range-for sum of `size` ints",
                             size, run_sum_range_for, reference_sum, false,
                             false });
      scenarios.push_back ({ "sum_index",
                             "sum with operator[] over `size` ints", size,
                             run_sum_index, reference_sum, false, false });
      scenarios.push_back (
          { "sum_iterators", "sum with begin () / end () over `size` ints",
            size, run_sum_iterators, reference_sum, false, false });
    }
  scenarios.push_back (
      { "sum_static_range_for", "range-for sum of a span<int const, 16>", 0,
        run_sum_static_range_for, ref_sum_small, false, false });
  scenarios.push_back ({ "first", "first (size / 2)", 1024, run_first,
                         ref_first, false, false });
  scenarios.push_back (
      { "last", "last (size / 2)", 1024, run_last, ref_last, false, false });
  scenarios.push_back ({ "subspan", "subspan (size / 4, size / 2)", 1024,
                         run_subspan, ref_subspan, false, false });
  scenarios.push_back ({ "subspan_rest", "subspan (size / 4)", 1024,
                         run_subspan_rest, ref_subspan_rest, false, false });
  scenarios.push_back ({ "static_first", "first<4> () of span<int, 16>", 0,
                         run_static_first, ref_static_first, false, false });
  scenarios.push_back ({ "static_subspan", "subspan<2, 4> () of span<int, 16>",
                         0, run_static_subspan, ref_static_subspan, false,
                         false });
  scenarios.push_back ({ "static_first_of_dynamic",
                         "first<4> () of a dynamic span", 1024,
                         run_static_first_of_dynamic,
                         ref_static_first_of_dynamic, false, false });
  scenarios.push_back ({ "static_to_dynamic", "span<int, 16> to span<int>", 0,
                         run_static_to_dynamic, ref_ctor_small, false,
                         false });
  scenarios.push_back (
      { "dynamic_to_static", "span<int> to span<int, 16> (explicit)", 0,
        run_dynamic_to_static, ref_ctor_small, false, false });
  scenarios.push_back ({ "mutable_to_const", "span<int> to span<int const>",
                         1024, run_mutable_to_const, ref_ctor_ptr_size, false,
                         false });
  scenarios.push_back ({ "from_std_span", "std::span<int> to span<int>", 1024,
                         run_from_std_span, ref_ctor_ptr_size, false, true });
  scenarios.push_back ({ "to_std_span", "span<int> to std::span<int>", 1024,
                         run_to_std_span, ref_ctor_ptr_size, false, true });
  scenarios.push_back (
      { "bytes_to_std_span", "span<byte const> to std::span<std::byte const>",
        1024, run_bytes_to_std_span, ref_bytes_to_std_span, false, true });
  scenarios.push_back ({ "pass_noinline",
                         "pass a span by value to a noinline function", 1024,
                         run_pass_noinline, ref_pass, false, false });
  scenarios.push_back ({ "pass_inline",
                         "pass a span by value to an inlined function", 1024,
                         run_pass_inline, ref_pass, false, false });
  scenarios.push_back (
      { "pass_static_noinline",
        "pass a span<int, 16> by value to a noinline function", 0,
        run_pass_static_noinline, ref_pass_static, false, false });
  for (std::size_t const size :
       { std::size_t (8), std::size_t (1024), std::size_t (1000000) })
    scenarios.push_back ({ "divide_and_conquer",
                           "recursive sum, first / subspan halves down to "
                           "one element",
                           size, run_divide_and_conquer,
                           ref_divide_and_conquer, false, false });
  for (std::size_t const size : { std::size_t (1024), std::size_t (1000000) })
    scenarios.push_back ({ "bytes_sum", "sum of as_bytes of `size` ints", size,
                           run_bytes_sum, ref_bytes_sum, true, false });
  scenarios.push_back ({ "size_bytes", "size_bytes ()", 1024, run_size_bytes,
                         ref_size_bytes, false, false });
  scenarios.push_back ({ "front_back", "front () + back ()", 1024,
                         run_front_back, ref_front_back, false, false });
  return scenarios;
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
                << "): checksum " << got << ", the raw-pointer reference "
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
  std::string output = "span_benchmark.csv";
  std::string filter;
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
      else
        output = argv[i];
    }
  prepare_data ();

  std::vector<scenario_t> scenarios;
  for (scenario_t const &scenario : make_scenarios ())
    {
      if (scenario.needs_as_bytes && !BENCH_SPAN_HAS_AS_BYTES)
        continue;
      if (scenario.needs_std_conversion && !BENCH_SPAN_HAS_STD_CONVERSION)
        continue;
      if (!filter.empty ()
          && std::string (scenario.name).find (filter) == std::string::npos)
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
      std::cerr << "a scenario disagrees with the raw-pointer reference; "
                   "the numbers would compare different work\n";
      return 1;
    }

  double const target_seconds = quick ? 0.002 : 0.02;
  int const repetitions = quick ? 3 : 15;
  std::ofstream csv (output.c_str ());
  csv << "# impl=" << BENCH_SPAN_IMPL_NAME << " compiler=" << compiler_name ()
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
      std::printf ("%-24s %8zu %10.3f ns/op (min %.3f, max %.3f)\n",
                   scenario.name, scenario.size, result.median_ns,
                   result.min_ns, result.max_ns);
    }
  std::cout << "written " << output << '\n';
  return 0;
}

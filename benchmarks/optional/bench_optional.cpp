// bench_optional.cpp
// One source, three optionals: the scenarios below are compiled against the
// optional of this library (BENCH_OPTIONAL_IMPL=1), the std::optional of the
// standard library in use (2) or boost::optional of Boost.Optional (3), and
// every build is its own executable (see bench_optional_impl.hpp and
// CMakeLists.txt). The numbers are nanoseconds per operation, an operation
// being what the description of the scenario says (one construction, one pass
// over `size` elements, ...).
//
// Equal work: before timing, every scenario is run for a few iterations and
// its checksum is compared with a reference that uses plain values and flags
// only; the checksums are written to the CSV so that run_benchmark.py can
// compare them across the executables (a scenario with different checksums
// stops the run). The compiler is kept from deleting or hoisting the work with
// empty asm statements that read the objects and clobber memory, and values
// that would otherwise be constants are passed through asm.
//
// Usage: LumexOptionalBench_<impl>_cxx<std> [output.csv] [--quick]
//        [--filter text] [--list] [--traits traits.csv]
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bench_optional_impl.hpp"

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
using bench::optional;
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

template <typename O>
BENCH_ALWAYS_INLINE bool
has (O const &o)
{
  return static_cast<bool> (o);
}

BENCH_ALWAYS_INLINE u64
u64_of (int value)
{
  return static_cast<u64> (static_cast<std::int64_t> (value));
}

// -- The value types --

struct pod32
{
  u64 a;
  u64 b;
  u64 c;
  u64 d;
};

inline bool
operator== (pod32 const &l, pod32 const &r)
{
  return l.a == r.a && l.b == r.b && l.c == r.c && l.d == r.d;
}

inline bool
operator< (pod32 const &l, pod32 const &r)
{
  return l.a < r.a;
}

/** A value type: a make function (two samples) and a digest of a value. */
struct kind_int
{
  typedef int type;
  static type
  make (int which)
  {
    return which == 0 ? 1234 : 4321;
  }
  static u64
  digest (type const &v)
  {
    return u64_of (v);
  }
};

struct kind_pod
{
  typedef pod32 type;
  static type
  make (int which)
  {
    u64 const w = static_cast<u64> (which);
    pod32 const v = { 10 + w, 20 + w, 30 + w, 40 + w };
    return v;
  }
  static u64
  digest (type const &v)
  {
    return v.a + v.b + v.c + v.d;
  }
};

struct kind_str_short
{
  typedef std::string type;
  static type
  make (int which)
  {
    return which == 0 ? "hello-0" : "hello-1";
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
  static type
  make (int which)
  {
    return std::string (79, 'x') + (which == 0 ? 'a' : 'b');
  }
  static u64
  digest (type const &v)
  {
    return v.size () * 131u + static_cast<unsigned char> (v[v.size () - 1]);
  }
};

/** Global samples and engaged optionals of a kind (built in prepare_data). */
template <typename K> struct store
{
  static typename K::type v[2];
  static optional<typename K::type> src;
  static optional<typename K::type> dst;
};
template <typename K> typename K::type store<K>::v[2];
template <typename K> optional<typename K::type> store<K>::src;
template <typename K> optional<typename K::type> store<K>::dst;

template <typename K>
void
prepare_kind ()
{
  store<K>::v[0] = K::make (0);
  store<K>::v[1] = K::make (1);
  store<K>::src = optional<typename K::type> (store<K>::v[0]);
  store<K>::dst = optional<typename K::type> (store<K>::v[1]);
}

template <typename K>
u64
digest_of (optional<typename K::type> const &o)
{
  return has (o) ? K::digest (*o) + 1 : 0;
}

// -- Arrays --

constexpr std::size_t k_max = 1024;
constexpr std::size_t k_str = 256;

optional<int> g_all[k_max];     // every element holds a value
optional<int> g_mixed_a[k_max]; // about two thirds hold a value
optional<int> g_mixed_b[k_max]; // equal to a at every third element
bool g_has_a[k_max];
bool g_has_b[k_max];
int g_val_a[k_max];
int g_val_b[k_max];
int g_all_val[k_max];

optional<std::string> g_str_a[k_str];
optional<std::string> g_str_b[k_str];
bool g_str_has_a[k_str];
bool g_str_has_b[k_str];
std::string g_str_val_a[k_str];
std::string g_str_val_b[k_str];

std::vector<optional<int>> g_vec_int;
std::vector<optional<std::string>> g_vec_str;

void
prepare_data ()
{
  prepare_kind<kind_int> ();
  prepare_kind<kind_pod> ();
  prepare_kind<kind_str_short> ();
  prepare_kind<kind_str_long> ();
  std::uint32_t state = 12345u;
  for (std::size_t i = 0; i < k_max; ++i)
    {
      state = state * 1664525u + 1013904223u;
      bool const ha = ((state >> 16) % 3u) != 0u;
      int const va = static_cast<int> ((state >> 8) % 1000u);
      state = state * 1664525u + 1013904223u;
      bool const hb = (i % 3u == 0u) ? ha : (((state >> 16) % 3u) != 0u);
      int const vb
          = (i % 3u == 0u) ? va : static_cast<int> ((state >> 8) % 1000u);
      g_has_a[i] = ha;
      g_has_b[i] = hb;
      g_val_a[i] = va;
      g_val_b[i] = vb;
      g_all_val[i] = va + 1;
      g_all[i] = optional<int> (va + 1);
      if (ha)
        g_mixed_a[i] = optional<int> (va);
      if (hb)
        g_mixed_b[i] = optional<int> (vb);
    }
  for (std::size_t i = 0; i < k_str; ++i)
    {
      g_str_has_a[i] = g_has_a[i];
      g_str_has_b[i] = g_has_b[i];
      g_str_val_a[i] = "item-" + std::to_string (g_val_a[i]);
      g_str_val_b[i] = "item-" + std::to_string (g_val_b[i]);
      if (g_str_has_a[i])
        g_str_a[i] = optional<std::string> (g_str_val_a[i]);
      if (g_str_has_b[i])
        g_str_b[i] = optional<std::string> (g_str_val_b[i]);
    }
  for (std::size_t i = 0; i < k_max; ++i)
    g_vec_int.push_back (g_mixed_a[i]);
  for (std::size_t i = 0; i < k_str; ++i)
    g_vec_str.push_back (g_str_a[i]);
}

// -- Scenarios: run (iterations, size) and the reference with plain values --

typedef u64 (*run_t) (std::size_t iterations, std::size_t size);

struct scenario_t
{
  char const *name;
  char const *description;
  std::size_t size;
  run_t run;
  run_t reference;
};

// construction and destruction

template <typename K>
u64
run_ctor_empty (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<typename K::type> o;
      keep_object (o);
      acc += has (o) ? 0u : 1u;
    }
  return acc;
}

template <typename K>
u64
ref_ctor_empty (std::size_t iterations, std::size_t)
{
  return iterations;
}

template <typename K>
u64
run_ctor_value (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<typename K::type> o (store<K>::v[0]);
      keep_object (o);
      acc += K::digest (*o);
    }
  return acc;
}

template <typename K>
u64
ref_value0 (std::size_t iterations, std::size_t)
{
  return iterations * K::digest (store<K>::v[0]);
}

template <typename K>
u64
run_copy_ctor (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (store<K>::src);
      optional<typename K::type> c (store<K>::src);
      keep_object (c);
      acc += K::digest (*c);
    }
  return acc;
}

template <typename K>
u64
run_make_move_destroy (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<typename K::type> a (store<K>::v[0]);
      keep_object (a);
      optional<typename K::type> b (std::move (a));
      keep_object (b);
      acc += K::digest (*b);
    }
  return acc;
}

// assignment, emplace, swap

template <typename K>
u64
run_assign_copy (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_object (store<K>::src);
      store<K>::dst = store<K>::src;
      keep_object (store<K>::dst);
      acc += K::digest (*store<K>::dst);
    }
  return acc;
}

template <typename K>
u64
run_assign_value (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      store<K>::dst = store<K>::v[0];
      keep_object (store<K>::dst);
      acc += K::digest (*store<K>::dst);
    }
  return acc;
}

template <typename K>
u64
run_emplace_reset (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  optional<typename K::type> o (store<K>::v[1]);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      o.reset ();
      keep_object (o);
      o.emplace (store<K>::v[0]);
      keep_object (o);
      acc += K::digest (*o);
    }
  return acc;
}

template <typename K>
u64
run_swap (std::size_t iterations, std::size_t)
{
  using std::swap;
  u64 acc = 0;
  optional<typename K::type> a (store<K>::v[0]);
  optional<typename K::type> b (store<K>::v[1]);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      swap (a, b);
      keep_object (a);
      keep_object (b);
      acc += K::digest (*a);
    }
  return acc;
}

template <typename K>
u64
ref_swap (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    acc += K::digest (store<K>::v[i % 2 == 0 ? 1 : 0]);
  return acc;
}

// access and comparison over arrays of optional<int>

u64
run_deref_sum (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const base = hide (&g_all[0]);
      u64 sum = 0;
      for (std::size_t j = 0; j < size; ++j)
        sum += u64_of (*base[j]);
      keep_value (sum);
      acc += sum;
    }
  return acc;
}

u64
run_value_sum (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const base = hide (&g_all[0]);
      u64 sum = 0;
      for (std::size_t j = 0; j < size; ++j)
        sum += u64_of (base[j].value ());
      keep_value (sum);
      acc += sum;
    }
  return acc;
}

u64
ref_deref_sum (std::size_t iterations, std::size_t size)
{
  u64 sum = 0;
  for (std::size_t j = 0; j < size; ++j)
    sum += u64_of (g_all_val[j]);
  return iterations * sum;
}

u64
run_value_or_sum (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const base = hide (&g_mixed_a[0]);
      u64 sum = 0;
      for (std::size_t j = 0; j < size; ++j)
        sum += u64_of (base[j].value_or (7));
      keep_value (sum);
      acc += sum;
    }
  return acc;
}

u64
ref_value_or_sum (std::size_t iterations, std::size_t size)
{
  u64 sum = 0;
  for (std::size_t j = 0; j < size; ++j)
    sum += u64_of (g_has_a[j] ? g_val_a[j] : 7);
  return iterations * sum;
}

u64
run_count_engaged (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const base = hide (&g_mixed_a[0]);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += has (base[j]) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_count_engaged (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    count += g_has_a[j] ? 1u : 0u;
  return iterations * count;
}

u64
run_eq_opt_opt (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const a = hide (&g_mixed_a[0]);
      optional<int> const *const b = hide (&g_mixed_b[0]);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += (a[j] == b[j]) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_eq_opt_opt (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    {
      bool eq;
      if (g_has_a[j] != g_has_b[j])
        eq = false;
      else if (!g_has_a[j])
        eq = true;
      else
        eq = g_val_a[j] == g_val_b[j];
      count += eq ? 1u : 0u;
    }
  return iterations * count;
}

u64
run_lt_opt_opt (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const a = hide (&g_mixed_a[0]);
      optional<int> const *const b = hide (&g_mixed_b[0]);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += (a[j] < b[j]) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_lt_opt_opt (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    {
      bool lt;
      if (!g_has_b[j])
        lt = false;
      else if (!g_has_a[j])
        lt = true;
      else
        lt = g_val_a[j] < g_val_b[j];
      count += lt ? 1u : 0u;
    }
  return iterations * count;
}

u64
run_eq_value (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const a = hide (&g_mixed_a[0]);
      int const probe = hide (500);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += (a[j] == probe) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_eq_value (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    count += (g_has_a[j] && g_val_a[j] == 500) ? 1u : 0u;
  return iterations * count;
}

u64
run_eq_nullopt (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> const *const a = hide (&g_mixed_a[0]);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += (a[j] == BENCH_NULLOPT) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_eq_nullopt (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    count += g_has_a[j] ? 0u : 1u;
  return iterations * count;
}

u64
run_eq_opt_opt_str (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<std::string> const *const a = hide (&g_str_a[0]);
      optional<std::string> const *const b = hide (&g_str_b[0]);
      u64 count = 0;
      for (std::size_t j = 0; j < size; ++j)
        count += (a[j] == b[j]) ? 1u : 0u;
      keep_value (count);
      acc += count;
    }
  return acc;
}

u64
ref_eq_opt_opt_str (std::size_t iterations, std::size_t size)
{
  u64 count = 0;
  for (std::size_t j = 0; j < size; ++j)
    {
      bool eq;
      if (g_str_has_a[j] != g_str_has_b[j])
        eq = false;
      else if (!g_str_has_a[j])
        eq = true;
      else
        eq = g_str_val_a[j] == g_str_val_b[j];
      count += eq ? 1u : 0u;
    }
  return iterations * count;
}

// optional as a return value and as an argument

BENCH_NOINLINE optional<int>
find_int (int key)
{
  if ((key & 3) != 0)
    return optional<int> (key * 3);
  return optional<int> ();
}

u64
run_return_int (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<int> r = find_int (hide (static_cast<int> (i)));
      acc += has (r) ? u64_of (*r) : u64_of (-1);
    }
  return acc;
}

u64
ref_return_int (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      int const key = static_cast<int> (i);
      acc += (key & 3) != 0 ? u64_of (key * 3) : u64_of (-1);
    }
  return acc;
}

BENCH_NOINLINE int
consume_int (optional<int> o)
{
  return has (o) ? *o : -1;
}

u64
run_pass_int (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      int const key = hide (static_cast<int> (i));
      acc += u64_of (consume_int ((key & 3) != 0 ? optional<int> (key * 3)
                                                 : optional<int> ()));
    }
  return acc;
}

BENCH_NOINLINE optional<std::string>
find_str (int key)
{
  if ((key & 3) != 0)
    return optional<std::string> (store<kind_str_short>::v[0]);
  return optional<std::string> ();
}

u64
run_return_str (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      optional<std::string> r = find_str (hide (static_cast<int> (i)));
      acc += has (r) ? kind_str_short::digest (*r) : 0u;
    }
  return acc;
}

u64
ref_return_str (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    acc += (static_cast<int> (i) & 3) != 0
               ? kind_str_short::digest (store<kind_str_short>::v[0])
               : 0u;
  return acc;
}

BENCH_NOINLINE u64
consume_str (optional<std::string> o)
{
  return has (o) ? kind_str_short::digest (*o) : 0u;
}

u64
run_pass_str (std::size_t iterations, std::size_t)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      int const key = hide (static_cast<int> (i));
      acc += consume_str (
          (key & 3) != 0 ? optional<std::string> (store<kind_str_short>::v[0])
                         : optional<std::string> ());
    }
  return acc;
}

// optional as a container element

u64
run_vector_copy_int (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_value (g_vec_int.data ());
      std::vector<optional<int>> copy (
          g_vec_int.begin (),
          g_vec_int.begin () + static_cast<std::ptrdiff_t> (size));
      keep_value (copy.data ());
      acc += copy.size ()
             + (has (copy[size / 2]) ? u64_of (*copy[size / 2]) + 1 : 0u);
    }
  return acc;
}

u64
ref_vector_copy_int (std::size_t iterations, std::size_t size)
{
  return iterations
         * (size + (g_has_a[size / 2] ? u64_of (g_val_a[size / 2]) + 1 : 0u));
}

u64
run_vector_copy_str (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      keep_value (g_vec_str.data ());
      std::vector<optional<std::string>> copy (
          g_vec_str.begin (),
          g_vec_str.begin () + static_cast<std::ptrdiff_t> (size));
      keep_value (copy.data ());
      acc += copy.size ()
             + (has (copy[size / 2])
                    ? kind_str_short::digest (*copy[size / 2]) + 1
                    : 0u);
    }
  return acc;
}

u64
ref_vector_copy_str (std::size_t iterations, std::size_t size)
{
  return iterations
         * (size
            + (g_str_has_a[size / 2]
                   ? kind_str_short::digest (g_str_val_a[size / 2]) + 1
                   : 0u));
}

u64
run_vector_push_int (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      std::vector<optional<int>> v;
      v.reserve (size);
      for (std::size_t j = 0; j < size; ++j)
        v.push_back (optional<int> (hide (static_cast<int> (j))));
      keep_value (v.data ());
      acc += v.size () + u64_of (*v[size - 1]);
    }
  return acc;
}

u64
ref_vector_push_int (std::size_t iterations, std::size_t size)
{
  return iterations * (size + u64_of (static_cast<int> (size - 1)));
}

u64
run_vector_grow_str (std::size_t iterations, std::size_t size)
{
  u64 acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      std::vector<optional<std::string>> v;
      for (std::size_t j = 0; j < size; ++j)
        v.push_back (optional<std::string> (store<kind_str_short>::v[0]));
      keep_value (v.data ());
      acc += v.size () + kind_str_short::digest (*v[size - 1]);
    }
  return acc;
}

u64
ref_vector_grow_str (std::size_t iterations, std::size_t size)
{
  return iterations
         * (size + kind_str_short::digest (store<kind_str_short>::v[0]));
}

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> s;
#define BENCH_ADD_KIND(family, K, kname, desc)                                \
  s.push_back (                                                               \
      { #family "_" kname, desc, 0, run_##family<K>, ref_##family<K> })
#define BENCH_ADD_KIND_V0(family, K, kname, desc)                             \
  s.push_back ({ #family "_" kname, desc, 0, run_##family<K>, ref_value0<K> })
#define BENCH_ALL_KINDS(macro, family, desc)                                  \
  macro (family, kind_int, "int", desc);                                      \
  macro (family, kind_pod, "pod32", desc);                                    \
  macro (family, kind_str_short, "str_short", desc);                          \
  macro (family, kind_str_long, "str_long", desc)

  BENCH_ADD_KIND (ctor_empty, kind_int, "int", "construct an empty optional");
  BENCH_ADD_KIND (ctor_empty, kind_str_short, "str_short",
                  "construct an empty optional");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, ctor_value,
                   "construct from a copy of a value, destroy");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, copy_ctor,
                   "copy-construct from an engaged optional, destroy");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, make_move_destroy,
                   "construct, move-construct from it, destroy both");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, assign_copy,
                   "copy-assign an engaged optional to an engaged one");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, assign_value,
                   "assign a value to an engaged optional");
  BENCH_ALL_KINDS (BENCH_ADD_KIND_V0, emplace_reset,
                   "emplace from a value, reset");
  BENCH_ADD_KIND (swap, kind_int, "int", "swap two engaged optionals");
  BENCH_ADD_KIND (swap, kind_str_short, "str_short",
                  "swap two engaged optionals");

  s.push_back ({ "deref_sum", "sum of *o over `size` engaged optional<int>", 8,
                 run_deref_sum, ref_deref_sum });
  s.push_back ({ "deref_sum", "sum of *o over `size` engaged optional<int>",
                 1024, run_deref_sum, ref_deref_sum });
  s.push_back ({ "value_sum",
                 "sum of o.value () over `size` engaged optional<int>", 1024,
                 run_value_sum, ref_deref_sum });
  s.push_back (
      { "value_or_sum",
        "sum of o.value_or (7) over `size` optional<int>, 2/3 engaged", 1024,
        run_value_or_sum, ref_value_or_sum });
  s.push_back ({ "count_engaged",
                 "count of engaged over `size` optional<int>, 2/3 engaged",
                 1024, run_count_engaged, ref_count_engaged });
  s.push_back ({ "eq_opt_opt", "a[j] == b[j] over `size` optional<int> pairs",
                 1024, run_eq_opt_opt, ref_eq_opt_opt });
  s.push_back ({ "lt_opt_opt", "a[j] < b[j] over `size` optional<int> pairs",
                 1024, run_lt_opt_opt, ref_lt_opt_opt });
  s.push_back ({ "eq_value", "a[j] == 500 over `size` optional<int>", 1024,
                 run_eq_value, ref_eq_value });
  s.push_back ({ "eq_nullopt", "a[j] == nullopt over `size` optional<int>",
                 1024, run_eq_nullopt, ref_eq_nullopt });
  s.push_back ({ "eq_opt_opt_str",
                 "a[j] == b[j] over `size` optional<std::string> pairs", 256,
                 run_eq_opt_opt_str, ref_eq_opt_opt_str });

  s.push_back (
      { "return_int",
        "a noinline function returns optional<int>, the caller reads it", 0,
        run_return_int, ref_return_int });
  s.push_back ({ "pass_int",
                 "optional<int> passed by value to a noinline function", 0,
                 run_pass_int, ref_return_int });
  s.push_back ({ "return_str",
                 "a noinline function returns optional<std::string>", 0,
                 run_return_str, ref_return_str });
  s.push_back (
      { "pass_str",
        "optional<std::string> passed by value to a noinline function", 0,
        run_pass_str, ref_return_str });

  s.push_back ({ "vector_copy_int",
                 "copy a std::vector of `size` optional<int>", 1024,
                 run_vector_copy_int, ref_vector_copy_int });
  s.push_back ({ "vector_copy_str",
                 "copy a std::vector of `size` optional<std::string>", 256,
                 run_vector_copy_str, ref_vector_copy_str });
  s.push_back ({ "vector_push_int",
                 "push_back `size` optional<int> after reserve, destroy", 1024,
                 run_vector_push_int, ref_vector_push_int });
  s.push_back ({ "vector_grow_str",
                 "push_back `size` optional<std::string> without reserve "
                 "(reallocations), destroy",
                 256, run_vector_grow_str, ref_vector_grow_str });
  return s;
}

// -- Layout and traits of optional<T> for a few T --

template <typename T>
void
traits_row (std::ostream &out, char const *name)
{
  typedef optional<T> O;
  out << name << ',' << sizeof (T) << ',' << sizeof (O) << ',' << alignof (O)
      << ',' << (std::is_trivially_copyable<O>::value ? 1 : 0) << ','
      << (std::is_trivially_destructible<O>::value ? 1 : 0) << ','
      << (std::is_trivially_copy_constructible<O>::value ? 1 : 0) << ','
      << (std::is_standard_layout<O>::value ? 1 : 0) << ','
      << (std::is_nothrow_move_constructible<O>::value ? 1 : 0) << ','
      << (std::is_nothrow_default_constructible<O>::value ? 1 : 0) << '\n';
}

void
write_traits (std::string const &path)
{
  std::ofstream out (path.c_str ());
  out << "type,sizeof_t,sizeof_optional,alignof_optional,"
         "trivially_copyable,trivially_destructible,"
         "trivially_copy_constructible,standard_layout,"
         "nothrow_move_constructible,nothrow_default_constructible\n";
  traits_row<bool> (out, "bool");
  traits_row<char> (out, "char");
  traits_row<int> (out, "int");
  traits_row<double> (out, "double");
  traits_row<std::uint64_t> (out, "uint64_t");
  traits_row<pod32> (out, "pod32");
  traits_row<std::string> (out, "std::string");
  traits_row<std::vector<int>> (out, "std::vector<int>");
  traits_row<std::unique_ptr<int>> (out, "std::unique_ptr<int>");
  traits_row<std::shared_ptr<int>> (out, "std::shared_ptr<int>");
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
                << "): checksum " << got << ", the plain-value reference "
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
  std::string output = "optional_benchmark.csv";
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
      write_traits (traits);
      return 0;
    }
  prepare_data ();

  std::vector<scenario_t> scenarios;
  for (scenario_t const &scenario : make_scenarios ())
    {
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
      std::cerr << "a scenario disagrees with the plain-value reference; "
                   "the numbers would compare different work\n";
      return 1;
    }

  double const target_seconds = quick ? 0.002 : 0.02;
  int const repetitions = quick ? 3 : 15;
  std::ofstream csv (output.c_str ());
  csv << "# impl=" << BENCH_OPTIONAL_IMPL_NAME
      << " compiler=" << compiler_name () << " library=" << library_name ()
      << " cplusplus=" << __cplusplus
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

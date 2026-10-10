// bench_string_view.cpp
// One source, three string views: the scenarios below are compiled against
// lumex_string_view of this library (BENCH_STRING_VIEW_IMPL=1),
// std::string_view of the standard library in use (2) or boost::string_view
// of Boost.Utility (3), and every build is its own executable (see
// bench_string_view_impl.hpp and CMakeLists.txt). The numbers are nanoseconds
// per operation, an operation being what the description of the scenario says
// (one construction, one search in one string, ...); a loop over the 256
// strings of the corpus is divided by 256.
//
// Equal work: before timing, every scenario is run for a few iterations and
// its checksum is compared with a reference that runs the same source on
// std::string objects (the same member functions, the same data); the
// checksums are written to the CSV so that run_benchmark.py can compare them
// across the executables (a scenario with different checksums stops the run).
// The compiler is kept from deleting or hoisting the work with empty asm
// statements that read the objects and clobber memory, and the corpus is built
// at run time, so no string or length is a compile-time constant (except where
// a scenario says that it uses a literal on purpose).
//
// Usage: LumexStringViewBench_<impl>_cxx<std> [output.csv] [--quick]
//        [--filter text] [--list] [--traits traits.csv]
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bench_string_view_impl.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BENCH_UNUSED __attribute__ ((unused))
#define BENCH_NOINLINE __attribute__ ((noinline))
#define BENCH_ALWAYS_INLINE inline __attribute__ ((always_inline))
#elif defined(_MSC_VER)
#define BENCH_UNUSED
#define BENCH_NOINLINE __declspec (noinline)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_UNUSED
#define BENCH_NOINLINE
#define BENCH_ALWAYS_INLINE inline
#endif

// starts_with and ends_with of std::string_view are C++20.
#if BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_STD && __cplusplus < 202002L
#define BENCH_HAS_STARTS_WITH 0
#else
#define BENCH_HAS_STARTS_WITH 1
#endif

namespace bench
{
// Hash of the reference (std::string) data; the views have theirs in the impl
// header. Declared in bench next to the view's overload.
inline std::size_t
hash_of (std::string const &s)
{
  return std::hash<std::string> () (s);
}
} // namespace bench

namespace
{
using bench::view;
#if BENCH_HAS_HASH
using bench::hash_of;
#endif
using u64 = std::uint64_t;
using std::size_t;

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

// -- The corpus --

constexpr size_t k_count = 256; // strings per loop
constexpr size_t k_text = 4096; // characters of the long text

std::vector<std::string> g_main;  // "methods/g3/run_17_isocratic.ini"
std::vector<std::string> g_equal; // the same text in other buffers
std::vector<std::string> g_early; // differs in the first character
std::vector<std::string> g_late;  // differs in the last character
std::vector<std::string> g_short; // a prefix: another length
std::string g_text;               // 4096 characters of 16 letters
std::string g_csv;                // g_text with a ',' every few characters
std::string g_needle_hit;         // "isocratic"
std::string g_needle_miss;        // "gradient"
std::string g_needle_long;        // 8 letters that g_text does not contain
std::string g_needle_text;        // "abcdefgh": absent, first letter common
std::string g_prefix;             // "methods/"
std::string g_suffix;             // ".ini"
std::string g_set;                // "/_."
std::string g_set_not;            // "methods/gr"
char const *g_cstr[k_count];
size_t g_len[k_count];

u64 g_seed = 88172645463325252ULL;

u64
next_random ()
{
  g_seed ^= g_seed << 13;
  g_seed ^= g_seed >> 7;
  g_seed ^= g_seed << 17;
  return g_seed;
}

void
prepare_corpus ()
{
  g_main.reserve (k_count);
  g_equal.reserve (k_count);
  g_early.reserve (k_count);
  g_late.reserve (k_count);
  g_short.reserve (k_count);
  for (size_t i = 0; i < k_count; ++i)
    {
      std::string text = "methods/g" + std::to_string (i % 7) + "/run_"
                         + std::to_string (i * 7919 % 10007)
                         + "_isocratic.ini";
      g_main.push_back (text);
      g_equal.push_back (std::string (text.begin (), text.end ()));
      std::string early = text;
      early[0] = 'x';
      g_early.push_back (early);
      std::string late = text;
      late[late.size () - 1] = 'x';
      g_late.push_back (late);
      g_short.push_back (text.substr (0, text.size () - 3));
    }
  for (size_t i = 0; i < k_count; ++i)
    {
      g_cstr[i] = g_main[i].c_str ();
      g_len[i] = g_main[i].size ();
    }
  g_text.resize (k_text);
  for (size_t i = 0; i < k_text; ++i)
    g_text[i] = static_cast<char> ('a' + next_random () % 16);
  g_csv = g_text;
  for (size_t i = 0; i < k_text; ++i)
    if (next_random () % 9 == 0)
      g_csv[i] = ',';
  g_needle_hit = "isocratic";
  g_needle_miss = "gradient";
  g_needle_long = "qqqqqqqq";
  g_needle_text = "abcdefgh";
  g_prefix = "methods/";
  g_suffix = ".ini";
  g_set = "/_.";
  g_set_not = "methods/gr";
}

/** The data a scenario works on, for a string type S (a view, or the
 * std::string of the reference). */
template <typename S> struct data
{
  static S main_[k_count];
  static S equal_[k_count];
  static S early_[k_count];
  static S late_[k_count];
  static S short_[k_count];
  static S text;
  static S csv;
  static S needle_hit;
  static S needle_miss;
  static S needle_long;
  static S needle_text;
  static S prefix;
  static S suffix;
  static S set;
  static S set_not;
  static std::size_t hashes[k_count];
  static std::map<S, u64> map;
};
template <typename S> S data<S>::main_[k_count];
template <typename S> S data<S>::equal_[k_count];
template <typename S> S data<S>::early_[k_count];
template <typename S> S data<S>::late_[k_count];
template <typename S> S data<S>::short_[k_count];
template <typename S> S data<S>::text;
template <typename S> S data<S>::csv;
template <typename S> S data<S>::needle_hit;
template <typename S> S data<S>::needle_miss;
template <typename S> S data<S>::needle_long;
template <typename S> S data<S>::needle_text;
template <typename S> S data<S>::prefix;
template <typename S> S data<S>::suffix;
template <typename S> S data<S>::set;
template <typename S> S data<S>::set_not;
template <typename S> std::size_t data<S>::hashes[k_count];
template <typename S> std::map<S, u64> data<S>::map;

template <typename S>
void
prepare_data ()
{
  typedef data<S> d;
  for (size_t i = 0; i < k_count; ++i)
    {
      d::main_[i] = S (g_main[i]);
      d::equal_[i] = S (g_equal[i]);
      d::early_[i] = S (g_early[i]);
      d::late_[i] = S (g_late[i]);
      d::short_[i] = S (g_short[i]);
      d::map[d::main_[i]] = i + 1;
    }
  d::text = S (g_text);
  d::csv = S (g_csv);
  d::needle_hit = S (g_needle_hit);
  d::needle_miss = S (g_needle_miss);
  d::needle_long = S (g_needle_long);
  d::needle_text = S (g_needle_text);
  d::prefix = S (g_prefix);
  d::suffix = S (g_suffix);
  d::set = S (g_set);
  d::set_not = S (g_set_not);
#if BENCH_HAS_HASH
  for (size_t i = 0; i < k_count; ++i)
    d::hashes[i] = hash_of (d::main_[i]);
#endif
}

// -- Operations that the views and std::string spell differently --

template <typename S>
BENCH_ALWAYS_INLINE bool
starts_with_s (S const &a, S const &b)
{
  return a.starts_with (b);
}
BENCH_ALWAYS_INLINE bool
starts_with_s (std::string const &a, std::string const &b)
{
  return a.compare (0, b.size (), b) == 0;
}
template <typename S>
BENCH_ALWAYS_INLINE bool
ends_with_s (S const &a, S const &b)
{
  return a.ends_with (b);
}
BENCH_ALWAYS_INLINE bool
ends_with_s (std::string const &a, std::string const &b)
{
  return a.size () >= b.size ()
         && a.compare (a.size () - b.size (), b.size (), b) == 0;
}
template <typename S>
BENCH_ALWAYS_INLINE bool
starts_with_lit (S const &a, char const *lit)
{
  return a.starts_with (lit);
}
BENCH_ALWAYS_INLINE bool
starts_with_lit (std::string const &a, char const *lit)
{
  return a.compare (0, std::strlen (lit), lit) == 0;
}
template <typename S>
BENCH_ALWAYS_INLINE bool
ends_with_lit (S const &a, char const *lit)
{
  return a.ends_with (lit);
}
BENCH_ALWAYS_INLINE bool
ends_with_lit (std::string const &a, char const *lit)
{
  size_t const n = std::strlen (lit);
  return a.size () >= n && a.compare (a.size () - n, n, lit) == 0;
}
template <typename S>
BENCH_ALWAYS_INLINE bool
starts_with_chr (S const &a, char c)
{
  return a.starts_with (c);
}
BENCH_ALWAYS_INLINE bool
starts_with_chr (std::string const &a, char c)
{
  return !a.empty () && a.front () == c;
}

BENCH_ALWAYS_INLINE u64
sign_of (int value)
{
  return value < 0 ? 0 : (value == 0 ? 1 : 2);
}

template <typename S>
BENCH_ALWAYS_INLINE u64
digest_of (S const &s)
{
  return s.size () * 131u
         + (s.empty () ? 0u : static_cast<unsigned char> (s[s.size () - 1]));
}

// -- Scenarios --

struct scenario_t
{
  char const *name;
  char const *description;
  size_t size; // operations per iteration
  u64 (*run) (size_t iterations, size_t size);
  u64 (*reference) (size_t iterations, size_t size);
};

// construction

template <typename S>
u64
run_ctor_literal (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const v ("methods/isocratic.ini");
          keep_object (v);
          sum += digest_of (v);
        }
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_ctor_cstr (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const v (g_cstr[i]);
          keep_object (v);
          sum += digest_of (v);
        }
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_ctor_string (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const v (g_main[i]);
          keep_object (v);
          sum += digest_of (v);
        }
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_ctor_ptr_size (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const v (g_cstr[i], g_len[i]);
          keep_object (v);
          sum += digest_of (v);
        }
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_copy (size_t iterations, size_t)
{
  typedef data<S> d;
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const v (d::main_[i]);
          keep_object (v);
          sum += digest_of (v);
        }
      keep_value (sum);
    }
  return sum;
}

// observers

template <typename S>
u64
run_index_sum (size_t iterations, size_t)
{
  typedef data<S> d;
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        {
          S const &v = d::main_[i];
          for (size_t j = 0; j < v.size (); ++j)
            sum += static_cast<unsigned char> (v[j]);
        }
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_range_sum (size_t iterations, size_t)
{
  typedef data<S> d;
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        for (char const c : d::main_[i])
          sum += static_cast<unsigned char> (c);
      keep_value (sum);
    }
  return sum;
}

// search

#define BENCH_LOOP_BEGIN                                                      \
  typedef data<S> d BENCH_UNUSED;                                             \
  u64 sum = 0;                                                                \
  for (size_t it = 0; it < iterations; ++it)                                  \
    {                                                                         \
      for (size_t i = 0; i < k_count; ++i)                                    \
        {
#define BENCH_LOOP_END                                                        \
  }                                                                           \
  keep_value (sum);                                                           \
  }                                                                           \
  return sum;

template <typename S>
u64
run_find_char (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find ('/');
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_char_late (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find ('.');
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_char_miss (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find ('#');
  BENCH_LOOP_END
}

template <typename S>
u64
run_rfind_char (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].rfind ('/');
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_str_hit (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find (d::needle_hit);
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_str_miss (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find (d::needle_miss);
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_str_text (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      sum += data<S>::text.find (data<S>::needle_text);
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_find_str_absent (size_t iterations, size_t)
{
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      sum += data<S>::text.find (data<S>::needle_long);
      keep_value (sum);
    }
  return sum;
}

template <typename S>
u64
run_rfind_str (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].rfind (d::needle_hit);
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_first_of (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find_first_of (d::set);
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_last_of (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find_last_of (d::set);
  BENCH_LOOP_END
}

template <typename S>
u64
run_find_first_not_of (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += d::main_[i].find_first_not_of (d::set_not);
  BENCH_LOOP_END
}

// slicing

template <typename S>
u64
run_substr (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  S const v = d::main_[i].substr (8, 12);
  keep_object (v);
  sum += digest_of (v);
  BENCH_LOOP_END
}

template <typename S>
u64
run_substr_tail (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  S const &whole = d::main_[i];
  S const v = whole.substr (whole.size () - 4);
  keep_object (v);
  sum += digest_of (v);
  BENCH_LOOP_END
}

// comparison

template <typename S>
u64
run_compare_equal (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += sign_of (d::main_[i].compare (d::equal_[i]));
  BENCH_LOOP_END
}

template <typename S>
u64
run_compare_early (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += sign_of (d::main_[i].compare (d::early_[i]));
  BENCH_LOOP_END
}

template <typename S>
u64
run_compare_late (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += sign_of (d::main_[i].compare (d::late_[i]));
  BENCH_LOOP_END
}

template <typename S>
u64
run_eq_equal (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] == d::equal_[i]) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_eq_size (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] == d::short_[i]) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_eq_late (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] == d::late_[i]) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_less (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] < d::late_[(i + 1) % k_count]) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_eq_literal (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] == "methods/g0/run_0_isocratic.ini") ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_starts_with (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += starts_with_s (d::main_[i], d::prefix) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_ends_with (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += ends_with_s (d::main_[i], d::suffix) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_starts_with_lit (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += starts_with_lit (d::main_[i], "methods/") ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_ends_with_lit (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += ends_with_lit (d::main_[i], ".ini") ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_starts_with_chr (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += starts_with_chr (d::main_[i], 'm') ? 1U : 0U;
  BENCH_LOOP_END
}

// hashing

template <typename S>
u64
run_hash (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  std::size_t const h = hash_of (d::main_[i]);
  keep_value (h);
  sum += (h == d::hashes[i]) ? 1U : 0U;
  BENCH_LOOP_END
}

// the use of a view in a program

template <typename S>
BENCH_NOINLINE u64
take_by_value (S v)
{
  return v.size () + (v.empty () ? 0u : static_cast<unsigned char> (v[0]));
}

template <typename S>
BENCH_NOINLINE S
give_back (char const *ptr, size_t len)
{
  return S (ptr, len);
}

template <typename S>
u64
run_pass_by_value (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += take_by_value<S> (d::main_[i]);
  BENCH_LOOP_END
}

template <typename S>
u64
run_return_value (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  S const v = give_back<S> (g_cstr[i], g_len[i]);
  sum += digest_of (v);
  BENCH_LOOP_END
}

template <typename S>
u64
run_sort (size_t iterations, size_t)
{
  typedef data<S> d;
  u64 sum = 0;
  std::vector<S> work (k_count);
  for (size_t it = 0; it < iterations; ++it)
    {
      for (size_t i = 0; i < k_count; ++i)
        work[i] = d::late_[(i * 37) % k_count];
      std::sort (work.begin (), work.end ());
      keep_object (work[0]);
      sum += digest_of (work[0]) + digest_of (work[k_count / 2])
             + digest_of (work[k_count - 1]);
    }
  return sum;
}

template <typename S>
u64
run_map_lookup (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  typename std::map<S, u64>::const_iterator const found
      = d::map.find (d::equal_[i]);
  sum += found->second;
  BENCH_LOOP_END
}

template <typename S>
u64
run_split (size_t iterations, size_t)
{
  typedef data<S> d;
  u64 sum = 0;
  for (size_t it = 0; it < iterations; ++it)
    {
      S const &text = d::csv;
      size_t pos = 0;
      while (pos <= text.size ())
        {
          size_t end = text.find (',', pos);
          if (end == S::npos)
            end = text.size ();
          S const field = text.substr (pos, end - pos);
          keep_object (field);
          sum += digest_of (field);
          pos = end + 1;
        }
      keep_value (sum);
    }
  return sum;
}

// conversions to and from std::string_view

#if BENCH_HAS_STD_CONVERSION
std::vector<std::string_view> g_std_views;

template <typename S>
void
prepare_std_views ()
{
  g_std_views.clear ();
  for (size_t i = 0; i < k_count; ++i)
    g_std_views.push_back (std::string_view (g_main[i]));
}

template <typename S>
u64
run_to_std (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  std::string_view const v (d::main_[i]);
  keep_object (v);
  sum += digest_of (v);
  BENCH_LOOP_END
}

template <typename S>
u64
run_from_std (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  S const v (g_std_views[i]);
  keep_object (v);
  sum += digest_of (v);
  BENCH_LOOP_END
}

template <typename S>
u64
run_eq_std (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] == g_std_views[i]) ? 1U : 0U;
  BENCH_LOOP_END
}

template <typename S>
u64
run_less_std (size_t iterations, size_t)
{
  BENCH_LOOP_BEGIN
  sum += (d::main_[i] < g_std_views[(i + 1) % k_count]) ? 1U : 0U;
  BENCH_LOOP_END
}
#endif

#define BENCH_SCENARIO(NAME, DESC, SIZE, FN)                                  \
  { NAME, DESC, SIZE, &FN<view>, &FN<std::string> }

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> list;
  scenario_t const base[] = {
    BENCH_SCENARIO (
        "ctor_literal",
        "construct from a string literal (length known to the compiler)",
        k_count, run_ctor_literal),
    BENCH_SCENARIO ("ctor_cstr",
                    "construct from a char const * (length by strlen)",
                    k_count, run_ctor_cstr),
    BENCH_SCENARIO ("ctor_string", "construct from a std::string", k_count,
                    run_ctor_string),
    BENCH_SCENARIO ("ctor_ptr_size", "construct from pointer and size",
                    k_count, run_ctor_ptr_size),
    BENCH_SCENARIO ("copy", "copy construct a view", k_count, run_copy),
    BENCH_SCENARIO (
        "index_sum",
        "sum of the characters of one string of 30 to 33, by operator[]",
        k_count, run_index_sum),
    BENCH_SCENARIO (
        "range_sum",
        "sum of the characters of one string of 30 to 33, by range-for",
        k_count, run_range_sum),
    BENCH_SCENARIO ("find_char", "find('/') in one string, hit at position 7",
                    k_count, run_find_char),
    BENCH_SCENARIO ("find_char_late",
                    "find('.') in one string, hit near the end", k_count,
                    run_find_char_late),
    BENCH_SCENARIO ("find_char_miss", "find('#') in one string, no hit",
                    k_count, run_find_char_miss),
    BENCH_SCENARIO ("rfind_char", "rfind('/') in one string", k_count,
                    run_rfind_char),
    BENCH_SCENARIO ("find_str_hit",
                    "find a 9-character view in one string, hit at 17",
                    k_count, run_find_str_hit),
    BENCH_SCENARIO ("find_str_miss",
                    "find an 8-character view in one string, no hit", k_count,
                    run_find_str_miss),
    BENCH_SCENARIO ("find_str_text",
                    "find an 8-character view in a text of 4096 characters, "
                    "no hit; its first letter occurs at every 16th position",
                    1, run_find_str_text),
    BENCH_SCENARIO ("find_str_absent",
                    "find an 8-character view in a text of 4096 characters, "
                    "no hit; its first letter does not occur",
                    1, run_find_str_absent),
    BENCH_SCENARIO ("rfind_str", "rfind a 9-character view in one string",
                    k_count, run_rfind_str),
    BENCH_SCENARIO ("find_first_of",
                    "find_first_of a set of 3 characters in one string",
                    k_count, run_find_first_of),
    BENCH_SCENARIO ("find_last_of",
                    "find_last_of a set of 3 characters in one string",
                    k_count, run_find_last_of),
    BENCH_SCENARIO ("find_first_not_of",
                    "find_first_not_of a set of 10 characters in one string",
                    k_count, run_find_first_not_of),
    BENCH_SCENARIO ("substr", "substr(8, 12) of one string", k_count,
                    run_substr),
    BENCH_SCENARIO ("substr_tail", "substr(size - 4) of one string", k_count,
                    run_substr_tail),
    BENCH_SCENARIO ("compare_equal",
                    "compare two equal strings in different buffers", k_count,
                    run_compare_equal),
    BENCH_SCENARIO ("compare_early",
                    "compare two strings that differ in the first character",
                    k_count, run_compare_early),
    BENCH_SCENARIO ("compare_late",
                    "compare two strings that differ in the last character",
                    k_count, run_compare_late),
    BENCH_SCENARIO ("eq_equal",
                    "operator== of two equal strings in different buffers",
                    k_count, run_eq_equal),
    BENCH_SCENARIO ("eq_size", "operator== of two strings of different sizes",
                    k_count, run_eq_size),
    BENCH_SCENARIO ("eq_late",
                    "operator== of two strings of one size that differ in the "
                    "last character",
                    k_count, run_eq_late),
    BENCH_SCENARIO (
        "less", "operator< of two strings that differ in the last character",
        k_count, run_less),
    BENCH_SCENARIO ("eq_literal", "operator== of a view and a string literal",
                    k_count, run_eq_literal),
    BENCH_SCENARIO ("pass_by_value",
                    "call a noinline function that takes the view by value",
                    k_count, run_pass_by_value),
    BENCH_SCENARIO ("return_value",
                    "call a noinline function that returns a view built from "
                    "pointer and size",
                    k_count, run_return_value),
    BENCH_SCENARIO ("sort", "std::sort of 256 views (per element)", k_count,
                    run_sort),
    BENCH_SCENARIO ("map_lookup",
                    "find in a std::map of 256 views by an equal view",
                    k_count, run_map_lookup),
    BENCH_SCENARIO ("split",
                    "split a text of 4096 characters at ',' into views (about "
                    "450 fields; one split)",
                    1, run_split),
  };
  list.assign (base, base + sizeof (base) / sizeof (base[0]));
#if BENCH_HAS_STARTS_WITH
  scenario_t const starts[] = {
    BENCH_SCENARIO ("starts_with", "starts_with a view in one string", k_count,
                    run_starts_with),
    BENCH_SCENARIO ("ends_with", "ends_with a view in one string", k_count,
                    run_ends_with),
    BENCH_SCENARIO ("starts_with_lit", "starts_with a string literal", k_count,
                    run_starts_with_lit),
    BENCH_SCENARIO ("ends_with_lit", "ends_with a string literal", k_count,
                    run_ends_with_lit),
    BENCH_SCENARIO ("starts_with_chr", "starts_with a character", k_count,
                    run_starts_with_chr),
  };
  list.insert (list.end (), starts, starts + 5);
#endif
#if BENCH_HAS_HASH
  scenario_t const hashing[] = {
    BENCH_SCENARIO (
        "hash",
        "hash one string (lumex: std::hash of the converted std::string_view)",
        k_count, run_hash),
  };
  list.insert (list.end (), hashing, hashing + 1);
#endif
#if BENCH_HAS_STD_CONVERSION
  scenario_t const conv[] = {
    BENCH_SCENARIO (
        "to_std",
        "construct a std::string_view from the view (the conversion)", k_count,
        run_to_std),
    BENCH_SCENARIO (
        "from_std",
        "construct the view from a std::string_view (the conversion)", k_count,
        run_from_std),
    BENCH_SCENARIO ("eq_std", "operator== of the view and a std::string_view",
                    k_count, run_eq_std),
    BENCH_SCENARIO ("less_std", "operator< of the view and a std::string_view",
                    k_count, run_less_std),
  };
  list.insert (list.end (), conv, conv + 4);
#endif
  return list;
}

// -- Layout --

// The conversions are tested against the string types of the same character
// type: Str (std::string or std::wstring), Chr (char or wchar_t) and SV (the
// standard view of that character type; a type nothing converts to where the
// standard has none).
struct no_standard_view
{
};

template <typename V, typename Str, typename Chr, typename SV>
void
traits_row (std::ostream &out, char const *name)
{
  out << name << ',' << sizeof (V) << ',' << alignof (V) << ','
      << (std::is_trivially_copyable<V>::value ? 1 : 0) << ','
      << (std::is_trivially_destructible<V>::value ? 1 : 0) << ','
      << (std::is_trivially_default_constructible<V>::value ? 1 : 0) << ','
      << (std::is_standard_layout<V>::value ? 1 : 0) << ','
      << (std::is_nothrow_default_constructible<V>::value ? 1 : 0) << ','
      << (std::is_nothrow_copy_constructible<V>::value ? 1 : 0) << ','
      << (std::is_convertible<Str const &, V>::value ? 1 : 0) << ','
      << (std::is_convertible<Chr const *, V>::value ? 1 : 0) << ','
      << (std::is_constructible<Str, V>::value ? 1 : 0) << ','
      << (std::is_convertible<V, Str>::value ? 1 : 0) << ','
      << (std::is_convertible<V, SV>::value ? 1 : 0) << ','
      << (std::is_convertible<SV, V>::value ? 1 : 0) << '\n';
}

void
write_traits (std::string const &path)
{
  std::ofstream out (path.c_str ());
  out << "type,sizeof,alignof,trivially_copyable,trivially_destructible,"
         "trivially_default_constructible,standard_layout,"
         "nothrow_default_constructible,nothrow_copy_constructible,"
         "from_std_string_implicit,from_cstr_implicit,"
         "to_std_string_constructible,to_std_string_implicit,"
         "to_std_string_view_implicit,from_std_string_view_implicit\n";
#if BENCH_HAS_STD_VIEW
  traits_row<bench::view, std::string, char, std::string_view> (out,
                                                                "string_view");
  traits_row<bench::wview, std::wstring, wchar_t, std::wstring_view> (
      out, "wstring_view");
#else
  traits_row<bench::view, std::string, char, no_standard_view> (out,
                                                                "string_view");
  traits_row<bench::wview, std::wstring, wchar_t, no_standard_view> (
      out, "wstring_view");
#endif
}

// -- Harness --

struct result_t
{
  double median_ns;
  double min_ns;
  double max_ns;
  size_t iterations;
  int repetitions;
  u64 checksum;
};

double
seconds_of (scenario_t const &scenario, size_t iterations)
{
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  u64 const checksum = scenario.run (iterations, scenario.size);
  std::chrono::duration<double> const elapsed
      = std::chrono::steady_clock::now () - start;
  keep_value (checksum);
  return elapsed.count ();
}

result_t
measure (scenario_t const &scenario, double target_seconds, int repetitions)
{
  // Calibrate: double the iteration count until one run lasts long enough.
  size_t iterations = 1;
  while (seconds_of (scenario, iterations) < target_seconds
         && iterations < (size_t (1) << 40))
    iterations *= 2;
  // Warm-up run with the chosen count.
  seconds_of (scenario, iterations);

  std::vector<double> samples;
  for (int r = 0; r < repetitions; ++r)
    samples.push_back (seconds_of (scenario, iterations) * 1e9
                       / (static_cast<double> (iterations)
                          * static_cast<double> (scenario.size)));
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
  u64 const got = scenario.run (8, scenario.size);
  u64 const want = scenario.reference (8, scenario.size);
  if (got != want)
    {
      std::cerr << scenario.name << ": checksum " << got
                << ", the std::string reference " << want << '\n';
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
  std::string output = "string_view_benchmark.csv";
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
  prepare_corpus ();
  prepare_data<view> ();
  prepare_data<std::string> ();
#if BENCH_HAS_STD_CONVERSION
  prepare_std_views<view> ();
#endif

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
      std::cerr << "a scenario disagrees with the std::string reference; "
                   "the numbers would compare different work\n";
      return 1;
    }

  double const target_seconds = quick ? 0.002 : 0.02;
  int const repetitions = quick ? 3 : 15;
  std::ofstream csv (output.c_str ());
  csv << "# impl=" << BENCH_STRING_VIEW_IMPL_NAME
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
      std::printf ("%-20s %10.3f ns/op (min %.3f, max %.3f)\n", scenario.name,
                   result.median_ns, result.min_ns, result.max_ns);
    }
  std::cout << "written " << output << '\n';
  return 0;
}

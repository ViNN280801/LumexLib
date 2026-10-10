// bench_expected.cpp
// One source, three result types: the scenarios below are compiled against the
// expected of this library (BENCH_EXPECTED_IMPL=1), the std::expected of the
// standard library in use (2) or boost::outcome_v2::result of Boost.Outcome
// (3), and every build is its own executable (see bench_expected_impl.hpp and
// CMakeLists.txt). The numbers are nanoseconds per operation, an operation
// being what the description of the scenario says (one construction and
// destruction, one element of an array of 1 024, one call of a chain, ...).
//
// Equal work: every scenario returns a checksum of what it computed from the
// same inputs; run_benchmark.py compares the checksums of all executables and
// stops when two of them computed different things. The compiler is kept from
// deleting or hoisting the work with empty asm statements that read the
// objects built in the loops (and clobber memory) and with inputs that pass
// through asm so that values, pointers and sizes are not known at compile
// time.
//
// Besides the timings the executable writes the size and the properties
// (triviality, standard layout) of a list of types to <output>.sizes.csv.
//
// Usage: LumexExpectedBench_<impl>_cxx<std> [output.csv] [--quick]
//        [--filter text] [--list]
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "bench_expected_impl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define BENCH_NOINLINE __attribute__ ((noinline))
#define BENCH_ALWAYS_INLINE inline __attribute__ ((always_inline))
#else
#error                                                                        \
    "this benchmark needs GCC or Clang (asm statements as optimizer barriers)"
#endif

namespace
{
using bench::err;
using bench::expected;
using bench::ok;

// -- Optimizer barriers --

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

// -- Payloads: what is stored in the value or in the error --

struct int_payload
{
  using type = int;
  static type
  make (std::size_t i)
  {
    return hide (static_cast<int> (i));
  }
  static type
  bump (type &&value)
  {
    return value + 1;
  }
  static std::uint64_t
  weight (type const &value)
  {
    return static_cast<std::uint64_t> (value);
  }
};

/** The error code of the "int" types: unsigned, see ii_t. */
struct code_payload
{
  using type = unsigned;
  static type
  make (std::size_t i)
  {
    return hide (static_cast<unsigned> (i));
  }
  static type
  bump (type &&value)
  {
    return value + 1;
  }
  static std::uint64_t
  weight (type const &value)
  {
    return value;
  }
};

char const k_short_text[] = "abcdefghijkl";
char const k_long_text[]
    = "0123456789abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrs";

struct short_string_payload
{
  using type = std::string;
  static type
  make (std::size_t)
  {
    char const *text = hide (static_cast<char const *> (k_short_text));
    return std::string (text, hide (std::size_t (12)));
  }
  static type
  bump (type &&value)
  {
    return std::move (value);
  }
  static std::uint64_t
  weight (type const &value)
  {
    return value.size ();
  }
};

struct long_string_payload
{
  using type = std::string;
  static type
  make (std::size_t)
  {
    char const *text = hide (static_cast<char const *> (k_long_text));
    return std::string (text, hide (std::size_t (64)));
  }
  static type
  bump (type &&value)
  {
    return std::move (value);
  }
  static std::uint64_t
  weight (type const &value)
  {
    return value.size ();
  }
};

struct big_payload
{
  using type = std::array<int, 16>;
  static type
  make (std::size_t i)
  {
    type value;
    int const base = hide (static_cast<int> (i));
    for (std::size_t k = 0; k < value.size (); ++k)
      value[k] = base + static_cast<int> (k);
    return value;
  }
  static type
  bump (type &&value)
  {
    value[0] += 1;
    return std::move (value);
  }
  static std::uint64_t
  weight (type const &value)
  {
    return static_cast<std::uint64_t> (value[0] + value[15]);
  }
};

// -- Types under test --

// Boost.Outcome refuses a result whose value type and error type are the same
// type, so the error of the "int" types is `unsigned` and the error of the
// "string" types is a struct that holds a string: the same on every side.
struct text_error_t
{
  std::string text;
  text_error_t (std::string value) : text (std::move (value)) {}
};

using ii_t = expected<int, unsigned>;
using si_t = expected<std::string, unsigned>;
using is_t = expected<int, std::string>;
using ss_t = expected<std::string, text_error_t>;
using bi_t = expected<std::array<int, 16>, unsigned>;

// -- Data --

constexpr std::size_t k_array = 1024;
constexpr std::size_t k_array_str = 256;

std::vector<ii_t> g_ii_ok;
std::vector<ii_t> g_ii_err;
std::vector<ii_t> g_ii_mixed;
std::vector<si_t> g_si_ok;
std::vector<si_t> g_si_mixed;

/** One in eight is an error, in a fixed pseudo-random order. */
bool
mixed_is_error (std::size_t i)
{
  std::uint32_t const h = static_cast<std::uint32_t> (i + 1) * 2654435761u;
  return (h >> 29) == 0;
}

void
prepare_data ()
{
  for (std::size_t i = 0; i < k_array; ++i)
    {
      g_ii_ok.push_back (ok<ii_t> (static_cast<int> (i)));
      g_ii_err.push_back (err<ii_t> (static_cast<unsigned> (i)));
      g_ii_mixed.push_back (mixed_is_error (i)
                                ? err<ii_t> (static_cast<unsigned> (i))
                                : ok<ii_t> (static_cast<int> (i)));
    }
  for (std::size_t i = 0; i < k_array_str; ++i)
    {
      g_si_ok.push_back (ok<si_t> (k_short_text));
      g_si_mixed.push_back (mixed_is_error (i)
                                ? err<si_t> (static_cast<unsigned> (i))
                                : ok<si_t> (k_short_text));
    }
}

template <typename X>
X const *
data_of (std::vector<X> const &data)
{
  return hide (data.data ());
}

// -- Scenarios: construction, copy, move --

template <typename X, typename Pay>
X
make_one_impl (std::size_t i, std::true_type /* error */)
{
  return err<X> (Pay::make (i));
}

template <typename X, typename Pay>
X
make_one_impl (std::size_t i, std::false_type /* value */)
{
  return ok<X> (Pay::make (i));
}

/** A value or an error built from the payload (the payload type is the
 * type of the alternative asked for). */
template <typename X, typename Pay, bool IsError>
X
make_one (std::size_t i)
{
  return make_one_impl<X, Pay> (i, std::integral_constant<bool, IsError> ());
}

/** Construct in place from a value or an error, then destroy. */
template <typename X, typename Pay, bool IsError>
std::uint64_t
run_construct (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      X x = make_one<X, Pay, IsError> (i);
      keep_object (x);
      acc += x.has_value () ? 1u : 2u;
    }
  return acc;
}

/** Copy construct from an object that exists, then destroy the copy. */
template <typename X, typename Pay, bool IsError>
std::uint64_t
run_copy (std::size_t iterations)
{
  std::uint64_t acc = 0;
  X const source = make_one<X, Pay, IsError> (7);
  X const *const from = hide (&source);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      X copy (*from);
      keep_object (copy);
      acc += copy.has_value () ? 1u : 2u;
    }
  return acc;
}

/** Construct, move construct from it, destroy both. The difference to
 * run_construct is the move. */
template <typename X, typename Pay, bool IsError>
std::uint64_t
run_move (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < iterations; ++i)
    {
      X first = make_one<X, Pay, IsError> (i);
      X second (std::move (first));
      keep_object (second);
      acc += second.has_value () ? 1u : 2u;
    }
  return acc;
}

/** Copy assignment that switches the alternative both ways: value over
 * error, error over value. Two assignments per iteration. */
template <typename X, typename ValuePay, typename ErrorPay>
std::uint64_t
run_assign_switch (std::size_t iterations)
{
  std::uint64_t acc = 0;
  X const holds_value = ok<X> (ValuePay::make (1));
  X const holds_error = err<X> (ErrorPay::make (2));
  X const *const a = hide (&holds_value);
  X const *const b = hide (&holds_error);
  X target (*a);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      target = *b;
      keep_object (target);
      acc += target.has_value () ? 1u : 2u;
      target = *a;
      keep_object (target);
      acc += target.has_value () ? 1u : 2u;
    }
  return acc;
}

/** Copy assignment of the same alternative: value over value. */
template <typename X, typename Pay>
std::uint64_t
run_assign_same (std::size_t iterations)
{
  std::uint64_t acc = 0;
  X const first = ok<X> (Pay::make (1));
  X const second = ok<X> (Pay::make (2));
  X const *const a = hide (&first);
  X const *const b = hide (&second);
  X target (*a);
  for (std::size_t i = 0; i < iterations; ++i)
    {
      target = (i & 1) ? *a : *b;
      keep_object (target);
      acc += target.has_value () ? 1u : 2u;
    }
  return acc;
}

// -- Scenarios: access --

std::uint64_t
run_value_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_ok);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += static_cast<std::uint64_t> (data[i].value ());
    }
  return acc;
}

std::uint64_t
run_deref_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_ok);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += static_cast<std::uint64_t> (bench::deref (data[i]));
    }
  return acc;
}

std::uint64_t
run_error_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_err);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += static_cast<std::uint64_t> (data[i].error ());
    }
  return acc;
}

std::uint64_t
run_value_str (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      si_t const *const data = data_of (g_si_ok);
      for (std::size_t i = 0; i < k_array_str; ++i)
        acc += data[i].value ().size ();
    }
  return acc;
}

std::uint64_t
run_has_value_ok (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_ok);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += data[i].has_value () ? 1u : 0u;
    }
  return acc;
}

std::uint64_t
run_has_value_mixed (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += data[i].has_value () ? 1u : 0u;
    }
  return acc;
}

/** The usual way to use the result: test, then dereference or take the
 * error. */
std::uint64_t
run_branch_mixed (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        {
          if (data[i].has_value ())
            acc += static_cast<std::uint64_t> (bench::deref (data[i]));
          else
            acc += static_cast<std::uint64_t> (data[i].error ()) * 3u;
        }
    }
  return acc;
}

#if BENCH_EXPECTED_HAS_VALUE_OR
std::uint64_t
run_value_or_mixed (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        acc += static_cast<std::uint64_t> (data[i].value_or (-1));
    }
  return acc;
}
#endif

// -- Scenarios: monadic operations --

#if BENCH_EXPECTED_HAS_MONADIC
std::uint64_t
run_transform_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        {
          ii_t r = data[i].transform ([] (int v) { return v + 1; });
          keep_object (r);
          acc += r.has_value () ? static_cast<std::uint64_t> (bench::deref (r))
                                : 1u;
        }
    }
  return acc;
}

std::uint64_t
run_and_then_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        {
          ii_t r = data[i].and_then (
              [] (int v)
                {
                  return v % 5 == 0 ? err<ii_t> (static_cast<unsigned> (v))
                                    : ok<ii_t> (v + 1);
                });
          keep_object (r);
          acc += r.has_value () ? static_cast<std::uint64_t> (bench::deref (r))
                                : 1u;
        }
    }
  return acc;
}

std::uint64_t
run_or_else_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        {
          ii_t r = data[i].or_else (
              [] (unsigned e) { return ok<ii_t> (static_cast<int> (e) * 2); });
          keep_object (r);
          acc += static_cast<std::uint64_t> (bench::deref (r));
        }
    }
  return acc;
}

std::uint64_t
run_chain_int (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      ii_t const *const data = data_of (g_ii_mixed);
      for (std::size_t i = 0; i < k_array; ++i)
        {
          ii_t r
              = data[i]
                    .transform ([] (int v) { return v + 1; })
                    .and_then (
                        [] (int v)
                          {
                            return v % 5 == 0
                                       ? err<ii_t> (static_cast<unsigned> (v))
                                       : ok<ii_t> (v + 1);
                          })
                    .or_else (
                        [] (unsigned e)
                          { return ok<ii_t> (static_cast<int> (e) * 2); });
          keep_object (r);
          acc += static_cast<std::uint64_t> (bench::deref (r));
        }
    }
  return acc;
}

/** transform on a const lvalue holding a string: the function sees a
 * const reference, no copy of the string. */
std::uint64_t
run_transform_str (std::size_t iterations)
{
  std::uint64_t acc = 0;
  for (std::size_t n = 0; n < iterations; ++n)
    {
      si_t const *const data = data_of (g_si_mixed);
      for (std::size_t i = 0; i < k_array_str; ++i)
        {
          expected<std::size_t, unsigned> r = data[i].transform (
              [] (std::string const &s) { return s.size (); });
          keep_object (r);
          acc += r.has_value () ? bench::deref (r) : 1u;
        }
    }
  return acc;
}
#endif

// -- Scenarios: returning by value through a call chain --

// Four noinline functions, each calls the next and passes the result up:
// the error (one call in eight) is returned as it is, a value is rebuilt
// with bump (). The `Pay::type` travels in an `expected<Pay::type, unsigned>`.

template <typename Pay> struct chain_t
{
  using value_type = typename Pay::type;
  using x_t = expected<value_type, unsigned>;

  static BENCH_NOINLINE x_t
  level3 (std::size_t n)
  {
    keep_value (n);
    if ((n & 7u) == 0)
      return err<x_t> (static_cast<unsigned> (n));
    return ok<x_t> (Pay::make (n));
  }

  static BENCH_NOINLINE x_t
  level2 (std::size_t n)
  {
    x_t r = level3 (n);
    if (!r.has_value ())
      return r;
    return ok<x_t> (Pay::bump (std::move (bench::deref (r))));
  }

  static BENCH_NOINLINE x_t
  level1 (std::size_t n)
  {
    x_t r = level2 (n);
    if (!r.has_value ())
      return r;
    return ok<x_t> (Pay::bump (std::move (bench::deref (r))));
  }

  static BENCH_NOINLINE x_t
  level0 (std::size_t n)
  {
    x_t r = level1 (n);
    if (!r.has_value ())
      return r;
    return ok<x_t> (Pay::bump (std::move (bench::deref (r))));
  }

  static std::uint64_t
  run (std::size_t iterations)
  {
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < iterations; ++i)
      {
        x_t r = level0 (hide (i));
        keep_object (r);
        acc += r.has_value () ? Pay::weight (bench::deref (r))
                              : static_cast<std::uint64_t> (r.error ());
      }
    return acc;
  }
};

// -- The table of scenarios --

typedef std::uint64_t (*run_t) (std::size_t iterations);

struct scenario_t
{
  char const *name;
  char const *group;
  char const *description;
  std::size_t ops_per_iteration;
  run_t run;
};

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> s;
  // Construction (in place) and destruction.
  s.push_back ({ "construct_value_int", "construct",
                 "expected<int, unsigned> built from a value, then destroyed",
                 1, run_construct<ii_t, int_payload, false> });
  s.push_back ({ "construct_error_int", "construct",
                 "expected<int, unsigned> built from an error, then destroyed",
                 1, run_construct<ii_t, code_payload, true> });
  s.push_back ({ "construct_value_big", "construct",
                 "expected<array<int, 16>, unsigned> built from a value", 1,
                 run_construct<bi_t, big_payload, false> });
  s.push_back ({ "construct_value_str12", "construct",
                 "expected<string, unsigned>, 12 characters (no allocation)",
                 1, run_construct<si_t, short_string_payload, false> });
  s.push_back ({ "construct_value_str64", "construct",
                 "expected<string, unsigned>, 64 characters (allocation)", 1,
                 run_construct<si_t, long_string_payload, false> });
  s.push_back ({ "construct_error_str12", "construct",
                 "expected<int, string> from an error, 12 characters", 1,
                 run_construct<is_t, short_string_payload, true> });
  // Copy.
  s.push_back ({ "copy_value_int", "copy",
                 "copy of an expected<int, unsigned> that holds a value", 1,
                 run_copy<ii_t, int_payload, false> });
  s.push_back ({ "copy_error_int", "copy",
                 "copy of an expected<int, unsigned> that holds an error", 1,
                 run_copy<ii_t, code_payload, true> });
  s.push_back ({ "copy_value_big", "copy",
                 "copy of an expected<array<int, 16>, unsigned> with a value",
                 1, run_copy<bi_t, big_payload, false> });
  s.push_back ({ "copy_value_str12", "copy",
                 "copy of an expected<string, unsigned>, 12 characters", 1,
                 run_copy<si_t, short_string_payload, false> });
  s.push_back ({ "copy_value_str64", "copy",
                 "copy of an expected<string, unsigned>, 64 characters", 1,
                 run_copy<si_t, long_string_payload, false> });
  s.push_back ({ "copy_error_str12", "copy",
                 "copy of an expected<int, string> with an error, 12 chars", 1,
                 run_copy<is_t, short_string_payload, true> });
  // Move (includes the construction of the source).
  s.push_back ({ "move_value_int", "move",
                 "construct an expected<int, unsigned> and move it", 1,
                 run_move<ii_t, int_payload, false> });
  s.push_back ({ "move_value_str12", "move",
                 "construct an expected<string, unsigned> (12) and move it", 1,
                 run_move<si_t, short_string_payload, false> });
  s.push_back ({ "move_value_str64", "move",
                 "construct an expected<string, unsigned> (64) and move it", 1,
                 run_move<si_t, long_string_payload, false> });
  s.push_back ({ "move_error_str12", "move",
                 "construct an expected<int, string> (12) and move it", 1,
                 run_move<is_t, short_string_payload, true> });
  // Assignment.
  s.push_back ({ "assign_same_int", "assign",
                 "copy assignment, value over value, expected<int, unsigned>",
                 1, run_assign_same<ii_t, int_payload> });
  s.push_back (
      { "assign_same_str12", "assign",
        "copy assignment, value over value, expected<string, unsigned>", 1,
        run_assign_same<si_t, short_string_payload> });
  s.push_back (
      { "assign_switch_int", "assign",
        "copy assignment that changes the alternative, <int, unsigned>", 2,
        run_assign_switch<ii_t, int_payload, code_payload> });
  s.push_back (
      { "assign_switch_str12", "assign",
        "copy assignment that changes the alternative, "
        "<string, text_error_t>",
        2,
        run_assign_switch<ss_t, short_string_payload, short_string_payload> });
  // Access.
  s.push_back ({ "value_int", "access",
                 "value () of an expected<int, unsigned> that holds a value",
                 k_array, run_value_int });
  s.push_back ({ "deref_int", "access",
                 "operator* of an expected<int, unsigned> that holds a value",
                 k_array, run_deref_int });
  s.push_back ({ "error_int", "access",
                 "error () of an expected<int, unsigned> that holds an error",
                 k_array, run_error_int });
  s.push_back ({ "value_str12", "access",
                 "value ().size () of an expected<string, unsigned>",
                 k_array_str, run_value_str });
  s.push_back ({ "has_value_ok", "access",
                 "has_value () over values only (predictable branch)", k_array,
                 run_has_value_ok });
  s.push_back ({ "has_value_mixed", "access",
                 "has_value () over 1/8 errors in a fixed random order",
                 k_array, run_has_value_mixed });
  s.push_back ({ "branch_mixed", "access",
                 "if (has_value ()) *x else x.error (), 1/8 errors", k_array,
                 run_branch_mixed });
#if BENCH_EXPECTED_HAS_VALUE_OR
  s.push_back ({ "value_or_mixed", "access", "value_or (-1), 1/8 errors",
                 k_array, run_value_or_mixed });
#endif
#if BENCH_EXPECTED_HAS_MONADIC
  s.push_back ({ "transform_int", "monadic",
                 "transform (v + 1) on <int, unsigned>, 1/8 errors", k_array,
                 run_transform_int });
  s.push_back (
      { "and_then_int", "monadic",
        "and_then (returns an expected) on <int, unsigned>, 1/8 errors",
        k_array, run_and_then_int });
  s.push_back ({ "or_else_int", "monadic",
                 "or_else (recovers the error) on <int, unsigned>, 1/8 errors",
                 k_array, run_or_else_int });
  s.push_back ({ "chain_int", "monadic",
                 "transform, and_then, or_else in a row, <int, unsigned>",
                 k_array, run_chain_int });
  s.push_back ({ "transform_str12", "monadic",
                 "transform (s.size ()) on <string, unsigned>, const lvalue",
                 k_array_str, run_transform_str });
#endif
  // Returning by value through four noinline calls.
  s.push_back ({ "return_chain_int", "return",
                 "4 nested calls return expected<int, unsigned> by value", 1,
                 chain_t<int_payload>::run });
  s.push_back ({ "return_chain_str12", "return",
                 "4 nested calls return expected<string, unsigned> (12)", 1,
                 chain_t<short_string_payload>::run });
  s.push_back ({ "return_chain_str64", "return",
                 "4 nested calls return expected<string, unsigned> (64)", 1,
                 chain_t<long_string_payload>::run });
  s.push_back ({ "return_chain_big", "return",
                 "4 nested calls return expected<array<int, 16>, unsigned>", 1,
                 chain_t<big_payload>::run });
  return s;
}

// -- Sizes and properties --

struct empty_t
{
};

using array16_t = std::array<int, 16>;

struct size_row_t
{
  std::string type;
  std::size_t size;
  std::size_t align;
  bool trivially_copyable;
  bool trivially_destructible;
  bool standard_layout;
  bool nothrow_move;
};

template <typename X>
size_row_t
size_row (char const *name)
{
  size_row_t row;
  row.type = name;
  row.size = sizeof (X);
  row.align = alignof (X);
  row.trivially_copyable = std::is_trivially_copyable<X>::value;
  row.trivially_destructible = std::is_trivially_destructible<X>::value;
  row.standard_layout = std::is_standard_layout<X>::value;
  row.nothrow_move = std::is_nothrow_move_constructible<X>::value;
  return row;
}

#define BENCH_SIZE_ROW(T, E)                                                  \
  rows.push_back (size_row<expected<T, E>> ("<" #T ", " #E ">"))

std::vector<size_row_t>
make_size_rows ()
{
  std::vector<size_row_t> rows;
  BENCH_SIZE_ROW (char, unsigned char);
  BENCH_SIZE_ROW (int, unsigned);
  BENCH_SIZE_ROW (long long, unsigned long long);
  BENCH_SIZE_ROW (double, int);
  BENCH_SIZE_ROW (int *, int);
  BENCH_SIZE_ROW (empty_t, int);
  BENCH_SIZE_ROW (int, std::error_code);
  BENCH_SIZE_ROW (std::string, int);
  BENCH_SIZE_ROW (int, std::string);
  BENCH_SIZE_ROW (std::string, text_error_t);
  BENCH_SIZE_ROW (std::string, std::error_code);
  BENCH_SIZE_ROW (array16_t, int);
  BENCH_SIZE_ROW (std::unique_ptr<int>, int);
#if BENCH_EXPECTED_HAS_VOID
  BENCH_SIZE_ROW (void, int);
  BENCH_SIZE_ROW (void, std::error_code);
  BENCH_SIZE_ROW (void, std::string);
#endif
  return rows;
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
  std::uint64_t const checksum = scenario.run (iterations);
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

  double const ops = static_cast<double> (iterations)
                     * static_cast<double> (scenario.ops_per_iteration);
  std::vector<double> samples;
  for (int r = 0; r < repetitions; ++r)
    samples.push_back (seconds_of (scenario, iterations) * 1e9 / ops);
  std::sort (samples.begin (), samples.end ());
  result_t result;
  result.median_ns = samples[samples.size () / 2];
  result.min_ns = samples.front ();
  result.max_ns = samples.back ();
  result.iterations = iterations;
  result.repetitions = repetitions;
  result.checksum = scenario.run (8);
  return result;
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
#else
  std::snprintf (buffer, sizeof (buffer), "GCC %d.%d.%d", __GNUC__,
                 __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
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
#else
  return "unknown";
#endif
}
} // namespace

int
main (int argc, char **argv)
{
  std::string output = "expected_benchmark.csv";
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
      if (!filter.empty ()
          && std::string (scenario.name).find (filter) == std::string::npos)
        continue;
      scenarios.push_back (scenario);
    }
  if (list)
    {
      for (scenario_t const &scenario : scenarios)
        std::cout << scenario.name << '\n';
      return 0;
    }

  double const target_seconds = quick ? 0.002 : 0.02;
  int const repetitions = quick ? 3 : 15;
  std::ofstream csv (output.c_str ());
  csv << "# impl=" << BENCH_EXPECTED_IMPL_NAME
      << " compiler=" << compiler_name () << " library=" << library_name ()
      << " cplusplus=" << __cplusplus
#if defined(NDEBUG)
      << " build=release"
#else
      << " build=debug"
#endif
      << '\n';
  csv << "scenario,group,description,median_ns,min_ns,max_ns,iterations,"
         "repetitions,checksum\n";
  for (scenario_t const &scenario : scenarios)
    {
      result_t const result = measure (scenario, target_seconds, repetitions);
      csv << scenario.name << ',' << scenario.group << ",\""
          << csv_quoted (scenario.description) << "\"," << result.median_ns
          << ',' << result.min_ns << ',' << result.max_ns << ','
          << result.iterations << ',' << result.repetitions << ','
          << result.checksum << '\n';
      std::printf ("%-24s %10.3f ns/op (min %.3f, max %.3f)\n", scenario.name,
                   result.median_ns, result.min_ns, result.max_ns);
    }
  csv.close ();

  std::string sizes_path = output;
  if (sizes_path.size () > 4
      && sizes_path.compare (sizes_path.size () - 4, 4, ".csv") == 0)
    sizes_path.erase (sizes_path.size () - 4);
  sizes_path += ".sizes.csv";
  std::ofstream sizes (sizes_path.c_str ());
  sizes << "type,sizeof,alignof,trivially_copyable,trivially_destructible,"
           "standard_layout,nothrow_move_constructible\n";
  for (size_row_t const &row : make_size_rows ())
    sizes << '"' << csv_quoted (row.type) << "\"," << row.size << ','
          << row.align << ',' << row.trivially_copyable << ','
          << row.trivially_destructible << ',' << row.standard_layout << ','
          << row.nothrow_move << '\n';
  std::cout << "written " << output << " and " << sizes_path << '\n';
  return 0;
}

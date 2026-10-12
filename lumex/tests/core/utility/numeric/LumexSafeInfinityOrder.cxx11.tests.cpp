// LumexSafeInfinityOrder.cxx11.tests.cpp
// An infinity is ordered by its sign everywhere in the safe comparator:
// -inf < every finite value < +inf, +inf == +inf, -inf == -inf, for every pair
// of arithmetic types (an integer against a floating-point type, a
// floating-point type against another one, long double included), in
// safe_three_way_compare and in the six comparisons (safe_less ..
// safe_not_equal, safe_compare) alike. NaN stays unordered: every comparison
// is false except safe_not_equal, and the three-way result is unordered.
//
// The table is every pair of {NaN, -inf, min, -1, -0, 0, 1, max, +inf} over
// int, unsigned, long long, unsigned long long, float, double and long double
// (49 type pairs). An integer type has no NaN, no infinity and no -0, and an
// unsigned one no -1; `min` is the lowest value of the type (the most
// negative one, so for a floating-point type -max and not the smallest
// positive number).
//
// The reference is mathematical and converts nothing to a common type:
//  - a NaN against anything is unordered;
//  - an infinity is its sign (-1, +1) and a finite number is 0, so any pair
//    with an infinity is ordered by those signs, and two equal infinities are
//    equal;
//  - two integers compare as a sign and a magnitude (integer_order);
//  - two floating-point values compare by the language operators on long
//    double, to which float and double widen without rounding;
//  - an integer against a finite floating-point value compares as a sign, a
//    whole part and "has a fractional part", with the whole part taken from
//    the floating-point value exactly (a floating-point value of 2^64 or more
//    is larger in magnitude than every integer of up to 64 bits).
// The same expectations run through the free functions, the members of a plain
// comparator and, except for long double (std::atomic<long double> needs
// libatomic on x86-64 GCC, which these tests do not link), of an atomic one.

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/numeric/LumexOrdering.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"
#include "lumex/tests/core/utility/numeric/LumexNumericSamples.hpp"

using namespace lumex::core::utility::numeric;
using namespace lumex_numeric_samples;

namespace
{
typedef type_list<int, unsigned int, long long, unsigned long long>
    table_integers;
typedef type_list<float, double, long double> table_floats;
typedef type_list<int, unsigned int, long long, unsigned long long, float,
                  double, long double>
    table_types;

// === The values of the table ===

// NaN, -inf, min, -1, -0, 0, 1, max, +inf of a floating-point type.
template <typename T>
void
fill_table (std::vector<T> &values, std::true_type /* floating */)
{
  values.push_back (std::numeric_limits<T>::quiet_NaN ());
  values.push_back (-std::numeric_limits<T>::infinity ());
  values.push_back (std::numeric_limits<T>::lowest ());
  values.push_back (static_cast<T> (-1));
  values.push_back (static_cast<T> (-0.0));
  values.push_back (static_cast<T> (0));
  values.push_back (static_cast<T> (1));
  values.push_back ((std::numeric_limits<T>::max) ());
  values.push_back (std::numeric_limits<T>::infinity ());
}

// min, -1 (signed only), 0, 1, max of an integer type.
template <typename T>
void
fill_table (std::vector<T> &values, std::false_type /* integer */)
{
  if (std::is_signed<T>::value)
    {
      values.push_back ((std::numeric_limits<T>::min) ());
      values.push_back (static_cast<T> (-1));
    }
  values.push_back (static_cast<T> (0));
  values.push_back (static_cast<T> (1));
  values.push_back ((std::numeric_limits<T>::max) ());
}

template <typename T>
std::vector<T>
table_values ()
{
  std::vector<T> values;
  fill_table (values, typename std::is_floating_point<T>::type ());
  return values;
}

// === The reference ===

// A number as the reference sees it.
struct number
{
  bool is_float;
  bool nan;
  int infinity;  // -1, 0 (finite) or +1
  bool negative; // finite: below zero (not -0)
  bool beyond;   // finite integer-sized view: 2^64 or more in magnitude
  unsigned long long whole;
  bool fraction;
  long double widened; // a floating-point value as long double
};

template <typename T>
number
describe_number (T value, std::false_type /* integer */)
{
  number result = number ();
  result.is_float = false;
  exact_int const exact = make_exact (value);
  result.negative = exact.negative;
  result.whole = exact.magnitude;
  return result;
}

template <typename T>
number
describe_number (T value, std::true_type /* floating */)
{
  number result = number ();
  result.is_float = true;
  long double const widened = static_cast<long double> (value);
  result.widened = widened;
  result.nan = std::isnan (widened);
  if (result.nan)
    return result;
  if (std::isinf (widened))
    {
      result.infinity = widened > 0 ? 1 : -1;
      return result;
    }
  result.negative = widened < 0;
  long double const magnitude = result.negative ? -widened : widened;
  result.beyond = magnitude >= std::ldexp (1.0L, 64);
  if (!result.beyond)
    {
      long double const whole = std::floor (magnitude);
      result.whole = static_cast<unsigned long long> (whole);
      result.fraction = magnitude > whole;
    }
  return result;
}

int
three_valued (int lhs, int rhs)
{
  return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

// -1, 0, 1 for the order of two finite numbers, at least one of them an
// integer; the whole part and the fraction decide, the type does not.
int
order_of_finite_with_integer (number const &lhs, number const &rhs)
{
  bool const lhs_zero = !lhs.beyond && lhs.whole == 0 && !lhs.fraction;
  bool const rhs_zero = !rhs.beyond && rhs.whole == 0 && !rhs.fraction;
  int const lhs_sign = lhs_zero ? 0 : (lhs.negative ? -1 : 1);
  int const rhs_sign = rhs_zero ? 0 : (rhs.negative ? -1 : 1);
  if (lhs_sign != rhs_sign)
    return three_valued (lhs_sign, rhs_sign);
  if (lhs_sign == 0)
    return 0;
  int by_magnitude = 0;
  if (lhs.beyond != rhs.beyond)
    by_magnitude = lhs.beyond ? 1 : -1;
  else if (lhs.whole != rhs.whole)
    by_magnitude = lhs.whole < rhs.whole ? -1 : 1;
  else if (lhs.fraction != rhs.fraction)
    by_magnitude = lhs.fraction ? 1 : -1;
  return lhs_sign < 0 ? -by_magnitude : by_magnitude;
}

// -1, 0, 1, or 2 for unordered.
int
reference_order (number const &lhs, number const &rhs)
{
  if (lhs.nan || rhs.nan)
    return 2;
  if (lhs.infinity != 0 || rhs.infinity != 0)
    return three_valued (lhs.infinity, rhs.infinity);
  if (lhs.is_float && rhs.is_float)
    return float_order (lhs.widened, rhs.widened);
  return order_of_finite_with_integer (lhs, rhs);
}

template <typename T, typename U>
int
reference_code (T lhs, U rhs)
{
  return reference_order (
      describe_number (lhs, typename std::is_floating_point<T>::type ()),
      describe_number (rhs, typename std::is_floating_point<U>::type ()));
}

// === The checks ===

template <typename T, typename U>
void
expect_flag (mismatch_report &report, char const *what, T lhs, U rhs,
             bool expected, bool actual)
{
  if (expected != actual)
    report.add (std::string (what) + ": " + typeid (T).name () + " "
                + describe (lhs) + " vs " + typeid (U).name () + " "
                + describe (rhs) + " expected "
                + (expected ? "true" : "false"));
}

template <typename T, typename U>
void
expect_code (mismatch_report &report, char const *what, T lhs, U rhs,
             int expected, int actual)
{
  if (expected != actual)
    report.add (std::string (what) + ": " + typeid (T).name () + " "
                + describe (lhs) + " <=> " + typeid (U).name () + " "
                + describe (rhs) + " expected " + describe (expected) + " got "
                + describe (actual));
}

// The six comparisons and the three-way result of one comparator (the plain or
// the atomic one) against the code of the exact order.
template <typename Comparator, typename T, typename U>
void
check_member (mismatch_report &report, char const *what, T lhs, U rhs,
              int code)
{
  Comparator const comparator (lhs);
  bool const less = code == -1;
  bool const equal = code == 0;
  bool const greater = code == 1;
  expect_flag (report, what, lhs, rhs, less, comparator.safe_less (rhs));
  expect_flag (report, what, lhs, rhs, less || equal,
               comparator.safe_less_equal (rhs));
  expect_flag (report, what, lhs, rhs, greater, comparator.safe_greater (rhs));
  expect_flag (report, what, lhs, rhs, greater || equal,
               comparator.safe_greater_equal (rhs));
  expect_flag (report, what, lhs, rhs, greater || equal,
               comparator.safe_compare (rhs));
  expect_flag (report, what, lhs, rhs, equal, comparator.safe_equal (rhs));
  expect_flag (report, what, lhs, rhs, !equal,
               comparator.safe_not_equal (rhs));
  expect_code (report, what, lhs, rhs, code,
               ordering_code (comparator.safe_three_way_compare (rhs)));
}

template <typename T, typename U>
void
check_atomic_member (mismatch_report &report, T lhs, U rhs, int code,
                     std::true_type /* usable */)
{
  check_member<safe_comparator<T, true>> (report, "atomic member", lhs, rhs,
                                          code);
}

template <typename T, typename U>
void
check_atomic_member (mismatch_report &, T, U, int, std::false_type)
{
}

template <typename T, typename U>
void
check_pair (mismatch_report &report, T lhs, U rhs)
{
  int const code = reference_code (lhs, rhs);
  bool const less = code == -1;
  bool const equal = code == 0;
  bool const greater = code == 1;

  expect_flag (report, "safe_less", lhs, rhs, less, safe_less (lhs, rhs));
  expect_flag (report, "safe_less_equal", lhs, rhs, less || equal,
               safe_less_equal (lhs, rhs));
  expect_flag (report, "safe_greater", lhs, rhs, greater,
               safe_greater (lhs, rhs));
  expect_flag (report, "safe_greater_equal", lhs, rhs, greater || equal,
               safe_greater_equal (lhs, rhs));
  expect_flag (report, "safe_compare", lhs, rhs, greater || equal,
               safe_compare (lhs, rhs));
  expect_flag (report, "safe_equal", lhs, rhs, equal, safe_equal (lhs, rhs));
  expect_flag (report, "safe_not_equal", lhs, rhs, !equal,
               safe_not_equal (lhs, rhs));
  expect_code (report, "safe_three_way_compare", lhs, rhs, code,
               ordering_code (safe_three_way_compare (lhs, rhs)));

  check_member<safe_comparator<T, false>> (report, "plain member", lhs, rhs,
                                           code);
  check_atomic_member (
      report, lhs, rhs, code,
      std::integral_constant<bool, !std::is_same<T, long double>::value> ());
}

template <typename T, typename U> struct table_check
{
  static void
  run (mismatch_report &report)
  {
    std::vector<T> const lhs = table_values<T> ();
    std::vector<U> const rhs = table_values<U> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        check_pair (report, lhs[i], rhs[j]);
  }
};

template <typename FirstList, typename SecondList>
void
expect_table ()
{
  mismatch_report report;
  for_each_pair<table_check, FirstList, SecondList, mismatch_report>::run (
      report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}
} // namespace

// === The whole table ===

TEST (LumexSafeInfinityOrder,
      GivenIntegerAgainstInteger_WhenCompared_ThenTheExactOrder)
{
  expect_table<table_integers, table_integers> ();
}

TEST (LumexSafeInfinityOrder,
      GivenFloatAgainstFloat_WhenCompared_ThenInfinitiesBySign)
{
  expect_table<table_floats, table_floats> ();
}

TEST (LumexSafeInfinityOrder,
      GivenIntegerAgainstFloat_WhenCompared_ThenInfinitiesBySign)
{
  expect_table<table_integers, table_floats> ();
}

TEST (LumexSafeInfinityOrder,
      GivenFloatAgainstInteger_WhenCompared_ThenInfinitiesBySign)
{
  expect_table<table_floats, table_integers> ();
}

TEST (LumexSafeInfinityOrder, GivenTheTable_WhenCounted_ThenItHasSevenTypes)
{
  // 7 types, so 49 pairs of types; the floating-point tables have the nine
  // values, the signed integers five and the unsigned ones three.
  EXPECT_EQ (table_values<double> ().size (), 9U);
  EXPECT_EQ (table_values<int> ().size (), 5U);
  EXPECT_EQ (table_values<unsigned int> ().size (), 3U);
  static_assert (
      std::is_same<table_types,
                   type_list<int, unsigned int, long long, unsigned long long,
                             float, double, long double>>::value,
      "the seven types");
}

// === Readable cases (the examples of the CHANGELOG) ===

TEST (LumexSafeInfinityOrder,
      GivenInfinityAgainstAnotherFloatType_WhenCompared_ThenOrderedBySign)
{
  float const finf = std::numeric_limits<float>::infinity ();
  double const dinf = std::numeric_limits<double>::infinity ();
  long double const linf = std::numeric_limits<long double>::infinity ();

  EXPECT_TRUE (safe_three_way_compare (finf, 1.0)
               == partial_ordering_t::greater);
  EXPECT_TRUE (safe_three_way_compare (-finf, 1.0)
               == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (1.0f, dinf)
               == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (finf, dinf)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (-finf, dinf)
               == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (linf, dinf)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (-dinf, linf)
               == partial_ordering_t::less);

  EXPECT_TRUE (safe_greater (finf, 1.0));
  EXPECT_TRUE (safe_less (-finf, 1.0));
  EXPECT_TRUE (safe_less (1.0f, dinf));
  EXPECT_TRUE (safe_greater (dinf, 1.0f));
  EXPECT_TRUE (safe_less_equal (finf, dinf));
  EXPECT_TRUE (safe_greater_equal (finf, dinf));
  EXPECT_TRUE (safe_equal (finf, dinf));
  EXPECT_FALSE (safe_not_equal (finf, dinf));
  EXPECT_FALSE (safe_less (finf, dinf));
  EXPECT_FALSE (safe_greater (finf, dinf));
  EXPECT_FALSE (safe_equal (finf, -dinf));
  EXPECT_TRUE (safe_greater (finf, -dinf));
}

TEST (LumexSafeInfinityOrder,
      GivenInfinityAgainstInteger_WhenCompared_ThenOrderedBySign)
{
  double const dinf = std::numeric_limits<double>::infinity ();
  float const finf = std::numeric_limits<float>::infinity ();

  EXPECT_TRUE (safe_three_way_compare (7, dinf) == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (7, -dinf)
               == partial_ordering_t::greater);
  EXPECT_TRUE (safe_three_way_compare (finf, 7)
               == partial_ordering_t::greater);
  EXPECT_TRUE (safe_three_way_compare (-finf, 7) == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (
                   (std::numeric_limits<unsigned long long>::max) (), dinf)
               == partial_ordering_t::less);
  EXPECT_TRUE (
      safe_three_way_compare ((std::numeric_limits<long long>::min) (), -dinf)
      == partial_ordering_t::greater);
  EXPECT_TRUE (safe_less (7, dinf));
  EXPECT_TRUE (safe_greater (finf, 7));
  EXPECT_FALSE (safe_equal (7, dinf));
  EXPECT_TRUE (safe_not_equal (7, dinf));
}

TEST (LumexSafeInfinityOrder,
      GivenNaN_WhenCompared_ThenEveryComparisonIsFalseButNotEqual)
{
  double const nan_value = std::numeric_limits<double>::quiet_NaN ();
  float const nan_float = std::numeric_limits<float>::quiet_NaN ();
  double const dinf = std::numeric_limits<double>::infinity ();

  EXPECT_TRUE (safe_three_way_compare (nan_value, dinf)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (dinf, nan_float)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (nan_float, 7)
               == partial_ordering_t::unordered);
  EXPECT_FALSE (safe_less (nan_value, dinf));
  EXPECT_FALSE (safe_greater (nan_value, dinf));
  EXPECT_FALSE (safe_less_equal (nan_float, dinf));
  EXPECT_FALSE (safe_greater_equal (dinf, nan_float));
  EXPECT_FALSE (safe_equal (nan_value, nan_value));
  EXPECT_TRUE (safe_not_equal (nan_value, nan_value));
  EXPECT_TRUE (safe_not_equal (7, nan_float));
}

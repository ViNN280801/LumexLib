// LumexSafeThreeWayCompare.cxx20.tests.cpp
// The three-way part of the safe comparator at C++20, where its result is
// still the class of this library (never the standard one): the result types
// are the own strong_ordering and partial_ordering; converted to the standard
// classes, the results equal the language <=> for two values of one type and
// std::cmp_less for two integers of any types; the helpers
// is_less .. is_not_equal agree with std::is_lt .. std::is_neq; and the
// ordering classes of LumexOrdering.hpp give the same answers as the standard
// ones over a table of values with NaN, the infinities and both zeros. GCC 8
// accepts -std=c++2a without <compare>, so there the tests skip.

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/numeric/LumexOrdering.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"
#include "lumex/tests/core/utility/numeric/LumexNumericSamples.hpp"

#if LUMEX_HAS_THREE_WAY_COMPARISON

namespace own = lumex::core::utility::numeric;
using namespace lumex_numeric_samples;

namespace
{
// The class of this library with the value of a standard one.
own::partial_ordering
to_own (std::partial_ordering value)
{
  if (std::is_lt (value))
    return own::partial_ordering::less;
  if (std::is_gt (value))
    return own::partial_ordering::greater;
  if (std::is_eq (value))
    return own::partial_ordering::equivalent;
  return own::partial_ordering::unordered;
}

own::strong_ordering
to_own (std::strong_ordering value)
{
  if (std::is_lt (value))
    return own::strong_ordering::less;
  if (std::is_gt (value))
    return own::strong_ordering::greater;
  return own::strong_ordering::equal;
}

// The answers to the comparisons with 0 in both operand orders as bits.
template <typename Ordering>
unsigned
zero_signature (Ordering const &value)
{
  unsigned signature = 0;
  unsigned bit = 1;
  bool const answers[] = { value == 0, 0 == value, value != 0, 0 != value,
                           value < 0,  0 < value,  value <= 0, 0 <= value,
                           value > 0,  0 > value,  value >= 0, 0 >= value };
  for (std::size_t i = 0; i < sizeof (answers) / sizeof (answers[0]); ++i)
    {
      if (answers[i])
        signature |= bit;
      bit <<= 1;
    }
  return signature;
}

template <template <typename, typename> class Check, typename FirstList,
          typename SecondList>
void
expect_no_mismatch ()
{
  mismatch_report report;
  for_each_pair<Check, FirstList, SecondList, mismatch_report>::run (report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}

template <typename T, typename U, typename Result>
void
add_if_differs (mismatch_report &report, char const *what, T lhs, U rhs,
                bool differs, Result const &)
{
  if (differs)
    report.add (std::string (what) + ": " + describe (lhs) + " <=> "
                + describe (rhs));
}

// The helpers of the comparator against those of <compare>, for one result.
template <typename T, typename U, typename Result>
void
check_helpers (mismatch_report &report, T lhs, U rhs, Result const &result)
{
  add_if_differs (report, "is_less", lhs, rhs,
                  own::is_less (result) != std::is_lt (result), result);
  add_if_differs (report, "is_equal", lhs, rhs,
                  own::is_equal (result) != std::is_eq (result), result);
  add_if_differs (report, "is_greater", lhs, rhs,
                  own::is_greater (result) != std::is_gt (result), result);
  add_if_differs (report, "is_less_equal", lhs, rhs,
                  own::is_less_equal (result) != std::is_lteq (result),
                  result);
  add_if_differs (report, "is_greater_equal", lhs, rhs,
                  own::is_greater_equal (result) != std::is_gteq (result),
                  result);
  add_if_differs (report, "is_not_equal", lhs, rhs,
                  own::is_not_equal (result) != std::is_neq (result), result);
}

// Integers of two types: std::cmp_less is the reference, <=> where the types
// are the same.
template <typename T, typename U> struct integer_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same_v<decltype (own::safe_three_way_compare (
                                      std::declval<T> (), std::declval<U> ())),
                                  own::strong_ordering>,
                   "two integers give the own strong_ordering");
    static_assert (std::is_same_v<own::three_way_comparison_result_t<T, U>,
                                  own::strong_ordering>,
                   "the trait gives the own strong_ordering");
    static_assert (
        std::is_same_v<decltype (own::safe_comparator<T> (T ())
                                     .safe_three_way_compare (U ())),
                       own::strong_ordering>,
        "the member gives the own strong_ordering");

    std::vector<T> const lhs = integer_samples<T> ();
    std::vector<U> const rhs = integer_samples<U> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        {
          T const a = lhs[i];
          U const b = rhs[j];
          std::strong_ordering const actual
              = own::safe_three_way_compare (a, b);
          std::strong_ordering const expected
              = std::cmp_less (a, b)   ? std::strong_ordering::less
                : std::cmp_less (b, a) ? std::strong_ordering::greater
                                       : std::strong_ordering::equal;
          add_if_differs (report, "cmp_less", a, b, actual != expected,
                          actual);
          if constexpr (std::is_same_v<T, U>)
            add_if_differs (report, "<=>", a, b, actual != (a <=> b), actual);
          add_if_differs (
              report, "member", a, b,
              actual != own::safe_comparator<T> (a).safe_three_way_compare (b),
              actual);
          check_helpers (report, a, b, actual);
          // The class of this library, made from the standard value, answers
          // as the standard value does.
          add_if_differs (report, "own class", a, b,
                          zero_signature (to_own (actual))
                              != zero_signature (actual),
                          actual);
        }
  }
};

// Floating-point values of two types: <=> on the values widened to long double
// is the reference. An infinity is ordered by its sign for every pair of types
// (see LumexSafeInfinityOrder.cxx11).
template <typename F, typename G> struct float_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same_v<decltype (own::safe_three_way_compare (
                                      std::declval<F> (), std::declval<G> ())),
                                  own::partial_ordering>,
                   "two floating-point types give the own partial_ordering");

    std::vector<F> const lhs = float_samples<F> ();
    std::vector<G> const rhs = float_samples<G> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        {
          F const a = lhs[i];
          G const b = rhs[j];
          std::partial_ordering const actual
              = own::safe_three_way_compare (a, b);
          std::partial_ordering const expected
              = static_cast<long double> (a) <=> static_cast<long double> (b);
          add_if_differs (report, "<=>", a, b, actual != expected, actual);
          add_if_differs (report, "own class", a, b,
                          zero_signature (to_own (expected))
                              != zero_signature (actual),
                          actual);
          check_helpers (report, a, b, actual);
        }
  }
};

template <typename I, typename F> struct mixed_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same_v<decltype (own::safe_three_way_compare (
                                      std::declval<I> (), std::declval<F> ())),
                                  own::partial_ordering>,
                   "integer against floating-point gives partial_ordering");
    static_assert (std::is_same_v<decltype (own::safe_three_way_compare (
                                      std::declval<F> (), std::declval<I> ())),
                                  own::partial_ordering>,
                   "floating-point against integer gives partial_ordering");

    std::vector<I> const integers = exactly_representable_integers<I, F> ();
    std::vector<F> const floats = mixed_float_samples<F> ();
    for (std::size_t i = 0; i < integers.size (); ++i)
      for (std::size_t j = 0; j < floats.size (); ++j)
        {
          I const a = integers[i];
          F const b = floats[j];
          std::partial_ordering const expected
              = static_cast<long double> (a) <=> static_cast<long double> (b);
          std::partial_ordering const forward
              = own::safe_three_way_compare (a, b);
          std::partial_ordering const backward
              = own::safe_three_way_compare (b, a);
          add_if_differs (report, "<=> forward", a, b, forward != expected,
                          forward);
          add_if_differs (report, "<=> backward", b, a,
                          backward
                              != (static_cast<long double> (b)
                                  <=> static_cast<long double> (a)),
                          backward);
          check_helpers (report, a, b, forward);
          check_helpers (report, b, a, backward);
        }
  }
};
} // namespace

#endif // LUMEX_HAS_THREE_WAY_COMPARISON

TEST (LumexSafeThreeWayStd,
      GivenCompare_WhenResultTypesAsked_ThenTheOwnClassesNotTheStandardOnes)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  static_assert (std::is_same_v<decltype (own::safe_three_way_compare (1, 2)),
                                own::strong_ordering>);
  static_assert (
      std::is_same_v<decltype (own::safe_three_way_compare (1u, -2)),
                     own::strong_ordering>);
  static_assert (
      std::is_same_v<decltype (own::safe_three_way_compare (1.0, 2.0)),
                     own::partial_ordering>);
  static_assert (
      std::is_same_v<decltype (own::safe_three_way_compare (1, 2.0f)),
                     own::partial_ordering>);
  static_assert (std::is_same_v<own::three_way_comparison_result_t<int, long>,
                                own::strong_ordering>);
  static_assert (
      std::is_same_v<own::three_way_comparison_result_t<float, long long>,
                     own::partial_ordering>);
  static_assert (
      std::is_same_v<own::three_way_comparison_result_t<double, float>,
                     own::partial_ordering>);
  static_assert (!std::is_same_v<decltype (own::safe_three_way_compare (1, 2)),
                                 std::strong_ordering>);
  static_assert (
      !std::is_same_v<decltype (own::safe_three_way_compare (1.0, 2.0)),
                      std::partial_ordering>);
  // They convert to the standard classes (and back), so code written against
  // the standard result keeps compiling.
  std::strong_ordering const asStrong = own::safe_three_way_compare (1, 2);
  std::partial_ordering const asPartial = own::safe_three_way_compare (
      1.0, std::numeric_limits<double>::quiet_NaN ());
  EXPECT_EQ (asStrong, std::strong_ordering::less);
  EXPECT_EQ (asPartial, std::partial_ordering::unordered);
  EXPECT_TRUE (own::safe_three_way_compare (1, 2)
               == std::strong_ordering::less);
  EXPECT_TRUE (std::strong_ordering::less
               == own::safe_three_way_compare (1, 2));
  EXPECT_TRUE (std::is_lt (own::safe_three_way_compare (1, 2)));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (
    LumexSafeThreeWayStd,
    GivenEveryPairOfIntegerTypes_WhenCompared_ThenTheStandardIntegerComparison)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON                                            \
    && defined(__cpp_lib_integer_comparison_functions)
  expect_no_mismatch<integer_pair_check, integer_types, integer_types> ();
#else
  GTEST_SKIP () << "the toolchain has no std::cmp_less";
#endif
}

TEST (LumexSafeThreeWayStd,
      GivenEveryPairOfFloatTypes_WhenCompared_ThenTheLanguageComparison)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  expect_no_mismatch<float_pair_check, float_types, float_types> ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexSafeThreeWayStd,
      GivenIntegerAndFloat_WhenComparedBothWays_ThenTheLanguageComparison)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  expect_no_mismatch<mixed_pair_check, integer_types, float_types> ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (
    LumexSafeThreeWayStd,
    GivenTableWithNaN_WhenOrderingClassesMadeFromStandardOnes_ThenSameAnswers)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  std::vector<double> const values = float_samples<double> ();
  std::size_t nan_pairs = 0;
  for (std::size_t i = 0; i < values.size (); ++i)
    for (std::size_t j = 0; j < values.size (); ++j)
      {
        std::partial_ordering const standard = values[i] <=> values[j];
        own::partial_ordering const mine = to_own (standard);

        // The class of this library, read through its own named values,
        // matches the language comparison made from the same operands.
        EXPECT_EQ (mine == own::partial_ordering::unordered,
                   standard == std::partial_ordering::unordered);
        EXPECT_EQ (mine == own::partial_ordering::less, values[i] < values[j]);
        EXPECT_EQ (mine == own::partial_ordering::greater,
                   values[i] > values[j]);
        EXPECT_EQ (mine == own::partial_ordering::equivalent,
                   values[i] == values[j]);
        EXPECT_EQ (zero_signature (mine), zero_signature (standard));
        EXPECT_EQ (own::is_lteq (mine), values[i] <= values[j]);
        EXPECT_EQ (own::is_gteq (mine), values[i] >= values[j]);
        EXPECT_EQ (own::is_neq (mine), values[i] != values[j]);
        if (mine == own::partial_ordering::unordered)
          ++nan_pairs;
      }
  // NaN against each of the 17 values, both ways, is 33 unordered pairs.
  EXPECT_EQ (nan_pairs, 33U);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexSafeThreeWayStd,
      GivenHelpersOfTheComparator_WhenGivenOrderingsOfTheLibrary_ThenAccepted)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  // The helpers take the standard classes and the classes of this library.
  EXPECT_TRUE (own::is_less (std::strong_ordering::less));
  EXPECT_TRUE (own::is_less (own::strong_ordering::less));
  EXPECT_TRUE (own::is_equal (std::weak_ordering::equivalent));
  EXPECT_TRUE (own::is_equal (own::weak_ordering::equivalent));
  EXPECT_TRUE (own::is_greater (std::partial_ordering::greater));
  EXPECT_TRUE (own::is_greater (own::partial_ordering::greater));
  EXPECT_FALSE (own::is_less_equal (std::partial_ordering::unordered));
  EXPECT_FALSE (own::is_less_equal (own::partial_ordering::unordered));
  EXPECT_TRUE (own::is_not_equal (std::partial_ordering::unordered));
  EXPECT_TRUE (own::is_not_equal (own::partial_ordering::unordered));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

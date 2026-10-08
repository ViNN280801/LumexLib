// LumexSafeThreeWayCompare.cxx11.tests.cpp
// The three-way part of the safe comparator, from C++11:
// safe_three_way_compare (the function and the member of safe_comparator,
// plain and atomic), three_way_comparison_result_t and the helpers is_equal,
// is_not_equal, is_less, is_less_equal, is_greater and is_greater_equal. Every
// pair of the integer types of every width and sign is checked at the limits
// and around zero against an exact reference (a sign and a magnitude, no
// conversion); the floating-point types with both zeros, the infinities, NaN
// and a denormal; integers against floating-point values. The result must also
// agree with the six boolean functions (safe_less, ...) that existed before.
// The result type is the alias of LumexOrdering.hpp: the library's own classes
// below C++20 and std::*_ordering from it, so the same expectations run in
// every suite.

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/numeric/LumexOrdering.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"
#include "lumex/tests/core/utility/numeric/LumexNumericSamples.hpp"

using namespace lumex::core::utility::numeric;
using namespace lumex_numeric_samples;

namespace
{
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

// The atomic comparator of every type but long double: std::atomic<long
// double> is 16 bytes and needs libatomic on x86-64, which these tests do not
// link.
template <typename T, typename U>
void
check_atomic_member (mismatch_report &report, T lhs, U rhs, int expected,
                     std::true_type /* usable */)
{
  expect_code (
      report, "atomic member", lhs, rhs, expected,
      ordering_code (
          safe_comparator<T, true> (lhs).safe_three_way_compare (rhs)));
}

template <typename T, typename U>
void
check_atomic_member (mismatch_report &, T, U, int, std::false_type)
{
}

// Everything the three-way part says about one pair, against the code of the
// exact order (-1, 0, 1, or 2 for unordered).
template <typename T, typename U>
void
check_one_pair (mismatch_report &report, T lhs, U rhs, int expected)
{
  typedef three_way_comparison_result_t<T, U> Result;

  Result const free_result = safe_three_way_compare (lhs, rhs);
  Result const plain_result
      = safe_comparator<T, false> (lhs).safe_three_way_compare (rhs);

  expect_code (report, "free function", lhs, rhs, expected,
               ordering_code (free_result));
  expect_code (report, "member", lhs, rhs, expected,
               ordering_code (plain_result));
  check_atomic_member (
      report, lhs, rhs, expected,
      std::integral_constant<bool, !std::is_same<T, long double>::value> ());

  // The helpers, read from the code of the exact order.
  bool const want_less = expected == -1;
  bool const want_equal = expected == 0;
  bool const want_greater = expected == 1;
  expect_flag (report, "is_less", lhs, rhs, want_less, is_less (free_result));
  expect_flag (report, "is_equal", lhs, rhs, want_equal,
               is_equal (free_result));
  expect_flag (report, "is_greater", lhs, rhs, want_greater,
               is_greater (free_result));
  expect_flag (report, "is_less_equal", lhs, rhs, want_less || want_equal,
               is_less_equal (free_result));
  expect_flag (report, "is_greater_equal", lhs, rhs,
               want_greater || want_equal, is_greater_equal (free_result));
  expect_flag (report, "is_not_equal", lhs, rhs, !want_equal,
               is_not_equal (free_result));

  // The ordering against the literal 0.
  expect_flag (report, "result < 0", lhs, rhs, want_less, free_result < 0);
  expect_flag (report, "result == 0", lhs, rhs, want_equal, free_result == 0);
  expect_flag (report, "result > 0", lhs, rhs, want_greater, free_result > 0);

  // The boolean functions, which were tested before the three-way part.
  expect_flag (report, "safe_less", lhs, rhs, want_less, safe_less (lhs, rhs));
  expect_flag (report, "safe_equal", lhs, rhs, want_equal,
               safe_equal (lhs, rhs));
  expect_flag (report, "safe_greater", lhs, rhs, want_greater,
               safe_greater (lhs, rhs));
  expect_flag (report, "safe_less_equal", lhs, rhs, want_less || want_equal,
               safe_less_equal (lhs, rhs));
  expect_flag (report, "safe_greater_equal", lhs, rhs,
               want_greater || want_equal, safe_greater_equal (lhs, rhs));
  expect_flag (report, "safe_not_equal", lhs, rhs, !want_equal,
               safe_not_equal (lhs, rhs));
}

// Integer against integer, every sample of both types.
template <typename T, typename U> struct integer_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same<three_way_comparison_result_t<T, U>,
                                strong_ordering_t>::value,
                   "two integer types give a strong ordering");
    static_assert (std::is_same<decltype (safe_three_way_compare (
                                    std::declval<T> (), std::declval<U> ())),
                                strong_ordering_t>::value,
                   "the function returns the strong ordering");

    std::vector<T> const lhs = integer_samples<T> ();
    std::vector<U> const rhs = integer_samples<U> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        check_one_pair (report, lhs[i], rhs[j],
                        integer_order (lhs[i], rhs[j]));
  }
};

// Floating-point against floating-point, every sample of both types.
template <typename F, typename G> struct float_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same<three_way_comparison_result_t<F, G>,
                                partial_ordering_t>::value,
                   "two floating-point types give a partial ordering");

    std::vector<F> const lhs = float_samples<F> ();
    std::vector<G> const rhs = float_samples<G> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        {
          // An infinity is ordered by its sign for every pair of types, so
          // the order of the widened values is the answer (see also
          // LumexSafeInfinityOrder.cxx11.tests.cpp).
          int const expected = float_order (lhs[i], rhs[j]);
          check_one_pair (report, lhs[i], rhs[j], expected);
        }
  }
};

// Integer against floating-point and floating-point against integer. An
// infinity is ordered by its sign, NaN is unordered.
template <typename I, typename F> struct mixed_pair_check
{
  static void
  run (mismatch_report &report)
  {
    static_assert (std::is_same<three_way_comparison_result_t<I, F>,
                                partial_ordering_t>::value,
                   "integer against floating-point gives a partial ordering");
    static_assert (std::is_same<three_way_comparison_result_t<F, I>,
                                partial_ordering_t>::value,
                   "floating-point against integer gives a partial ordering");

    std::vector<I> const integers = exactly_representable_integers<I, F> ();
    std::vector<F> const floats = mixed_float_samples<F> ();
    for (std::size_t i = 0; i < integers.size (); ++i)
      for (std::size_t j = 0; j < floats.size (); ++j)
        {
          // float_order answers 2 for NaN and orders an infinity by its sign.
          int const forward = float_order (
              static_cast<long double> (integers[i]), floats[j]);
          int const backward = forward == 2 ? 2 : -forward;
          check_one_pair (report, integers[i], floats[j], forward);
          check_one_pair (report, floats[j], integers[i], backward);
        }
  }
};

template <template <typename, typename> class Check, typename FirstList,
          typename SecondList>
void
expect_no_mismatch ()
{
  mismatch_report report;
  for_each_pair<Check, FirstList, SecondList, mismatch_report>::run (report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}
} // namespace

// === Every pair of integer types, the limits and around zero ===

TEST (LumexSafeThreeWayInteger,
      GivenEveryPairOfIntegerTypes_WhenCompared_ThenTheExactStrongOrdering)
{
  expect_no_mismatch<integer_pair_check, integer_types, integer_types> ();
}

// === Floating-point, NaN, the infinities, the zeros ===

TEST (LumexSafeThreeWayFloat,
      GivenEveryPairOfFloatTypes_WhenCompared_ThenThePartialOrdering)
{
  expect_no_mismatch<float_pair_check, float_types, float_types> ();
}

TEST (LumexSafeThreeWayMixed,
      GivenIntegerAndFloat_WhenComparedBothWays_ThenThePartialOrdering)
{
  expect_no_mismatch<mixed_pair_check, integer_types, float_types> ();
}

// === Readable cases with fixed answers ===

TEST (LumexSafeThreeWayCases,
      GivenSignedAgainstUnsigned_WhenCompared_ThenNoSignSurprise)
{
  // Comparing -1 with 0u as unsigned would give greater.
  EXPECT_TRUE (safe_three_way_compare (-1, 0u) < 0);
  EXPECT_TRUE (safe_three_way_compare (0u, -1) > 0);
  EXPECT_TRUE (safe_three_way_compare (-1, 4294967295u) < 0);
  EXPECT_TRUE (safe_three_way_compare (4294967295u, -1) > 0);
  EXPECT_TRUE (
      (safe_three_way_compare ((std::numeric_limits<int>::min) (), 0u)) < 0);
  EXPECT_TRUE ((safe_three_way_compare (
                   (std::numeric_limits<long long>::min) (),
                   (std::numeric_limits<unsigned long long>::max) ()))
               < 0);
  EXPECT_TRUE ((safe_three_way_compare (
                   (std::numeric_limits<unsigned long long>::max) (),
                   (std::numeric_limits<long long>::max) ()))
               > 0);
  EXPECT_TRUE (safe_three_way_compare (0, 0u) == 0);
  EXPECT_TRUE (safe_three_way_compare (-1, static_cast<unsigned char> (255))
               < 0);
}

TEST (LumexSafeThreeWayCases,
      GivenNarrowAgainstWide_WhenCompared_ThenNoOverflow)
{
  // The example of the header: unsigned char(1652) is 116.
  EXPECT_TRUE (safe_three_way_compare (static_cast<unsigned char> (252), 1652)
               < 0);
  EXPECT_TRUE (safe_three_way_compare (1652, static_cast<unsigned char> (252))
               > 0);
  EXPECT_TRUE (safe_three_way_compare (static_cast<signed char> (-128), 128)
               < 0);
  EXPECT_TRUE (safe_three_way_compare (static_cast<signed char> (127), 127L)
               == 0);
  EXPECT_TRUE (
      safe_three_way_compare (static_cast<unsigned short> (65535), 65536) < 0);
  EXPECT_TRUE (safe_three_way_compare (4294967296LL, 0u) > 0);
  EXPECT_TRUE (safe_three_way_compare (0u, 4294967296LL) < 0);
  EXPECT_TRUE (safe_three_way_compare (4294967296ULL, 1) > 0);
}

TEST (LumexSafeThreeWayCases, GivenSameType_WhenCompared_ThenTheNaturalOrder)
{
  EXPECT_TRUE (safe_three_way_compare (1, 2) < 0);
  EXPECT_TRUE (safe_three_way_compare (2, 1) > 0);
  EXPECT_TRUE (safe_three_way_compare (7, 7) == 0);
  EXPECT_TRUE (safe_three_way_compare (1u, 2u) < 0);
  EXPECT_TRUE (safe_three_way_compare (3.5, 2.5) > 0);
  EXPECT_TRUE (safe_three_way_compare (2.5f, 2.5f) == 0);
}

TEST (LumexSafeThreeWayCases, GivenNaN_WhenCompared_ThenUnordered)
{
  double const nan_value = std::numeric_limits<double>::quiet_NaN ();
  float const nan_float = std::numeric_limits<float>::quiet_NaN ();

  partial_ordering_t const nan_nan
      = safe_three_way_compare (nan_value, nan_value);
  EXPECT_FALSE (nan_nan < 0);
  EXPECT_FALSE (nan_nan == 0);
  EXPECT_FALSE (nan_nan > 0);
  EXPECT_TRUE (nan_nan != 0);
  EXPECT_FALSE (is_less (nan_nan));
  EXPECT_FALSE (is_equal (nan_nan));
  EXPECT_FALSE (is_greater (nan_nan));
  EXPECT_TRUE (is_not_equal (nan_nan));

  EXPECT_TRUE (nan_nan == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (nan_value, 1.0)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (1.0, nan_value)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (nan_float, 1.0)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (1, nan_value)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (nan_float, 7u)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (7u, nan_float)
               == partial_ordering_t::unordered);
  EXPECT_TRUE (safe_three_way_compare (nan_value, nan_float)
               == partial_ordering_t::unordered);
}

TEST (LumexSafeThreeWayCases, GivenZeros_WhenCompared_ThenEquivalent)
{
  EXPECT_TRUE (safe_three_way_compare (0.0, -0.0)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (-0.0, 0.0)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (0, -0.0)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (-0.0f, 0)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_three_way_compare (0.0f, -0.0)
               == partial_ordering_t::equivalent);
}

TEST (LumexSafeThreeWayCases, GivenInfinities_WhenSameType_ThenOrderedBySign)
{
  double const inf = std::numeric_limits<double>::infinity ();

  EXPECT_TRUE (safe_three_way_compare (inf, 1.0) > 0);
  EXPECT_TRUE (safe_three_way_compare (-inf, 1.0) < 0);
  EXPECT_TRUE (safe_three_way_compare (1.0, inf) < 0);
  EXPECT_TRUE (safe_three_way_compare (inf, inf) == 0);
  EXPECT_TRUE (safe_three_way_compare (-inf, inf) < 0);
  EXPECT_TRUE (
      safe_three_way_compare (inf, (std::numeric_limits<double>::max) ()) > 0);
}

TEST (LumexSafeThreeWayCases,
      GivenIntegerAgainstFloat_WhenCompared_ThenTheOrderOfTheNumbers)
{
  EXPECT_TRUE (safe_three_way_compare (42, 42.0f) == 0);
  EXPECT_TRUE (safe_three_way_compare (42, 41.5) > 0);
  EXPECT_TRUE (safe_three_way_compare (42, 42.5) < 0);
  EXPECT_TRUE (safe_three_way_compare (-1, -0.5) < 0);
  EXPECT_TRUE (safe_three_way_compare (41.5, 42) < 0);
  EXPECT_TRUE (safe_three_way_compare (3.14f, 3) > 0);
  EXPECT_TRUE (safe_three_way_compare (static_cast<unsigned char> (200), 199.5)
               > 0);
  EXPECT_TRUE (safe_three_way_compare (0u, -0.5) > 0);
}

// An infinity is ordered by its sign for every pair of types, and the boolean
// siblings (safe_less, ...) say the same. Before 2.0.0.0 the comparison
// answered unordered here unless both were the same infinity, and safe_less
// and safe_greater gave an infinity no order against another floating-point
// type. The whole table is LumexSafeInfinityOrder.cxx11.tests.cpp.
TEST (LumexSafeThreeWayCases,
      GivenInfinityAgainstAnotherType_WhenCompared_ThenOrderedBySign)
{
  float const finf = std::numeric_limits<float>::infinity ();
  double const dinf = std::numeric_limits<double>::infinity ();

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
  EXPECT_TRUE (safe_three_way_compare (7, dinf) == partial_ordering_t::less);
  EXPECT_TRUE (safe_three_way_compare (finf, 7)
               == partial_ordering_t::greater);
  EXPECT_TRUE (safe_greater (finf, 1.0));
  EXPECT_TRUE (safe_less (-finf, 1.0));
  EXPECT_TRUE (safe_less (7, dinf));
  EXPECT_TRUE (safe_greater (finf, 7));
}

TEST (
    LumexSafeThreeWayCharacterization,
    GivenIntegerBeyondTheFloatMantissa_WhenCompared_ThenConvertedToTheFloatType)
{
  // 2^24 + 1 is not a float: the integer converts to the common type, as in
  // safe_equal.
  EXPECT_TRUE (safe_three_way_compare (16777217, 16777216.0f)
               == partial_ordering_t::equivalent);
  EXPECT_TRUE (safe_equal (16777217, 16777216.0f));
  EXPECT_TRUE (safe_three_way_compare (16777217, 16777216.0)
               == partial_ordering_t::greater);
}

// === The result type ===

TEST (LumexSafeThreeWayResultType,
      GivenTypePairs_WhenResultTypeAsked_ThenStrongForIntegersPartialOtherwise)
{
  static_assert (std::is_same<three_way_comparison_result_t<int, int>,
                              strong_ordering_t>::value,
                 "int, int");
  static_assert (std::is_same<three_way_comparison_result_t<char, long long>,
                              strong_ordering_t>::value,
                 "char, long long");
  static_assert (std::is_same<three_way_comparison_result_t<bool, unsigned>,
                              strong_ordering_t>::value,
                 "bool, unsigned");
  static_assert (std::is_same<three_way_comparison_result_t<float, float>,
                              partial_ordering_t>::value,
                 "float, float");
  static_assert (std::is_same<three_way_comparison_result_t<float, double>,
                              partial_ordering_t>::value,
                 "float, double");
  static_assert (std::is_same<three_way_comparison_result_t<int, float>,
                              partial_ordering_t>::value,
                 "int, float");
  static_assert (
      std::is_same<three_way_comparison_result_t<long double, short>,
                   partial_ordering_t>::value,
      "long double, short");
  static_assert (
      std::is_same<
          three_way_comparison_result_t<int const &, unsigned long &&>,
          strong_ordering_t>::value,
      "qualifiers are ignored");
  static_assert (std::is_same<three_way_comparison_result<int, double>::type,
                              partial_ordering_t>::value,
                 "the metafunction has the same result");
  static_assert (three_way_comparison_result<int, long>::both_integral,
                 "both_integral");
  static_assert (three_way_comparison_result<float, double>::both_floating,
                 "both_floating");
  static_assert (three_way_comparison_result<int, double>::mixed_types,
                 "mixed_types");
  SUCCEED ();
}

TEST (LumexSafeThreeWayResultType,
      GivenComparatorMember_WhenCalledOnConst_ThenReturnsTheAliasType)
{
  safe_comparator<int> const comparator (5);
  safe_comparator<double, true> const atomic_comparator (2.5);

  static_assert (
      std::is_same<decltype (comparator.safe_three_way_compare (6u)),
                   strong_ordering_t>::value,
      "integer member");
  static_assert (
      std::is_same<decltype (atomic_comparator.safe_three_way_compare (1)),
                   partial_ordering_t>::value,
      "floating-point member");
  EXPECT_TRUE (comparator.safe_three_way_compare (6u) < 0);
  EXPECT_TRUE (comparator.safe_three_way_compare (5u) == 0);
  EXPECT_TRUE (atomic_comparator.safe_three_way_compare (1) > 0);
}

TEST (LumexSafeThreeWayResultType,
      GivenResultAlias_WhenCompared_ThenItIsTheOwnClassBelowCxx20)
{
  // The result is the ordering class of LumexOrdering.hpp unless <compare>
  // gives the standard one.
  EXPECT_EQ ((std::is_same<three_way_comparison_result_t<int, int>,
                           strong_ordering>::value),
             !LUMEX_HAS_THREE_WAY_COMPARISON);
  EXPECT_EQ ((std::is_same<three_way_comparison_result_t<double, int>,
                           partial_ordering>::value),
             !LUMEX_HAS_THREE_WAY_COMPARISON);
}

// === is_equal .. is_not_equal ===

TEST (LumexSafeThreeWayHelpers,
      GivenOrderingsOfTheLibrary_WhenHelpersAsked_ThenByValue)
{
  EXPECT_TRUE (is_less (strong_ordering::less));
  EXPECT_FALSE (is_less (strong_ordering::equal));
  EXPECT_FALSE (is_less (strong_ordering::greater));
  EXPECT_TRUE (is_equal (strong_ordering::equal));
  EXPECT_FALSE (is_equal (strong_ordering::less));
  EXPECT_TRUE (is_greater (strong_ordering::greater));
  EXPECT_FALSE (is_greater (strong_ordering::equal));
  EXPECT_TRUE (is_less_equal (strong_ordering::less));
  EXPECT_TRUE (is_less_equal (strong_ordering::equal));
  EXPECT_FALSE (is_less_equal (strong_ordering::greater));
  EXPECT_TRUE (is_greater_equal (strong_ordering::greater));
  EXPECT_TRUE (is_greater_equal (strong_ordering::equal));
  EXPECT_FALSE (is_greater_equal (strong_ordering::less));
  EXPECT_TRUE (is_not_equal (strong_ordering::less));
  EXPECT_TRUE (is_not_equal (strong_ordering::greater));
  EXPECT_FALSE (is_not_equal (strong_ordering::equal));

  EXPECT_TRUE (is_less (weak_ordering::less));
  EXPECT_TRUE (is_equal (weak_ordering::equivalent));
  EXPECT_TRUE (is_greater (weak_ordering::greater));
  EXPECT_TRUE (is_less_equal (weak_ordering::equivalent));
  EXPECT_FALSE (is_less_equal (weak_ordering::greater));
  EXPECT_TRUE (is_greater_equal (weak_ordering::equivalent));
  EXPECT_FALSE (is_greater_equal (weak_ordering::less));
  EXPECT_TRUE (is_not_equal (weak_ordering::less));
  EXPECT_FALSE (is_not_equal (weak_ordering::equivalent));

  EXPECT_TRUE (is_less (partial_ordering::less));
  EXPECT_TRUE (is_equal (partial_ordering::equivalent));
  EXPECT_TRUE (is_greater (partial_ordering::greater));
  EXPECT_TRUE (is_less_equal (partial_ordering::less));
  EXPECT_TRUE (is_less_equal (partial_ordering::equivalent));
  EXPECT_FALSE (is_less_equal (partial_ordering::greater));
  EXPECT_TRUE (is_greater_equal (partial_ordering::greater));
  EXPECT_TRUE (is_greater_equal (partial_ordering::equivalent));
  EXPECT_FALSE (is_greater_equal (partial_ordering::less));
  EXPECT_TRUE (is_not_equal (partial_ordering::greater));
  EXPECT_FALSE (is_not_equal (partial_ordering::equivalent));
}

TEST (LumexSafeThreeWayHelpers,
      GivenUnordered_WhenHelpersAsked_ThenOnlyNotEqual)
{
  partial_ordering const unordered = partial_ordering::unordered;

  EXPECT_FALSE (is_less (unordered));
  EXPECT_FALSE (is_equal (unordered));
  EXPECT_FALSE (is_greater (unordered));
  EXPECT_FALSE (is_less_equal (unordered));
  EXPECT_FALSE (is_greater_equal (unordered));
  EXPECT_TRUE (is_not_equal (unordered));
}

TEST (LumexSafeThreeWayHelpers, GivenAliasTypes_WhenHelpersAsked_ThenByValue)
{
  EXPECT_TRUE (is_less (strong_ordering_t::less));
  EXPECT_TRUE (is_equal (strong_ordering_t::equal));
  EXPECT_TRUE (is_greater (strong_ordering_t::greater));
  EXPECT_TRUE (is_less (weak_ordering_t::less));
  EXPECT_TRUE (is_equal (weak_ordering_t::equivalent));
  EXPECT_TRUE (is_greater (weak_ordering_t::greater));
  EXPECT_TRUE (is_less (partial_ordering_t::less));
  EXPECT_TRUE (is_equal (partial_ordering_t::equivalent));
  EXPECT_TRUE (is_greater (partial_ordering_t::greater));
  EXPECT_FALSE (is_less (partial_ordering_t::unordered));
  EXPECT_FALSE (is_greater (partial_ordering_t::unordered));
  EXPECT_TRUE (is_not_equal (partial_ordering_t::unordered));
}

TEST (
    LumexSafeThreeWayHelpers,
    GivenConstantExpressions_WhenHelpersUsedInStaticAssert_ThenEvaluatedAtCompileTime)
{
  static_assert (is_less (strong_ordering::less), "is_less");
  static_assert (!is_less (strong_ordering::equal), "is_less");
  static_assert (is_equal (weak_ordering::equivalent), "is_equal");
  static_assert (is_greater (partial_ordering::greater), "is_greater");
  static_assert (is_less_equal (strong_ordering::equal), "is_less_equal");
  static_assert (is_greater_equal (partial_ordering::equivalent),
                 "is_greater_equal");
  static_assert (is_not_equal (partial_ordering::unordered), "is_not_equal");
  static_assert (!is_less_equal (partial_ordering::unordered),
                 "unordered is not less or equal");
  SUCCEED ();
}

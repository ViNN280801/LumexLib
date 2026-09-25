// LumexMath.tests.cpp
//
// Built three times (LumexMathTests at C++11, LumexMathCxx17Tests,
// LumexMathCxx20Tests): LumexMath works from C++11 on, and the C++20 block at
// the end checks std::views (non-const-iterable views, sentinel ends).
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#if __cplusplus >= 202002L
#include <ranges>
#endif

#include <gtest/gtest.h>

#include "lumex/core/math/LumexMath"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::math::ops;

using lumex::core::math::ops::avg;
using lumex::core::math::ops::checked_narrow_cast;
using lumex::core::math::ops::distance;
using lumex::core::math::ops::is_nan_inf;
using lumex::core::math::ops::rms;
using lumex::core::math::ops::rmse;
using lumex::core::math::ops::squared_difference;

// --- is_nan_inf -------------------------------------------------------------

TEST (LumexMathTest, GivenFiniteValue_Whenis_nan_inf_ThenReturnsFalse)
{
  EXPECT_FALSE (is_nan_inf (1.0));
  EXPECT_FALSE (is_nan_inf (-42.5f));
  EXPECT_FALSE (is_nan_inf (0.0));
}

TEST (LumexMathTest, GivenNan_Whenis_nan_inf_ThenReturnsTrue)
{
  EXPECT_TRUE (is_nan_inf (std::numeric_limits<double>::quiet_NaN ()));
}

TEST (LumexMathTest, GivenInfinity_Whenis_nan_inf_ThenReturnsTrue)
{
  EXPECT_TRUE (is_nan_inf (std::numeric_limits<double>::infinity ()));
  EXPECT_TRUE (is_nan_inf (-std::numeric_limits<double>::infinity ()));
}

// --- checked_narrow_cast
// ------------------------------------------------------

TEST (LumexMathTest,
      GivenInRangeValue_Whenchecked_narrow_cast_ThenReturnsCastValue)
{
  // Result captured in a named variable rather than passed to EXPECT_EQ
  // directly - the preprocessor does not understand template angle
  // brackets, so a two-argument macro like EXPECT_EQ would otherwise see
  // the comma inside checked_narrow_cast<int, short> as an extra macro
  // argument separator (the same class of trap documented for LUMEX_VARINFO
  // in lumex/core/reflection/LumexReflection).
  short const kResult = checked_narrow_cast<int, short> (100, "field");
  EXPECT_EQ (kResult, 100);
}

TEST (LumexMathTest,
      GivenOutOfRangeValue_Whenchecked_narrow_cast_ThenThrowsOutOfRange)
{
  auto const kCastOutOfRange
      = [] { return checked_narrow_cast<int, short> (1000000, "field"); };
  EXPECT_THROW (kCastOutOfRange (), std::out_of_range);
}

TEST (LumexMathTest,
      GivenNonFiniteFloatingValue_Whenchecked_narrow_cast_ThenThrowsOutOfRange)
{
  auto const kCastInfinity = [] {
    return checked_narrow_cast<double, int> (
        std::numeric_limits<double>::infinity (), "field");
  };
  auto const kCastNan = [] {
    return checked_narrow_cast<double, int> (
        std::numeric_limits<double>::quiet_NaN (), "field");
  };
  EXPECT_THROW (kCastInfinity (), std::out_of_range);
  EXPECT_THROW (kCastNan (), std::out_of_range);
}

// --- distance / squared_difference
// -------------------------------------------

TEST (LumexMathTest,
      GivenTwoIntegers_WhenDistance_ThenReturnsAbsoluteDifference)
{
  EXPECT_EQ (distance (5, 3), 2);
  EXPECT_EQ (distance (3, 5), 2);
}

TEST (LumexMathTest, GivenUnsignedOperands_WhenDistance_ThenDoesNotUnderflow)
{
  unsigned int const a = 3;
  unsigned int const b = 5;
  EXPECT_EQ (distance (a, b), 2u);
}

TEST (LumexMathTest,
      GivenFloatingPointOperand_WhenDistance_ThenReturnsAbsoluteDifference)
{
  EXPECT_DOUBLE_EQ (distance (5.5, 2.0), 3.5);
  EXPECT_DOUBLE_EQ (distance (2.0, 5.5), 3.5);
}

TEST (LumexMathTest,
      GivenTwoValues_WhenSquaredDifference_ThenReturnsSquareOfDifference)
{
  EXPECT_EQ (squared_difference (5, 2), 9);
  EXPECT_DOUBLE_EQ (squared_difference (1.5, 0.5), 1.0);
}

// --- avg ----------------------------------------------------------------

TEST (LumexMathTest, GivenNonEmptyRange_WhenAverage_ThenReturnsArithmeticMean)
{
  std::vector<int> const values{ 1, 2, 3, 4, 5 };
  EXPECT_EQ (avg (values), 3);
}

TEST (LumexMathTest, GivenEmptyRange_WhenAverage_ThenReturnsZero)
{
  std::vector<double> const values{};
  EXPECT_DOUBLE_EQ (avg (values), 0.0);
}

TEST (
    LumexMathTest,
    GivenMutableRange_WhenAverage_ThenNonConstOverloadWorksTooWithoutInfiniteRecursion)
{
  // Regression test: the ported original had the const-ref overload call
  // itself (avg(range) resolving right back to the same const-ref
  // overload), an infinite-recursion bug fixed during the port by
  // delegating both overloads to a shared Detail::_avg_impl helper.
  std::vector<int> values{ 2, 4, 6 };
  EXPECT_EQ (avg (values), 4);

  std::vector<int> const constValues{ 2, 4, 6 };
  EXPECT_EQ (avg (constValues), 4);
}

TEST (LumexMathTest,
      GivenPredicate_WhenAverage_ThenOnlyMatchingElementsAreAveraged)
{
  std::vector<int> const values{ 1, 2, 3, 4, 5, 6 };
  auto const isEven = [] (int v) { return v % 2 == 0; };
  EXPECT_EQ (avg (values, isEven), 4); // (2 + 4 + 6) / 3
}

TEST (LumexMathTest,
      GivenNoElementMatchesPredicate_WhenAverage_ThenReturnsZero)
{
  std::vector<int> const values{ 1, 3, 5 };
  auto const isEven = [] (int v) { return v % 2 == 0; };
  EXPECT_EQ (avg (values, isEven), 0);
}

// --- rms ----------------------------------------------------------------

TEST (LumexMathTest, GivenRange_WhenRMS_ThenReturnsRootMeanSquare)
{
  std::vector<double> const values{ 3.0, 4.0 };
  // sqrt((9 + 16) / 2) = sqrt(12.5)
  EXPECT_DOUBLE_EQ (rms (values), std::sqrt (12.5));
}

TEST (LumexMathTest, GivenEmptyRange_WhenRMS_ThenReturnsZero)
{
  std::vector<double> const values{};
  EXPECT_DOUBLE_EQ (rms (values), 0.0);
}

TEST (
    LumexMathTest,
    GivenTwoEqualSizedRanges_WhenRMSTwoArg_ThenComputesFromElementwiseProduct)
{
  std::vector<double> const a{ 1.0, 2.0 };
  std::vector<double> const b{ 3.0, 4.0 };
  // inner_product = 1*3 + 2*4 = 11; sqrt(11/2)
  EXPECT_DOUBLE_EQ (rms (a, b), std::sqrt (5.5));
}

TEST (LumexMathTest,
      GivenMismatchedSizes_WhenRMSTwoArg_ThenThrowsInvalidArgument)
{
  std::vector<double> const a{ 1.0, 2.0 };
  std::vector<double> const b{ 1.0, 2.0, 3.0 };
  EXPECT_THROW (rms (a, b), std::invalid_argument);
}

// --- rmse ---------------------------------------------------------------

TEST (LumexMathTest,
      GivenRangeAndScalar_WhenRMSE_ThenComputesRootMeanSquaredError)
{
  std::vector<double> const values{ 1.0, 2.0, 3.0 };
  double const scalar = 2.0;
  // ((1-2)^2 + (2-2)^2 + (3-2)^2) / 3 = 2/3
  EXPECT_DOUBLE_EQ (rmse (values, scalar), std::sqrt (2.0 / 3.0));
}

TEST (LumexMathTest, GivenEmptyRange_WhenRMSEWithScalar_ThenReturnsZero)
{
  std::vector<double> const values{};
  EXPECT_DOUBLE_EQ (rmse (values, 1.0), 0.0);
}

TEST (LumexMathTest,
      GivenTwoEqualSizedRanges_WhenRMSETwoRanges_ThenComputesElementwiseError)
{
  std::vector<double> const a{ 1.0, 2.0, 3.0 };
  std::vector<double> const b{ 2.0, 2.0, 2.0 };
  // ((1-2)^2 + (2-2)^2 + (3-2)^2) / 3 = 2/3
  EXPECT_DOUBLE_EQ (rmse (a, b), std::sqrt (2.0 / 3.0));
}

TEST (LumexMathTest,
      GivenMismatchedSizes_WhenRMSETwoRanges_ThenThrowsInvalidArgument)
{
  std::vector<double> const a{ 1.0, 2.0 };
  std::vector<double> const b{ 1.0, 2.0, 3.0 };
  EXPECT_THROW (rmse (a, b), std::invalid_argument);
}

TEST (LumexMathTest, GivenEmptyRanges_WhenRMSETwoRanges_ThenReturnsZero)
{
  std::vector<double> const a{};
  std::vector<double> const b{};
  EXPECT_DOUBLE_EQ (rmse (a, b), 0.0);
}

// --- C++11 floor: traits, constexpr, overload set -------------------------

static_assert (lumex::core::math::ops::traits::is_numeric<int>::value, "");
static_assert (lumex::core::math::ops::traits::is_numeric<char>::value, "");
static_assert (lumex::core::math::ops::traits::is_numeric<double>::value, "");
static_assert (lumex::core::math::ops::traits::is_numeric<long double>::value,
               "");
static_assert (!lumex::core::math::ops::traits::is_numeric<bool>::value, "");
static_assert (!lumex::core::math::ops::traits::is_numeric<bool const>::value,
               "");
static_assert (!lumex::core::math::ops::traits::is_numeric<std::string>::value,
               "");

// distance and squared_difference of integers are constant expressions.
static_assert (distance (5, 3) == 2, "");
static_assert (distance (3u, 5u) == 2u, "");
static_assert (squared_difference (5, 2) == 9, "");

// Results are promoted like `a - b`: two shorts give an int, not a short.
static_assert (
    std::is_same<decltype (squared_difference (short (1), short (2))),
                 int>::value,
    "");
static_assert (
    std::is_same<decltype (distance (short (1), short (2))), int>::value, "");
static_assert (std::is_same<decltype (distance (1, 2.5f)), float>::value, "");

TEST (LumexMathTest,
      GivenIterators_WhenUnqualifiedDistance_ThenStdDistanceIsStillFound)
{
  // ops::distance must drop out of overload resolution for non-numeric
  // arguments (SFINAE) instead of failing to compile.
  std::vector<int> const values{ 1, 2, 3 };
  EXPECT_EQ (distance (values.begin (), values.end ()), 3);
}

// --- ranges accepted from C++11 on -----------------------------------------

TEST (LumexMathTest, GivenCArray_WhenAverageAndRMS_ThenComputesOverTheArray)
{
  int const values[] = { 2, 4, 6 };
  EXPECT_EQ (avg (values), 4);
  EXPECT_DOUBLE_EQ (rms (values), std::sqrt (56.0 / 3.0));
}

TEST (LumexMathTest, GivenTemporaryRange_WhenAverage_ThenComputesMean)
{
  EXPECT_EQ (avg (std::vector<int>{ 1, 2, 3 }), 2);
  EXPECT_DOUBLE_EQ (avg (std::list<double>{ 1.0, 2.0 }), 1.5);
}

TEST (LumexMathTest,
      GivenNonRandomAccessRanges_WhenRMSETwoRanges_ThenComputesElementwise)
{
  // The C++20 version needed random-access iterators (it + idx).
  std::list<double> const a{ 1.0, 2.0, 3.0 };
  std::list<double> const b{ 2.0, 2.0, 2.0 };
  EXPECT_DOUBLE_EQ (rmse (a, b), std::sqrt (2.0 / 3.0));
  EXPECT_DOUBLE_EQ (rms (a, b), std::sqrt (12.0 / 3.0));
}

TEST (LumexMathTest,
      GivenMismatchedListSizes_WhenRMSAndRMSE_ThenThrowInvalidArgument)
{
  std::list<double> const a{ 1.0, 2.0 };
  std::list<double> const b{ 1.0, 2.0, 3.0 };
  EXPECT_THROW (rms (a, b), std::invalid_argument);
  EXPECT_THROW (rmse (b, a), std::invalid_argument);
}

TEST (LumexMathTest,
      GivenOneEmptyRange_WhenRMSAndRMSETwoRanges_ThenThrowSizeMismatch)
{
  // Used to return 0: an empty range against a non-empty one is a size
  // mismatch like any other.
  std::vector<double> const empty{};
  std::vector<double> const values{ 1.0, 2.0 };
  EXPECT_THROW (rms (empty, values), LumexMathSizeMismatchException);
  EXPECT_THROW (rmse (values, empty), LumexMathSizeMismatchException);
}

TEST (LumexMathTest, GivenTwoEmptyRanges_WhenRMSTwoArg_ThenReturnsZero)
{
  std::vector<double> const a{};
  std::list<int> const b{};
  EXPECT_DOUBLE_EQ (rms (a, b), 0.0);
}

TEST (LumexMathTest,
      GivenMismatchedSizes_WhenRMSE_ThenExceptionIsInvalidArgumentWithSizes)
{
  std::vector<double> const a{ 1.0, 2.0 };
  std::list<double> const b{ 1.0, 2.0, 3.0, 4.0 };
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (rmse (a, b));
      FAIL () << "expected LumexMathSizeMismatchException";
    }
  catch (std::invalid_argument const &ex)
    {
      EXPECT_NE (dynamic_cast<LumexMathSizeMismatchException const *> (&ex),
                 nullptr);
      EXPECT_STREQ (ex.what (),
                    "rmse: size of both ranges must be equal (2 vs 4)");
    }
}

TEST (LumexMathTest,
      GivenRangesOfDifferentElementTypes_WhenRMSE_ThenUsesTheCommonType)
{
  std::vector<int> const a{ 1, 2, 3 };
  std::vector<double> const b{ 1.5, 2.5, 3.5 };
  EXPECT_DOUBLE_EQ (rmse (a, b), 0.5);
}

TEST (LumexMathTest,
      GivenPredicateOnNonRandomAccessRange_WhenAverage_ThenFiltersElements)
{
  std::list<double> const values{ 1.0, 2.0, 3.0, 4.0 };
  EXPECT_DOUBLE_EQ (avg (values, [] (double v) { return v > 2.0; }), 3.5);
}

// --- integer ranges are not truncated before the square root --------------
// The C++20 version divided the integer sum by an integer N first:
// rmse ({1, 2}, 0) was sqrt (5 / 2) = sqrt (2), not sqrt (2.5).

TEST (LumexMathTest,
      GivenIntegerRangeAndScalar_WhenRMSE_ThenDividesInFloatingPoint)
{
  std::vector<int> const values{ 1, 2 };
  EXPECT_DOUBLE_EQ (rmse (values, 0), std::sqrt (2.5));
}

TEST (LumexMathTest, GivenTwoIntegerRanges_WhenRMSE_ThenDividesInFloatingPoint)
{
  std::vector<int> const a{ 1, 2 };
  std::vector<int> const b{ 0, 0 };
  EXPECT_DOUBLE_EQ (rmse (a, b), std::sqrt (2.5));
}

TEST (LumexMathTest, GivenTwoIntegerRanges_WhenRMS_ThenDividesInFloatingPoint)
{
  std::vector<int> const a{ 1, 2 };
  EXPECT_DOUBLE_EQ (rms (a, a), std::sqrt (2.5));
}

// --- checked_narrow_cast is exact at the 64-bit limits ---------------------
// The C++20 version compared in long double, which is double on MSVC: the
// maximum of a 64-bit target rounded up to 2^63 / 2^64 and let out-of-range
// values through.

TEST (LumexMathTest,
      GivenUnsignedAboveInt64Max_Whenchecked_narrow_cast_ThenThrowsOutOfRange)
{
  auto const kCast = [] {
    return checked_narrow_cast<std::uint64_t, std::int64_t> (
        std::uint64_t (1) << 63, "field");
  };
  EXPECT_THROW (kCast (), std::out_of_range);

  std::uint64_t const fits = (std::uint64_t (1) << 63) - 1;
  std::int64_t const kResult
      = checked_narrow_cast<std::uint64_t, std::int64_t> (fits, "field");
  EXPECT_EQ (kResult, (std::numeric_limits<std::int64_t>::max) ());
}

TEST (LumexMathTest,
      GivenDoubleAtTwoToThe63_Whenchecked_narrow_cast_ThenThrowsOutOfRange)
{
  auto const kToSigned = [] {
    return checked_narrow_cast<double, std::int64_t> (9223372036854775808.0,
                                                      "field");
  };
  auto const kToUnsigned = [] {
    return checked_narrow_cast<double, std::uint64_t> (18446744073709551616.0,
                                                       "field");
  };
  EXPECT_THROW (kToSigned (), std::out_of_range);
  EXPECT_THROW (kToUnsigned (), std::out_of_range);

  std::int64_t const kLowest = checked_narrow_cast<double, std::int64_t> (
      -9223372036854775808.0, "field");
  EXPECT_EQ (kLowest, std::numeric_limits<std::int64_t>::lowest ());
}

TEST (LumexMathTest,
      GivenNegativeValueAndUnsignedTarget_Whenchecked_narrow_cast_ThenThrows)
{
  auto const kFromInt64 = [] {
    return checked_narrow_cast<std::int64_t, std::uint64_t> (-1, "field");
  };
  auto const kFromDouble
      = [] { return checked_narrow_cast<double, unsigned> (-0.5, "field"); };
  EXPECT_THROW (kFromInt64 (), std::out_of_range);
  EXPECT_THROW (kFromDouble (), std::out_of_range);
}

TEST (LumexMathTest,
      GivenBoundaryValues_Whenchecked_narrow_cast_ThenAcceptsExactlyTheRange)
{
  unsigned char const kByte
      = checked_narrow_cast<int, unsigned char> (255, "b");
  EXPECT_EQ (kByte, 255);
  short const kShort = checked_narrow_cast<double, short> (-32768.0, "s");
  EXPECT_EQ (kShort, -32768);
  int const kInt = checked_narrow_cast<double, int> (2147483647.0, "i");
  EXPECT_EQ (kInt, 2147483647);

  auto const kByteOver
      = [] { return checked_narrow_cast<int, unsigned char> (256, "b"); };
  auto const kIntOver
      = [] { return checked_narrow_cast<double, int> (2147483647.5, "i"); };
  auto const kFloatOver
      = [] { return checked_narrow_cast<double, float> (1.0e300, "f"); };
  EXPECT_THROW (kByteOver (), std::out_of_range);
  EXPECT_THROW (kIntOver (), std::out_of_range);
  EXPECT_THROW (kFloatOver (), std::out_of_range);
}

TEST (
    LumexMathTest,
    GivenOutOfRangeValue_Whenchecked_narrow_cast_ThenMessageNamesFieldAndRange)
{
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
          checked_narrow_cast<int, signed char> (300, std::string ("gain")));
      FAIL () << "expected std::out_of_range";
    }
  catch (std::out_of_range const &ex)
    {
      EXPECT_STREQ (ex.what (),
                    "Field 'gain' value 300 is out of range [-128; 127]");
    }
}

#if __cplusplus >= 202002L
// --- C++20 views ------------------------------------------------------------

TEST (LumexMathTest,
      GivenFilterView_WhenAverage_ThenNonConstOverloadIteratesTheView)
{
  // std::views::filter caches its begin () and cannot be iterated through
  // a const reference. avg(Range&) used to hand the view to a const-ref
  // helper, which did not compile for such views (PeakExpertWeb
  // SpectrumArray: filter | transform over data points).
  std::vector<double> const points{ 1.0, 2.0, 3.0, 10.0, 20.0 };
  auto view = points | std::views::filter ([] (double v) { return v < 5.0; })
              | std::views::transform ([] (double v) { return v * 2.0; });
  EXPECT_DOUBLE_EQ (avg (view), 4.0); // (2 + 4 + 6) / 3
  EXPECT_DOUBLE_EQ (rms (view), std::sqrt (56.0 / 3.0));
  EXPECT_DOUBLE_EQ (rmse (view, 4.0), std::sqrt (8.0 / 3.0));
}

TEST (LumexMathTest, GivenViewWithSentinelEnd_WhenAverage_ThenStopsAtSentinel)
{
  // take_while's end () is a sentinel of a different type than begin ().
  auto view = std::views::iota (1)
              | std::views::take_while ([] (int v) { return v < 4; });
  EXPECT_EQ (avg (view), 2);
  EXPECT_EQ (avg (std::views::iota (1, 4)), 2); // temporary view
}

TEST (LumexMathTest,
      GivenFilterViewsOfDifferentLengths_WhenRMSE_ThenThrowsInvalidArgument)
{
  std::vector<int> const values{ 1, 2, 3, 4 };
  auto even = values | std::views::filter ([] (int v) { return v % 2 == 0; });
  auto all = values | std::views::filter ([] (int) { return true; });
  EXPECT_THROW (rmse (even, all), std::invalid_argument);
}
#endif

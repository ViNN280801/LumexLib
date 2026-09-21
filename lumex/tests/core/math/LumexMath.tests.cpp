// LumexMath.tests.cpp
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

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

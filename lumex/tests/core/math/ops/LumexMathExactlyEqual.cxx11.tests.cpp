// lumex/tests/core/math/ops/LumexMathExactlyEqual.cxx11.tests.cpp
//
// exactly_equal of LumexMath.hpp: the IEEE equality of two floating-point
// numbers, the one place that silences -Wfloat-equal. It works from C++11, so
// the suite of every standard runs this file. The cases are the ones the XPath
// number rules rely on: zero, signed zero, infinities, NaN, values that are
// close but not equal, and a volatile operand.
#include <cfloat>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/math/LumexMath"

using lumex::core::math::ops::exactly_equal;

namespace
{
template <typename L, typename R, typename Enable = void>
struct IsExactlyEqualCallable : std::false_type
{
};

template <typename L, typename R>
struct IsExactlyEqualCallable<
    L, R,
    typename std::enable_if<std::is_same<
        decltype (exactly_equal (std::declval<L> (), std::declval<R> ())),
        bool>::value>::type> : std::true_type
{
};

template <typename T>
void
expect_ieee_equality ()
{
  T const zero = static_cast<T> (0);
  T const one = static_cast<T> (1);
  T const infinity = std::numeric_limits<T>::infinity ();
  T const nan = std::numeric_limits<T>::quiet_NaN ();
  T const epsilon = std::numeric_limits<T>::epsilon ();

  EXPECT_TRUE (exactly_equal (one, one));
  EXPECT_FALSE (exactly_equal (one, zero));
  EXPECT_TRUE (exactly_equal (zero, zero));
  EXPECT_TRUE (exactly_equal (zero, -zero));
  EXPECT_TRUE (exactly_equal (-zero, zero));

  EXPECT_TRUE (exactly_equal (infinity, infinity));
  EXPECT_TRUE (exactly_equal (-infinity, -infinity));
  EXPECT_FALSE (exactly_equal (infinity, -infinity));
  EXPECT_FALSE (exactly_equal (infinity, (std::numeric_limits<T>::max) ()));

  EXPECT_FALSE (exactly_equal (nan, nan));
  EXPECT_FALSE (exactly_equal (nan, one));
  EXPECT_FALSE (exactly_equal (one, nan));
  EXPECT_FALSE (exactly_equal (nan, infinity));

  // Neighbours are different numbers: there is no tolerance.
  EXPECT_FALSE (exactly_equal (one, one + epsilon));
  EXPECT_FALSE (exactly_equal (one + epsilon, one));
  EXPECT_TRUE (exactly_equal (one + epsilon, one + epsilon));
  EXPECT_FALSE (exactly_equal (std::numeric_limits<T>::denorm_min (), zero));
  EXPECT_TRUE (exactly_equal (std::numeric_limits<T>::denorm_min (),
                              std::numeric_limits<T>::denorm_min ()));
  EXPECT_TRUE (exactly_equal (std::numeric_limits<T>::lowest (),
                              std::numeric_limits<T>::lowest ()));
}
} // namespace

TEST (ExactlyEqualTest, GivenFloat_WhenCompared_ThenIeeeEquality)
{
  expect_ieee_equality<float> ();
}

TEST (ExactlyEqualTest, GivenDouble_WhenCompared_ThenIeeeEquality)
{
  expect_ieee_equality<double> ();
}

TEST (ExactlyEqualTest, GivenLongDouble_WhenCompared_ThenIeeeEquality)
{
  expect_ieee_equality<long double> ();
}

TEST (ExactlyEqualTest,
      GivenValuesThatRoundDifferently_WhenCompared_ThenTheyAreNotEqual)
{
  // 0.1 + 0.2 is not 0.3 in binary floating point; an exact comparison says
  // so, and 0.5 + 0.25 is exactly 0.75.
  double const volatile first = 0.1;
  double const volatile second = 0.2;
  EXPECT_FALSE (exactly_equal (first + second, 0.3));
  EXPECT_TRUE (exactly_equal (0.5 + 0.25, 0.75));
}

TEST (ExactlyEqualTest,
      GivenNanFromDivision_WhenComparedWithItself_ThenNotEqual)
{
  double const volatile zero = 0.0;
  double const nan = zero / zero;
  EXPECT_TRUE (std::isnan (nan));
  EXPECT_FALSE (exactly_equal (nan, nan));
}

TEST (ExactlyEqualTest,
      GivenVolatileOperands_WhenCompared_ThenEachIsReadOnceByValue)
{
  double const volatile value = 2.5;
  double const volatile other = 2.5;
  EXPECT_TRUE (exactly_equal (value, other));
  EXPECT_TRUE (exactly_equal (value * 2, value + value));
  EXPECT_FALSE (exactly_equal (value, value + 1.0));
  double const volatile nan = std::numeric_limits<double>::quiet_NaN ();
  EXPECT_FALSE (exactly_equal (nan, nan));
}

TEST (ExactlyEqualTest,
      GivenAConstantExpression_WhenCompared_ThenEvaluatedAtCompileTime)
{
  static_assert (exactly_equal (1.5, 1.5), "equal constants");
  static_assert (!exactly_equal (1.5, 2.5), "different constants");
  static_assert (exactly_equal (0.0, -0.0), "signed zeros are equal");
  SUCCEED ();
}

TEST (ExactlyEqualTest, GivenTheSignature_WhenInspected_ThenBoolAndNoexcept)
{
  static_assert (noexcept (exactly_equal (1.0, 2.0)),
                 "exactly_equal is noexcept");
  static_assert (
      std::is_same<decltype (exactly_equal (1.0f, 2.0f)), bool>::value,
      "the result is bool");
  SUCCEED ();
}

TEST (
    ExactlyEqualTest,
    GivenIntegersOrMixedTypes_WhenCalled_ThenTheyDoNotTakePartInOverloadResolution)
{
  static_assert (IsExactlyEqualCallable<double, double>::value,
                 "double, double");
  static_assert (IsExactlyEqualCallable<float, float>::value, "float, float");
  static_assert (IsExactlyEqualCallable<long double, long double>::value,
                 "long double, long double");
  static_assert (!IsExactlyEqualCallable<int, int>::value,
                 "integers are not floating point");
  static_assert (!IsExactlyEqualCallable<float, double>::value,
                 "mixed types are rejected, not converted");
  static_assert (!IsExactlyEqualCallable<double, int>::value,
                 "an integer operand is rejected");
  SUCCEED ();
}

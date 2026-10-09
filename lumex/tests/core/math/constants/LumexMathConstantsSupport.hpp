/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_CORE_MATH_CONSTANTS_SUPPORT_HPP
#define LUMEX_TESTS_CORE_MATH_CONSTANTS_SUPPORT_HPP

// What the relation tests of lumex/core/math/constants share (the .cxx11,
// .cxx17 and .cxx20 files): the constants of a floating type T built from the
// macros with the suffix of T, and a comparison in units in the last place.

#include <cmath>
#include <iomanip>
#include <limits>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsReference.hpp"

namespace lumex_math_constants_test
{
/// The constants of the type T, from the macros with the suffix of T.
template <typename T> struct constants;

#define LUMEX_TEST_CONSTANT_FUNCTION_F(NAME, DIGITS, DECIMALS, FM, FE, DM,    \
                                       DE, LM, LE)                            \
  static constexpr float NAME ()                                              \
  {                                                                           \
    return LUMEX_TEST_FLOAT_LITERAL (LUMEX_MATH_CONSTANTS_##NAME);            \
  }
#define LUMEX_TEST_CONSTANT_FUNCTION_D(NAME, DIGITS, DECIMALS, FM, FE, DM,    \
                                       DE, LM, LE)                            \
  static constexpr double NAME () { return LUMEX_MATH_CONSTANTS_##NAME; }
#define LUMEX_TEST_CONSTANT_FUNCTION_L(NAME, DIGITS, DECIMALS, FM, FE, DM,    \
                                       DE, LM, LE)                            \
  static constexpr long double NAME ()                                        \
  {                                                                           \
    return LUMEX_TEST_LONG_DOUBLE_LITERAL (LUMEX_MATH_CONSTANTS_##NAME);      \
  }

template <> struct constants<float>
{
  LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_CONSTANT_FUNCTION_F)
};
template <> struct constants<double>
{
  LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_CONSTANT_FUNCTION_D)
};
template <> struct constants<long double>
{
  LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_CONSTANT_FUNCTION_L)
};

/// One unit in the last place at the magnitude of `value`.
template <typename T>
T
ulp_of (T value)
{
  T const magnitude = std::fabs (value);
  return std::nextafter (magnitude, std::numeric_limits<T>::infinity ())
         - magnitude;
}

/// `actual` is within `ulps` units in the last place of `expected`.
template <typename T>
::testing::AssertionResult
near_ulps (T actual, T expected, int ulps)
{
  T const tolerance = static_cast<T> (ulps) * ulp_of (expected);
  T const difference = std::fabs (actual - expected);
  if (difference <= tolerance)
    return ::testing::AssertionSuccess ();
  return ::testing::AssertionFailure ()
         << "actual "
         << std::setprecision (std::numeric_limits<T>::max_digits10) << actual
         << ", expected " << expected << ", difference "
         << difference / ulp_of (expected) << " ulp (allowed " << ulps << ")";
}

/// The types the relations run at.
using floating_types = ::testing::Types<float, double, long double>;

/// The suite name of a type: Suite/float, Suite/double, Suite/long_double.
struct type_name
{
  template <typename T>
  static std::string
  GetName (int)
  {
    return std::is_same<T, float>::value    ? "float"
           : std::is_same<T, double>::value ? "double"
                                            : "long_double";
  }
};
} // namespace lumex_math_constants_test

#endif // !LUMEX_TESTS_CORE_MATH_CONSTANTS_SUPPORT_HPP

// lumex/tests/core/math/constants/LumexMathConstantsMacros.cxx11.tests.cpp
//
// The macro forms of LumexMathConstants.hpp: the header is standalone and
// can be included twice, every documented macro is defined and no other
// spelling is, a constant stays a single literal token next to any operator,
// and the include guard has the name the project's convention gives. This
// file includes the header twice and through the umbrella. It works from
// C++11, so the suite of every standard runs it.
#include <cstddef>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

// The constants header twice, and the umbrella that includes it too. That
// the header needs nothing before it is checked by lint.headers_standalone.
#include "lumex/core/math/LumexMath"
#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsReference.hpp"

#ifndef LUMEX_CORE_MATH_CONSTANTS_HPP
#error                                                                        \
    "the include guard of LumexMathConstants.hpp is LUMEX_CORE_MATH_CONSTANTS_HPP"
#endif

// Every documented macro is defined (the list of the module rule).
#ifndef LUMEX_MATH_CONSTANTS_PI
#error "LUMEX_MATH_CONSTANTS_PI is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_EULER_NUMBER
#error "LUMEX_MATH_CONSTANTS_EULER_NUMBER is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_GOLDEN_RATIO
#error "LUMEX_MATH_CONSTANTS_GOLDEN_RATIO is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_SILVER_RATIO
#error "LUMEX_MATH_CONSTANTS_SILVER_RATIO is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_EULER_MASCHERONI
#error "LUMEX_MATH_CONSTANTS_EULER_MASCHERONI is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_SQRT_2
#error "LUMEX_MATH_CONSTANTS_SQRT_2 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_SQRT_3
#error "LUMEX_MATH_CONSTANTS_SQRT_3 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_SQRT_5
#error "LUMEX_MATH_CONSTANTS_SQRT_5 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_LN_2
#error "LUMEX_MATH_CONSTANTS_LN_2 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_LN_10
#error "LUMEX_MATH_CONSTANTS_LN_10 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_APERY
#error "LUMEX_MATH_CONSTANTS_APERY is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_CATALAN
#error "LUMEX_MATH_CONSTANTS_CATALAN is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_LEMNISCATE
#error "LUMEX_MATH_CONSTANTS_LEMNISCATE is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_GAMMA_1_4
#error "LUMEX_MATH_CONSTANTS_GAMMA_1_4 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_GAMMA_1_3
#error "LUMEX_MATH_CONSTANTS_GAMMA_1_3 is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_BRUNS_CONSTANT_TWIN_PRIMES
#error "LUMEX_MATH_CONSTANTS_BRUNS_CONSTANT_TWIN_PRIMES is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_PLASTIC_NUMBER
#error "LUMEX_MATH_CONSTANTS_PLASTIC_NUMBER is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_RECIPROCAL_PI
#error "LUMEX_MATH_CONSTANTS_RECIPROCAL_PI is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_SQRT_PI
#error "LUMEX_MATH_CONSTANTS_SQRT_PI is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_INVERSE_SQRT_PI
#error "LUMEX_MATH_CONSTANTS_INVERSE_SQRT_PI is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_COPERNICUS_CONSTANT
#error "LUMEX_MATH_CONSTANTS_COPERNICUS_CONSTANT is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_GRAVITY_ACCELERATION
#error "LUMEX_MATH_CONSTANTS_GRAVITY_ACCELERATION is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_LIGHT_SPEED
#error "LUMEX_MATH_CONSTANTS_LIGHT_SPEED is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT
#error "LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT is not defined"
#endif
#ifndef LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT
#error "LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT is not defined"
#endif

// Names that do not exist: a spelling nobody documented is not defined
// quietly (a mistyped name is a compile error at the use, not a silent
// default), and the constants are not variables, so no namespace-scope name
// like them is introduced.
#ifdef LUMEX_MATH_CONSTANTS_TAU
#error "LUMEX_MATH_CONSTANTS_TAU is not part of the header"
#endif
#ifdef LUMEX_MATH_CONSTANTS_E
#error "LUMEX_MATH_CONSTANTS_E is not part of the header"
#endif
#ifdef LUMEX_MATH_CONSTANTS_PHI
#error "LUMEX_MATH_CONSTANTS_PHI is not part of the header"
#endif
#ifdef LUMEX_MATH_CONSTANTS_HALF_PI
#error "LUMEX_MATH_CONSTANTS_HALF_PI is not part of the header"
#endif
#ifdef LUMEX_MATH_CONSTANTS_PI_F
#error "LUMEX_MATH_CONSTANTS_PI_F is not part of the header"
#endif
#ifdef LUMEX_MATH_CONSTANTS_PI_L
#error "LUMEX_MATH_CONSTANTS_PI_L is not part of the header"
#endif

namespace
{
// The macro as the whole operand of an operator, a cast and a stringification.
static_assert (sizeof (LUMEX_MATH_CONSTANTS_PI) == sizeof (double),
               "a constant is a double");
static_assert (
    std::is_floating_point<decltype (+LUMEX_MATH_CONSTANTS_PI)>::value,
    "a constant is floating point");
} // namespace

TEST (MathConstantsMacrosTest,
      GivenAConstantNextToOperators_WhenEvaluated_ThenItIsOneToken)
{
  // A literal token, not an expression: no operator binds into it.
  EXPECT_EQ (2 * LUMEX_MATH_CONSTANTS_PI,
             LUMEX_MATH_CONSTANTS_PI + LUMEX_MATH_CONSTANTS_PI);
  EXPECT_EQ (-LUMEX_MATH_CONSTANTS_PI, 0.0 - LUMEX_MATH_CONSTANTS_PI);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_PI - LUMEX_MATH_CONSTANTS_PI, 0.0);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_LIGHT_SPEED
                 / LUMEX_MATH_CONSTANTS_LIGHT_SPEED,
             1.0);
}

TEST (MathConstantsMacrosTest,
      GivenUnexpandedName_WhenStringified_ThenItIsTheMacroName)
{
  // Stringification without the second level gives the name, with it the
  // digits: the pair the digits test relies on.
#define LUMEX_TEST_RAW(x) #x
  EXPECT_STREQ (LUMEX_TEST_RAW (LUMEX_MATH_CONSTANTS_PI),
                "LUMEX_MATH_CONSTANTS_PI");
#undef LUMEX_TEST_RAW
  EXPECT_EQ (std::string (LUMEX_TEST_STRINGIZE (LUMEX_MATH_CONSTANTS_SQRT_2))
                 .substr (0, 5),
             "1.414");
}

TEST (MathConstantsMacrosTest,
      GivenSuffixPasted_WhenCompiled_ThenTheTypeIsThatOfTheSuffix)
{
  static_assert (std::is_same<decltype (LUMEX_TEST_FLOAT_LITERAL (
                                  LUMEX_MATH_CONSTANTS_PI)),
                              float>::value,
                 "F makes a float");
  static_assert (std::is_same<decltype (LUMEX_TEST_LONG_DOUBLE_LITERAL (
                                  LUMEX_MATH_CONSTANTS_PI)),
                              long double>::value,
                 "L makes a long double");
  static_assert (
      std::is_same<decltype (LUMEX_MATH_CONSTANTS_PI), double>::value,
      "no suffix: a double");
  SUCCEED ();
}

TEST (MathConstantsMacrosTest,
      GivenAConstantInAMacroOfTheUser_WhenExpanded_ThenItNestsOnce)
{
  // A user macro that forwards a constant, as a module would define its own.
#define LUMEX_TEST_USER_TWO_PI (2.0 * LUMEX_MATH_CONSTANTS_PI)
  EXPECT_EQ (LUMEX_TEST_USER_TWO_PI, 2.0 * LUMEX_MATH_CONSTANTS_PI);
#undef LUMEX_TEST_USER_TWO_PI
  // The umbrella header gives the same macros as the header itself.
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_PI, 3.141592653589793);
}

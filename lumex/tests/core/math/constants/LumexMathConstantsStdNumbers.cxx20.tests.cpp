// lumex/tests/core/math/constants/LumexMathConstantsStdNumbers.cxx20.tests.cpp
//
// The constants against <numbers> of C++20, the differential reference the
// standard library itself provides (std::numbers::pi_v<T>, e_v, sqrt2_v,
// sqrt3_v, ln2_v, ln10_v, phi_v, egamma_v, inv_pi_v, inv_sqrtpi_v): the
// library's 36-digit literals and the header's 50-digit ones must give the
// same float, double and long double (libc++ gives its long double constants
// double precision only; see same_as_std). In the same file the constants in
// consteval functions and constinit variables (C++20). A toolchain without
// <numbers> (GCC 8 with -std=c++2a) or without consteval / constinit reports
// GTEST_SKIP or leaves the check out.
#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif

#if defined(__cpp_lib_math_constants)
#include <numbers>
#endif

#include <iomanip>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsSupport.hpp"

using lumex_math_constants_test::constants;

namespace
{
template <typename T>
class MathConstantsStdNumbersTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (MathConstantsStdNumbersTest,
                  lumex_math_constants_test::floating_types,
                  lumex_math_constants_test::type_name);

#if defined(__cpp_consteval)
consteval double
twice_pi ()
{
  return 2.0 * LUMEX_MATH_CONSTANTS_PI;
}
static_assert (twice_pi () == 2.0 * LUMEX_MATH_CONSTANTS_PI,
               "a macro works in a consteval function");
#endif

#if defined(__cpp_constinit)
constinit double g_constinit_pi = LUMEX_MATH_CONSTANTS_PI;
constinit long double g_constinit_pi_long
    = LUMEX_TEST_LONG_DOUBLE_LITERAL (LUMEX_MATH_CONSTANTS_PI);
#endif
} // namespace

#if defined(__cpp_lib_math_constants)

namespace
{
/// `ours` against the standard library's number. libstdc++ and MSVC define
/// pi_v<long double> and the others with long double digits; libc++ (LLVM 23)
/// defines them with a double literal for every type, so its long double
/// constants are the widened double and carry no more than 17 digits. That
/// is the library's narrowness, not a wrong digit here (the Values tests hold
/// the long double digits), so a long double that equals the double rounding
/// of ours is accepted too.
template <typename T>
::testing::AssertionResult
same_as_std (T ours, T theirs)
{
  if (ours == theirs)
    return ::testing::AssertionSuccess ();
  if (std::is_same<T, long double>::value
      && theirs == static_cast<T> (static_cast<double> (ours)))
    return ::testing::AssertionSuccess ()
           << "the library's value has double precision only";
  return ::testing::AssertionFailure ()
         << "ours " << std::setprecision (std::numeric_limits<T>::max_digits10)
         << ours << ", the library's " << theirs;
}
} // namespace

TYPED_TEST (MathConstantsStdNumbersTest,
            GivenStdNumbers_WhenCompared_ThenTheSameNumber)
{
  using K = constants<TypeParam>;
  namespace n = std::numbers;
  EXPECT_TRUE (same_as_std (K::PI (), n::pi_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::EULER_NUMBER (), n::e_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::SQRT_2 (), n::sqrt2_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::SQRT_3 (), n::sqrt3_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::LN_2 (), n::ln2_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::LN_10 (), n::ln10_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::GOLDEN_RATIO (), n::phi_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::EULER_MASCHERONI (), n::egamma_v<TypeParam>));
  EXPECT_TRUE (same_as_std (K::RECIPROCAL_PI (), n::inv_pi_v<TypeParam>));
  EXPECT_TRUE (
      same_as_std (K::INVERSE_SQRT_PI (), n::inv_sqrtpi_v<TypeParam>));
}

TEST (MathConstantsStdNumbersPlainTest,
      GivenStdNumbers_WhenReadAsDouble_ThenTheMacroIsTheSameDouble)
{
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_PI, std::numbers::pi);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_EULER_NUMBER, std::numbers::e);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_SQRT_2, std::numbers::sqrt2);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_SQRT_3, std::numbers::sqrt3);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_LN_2, std::numbers::ln2);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_LN_10, std::numbers::ln10);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_GOLDEN_RATIO, std::numbers::phi);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_EULER_MASCHERONI, std::numbers::egamma);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_RECIPROCAL_PI, std::numbers::inv_pi);
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_INVERSE_SQRT_PI, std::numbers::inv_sqrtpi);
}

#else

TYPED_TEST (MathConstantsStdNumbersTest,
            GivenNoNumbersHeader_WhenBuilt_ThenSkipped)
{
  GTEST_SKIP () << "the C++ library has no <numbers> "
                   "(__cpp_lib_math_constants is not defined)";
}

#endif

TEST (MathConstantsStdNumbersPlainTest,
      GivenConstinitAndConsteval_WhenRead_ThenTheMacroValue)
{
#if defined(__cpp_constinit)
  EXPECT_EQ (g_constinit_pi, LUMEX_MATH_CONSTANTS_PI);
  EXPECT_EQ (g_constinit_pi_long,
             LUMEX_TEST_LONG_DOUBLE_LITERAL (LUMEX_MATH_CONSTANTS_PI));
#endif
#if defined(__cpp_consteval)
  EXPECT_EQ (twice_pi (), 2.0 * LUMEX_MATH_CONSTANTS_PI);
#endif
#if !defined(__cpp_constinit) && !defined(__cpp_consteval)
  GTEST_SKIP () << "the compiler has neither constinit nor consteval";
#endif
}

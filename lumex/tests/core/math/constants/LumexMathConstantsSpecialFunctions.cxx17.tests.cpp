// lumex/tests/core/math/constants/LumexMathConstantsSpecialFunctions.cxx17.tests.cpp
//
// The constants against the special mathematical functions of <cmath> that
// C++17 added (ISO/IEC 29124): the Riemann zeta function, the complete
// elliptic integral of the first kind and the beta function. They give
// Apery's constant, the lemniscate constant and the Gamma values by a route
// that has nothing in common with the digits of the header. libstdc++ has the
// functions from C++17; a library without them
// (`__cpp_lib_math_special_functions` not defined, libc++ today) reports
// GTEST_SKIP. The tolerances are wide (16 ulps; libstdc++ is within 4): the
// special functions are accurate, not correctly rounded.
#include <cmath>

#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsSupport.hpp"

using lumex_math_constants_test::constants;
using lumex_math_constants_test::near_ulps;

namespace
{
template <typename T>
class MathConstantsSpecialFunctionsTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (MathConstantsSpecialFunctionsTest,
                  lumex_math_constants_test::floating_types,
                  lumex_math_constants_test::type_name);
} // namespace

#if defined(__cpp_lib_math_special_functions)

TYPED_TEST (MathConstantsSpecialFunctionsTest,
            GivenZeta_WhenEvaluated_ThenAperyAndPiSquaredOverSix)
{
  using K = constants<TypeParam>;
  EXPECT_TRUE (near_ulps (std::riemann_zeta (TypeParam (3)), K::APERY (), 16));
  // zeta (2) = pi^2 / 6
  EXPECT_TRUE (near_ulps (std::riemann_zeta (TypeParam (2)),
                          K::PI () * K::PI () / 6, 16));
}

TYPED_TEST (MathConstantsSpecialFunctionsTest,
            GivenEllipticIntegral_WhenEvaluated_ThenLemniscateAndGamma)
{
  using K = constants<TypeParam>;
  // K (1/sqrt 2) = Gamma (1/4)^2 / (4 sqrt pi); 2 varpi = 2 sqrt 2 K (1/sqrt
  // 2)
  TypeParam const k = K::SQRT_2 () / 2;
  TypeParam const integral = std::comp_ellint_1 (k);
  EXPECT_TRUE (near_ulps (
      integral, K::GAMMA_1_4 () * K::GAMMA_1_4 () / (4 * K::SQRT_PI ()), 16));
  EXPECT_TRUE (near_ulps (2 * K::SQRT_2 () * integral, K::LEMMISCATE (), 16));
}

TYPED_TEST (MathConstantsSpecialFunctionsTest,
            GivenBeta_WhenEvaluated_ThenGammaQuarterSquaredOverSqrtPi)
{
  using K = constants<TypeParam>;
  // B (1/4, 1/4) = Gamma (1/4)^2 / Gamma (1/2), Gamma (1/2) = sqrt pi
  EXPECT_TRUE (near_ulps (std::beta (TypeParam (1) / 4, TypeParam (1) / 4),
                          K::GAMMA_1_4 () * K::GAMMA_1_4 () / K::SQRT_PI (),
                          16));
  // B (1/3, 2/3) = Gamma (1/3) Gamma (2/3) = 2 pi / sqrt 3, and
  // B (1/3, 1/3) = Gamma (1/3)^2 / Gamma (2/3)
  EXPECT_TRUE (near_ulps (std::beta (TypeParam (1) / 3, TypeParam (2) / 3),
                          2 * K::PI () / K::SQRT_3 (), 16));
}

#else

TYPED_TEST (MathConstantsSpecialFunctionsTest,
            GivenNoSpecialFunctions_WhenBuilt_ThenSkipped)
{
  GTEST_SKIP () << "the C++ library has no special mathematical functions "
                   "(__cpp_lib_math_special_functions is not defined)";
}

#endif

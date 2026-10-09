// lumex/tests/core/math/constants/LumexMathConstantsRelations.cxx11.tests.cpp
//
// Identities between the constants and against the C++ library, at float,
// double and long double: the constants of a type T are the macro literals
// with the suffix of T (3.14...F, 3.14..., 3.14...L), so a long double check
// sees all of the 50 digits. The tolerances are counted in units in the last
// place (ulps) of the result; where a reference is computed here (a series, an
// AGM) it uses the constants as inputs only where the identity needs them. It
// works from C++11, so the suite of every standard runs this file.
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsSupport.hpp"

using lumex_math_constants_test::constants;
using lumex_math_constants_test::near_ulps;
using lumex_math_constants_test::ulp_of;

namespace
{
template <typename T> class MathConstantsRelationsTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (MathConstantsRelationsTest,
                  lumex_math_constants_test::floating_types,
                  lumex_math_constants_test::type_name);
} // namespace

TYPED_TEST (MathConstantsRelationsTest, GivenRoots_WhenSquared_ThenTheRadicand)
{
  using K = constants<TypeParam>;
  EXPECT_TRUE (near_ulps (K::SQRT_2 () * K::SQRT_2 (), TypeParam (2), 2));
  EXPECT_TRUE (near_ulps (K::SQRT_3 () * K::SQRT_3 (), TypeParam (3), 2));
  EXPECT_TRUE (near_ulps (K::SQRT_5 () * K::SQRT_5 (), TypeParam (5), 2));
  EXPECT_TRUE (near_ulps (K::SQRT_PI () * K::SQRT_PI (), K::PI (), 2));
  EXPECT_TRUE (near_ulps (K::INVERSE_SQRT_PI () * K::INVERSE_SQRT_PI (),
                          K::RECIPROCAL_PI (), 2));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenReciprocals_WhenMultiplied_ThenOne)
{
  using K = constants<TypeParam>;
  EXPECT_TRUE (near_ulps (K::PI () * K::RECIPROCAL_PI (), TypeParam (1), 2));
  EXPECT_TRUE (
      near_ulps (K::SQRT_PI () * K::INVERSE_SQRT_PI (), TypeParam (1), 2));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenMetallicMeans_WhenSubstituted_ThenTheyAreRoots)
{
  using K = constants<TypeParam>;
  TypeParam const phi = K::GOLDEN_RATIO ();
  TypeParam const delta = K::SILVER_RATIO ();
  TypeParam const rho = K::PLASTIC_NUMBER ();
  // phi^2 = phi + 1, phi = (1 + sqrt 5) / 2, 1 / phi = phi - 1
  EXPECT_TRUE (near_ulps (phi * phi, phi + 1, 2));
  EXPECT_TRUE (near_ulps (phi, (1 + K::SQRT_5 ()) / 2, 1));
  EXPECT_TRUE (near_ulps (1 / phi, phi - 1, 2));
  // delta = 1 + sqrt 2, delta^2 = 2 delta + 1
  EXPECT_TRUE (near_ulps (delta, 1 + K::SQRT_2 (), 1));
  EXPECT_TRUE (near_ulps (delta * delta, 2 * delta + 1, 2));
  // rho^3 = rho + 1
  EXPECT_TRUE (near_ulps (rho * rho * rho, rho + 1, 3));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenLogarithmsAndExponentials_WhenEvaluated_ThenTheyInvert)
{
  using K = constants<TypeParam>;
  EXPECT_TRUE (near_ulps (std::exp (K::LN_2 ()), TypeParam (2), 4));
  EXPECT_TRUE (near_ulps (std::exp (K::LN_10 ()), TypeParam (10), 4));
  EXPECT_TRUE (near_ulps (std::log (K::EULER_NUMBER ()), TypeParam (1), 4));
  EXPECT_TRUE (near_ulps (std::log (TypeParam (2)), K::LN_2 (), 2));
  EXPECT_TRUE (near_ulps (std::log (TypeParam (10)), K::LN_10 (), 2));
  EXPECT_TRUE (near_ulps (std::exp (TypeParam (1)), K::EULER_NUMBER (), 2));
  EXPECT_TRUE (
      near_ulps (K::LN_10 () / K::LN_2 (), std::log2 (TypeParam (10)), 4));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenEulerNumber_WhenSummed_ThenTheSeriesOfFactorials)
{
  using K = constants<TypeParam>;
  TypeParam sum = 0;
  TypeParam term = 1;
  for (int n = 1; n <= 30; ++n)
    {
      sum += term;
      term /= static_cast<TypeParam> (n);
    }
  EXPECT_TRUE (near_ulps (sum, K::EULER_NUMBER (), 4));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenPi_WhenUsedInTrigonometry_ThenTheFamousValues)
{
  using K = constants<TypeParam>;
  TypeParam const pi = K::PI ();
  // The inverse functions give pi back (pi is the one number all of them
  // return, so these are exact to a few ulps).
  EXPECT_TRUE (near_ulps (4 * std::atan (TypeParam (1)), pi, 2));
  EXPECT_TRUE (near_ulps (std::acos (TypeParam (-1)), pi, 2));
  EXPECT_TRUE (near_ulps (2 * std::asin (TypeParam (1)), pi, 2));
  EXPECT_TRUE (near_ulps (std::atan2 (TypeParam (0), TypeParam (-1)), pi, 2));
  // Machin: pi = 16 atan (1/5) - 4 atan (1/239)
  EXPECT_TRUE (near_ulps (16 * std::atan (TypeParam (1) / 5)
                              - 4 * std::atan (TypeParam (1) / 239),
                          pi, 4));
  // Exact angles through the constants: cos pi = -1, tan (pi/4) = 1,
  // sin (pi/3) = sqrt 3 / 2, cos (pi/4) = sqrt 2 / 2, cos (pi/5) = phi / 2.
  EXPECT_TRUE (near_ulps (std::cos (pi), TypeParam (-1), 2));
  EXPECT_TRUE (near_ulps (std::tan (pi / 4), TypeParam (1), 8));
  EXPECT_TRUE (near_ulps (std::sin (pi / 3), K::SQRT_3 () / 2, 4));
  EXPECT_TRUE (near_ulps (std::cos (pi / 4), K::SQRT_2 () / 2, 4));
  EXPECT_TRUE (near_ulps (std::cos (pi / 5), K::GOLDEN_RATIO () / 2, 4));
  // sin pi is not zero: pi is rounded, and the error of the rounding is
  // what sin returns, which is at most a few ulps of pi.
  EXPECT_LE (std::fabs (std::sin (pi)), 2 * ulp_of (pi));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenCopernicus_WhenScaled_ThenPiOver180)
{
  using K = constants<TypeParam>;
  EXPECT_TRUE (near_ulps (K::COPERNICUS_CONSTANT () * 180, K::PI (), 2));
  EXPECT_TRUE (near_ulps (K::COPERNICUS_CONSTANT (), K::PI () / 180, 2));
  // 90 degrees is a right angle: sin is 1, cos is (nearly) 0.
  EXPECT_TRUE (
      near_ulps (std::sin (90 * K::COPERNICUS_CONSTANT ()), TypeParam (1), 2));
}

namespace
{
/// The arithmetic-geometric mean.
template <typename T>
T
agm (T a, T b)
{
  for (int i = 0; i < 40; ++i)
    {
      T const next_a = (a + b) / 2;
      b = std::sqrt (a * b);
      a = next_a;
    }
  return a;
}
} // namespace

TYPED_TEST (MathConstantsRelationsTest,
            GivenLemniscate_WhenComputedTwoWays_ThenItAgrees)
{
  using K = constants<TypeParam>;
  // 2 varpi = 2 pi / AGM (1, sqrt 2) = Gamma (1/4)^2 / sqrt (2 pi)
  EXPECT_TRUE (near_ulps (2 * K::PI () / agm (TypeParam (1), K::SQRT_2 ()),
                          K::LEMMISCATE (), 4));
  EXPECT_TRUE (
      near_ulps (K::GAMMA_1_4 () * K::GAMMA_1_4 () / std::sqrt (2 * K::PI ()),
                 K::LEMMISCATE (), 4));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenGammaConstants_WhenComparedWithTgamma_ThenTheyAgree)
{
  using K = constants<TypeParam>;
  // tgamma of the C library is accurate to a few ulps, not correctly rounded.
  EXPECT_TRUE (
      near_ulps (std::tgamma (TypeParam (1) / 4), K::GAMMA_1_4 (), 16));
  EXPECT_TRUE (
      near_ulps (std::tgamma (TypeParam (1) / 3), K::GAMMA_1_3 (), 16));
  // Gamma (1/4) Gamma (3/4) = pi sqrt 2, Gamma (3/4) from the reflection
  // formula, as a relation between the two constants and pi.
  EXPECT_TRUE (near_ulps (K::GAMMA_1_4 () * std::tgamma (TypeParam (3) / 4),
                          K::PI () * K::SQRT_2 (), 16));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenApery_WhenSummed_ThenTheSeriesOfZetaThree)
{
  using K = constants<TypeParam>;
  // zeta (3) = 5/2 sum_{n>=1} (-1)^(n-1) / (n^3 C (2n, n))
  TypeParam sum = 0;
  TypeParam central = 1; // C (2n, n)
  for (int n = 1; n <= 80; ++n)
    {
      central = central * static_cast<TypeParam> (2 * (2 * n - 1))
                / static_cast<TypeParam> (n);
      TypeParam const x = static_cast<TypeParam> (n);
      TypeParam const term = 1 / (x * x * x * central);
      sum += (n % 2 == 1) ? term : -term;
    }
  EXPECT_TRUE (near_ulps (TypeParam (5) / 2 * sum, K::APERY (), 8));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenCatalan_WhenSummed_ThenTheFastSeries)
{
  using K = constants<TypeParam>;
  // G = pi/8 ln (2 + sqrt 3) + 3/8 sum_{n>=0} 1 / ((2n + 1)^2 C (2n, n))
  TypeParam sum = 0;
  TypeParam central = 1;
  for (int n = 0; n <= 80; ++n)
    {
      if (n > 0)
        central = central * static_cast<TypeParam> (2 * (2 * n - 1))
                  / static_cast<TypeParam> (n);
      TypeParam const odd = static_cast<TypeParam> (2 * n + 1);
      sum += 1 / (odd * odd * central);
    }
  TypeParam const catalan
      = K::PI () / 8 * std::log (2 + K::SQRT_3 ()) + TypeParam (3) / 8 * sum;
  EXPECT_TRUE (near_ulps (catalan, K::CATALAN (), 8));
}

TYPED_TEST (MathConstantsRelationsTest,
            GivenEulerMascheroni_WhenSummed_ThenEulerMaclaurin)
{
  using K = constants<TypeParam>;
  // gamma = H_N - ln N - 1/(2N) + sum_k B_2k / (2k N^2k), N = 20, k <= 8:
  // the first omitted term is below 1e-23.
  TypeParam const n = 20;
  TypeParam harmonic = 0;
  for (int k = 20; k >= 1; --k)
    harmonic += 1 / static_cast<TypeParam> (k);
  TypeParam const n2 = n * n;
  TypeParam power = n2; // N^2k
  // B_2 .. B_16
  TypeParam const bernoulli[]
      = { TypeParam (1) / 6,   -TypeParam (1) / 30,    TypeParam (1) / 42,
          -TypeParam (1) / 30, TypeParam (5) / 66,     -TypeParam (691) / 2730,
          TypeParam (7) / 6,   -TypeParam (3617) / 510 };
  TypeParam correction = 0;
  for (int k = 1; k <= 8; ++k)
    {
      correction
          += bernoulli[k - 1] / (static_cast<TypeParam> (2 * k) * power);
      power *= n2;
    }
  TypeParam const gamma = harmonic - std::log (n) - 1 / (2 * n) + correction;
  EXPECT_TRUE (near_ulps (gamma, K::EULER_MASCHERONI (), 32));
}

TEST (MathConstantsRelationsPlainTest,
      GivenBrunsConstant_WhenTwinPrimesSummed_ThenItIsAboveThePartialSum)
{
  // Brun's constant is the sum of 1/p + 1/(p+2) over all twin primes; the
  // partial sums grow towards it, slowly (the tail is about 4 C2 / ln x, with
  // C2 = 0.66016...). Below 10^6 the sum is 1.7107...
  std::size_t const limit = 1000000;
  std::vector<bool> composite (limit + 3, false);
  for (std::size_t i = 2; i * i <= limit + 2; ++i)
    if (!composite[i])
      for (std::size_t j = i * i; j <= limit + 2; j += i)
        composite[j] = true;
  double sum = 0.0;
  for (std::size_t p = 3; p <= limit; ++p)
    if (!composite[p] && !composite[p + 2])
      sum += 1.0 / static_cast<double> (p) + 1.0 / static_cast<double> (p + 2);
  EXPECT_GT (LUMEX_MATH_CONSTANTS_BRUNS_CONSTANT_TWIN_PRIMES, sum);
  EXPECT_LT (LUMEX_MATH_CONSTANTS_BRUNS_CONSTANT_TWIN_PRIMES - sum, 0.25);
  EXPECT_GT (sum, 1.7);
}

TEST (MathConstantsRelationsPlainTest,
      GivenPhysicalConstants_WhenCombined_ThenTheSIRelations)
{
  // 1 / c^2 is the permittivity-permeability product (c exact); the standard
  // gravity of 1901 is 9.80665 m/s^2 exactly. Plain arithmetic on the values.
  double const c = LUMEX_MATH_CONSTANTS_LIGHT_SPEED;
  EXPECT_EQ (c, 299792458.0);
  EXPECT_EQ (c * c, 89875517873681764.0); // 299792458^2, exact in a double
  EXPECT_EQ (LUMEX_MATH_CONSTANTS_GRAVITY_ACCELERATION * 100000.0, 980665.0);
}

TEST (MathConstantsRelationsPlainTest,
      GivenOffByOneUlp_WhenCompared_ThenNearUlpsFails)
{
  // The helper itself: it accepts a few ulps and rejects one ulp more.
  double const pi = LUMEX_MATH_CONSTANTS_PI;
  double three_ulps = pi;
  for (int i = 0; i < 3; ++i)
    three_ulps = std::nextafter (three_ulps, 10.0);
  EXPECT_TRUE (near_ulps (three_ulps, pi, 3));
  EXPECT_FALSE (near_ulps (three_ulps, pi, 2));
  EXPECT_FALSE (near_ulps (std::nextafter (three_ulps, 10.0), pi, 3));
}

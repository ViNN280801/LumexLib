// lumex/tests/core/units/api/LumexUnitsDoubleDouble.cxx11.tests.cpp
//
// The double-double factor and the converter: the sweep of the oracle (the
// correctly rounded double of x times the exact ratio of two (unit, prefix)
// items of one kind, compared bit for bit through every route: `convert`,
// the converter, the fused path and Dekker's path), the converter against
// `convert` (values and refusals), the special values, the two-product with
// very large arguments, and the computation of the factor for exponents and
// pi powers up to the integer limits (no write past the integers, the right
// infinity or zero, a NaN where a factor is not computed).
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64)
#define LUMEX_TEST_UNITS_HAS_MXCSR 1
#include <xmmintrin.h>
#else
#define LUMEX_TEST_UNITS_HAS_MXCSR 0
#endif

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/LumexUnitsTestSupport.hpp"

using namespace lumex_test_units;

// Dekker's two-product needs the arithmetic of a double without excess
// precision and without the rewriting of -ffast-math; where it has not, only
// the fused and the exact routes are run (and the two-product stands in for
// none of them).
#if (!defined(FLT_EVAL_METHOD) || FLT_EVAL_METHOD == 0)                       \
    && !defined(__FAST_MATH__) && !defined(_M_FP_FAST)
#define LUMEX_TEST_UNITS_HAS_DEKKER 1
#else
#define LUMEX_TEST_UNITS_HAS_DEKKER 0
#endif

// The fused route is the library's own `multiply_add_fma` (the product exact
// inside `std::fma`). Where `std::fma` is not fused the route is not valid and
// the exact route stands in: mingw-w64 without -mfma evaluates x * y + z with
// two roundings, which is why the library takes another route there.
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED                              \
    || (!defined(__MINGW32__) && !defined(LUMEX_UNITS_TEST_UNSAFE_MATH))
#define LUMEX_TEST_UNITS_HAS_FMA_ROUTE 1
#else
#define LUMEX_TEST_UNITS_HAS_FMA_ROUTE 0
#endif

// The flags of the variant unsafe_math include -fno-signed-zeros, which lets
// the compiler drop the sign of a zero: the sign of a zero result is not
// checked there, and a result below the normal range is not compared (the
// flags also say that a subnormal is read as zero, which the compiler uses
// in a comparison with the smallest normal number).
#if defined(LUMEX_UNITS_TEST_UNSAFE_MATH)
#define LUMEX_TEST_UNITS_SIGNED_ZEROS 0
#define LUMEX_TEST_UNITS_SMALLEST_COMPARED 1e-300
#else
#define LUMEX_TEST_UNITS_SIGNED_ZEROS 1
#define LUMEX_TEST_UNITS_SMALLEST_COMPARED                                    \
  (std::numeric_limits<double>::min ())
#endif

namespace
{
double
fused_route (double value, double high, double low)
{
#if LUMEX_TEST_UNITS_HAS_FMA_ROUTE
  return route_detail::multiply_add_fma (value, high, low);
#else
  return route_detail::multiply_add_exact (value, high, low);
#endif
}

double
two_product_route (double value, double high, double low)
{
#if LUMEX_TEST_UNITS_HAS_DEKKER
  return route_detail::multiply_add_dekker (value, high, low);
#else
  return route_detail::multiply_add_exact (value, high, low);
#endif
}

double
exact_route (double value, double high, double low)
{
  return route_detail::multiply_add_exact (value, high, low);
}

double
apply_route (double value, double high, double low)
{
  converter_t const converter = { high, low };
  return converter.apply (value);
}

std::uint64_t
bits_of (double value)
{
  std::uint64_t bits = 0;
  std::memcpy (&bits, &value, sizeof (bits));
  return bits;
}

double
double_of (std::uint64_t bits)
{
  double value = 0.0;
  std::memcpy (&value, &bits, sizeof (value));
  return value;
}

std::string
describe (double value)
{
  std::ostringstream text;
  text.precision (17);
  text << value << " (0x" << std::hex << bits_of (value) << ")";
  return text.str ();
}

prefixed_unit_t
pair_of (unit_id unit, prefix_id prefix = prefix_id::none)
{
  prefixed_unit_t pair = { unit, prefix };
  return pair;
}

std::vector<prefixed_unit_t>
sweep_items ()
{
  std::vector<prefixed_unit_t> items;
  for (std::size_t index = 0; index < oracle_sweep_item_count; ++index)
    items.push_back (
        pair_of (unit_from_name (oracle_sweep_items[index].unit),
                 prefix_from_name (oracle_sweep_items[index].prefix)));
  return items;
}
} // namespace

// --- the sweep ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTheSweepOfTheOracle_WhenConverted_ThenEveryRouteIsBitEqualToTheCorrectlyRoundedResult)
{
  std::vector<prefixed_unit_t> const items = sweep_items ();
  std::size_t cases = 0;
  std::size_t failures = 0;
  std::size_t shown = 0;
  for (std::size_t row = 0; row < oracle_sweep_count; ++row)
    {
      oracle_sweep_t const &sweep = oracle_sweeps[row];
      prefixed_unit_t const from = items[sweep.from];
      prefixed_unit_t const to = items[sweep.to];
      result_t<converter_t, conversion_error> const converter
          = make_converter (from, to);
      ASSERT_TRUE (converter.has_value ()) << row;
      for (unsigned slot = 0; slot < 4; ++slot)
        {
          double const x
              = oracle_sweep_inputs[(sweep.selection >> (4 * slot)) & 15u];
          std::uint64_t const expected = sweep.expected[slot];
          result_t<double, conversion_error> const converted
              = convert (x, from, to);
          double const routes[5] = {
            converted.has_value () ? converted.value () : 0.0,
            converter.value ().apply (x),
            fused_route (x, converter.value ().high, converter.value ().low),
            two_product_route (x, converter.value ().high,
                               converter.value ().low),
            exact_route (x, converter.value ().high, converter.value ().low)
          };
          static char const *const names[5]
              = { "convert", "apply", "fma", "dekker", "exact" };
          for (int route = 0; route < 5; ++route)
            {
              ++cases;
              if (bits_of (routes[route]) != expected)
                {
                  ++failures;
                  if (shown++ < 10)
                    ADD_FAILURE ()
                        << names[route] << " row " << row << " "
                        << descriptor (from.unit).name << "("
                        << static_cast<int> (from.prefix) << ") -> "
                        << descriptor (to.unit).name << "("
                        << static_cast<int> (to.prefix) << ") of " << x
                        << ": got " << describe (routes[route])
                        << ", expected " << describe (double_of (expected));
                }
            }
        }
    }
  std::cout << "[ units ] sweep: " << oracle_sweep_count << " pairs, " << cases
            << " results through five routes, " << failures
            << " not correctly rounded\n";
  EXPECT_EQ (0u, failures);
  EXPECT_GT (oracle_sweep_count, 5000u);
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTheSweepOfTheOracle_WhenItsCoverageIsCounted_ThenEveryKindPiUnitAndPrefixExtremeIsIn)
{
  std::vector<prefixed_unit_t> const items = sweep_items ();
  std::set<unit_id> units;
  std::set<std::string> kinds;
  std::set<std::pair<unit_id, prefix_id>> extremes;
  std::size_t mixed_pi = 0;
  for (std::size_t row = 0; row < oracle_sweep_count; ++row)
    {
      prefixed_unit_t const from = items[oracle_sweeps[row].from];
      prefixed_unit_t const to = items[oracle_sweeps[row].to];
      units.insert (from.unit);
      units.insert (to.unit);
      kinds.insert (
          kind_names[static_cast<std::size_t> (descriptor (from.unit).kind)]);
      if (from.prefix == prefix_id::quecto || from.prefix == prefix_id::quetta)
        extremes.insert (std::make_pair (from.unit, from.prefix));
      if (descriptor (from.unit).scale.pi_power
          != descriptor (to.unit).scale.pi_power)
        ++mixed_pi;
    }
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      if (unit.scale.pi_power != 0)
        EXPECT_TRUE (units.count (unit_id_at (index)) == 1) << unit.name;
    }
  std::set<std::string> kinds_with_pairs;
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      if (unit.factor != factor_kind::none && !is_affine (unit_id_at (index)))
        {
          for (std::size_t other = 0; other < unit_count; ++other)
            {
              unit_descriptor_t const &second
                  = descriptor (unit_id_at (other));
              if (other != index && second.kind == unit.kind
                  && second.factor != factor_kind::none
                  && !is_affine (unit_id_at (other)))
                kinds_with_pairs.insert (
                    kind_names[static_cast<std::size_t> (unit.kind)]);
            }
          if (unit.prefixes != 0)
            kinds_with_pairs.insert (
                kind_names[static_cast<std::size_t> (unit.kind)]);
        }
    }
  for (std::string const &kind : kinds_with_pairs)
    EXPECT_EQ (1u, kinds.count (kind)) << kind;
  EXPECT_GT (extremes.size (), 100u);
  EXPECT_GT (mixed_pi, 100u);
  EXPECT_GT (units.size (), 90u);
}

// --- the factor ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTheRatiosOfTheOracle_WhenMadeIntoConverters_ThenHighIsTheFactorAndLowIsBelowHalfAnUlp)
{
  for (std::size_t index = 0; index < oracle_ratio_count; ++index)
    {
      oracle_ratio_t const &row = oracle_ratios[index];
      prefixed_unit_t const from = pair_of (unit_from_name (row.from));
      prefixed_unit_t const to = pair_of (unit_from_name (row.to));
      result_t<converter_t, conversion_error> const converter
          = make_converter (from, to);
      ASSERT_TRUE (converter.has_value ()) << row.from << " -> " << row.to;
      result_t<scale_t, conversion_error> const scale = ratio (from, to);
      ASSERT_TRUE (scale.has_value ());
      double const high = converter.value ().high;
      double const low = converter.value ().low;
      EXPECT_EQ (bits_of (scale_factor (scale.value ())), bits_of (high));
      double const half_ulp
          = 0.5
            * (std::nextafter (high, std::numeric_limits<double>::infinity ())
               - high);
      EXPECT_LE (std::fabs (low), half_ulp) << row.from << " -> " << row.to;
      // the sum is the nearest double again: low never changes high
      EXPECT_EQ (high, high + low) << row.from << " -> " << row.to;
      if (row.pi == 0 && row.q == 1 && row.k == 0 && row.p < (1ll << 53))
        EXPECT_EQ (0.0, low)
            << "an exact factor has no low part: " << row.from;
    }
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTheMilliFactor_WhenMade_ThenHighPlusLowHoldsTheDecimalToAbout106Bits)
{
  converter_t const milli = make_converter (make_scale (-3, 1, 1, 0));
  // 1e-3 is not a double: fl (1e-3) is 0.001000000000000000020816...
  EXPECT_EQ (1e-3, milli.high);
  EXPECT_NE (0.0, milli.low);
  EXPECT_LT (milli.low, 0.0);
  EXPECT_EQ (0.7, milli.apply (700.0));
  EXPECT_EQ (0.7, fused_route (700.0, milli.high, milli.low));
#if LUMEX_TEST_UNITS_HAS_DEKKER
  EXPECT_EQ (0.7,
             route_detail::multiply_add_dekker (700.0, milli.high, milli.low));
#endif
  // one and zero scales have an exact factor
  converter_t const one = make_converter (make_scale (0, 1, 1, 0));
  EXPECT_EQ (1.0, one.high);
  EXPECT_EQ (0.0, one.low);
  converter_t const none = make_converter (make_scale (0, 0, 1, 0));
  EXPECT_EQ (0.0, none.high);
  EXPECT_EQ (0.0, none.low);
}

// --- the converter against convert ---

TEST (LumexUnitsDoubleDoubleTest,
      GivenEveryPairOfUnits_WhenAConverterIsMade_ThenItAnswersAsConvertDoes)
{
  std::size_t made = 0;
  std::size_t refused = 0;
  for (std::size_t first = 0; first < unit_count; ++first)
    {
      for (std::size_t second = 0; second < unit_count; ++second)
        {
          prefixed_unit_t const from = pair_of (unit_id_at (first));
          prefixed_unit_t const to = pair_of (unit_id_at (second));
          result_t<converter_t, conversion_error> const converter
              = make_converter (from, to);
          result_t<double, conversion_error> const converted
              = convert (700.5, from, to);
          ASSERT_EQ (converter.has_value (), converted.has_value ())
              << first << " " << second;
          if (converter.has_value ())
            {
              ++made;
              EXPECT_EQ (bits_of (converted.value ()),
                         bits_of (converter.value ().apply (700.5)));
              EXPECT_EQ (bits_of (converted.value ()),
                         bits_of (converter.value () (700.5)));
            }
          else
            {
              ++refused;
              EXPECT_EQ (converted.error (), converter.error ());
            }
        }
    }
  EXPECT_GT (made, 400u);
  EXPECT_GT (refused, 9000u);
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenInvalidAndAffineArguments_WhenAConverterIsMade_ThenItIsRefusedOnceWithTheReason)
{
  prefixed_unit_t const celsius = pair_of (unit_id::degree_celsius);
  prefixed_unit_t const kelvin = pair_of (unit_id::kelvin);
  prefixed_unit_t const metre = pair_of (unit_id::metre);
  prefixed_unit_t const bad_unit
      = { static_cast<unit_id> (500), prefix_id::none };
  prefixed_unit_t const bad_prefix
      = { unit_id::metre, static_cast<prefix_id> (40) };
  EXPECT_EQ (conversion_error::offset_unit,
             make_converter (celsius, kelvin).error ());
  EXPECT_EQ (conversion_error::offset_unit,
             make_converter (kelvin, celsius).error ());
  EXPECT_EQ (conversion_error::different_quantity_kind,
             make_converter (metre, kelvin).error ());
  EXPECT_EQ (conversion_error::invalid_argument,
             make_converter (bad_unit, metre).error ());
  EXPECT_EQ (conversion_error::invalid_argument,
             make_converter (metre, bad_prefix).error ());
  EXPECT_EQ (conversion_error::no_si_value,
             make_converter (pair_of (unit_id::bel), pair_of (unit_id::neper))
                 .error ());
  // the point and the difference keep working through the converter
  EXPECT_EQ (0.0, convert_point (273.15, kelvin, celsius).value ());
  EXPECT_EQ (1.0, convert_difference (1.0, kelvin, celsius).value ());
}

// --- the special values ---

TEST (LumexUnitsDoubleDoubleTest,
      GivenZeroInfinityAndNan_WhenApplied_ThenTheyPassLikeAMultiplication)
{
  converter_t const milli = make_converter (make_scale (-3, 1, 1, 0));
  double const infinity = std::numeric_limits<double>::infinity ();
  for (int path = 0; path < 4; ++path)
    {
      double (*const apply_by[4]) (double, double, double)
          = { &apply_route, &fused_route, &two_product_route, &exact_route };
      auto apply = [&] (double x)
        { return apply_by[path](x, milli.high, milli.low); };
      EXPECT_EQ (0.0, apply (0.0));
      EXPECT_EQ (0.0, apply (-0.0));
#if LUMEX_TEST_UNITS_SIGNED_ZEROS
      EXPECT_FALSE (std::signbit (apply (0.0)));
      EXPECT_TRUE (std::signbit (apply (-0.0)));
#endif
      EXPECT_EQ (infinity, apply (infinity));
      EXPECT_EQ (-infinity, apply (-infinity));
      EXPECT_TRUE (
          std::isnan (apply (std::numeric_limits<double>::quiet_NaN ())));
      EXPECT_EQ (-apply (700.0), apply (-700.0));
      EXPECT_EQ (-0.7, apply (-700.0));
    }
  // a factor that overflows the product: the infinity of the product, not a
  // NaN or the infinity of the (overflowing) correction, which has the other
  // sign when low is negative
  converter_t const huge = make_converter (make_scale (300, 1, 1, 0));
  EXPECT_EQ (infinity, huge.apply (1e100));
  EXPECT_EQ (-infinity, huge.apply (-1e100));
  EXPECT_EQ (1e300, huge.apply (1.0));
  for (double const sign : { 1.0, -1.0 })
    {
      for (double const low : { 1e283, -1e283, 1e290, -1e290 })
        {
          EXPECT_EQ (sign * infinity, fused_route (sign * 1e100, 1e300, low))
              << low;
          EXPECT_EQ (sign * infinity,
                     two_product_route (sign * 1e100, 1e300, low))
              << low;
          EXPECT_EQ (sign * infinity, exact_route (sign * 1e100, 1e300, low))
              << low;
          EXPECT_EQ (sign * infinity, apply_route (sign * 1e100, 1e300, low))
              << low;
        }
    }
}

// --- the two-product with very large arguments ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenVeryLargeArguments_WhenMultipliedAdded_ThenBothPathsAgreeAndOverflowIsInfinity)
{
  converter_t const milli = make_converter (make_scale (-3, 1, 1, 0));
  EXPECT_EQ (1e297, milli.apply (1e300));
  EXPECT_EQ (1e305, milli.apply (1e308));
  EXPECT_EQ (1e297, fused_route (1e300, milli.high, milli.low));
#if LUMEX_TEST_UNITS_HAS_DEKKER
  EXPECT_EQ (1e297,
             route_detail::multiply_add_dekker (1e300, milli.high, milli.low));
  EXPECT_EQ (1e305,
             route_detail::multiply_add_dekker (1e308, milli.high, milli.low));
  EXPECT_EQ (-1e305, route_detail::multiply_add_dekker (-1e308, milli.high,
                                                        milli.low));
  double const infinity = std::numeric_limits<double>::infinity ();
  EXPECT_EQ (infinity,
             route_detail::multiply_add_dekker (1e200, 1e200, 1e180));
  EXPECT_EQ (-infinity,
             route_detail::multiply_add_dekker (-1e200, 1e200, 1e180));
  // a large factor and a large value, and each alone
  double const arguments[][3]
      = { { 3.0, 1e300, 1e283 }, { 1e-5, 1e300, -1e283 },
          { 1e301, 7.0, 1e-16 }, { 1e301, 1e-10, 1e-27 },
          { 1e307, 1.5, 1e-17 }, { 1e155, 1e155, 1e139 } };
  for (auto const &row : arguments)
    EXPECT_EQ (
        bits_of (fused_route (row[0], row[1], row[2])),
        bits_of (route_detail::multiply_add_dekker (row[0], row[1], row[2])))
        << row[0] << " " << row[1];
#endif
}

TEST (LumexUnitsDoubleDoubleTest,
      GivenTheRouteMacro_WhenCompiled_ThenApplyTakesItAndItsNamespaceNamesIt)
{
  EXPECT_TRUE (LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
               || LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
               || LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_EXACT);
  converter_t const third = make_converter (make_scale (0, 1, 3, 0));
  double const through_apply = third.apply (7.0);
  char const *name = "";
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
  name = "route_fused";
  EXPECT_EQ (
      bits_of (route_detail::multiply_add_fma (7.0, third.high, third.low)),
      bits_of (through_apply));
#elif LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
  name = "route_two_product";
  EXPECT_EQ (
      bits_of (route_detail::multiply_add_dekker (7.0, third.high, third.low)),
      bits_of (through_apply));
#else
  name = "route_exact";
  EXPECT_EQ (
      bits_of (route_detail::multiply_add_exact (7.0, third.high, third.low)),
      bits_of (through_apply));
#endif
  // the converter type is in the inline namespace of the route, so the
  // linker never merges the `apply` of two routes
  EXPECT_NE (std::string::npos,
             std::string (typeid (converter_t).name ()).find (name));
  EXPECT_NE (
      std::string::npos,
      std::string (typeid (make_converter (make_scale (0, 1, 1, 0))).name ())
          .find (name));
  std::cout << "[ units ] apply () uses the route " << name << "\n";
}

// --- the magnitude of the product ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenAProductNearTheLargestDouble_WhenConverted_ThenEveryRouteIsFiniteAndExact)
{
  // The reviewer's failing case: 1.7976931348443387e248 metres in
  // quetta-metres to quecto-metres is 1.7976931348443388e308, 1 - 1e-11 of the
  // largest double. The halves of the split product overflowed.
  result_t<double, conversion_error> const reviewer = convert (
      1.7976931348443387e248, pair_of (unit_id::metre, prefix_id::quetta),
      pair_of (unit_id::metre, prefix_id::quecto));
  ASSERT_TRUE (reviewer.has_value ());
  EXPECT_EQ (1.7976931348443388e308, reviewer.value ());
  converter_t const scale = make_converter (make_scale (60, 1, 1, 0));
  double const largest = std::numeric_limits<double>::max ();
  double const base = largest / scale.high;
  std::size_t checked = 0;
  for (int step = 1; step <= 4096; ++step)
    {
      double const x
          = base * (1.0 - std::ldexp (static_cast<double> (step), -34));
      double const exact = exact_route (x, scale.high, scale.low);
      ASSERT_TRUE (std::isfinite (exact)) << step;
      EXPECT_EQ (bits_of (exact),
                 bits_of (fused_route (x, scale.high, scale.low)))
          << step;
      EXPECT_EQ (bits_of (exact),
                 bits_of (two_product_route (x, scale.high, scale.low)))
          << step;
      EXPECT_EQ (bits_of (exact), bits_of (scale.apply (x))) << step;
      EXPECT_EQ (bits_of (exact),
                 bits_of (apply_route (-x, scale.high, scale.low) * -1.0))
          << step;
      ++checked;
    }
  // above the largest double the product is infinity, on every route
  double const over = base * (1.0 + std::ldexp (1.0, -30));
  double const infinity = std::numeric_limits<double>::infinity ();
  EXPECT_EQ (infinity, fused_route (over, scale.high, scale.low));
  EXPECT_EQ (infinity, two_product_route (over, scale.high, scale.low));
  EXPECT_EQ (infinity, exact_route (over, scale.high, scale.low));
  EXPECT_EQ (4096u, checked);
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenAProductNearTheSmallestNormalDouble_WhenConverted_ThenEveryRouteIsExact)
{
  // The reviewer's failing value: the error term of the two-product and
  // value * low fell below 2^-1022.
  converter_t const reviewer
      = make_converter (make_scale (-182, 41271077348469884, 1, 0));
  double const x = 7.209311865789861e-142;
  EXPECT_EQ (2.975360676422551e-307, reviewer.apply (x));
  EXPECT_EQ (2.975360676422551e-307,
             fused_route (x, reviewer.high, reviewer.low));
  EXPECT_EQ (2.975360676422551e-307,
             two_product_route (x, reviewer.high, reviewer.low));
  EXPECT_EQ (2.975360676422551e-307,
             exact_route (x, reviewer.high, reviewer.low));
  // a band from 2^-1022 to 2^-940 of the result, in steps of 2^(1/150)
  converter_t const scale = make_converter (make_scale (-250, 3, 7, 0));
  double const smallest = std::numeric_limits<double>::min ();
  double const base = smallest / scale.high;
  std::size_t checked = 0;
  for (int step = 0; step <= 12000; ++step)
    {
      double const x_step
          = base * std::pow (2.0, static_cast<double> (step) / 150.0);
      double const exact = exact_route (x_step, scale.high, scale.low);
      if (!(exact >= smallest))
        continue;
      EXPECT_EQ (bits_of (exact),
                 bits_of (fused_route (x_step, scale.high, scale.low)))
          << step;
      EXPECT_EQ (bits_of (exact),
                 bits_of (two_product_route (x_step, scale.high, scale.low)))
          << step;
      EXPECT_EQ (bits_of (exact), bits_of (scale.apply (x_step))) << step;
      ++checked;
    }
  EXPECT_GT (checked, 11000u);
}

TEST (LumexUnitsDoubleDoubleTest,
      GivenTheExactRoute_WhenCompared_ThenItIsTheRoundedIntegerProduct)
{
  // x * (high + low) with an exactly representable sum and a tie: 1 + 2^-53
  // is the sum of high = 1 and low = 2^-53, a tie between 1 and 1 + 2^-52
  // that goes to the even 1; one hair above goes up.
  EXPECT_EQ (1.0, exact_route (1.0, 1.0, std::ldexp (1.0, -53)));
  EXPECT_EQ (
      1.0 + std::ldexp (1.0, -51),
      exact_route (1.0, 1.0 + std::ldexp (1.0, -52), std::ldexp (1.0, -53)));
  EXPECT_EQ (
      1.0 + std::ldexp (1.0, -52),
      exact_route (1.0, 1.0, std::ldexp (1.0, -53) + std::ldexp (1.0, -105)));
  EXPECT_EQ (-1.0, exact_route (-1.0, 1.0, std::ldexp (1.0, -53)));
  // a negative low part, and a low part far below the last bit of the high one
  EXPECT_EQ (1.0, exact_route (1.0, 1.0, -std::ldexp (1.0, -54)));
  EXPECT_EQ (1.0 - std::ldexp (1.0, -53),
             exact_route (1.0, 1.0, -std::ldexp (1.0, -53)));
  EXPECT_EQ (3.0, exact_route (3.0, 1.0, 1e-300));
}

// --- one rounding in the subnormal range ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenAQuotientNearATieInTheSubnormalRange_WhenRounded_ThenItIsRoundedOnceWithTheRemainder)
{
  // value = (m + 1/2 + eps) * 2^-1074 with m below 2^52: a subnormal double
  // keeps m bits, so the exact value is above the tie between m and m + 1.
  // Rounded first to 53 bits (which drops eps) and then by ldexp, the tie goes
  // to the even m; rounded once it goes up.
  std::uint64_t const even = (static_cast<std::uint64_t> (1) << 51) + 2;
  std::uint64_t const odd = even + 1;
  for (std::uint64_t const m : { even, odd })
    {
      route_detail::big_t numerator;
      route_detail::big_t denominator;
      route_detail::big_t one;
      route_detail::big_set (numerator, 2 * m + 1);
      route_detail::big_shift_left (numerator, 61);
      route_detail::big_set (denominator, 1);
      route_detail::big_shift_left (denominator, 62);
      route_detail::big_set (one, 1);
      // an exact tie: to the even neighbour
      double const tie
          = route_detail::big_ratio_to_double (numerator, denominator, -1074);
      EXPECT_EQ (
          std::ldexp (static_cast<double> (m % 2 == 0 ? m : m + 1), -1074),
          tie)
          << m;
      // a hair above the tie: up
      route_detail::big_add (numerator, one);
      double const above
          = route_detail::big_ratio_to_double (numerator, denominator, -1074);
      EXPECT_EQ (std::ldexp (static_cast<double> (m + 1), -1074), above) << m;
    }
  // the smallest subnormal, a tie below it and the value just above the tie
  route_detail::big_t numerator;
  route_detail::big_t denominator;
  route_detail::big_set (numerator, 1);
  route_detail::big_set (denominator, 2);
  EXPECT_EQ (
      0.0, route_detail::big_ratio_to_double (numerator, denominator, -1074));
  route_detail::big_set (numerator, 3);
  route_detail::big_shift_left (numerator, 60);
  route_detail::big_set (denominator, 1);
  route_detail::big_shift_left (denominator, 62);
  // 3 * 2^60 / 2^62 = 3/4 of 2^-1074
  EXPECT_EQ (std::ldexp (1.0, -1074), route_detail::big_ratio_to_double (
                                          numerator, denominator, -1074));
}

// --- the size of the integers ---

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenDecimalExponentsBeyondTheDoubleRange_WhenTheFactorIsMade_ThenItIsInfinityOrZero)
{
  double const infinity = std::numeric_limits<double>::infinity ();
  std::int64_t const big = std::numeric_limits<std::int64_t>::max ();
  EXPECT_EQ (1e281, scale_factor (make_scale (281, 1, 1, 0)));
  EXPECT_EQ (1e-281, scale_factor (make_scale (-281, 1, 1, 0)));
  EXPECT_EQ (1e308, scale_factor (make_scale (308, 1, 1, 0)));
  EXPECT_EQ (1e-320, scale_factor (make_scale (-320, 1, 1, 0)));
  EXPECT_EQ (infinity, scale_factor (make_scale (309, 1, 1, 0)));
  EXPECT_EQ (infinity, scale_factor (make_scale (400, 1, 1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (-400, 1, 1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (-345, 1, 1, 0)));
  // a large numerator or denominator moves the limit
  EXPECT_FALSE (std::isinf (scale_factor (make_scale (290, 1, big, 0))));
  EXPECT_NEAR (1e290 / 9223372036854775807.0,
               scale_factor (make_scale (290, 1, big, 0)), 1e257);
  EXPECT_EQ (infinity, scale_factor (make_scale (290, big, 1, 0)));
  EXPECT_FALSE (std::isinf (scale_factor (make_scale (280, big, 1, 0))));
  EXPECT_GT (scale_factor (make_scale (-290, big, 1, 0)), 0.0);
  EXPECT_LT (scale_factor (make_scale (-290, big, 1, 0)), 1e-270);
  EXPECT_EQ (0.0, scale_factor (make_scale (-340, 1, big, 0)));
  // the extremes of the exponent
  EXPECT_EQ (infinity,
             scale_factor (make_scale (
                 std::numeric_limits<std::int32_t>::max (), 1, 1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (
                      std::numeric_limits<std::int32_t>::min (), 1, 1, 0)));
  EXPECT_EQ (infinity,
             scale_factor (make_scale (
                 std::numeric_limits<std::int32_t>::max (), big, 1, 127)));
  EXPECT_EQ (
      0.0, scale_factor (make_scale (std::numeric_limits<std::int32_t>::min (),
                                     1, big, -128)));
  // zero and negative numerators and denominators have no value
  EXPECT_EQ (0.0, scale_factor (make_scale (0, 0, 1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (0, -1, 1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (0, 1, 0, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (0, 1, -1, 0)));
  EXPECT_EQ (0.0, scale_factor (make_scale (500, 0, 1, 0)));
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenPiPowersBeyondTheLimit_WhenTheFactorIsMade_ThenItIsInfinityZeroOrNanNeverARead)
{
  std::int64_t const big = std::numeric_limits<std::int64_t>::max ();
  double const infinity = std::numeric_limits<double>::infinity ();
  // The pi powers 3..127 (the int8 range) with the exponents that put the
  // value far above or far below the double range, and with those that keep it
  // in: out of the range the answer is infinity or zero, in the range and over
  // the limit it is a NaN, and nothing is read or written past the integers.
  for (int power = 3; power <= 127; ++power)
    {
      std::int8_t const n = static_cast<std::int8_t> (power);
      std::int8_t const m = static_cast<std::int8_t> (-power);
      double const pi_logarithm = power * 1.6514961294723187;
      EXPECT_EQ (infinity, scale_factor (make_scale (400, 1, 1, n))) << power;
      EXPECT_EQ (0.0, scale_factor (make_scale (-400, 1, 1, n))) << power;
      EXPECT_EQ (infinity, scale_factor (make_scale (400, big, 1, m)))
          << power;
      EXPECT_EQ (0.0, scale_factor (make_scale (-400, 1, big, m))) << power;
      // 1e280 * big * pi^power, 1e-280 / big / pi^power
      if (63.0 + 280 * 3.3219280948873623 + pi_logarithm > 1100.0)
        {
          EXPECT_EQ (infinity, scale_factor (make_scale (280, big, 1, n)))
              << power;
          EXPECT_EQ (0.0, scale_factor (make_scale (-280, 1, big, m)))
              << power;
        }
      // in the range but over the limit of the powers: not computed
      if (power > scale_pi_power_limit)
        {
          EXPECT_TRUE (std::isnan (scale_factor (make_scale (0, 1, 1, n))))
              << power;
          EXPECT_TRUE (std::isnan (scale_factor (make_scale (-30, 1, 1, n))))
              << power;
          EXPECT_TRUE (std::isnan (scale_factor (make_scale (0, 1, 1, m))))
              << power;
        }
    }
  EXPECT_TRUE (std::isnan (scale_factor (
      make_scale (0, 1, 1, std::numeric_limits<std::int8_t>::max ()))));
  EXPECT_TRUE (std::isnan (scale_factor (
      make_scale (0, 1, 1, std::numeric_limits<std::int8_t>::min ()))));
  EXPECT_TRUE (std::isnan (apply_scale (make_scale (0, 1, 1, 100), 1.0)));
  // the powers up to the limit are computed, with an exponent that keeps
  // them in the range
  EXPECT_FALSE (
      std::isnan (scale_factor (make_scale (0, 1, 1, scale_pi_power_limit))));
  EXPECT_FALSE (
      std::isnan (scale_factor (make_scale (0, 1, 1, -scale_pi_power_limit))));
  EXPECT_NEAR (std::pow (3.14159265358979323846, 16),
               scale_factor (make_scale (0, 1, 1, 16)),
               1e-9 * std::pow (3.14159265358979323846, 16));
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenEveryExponentAndPiPowerNearTheEdges_WhenTheFactorIsMade_ThenItIsComputedExactlyOrOutOfRange)
{
  std::int64_t const big = std::numeric_limits<std::int64_t>::max ();
  std::int64_t const numerators[] = { 1, 3, big, big - 1, 1000000007 };
  std::int64_t const denominators[] = { 1, 7, big, big - 2, 998244353 };
  std::size_t computed = 0;
  std::size_t out_of_range = 0;
  for (int power = -scale_pi_power_limit; power <= scale_pi_power_limit;
       ++power)
    {
      for (int exponent = -480; exponent <= 480; ++exponent)
        {
          for (std::int64_t const numerator : numerators)
            {
              for (std::int64_t const denominator : denominators)
                {
                  scale_t const scale
                      = make_scale (exponent, numerator, denominator, power);
                  converter_t const converter = make_converter (scale);
                  // with |pi_power| up to the limit nothing is refused
                  ASSERT_FALSE (std::isnan (converter.high))
                      << exponent << " " << numerator << " " << denominator
                      << " " << power;
                  double const logarithm
                      = std::log2 (static_cast<double> (numerator))
                        - std::log2 (static_cast<double> (denominator))
                        + exponent * 3.3219280948873623
                        + power * 1.6514961294723187;
                  if (logarithm > 1030.0)
                    {
                      ++out_of_range;
                      ASSERT_TRUE (std::isinf (converter.high))
                          << exponent << " " << power;
                    }
                  else if (logarithm < -1080.0)
                    {
                      ++out_of_range;
                      ASSERT_EQ (0.0, converter.high)
                          << exponent << " " << power;
                    }
                  else if (logarithm < 1020.0 && logarithm > -1020.0)
                    {
                      ++computed;
                      ASSERT_TRUE (std::isfinite (converter.high));
                      ASSERT_GT (converter.high, 0.0);
                      EXPECT_NEAR (std::log2 (converter.high), logarithm,
                                   1e-9 * (1.0 + std::fabs (logarithm)))
                          << exponent << " " << power;
                    }
                }
            }
        }
    }
  EXPECT_GT (computed, 20000u);
  EXPECT_GT (out_of_range, 20000u);
}

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTheScalesOfTheCatalog_WhenTheFactorIsMadeOfEveryPrefixedUnit_ThenItIsNeverNan)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const unit = unit_id_at (index);
      for (std::size_t prefix = 0; prefix <= prefix_count; ++prefix)
        {
          prefixed_unit_t const pair = pair_of (
              unit, prefix == prefix_count ? prefix_id::none
                                           : static_cast<prefix_id> (prefix));
          if (pair.prefix != prefix_id::none
              && !takes_prefix (unit, pair.prefix))
            continue;
          scale_t const scale = scale_of (pair);
          converter_t const converter = make_converter (scale);
          EXPECT_FALSE (std::isnan (converter.high)) << descriptor (unit).name;
          if (descriptor (unit).factor != factor_kind::none)
            EXPECT_GT (converter.high, 0.0) << descriptor (unit).name;
        }
    }
}

// --- the sweep of the exponents, the split, the overflow, the x87 product ---

#if LUMEX_TEST_UNITS_HAS_DEKKER
// Random factors and values of exponents up to +-600 (the product is then
// anywhere from 2^-1200 to 2^1200: the scaled paths, the overflow and the
// underflow), the fused and the two-product routes against the exact one,
// bit for bit. A result below the normal range is rounded twice by the first
// two routes and is left out. This is the test that a contraction of the
// split and of the product into the final sum (GCC with FMA instructions and
// the default `-ffp-contract=fast`) fails once the arguments are scaled: the
// exponents of +-100 pass, +-600 do not (1715 of 395712 results).
TEST (
    LumexUnitsDoubleDoubleTest,
    GivenRandomFactorsOfWideExponents_WhenMultipliedAdded_ThenRoutesAgreeWithTheExactOne)
{
  std::uint64_t state = 205;
  auto next = [&state] ()
    {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      return state >> 11;
    };
  std::size_t compared = 0;
  std::size_t overflowed = 0;
  std::size_t different_fused = 0;
  std::size_t different_two_product = 0;
  std::string first_difference;
  for (int index = 0; index < 400000; ++index)
    {
      double const sign_x = (next () & 1u) != 0 ? -1.0 : 1.0;
      double const sign_high = (next () & 1u) != 0 ? -1.0 : 1.0;
      double const x
          = sign_x
            * std::ldexp (
                1.0 + static_cast<double> (next ()) / 9007199254740992.0,
                static_cast<int> (next () % 1201) - 600);
      double const high
          = sign_high
            * std::ldexp (
                1.0 + static_cast<double> (next ()) / 9007199254740992.0,
                static_cast<int> (next () % 1201) - 600);
      double const low
          = high
            * std::ldexp (
                static_cast<double> (next ()) / 9007199254740992.0 - 0.5, -53);
      if (low == 0.0)
        continue;
      double const expected = exact_route (x, high, low);
      if (!(std::fabs (expected) >= LUMEX_TEST_UNITS_SMALLEST_COMPARED
            || std::isinf (expected)))
        continue;
      ++compared;
      if (std::isinf (expected))
        ++overflowed;
      std::uint64_t const expected_bits = bits_of (expected);
      if (bits_of (fused_route (x, high, low)) != expected_bits)
        ++different_fused;
      if (bits_of (route_detail::multiply_add_dekker (x, high, low))
          != expected_bits)
        {
          ++different_two_product;
          if (first_difference.empty ())
            first_difference
                = describe (x) + " * " + describe (high) + " + "
                  + describe (low) + " gave "
                  + describe (route_detail::multiply_add_dekker (x, high, low))
                  + ", expected " + describe (expected);
        }
    }
  EXPECT_GT (compared, 150000u);
  EXPECT_GT (overflowed, 1000u);
  EXPECT_EQ (0u, different_fused);
  EXPECT_EQ (0u, different_two_product) << first_difference;
}

// The halves of the split: the first 26 bits and the rest, exactly, for any
// value of the scaled range, so that every product of two halves is exact.
TEST (LumexUnitsDoubleDoubleTest,
      GivenARandomDouble_WhenSplit_ThenTheHalvesHaveTwentySixBitsAndSumToIt)
{
  std::uint64_t state = 7;
  auto next = [&state] ()
    {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      return state >> 11;
    };
  double const specials[] = { 1.0,
                              3.0,
                              134217729.0,
                              1.5,
                              9007199254740991.0,
                              1.0 - 1.1102230246251565e-16,
                              6.6e299,
                              1e-271 };
  for (int index = 0; index < 100000 + 8; ++index)
    {
      double const value
          = index < 8
                ? specials[index]
                : (next () % 2 == 0 ? 1.0 : -1.0)
                      * std::ldexp (1.0
                                        + static_cast<double> (next ())
                                              / 9007199254740992.0,
                                    static_cast<int> (next () % 1800) - 900);
      double upper = 0.0;
      double lower = 0.0;
      route_detail::split_double (value, upper, lower);
      ASSERT_EQ (value, upper + lower) << describe (value);
      // upper has at most 26 significant bits
      int exponent = 0;
      double const upper_fraction = std::frexp (upper, &exponent);
      EXPECT_EQ (0.0, std::fmod (std::ldexp (upper_fraction, 26), 1.0))
          << describe (value);
      int lower_exponent = 0;
      double const lower_fraction = std::frexp (lower, &lower_exponent);
      EXPECT_EQ (0.0, std::fmod (std::ldexp (lower_fraction, 26), 1.0))
          << describe (value);
      EXPECT_LE (std::fabs (lower), std::ldexp (std::fabs (value), -26))
          << describe (value);
    }
}
#endif

// A product that overflows in a double but whose exact value rounds to the
// largest double is that double, not the infinity, on every route; one that
// rounds to the infinity is the infinity.
TEST (
    LumexUnitsDoubleDoubleTest,
    GivenAProductThatRoundsToTheLargestDouble_WhenApplied_ThenItIsNotInfinity)
{
  double const largest = std::numeric_limits<double>::max ();
  double const infinity = std::numeric_limits<double>::infinity ();
  converter_t const quetta_quecto = make_converter (make_scale (25, 1, 1, 0));
  for (double const sign : { 1.0, -1.0 })
    {
      double const value = sign * 1.7976931348623158e283;
      // 1e25 is 1e25 - 905969664 + 905969664 as a pair, and RN (value * high)
      // is the infinity
      EXPECT_TRUE (std::isinf (value * quetta_quecto.high));
      EXPECT_EQ (sign * largest,
                 fused_route (value, quetta_quecto.high, quetta_quecto.low));
      EXPECT_EQ (sign * largest,
                 exact_route (value, quetta_quecto.high, quetta_quecto.low));
      EXPECT_EQ (sign * largest,
                 apply_route (value, quetta_quecto.high, quetta_quecto.low));
      EXPECT_EQ (sign * largest,
                 apply_scale (make_scale (25, 1, 1, 0), value));
#if LUMEX_TEST_UNITS_HAS_DEKKER
      EXPECT_EQ (sign * largest,
                 route_detail::multiply_add_dekker (value, quetta_quecto.high,
                                                    quetta_quecto.low));
#endif
      // twice as large: the infinity
      EXPECT_EQ (
          sign * infinity,
          fused_route (sign * 3.6e283, quetta_quecto.high, quetta_quecto.low));
      EXPECT_EQ (
          sign * infinity,
          exact_route (sign * 3.6e283, quetta_quecto.high, quetta_quecto.low));
#if LUMEX_TEST_UNITS_HAS_DEKKER
      EXPECT_EQ (sign * infinity,
                 route_detail::multiply_add_dekker (
                     sign * 3.6e283, quetta_quecto.high, quetta_quecto.low));
#endif
    }
  // the same near the largest double with a plain large factor and value
  EXPECT_EQ (largest, exact_route (1.7976931348623157e308, 1.0, -1e-17));
  EXPECT_EQ (largest, fused_route (1.7976931348623157e308, 1.0, -1e-17));
#if LUMEX_TEST_UNITS_HAS_DEKKER
  EXPECT_EQ (largest, route_detail::multiply_add_dekker (
                          1.7976931348623157e308, 1.0, -1e-17));
#endif
}

// The product of two doubles in integers (the exact route on a machine with
// excess precision: x87 rounds a product to 64 bits and then to 53).
TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTwoDoublesWhoseExtendedProductIsAHalfwayCase_WhenMultipliedInIntegers_ThenRoundedOnce)
{
  // 9.29992966962503e-10 * 7150989373298505 = 6650369.82399120650..., the
  // exact product is just below the middle of two doubles that the 64-bit
  // product rounds up to
  double const x = 9.29992966962503e-10;
  double const h = 7150989373298505.0;
  double const correct = 6650369.8239912065;
  EXPECT_EQ (bits_of (correct),
             bits_of (route_detail::multiply_exact_product (x, h)));
  EXPECT_EQ (bits_of (correct), bits_of (exact_route (x, h, 0.0)));
  EXPECT_EQ (bits_of (-correct), bits_of (exact_route (-x, h, 0.0)));
  EXPECT_EQ (bits_of (-correct), bits_of (exact_route (x, -h, 0.0)));
  EXPECT_EQ (bits_of (correct), bits_of (exact_route (-x, -h, 0.0)));
  // the special values and the limits
  double const infinity = std::numeric_limits<double>::infinity ();
  EXPECT_EQ (0.0, route_detail::multiply_exact_product (0.0, 3.0));
#if LUMEX_TEST_UNITS_SIGNED_ZEROS
  EXPECT_TRUE (
      std::signbit (route_detail::multiply_exact_product (-0.0, 3.0)));
#endif
  EXPECT_EQ (infinity, route_detail::multiply_exact_product (infinity, 3.0));
  EXPECT_TRUE (std::isnan (route_detail::multiply_exact_product (
      std::numeric_limits<double>::quiet_NaN (), 3.0)));
  EXPECT_EQ (infinity, route_detail::multiply_exact_product (1e200, 1e200));
  EXPECT_EQ (-infinity, route_detail::multiply_exact_product (-1e200, 1e200));
  EXPECT_EQ (std::numeric_limits<double>::max (),
             route_detail::multiply_exact_product (
                 std::numeric_limits<double>::max (), 1.0));
  EXPECT_EQ (0.0, route_detail::multiply_exact_product (1e-200, 1e-200));
#if LUMEX_TEST_UNITS_SIGNED_ZEROS
  EXPECT_TRUE (
      std::signbit (route_detail::multiply_exact_product (-1e-200, 1e-200)));
#endif
  // a subnormal result is rounded once, to the nearest even
  EXPECT_EQ (std::numeric_limits<double>::denorm_min (),
             route_detail::multiply_exact_product (
                 std::numeric_limits<double>::denorm_min (), 1.0));
  EXPECT_EQ (0.0, route_detail::multiply_exact_product (
                      std::numeric_limits<double>::denorm_min (), 0.5));
  EXPECT_EQ (2 * std::numeric_limits<double>::denorm_min (),
             route_detail::multiply_exact_product (
                 3 * std::numeric_limits<double>::denorm_min (), 0.5));
  // random products agree with the machine product (no excess precision
  // here, or the sweep below finds the difference)
  std::uint64_t state = 99;
  auto next = [&state] ()
    {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      return state >> 11;
    };
  std::size_t different = 0;
  for (int index = 0; index < 100000; ++index)
    {
      double const left = std::ldexp (
          1.0 + static_cast<double> (next ()) / 9007199254740992.0,
          static_cast<int> (next () % 400) - 200);
      double const right = std::ldexp (
          1.0 + static_cast<double> (next ()) / 9007199254740992.0,
          static_cast<int> (next () % 400) - 200);
      volatile double const machine = left * right;
#if !LUMEX_UNITS_EXCESS_PRECISION
      if (bits_of (machine)
          != bits_of (route_detail::multiply_exact_product (left, right)))
        ++different;
#else
      (void)machine;
#endif
    }
  EXPECT_EQ (0u, different);
}

// GCC 12 and later keeps the cores out of the reach of reassociation only
// where `__ASSOCIATIVE_MATH__` tells that it is on (the attribute that
// protects them makes GCC refuse to inline `multiply_add_scaled`, so a default
// build does not carry it). The variant unsafe_math has to be the side that
// the attribute protects: with -funsafe-math-optimizations GCC 12 and later
// defines the macro, and the random operands of the next test then run on the
// protected cores. A mutation of the gate that drops the macro from it leaves
// those cores bare in this variant and fails that test.
TEST (LumexUnitsDoubleDoubleTest,
      GivenTheUnsafeMathVariant_WhenGcc12OrLater_ThenAssociativeMathIsOn)
{
#if defined(LUMEX_UNITS_TEST_UNSAFE_MATH) && defined(__GNUC__)                \
    && !defined(__clang__) && __GNUC__ >= 12
#if defined(__ASSOCIATIVE_MATH__)
  EXPECT_TRUE (true);
#else
  ADD_FAILURE () << "-funsafe-math-optimizations does not define "
                    "__ASSOCIATIVE_MATH__: the gate of the GCC attribute "
                    "would leave the cores bare in the variant";
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12              \
    && !defined(__FAST_MATH__) && !defined(LUMEX_UNITS_TEST_UNSAFE_MATH)
  // a default build does not reassociate, and carries no attribute
#if defined(__ASSOCIATIVE_MATH__)
  ADD_FAILURE () << "a default build defines __ASSOCIATIVE_MATH__";
#else
  EXPECT_TRUE (true);
#endif
#else
  SUCCEED ();
#endif
}

// A compiler that may reassociate (the variant unsafe_math compiles the suite
// with -funsafe-math-optimizations, cl.exe and clang-cl with /fp:fast)
// rewrites the two-product (35237 of 395712 random results of GCC 9 to 13
// differ when the cores are not protected) and a fused product that has no
// hardware operation under it. The route of the build has to be right under
// them whatever the flags are: the route that the header takes under the
// flags of the variant, and random operands against the product in integers.
TEST (LumexUnitsDoubleDoubleTest,
      GivenRandomOperands_WhenApplied_ThenTheRouteOfTheBuildIsCorrectlyRounded)
{
#if defined(LUMEX_UNITS_TEST_UNSAFE_MATH)
  // The route that the header takes under the flags of the variant: the
  // exact one where the compiler tells that it rewrites everything (MSVC and
  // clang-cl `/fp:fast`: `_M_FP_FAST`), where its arithmetic has excess
  // precision (clang 15 has `FLT_EVAL_METHOD` of -1 under
  // `-funsafe-math-optimizations`) and for a clang before 14, which has no
  // pragma to protect the cores. Every other compiler (GCC of every version,
  // whether it defines `__ASSOCIATIVE_MATH__` or not; clang 14, 16 and later)
  // keeps the fused or the two-product route, and the cores protect
  // themselves.
#if defined(_M_FP_FAST) || LUMEX_UNITS_EXCESS_PRECISION                       \
    || (defined(__clang__) && __clang_major__ < 14)
  EXPECT_EQ (LUMEX_UNITS_ROUTE_EXACT, LUMEX_UNITS_ROUTE);
#else
  EXPECT_NE (LUMEX_UNITS_ROUTE_EXACT, LUMEX_UNITS_ROUTE);
#endif
#endif
  std::uint64_t state = 606;
  auto next = [&state] ()
    {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      std::uint64_t mixed = state;
      mixed ^= mixed >> 29;
      mixed *= 0xbf58476d1ce4e5b9ull;
      mixed ^= mixed >> 32;
      return mixed;
    };
  auto fraction = [&next] ()
    { return 1.0 + static_cast<double> (next () >> 12) / 4503599627370496.0; };
  std::size_t different = 0;
  std::size_t checked = 0;
  for (int index = 0; index < 40000; ++index)
    {
      double value
          = std::ldexp (fraction (), static_cast<int> (next () % 600) - 300);
      double high
          = std::ldexp (fraction (), static_cast<int> (next () % 600) - 300);
      double low
          = std::ldexp (high, -53)
            * (static_cast<double> (next () >> 11) / 9007199254740992.0 - 0.5);
      if ((next () & 1) != 0)
        value = -value;
      if ((next () & 1) != 0)
        {
          high = -high;
          low = -low;
        }
      double const expected = exact_route (value, high, low);
      if (!(std::fabs (expected) >= (std::numeric_limits<double>::min) ()))
        continue;
      ++checked;
      if (bits_of (apply_route (value, high, low)) != bits_of (expected))
        ++different;
    }
  EXPECT_GT (checked, 39000u);
  EXPECT_EQ (0u, different);
}

// A start-up code that sets DAZ and FTZ in MXCSR (what `-ffast-math` and
// `-funsafe-math-optimizations` link in) flushes a subnormal operand and a
// subnormal result of every operation. The lower half of the split of a value
// below 2^-968 is subnormal, so the routes scale such a value up before the
// core runs. Operands and results are normal here: only an intermediate is
// subnormal. The expected values are formed before the bits are set, by the
// exact route, which is integers.
#if LUMEX_TEST_UNITS_HAS_MXCSR
namespace
{
// DAZ is bit 6 and FTZ bit 15 of MXCSR; the previous word comes back on
// the end of the scope
class flush_denormals_t
{
public:
  flush_denormals_t () : saved_ (_mm_getcsr ())
  {
    _mm_setcsr (saved_ | 0x8040u);
  }
  flush_denormals_t (flush_denormals_t const &) = delete;
  flush_denormals_t &operator= (flush_denormals_t const &) = delete;
  ~flush_denormals_t () { _mm_setcsr (saved_); }

private:
  unsigned int saved_;
};

struct tiny_case_t
{
  double value;
  double high;
  double low;
  double expected;
};
} // namespace

TEST (
    LumexUnitsDoubleDoubleTest,
    GivenTinyOperandsWithDenormalsFlushed_WhenApplied_ThenEveryRouteIsCorrectlyRounded)
{
  std::uint64_t state = 808;
  auto next = [&state] ()
    {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      std::uint64_t mixed = state;
      mixed ^= mixed >> 29;
      mixed *= 0xbf58476d1ce4e5b9ull;
      mixed ^= mixed >> 32;
      return mixed;
    };
  auto fraction = [&next] ()
    { return 1.0 + static_cast<double> (next () >> 12) / 4503599627370496.0; };
  std::vector<tiny_case_t> cases;
  for (int index = 0; index < 40000; ++index)
    {
      // |value| in [2^-1021, 2^-969), a factor of [1, 2^500)
      double value
          = std::ldexp (fraction (), -1021 + static_cast<int> (next () % 52));
      double high = std::ldexp (fraction (), static_cast<int> (next () % 500));
      double low
          = std::ldexp (high, -53)
            * (static_cast<double> (next () >> 11) / 9007199254740992.0 - 0.5);
      if ((next () & 1) != 0)
        value = -value;
      double const expected = exact_route (value, high, low);
      if (!(std::fabs (expected) >= LUMEX_TEST_UNITS_SMALLEST_COMPARED))
        continue;
      tiny_case_t const made = { value, high, low, expected };
      cases.push_back (made);
    }
  std::size_t different_apply = 0;
  std::size_t different_fused = 0;
  std::size_t different_two_product = 0;
  {
    flush_denormals_t const flushed;
    for (tiny_case_t const &item : cases)
      {
        if (bits_of (apply_route (item.value, item.high, item.low))
            != bits_of (item.expected))
          ++different_apply;
        if (bits_of (fused_route (item.value, item.high, item.low))
            != bits_of (item.expected))
          ++different_fused;
        if (bits_of (two_product_route (item.value, item.high, item.low))
            != bits_of (item.expected))
          ++different_two_product;
      }
  }
  EXPECT_GT (cases.size (), 30000u);
  EXPECT_EQ (0u, different_apply);
  EXPECT_EQ (0u, different_fused);
  EXPECT_EQ (0u, different_two_product);
}
#endif

// lumex/tests/core/units/api/LumexUnitsConvert.cxx11.tests.cpp
//
// The exact ratio of two units and the conversion of a double: the
// conversions of the oracle (the correctly rounded double of the exact
// result), spot values with their clause, the refusals (kinds, logarithmic
// units, affine units), the temperature points and differences, and the
// checked arithmetic.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/LumexUnitsTestSupport.hpp"

using namespace lumex_test_units;

namespace
{
prefixed_unit_t
pair_of (unit_id unit, prefix_id prefix = prefix_id::none)
{
  prefixed_unit_t pair = { unit, prefix };
  return pair;
}

double
converted (double value, prefixed_unit_t from, prefixed_unit_t to)
{
  result_t<double, conversion_error> const result = convert (value, from, to);
  EXPECT_TRUE (result.has_value ());
  return result.has_value () ? result.value ()
                             : std::numeric_limits<double>::quiet_NaN ();
}

conversion_error
refused (double value, prefixed_unit_t from, prefixed_unit_t to)
{
  result_t<double, conversion_error> const result = convert (value, from, to);
  EXPECT_FALSE (result.has_value ());
  return result.has_value () ? conversion_error::overflow : result.error ();
}

scale_t
ratio_of (prefixed_unit_t from, prefixed_unit_t to)
{
  result_t<scale_t, conversion_error> const result = ratio (from, to);
  EXPECT_TRUE (result.has_value ());
  return result.has_value () ? result.value () : scale_t ();
}
} // namespace

// --- the oracle's conversions ----------------------------------------------

TEST (
    LumexUnitsConvertTest,
    GivenOracleConversions_WhenConverted_ThenTheyAreWithinOneUlpOfTheCorrectlyRoundedResult)
{
  std::uint64_t worst_rational = 0;
  std::uint64_t worst_pi = 0;
  std::size_t exact_rational = 0;
  std::size_t rational = 0;
  for (std::size_t index = 0; index < oracle_conversion_count; ++index)
    {
      oracle_conversion_t const &row = oracle_conversions[index];
      prefixed_unit_t const from = pair_of (
          unit_from_name (row.from), prefix_from_name (row.from_prefix));
      prefixed_unit_t const to = pair_of (unit_from_name (row.to),
                                          prefix_from_name (row.to_prefix));
      result_t<double, conversion_error> const result
          = convert (row.input, from, to);
      ASSERT_TRUE (result.has_value ())
          << row.from << " -> " << row.to << " " << row.input;
      std::uint64_t const distance
          = ulp_distance (result.value (), row.expected);
      EXPECT_LE (distance, 1u)
          << row.from << " (" << row.from_prefix << ") -> " << row.to << " ("
          << row.to_prefix << ") of " << row.input << ": got "
          << result.value () << ", expected " << row.expected;
      // An input of 1 gives the factor itself: correctly rounded, no error.
      if (row.input == 1.0)
        EXPECT_EQ (0u, distance) << row.from << " -> " << row.to << " of 1";
      if (row.has_pi)
        {
          if (distance > worst_pi)
            worst_pi = distance;
        }
      else
        {
          ++rational;
          if (distance == 0)
            ++exact_rational;
          if (distance > worst_rational)
            worst_rational = distance;
        }
    }
  std::cout << "[ units ] conversions: " << oracle_conversion_count
            << ", rational " << rational << " (bit-exact " << exact_rational
            << ", worst " << worst_rational
            << " ulp), with pi: " << oracle_conversion_count - rational
            << " (worst " << worst_pi << " ulp)\n";
}

// --- spot values, each with its clause --------------------------------------

TEST (LumexUnitsConvertTest,
      GivenSpotValues_WhenConverted_ThenTheyAreTheDefinedOnes)
{
  // 1 eV = 1.602 176 634e-19 J, exact since the 26th CGPM (2018): SI
  // Brochure V4.01, Table 8 note (l); GOST 8.417-2024, table 5.
  EXPECT_LE (ulp_distance (1.602176634e-19,
                           converted (1.0, pair_of (unit_id::electronvolt),
                                      pair_of (unit_id::joule))),
             1u);
  // 1 atm = 101 325 Pa: 10th CGPM (1954), Resolution 4.
  EXPECT_EQ (101325.0, converted (1.0, pair_of (unit_id::standard_atmosphere),
                                  pair_of (unit_id::pascal_unit)));
  // 1 cal = 4.1868 J: GOST 8.417-2024, annex G (international calorie).
  EXPECT_LE (ulp_distance (4.1868, converted (1.0, pair_of (unit_id::calorie),
                                              pair_of (unit_id::joule))),
             1u);
  // 1 Ci = 3.7e10 Bq: GOST 8.417-2024, annex G.
  EXPECT_EQ (3.7e10, converted (1.0, pair_of (unit_id::curie),
                                pair_of (unit_id::becquerel)));
  // 1 R = 2.58e-4 C/kg: GOST 8.417-2024, annex G. The roentgen is an
  // exposure and the coulomb per kilogram is not a catalog unit, so the value
  // is that of the scale.
  scale_t const roentgen = descriptor (unit_id::roentgen).scale;
  EXPECT_EQ (129, roentgen.numerator);
  EXPECT_EQ (5, roentgen.denominator);
  EXPECT_EQ (-5, roentgen.exponent10);
  // 1 Torr = 101325 / 760 Pa: 1/760 of the standard atmosphere.
  EXPECT_EQ (101325.0 / 760.0, converted (1.0, pair_of (unit_id::torr),
                                          pair_of (unit_id::pascal_unit)));
  // 1 mmHg = 133.322387415 Pa (13595.1 kg/m^3 times 9.80665 m/s^2 times 1 mm).
  EXPECT_EQ (133.322387415,
             converted (1.0, pair_of (unit_id::millimetre_of_mercury),
                        pair_of (unit_id::pascal_unit)));
  // The torr and the mmHg differ by 1.4e-7 (relative).
  double const torr = converted (1.0, pair_of (unit_id::torr),
                                 pair_of (unit_id::pascal_unit));
  double const mmhg = converted (1.0, pair_of (unit_id::millimetre_of_mercury),
                                 pair_of (unit_id::pascal_unit));
  EXPECT_GT (mmhg, torr);
  EXPECT_NEAR (1.42e-7, (mmhg - torr) / torr, 1e-9);
  // 1 kgf = 9.80665 N, 1 kgf/cm^2 = 98066.5 Pa, 1 mm H2O = 9.80665 Pa.
  EXPECT_EQ (9.80665, converted (1.0, pair_of (unit_id::kilogram_force),
                                 pair_of (unit_id::newton)));
  EXPECT_EQ (
      98066.5,
      converted (1.0, pair_of (unit_id::kilogram_force_per_square_centimetre),
                 pair_of (unit_id::pascal_unit)));
  EXPECT_EQ (9.80665, converted (1.0, pair_of (unit_id::millimetre_of_water),
                                 pair_of (unit_id::pascal_unit)));
  // 1 h = 3600 s, 1 d = 86400 s, 1 kW.h = 3.6e6 J, 1 A.h = 3600 C.
  EXPECT_EQ (3600.0, converted (1.0, pair_of (unit_id::hour),
                                pair_of (unit_id::second)));
  EXPECT_EQ (86400.0, converted (1.0, pair_of (unit_id::day),
                                 pair_of (unit_id::second)));
  EXPECT_EQ (3.6e6, converted (1.0, pair_of (unit_id::kilowatt_hour),
                               pair_of (unit_id::joule)));
  EXPECT_EQ (3600.0, converted (1.0, pair_of (unit_id::ampere_hour),
                                pair_of (unit_id::coulomb)));
  // 1 ly = 9 460 730 472 580 800 m, 1 au = 149 597 870 700 m,
  // 1 n mile = 1852 m, 1 ft = 0.3048 m, 1 L = 1e-3 m^3, 1 bar = 1e5 Pa.
  EXPECT_EQ (9460730472580800.0, converted (1.0, pair_of (unit_id::light_year),
                                            pair_of (unit_id::metre)));
  EXPECT_EQ (149597870700.0,
             converted (1.0, pair_of (unit_id::astronomical_unit),
                        pair_of (unit_id::metre)));
  EXPECT_EQ (1852.0, converted (1.0, pair_of (unit_id::nautical_mile),
                                pair_of (unit_id::metre)));
  EXPECT_EQ (0.3048, converted (1.0, pair_of (unit_id::foot),
                                pair_of (unit_id::metre)));
  EXPECT_EQ (1e5, converted (1.0, pair_of (unit_id::bar),
                             pair_of (unit_id::pascal_unit)));
  // 1 t = 1000 kg: the gram is 1e-3 kg, the kilogram is (kilo, gram).
  EXPECT_EQ (1000.0, converted (1.0, pair_of (unit_id::tonne),
                                pair_of (unit_id::gram, prefix_id::kilo)));
  EXPECT_EQ (
      1e6, converted (1.0, pair_of (unit_id::tonne), pair_of (unit_id::gram)));
}

TEST (
    LumexUnitsConvertTest,
    GivenSevenHundredInAMilliUnit_WhenConverted_ThenTheBaseUnitGetsExactlyPointSeven)
{
  // The factor of the milli prefix is a double-double (fl (1e-3) and its
  // exact residual) and the product is rounded once: 700 of a milli unit is
  // the double 0.7, which 700 * fl (1e-3) (the double after 0.7) is not.
  for (unit_id const unit : { unit_id::mole, unit_id::gram, unit_id::metre })
    {
      double const value = converted (700.0, pair_of (unit, prefix_id::milli),
                                      pair_of (unit));
      EXPECT_EQ (0.7, value) << descriptor (unit).name;
    }
  EXPECT_EQ (0.0007,
             converted (700.0, pair_of (unit_id::gram, prefix_id::micro),
                        pair_of (unit_id::gram)));
  EXPECT_EQ (700.0, converted (0.7, pair_of (unit_id::gram),
                               pair_of (unit_id::gram, prefix_id::milli)));
}

TEST (LumexUnitsConvertTest,
      GivenPiFactors_WhenConverted_ThenTheyAreWithinOneUlpOfTheExactValue)
{
  double const pi = 3.14159265358979323846;
  EXPECT_LE (ulp_distance (pi, converted (180.0, pair_of (unit_id::degree),
                                          pair_of (unit_id::radian))),
             1u);
  EXPECT_LE (
      ulp_distance (2.0 * pi, converted (1.0, pair_of (unit_id::revolution),
                                         pair_of (unit_id::radian))),
      1u);
  EXPECT_LE (ulp_distance (pi / 200.0, converted (1.0, pair_of (unit_id::gon),
                                                  pair_of (unit_id::radian))),
             1u);
  EXPECT_NEAR (360.0,
               converted (1.0, pair_of (unit_id::revolution),
                          pair_of (unit_id::degree)),
               1e-12);
  EXPECT_NEAR (
      90.0,
      converted (100.0, pair_of (unit_id::gon), pair_of (unit_id::degree)),
      1e-12);
  EXPECT_NEAR (
      3600.0,
      converted (1.0, pair_of (unit_id::degree), pair_of (unit_id::arcsecond)),
      1e-9);
  EXPECT_NEAR (
      1.0 / 3600.0,
      converted (1.0, pair_of (unit_id::arcsecond), pair_of (unit_id::degree)),
      1e-18);
  // The oersted (1000 / (4 pi) A/m) and the gilbert (10 / (4 pi) A): their
  // scales have the power -1 of pi.
  EXPECT_LE (
      ulp_distance (1000.0 / (4.0 * pi),
                    apply_scale (descriptor (unit_id::oersted).scale, 1.0)),
      1u);
  EXPECT_LE (
      ulp_distance (10.0 / (4.0 * pi),
                    apply_scale (descriptor (unit_id::gilbert).scale, 1.0)),
      1u);
}

// --- the exact ratio --------------------------------------------------------

TEST (LumexUnitsConvertTest,
      GivenRatios_WhenComputed_ThenTheyAreExactAndCanonical)
{
  scale_t scale
      = ratio_of (pair_of (unit_id::hour), pair_of (unit_id::minute));
  EXPECT_EQ (1, scale.exponent10);
  EXPECT_EQ (6, scale.numerator);
  EXPECT_EQ (1, scale.denominator);
  scale = ratio_of (pair_of (unit_id::minute), pair_of (unit_id::hour));
  EXPECT_EQ (-1, scale.exponent10);
  EXPECT_EQ (1, scale.numerator);
  EXPECT_EQ (6, scale.denominator);
  scale = ratio_of (pair_of (unit_id::metre, prefix_id::kilo),
                    pair_of (unit_id::metre));
  EXPECT_EQ (3, scale.exponent10);
  EXPECT_EQ (1, scale.numerator);
  EXPECT_EQ (1, scale.denominator);
  scale = ratio_of (pair_of (unit_id::torr),
                    pair_of (unit_id::millimetre_of_mercury));
  // 20265 / 152 over 26664477483 / 2 * 10^8
  EXPECT_NE (0, scale.numerator);
  scale = ratio_of (pair_of (unit_id::degree), pair_of (unit_id::radian));
  EXPECT_EQ (1, scale.pi_power);
  EXPECT_EQ (-1, scale.exponent10);
  EXPECT_EQ (1, scale.numerator);
  EXPECT_EQ (18, scale.denominator);
  // A unit against itself is one.
  scale = ratio_of (pair_of (unit_id::electronvolt),
                    pair_of (unit_id::electronvolt));
  EXPECT_EQ (0, scale.exponent10);
  EXPECT_EQ (1, scale.numerator);
  EXPECT_EQ (1, scale.denominator);
  EXPECT_EQ (0, scale.pi_power);
}

TEST (
    LumexUnitsConvertTest,
    GivenEveryPairOfUnitsOfOneKind_WhenRatioIsComputed_ThenItIsCanonicalAndInvertible)
{
  std::size_t pairs = 0;
  for (std::size_t first = 0; first < unit_count; ++first)
    {
      for (std::size_t second = 0; second < unit_count; ++second)
        {
          prefixed_unit_t const from = pair_of (unit_id_at (first));
          prefixed_unit_t const to = pair_of (unit_id_at (second));
          result_t<scale_t, conversion_error> const forward = ratio (from, to);
          if (!forward.has_value ())
            continue;
          ++pairs;
          SCOPED_TRACE (std::string (descriptor (from.unit).name) + " -> "
                        + descriptor (to.unit).name);
          scale_t const s = forward.value ();
          EXPECT_TRUE (is_canonical_scale (s, factor_kind::exact));
          result_t<scale_t, conversion_error> const back = ratio (to, from);
          ASSERT_TRUE (back.has_value ());
          // the product of a ratio and its inverse is one
          EXPECT_EQ (s.pi_power, -back.value ().pi_power);
          EXPECT_EQ (s.exponent10, -back.value ().exponent10);
          EXPECT_EQ (s.numerator, back.value ().denominator);
          EXPECT_EQ (s.denominator, back.value ().numerator);
        }
    }
  EXPECT_GT (pairs, 400u);
}

// --- refusals ---------------------------------------------------------------

TEST (LumexUnitsConvertTest,
      GivenUnitsOfOneDimensionAndTwoKinds_WhenConverted_ThenTheyAreRefused)
{
  // SI Brochure Table 4 note (d): the hertz only for periodic phenomena, the
  // becquerel only for stochastic processes.
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::hertz), pair_of (unit_id::becquerel)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::becquerel), pair_of (unit_id::hertz)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::gray), pair_of (unit_id::sievert)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::sievert), pair_of (unit_id::gray)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::curie), pair_of (unit_id::hertz)));
  EXPECT_EQ (conversion_error::different_quantity_kind,
             refused (1.0, pair_of (unit_id::revolution_per_second),
                      pair_of (unit_id::hertz)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::rad_dose), pair_of (unit_id::sievert)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::radian), pair_of (unit_id::steradian)));
  EXPECT_EQ (conversion_error::different_quantity_kind,
             refused (1.0, pair_of (unit_id::radian), pair_of (unit_id::one)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::metre), pair_of (unit_id::second)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::joule), pair_of (unit_id::newton)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::ampere), pair_of (unit_id::gilbert)));
  EXPECT_EQ (
      conversion_error::different_quantity_kind,
      refused (1.0, pair_of (unit_id::pascal_unit), pair_of (unit_id::joule)));
}

TEST (LumexUnitsConvertTest,
      GivenLogarithmicUnits_WhenConverted_ThenTheyAreRefused)
{
  unit_id const logs[] = { unit_id::bel,  unit_id::decibel, unit_id::neper,
                           unit_id::phon, unit_id::octave,  unit_id::decade };
  for (unit_id const log : logs)
    {
      SCOPED_TRACE (descriptor (log).name);
      EXPECT_EQ (conversion_error::no_si_value,
                 refused (1.0, pair_of (log), pair_of (log)));
      EXPECT_EQ (conversion_error::no_si_value,
                 refused (1.0, pair_of (log), pair_of (unit_id::one)));
      EXPECT_EQ (conversion_error::no_si_value,
                 refused (1.0, pair_of (unit_id::one), pair_of (log)));
      EXPECT_FALSE (ratio (pair_of (log), pair_of (log)).has_value ());
    }
  EXPECT_EQ (
      conversion_error::no_si_value,
      refused (1.0, pair_of (unit_id::bel), pair_of (unit_id::decibel)));
}

TEST (
    LumexUnitsConvertTest,
    GivenTheDegreeCelsius_WhenConvertedAsAValue_ThenItIsRefusedWithItsOwnError)
{
  EXPECT_EQ (conversion_error::offset_unit,
             refused (1.0, pair_of (unit_id::degree_celsius),
                      pair_of (unit_id::kelvin)));
  EXPECT_EQ (conversion_error::offset_unit,
             refused (1.0, pair_of (unit_id::kelvin),
                      pair_of (unit_id::degree_celsius)));
  EXPECT_EQ (conversion_error::offset_unit,
             refused (1.0, pair_of (unit_id::degree_celsius),
                      pair_of (unit_id::degree_celsius)));
  // The kelvin converts among its own prefixes.
  EXPECT_EQ (0.5,
             converted (500.0, pair_of (unit_id::kelvin, prefix_id::milli),
                        pair_of (unit_id::kelvin)));
  EXPECT_TRUE (is_affine (unit_id::degree_celsius));
  EXPECT_FALSE (is_affine (unit_id::kelvin));
}

// --- temperatures -----------------------------------------------------------

TEST (LumexUnitsConvertTest,
      GivenTemperaturePoints_WhenConverted_ThenTheOffsetIsApplied)
{
  prefixed_unit_t const celsius = pair_of (unit_id::degree_celsius);
  prefixed_unit_t const kelvin = pair_of (unit_id::kelvin);
  EXPECT_EQ (273.15, convert_point (0.0, celsius, kelvin).value ());
  EXPECT_EQ (298.15, convert_point (25.0, celsius, kelvin).value ());
  EXPECT_EQ (373.15, convert_point (100.0, celsius, kelvin).value ());
  EXPECT_EQ (0.0, convert_point (273.15, kelvin, celsius).value ())
      << "ice point";
  EXPECT_EQ (25.0, convert_point (298.15, kelvin, celsius).value ());
  EXPECT_EQ (-273.15, convert_point (0.0, kelvin, celsius).value ());
  EXPECT_EQ (10.0, convert_point (10.0, celsius, celsius).value ());
  EXPECT_EQ (10.0, convert_point (10.0, kelvin, kelvin).value ());
  // a prefixed kelvin: 25000 mK is 25 K, which is -248.15 degrees Celsius
  EXPECT_EQ (25.0, convert_point (25000.0,
                                  pair_of (unit_id::kelvin, prefix_id::milli),
                                  kelvin)
                       .value ());
  EXPECT_NEAR (-248.15,
               convert_point (25000.0,
                              pair_of (unit_id::kelvin, prefix_id::milli),
                              celsius)
                   .value (),
               1e-12);
  EXPECT_NEAR (298150.0,
               convert_point (25.0, celsius,
                              pair_of (unit_id::kelvin, prefix_id::milli))
                   .value (),
               1e-8);
}

TEST (LumexUnitsConvertTest,
      GivenTemperatureDifferences_WhenConverted_ThenTheNumberIsTheSame)
{
  prefixed_unit_t const celsius = pair_of (unit_id::degree_celsius);
  prefixed_unit_t const kelvin = pair_of (unit_id::kelvin);
  EXPECT_EQ (10.0, convert_difference (10.0, celsius, kelvin).value ());
  EXPECT_EQ (10.0, convert_difference (10.0, kelvin, celsius).value ());
  EXPECT_EQ (0.01,
             convert_difference (
                 10.0, pair_of (unit_id::kelvin, prefix_id::milli), kelvin)
                 .value ());
  // Other kinds convert as a difference too, and a mismatch is refused.
  EXPECT_EQ (60.0, convert_difference (1.0, pair_of (unit_id::minute),
                                       pair_of (unit_id::second))
                       .value ());
  EXPECT_FALSE (convert_difference (1.0, celsius, pair_of (unit_id::metre))
                    .has_value ());
}

TEST (LumexUnitsConvertTest,
      GivenNonTemperatureUnits_WhenConvertedAsAPoint_ThenTheyAreRefused)
{
  result_t<double, conversion_error> const result = convert_point (
      1.0, pair_of (unit_id::metre), pair_of (unit_id::metre));
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (conversion_error::not_a_temperature, result.error ());
  EXPECT_EQ (conversion_error::not_a_temperature,
             convert_point (1.0, pair_of (unit_id::degree_celsius),
                            pair_of (unit_id::metre))
                 .error ());
  EXPECT_TRUE (is_temperature_kind (quantity_kind::celsius_temperature));
  EXPECT_TRUE (is_temperature_kind (quantity_kind::thermodynamic_temperature));
  EXPECT_FALSE (is_temperature_kind (quantity_kind::length));
}

// --- every pair of the catalog ----------------------------------------------

TEST (
    LumexUnitsConvertTest,
    GivenEveryPairOfUnits_WhenConverted_ThenOnlyUnitsOfOneKindWithAValueAreAccepted)
{
  std::size_t accepted = 0;
  std::size_t refused_kind = 0;
  for (std::size_t first = 0; first < unit_count; ++first)
    {
      for (std::size_t second = 0; second < unit_count; ++second)
        {
          unit_descriptor_t const &from = descriptor (unit_id_at (first));
          unit_descriptor_t const &to = descriptor (unit_id_at (second));
          result_t<double, conversion_error> const result
              = convert (1.0, pair_of (from.id), pair_of (to.id));
          SCOPED_TRACE (std::string (from.name) + " -> " + to.name);
          if (from.factor == factor_kind::none
              || to.factor == factor_kind::none)
            {
              ASSERT_FALSE (result.has_value ());
              EXPECT_EQ (conversion_error::no_si_value, result.error ());
            }
          else if (from.kind != to.kind
                   && !(is_temperature_kind (from.kind)
                        && is_temperature_kind (to.kind)))
            {
              ASSERT_FALSE (result.has_value ());
              EXPECT_EQ (conversion_error::different_quantity_kind,
                         result.error ());
              ++refused_kind;
            }
          else if (is_affine (from.id) || is_affine (to.id))
            {
              ASSERT_FALSE (result.has_value ());
              EXPECT_EQ (conversion_error::offset_unit, result.error ());
            }
          else
            {
              ASSERT_TRUE (result.has_value ());
              EXPECT_GT (result.value (), 0.0);
              ++accepted;
            }
        }
    }
  EXPECT_GT (accepted, 400u);
  EXPECT_GT (refused_kind, 9000u);
}

// --- the arithmetic ---------------------------------------------------------

TEST (LumexUnitsConvertTest,
      GivenScales_WhenApplied_ThenTheValueTimesTheCorrectlyRoundedFactor)
{
  // The factor is the correctly rounded value of 10^k * p / q * pi^n.
  EXPECT_EQ (3.0 / 7.0, scale_factor (scale_t{ 0, 3, 7, 0 }));
  EXPECT_EQ (3000.0 / 7.0, scale_factor (scale_t{ 3, 3, 7, 0 }));
  EXPECT_EQ (3.0 / 7000.0, scale_factor (scale_t{ -3, 3, 7, 0 }));
  EXPECT_EQ (10.0 * (3.0 / 7.0), apply_scale (scale_t{ 0, 3, 7, 0 }, 10.0));
  EXPECT_EQ (3.14159265358979323846, scale_factor (scale_t{ 0, 1, 1, 1 }));
  EXPECT_EQ (2.0 * 3.14159265358979323846,
             apply_scale (scale_t{ 0, 1, 1, 1 }, 2.0));
  // 1 / pi^2 and pi^2, from the exact value
  EXPECT_EQ (0.10132118364233778, scale_factor (scale_t{ 0, 1, 1, -2 }));
  EXPECT_EQ (9.869604401089358, scale_factor (scale_t{ 0, 1, 1, 2 }));
  // exact decimal powers over the whole range, with no intermediate overflow
  EXPECT_EQ (1e100, scale_factor (scale_t{ 100, 1, 1, 0 }));
  EXPECT_EQ (1e-100, scale_factor (scale_t{ -100, 1, 1, 0 }));
  EXPECT_EQ (1e250, scale_factor (scale_t{ 250, 1, 1, 0 }));
  EXPECT_EQ (1e-250, scale_factor (scale_t{ -250, 1, 1, 0 }));
  EXPECT_EQ (1e22, scale_factor (scale_t{ 22, 1, 1, 0 }));
  EXPECT_EQ (1e23, scale_factor (scale_t{ 23, 1, 1, 0 }));
  EXPECT_EQ (1e281, scale_factor (scale_t{ 281, 1, 1, 0 }));
  EXPECT_EQ (1e-281, scale_factor (scale_t{ -281, 1, 1, 0 }));
  EXPECT_EQ (std::numeric_limits<double>::infinity (),
             scale_factor (scale_t{ 309, 1, 1, 0 }));
  EXPECT_EQ (0.0, scale_factor (scale_t{ -400, 1, 1, 0 }));
  EXPECT_EQ (0.0, scale_factor (scale_t{ 0, 0, 1, 0 }));
  for (int exponent = 0; exponent <= 22; ++exponent)
    {
      double expected = 1.0;
      for (int step = 0; step < exponent; ++step)
        expected *= 10.0;
      EXPECT_EQ (expected, scale_factor (scale_t{ exponent, 1, 1, 0 }))
          << exponent;
    }
}

TEST (LumexUnitsConvertTest,
      GivenLargeFactors_WhenMultiplied_ThenOverflowIsReportedNotWrapped)
{
  std::int64_t product = 0;
  EXPECT_TRUE (detail::checked_multiply (3, 4, product));
  EXPECT_EQ (12, product);
  std::int64_t const big = std::numeric_limits<std::int64_t>::max ();
  EXPECT_TRUE (detail::checked_multiply (big, 1, product));
  EXPECT_EQ (big, product);
  EXPECT_FALSE (detail::checked_multiply (big, 2, product));
  EXPECT_FALSE (detail::checked_multiply (big / 2 + 1, 2, product));
  EXPECT_TRUE (detail::checked_multiply (big / 2, 2, product));
  EXPECT_FALSE (
      detail::checked_multiply (3037000500LL, 3037000500LL, product));
  EXPECT_TRUE (detail::checked_multiply (3037000499LL, 3037000499LL, product));
}

TEST (LumexUnitsConvertTest,
      GivenEveryUnit_WhenConvertedThereAndBack_ThenTheValueReturnsWithinTwoUlp)
{
  for (std::size_t first = 0; first < unit_count; ++first)
    {
      for (std::size_t second = 0; second < unit_count; ++second)
        {
          prefixed_unit_t const from = pair_of (unit_id_at (first));
          prefixed_unit_t const to = pair_of (unit_id_at (second));
          result_t<double, conversion_error> const there
              = convert (700.5, from, to);
          if (!there.has_value ())
            continue;
          result_t<double, conversion_error> const back
              = convert (there.value (), to, from);
          ASSERT_TRUE (back.has_value ());
          EXPECT_LE (ulp_distance (700.5, back.value ()), 4u)
              << descriptor (from.unit).name << " -> "
              << descriptor (to.unit).name;
        }
    }
}

// --- the oracle's ratios and values -----------------------------------------

TEST (
    LumexUnitsConvertTest,
    GivenEveryPairOfOneKind_WhenRatioIsComputed_ThenItEqualsTheOraclesCanonicalForm)
{
  for (std::size_t index = 0; index < oracle_ratio_count; ++index)
    {
      oracle_ratio_t const &row = oracle_ratios[index];
      prefixed_unit_t const from = pair_of (unit_from_name (row.from));
      prefixed_unit_t const to = pair_of (unit_from_name (row.to));
      result_t<scale_t, conversion_error> const result = ratio (from, to);
      SCOPED_TRACE (std::string (row.from) + " -> " + row.to);
      ASSERT_TRUE (result.has_value ());
      EXPECT_EQ (row.k, result.value ().exponent10);
      EXPECT_EQ (row.p, result.value ().numerator);
      EXPECT_EQ (row.q, result.value ().denominator);
      EXPECT_EQ (row.pi, result.value ().pi_power);
      EXPECT_TRUE (is_canonical_scale (result.value (), factor_kind::exact));
    }
  EXPECT_GT (oracle_ratio_count, 300u);
}

TEST (LumexUnitsConvertTest,
      GivenKgfAndMicronewton_WhenRatioIsComputed_ThenTheFormIsTheOneOfTheValue)
{
  // 1 kgf = 9.80665e6 uN = 980665 * 10^1 (not 196133 * 10^2 / 2)
  scale_t const scale = ratio_of (pair_of (unit_id::kilogram_force),
                                  pair_of (unit_id::newton, prefix_id::micro));
  EXPECT_EQ (1, scale.exponent10);
  EXPECT_EQ (980665, scale.numerator);
  EXPECT_EQ (1, scale.denominator);
}

TEST (
    LumexUnitsConvertTest,
    GivenEveryAllowedUnitAndPrefix_WhenTheFactorIsComputed_ThenItIsTheCorrectlyRoundedValue)
{
  std::size_t rounded_with_pi = 0;
  for (std::size_t index = 0; index < oracle_value_count; ++index)
    {
      oracle_value_t const &row = oracle_values[index];
      prefixed_unit_t const pair
          = pair_of (unit_from_name (row.unit), prefix_from_name (row.prefix));
      SCOPED_TRACE (std::string (row.unit) + " " + row.prefix);
      scale_t const scale = scale_of (pair);
      EXPECT_EQ (row.value, scale_factor (scale));
      EXPECT_EQ (row.value, 1.0 * scale_factor (scale));
      if (scale.pi_power != 0)
        ++rounded_with_pi;
    }
  EXPECT_GT (oracle_value_count, 1000u);
  EXPECT_GT (rounded_with_pi, 20u);
}

TEST (
    LumexUnitsConvertTest,
    GivenMeasuredAndDefinedValues_WhenConverted_ThenTheyAreTheSpotValuesOfTheSources)
{
  // Not from the oracle: the dalton of CODATA 2022 (SI Brochure V4.01 Table 8
  // note (i)), 1.660 539 068 92e-27 kg; the knot, 1852 / 3600 m/s; the metric
  // horsepower, 75 kgf m/s = 735.49875 W; the parsec, 648000 / pi au.
  EXPECT_EQ (1.66053906892e-27,
             converted (1.0, pair_of (unit_id::dalton),
                        pair_of (unit_id::gram, prefix_id::kilo)));
  EXPECT_EQ (1.66053906892e-24, converted (1.0, pair_of (unit_id::dalton),
                                           pair_of (unit_id::gram)));
  EXPECT_EQ (1852.0 / 3600.0,
             scale_factor (scale_of (pair_of (unit_id::knot))));
  EXPECT_EQ (735.49875, converted (1.0, pair_of (unit_id::metric_horsepower),
                                   pair_of (unit_id::watt)));
  EXPECT_EQ (descriptor (unit_id::metric_horsepower).factor,
             factor_kind::exact);
  EXPECT_EQ (descriptor (unit_id::parsec).factor, factor_kind::exact);
  EXPECT_LE (ulp_distance (3.085677581491367e16,
                           converted (1.0, pair_of (unit_id::parsec),
                                      pair_of (unit_id::metre))),
             1u);
  EXPECT_LE (ulp_distance (206264.80624709636,
                           converted (1.0, pair_of (unit_id::parsec),
                                      pair_of (unit_id::astronomical_unit))),
             1u);
  EXPECT_EQ (descriptor (unit_id::dalton).factor, factor_kind::measured);
}

TEST (LumexUnitsConvertTest,
      GivenUnitsOrPrefixesOutsideTheCatalog_WhenUsed_ThenTheyAreRefusedNotRead)
{
  prefixed_unit_t const metre = pair_of (unit_id::metre);
  prefixed_unit_t const bad_unit = pair_of (static_cast<unit_id> (unit_count));
  prefixed_unit_t const bad_prefix
      = pair_of (unit_id::metre, static_cast<prefix_id> (30));
  EXPECT_EQ (conversion_error::invalid_argument,
             refused (1.0, bad_unit, metre));
  EXPECT_EQ (conversion_error::invalid_argument,
             refused (1.0, metre, bad_unit));
  EXPECT_EQ (conversion_error::invalid_argument,
             refused (1.0, bad_prefix, metre));
  EXPECT_EQ (conversion_error::invalid_argument,
             refused (1.0, metre, bad_prefix));
  EXPECT_FALSE (ratio (bad_prefix, metre).has_value ());
  EXPECT_FALSE (convert_difference (1.0, bad_prefix, metre).has_value ());
  EXPECT_FALSE (convert_point (1.0, bad_unit, bad_unit).has_value ());
  EXPECT_EQ (0, scale_of (bad_unit).numerator);
  EXPECT_EQ (0, scale_of (bad_prefix).numerator);
  EXPECT_TRUE (canonical_symbol (static_cast<unit_id> (unit_count),
                                 script::international)
                   .empty ());
  EXPECT_TRUE (canonical_symbol (static_cast<prefix_id> (30), script::russian)
                   .empty ());
  EXPECT_TRUE (canonical_symbol (prefix_id::none, script::russian).empty ());
  EXPECT_FALSE (
      takes_prefix (static_cast<unit_id> (unit_count), prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::metre, static_cast<prefix_id> (30)));
  EXPECT_FALSE (takes_prefix (unit_id::metre, prefix_id::none));
  EXPECT_FALSE (is_affine (static_cast<unit_id> (unit_count)));
  EXPECT_EQ (0u, prefix_bit (prefix_id::none));
  EXPECT_EQ (0u, prefix_bit (static_cast<prefix_id> (30)));
  EXPECT_EQ (0u, prefix_bit (static_cast<prefix_id> (255)));
  EXPECT_EQ (1u, prefix_bit (prefix_id::quecto));
}

TEST (
    LumexUnitsConvertTest,
    GivenPrefixedDegreesCelsius_WhenConvertedAsAPoint_ThenTheOffsetStillApplies)
{
  prefixed_unit_t const milli_celsius
      = pair_of (unit_id::degree_celsius, prefix_id::milli);
  prefixed_unit_t const kelvin = pair_of (unit_id::kelvin);
  EXPECT_LE (
      ulp_distance (298.15,
                    convert_point (25000.0, milli_celsius, kelvin).value ()),
      1u);
  EXPECT_NEAR (
      25.0,
      convert_point (298.15, kelvin, pair_of (unit_id::degree_celsius))
          .value (),
      1e-12);
  EXPECT_NEAR (25000.0, convert_point (298.15, kelvin, milli_celsius).value (),
               1e-9);
  EXPECT_EQ (conversion_error::offset_unit,
             refused (1.0, milli_celsius, pair_of (unit_id::degree_celsius)));
  EXPECT_EQ (5.0, convert_difference (5000.0, milli_celsius,
                                      pair_of (unit_id::kelvin))
                      .value ());
}

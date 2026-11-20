/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_CORE_UNITS_DATA_HPP
#define LUMEX_CORE_UNITS_DATA_HPP

#if defined(__clang__)
// The warnings that a strict consumer (clang-cl /W4 maps to -WCL4, or
// -Weverything) reports on a table of aggregates and on LaTeX in comments.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#if __has_warning("-Wdeprecated-redundant-constexpr-static-def")
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#endif
#endif

/**
 * @file LumexUnitsData.hpp
 * @brief The data of the units catalog: enumerations, aggregates, the prefix
 * table and one row per unit, with nothing but `<cstddef>` and `<cstdint>`.
 * @details Everything here is standard C++11 and uses no macro or class of
 * LumexLib (the keywords are written as `constexpr` and `noexcept`), so the
 * header can be copied into a project that does not use the library; the
 * lookup, the formatting and the conversions are in `api/LumexUnitsApi.hpp`.
 *
 * The catalog is the X-macro `LUMEX_UNITS_CATALOG (X)`: one `X (...)` row per
 * unit, from which the enumeration `unit_id`, the descriptor array and the
 * compile-time checks are generated; `LUMEX_UNITS_ALIASES (X)` lists the other
 * spellings that the lookup accepts, `LUMEX_UNITS_PREFIXES (X)` and
 * `LUMEX_UNITS_PREFIX_ALIASES (X)` the 24 prefixes of the SI, and
 * `LUMEX_UNITS_SHADOWED (X)` the generated symbols that the symbol of another
 * unit takes away.
 *
 * Sources: the SI Brochure, 9th edition 2019, version 4.01 (June 2026), for
 * the international symbols; GOST 8.417-2024 "State system for ensuring the
 * uniformity of measurements. Units of quantities" (tables 1, 3, 5, 6, 7 and
 * 8, annex G) for the Russian symbols and the units of the standard; NIST
 * Special Publication 811 (2008), Appendix B.8, for the CGS and other old
 * units; the 10th CGPM (1954), Resolution 4, for the standard atmosphere.
 *
 * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
 * @see
 * https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
 *
 * Symbols are UTF-8 in plain `char` literals written as `\x` escapes, with
 * the glyph in a comment next to them, so that a translation unit compiles
 * the same with and without `/utf-8` and from C++11 to C++20. A literal that
 * continues after an escape is split into adjacent literals, because an
 * escape sequence swallows every following hexadecimal digit.
 */

#include <cstddef>
#include <cstdint>

namespace lumex
{
namespace core
{
namespace units
{
/**
 * @brief The seven base quantities of the SI, in the order of the
 * dimension array of `dimension_t`.
 * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
 */
enum class base_quantity : std::uint8_t
{
  length,
  mass,
  time,
  electric_current,
  thermodynamic_temperature,
  amount_of_substance,
  luminous_intensity
};

/** @brief The number of base quantities, the length of a dimension. */
constexpr std::size_t base_quantity_count = 7;

/**
 * @brief The grouping of the catalog: the parts of ISO/IEC 80000 (quantities
 * and units), with `general` for the dimensionless units that belong to no
 * part.
 */
enum class domain : std::uint8_t
{
  space_and_time,
  mechanics,
  thermodynamics,
  electromagnetism,
  light_and_radiation,
  acoustics,
  physical_chemistry,
  atomic_and_nuclear_physics,
  general
};

/**
 * @brief The quantity that a unit measures. Two units convert into each other
 * only when their kinds are equal (the hertz and the becquerel have one
 * dimension and two kinds); the degree Celsius and the kelvin are the two
 * temperature kinds.
 */
enum class quantity_kind : std::uint8_t
{
  length,
  mass,
  time,
  electric_current,
  thermodynamic_temperature,
  celsius_temperature,
  amount_of_substance,
  luminous_intensity,
  plane_angle,
  solid_angle,
  frequency,
  force,
  pressure,
  energy,
  power,
  electric_charge,
  voltage,
  capacitance,
  resistance,
  conductance,
  magnetic_flux,
  magnetic_flux_density,
  inductance,
  luminous_flux,
  illuminance,
  activity,
  absorbed_dose,
  dose_equivalent,
  catalytic_activity,
  volume,
  optical_power,
  area,
  apparent_power,
  reactive_power,
  dimensionless_ratio,
  logarithmic_level,
  loudness_level,
  frequency_interval,
  linear_density,
  velocity,
  acceleration,
  rotational_frequency,
  dynamic_viscosity,
  kinematic_viscosity,
  resistivity,
  magnetomotive_force,
  magnetic_field_strength,
  exposure,
  luminance
};

/** @brief Where a unit stands in the standards. */
enum class standing : std::uint8_t
{
  /** The seven SI base units. */
  si_base,
  /** The SI units with a special name (the 22 of SI Brochure Table 4). */
  si_special,
  /** Allowed on a par with the SI (GOST table 5, SI Brochure Table 8). */
  accepted,
  /** A relative or logarithmic unit (GOST table 6). */
  relative_log,
  /** Allowed temporarily (GOST table 7). */
  temporary,
  /** Listed in annex G of the GOST as an old unit, not recommended. */
  annex,
  /** Defined by a CGPM resolution and listed in neither table. */
  cgpm
};

/** @brief The system a unit belongs to. */
enum class unit_system : std::uint8_t
{
  si,
  non_si,
  cgs
};

/** @brief How a conversion factor is known. */
enum class factor_kind : std::uint8_t
{
  /** A definition: the factor is exactly the stored fraction. */
  exact,
  /** A measured value (the dalton and the X unit only: the parsec and the
   * metric horsepower are exact): the stored fraction is the printed value
   * of the source. */
  measured,
  /** The unit has no value in an SI unit (the logarithmic units). */
  none
};

/** @brief The two scripts of the symbols (GOST 8.417-2024, 8.1). */
enum class script : std::uint8_t
{
  /** Latin and Greek letters. */
  international,
  /** Cyrillic letters. */
  russian
};

/**
 * @brief The 24 prefixes, from quecto to quetta, in the order of their
 * exponents; the value is the bit of the prefix in a prefix mask and its index
 * in the prefix table. `none` stands for no prefix.
 */
enum class prefix_id : std::uint8_t
{
  quecto,
  ronto,
  yocto,
  zepto,
  atto,
  femto,
  pico,
  nano,
  micro,
  milli,
  centi,
  deci,
  deca,
  hecto,
  kilo,
  mega,
  giga,
  tera,
  peta,
  exa,
  zetta,
  yotta,
  ronna,
  quetta,
  none
};

/** @brief The number of prefixes (`prefix_id::none` is not one of them). */
constexpr std::size_t prefix_count = 24;

#define LUMEX_UNITS_SYMBOL(literal)                                           \
  ::lumex::core::units::make_symbol (literal, sizeof (literal) - 1, true)
#define LUMEX_UNITS_BARE_SYMBOL(literal)                                      \
  ::lumex::core::units::make_symbol (literal, sizeof (literal) - 1, false)
#define LUMEX_UNITS_NO_SYMBOL ::lumex::core::units::make_symbol ("", 0, false)
#define LUMEX_UNITS_DIMENSION(l, m, t, i, th, n, j)                           \
  ::lumex::core::units::make_dimension (l, m, t, i, th, n, j)
#define LUMEX_UNITS_SCALE(exponent10, numerator, denominator, pi_power)       \
  ::lumex::core::units::make_scale (exponent10, numerator, denominator,       \
                                    pi_power)
#define LUMEX_UNITS_NO_VALUE LUMEX_UNITS_SCALE (0, 0, 1, 0)
#define LUMEX_UNITS_NO_OFFSET LUMEX_UNITS_SCALE (0, 0, 1, 0)
#define LUMEX_UNITS_ALL_PREFIXES ::lumex::core::units::all_prefixes_mask
#define LUMEX_UNITS_NO_PREFIXES 0u
#define LUMEX_UNITS_THREE_PREFIXES(first, second, third)                      \
  (::lumex::core::units::prefix_bit (::lumex::core::units::prefix_id::first)  \
   | ::lumex::core::units::prefix_bit (                                       \
       ::lumex::core::units::prefix_id::second)                               \
   | ::lumex::core::units::prefix_bit (                                       \
       ::lumex::core::units::prefix_id::third))

// clang-format off
/**
 * @brief The catalog: one `X (...)` row per unit, written from the
 * standards, each with the comment that names its source.
 * @details The columns of a row are the enumerator of `unit_id`, the
 * `domain`, the `quantity_kind`, the `standing`, the `unit_system`, the
 * international and the Russian canonical symbol (`LUMEX_UNITS_SYMBOL`,
 * `LUMEX_UNITS_BARE_SYMBOL` for a symbol that takes no prefix,
 * `LUMEX_UNITS_NO_SYMBOL`), the dimension, the scale (the value in the
 * coherent SI unit), the offset, the `factor_kind`, the prefix set and
 * whether the symbol is a named compound. The other spellings are in
 * `LUMEX_UNITS_ALIASES`.
 */
#define LUMEX_UNITS_CATALOG(X) \
  /* Metre (length; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of length.
   */ \
  X (metre, space_and_time, length, si_base, si, \
     LUMEX_UNITS_SYMBOL ("m"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" /* U+043C */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Gram (mass; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, chapter 3 (the kilogram and its prefixes).)
   * GOST 8.417-2024 (units of quantities), table 1, 7.5.
   * The catalog unit of mass is the gram, 10^-3 of the coherent SI unit
   * kilogram, so that the prefixes attach to "g"; the pair (kilo, gram) is the
   * kilogram, symbol kg.
   */ \
  X (gram, mechanics, mass, si_base, si, \
     LUMEX_UNITS_SYMBOL ("g"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB3" /* U+0433 */), \
     LUMEX_UNITS_DIMENSION (0, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-3, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Second (time; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of time.
   */ \
  X (second, space_and_time, time, si_base, si, \
     LUMEX_UNITS_SYMBOL ("s"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x81" /* U+0441 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Ampere (electric current; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of electric current.
   */ \
  X (ampere, electromagnetism, electric_current, si_base, si, \
     LUMEX_UNITS_SYMBOL ("A"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x90" /* U+0410 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Kelvin (thermodynamic temperature; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of thermodynamic temperature. The input alias is U+212A
   * KELVIN SIGN, which Unicode decomposes to the letter K.
   */ \
  X (kelvin, thermodynamics, thermodynamic_temperature, si_base, si, \
     LUMEX_UNITS_SYMBOL ("K"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9A" /* U+041A */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 1, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Mole (amount of substance; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of amount of substance.
   */ \
  X (mole, physical_chemistry, amount_of_substance, si_base, si, \
     LUMEX_UNITS_SYMBOL ("mol"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBE" "\xD0\xBB" "\xD1\x8C" /* U+043C U+043E U+043B U+044C */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 1, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Candela (luminous intensity; SI base unit).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 2.)
   * GOST 8.417-2024 (units of quantities), table 1.
   * SI base unit of luminous intensity.
   */ \
  X (candela, light_and_radiation, luminous_intensity, si_base, si, \
     LUMEX_UNITS_SYMBOL ("cd"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB4" /* U+043A U+0434 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 1), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Radian (plane angle; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{rad} = \text{m} \cdot \text{m}^{-1} = 1$$
   *
   * The symbol rad and its Russian form belong to the radian; the old unit of
   * absorbed dose rad has the international symbol rd only.
   */ \
  X (radian, space_and_time, plane_angle, si_special, si, \
     LUMEX_UNITS_SYMBOL ("rad"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x80" "\xD0\xB0" "\xD0\xB4" /* U+0440 U+0430 U+0434 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Steradian (solid angle; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{sr} = \text{m}^{2} \cdot \text{m}^{-2} = 1$$
   */ \
  X (steradian, space_and_time, solid_angle, si_special, si, \
     LUMEX_UNITS_SYMBOL ("sr"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x81" "\xD1\x80" /* U+0441 U+0440 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Hertz (frequency; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Hz} = \text{s}^{-1}$$
   *
   * Only for periodic phenomena (SI Brochure Table 4 note (d)); the becquerel
   * is the unit of the same dimension for stochastic processes, so the kinds
   * differ and a conversion is refused.
   */ \
  X (hertz, space_and_time, frequency, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Hz"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD1\x86" /* U+0413 U+0446 */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Newton (force; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{N} = \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   */ \
  X (newton, mechanics, force, si_special, si, \
     LUMEX_UNITS_SYMBOL ("N"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9D" /* U+041D */), \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Pascal (pressure; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Pa} = \text{N} \cdot \text{m}^{-2} = \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (pascal_unit, mechanics, pressure, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Pa"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9F" "\xD0\xB0" /* U+041F U+0430 */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Joule (energy; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{J} = \text{N} \cdot \text{m} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2}$$
   */ \
  X (joule, mechanics, energy, si_special, si, \
     LUMEX_UNITS_SYMBOL ("J"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x94" "\xD0\xB6" /* U+0414 U+0436 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Watt (power; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{W} = \text{J} \cdot \text{s}^{-1} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3}$$
   */ \
  X (watt, mechanics, power, si_special, si, \
     LUMEX_UNITS_SYMBOL ("W"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x92" "\xD1\x82" /* U+0412 U+0442 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Coulomb (electric charge; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{C} = \text{A} \cdot \text{s} = \text{s} \cdot \text{A}$$
   */ \
  X (coulomb, electromagnetism, electric_charge, si_special, si, \
     LUMEX_UNITS_SYMBOL ("C"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9A" "\xD0\xBB" /* U+041A U+043B */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Volt (voltage; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{V} = \text{W} \cdot \text{A}^{-1} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3} \cdot \text{A}^{-1}$$
   */ \
  X (volt, electromagnetism, voltage, si_special, si, \
     LUMEX_UNITS_SYMBOL ("V"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x92" /* U+0412 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, -1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Farad (capacitance; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{F} = \text{C} \cdot \text{V}^{-1} = \text{kg}^{-1} \cdot \text{m}^{-2} \cdot \text{s}^{4} \cdot \text{A}^{2}$$
   */ \
  X (farad, electromagnetism, capacitance, si_special, si, \
     LUMEX_UNITS_SYMBOL ("F"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xA4" /* U+0424 */), \
     LUMEX_UNITS_DIMENSION (-2, -1, 4, 2, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Ohm (resistance; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\Omega = \text{V} \cdot \text{A}^{-1} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3} \cdot \text{A}^{-2}$$
   *
   * The canonical international symbol is U+03A9 GREEK CAPITAL LETTER OMEGA;
   * the input aliases are U+2126 OHM SIGN (which Unicode decomposes to U+03A9)
   * and the ASCII word ohm.
   */ \
  X (ohm, electromagnetism, resistance, si_special, si, \
     LUMEX_UNITS_SYMBOL ("\xCE\xA9" /* U+03A9 GREEK CAPITAL LETTER OMEGA */), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9E" "\xD0\xBC" /* U+041E U+043C */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, -2, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Siemens (conductance; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{S} = \text{A} \cdot \text{V}^{-1} = \text{kg}^{-1} \cdot \text{m}^{-2} \cdot \text{s}^{3} \cdot \text{A}^{2}$$
   */ \
  X (siemens, electromagnetism, conductance, si_special, si, \
     LUMEX_UNITS_SYMBOL ("S"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xA1" "\xD0\xBC" /* U+0421 U+043C */), \
     LUMEX_UNITS_DIMENSION (-2, -1, 3, 2, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Weber (magnetic flux; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Wb} = \text{V} \cdot \text{s} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2} \cdot \text{A}^{-1}$$
   */ \
  X (weber, electromagnetism, magnetic_flux, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Wb"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x92" "\xD0\xB1" /* U+0412 U+0431 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, -1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Tesla (magnetic flux density; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{T} = \text{Wb} \cdot \text{m}^{-2} = \text{kg} \cdot \text{s}^{-2} \cdot \text{A}^{-1}$$
   */ \
  X (tesla, electromagnetism, magnetic_flux_density, si_special, si, \
     LUMEX_UNITS_SYMBOL ("T"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xA2" "\xD0\xBB" /* U+0422 U+043B */), \
     LUMEX_UNITS_DIMENSION (0, 1, -2, -1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Henry (inductance; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{H} = \text{Wb} \cdot \text{A}^{-1} = \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2} \cdot \text{A}^{-2}$$
   */ \
  X (henry, electromagnetism, inductance, si_special, si, \
     LUMEX_UNITS_SYMBOL ("H"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD0\xBD" /* U+0413 U+043D */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, -2, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Degree Celsius (celsius temperature; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4 note (f).)
   * GOST 8.417-2024 (units of quantities), table 1 note 1, table 3.
   *
   * $$t / {}^{\circ}\text{C} = T / \text{K} - 273.15 \qquad 1\ {}^{\circ}\text{C} = 1\ \text{K}$$
   *
   * The temperature in degrees Celsius is t = T - 273.15 K, the offset is
   * exact; a difference of temperatures is the same number of kelvin and
   * degrees Celsius. Prefixes are allowed (SI Brochure 2.3.4: the milli degree
   * Celsius).
   * Both columns use the Latin letter C (the table of the standard does); the
   * Cyrillic letter in the Russian text is rejected.
   */ \
  X (degree_celsius, thermodynamics, celsius_temperature, si_special, si, \
     LUMEX_UNITS_SYMBOL ("\xC2\xB0" "C" /* U+00B0 DEGREE SIGN C */), \
     LUMEX_UNITS_SYMBOL ("\xC2\xB0" "C" /* U+00B0 DEGREE SIGN C */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 1, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_SCALE (-1, 5463, 2, 0), \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Lumen (luminous flux; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{lm} = \text{cd} \cdot \text{sr} = \text{cd}$$
   */ \
  X (lumen, light_and_radiation, luminous_flux, si_special, si, \
     LUMEX_UNITS_SYMBOL ("lm"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBB" "\xD0\xBC" /* U+043B U+043C */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 1), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Lux (illuminance; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{lx} = \text{lm} \cdot \text{m}^{-2} = \text{m}^{-2} \cdot \text{cd}$$
   */ \
  X (lux, light_and_radiation, illuminance, si_special, si, \
     LUMEX_UNITS_SYMBOL ("lx"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBB" "\xD0\xBA" /* U+043B U+043A */), \
     LUMEX_UNITS_DIMENSION (-2, 0, 0, 0, 0, 0, 1), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Becquerel (activity; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Bq} = \text{s}^{-1}$$
   *
   * Only for stochastic processes (SI Brochure Table 4 note (d)).
   */ \
  X (becquerel, atomic_and_nuclear_physics, activity, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Bq"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x91" "\xD0\xBA" /* U+0411 U+043A */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Gray (absorbed dose; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Gy} = \text{J} \cdot \text{kg}^{-1} = \text{m}^{2} \cdot \text{s}^{-2}$$
   */ \
  X (gray, atomic_and_nuclear_physics, absorbed_dose, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Gy"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD1\x80" /* U+0413 U+0440 */), \
     LUMEX_UNITS_DIMENSION (2, 0, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Sievert (dose equivalent; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{Sv} = \text{J} \cdot \text{kg}^{-1} = \text{m}^{2} \cdot \text{s}^{-2}$$
   */ \
  X (sievert, atomic_and_nuclear_physics, dose_equivalent, si_special, si, \
     LUMEX_UNITS_SYMBOL ("Sv"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x97" "\xD0\xB2" /* U+0417 U+0432 */), \
     LUMEX_UNITS_DIMENSION (2, 0, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Katal (catalytic activity; SI unit with a special name).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 4.)
   * GOST 8.417-2024 (units of quantities), table 3.
   *
   * $$\text{kat} = \text{mol} \cdot \text{s}^{-1} = \text{s}^{-1} \cdot \text{mol}$$
   */ \
  X (katal, physical_chemistry, catalytic_activity, si_special, si, \
     LUMEX_UNITS_SYMBOL ("kat"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB0" "\xD1\x82" /* U+043A U+0430 U+0442 */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 1, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Tonne (mass; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (e).)
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{t} = 1000\ \text{kg}$$
   */ \
  X (tonne, mechanics, mass, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("t"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x82" /* U+0442 */), \
     LUMEX_UNITS_DIMENSION (0, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (3, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Dalton (unified atomic mass unit) (mass; non-SI unit allowed on a par with
   *    the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (i).)
   * GOST 8.417-2024 (units of quantities), table 5 and note 2.
   *
   * $$1\ \text{Da} = 1.660\,539\,068\,92 \times 10^{-27}\ \text{kg}$$
   *
   * Measured, not defined: CODATA 2022 value 1.660 539 068 92(52) x 10^-27 kg,
   * as in the SI Brochure V4.01. The table of the standard still has the
   * CODATA 2018 value 1.660 539 066 60(50) x 10^-27 kg (the two differ from
   * the 10th significant digit).
   * The Russian symbol takes no prefixes (table 5 note 2); the symbol u is an
   * alternative name and symbol of the same unit. Table 5 of GOST 8.417-2024
   * gives the Russian name dalton beside the symbol of the Russian column and
   * no symbol of its own for it (no Da in Cyrillic), so none is added.
   */ \
  X (dalton, atomic_and_nuclear_physics, mass, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("Da"), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xB0" ".\xD0\xB5" ".\xD0\xBC" "." /* U+0430 . U+0435 . U+043C . */), \
     LUMEX_UNITS_DIMENSION (0, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-36, 41513476723, 25, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     measured, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Minute (of time) (time; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8.)
   * GOST 8.417-2024 (units of quantities), table 5 note 2.
   *
   * $$1\ \text{min} = 60\ \text{s}$$
   */ \
  X (minute, space_and_time, time, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("min"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xB8" "\xD0\xBD" /* U+043C U+0438 U+043D */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (1, 6, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Hour (time; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8.)
   * GOST 8.417-2024 (units of quantities), table 5 note 2.
   *
   * $$1\ \text{h} = 3600\ \text{s}$$
   */ \
  X (hour, space_and_time, time, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("h"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x87" /* U+0447 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 36, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Day (time; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8.)
   * GOST 8.417-2024 (units of quantities), table 5 note 2.
   *
   * $$1\ \text{d} = 86400\ \text{s}$$
   */ \
  X (day, space_and_time, time, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("d"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x81" "\xD1\x83" "\xD1\x82" /* U+0441 U+0443 U+0442 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 864, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Degree (of plane angle) (plane angle; non-SI unit allowed on a par with
   *    the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8.)
   * GOST 8.417-2024 (units of quantities), table 5 notes 2, 4.
   *
   * $$1\ {}^{\circ} = \frac{\pi}{180}$$
   *
   * Not rational: the factor contains the number pi, so a conversion of a
   * value is within one ulp of the exact result.
   */ \
  X (degree, space_and_time, plane_angle, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("\xC2\xB0" /* U+00B0 DEGREE SIGN */), \
     LUMEX_UNITS_SYMBOL ("\xC2\xB0" /* U+00B0 DEGREE SIGN */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-1, 1, 18, 1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Minute (of plane angle) (plane angle; non-SI unit allowed on a par with
   *    the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8.)
   * GOST 8.417-2024 (units of quantities), table 5 notes 2, 4.
   *
   * $$1\ ^{\prime} = \frac{\pi}{10800}$$
   *
   * Contains pi (see the degree). The ASCII apostrophe is an input alias (the
   * online text of the standard writes it); it takes no prefix and is never
   * written.
   */ \
  X (arcminute, space_and_time, plane_angle, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("\xE2\x80\xB2" /* U+2032 PRIME */), \
     LUMEX_UNITS_SYMBOL ("\xE2\x80\xB2" /* U+2032 PRIME */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 108, 1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Second (of plane angle), arcsecond (plane angle; non-SI unit allowed on a
   *    par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (a).)
   * GOST 8.417-2024 (units of quantities), table 5 notes 2, 4.
   *
   * $$1\ ^{\prime\prime} = \frac{\pi}{648000}$$
   *
   * Contains pi (see the degree).
   * The symbol as takes the prefixes milli, micro and pico (mas, uas, pas of
   * SI Brochure Table 8 note (a)); the sign U+2033 and the Russian sign take
   * none. The ASCII quotation mark is an input alias (never written, no
   * prefix).
   */ \
  X (arcsecond, space_and_time, plane_angle, accepted, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("\xE2\x80\xB3" /* U+2033 DOUBLE PRIME */), \
     LUMEX_UNITS_BARE_SYMBOL ("\xE2\x80\xB3" /* U+2033 DOUBLE PRIME */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-3, 1, 648, 1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_THREE_PREFIXES (milli, micro, pico), false) \
  /* Gon (grad) (plane angle; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{gon} = \frac{\pi}{200}$$
   *
   * Contains pi (see the degree). The Russian symbol equals the generated
   * symbol of the hectoradian: the unit wins. Prefixes are allowed (the table
   * of the standard does not forbid them).
   */ \
  X (gon, space_and_time, plane_angle, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("gon"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB3" "\xD1\x80" "\xD0\xB0" "\xD0\xB4" /* U+0433 U+0440 U+0430 U+0434 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 2, 1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Litre (volume; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (d).)
   * GOST 8.417-2024 (units of quantities), table 5 note 5.
   *
   * $$1\ \text{L} = 0.001\ \text{m}^{3}$$
   *
   * The capital L is canonical (16th CGPM 1979, against confusion with the
   * digit 1); the lower-case l is an accepted alias.
   */ \
  X (litre, space_and_time, volume, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("L"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBB" /* U+043B */), \
     LUMEX_UNITS_DIMENSION (3, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-3, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Astronomical unit (length; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (j).)
   * GOST 8.417-2024 (units of quantities), table 5 and note 2.
   *
   * $$1\ \text{au} = 1.495978707 \times 10^{11}\ \text{m}$$
   *
   * IAU 2012 Resolution B2. The standard has ua in its international column;
   * the SI Brochure has au, which is canonical.
   */ \
  X (astronomical_unit, space_and_time, length, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("au"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB0" ".\xD0\xB5" "." /* U+0430 . U+0435 . */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 1495978707, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Light year (length; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{ly} = 9.4607304725808 \times 10^{15}\ \text{m}$$
   */ \
  X (light_year, space_and_time, length, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("ly"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x81" "\xD0\xB2" ". \xD0\xB3" "\xD0\xBE" "\xD0\xB4" /* U+0441 U+0432 .  U+0433 U+043E U+0434 */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 94607304725808, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Parsec (length; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   * IAU 2015 Resolution B2 (International Astronomical Union).
   *
   * $$1\ \text{pc} = \frac{648000 \times 149597870700}{\pi}\ \text{m}$$
   *
   * Exactly 648000 / pi astronomical units: IAU 2015 Resolution B2 implies it
   * (Mamajek et al., arXiv:1510.06262); the table of the standard rounds it to
   * 3.085677e16 m. Contains pi.
   */ \
  X (parsec, space_and_time, length, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("pc"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBF" "\xD0\xBA" /* U+043F U+043A */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (5, 969394202136, 1, -1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Diopter (optical power; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5 and note 2.
   *
   * $$1\ \text{diopter} = 1\ \text{m}^{-1}$$
   *
   * The standard has no international symbol.
   */ \
  X (diopter, space_and_time, optical_power, accepted, non_si, \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_SYMBOL ("\xD0\xB4" "\xD0\xBF" "\xD1\x82" "\xD1\x80" /* U+0434 U+043F U+0442 U+0440 */), \
     LUMEX_UNITS_DIMENSION (-1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Hectare (area; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (b).)
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{ha} = 10000\ \text{m}^{2}$$
   */ \
  X (hectare, space_and_time, area, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("ha"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB3" "\xD0\xB0" /* U+0433 U+0430 */), \
     LUMEX_UNITS_DIMENSION (2, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (4, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Electronvolt (energy; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (l).)
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{eV} = 1.602\,176\,634 \times 10^{-19}\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2}$$
   *
   * Exact since the 26th CGPM (2018) fixed the elementary charge.
   */ \
  X (electronvolt, atomic_and_nuclear_physics, energy, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("eV"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x8D" "\xD0\x92" /* U+044D U+0412 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-27, 801088317, 5, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Kilowatt-hour (energy; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{kW} \cdot \text{h} = 3600000\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2}$$
   *
   * A named compound: the symbol is the whole product; the alias uses U+22C5
   * DOT OPERATOR.
   */ \
  X (kilowatt_hour, mechanics, energy, accepted, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("kW\xC2\xB7" "h" /* kW U+00B7 MIDDLE DOT h */), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBA" "\xD0\x92" "\xD1\x82" "\xC2\xB7" "\xD1\x87" /* U+043A U+0412 U+0442 U+00B7 MIDDLE DOT U+0447 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (5, 36, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Volt-ampere (apparent power; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{V} \cdot \text{A} = 1\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3}$$
   *
   * A named compound; the prefix goes in front of the whole symbol (kV.A).
   */ \
  X (volt_ampere, electromagnetism, apparent_power, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("V\xC2\xB7" "A" /* V U+00B7 MIDDLE DOT A */), \
     LUMEX_UNITS_SYMBOL ("\xD0\x92" "\xC2\xB7" "\xD0\x90" /* U+0412 U+00B7 MIDDLE DOT U+0410 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, true) \
  /* Var (reactive power; non-SI unit allowed on a par with the SI).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (n).)
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{var} = 1\ \text{V} \cdot \text{A} = 1\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3}$$
   */ \
  X (var, electromagnetism, reactive_power, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("var"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB2" "\xD0\xB0" "\xD1\x80" /* U+0432 U+0430 U+0440 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Ampere-hour (electric charge; non-SI unit allowed on a par with the SI).
   *
   * GOST 8.417-2024 (units of quantities), table 5.
   *
   * $$1\ \text{A} \cdot \text{h} = 3600\ \text{s} \cdot \text{A}$$
   *
   * A named compound; the prefix goes in front of the whole symbol (mA.h).
   */ \
  X (ampere_hour, electromagnetism, electric_charge, accepted, non_si, \
     LUMEX_UNITS_SYMBOL ("A\xC2\xB7" "h" /* A U+00B7 MIDDLE DOT h */), \
     LUMEX_UNITS_SYMBOL ("\xD0\x90" "\xC2\xB7" "\xD1\x87" /* U+0410 U+00B7 MIDDLE DOT U+0447 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 1, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 36, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, true) \
  /* Unit one (dimensionless ratio; unit of a relative or logarithmic
   *    quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, 5.4.7.)
   * GOST 8.417-2024 (units of quantities), table 6.
   * No prefix may be attached to the symbol 1.
   */ \
  X (one, general, dimensionless_ratio, relative_log, si, \
     LUMEX_UNITS_SYMBOL ("1"), \
     LUMEX_UNITS_SYMBOL ("1"), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Percent (dimensionless ratio; unit of a relative or logarithmic quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, 5.4.7.)
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * $$1\ \% = 0.01$$
   */ \
  X (percent, general, dimensionless_ratio, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("%"), \
     LUMEX_UNITS_SYMBOL ("%"), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Permille (dimensionless ratio; unit of a relative or logarithmic
   *    quantity).
   *
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * $$1\ \text{\textperthousand} = 0.001$$
   *
   * The online text of the standard has the sign as an image; the code point
   * is U+2030 PER MILLE SIGN.
   */ \
  X (permille, general, dimensionless_ratio, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("\xE2\x80\xB0" /* U+2030 PER MILLE SIGN */), \
     LUMEX_UNITS_SYMBOL ("\xE2\x80\xB0" /* U+2030 PER MILLE SIGN */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-3, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Parts per million (dimensionless ratio; unit of a relative or logarithmic
   *    quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, 5.4.7.)
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * $$1\ \text{ppm} = 10^{-6}$$
   *
   * The Russian symbol is the three letters with the superscript minus and one
   * (U+207B U+00B9).
   */ \
  X (ppm, general, dimensionless_ratio, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("ppm"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBB" "\xD0\xBD" "\xE2\x81\xBB" "\xC2\xB9" /* U+043C U+043B U+043D U+207B SUPERSCRIPT MINUS U+00B9 SUPERSCRIPT ONE */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-6, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Bel (logarithmic level; unit of a relative or logarithmic quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (m).)
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic: the value is a logarithm of a ratio of two powers (1 B = lg
   * (P2 / P1)), not a multiple of an SI unit, so there is no expansion to base
   * units and no conversion factor.
   */ \
  X (bel, general, logarithmic_level, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("B"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x91" /* U+0411 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Decibel (logarithmic level; unit of a relative or logarithmic quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (m).)
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic, 1 dB = 0.1 B; no expansion to base units and no conversion
   * factor (see the bel).
   */ \
  X (decibel, general, logarithmic_level, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("dB"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB4" "\xD0\x91" /* U+0434 U+0411 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Neper (logarithmic level; unit of a relative or logarithmic quantity).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (m).)
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic: the natural logarithm of an amplitude ratio; no expansion to
   * base units and no conversion factor.
   */ \
  X (neper, general, logarithmic_level, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("Np"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9D" "\xD0\xBF" /* U+041D U+043F */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Phon (loudness level; unit of a relative or logarithmic quantity).
   *
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic: a loudness level; no expansion to base units and no
   * conversion factor.
   */ \
  X (phon, acoustics, loudness_level, relative_log, non_si, \
     LUMEX_UNITS_SYMBOL ("phon"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x84" "\xD0\xBE" "\xD0\xBD" /* U+0444 U+043E U+043D */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Octave (frequency interval; unit of a relative or logarithmic quantity).
   *
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic: log2 of a frequency ratio; no expansion to base units and no
   * conversion factor. The standard has no international symbol.
   */ \
  X (octave, acoustics, frequency_interval, relative_log, non_si, \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_SYMBOL ("\xD0\xBE" "\xD0\xBA" "\xD1\x82" /* U+043E U+043A U+0442 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Decade (frequency interval; unit of a relative or logarithmic quantity).
   *
   * GOST 8.417-2024 (units of quantities), table 6.
   *
   * Logarithmic: lg of a frequency ratio; no expansion to base units and no
   * conversion factor. The standard has no international symbol.
   */ \
  X (decade, acoustics, frequency_interval, relative_log, non_si, \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_SYMBOL ("\xD0\xB4" "\xD0\xB5" "\xD0\xBA" /* U+0434 U+0435 U+043A */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_NO_VALUE, \
     LUMEX_UNITS_NO_OFFSET, \
     none, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Nautical mile (length; non-SI unit allowed temporarily).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (k).)
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{n mile} = 1852\ \text{m}$$
   */ \
  X (nautical_mile, space_and_time, length, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("n mile"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xB8" "\xD0\xBB" "\xD1\x8F" /* U+043C U+0438 U+043B U+044F */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1852, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Foot (length; non-SI unit allowed temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{ft} = 0.3048\ \text{m}$$
   *
   * The symbol equals the generated symbol of the femtotonne: the unit wins.
   */ \
  X (foot, space_and_time, length, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("ft"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x84" "\xD1\x82" /* U+0444 U+0442 */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-1, 381, 125, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Angstrom (length; non-SI unit allowed temporarily).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (f).)
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{\AA} = 10^{-10}\ \text{m}$$
   *
   * U+00C5 LATIN CAPITAL LETTER A WITH RING ABOVE is canonical; U+212B
   * ANGSTROM SIGN (which Unicode decomposes to it) is an alias. The online
   * text has the sign as an image.
   */ \
  X (angstrom, space_and_time, length, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("\xC3\x85" /* U+00C5 LATIN CAPITAL LETTER A WITH RING ABOVE */), \
     LUMEX_UNITS_SYMBOL ("\xC3\x85" /* U+00C5 LATIN CAPITAL LETTER A WITH RING ABOVE */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-10, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Metric carat (mass; non-SI unit allowed temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{carat} = 2 \times 10^{-4}\ \text{kg}$$
   *
   * The standard has no international symbol.
   */ \
  X (carat, mechanics, mass, temporary, non_si, \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB0" "\xD1\x80" /* U+043A U+0430 U+0440 */), \
     LUMEX_UNITS_DIMENSION (0, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-3, 1, 5, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Tex (linear density; non-SI unit allowed temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{tex} = 10^{-6}\ \text{kg} \cdot \text{m}^{-1}$$
   */ \
  X (tex, mechanics, linear_density, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("tex"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x82" "\xD0\xB5" "\xD0\xBA" "\xD1\x81" /* U+0442 U+0435 U+043A U+0441 */), \
     LUMEX_UNITS_DIMENSION (-1, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-6, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Knot (velocity; non-SI unit allowed temporarily).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (k).)
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{kn} = \frac{1852}{3600}\ \text{m} \cdot \text{s}^{-1}$$
   *
   * 1 nautical mile per hour, 1852 / 3600 m/s exactly; the standard rounds it
   * to 0.514(4) m/s.
   */ \
  X (knot, space_and_time, velocity, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("kn"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x83" "\xD0\xB7" /* U+0443 U+0437 */), \
     LUMEX_UNITS_DIMENSION (1, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 463, 9, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Gal (acceleration; non-SI unit allowed temporarily).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (g).)
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{Gal} = 0.01\ \text{m} \cdot \text{s}^{-2}$$
   *
   * 1 Gal = 1 cm/s^2; the only CGS unit that the SI Brochure keeps.
   */ \
  X (gal, mechanics, acceleration, temporary, cgs, \
     LUMEX_UNITS_SYMBOL ("Gal"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD0\xB0" "\xD0\xBB" /* U+0413 U+0430 U+043B */), \
     LUMEX_UNITS_DIMENSION (1, 0, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Revolution per second (rotational frequency; non-SI unit allowed
   *    temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{r} \cdot \text{s}^{-1} = 1\ \text{s}^{-1}$$
   *
   * The standard gives 1 s^-1 (not 2 pi rad/s); the kind is the rotational
   * frequency, so a conversion to hertz is refused.
   */ \
  X (revolution_per_second, space_and_time, rotational_frequency, temporary, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("r/s"), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBE" "\xD0\xB1" "/\xD1\x81" /* U+043E U+0431 / U+0441 */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Revolution per minute (rotational frequency; non-SI unit allowed
   *    temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{r} \cdot \text{min}^{-1} = \frac{1}{60}\ \text{s}^{-1}$$
   *
   * The standard gives 1/60 s^-1 (see the revolution per second).
   */ \
  X (revolution_per_minute, space_and_time, rotational_frequency, temporary, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("r/min"), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBE" "\xD0\xB1" "/\xD0\xBC" "\xD0\xB8" "\xD0\xBD" /* U+043E U+0431 / U+043C U+0438 U+043D */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-1, 1, 6, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Bar (pressure; non-SI unit allowed temporarily).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (h).)
   * GOST 8.417-2024 (units of quantities), table 7.
   *
   * $$1\ \text{bar} = 100000\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (bar, mechanics, pressure, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("bar"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB1" "\xD0\xB0" "\xD1\x80" /* U+0431 U+0430 U+0440 */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (5, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Millimetre of mercury (conventional) (pressure; non-SI unit allowed
   *    temporarily).
   *
   * GOST 8.417-2024 (units of quantities), table 7.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   * Conventional definition: mercury density 13595.1 kg/m^3.
   *
   * $$1\ \text{mm Hg} = 13595.1 \cdot 9.80665 \cdot 10^{-3}\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   *
   * Conventional definition: 13595.1 kg/m^3 times 9.80665 m/s^2 times 1 mm =
   * 133.322387415 Pa exactly. The table of the standard rounds it to 133.3224
   * Pa. It is not the torr (101325/760 Pa), which is 1.4e-7 smaller in
   * relative terms. The Russian alias is the form printed in the online text
   * of table 7 (input only).
   */ \
  X (millimetre_of_mercury, mechanics, pressure, temporary, non_si, \
     LUMEX_UNITS_SYMBOL ("mm Hg"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBC" " \xD1\x80" "\xD1\x82" ". \xD1\x81" "\xD1\x82" "." /* U+043C U+043C   U+0440 U+0442 .  U+0441 U+0442 . */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-8, 26664477483, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* X unit (length; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{X} = 1.00206 \times 10^{-13}\ \text{m}$$
   *
   * Approximate in the standard.
   */ \
  X (x_unit, space_and_time, length, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("X"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB8" "\xD0\xBA" "\xD1\x81" "-\xD0\xB5" "\xD0\xB4" "." /* U+0438 U+043A U+0441 - U+0435 U+0434 . */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-17, 50103, 5, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     measured, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Barn (area; old non-SI unit of annex G of the GOST).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (c).)
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{b} = 10^{-28}\ \text{m}^{2}$$
   */ \
  X (barn, atomic_and_nuclear_physics, area, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("b"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB1" /* U+0431 */), \
     LUMEX_UNITS_DIMENSION (2, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-28, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Centner (quintal) (mass; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{q} = 100\ \text{kg}$$
   */ \
  X (centner, mechanics, mass, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("q"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x86" /* U+0446 */), \
     LUMEX_UNITS_DIMENSION (0, 1, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Square degree (solid angle; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \left({}^{\circ}\right)^{2} = \frac{\pi^{2}}{32400}$$
   *
   * (pi/180)^2 sr = 3.0462...e-4 sr, contains pi. The international symbol of
   * the standard is (sq. deg), its Russian symbol (deg)^2; the Russian form is
   * also accepted as an international alias.
   */ \
  X (square_degree, space_and_time, solid_angle, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("(sq. \xC2\xB0" ")" /* (sq.  U+00B0 DEGREE SIGN ) */), \
     LUMEX_UNITS_BARE_SYMBOL ("(\xC2\xB0" ")\xC2\xB2" /* ( U+00B0 DEGREE SIGN ) U+00B2 SUPERSCRIPT TWO */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 324, 2), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Dyne (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{dyn} = 10^{-5}\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   */ \
  X (dyne, mechanics, force, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("dyn"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB4" "\xD0\xB8" "\xD0\xBD" /* U+0434 U+0438 U+043D */), \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-5, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Kilogram-force (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{kgf} = 9.80665\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   *
   * Exact: the standard acceleration of gravity 9.80665 m/s^2 times 1 kg.
   */ \
  X (kilogram_force, mechanics, force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("kgf"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB3" "\xD1\x81" /* U+043A U+0433 U+0441 */), \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-4, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Kilopond (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{kp} = 9.80665\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   *
   * No Russian symbol: the standard gives none and none was found in the other
   * sources read; the letter that the unit would need as an alias (a bare
   * letter p) would make the lookup of that letter surprising, so none is
   * added.
   */ \
  X (kilopond, mechanics, force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("kp"), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-4, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Gram-force (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{gf} = 9.80665 \times 10^{-3}\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   *
   * The Russian symbol differs from the gauss only in the letter case.
   */ \
  X (gram_force, mechanics, force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("gf"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB3" "\xD1\x81" /* U+0433 U+0441 */), \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-7, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Pond (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{p} = 9.80665 \times 10^{-3}\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   *
   * No Russian symbol: the standard gives none and none was found in the other
   * sources read; a bare letter would make the lookup of that letter
   * surprising, so none is added.
   */ \
  X (pond, mechanics, force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("p"), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-7, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Tonne-force (force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{tf} = 9806.65\ \text{kg} \cdot \text{m} \cdot \text{s}^{-2}$$
   */ \
  X (tonne_force, mechanics, force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("tf"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x82" "\xD1\x81" /* U+0442 U+0441 */), \
     LUMEX_UNITS_DIMENSION (1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-1, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Kilogram-force per square centimetre (pressure; old non-SI unit of annex G
   *    of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{kgf} \cdot \text{cm}^{-2} = 98066.5\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (kilogram_force_per_square_centimetre, mechanics, pressure, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("kgf/cm\xC2\xB2" /* kgf/cm U+00B2 SUPERSCRIPT TWO */), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBA" "\xD0\xB3" "\xD1\x81" "/\xD1\x81" "\xD0\xBC" "\xC2\xB2" /* U+043A U+0433 U+0441 / U+0441 U+043C U+00B2 SUPERSCRIPT TWO */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Kilopond per square centimetre (pressure; old non-SI unit of annex G of
   *    the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{kp} \cdot \text{cm}^{-2} = 98066.5\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (kilopond_per_square_centimetre, mechanics, pressure, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("kp/cm\xC2\xB2" /* kp/cm U+00B2 SUPERSCRIPT TWO */), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Millimetre of water (pressure; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{mm H}_{2}\text{O} = 9.80665\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (millimetre_of_water, mechanics, pressure, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("mm H\xE2\x82\x82" "O" /* mm H U+2082 SUBSCRIPT TWO O */), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBC" "\xD0\xBC" " \xD0\xB2" "\xD0\xBE" "\xD0\xB4" ". \xD1\x81" "\xD1\x82" "." /* U+043C U+043C   U+0432 U+043E U+0434 .  U+0441 U+0442 . */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-4, 196133, 2, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Torr (pressure; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (10th CGPM 1954, Resolution 4, reprinted in the SI Brochure V4.01, Appendix 1, p. 158.)
   *
   * $$1\ \text{Torr} = \frac{101325}{760}\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   *
   * 1/760 of the standard atmosphere, 133.322 368 421... Pa; the table of the
   * standard rounds it to 133.322 Pa. No standard read gives a Russian symbol
   * (GOST 8.417-2024 table 7 and annex G, GOST 8.417-2002 annex B, Cyrillic
   * letter Ve); the Russian name written as a word is an input alias only and
   * is never written.
   */ \
  X (torr, mechanics, pressure, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("Torr"), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 20265, 152, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Kilopond per square millimetre (pressure; old non-SI unit of annex G of
   *    the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{kp} \cdot \text{mm}^{-2} = 9.80665 \times 10^{6}\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   */ \
  X (kilopond_per_square_millimetre, mechanics, pressure, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("kp/mm\xC2\xB2" /* kp/mm U+00B2 SUPERSCRIPT TWO */), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (1, 980665, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Erg (energy; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{erg} = 10^{-7}\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2}$$
   */ \
  X (erg, mechanics, energy, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("erg"), \
     LUMEX_UNITS_SYMBOL ("\xD1\x8D" "\xD1\x80" "\xD0\xB3" /* U+044D U+0440 U+0433 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-7, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Metric horsepower (power; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{metric horsepower} = 735.49875\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-3}$$
   *
   * Exactly 75 kgf m/s = 75 x 9.80665 W = 735.49875 W (the definition of the
   * metric horsepower); the table of the standard prints it rounded, 735.499
   * W. The standard has no international symbol.
   */ \
  X (metric_horsepower, mechanics, power, annex, non_si, \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_SYMBOL ("\xD0\xBB" ".\xD1\x81" "." /* U+043B . U+0441 . */), \
     LUMEX_UNITS_DIMENSION (2, 1, -3, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 588399, 8, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Poise (dynamic viscosity; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Pa} \cdot \text{s} = 0.1\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-1}$$
   */ \
  X (poise, mechanics, dynamic_viscosity, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("P"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9F" /* U+041F */), \
     LUMEX_UNITS_DIMENSION (-1, 1, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-1, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Stokes (kinematic viscosity; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{St} = 10^{-4}\ \text{m}^{2} \cdot \text{s}^{-1}$$
   */ \
  X (stokes, mechanics, kinematic_viscosity, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("St"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xA1" "\xD1\x82" /* U+0421 U+0442 */), \
     LUMEX_UNITS_DIMENSION (2, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-4, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Ohm square millimetre per metre (resistivity; old non-SI unit of annex G
   *    of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \Omega \cdot \text{mm}^{2} \cdot \text{m}^{-1} = 10^{-6}\ \text{kg} \cdot \text{m}^{3} \cdot \text{s}^{-3} \cdot \text{A}^{-2}$$
   *
   * 1e-6 ohm metre (= 1 ohm mm^2/m). The international symbol is an image in
   * the online text; the ohm is U+03A9.
   */ \
  X (ohm_square_millimetre_per_metre, electromagnetism, resistivity, annex, non_si, \
     LUMEX_UNITS_BARE_SYMBOL ("\xCE\xA9" "\xC2\xB7" "mm\xC2\xB2" "/m" /* U+03A9 GREEK CAPITAL LETTER OMEGA U+00B7 MIDDLE DOT mm U+00B2 SUPERSCRIPT TWO /m */), \
     LUMEX_UNITS_BARE_SYMBOL ("\xD0\x9E" "\xD0\xBC" "\xC2\xB7" "\xD0\xBC" "\xD0\xBC" "\xC2\xB2" "/\xD0\xBC" /* U+041E U+043C U+00B7 MIDDLE DOT U+043C U+043C U+00B2 SUPERSCRIPT TWO / U+043C */), \
     LUMEX_UNITS_DIMENSION (3, 1, -3, -2, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-6, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, true) \
  /* Maxwell (magnetic flux; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Mx} = 10^{-8}\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2} \cdot \text{A}^{-1}$$
   */ \
  X (maxwell, electromagnetism, magnetic_flux, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("Mx"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9C" "\xD0\xBA" "\xD1\x81" /* U+041C U+043A U+0441 */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, -1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-8, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Gauss (magnetic flux density; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Gs} = 10^{-4}\ \text{kg} \cdot \text{s}^{-2} \cdot \text{A}^{-1}$$
   *
   * The standard writes Gs, which is canonical; NIST lists Gs and G, and G is
   * accepted on input.
   */ \
  X (gauss, electromagnetism, magnetic_flux_density, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("Gs"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD1\x81" /* U+0413 U+0441 */), \
     LUMEX_UNITS_DIMENSION (0, 1, -2, -1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-4, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Gilbert (magnetomotive force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Gb} = \frac{10}{4 \pi}\ \text{A}$$
   *
   * 10 / (4 pi) A, contains pi; NIST writes Gi, which takes no prefix because
   * Gi is also the binary prefix gibi.
   */ \
  X (gilbert, electromagnetism, magnetomotive_force, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("Gb"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD0\xB1" /* U+0413 U+0431 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 5, 2, -1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Oersted (magnetic field strength; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Oe} = \frac{10^{3}}{4 \pi}\ \text{m}^{-1} \cdot \text{A}$$
   *
   * 10^3 / (4 pi) A/m, contains pi.
   */ \
  X (oersted, electromagnetism, magnetic_field_strength, annex, cgs, \
     LUMEX_UNITS_SYMBOL ("Oe"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xAD" /* U+042D */), \
     LUMEX_UNITS_DIMENSION (-1, 0, 0, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (1, 25, 1, -1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Calorie (international) (energy; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{cal} = 4.1868\ \text{kg} \cdot \text{m}^{2} \cdot \text{s}^{-2}$$
   *
   * The international table calorie, exact.
   */ \
  X (calorie, thermodynamics, energy, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("cal"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB0" "\xD0\xBB" /* U+043A U+0430 U+043B */), \
     LUMEX_UNITS_DIMENSION (2, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 10467, 25, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Rad (unit of absorbed dose) (absorbed dose; old non-SI unit of annex G of
   *    the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{rd} = 0.01\ \text{m}^{2} \cdot \text{s}^{-2}$$
   *
   * The standard writes rad, rd in the international column and rad in the
   * Russian one, but rad belongs to the radian: the dose has the symbol rd
   * only and no Russian symbol (its Russian name is the radian's symbol).
   */ \
  X (rad_dose, atomic_and_nuclear_physics, absorbed_dose, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("rd"), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (2, 0, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Rem (dose equivalent; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{rem} = 0.01\ \text{m}^{2} \cdot \text{s}^{-2}$$
   */ \
  X (rem, atomic_and_nuclear_physics, dose_equivalent, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("rem"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB1" "\xD1\x8D" "\xD1\x80" /* U+0431 U+044D U+0440 */), \
     LUMEX_UNITS_DIMENSION (2, 0, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Roentgen (exposure; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{R} = 2.58 \times 10^{-4}\ \text{kg}^{-1} \cdot \text{s} \cdot \text{A}$$
   */ \
  X (roentgen, atomic_and_nuclear_physics, exposure, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("R"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xA0" /* U+0420 */), \
     LUMEX_UNITS_DIMENSION (0, -1, 1, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-5, 129, 5, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Curie (activity; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{Ci} = 3.7 \times 10^{10}\ \text{s}^{-1}$$
   */ \
  X (curie, atomic_and_nuclear_physics, activity, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("Ci"), \
     LUMEX_UNITS_SYMBOL ("\xD0\x9A" "\xD0\xB8" /* U+041A U+0438 */), \
     LUMEX_UNITS_DIMENSION (0, 0, -1, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (9, 37, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_ALL_PREFIXES, false) \
  /* Micron (length; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \mu = 10^{-6}\ \text{m}$$
   *
   * U+00B5 MICRO SIGN is canonical (it survives Windows-1251 and Latin-1),
   * U+03BC GREEK SMALL LETTER MU is an alias.
   */ \
  X (micron, space_and_time, length, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("\xC2\xB5" /* U+00B5 MICRO SIGN */), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBA" /* U+043C U+043A */), \
     LUMEX_UNITS_DIMENSION (1, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (-6, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Revolution (turn) (plane angle; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{r} = 2 \pi$$
   *
   * 2 pi rad, contains pi.
   */ \
  X (revolution, space_and_time, plane_angle, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("r"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBE" "\xD0\xB1" /* U+043E U+0431 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 2, 1, 1), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Ampere-turn (magnetomotive force; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{At} = 1\ \text{A}$$
   */ \
  X (ampere_turn, electromagnetism, magnetomotive_force, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("At"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB0" "\xD0\xB2" /* U+0430 U+0432 */), \
     LUMEX_UNITS_DIMENSION (0, 0, 0, 1, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Nit (luminance; old non-SI unit of annex G of the GOST).
   *
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{nt} = 1\ \text{cd} \cdot \text{m}^{-2} = 1\ \text{m}^{-2} \cdot \text{cd}$$
   *
   * The symbol equals the generated symbol of the nanotonne: the unit wins.
   */ \
  X (nit, light_and_radiation, luminance, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("nt"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xBD" "\xD1\x82" /* U+043D U+0442 */), \
     LUMEX_UNITS_DIMENSION (-2, 0, 0, 0, 0, 0, 1), \
     LUMEX_UNITS_SCALE (0, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Are (area; old non-SI unit of annex G of the GOST).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (SI Brochure, 9th edition, version 4.01, Table 8 note (b).)
   * GOST 8.417-2024 (units of quantities), annex G, table G.1.
   *
   * $$1\ \text{a} = 100\ \text{m}^{2}$$
   */ \
  X (are, space_and_time, area, annex, non_si, \
     LUMEX_UNITS_SYMBOL ("a"), \
     LUMEX_UNITS_SYMBOL ("\xD0\xB0" /* U+0430 */), \
     LUMEX_UNITS_DIMENSION (2, 0, 0, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (2, 1, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false) \
  /* Standard atmosphere (pressure; unit defined by a CGPM resolution).
   *
   * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
   * (10th CGPM 1954, Resolution 4, reprinted in the SI Brochure V4.01, Appendix 1, p. 158.)
   * @see https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8
   * (NIST SP 811, Appendix B.8.)
   *
   * $$1\ \text{atm} = 101325\ \text{kg} \cdot \text{m}^{-1} \cdot \text{s}^{-2}$$
   *
   * 1 atm = 1 013 250 dyn/cm^2 = 101 325 Pa, exact (10th CGPM 1954 Resolution
   * 4). It is in neither table of the standard. No standard read gives a
   * Russian symbol (GOST 8.417-2024 table 7 and annex G, GOST 8.417-2002 annex
   * B, Cyrillic letter Ve); the usual abbreviation is an input alias only and
   * is never written.
   */ \
  X (standard_atmosphere, mechanics, pressure, cgpm, non_si, \
     LUMEX_UNITS_SYMBOL ("atm"), \
     LUMEX_UNITS_NO_SYMBOL, \
     LUMEX_UNITS_DIMENSION (-1, 1, -2, 0, 0, 0, 0), \
     LUMEX_UNITS_SCALE (0, 101325, 1, 0), \
     LUMEX_UNITS_NO_OFFSET, \
     exact, LUMEX_UNITS_NO_PREFIXES, false)
// clang-format on

// clang-format off
/**
 * @brief The other spellings that the lookup accepts, one
 * `X (unit, script, symbol)` row each: the compatibility forms that
 * Unicode folds into the canonical symbol (U+2126 for the ohm, U+212B for
 * the angstrom, U+212A for the kelvin, U+2103 for the degree Celsius), the
 * ASCII forms, the form of the other standard (au and ua, G and Gs) and
 * the variants of the compound symbols.
 */
#define LUMEX_UNITS_ALIASES(X) \
  X (kelvin, international, LUMEX_UNITS_SYMBOL ("\xE2\x84\xAA" /* U+212A KELVIN SIGN */)) \
  X (ohm, international, LUMEX_UNITS_SYMBOL ("\xE2\x84\xA6" /* U+2126 OHM SIGN */)) \
  X (ohm, international, LUMEX_UNITS_SYMBOL ("ohm")) \
  X (degree_celsius, international, LUMEX_UNITS_SYMBOL ("\xE2\x84\x83" /* U+2103 DEGREE CELSIUS */)) \
  X (degree_celsius, russian, LUMEX_UNITS_SYMBOL ("\xE2\x84\x83" /* U+2103 DEGREE CELSIUS */)) \
  X (dalton, international, LUMEX_UNITS_BARE_SYMBOL ("u")) \
  X (arcminute, international, LUMEX_UNITS_BARE_SYMBOL ("'")) \
  X (arcminute, russian, LUMEX_UNITS_BARE_SYMBOL ("'")) \
  X (arcsecond, international, LUMEX_UNITS_SYMBOL ("as")) \
  X (arcsecond, international, LUMEX_UNITS_BARE_SYMBOL ("\"")) \
  X (arcsecond, russian, LUMEX_UNITS_BARE_SYMBOL ("\"")) \
  X (litre, international, LUMEX_UNITS_SYMBOL ("l")) \
  X (astronomical_unit, international, LUMEX_UNITS_SYMBOL ("ua")) \
  X (kilowatt_hour, international, LUMEX_UNITS_BARE_SYMBOL ("kW\xE2\x8B\x85" "h" /* kW U+22C5 DOT OPERATOR h */)) \
  X (kilowatt_hour, russian, LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBA" "\xD0\x92" "\xD1\x82" "\xE2\x8B\x85" "\xD1\x87" /* U+043A U+0412 U+0442 U+22C5 DOT OPERATOR U+0447 */)) \
  X (volt_ampere, international, LUMEX_UNITS_SYMBOL ("V\xE2\x8B\x85" "A" /* V U+22C5 DOT OPERATOR A */)) \
  X (volt_ampere, russian, LUMEX_UNITS_SYMBOL ("\xD0\x92" "\xE2\x8B\x85" "\xD0\x90" /* U+0412 U+22C5 DOT OPERATOR U+0410 */)) \
  X (ampere_hour, international, LUMEX_UNITS_SYMBOL ("A\xE2\x8B\x85" "h" /* A U+22C5 DOT OPERATOR h */)) \
  X (ampere_hour, russian, LUMEX_UNITS_SYMBOL ("\xD0\x90" "\xE2\x8B\x85" "\xD1\x87" /* U+0410 U+22C5 DOT OPERATOR U+0447 */)) \
  X (ppm, russian, LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBB" "\xD0\xBD" "-1" /* U+043C U+043B U+043D -1 */)) \
  X (ppm, russian, LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBB" "\xD0\xBD" "^-1" /* U+043C U+043B U+043D ^-1 */)) \
  X (angstrom, international, LUMEX_UNITS_SYMBOL ("\xE2\x84\xAB" /* U+212B ANGSTROM SIGN */)) \
  X (angstrom, russian, LUMEX_UNITS_SYMBOL ("\xE2\x84\xAB" /* U+212B ANGSTROM SIGN */)) \
  X (millimetre_of_mercury, international, LUMEX_UNITS_SYMBOL ("mmHg")) \
  X (millimetre_of_mercury, russian, LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBC" ". \xD1\x80" "\xD1\x82" ". \xD1\x81" "\xD1\x82" /* U+043C U+043C .  U+0440 U+0442 .  U+0441 U+0442 */)) \
  X (square_degree, international, LUMEX_UNITS_BARE_SYMBOL ("(\xC2\xB0" ")\xC2\xB2" /* ( U+00B0 DEGREE SIGN ) U+00B2 SUPERSCRIPT TWO */)) \
  X (kilogram_force_per_square_centimetre, international, LUMEX_UNITS_BARE_SYMBOL ("kgf/cm2")) \
  X (kilogram_force_per_square_centimetre, russian, LUMEX_UNITS_BARE_SYMBOL ("\xD0\xBA" "\xD0\xB3" "\xD1\x81" "/\xD1\x81" "\xD0\xBC" "2" /* U+043A U+0433 U+0441 / U+0441 U+043C 2 */)) \
  X (kilopond_per_square_centimetre, international, LUMEX_UNITS_BARE_SYMBOL ("kp/cm2")) \
  X (millimetre_of_water, international, LUMEX_UNITS_BARE_SYMBOL ("mmH2O")) \
  X (millimetre_of_water, international, LUMEX_UNITS_BARE_SYMBOL ("mm H2O")) \
  X (torr, russian, LUMEX_UNITS_SYMBOL ("\xD1\x82" "\xD0\xBE" "\xD1\x80" "\xD1\x80" /* U+0442 U+043E U+0440 U+0440 */)) \
  X (kilopond_per_square_millimetre, international, LUMEX_UNITS_BARE_SYMBOL ("kp/mm2")) \
  X (ohm_square_millimetre_per_metre, international, LUMEX_UNITS_BARE_SYMBOL ("\xCE\xA9" "\xC2\xB7" "mm2/m" /* U+03A9 GREEK CAPITAL LETTER OMEGA U+00B7 MIDDLE DOT mm2/m */)) \
  X (ohm_square_millimetre_per_metre, russian, LUMEX_UNITS_BARE_SYMBOL ("\xD0\x9E" "\xD0\xBC" "\xC2\xB7" "\xD0\xBC" "\xD0\xBC" "2/\xD0\xBC" /* U+041E U+043C U+00B7 MIDDLE DOT U+043C U+043C 2/ U+043C */)) \
  X (gauss, international, LUMEX_UNITS_SYMBOL ("G")) \
  X (gilbert, international, LUMEX_UNITS_BARE_SYMBOL ("Gi")) \
  X (micron, international, LUMEX_UNITS_SYMBOL ("\xCE\xBC" /* U+03BC GREEK SMALL LETTER MU */)) \
  X (standard_atmosphere, russian, LUMEX_UNITS_SYMBOL ("\xD0\xB0" "\xD1\x82" "\xD0\xBC" /* U+0430 U+0442 U+043C */))
// clang-format on

// clang-format off
/**
 * @brief The 24 prefixes, one `X (id, exponent, international, russian)`
 * row each, in the order of `prefix_id`.
 * @see https://www.bipm.org/documents/20126/41483022/SI-Brochure-9-EN.pdf
 * (SI Brochure, 9th edition, version 4.01, Table 7; the 2022 prefixes ronna,
 * quetta, ronto and quecto are in the 27th CGPM, Resolution 3,
 * https://www.bipm.org/en/cgpm-2022/resolution-3)
 * GOST 8.417-2024 (units of quantities), table 8, which gives the Russian
 * symbols of the new prefixes.
 */
#define LUMEX_UNITS_PREFIXES(X) \
  X (quecto, -30, LUMEX_UNITS_SYMBOL ("q"), LUMEX_UNITS_SYMBOL ("\xD0\xBA" "\xD0\xB2" /* U+043A U+0432 */)) \
  X (ronto, -27, LUMEX_UNITS_SYMBOL ("r"), LUMEX_UNITS_SYMBOL ("\xD1\x80" "\xD0\xBD" /* U+0440 U+043D */)) \
  X (yocto, -24, LUMEX_UNITS_SYMBOL ("y"), LUMEX_UNITS_SYMBOL ("\xD0\xB8" /* U+0438 */)) \
  X (zepto, -21, LUMEX_UNITS_SYMBOL ("z"), LUMEX_UNITS_SYMBOL ("\xD0\xB7" /* U+0437 */)) \
  X (atto, -18, LUMEX_UNITS_SYMBOL ("a"), LUMEX_UNITS_SYMBOL ("\xD0\xB0" /* U+0430 */)) \
  X (femto, -15, LUMEX_UNITS_SYMBOL ("f"), LUMEX_UNITS_SYMBOL ("\xD1\x84" /* U+0444 */)) \
  X (pico, -12, LUMEX_UNITS_SYMBOL ("p"), LUMEX_UNITS_SYMBOL ("\xD0\xBF" /* U+043F */)) \
  X (nano, -9, LUMEX_UNITS_SYMBOL ("n"), LUMEX_UNITS_SYMBOL ("\xD0\xBD" /* U+043D */)) \
  X (micro, -6, LUMEX_UNITS_SYMBOL ("\xC2\xB5" /* U+00B5 MICRO SIGN */), LUMEX_UNITS_SYMBOL ("\xD0\xBC" "\xD0\xBA" /* U+043C U+043A */)) \
  X (milli, -3, LUMEX_UNITS_SYMBOL ("m"), LUMEX_UNITS_SYMBOL ("\xD0\xBC" /* U+043C */)) \
  X (centi, -2, LUMEX_UNITS_SYMBOL ("c"), LUMEX_UNITS_SYMBOL ("\xD1\x81" /* U+0441 */)) \
  X (deci, -1, LUMEX_UNITS_SYMBOL ("d"), LUMEX_UNITS_SYMBOL ("\xD0\xB4" /* U+0434 */)) \
  X (deca, 1, LUMEX_UNITS_SYMBOL ("da"), LUMEX_UNITS_SYMBOL ("\xD0\xB4" "\xD0\xB0" /* U+0434 U+0430 */)) \
  X (hecto, 2, LUMEX_UNITS_SYMBOL ("h"), LUMEX_UNITS_SYMBOL ("\xD0\xB3" /* U+0433 */)) \
  X (kilo, 3, LUMEX_UNITS_SYMBOL ("k"), LUMEX_UNITS_SYMBOL ("\xD0\xBA" /* U+043A */)) \
  X (mega, 6, LUMEX_UNITS_SYMBOL ("M"), LUMEX_UNITS_SYMBOL ("\xD0\x9C" /* U+041C */)) \
  X (giga, 9, LUMEX_UNITS_SYMBOL ("G"), LUMEX_UNITS_SYMBOL ("\xD0\x93" /* U+0413 */)) \
  X (tera, 12, LUMEX_UNITS_SYMBOL ("T"), LUMEX_UNITS_SYMBOL ("\xD0\xA2" /* U+0422 */)) \
  X (peta, 15, LUMEX_UNITS_SYMBOL ("P"), LUMEX_UNITS_SYMBOL ("\xD0\x9F" /* U+041F */)) \
  X (exa, 18, LUMEX_UNITS_SYMBOL ("E"), LUMEX_UNITS_SYMBOL ("\xD0\xAD" /* U+042D */)) \
  X (zetta, 21, LUMEX_UNITS_SYMBOL ("Z"), LUMEX_UNITS_SYMBOL ("\xD0\x97" /* U+0417 */)) \
  X (yotta, 24, LUMEX_UNITS_SYMBOL ("Y"), LUMEX_UNITS_SYMBOL ("\xD0\x98" /* U+0418 */)) \
  X (ronna, 27, LUMEX_UNITS_SYMBOL ("R"), LUMEX_UNITS_SYMBOL ("\xD0\xA0" "\xD0\xBD" /* U+0420 U+043D */)) \
  X (quetta, 30, LUMEX_UNITS_SYMBOL ("Q"), LUMEX_UNITS_SYMBOL ("\xD0\x9A" "\xD0\xB2" /* U+041A U+0432 */))
// clang-format on

// clang-format off
/**
 * @brief The other spellings of the prefixes that the lookup accepts: the
 * micro sign has two (U+03BC GREEK SMALL LETTER MU, which NFKC makes of
 * U+00B5, and the ASCII letter u).
 */
#define LUMEX_UNITS_PREFIX_ALIASES(X) \
  X (micro, international, LUMEX_UNITS_SYMBOL ("\xCE\xBC" /* U+03BC GREEK SMALL LETTER MU */)) \
  X (micro, international, LUMEX_UNITS_SYMBOL ("u"))
// clang-format on

// clang-format off
/**
 * @brief The generated symbols that the symbol of another unit takes
 * away: a bare symbol of a unit wins over the symbol that a prefix and
 * another unit make, and the lookup of the text finds the unit. Each
 * `X (script, prefix, unit, shadowed_by, symbol)` row names the pair that
 * cannot be reached by its symbol (it is reached by `unit_id` and
 * `prefix_id`) and the unit that has the symbol. A test recomputes the
 * list from the catalog and requires it to be equal to this one.
 */
#define LUMEX_UNITS_SHADOWED(X) \
  /* giga + barn is the symbol of gilbert */ \
  X (international, giga, barn, gilbert, LUMEX_UNITS_SYMBOL ("Gb")) \
  /* giga + second is the symbol of gauss */ \
  X (international, giga, second, gauss, LUMEX_UNITS_SYMBOL ("Gs")) \
  /* atto + second is the symbol of arcsecond */ \
  X (international, atto, second, arcsecond, LUMEX_UNITS_SYMBOL ("as")) \
  /* femto + tonne is the symbol of foot */ \
  X (international, femto, tonne, foot, LUMEX_UNITS_SYMBOL ("ft")) \
  /* nano + tonne is the symbol of nit */ \
  X (international, nano, tonne, nit, LUMEX_UNITS_SYMBOL ("nt")) \
  /* giga + barn is the symbol of gilbert */ \
  X (russian, giga, barn, gilbert, LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD0\xB1" /* U+0413 U+0431 */)) \
  /* giga + second is the symbol of gauss */ \
  X (russian, giga, second, gauss, LUMEX_UNITS_SYMBOL ("\xD0\x93" "\xD1\x81" /* U+0413 U+0441 */)) \
  /* tera + litre is the symbol of tesla */ \
  X (russian, tera, litre, tesla, LUMEX_UNITS_SYMBOL ("\xD0\xA2" "\xD0\xBB" /* U+0422 U+043B */)) \
  /* hecto + radian is the symbol of gon */ \
  X (russian, hecto, radian, gon, LUMEX_UNITS_SYMBOL ("\xD0\xB3" "\xD1\x80" "\xD0\xB0" "\xD0\xB4" /* U+0433 U+0440 U+0430 U+0434 */)) \
  /* hecto + second is the symbol of gram_force */ \
  X (russian, hecto, second, gram_force, LUMEX_UNITS_SYMBOL ("\xD0\xB3" "\xD1\x81" /* U+0433 U+0441 */)) \
  /* nano + tonne is the symbol of nit */ \
  X (russian, nano, tonne, nit, LUMEX_UNITS_SYMBOL ("\xD0\xBD" "\xD1\x82" /* U+043D U+0442 */)) \
  /* femto + tonne is the symbol of foot */ \
  X (russian, femto, tonne, foot, LUMEX_UNITS_SYMBOL ("\xD1\x84" "\xD1\x82" /* U+0444 U+0442 */))
// clang-format on

/**
 * @brief The dimension of a unit: the exponents of the seven base
 * quantities in the order of `base_quantity`.
 * @tparam Exponent The type of an exponent.
 */
template <class Exponent = std::int8_t> struct basic_dimension_t
{
  using exponent_type = Exponent;

  exponent_type exponent[7];
};

/** @brief The default dimension. */
using dimension_t = basic_dimension_t<>;

/**
 * @brief The value of a unit in the coherent SI unit of its dimension (the
 * kilogram for mass), as
 * $$10^{k} \cdot \frac{p}{q} \cdot \pi^{n}$$
 * with the decimal exponent $$k$$, the numerator $$p$$, the denominator $$q$$
 * and the power $$n$$ of the number pi.
 * @details The canonical form has $$q > 0$$, $$p$$ and $$q$$ without a common
 * divisor and without a factor 10, so that the same value has one
 * representation. A zero value (numerator 0, denominator 1) means that there
 * is no value (factor kind `none`) or, for an offset, that there is none.
 * @tparam Decimal The type of the decimal exponent.
 * @tparam Integer The type of the numerator and the denominator.
 * @tparam PiPower The type of the power of pi.
 */
template <class Decimal = std::int32_t, class Integer = std::int64_t,
          class PiPower = std::int8_t>
struct basic_scale_t
{
  using decimal_type = Decimal;
  using integer_type = Integer;
  using pi_power_type = PiPower;

  decimal_type exponent10;
  integer_type numerator;
  integer_type denominator;
  pi_power_type pi_power;
};

/** @brief The default scale. */
using scale_t = basic_scale_t<>;

/**
 * @brief A symbol of a unit or of a prefix in one script.
 * @details `text` is UTF-8 and has `size` bytes; an empty symbol has the size
 * 0. `prefixable` says that the symbol takes the prefixes that its unit
 * allows (a symbol such as the dalton alias `u`, the arcsecond sign or
 * `kW.h` does not).
 */
struct symbol_t
{
  char const *text;
  std::size_t size;
  bool prefixable;
};

/** @brief A prefix: its exponent, its two symbols and its name. */
struct prefix_descriptor_t
{
  prefix_id id;
  std::int8_t exponent;
  symbol_t international;
  symbol_t russian;
  char const *name;
};

#define LUMEX_UNITS_ROW_ENUM(ID, DOMAIN, KIND, STANDING, SYSTEM, INTL, RU,    \
                             DIMENSION, SCALE, OFFSET, FACTOR, PREFIXES,      \
                             COMPOUND)                                        \
  ID,

/**
 * @brief The units of the catalog, one enumerator per `LUMEX_UNITS_CATALOG`
 * row, in the order of the rows; the value is the index in the descriptor
 * array.
 */
enum class unit_id : std::uint16_t
{
  LUMEX_UNITS_CATALOG (LUMEX_UNITS_ROW_ENUM)
};

/**
 * @brief Everything the catalog knows about one unit.
 * @details `offset` is the zero of an affine unit in the coherent SI unit
 * (273.15 for the degree Celsius, so that a point in kelvin is the point in
 * the unit times `scale` plus `offset`); it is the zero scale for every other
 * unit. `prefixes` is the set of the prefixes that the unit takes, one bit per
 * `prefix_id`.
 */
struct unit_descriptor_t
{
  unit_id id;
  domain unit_domain;
  quantity_kind kind;
  standing unit_standing;
  unit_system system;
  char const *name;
  symbol_t international;
  symbol_t russian;
  dimension_t dimension;
  scale_t scale;
  scale_t offset;
  factor_kind factor;
  std::uint32_t prefixes;
  bool compound;
};

/** @brief Another spelling of a unit that the lookup accepts. */
struct alias_descriptor_t
{
  unit_id unit;
  script symbol_script;
  symbol_t symbol;
};

/** @brief Another spelling of a prefix that the lookup accepts. */
struct prefix_alias_descriptor_t
{
  prefix_id prefix;
  script symbol_script;
  symbol_t symbol;
};

/**
 * @brief A generated symbol that the symbol of another unit takes away: the
 * lookup of `symbol` in `symbol_script` finds `shadowed_by`, not the pair
 * (`prefix`, `unit`).
 */
struct shadow_descriptor_t
{
  script symbol_script;
  prefix_id prefix;
  unit_id unit;
  unit_id shadowed_by;
  symbol_t symbol;
};

/** @brief The prefix mask of every prefix. */
constexpr std::uint32_t all_prefixes_mask = 0xFFFFFFu;

/** @brief The bit of a prefix in a prefix mask (0 for `prefix_id::none` and
 * for a value outside the 24 prefixes). */
constexpr std::uint32_t
prefix_bit (prefix_id id) noexcept
{
  return static_cast<unsigned> (id) < 24u
             ? static_cast<std::uint32_t> (1u << static_cast<unsigned> (id))
             : 0u;
}

/** @brief A dimension from the seven exponents, in the order of
 * `base_quantity`. */
constexpr dimension_t
make_dimension (std::int8_t length, std::int8_t mass, std::int8_t time,
                std::int8_t electric_current,
                std::int8_t thermodynamic_temperature,
                std::int8_t amount_of_substance,
                std::int8_t luminous_intensity) noexcept
{
  return dimension_t{ { length, mass, time, electric_current,
                        thermodynamic_temperature, amount_of_substance,
                        luminous_intensity } };
}

/** @brief A scale from its four parts. */
constexpr scale_t
make_scale (std::int32_t exponent10, std::int64_t numerator,
            std::int64_t denominator, std::int8_t pi_power) noexcept
{
  return scale_t{ exponent10, numerator, denominator, pi_power };
}

/** @brief A symbol from a string literal, its size and its prefixable flag. */
constexpr symbol_t
make_symbol (char const *text, std::size_t size, bool prefixable) noexcept
{
  return symbol_t{ text, size, prefixable };
}

/** @brief The greatest common divisor of two non-negative integers. */
constexpr std::int64_t
greatest_common_divisor (std::int64_t first, std::int64_t second) noexcept
{
  return second == 0 ? first
                     : greatest_common_divisor (second, first % second);
}

/**
 * @brief Whether a scale is in the canonical form: for a unit with a value
 * $$q > 0$$, $$p > 0$$, $$p$$ and $$q$$ coprime and neither divisible by 10,
 * $$p$$ coprime to 10 for a negative $$k$$ and $$q$$ coprime to 10 for a
 * positive one (so that one value has one representation);
 * for a unit without one (`factor_kind::none`) all parts zero but the
 * denominator 1.
 */
constexpr bool
is_canonical_scale (scale_t const &scale, factor_kind factor) noexcept
{
  return factor == factor_kind::none
             ? (scale.exponent10 == 0 && scale.numerator == 0
                && scale.denominator == 1 && scale.pi_power == 0)
             : (scale.numerator > 0 && scale.denominator > 0
                && greatest_common_divisor (scale.numerator, scale.denominator)
                       == 1
                && scale.numerator % 10 != 0 && scale.denominator % 10 != 0
                && (scale.exponent10 >= 0
                    || (scale.numerator % 2 != 0 && scale.numerator % 5 != 0))
                && (scale.exponent10 <= 0
                    || (scale.denominator % 2 != 0
                        && scale.denominator % 5 != 0)));
}

/** @brief Whether an offset is absent (zero) or in the canonical form. */
constexpr bool
is_canonical_offset (scale_t const &offset) noexcept
{
  return offset.numerator == 0
             ? (offset.exponent10 == 0 && offset.denominator == 1
                && offset.pi_power == 0)
             : is_canonical_scale (offset, factor_kind::exact);
}

/** @brief Whether a symbol is not empty. */
constexpr bool
has_symbol (symbol_t const &symbol) noexcept
{
  return symbol.size != 0;
}

#define LUMEX_UNITS_ROW_COUNT(ID, DOMAIN, KIND, STANDING, SYSTEM, INTL, RU,   \
                              DIMENSION, SCALE, OFFSET, FACTOR, PREFIXES,     \
                              COMPOUND)                                       \
  +1
#define LUMEX_UNITS_ROW_DESCRIPTOR(ID, DOMAIN, KIND, STANDING, SYSTEM, INTL,  \
                                   RU, DIMENSION, SCALE, OFFSET, FACTOR,      \
                                   PREFIXES, COMPOUND)                        \
  { unit_id::ID,                                                              \
    domain::DOMAIN,                                                           \
    quantity_kind::KIND,                                                      \
    standing::STANDING,                                                       \
    unit_system::SYSTEM,                                                      \
    #ID,                                                                      \
    INTL,                                                                     \
    RU,                                                                       \
    DIMENSION,                                                                \
    SCALE,                                                                    \
    OFFSET,                                                                   \
    factor_kind::FACTOR,                                                      \
    PREFIXES,                                                                 \
    COMPOUND },
#define LUMEX_UNITS_ROW_CHECK(ID, DOMAIN, KIND, STANDING, SYSTEM, INTL, RU,   \
                              DIMENSION, SCALE, OFFSET, FACTOR, PREFIXES,     \
                              COMPOUND)                                       \
  static_assert (                                                             \
      is_canonical_scale (SCALE, factor_kind::FACTOR),                        \
      "unit " #ID ": the scale is not canonical (it needs q > 0, "            \
      "p and q coprime, no factor 10 left in p or q, p coprime to "           \
      "10 for k < 0, q coprime to 10 for k > 0, and the "                     \
      "zero scale for a unit without a value)");                              \
  static_assert (is_canonical_offset (OFFSET),                                \
                 "unit " #ID ": the offset is not canonical (zero, or the "   \
                 "canonical form of a scale)");                               \
  static_assert (has_symbol (INTL) || has_symbol (RU),                        \
                 "unit " #ID ": it has no symbol in either script (the "      \
                 "standards give every unit a symbol in at least one)");      \
  static_assert ((PREFIXES & ~all_prefixes_mask) == 0,                        \
                 "unit " #ID ": the prefix set has a bit above the 24th");

#define LUMEX_UNITS_ALIAS_ROW_ENUM(UNIT, SCRIPT, SYMBOL) +1
#define LUMEX_UNITS_ALIAS_ROW_DESCRIPTOR(UNIT, SCRIPT, SYMBOL)                \
  { unit_id::UNIT, script::SCRIPT, SYMBOL },
#define LUMEX_UNITS_PREFIX_ROW_DESCRIPTOR(ID, EXPONENT, INTL, RU)             \
  { prefix_id::ID, EXPONENT, INTL, RU, #ID },
#define LUMEX_UNITS_PREFIX_ALIAS_ROW_COUNT(PREFIX, SCRIPT, SYMBOL) +1
#define LUMEX_UNITS_PREFIX_ALIAS_ROW_DESCRIPTOR(PREFIX, SCRIPT, SYMBOL)       \
  { prefix_id::PREFIX, script::SCRIPT, SYMBOL },
#define LUMEX_UNITS_SHADOW_ROW_COUNT(SCRIPT, PREFIX, UNIT, SHADOWED_BY,       \
                                     SYMBOL)                                  \
  +1
#define LUMEX_UNITS_SHADOW_ROW_DESCRIPTOR(SCRIPT, PREFIX, UNIT, SHADOWED_BY,  \
                                          SYMBOL)                             \
  { script::SCRIPT, prefix_id::PREFIX, unit_id::UNIT, unit_id::SHADOWED_BY,   \
    SYMBOL },

/** @brief The number of units of the catalog. */
constexpr std::size_t unit_count
    = 0 LUMEX_UNITS_CATALOG (LUMEX_UNITS_ROW_COUNT);

/** @brief The number of other spellings of units. */
constexpr std::size_t alias_count
    = 0 LUMEX_UNITS_ALIASES (LUMEX_UNITS_ALIAS_ROW_ENUM);

/** @brief The number of other spellings of prefixes. */
constexpr std::size_t prefix_alias_count
    = 0 LUMEX_UNITS_PREFIX_ALIASES (LUMEX_UNITS_PREFIX_ALIAS_ROW_COUNT);

/** @brief The number of generated symbols that another unit's symbol takes. */
constexpr std::size_t shadow_count
    = 0 LUMEX_UNITS_SHADOWED (LUMEX_UNITS_SHADOW_ROW_COUNT);

LUMEX_UNITS_CATALOG (LUMEX_UNITS_ROW_CHECK)

/**
 * @brief The storage of the tables: static constexpr arrays of a class
 * template, which a header may define in every translation unit (before C++17
 * a static data member of a plain class needs one definition in one file).
 * @tparam Tag Unused; it makes the class a template.
 */
template <class Tag = void> struct catalog_storage_t
{
  static constexpr unit_descriptor_t units[unit_count]
      = { LUMEX_UNITS_CATALOG (LUMEX_UNITS_ROW_DESCRIPTOR) };
  static constexpr alias_descriptor_t aliases[alias_count]
      = { LUMEX_UNITS_ALIASES (LUMEX_UNITS_ALIAS_ROW_DESCRIPTOR) };
  static constexpr prefix_descriptor_t prefixes[prefix_count]
      = { LUMEX_UNITS_PREFIXES (LUMEX_UNITS_PREFIX_ROW_DESCRIPTOR) };
  static constexpr prefix_alias_descriptor_t prefix_aliases[prefix_alias_count]
      = { LUMEX_UNITS_PREFIX_ALIASES (
          LUMEX_UNITS_PREFIX_ALIAS_ROW_DESCRIPTOR) };
  static constexpr shadow_descriptor_t shadows[shadow_count]
      = { LUMEX_UNITS_SHADOWED (LUMEX_UNITS_SHADOW_ROW_DESCRIPTOR) };
};

#if !defined(__cpp_inline_variables)
template <class Tag>
constexpr unit_descriptor_t catalog_storage_t<Tag>::units[unit_count];
template <class Tag>
constexpr alias_descriptor_t catalog_storage_t<Tag>::aliases[alias_count];
template <class Tag>
constexpr prefix_descriptor_t catalog_storage_t<Tag>::prefixes[prefix_count];
template <class Tag>
constexpr prefix_alias_descriptor_t
    catalog_storage_t<Tag>::prefix_aliases[prefix_alias_count];
template <class Tag>
constexpr shadow_descriptor_t catalog_storage_t<Tag>::shadows[shadow_count];
#endif

/** @brief The unit with the given index (the value of its `unit_id`). */
constexpr unit_descriptor_t const &
unit_at (std::size_t index) noexcept
{
  return catalog_storage_t<>::units[index];
}

/** @brief The alias with the given index. */
constexpr alias_descriptor_t const &
alias_at (std::size_t index) noexcept
{
  return catalog_storage_t<>::aliases[index];
}

/** @brief The prefix with the given index (the value of its `prefix_id`). */
constexpr prefix_descriptor_t const &
prefix_at (std::size_t index) noexcept
{
  return catalog_storage_t<>::prefixes[index];
}

/** @brief The prefix alias with the given index. */
constexpr prefix_alias_descriptor_t const &
prefix_alias_at (std::size_t index) noexcept
{
  return catalog_storage_t<>::prefix_aliases[index];
}

/** @brief The shadowed symbol with the given index. */
constexpr shadow_descriptor_t const &
shadow_at (std::size_t index) noexcept
{
  return catalog_storage_t<>::shadows[index];
}

/** @brief The edition of the SI Brochure that the international column
 * follows. */
constexpr char const *
si_brochure_edition () noexcept
{
  return "SI Brochure, 9th edition 2019, version 4.01 (June 2026)";
}

/** @brief The standard that the Russian column and the units of annex G
 * follow. */
constexpr char const *
gost_edition () noexcept
{
  return "GOST 8.417-2024";
}

/** @brief The adjustment of the fundamental constants of the measured values.
 */
constexpr char const *
codata_edition () noexcept
{
  return "CODATA 2022";
}

#undef LUMEX_UNITS_ROW_ENUM
#undef LUMEX_UNITS_ROW_COUNT
#undef LUMEX_UNITS_ROW_DESCRIPTOR
#undef LUMEX_UNITS_ROW_CHECK
#undef LUMEX_UNITS_ALIAS_ROW_ENUM
#undef LUMEX_UNITS_ALIAS_ROW_DESCRIPTOR
#undef LUMEX_UNITS_PREFIX_ROW_DESCRIPTOR
#undef LUMEX_UNITS_PREFIX_ALIAS_ROW_COUNT
#undef LUMEX_UNITS_PREFIX_ALIAS_ROW_DESCRIPTOR
#undef LUMEX_UNITS_SHADOW_ROW_COUNT
#undef LUMEX_UNITS_SHADOW_ROW_DESCRIPTOR
} // namespace units
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // LUMEX_CORE_UNITS_DATA_HPP

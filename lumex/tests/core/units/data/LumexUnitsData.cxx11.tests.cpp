// lumex/tests/core/units/data/LumexUnitsData.cxx11.tests.cpp
//
// The catalog as a table: evaluated in constant expressions at C++11, the
// shape of every row (index, name, canonical scale, valid UTF-8, one script
// per symbol, no symbol shared by two units of a script) and the prefix table.
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/LumexUnitsTestSupport.hpp"

using namespace lumex_test_units;

// --- constant expressions at C++11 -----------------------------------------

static_assert (unit_count == 105, "the catalog has 105 units");
static_assert (prefix_count == 24, "the SI has 24 prefixes");
static_assert (descriptor (unit_id::pascal_unit).dimension.exponent[0] == -1,
               "pascal: kg m^-1 s^-2, metre exponent");
static_assert (descriptor (unit_id::pascal_unit).dimension.exponent[1] == 1,
               "pascal: mass exponent");
static_assert (descriptor (unit_id::pascal_unit).dimension.exponent[2] == -2,
               "pascal: time exponent");
static_assert (descriptor (unit_id::pascal_unit).scale.numerator == 1
                   && descriptor (unit_id::pascal_unit).scale.denominator == 1
                   && descriptor (unit_id::pascal_unit).scale.exponent10 == 0,
               "pascal is the coherent unit");
static_assert (descriptor (unit_id::gram).scale.exponent10 == -3,
               "the gram is 10^-3 of the kilogram");
static_assert (descriptor (unit_id::electronvolt).scale.exponent10 == -27
                   && descriptor (unit_id::electronvolt).scale.numerator
                          == 801088317
                   && descriptor (unit_id::electronvolt).scale.denominator
                          == 5,
               "1 eV = 1.602176634e-19 J = 801088317 / 5 * 10^-27 J");
static_assert (
    descriptor (unit_id::millimetre_of_mercury).scale.exponent10 == -8
        && descriptor (unit_id::millimetre_of_mercury).scale.numerator
               == 26664477483
        && descriptor (unit_id::millimetre_of_mercury).scale.denominator == 2,
    "1 mmHg = 133.322387415 Pa = 26664477483 / 2 * 10^-8 Pa");
static_assert (descriptor (unit_id::torr).scale.exponent10 == 0
                   && descriptor (unit_id::torr).scale.numerator == 20265
                   && descriptor (unit_id::torr).scale.denominator == 152,
               "1 Torr = 101325 / 760 Pa = 20265 / 152 Pa");
static_assert (descriptor (unit_id::calorie).scale.exponent10 == -2
                   && descriptor (unit_id::calorie).scale.numerator == 10467
                   && descriptor (unit_id::calorie).scale.denominator == 25,
               "1 cal = 4.1868 J = 10467 / 25 * 10^-2 J");
static_assert (descriptor (unit_id::light_year).scale.exponent10 == 2
                   && descriptor (unit_id::light_year).scale.numerator
                          == 94607304725808,
               "1 ly = 9 460 730 472 580 800 m");
static_assert (descriptor (unit_id::hour).scale.exponent10 == 2
                   && descriptor (unit_id::hour).scale.numerator == 36,
               "1 h = 3600 s = 36 * 10^2 s");
static_assert (descriptor (unit_id::degree).scale.pi_power == 1
                   && descriptor (unit_id::degree).scale.denominator == 18,
               "1 degree = pi / 180 rad");
static_assert (descriptor (unit_id::degree_celsius).offset.numerator == 5463
                   && descriptor (unit_id::degree_celsius).offset.denominator
                          == 2
                   && descriptor (unit_id::degree_celsius).offset.exponent10
                          == -1,
               "the zero of the Celsius scale is 273.15 K = 5463 / 2 * 10^-1");
static_assert (descriptor (unit_id::decibel).factor == factor_kind::none,
               "a logarithmic unit has no value");
static_assert (descriptor (unit_id::dalton).factor == factor_kind::measured,
               "the dalton is measured");
static_assert (descriptor (unit_id::ohm).international.size == 2,
               "the ohm sign is two bytes of UTF-8");
static_assert (descriptor (unit_id::ohm).russian.size == 4,
               "Om is two Cyrillic letters, four bytes");
static_assert (descriptor (unit_id::metre).international.size == 1
                   && descriptor (unit_id::metre).international.text[0] == 'm',
               "the symbol of the metre is the letter m");
static_assert (descriptor (prefix_id::micro).exponent == -6, "micro is 10^-6");
static_assert (descriptor (prefix_id::quetta).exponent == 30,
               "quetta is 10^30");
static_assert (descriptor (prefix_id::quecto).exponent == -30,
               "quecto is 10^-30");
static_assert (prefix_bit (prefix_id::quetta) == 0x800000u,
               "quetta is the 24th bit");
static_assert (descriptor (unit_id::minute).prefixes == 0u,
               "the minute takes no prefixes");
static_assert (descriptor (unit_id::metre).prefixes == all_prefixes_mask,
               "the metre takes every prefix");
static_assert (descriptor (unit_id::kilogram_force).kind
                   == quantity_kind::force,
               "the kilogram-force is a force");
static_assert (is_canonical_scale (make_scale (-8, 26664477483, 2, 0),
                                   factor_kind::exact),
               "a canonical scale is accepted");
static_assert (!is_canonical_scale (make_scale (0, 10, 1, 0),
                                    factor_kind::exact),
               "a factor 10 left in p is refused");
static_assert (!is_canonical_scale (make_scale (0, 2, 4, 0),
                                    factor_kind::exact),
               "a common divisor is refused");
static_assert (!is_canonical_scale (make_scale (0, 1, 0, 0),
                                    factor_kind::exact),
               "a zero denominator is refused");
static_assert (is_canonical_scale (make_scale (0, 0, 1, 0), factor_kind::none),
               "the zero scale belongs to a unit without a value");
static_assert (!is_canonical_scale (make_scale (0, 1, 1, 0),
                                    factor_kind::none),
               "a unit without a value has no scale");

static_assert (!is_canonical_scale (make_scale (2, 196133, 2, 0),
                                    factor_kind::exact),
               "a denominator with the factor 2 under a positive exponent is "
               "the same value as (1, 980665, 1)");
static_assert (!is_canonical_scale (make_scale (-4, 980665, 1, 0),
                                    factor_kind::exact),
               "a numerator with the factor 5 under a negative exponent is "
               "the same value as (-4, 196133, 2)");
static_assert (is_canonical_scale (make_scale (1, 980665, 1, 0),
                                   factor_kind::exact),
               "980665 * 10 is canonical");
static_assert (prefix_bit (prefix_id::none) == 0u
                   && prefix_bit (static_cast<prefix_id> (99)) == 0u,
               "no bit for no prefix or a value outside the 24");
static_assert (
    descriptor (unit_id::degree_celsius).prefixes == all_prefixes_mask
        && descriptor (unit_id::gon).prefixes == all_prefixes_mask
        && descriptor (unit_id::oersted).prefixes == all_prefixes_mask,
    "the degree Celsius, the gon and the oersted take every prefix");
static_assert (!descriptor (unit_id::arcsecond).russian.prefixable,
               "the Russian arcsecond sign takes no prefix");
static_assert (descriptor (unit_id::metric_horsepower).factor
                       == factor_kind::exact
                   && descriptor (unit_id::parsec).scale.pi_power == -1,
               "the metric horsepower is a definition, the parsec has 1/pi");

// --- the rows ---------------------------------------------------------------

namespace
{
std::vector<std::uint32_t>
decode_utf8 (std::string const &text, bool &valid)
{
  std::vector<std::uint32_t> points;
  valid = true;
  std::size_t index = 0;
  while (index < text.size ())
    {
      unsigned char const lead = static_cast<unsigned char> (text[index]);
      std::size_t length = 1;
      std::uint32_t point = lead;
      if (lead >= 0xF0 && lead <= 0xF4)
        {
          length = 4;
          point = lead & 0x07u;
        }
      else if (lead >= 0xE0 && lead <= 0xEF)
        {
          length = 3;
          point = lead & 0x0Fu;
        }
      else if (lead >= 0xC2 && lead <= 0xDF)
        {
          length = 2;
          point = lead & 0x1Fu;
        }
      else if (lead >= 0x80)
        {
          valid = false;
          return points;
        }
      if (index + length > text.size ())
        {
          valid = false;
          return points;
        }
      for (std::size_t more = 1; more < length; ++more)
        {
          unsigned char const next
              = static_cast<unsigned char> (text[index + more]);
          if ((next & 0xC0u) != 0x80u)
            {
              valid = false;
              return points;
            }
          point = (point << 6) | (next & 0x3Fu);
        }
      points.push_back (point);
      index += length;
    }
  return points;
}

bool
is_cyrillic (std::uint32_t point)
{
  return point >= 0x400 && point <= 0x4FF;
}

bool
is_ascii_letter (std::uint32_t point)
{
  return (point >= 'A' && point <= 'Z') || (point >= 'a' && point <= 'z');
}

std::vector<std::string>
all_symbol_texts (unit_id id, script in)
{
  std::vector<std::string> texts;
  for (unit_symbol_t const &symbol : symbols_of (id, in))
    texts.push_back (symbol.text);
  return texts;
}
} // namespace

TEST (LumexUnitsDataTest,
      GivenEveryUnit_WhenIndexed_ThenEnumeratorIsTheIndexAndNameIsSpelled)
{
  std::set<std::string> names;
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      EXPECT_EQ (index, static_cast<std::size_t> (unit.id)) << unit.name;
      EXPECT_TRUE (names.insert (unit.name).second) << unit.name;
      EXPECT_NE (std::string (), std::string (unit.name));
    }
  EXPECT_EQ (unit_count, names.size ());
}

TEST (LumexUnitsDataTest, GivenEveryScale_WhenChecked_ThenItIsCanonical)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      SCOPED_TRACE (unit.name);
      if (unit.factor == factor_kind::none)
        {
          EXPECT_EQ (0, unit.scale.numerator);
          EXPECT_EQ (1, unit.scale.denominator);
          EXPECT_EQ (0, unit.scale.exponent10);
          EXPECT_EQ (0, unit.scale.pi_power);
          continue;
        }
      EXPECT_GT (unit.scale.numerator, 0);
      EXPECT_GT (unit.scale.denominator, 0);
      EXPECT_EQ (1, greatest_common_divisor (unit.scale.numerator,
                                             unit.scale.denominator));
      EXPECT_NE (0, unit.scale.numerator % 10);
      EXPECT_NE (0, unit.scale.denominator % 10);
      EXPECT_TRUE (is_canonical_offset (unit.offset));
    }
}

TEST (
    LumexUnitsDataTest,
    GivenEveryUnit_WhenSymbolsDecoded_ThenTheyAreValidUtf8WithoutControlCharacters)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const id = unit_id_at (index);
      for (script in : { script::international, script::russian })
        {
          for (std::string const &text : all_symbol_texts (id, in))
            {
              SCOPED_TRACE (std::string (descriptor (id).name) + " " + text);
              bool valid = false;
              std::vector<std::uint32_t> const points
                  = decode_utf8 (text, valid);
              EXPECT_TRUE (valid);
              EXPECT_FALSE (points.empty ());
              for (std::uint32_t point : points)
                EXPECT_GE (point, 0x20u);
            }
        }
    }
}

TEST (LumexUnitsDataTest,
      GivenEveryUnit_WhenInEitherScript_ThenAtLeastOneCanonicalSymbolExists)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      EXPECT_TRUE (unit.international.size != 0 || unit.russian.size != 0)
          << unit.name;
    }
}

TEST (LumexUnitsDataTest,
      GivenInternationalSymbols_WhenDecoded_ThenNoCyrillicLetterAppears)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const id = unit_id_at (index);
      for (std::string const &text :
           all_symbol_texts (id, script::international))
        {
          bool valid = false;
          for (std::uint32_t point : decode_utf8 (text, valid))
            EXPECT_FALSE (is_cyrillic (point))
                << descriptor (id).name << " " << text;
        }
    }
}

TEST (
    LumexUnitsDataTest,
    GivenRussianSymbols_WhenDecoded_ThenNoLatinLetterAppearsOutsideTheStandardsForms)
{
  // The standard writes a Latin letter in its Russian column only in the
  // degree Celsius (the Latin C) and in the angstrom (U+00C5, a Latin letter
  // with a ring).
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const id = unit_id_at (index);
      for (std::string const &text : all_symbol_texts (id, script::russian))
        {
          bool valid = false;
          for (std::uint32_t point : decode_utf8 (text, valid))
            {
              if (is_ascii_letter (point))
                {
                  EXPECT_TRUE (id == unit_id::degree_celsius && (point == 'C'))
                      << descriptor (id).name << " " << text;
                }
            }
        }
    }
}

TEST (LumexUnitsDataTest,
      GivenBareSymbols_WhenCollectedPerScript_ThenNoTwoUnitsShareOne)
{
  for (script in : { script::international, script::russian })
    {
      std::map<std::string, unit_id> owner;
      for (std::size_t index = 0; index < unit_count; ++index)
        {
          unit_id const id = unit_id_at (index);
          for (std::string const &text : all_symbol_texts (id, in))
            {
              std::map<std::string, unit_id>::const_iterator const found
                  = owner.find (text);
              if (found != owner.end ())
                EXPECT_EQ (found->second, id)
                    << script_name (in) << " " << text << " is the symbol of "
                    << descriptor (found->second).name << " and "
                    << descriptor (id).name;
              owner[text] = id;
            }
        }
    }
}

TEST (LumexUnitsDataTest,
      GivenPrefixTable_WhenRead_ThenIndexExponentAndNameAgree)
{
  std::int8_t const exponents[24]
      = { -30, -27, -24, -21, -18, -15, -12, -9, -6, -3, -2, -1,
          1,   2,   3,   6,   9,   12,  15,  18, 21, 24, 27, 30 };
  for (std::size_t index = 0; index < prefix_count; ++index)
    {
      prefix_descriptor_t const &prefix = prefix_at (index);
      EXPECT_EQ (index, static_cast<std::size_t> (prefix.id));
      EXPECT_EQ (exponents[index], prefix.exponent) << prefix.name;
      EXPECT_STREQ (prefix_names[index], prefix.name);
      EXPECT_NE (0u, prefix.international.size);
      EXPECT_NE (0u, prefix.russian.size);
    }
}

TEST (LumexUnitsDataTest, GivenEditions_WhenRead_ThenTheyNameTheSources)
{
  EXPECT_NE (std::string::npos,
             std::string (si_brochure_edition ()).find ("4.01"));
  EXPECT_STREQ ("GOST 8.417-2024", gost_edition ());
  EXPECT_STREQ ("CODATA 2022", codata_edition ());
}

TEST (LumexUnitsDataTest,
      GivenUnitsWithoutPrefixes_WhenListed_ThenTheStandardsOnesAreAmongThem)
{
  // GOST 8.417-2024, table 5 note 2: no prefixes on the units of time, the
  // units of plane angle, the astronomical unit, the diopter and the atomic
  // mass unit (the Russian symbol).
  unit_id const none[]
      = { unit_id::minute, unit_id::hour,      unit_id::day,
          unit_id::degree, unit_id::arcminute, unit_id::astronomical_unit,
          unit_id::diopter };
  for (unit_id id : none)
    EXPECT_EQ (0u, descriptor (id).prefixes) << descriptor (id).name;
  // The dalton: Da takes prefixes, the Russian symbol and u do not.
  EXPECT_TRUE (descriptor (unit_id::dalton).international.prefixable);
  EXPECT_FALSE (descriptor (unit_id::dalton).russian.prefixable);
  for (unit_symbol_t const &symbol :
       symbols_of (unit_id::dalton, script::international))
    {
      if (symbol.text == "u")
        EXPECT_FALSE (symbol.prefixable);
    }
}

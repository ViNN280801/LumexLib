// lumex/tests/core/units/api/LumexUnitsLookup.cxx11.tests.cpp
//
// The lookup of a symbol and the formatting of a symbol: the round trip of
// every generated symbol, the uniqueness of the generated symbols against the
// shadow table, the rules of the prefixes, the encoding aliases and the
// rejections.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/LumexUnitsTestSupport.hpp"

using namespace lumex_test_units;

namespace
{
// UTF-8 of the characters that the tests spell, as escapes (a literal
// continues after an escape only through adjacent literals).
#define U_MICRO "\xC2\xB5"
#define U_MU "\xCE\xBC"
#define U_OHM "\xCE\xA9"
#define U_OHM_SIGN "\xE2\x84\xA6"
#define U_ANGSTROM "\xC3\x85"
#define U_ANGSTROM_SIGN "\xE2\x84\xAB"
#define U_DEGREE "\xC2\xB0"
#define U_CELSIUS_SIGN "\xE2\x84\x83"
#define U_KELVIN_SIGN "\xE2\x84\xAA"
#define U_CYRILLIC_ES "\xD0\xA1"

bool
find (std::string const &text, script in, prefixed_unit_t &out)
{
  result_t<prefixed_unit_t, lookup_error> const result
      = lookup (view_of (text), in);
  if (!result.has_value ())
    return false;
  out = result.value ();
  return true;
}

std::uint32_t
data_prefixes (unit_id unit)
{
  return descriptor (unit).prefixes;
}

prefixed_unit_t
pair_of (unit_id unit, prefix_id prefix = prefix_id::none)
{
  prefixed_unit_t pair = { unit, prefix };
  return pair;
}

// One generated symbol: the text a prefix symbol and a symbol of a unit make.
struct generated_t
{
  script in;
  prefix_id prefix;
  unit_id unit;
  std::string text;
};

std::vector<generated_t>
all_generated ()
{
  std::vector<generated_t> list;
  for (script in : { script::international, script::russian })
    {
      for (std::size_t unit_index = 0; unit_index < unit_count; ++unit_index)
        {
          unit_id const unit = unit_id_at (unit_index);
          for (unit_symbol_t const &symbol : symbols_of (unit, in))
            {
              if (!symbol.prefixable)
                continue;
              for (std::size_t prefix_index = 0; prefix_index < prefix_count;
                   ++prefix_index)
                {
                  prefix_id const prefix
                      = static_cast<prefix_id> (prefix_index);
                  if (!takes_prefix (unit, prefix))
                    continue;
                  for (std::string const &prefix_text :
                       prefix_symbols_of (prefix, in))
                    {
                      generated_t entry
                          = { in, prefix, unit, prefix_text + symbol.text };
                      list.push_back (entry);
                    }
                }
            }
        }
    }
  return list;
}

// The bare symbols and aliases of a script: text to unit.
std::map<std::string, unit_id>
bare_symbols (script in)
{
  std::map<std::string, unit_id> map;
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const unit = unit_id_at (index);
      for (unit_symbol_t const &symbol : symbols_of (unit, in))
        map[symbol.text] = unit;
    }
  return map;
}
} // namespace

TEST (
    LumexUnitsLookupTest,
    GivenEveryBareSymbolAndAlias_WhenLookedUp_ThenTheUnitComesBackWithoutPrefix)
{
  for (script in : { script::international, script::russian })
    {
      for (std::size_t index = 0; index < unit_count; ++index)
        {
          unit_id const unit = unit_id_at (index);
          for (unit_symbol_t const &symbol : symbols_of (unit, in))
            {
              SCOPED_TRACE (std::string (script_name (in)) + " "
                            + descriptor (unit).name + " " + symbol.text);
              prefixed_unit_t found = pair_of (unit_id ());
              ASSERT_TRUE (find (symbol.text, in, found));
              EXPECT_EQ (unit, found.unit);
              EXPECT_EQ (prefix_id::none, found.prefix);
            }
        }
    }
}

TEST (
    LumexUnitsLookupTest,
    GivenEveryGeneratedSymbol_WhenLookedUp_ThenThePairComesBackExceptShadowedOnes)
{
  std::vector<generated_t> const generated = all_generated ();
  ASSERT_GT (generated.size (), 2000u);
  std::map<std::string, unit_id> const bare_intl
      = bare_symbols (script::international);
  std::map<std::string, unit_id> const bare_ru
      = bare_symbols (script::russian);
  std::size_t shadowed = 0;
  for (generated_t const &entry : generated)
    {
      SCOPED_TRACE (std::string (script_name (entry.in)) + " "
                    + prefix_names[static_cast<std::size_t> (entry.prefix)]
                    + " + " + descriptor (entry.unit).name + " = "
                    + entry.text);
      std::map<std::string, unit_id> const &bare
          = entry.in == script::international ? bare_intl : bare_ru;
      prefixed_unit_t found = pair_of (unit_id ());
      ASSERT_TRUE (find (entry.text, entry.in, found));
      std::map<std::string, unit_id>::const_iterator const hidden
          = bare.find (entry.text);
      if (hidden != bare.end ())
        {
          ++shadowed;
          EXPECT_EQ (hidden->second, found.unit);
          EXPECT_EQ (prefix_id::none, found.prefix);
        }
      else
        {
          EXPECT_EQ (entry.unit, found.unit);
          EXPECT_EQ (entry.prefix, found.prefix);
        }
    }
  EXPECT_GE (shadowed, shadow_count);
}

TEST (LumexUnitsLookupTest,
      GivenEveryGeneratedSymbol_WhenCollided_ThenOnlyTheShadowTableDiffers)
{
  // Recompute every collision: a generated text equal to a bare symbol is
  // shadowed; a generated text made by two different pairs would be ambiguous.
  std::set<std::string> recomputed;
  std::map<std::string, std::set<std::pair<int, int>>> by_text;
  std::map<std::string, unit_id> const bare_intl
      = bare_symbols (script::international);
  std::map<std::string, unit_id> const bare_ru
      = bare_symbols (script::russian);
  for (generated_t const &entry : all_generated ())
    {
      std::map<std::string, unit_id> const &bare
          = entry.in == script::international ? bare_intl : bare_ru;
      std::string const scripted
          = std::string (script_name (entry.in)) + ":" + entry.text;
      by_text[scripted].insert (std::make_pair (
          static_cast<int> (entry.prefix), static_cast<int> (entry.unit)));
      std::map<std::string, unit_id>::const_iterator const hidden
          = bare.find (entry.text);
      if (hidden != bare.end ())
        {
          recomputed.insert (
              std::string (script_name (entry.in)) + "|"
              + prefix_names[static_cast<std::size_t> (entry.prefix)] + "|"
              + descriptor (entry.unit).name + "|"
              + descriptor (hidden->second).name + "|" + entry.text);
        }
    }
  // No two pairs make one text.
  for (std::map<std::string, std::set<std::pair<int, int>>>::const_iterator it
       = by_text.begin ();
       it != by_text.end (); ++it)
    EXPECT_EQ (1u, it->second.size ()) << it->first << " is made by two pairs";
  // The shadow table is exactly the recomputed set.
  std::set<std::string> tabulated;
  for (std::size_t index = 0; index < shadow_count; ++index)
    {
      shadow_descriptor_t const &entry = shadow_at (index);
      std::string const row
          = std::string (script_name (entry.symbol_script)) + "|"
            + prefix_names[static_cast<std::size_t> (entry.prefix)] + "|"
            + descriptor (entry.unit).name + "|"
            + descriptor (entry.shadowed_by).name + "|"
            + text_of (entry.symbol);
      EXPECT_TRUE (tabulated.insert (row).second) << "twice: " << row;
    }
  EXPECT_EQ (tabulated, recomputed);
  std::cout << "[ units ] shadowed generated symbols: " << recomputed.size ()
            << "\n";
  for (std::set<std::string>::const_iterator it = recomputed.begin ();
       it != recomputed.end (); ++it)
    std::cout << "[ units ]   " << *it << "\n";
}

TEST (
    LumexUnitsLookupTest,
    GivenTheShadowTable_WhenLookedUp_ThenTheBareUnitWinsAndThePairIsReachedById)
{
  for (std::size_t index = 0; index < shadow_count; ++index)
    {
      shadow_descriptor_t const &entry = shadow_at (index);
      SCOPED_TRACE (text_of (entry.symbol));
      prefixed_unit_t found = pair_of (unit_id ());
      ASSERT_TRUE (find (text_of (entry.symbol), entry.symbol_script, found));
      EXPECT_EQ (entry.shadowed_by, found.unit);
      EXPECT_EQ (prefix_id::none, found.prefix);
      // The shadowed pair itself is a legal pair that is formatted and
      // converted by id.
      EXPECT_TRUE (takes_prefix (entry.unit, entry.prefix));
      // ... but its symbol is the other unit's: it is not written (what is
      // written is always read back as the same pair).
      result_t<symbol_buffer_t, format_error> const written = format_symbol (
          pair_of (entry.unit, entry.prefix), entry.symbol_script);
      ASSERT_FALSE (written.has_value ());
      EXPECT_EQ (format_error::shadowed, written.error ());
    }
}

TEST (LumexUnitsLookupTest,
      GivenShadowedSymbolsOfTheDesign_WhenLookedUp_ThenTheBareUnitIsFound)
{
  struct case_t
  {
    char const *text;
    script in;
    unit_id unit;
  };
  case_t const cases[] = {
    { "\xD0\xA2\xD0\xBB", script::russian, unit_id::tesla }, // Tl
    { "\xD0\x93\xD1\x81", script::russian, unit_id::gauss }, // Gs
    { "Gs", script::international, unit_id::gauss },
    { "\xD0\xB3\xD1\x81", script::russian, unit_id::gram_force }, // gs
    { "\xD0\x93\xD0\xB1", script::russian, unit_id::gilbert },    // Gb
    { "Gb", script::international, unit_id::gilbert },
    { "as", script::international, unit_id::arcsecond },
    { "ft", script::international, unit_id::foot },
    { "\xD1\x84\xD1\x82", script::russian, unit_id::foot }, // ft
    { "nt", script::international, unit_id::nit },
    { "\xD0\xBD\xD1\x82", script::russian, unit_id::nit }, // nt
    { "\xD0\xB3\xD1\x80\xD0\xB0\xD0\xB4", script::russian,
      unit_id::gon }, // grad
    { "rad", script::international, unit_id::radian },
    { "\xD1\x80\xD0\xB0\xD0\xB4", script::russian, unit_id::radian },
    { "rd", script::international, unit_id::rad_dose },
  };
  for (case_t const &item : cases)
    {
      SCOPED_TRACE (item.text);
      prefixed_unit_t found = pair_of (unit_id ());
      ASSERT_TRUE (find (item.text, item.in, found));
      EXPECT_EQ (item.unit, found.unit);
      EXPECT_EQ (prefix_id::none, found.prefix);
    }
}

TEST (
    LumexUnitsLookupTest,
    GivenPrefixedSymbols_WhenLookedUp_ThenTheOtherReadingOfASpellingIsStillReachable)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("mas", script::international, found));
  EXPECT_EQ (pair_of (unit_id::arcsecond, prefix_id::milli), found);
  ASSERT_TRUE (find (U_MICRO "as", script::international, found));
  EXPECT_EQ (pair_of (unit_id::arcsecond, prefix_id::micro), found);
  ASSERT_TRUE (find ("hrad", script::international, found));
  EXPECT_EQ (pair_of (unit_id::radian, prefix_id::hecto), found);
  ASSERT_TRUE (find ("TL", script::international, found));
  EXPECT_EQ (pair_of (unit_id::litre, prefix_id::tera), found);
  ASSERT_TRUE (find ("kGs", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gauss, prefix_id::kilo), found);
  ASSERT_TRUE (find ("mG", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gauss, prefix_id::milli), found);
}

TEST (LumexUnitsLookupTest,
      GivenKilogram_WhenLookedUp_ThenItIsTheKiloOfTheGram)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("kg", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gram, prefix_id::kilo), found);
  ASSERT_TRUE (find ("\xD0\xBA\xD0\xB3", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::gram, prefix_id::kilo), found);
  // The kilogram is the coherent unit of mass: 10^-3 * 10^3 = 1.
  scale_t const scale = scale_of (found);
  EXPECT_EQ (0, scale.exponent10);
  EXPECT_EQ (1, scale.numerator);
  EXPECT_EQ (1, scale.denominator);
}

TEST (LumexUnitsLookupTest,
      GivenCompoundPrefixes_WhenLookedUp_ThenTheyAreUnknown)
{
  char const *const unknown_intl[] = { U_MICRO "kg",
                                       "mkm",
                                       "kkg",
                                       "Mkg",
                                       U_MICRO U_MICRO "F",
                                       "dakm",
                                       "kk",
                                       "mm"
                                       "m",
                                       "uu"
                                       "F" };
  for (char const *text : unknown_intl)
    {
      SCOPED_TRACE (text);
      prefixed_unit_t found = pair_of (unit_id ());
      EXPECT_FALSE (find (text, script::international, found));
    }
  char const *const unknown_ru[]
      = { "\xD0\xBC\xD0\xBA\xD0\xBA\xD0\xB3",         // mkkg
          "\xD0\xBC\xD0\xBA\xD0\xBC\xD0\xBA\xD0\xA4", // mkmkF
          "\xD0\xBA\xD0\xBA\xD0\xB3",                 // kkg
          "\xD0\xBC\xD0\xBC\xD0\xBC" };               // mmm
  for (char const *text : unknown_ru)
    {
      SCOPED_TRACE (text);
      prefixed_unit_t found = pair_of (unit_id ());
      EXPECT_FALSE (find (text, script::russian, found));
    }
}

TEST (LumexUnitsLookupTest,
      GivenUnitsThatTakeNoPrefix_WhenLookedUpWithOne_ThenTheyAreUnknown)
{
  struct case_t
  {
    char const *text;
    script in;
  };
  case_t const cases[] = {
    { "kmin", script::international },
    { "\xD0\xBA\xD0\xBC\xD0\xB8\xD0\xBD", script::russian }, // kmin
    { "kh", script::international },
    { "kd", script::international },
    { "k" U_DEGREE, script::international },
    { "mau", script::international },
    { "kua", script::international },
    { "k\xD0\xB4\xD0\xBF\xD1\x82\xD1\x80", script::russian }, // k diopter
    { "k\xD0\xB0.\xD0\xB5.\xD0\xBC.", script::russian },      // k a.e.m.
    { "mu", script::international },                          // m + u
    { "ku", script::international },
    { "kmmHg", script::international },
    { "kmin", script::international },
  };
  for (case_t const &item : cases)
    {
      SCOPED_TRACE (item.text);
      prefixed_unit_t found = pair_of (unit_id ());
      EXPECT_FALSE (find (item.text, item.in, found));
    }
  EXPECT_FALSE (takes_prefix (unit_id::minute, prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::hour, prefix_id::milli));
  EXPECT_FALSE (takes_prefix (unit_id::day, prefix_id::deci));
  EXPECT_FALSE (takes_prefix (unit_id::degree, prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::arcminute, prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::astronomical_unit, prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::diopter, prefix_id::kilo));
  EXPECT_FALSE (takes_prefix (unit_id::one, prefix_id::kilo));
}

TEST (LumexUnitsLookupTest,
      GivenDaltonSymbols_WhenPrefixed_ThenOnlyDaTakesPrefixes)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("kDa", script::international, found));
  EXPECT_EQ (pair_of (unit_id::dalton, prefix_id::kilo), found);
  ASSERT_TRUE (find ("u", script::international, found));
  EXPECT_EQ (pair_of (unit_id::dalton), found);
  // "mu" would be the milli dalton by its alias u, which takes no prefix.
  EXPECT_FALSE (find ("mu", script::international, found));
  ASSERT_TRUE (find ("\xD0\xB0.\xD0\xB5.\xD0\xBC.", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::dalton), found);
}

TEST (LumexUnitsLookupTest, GivenArcsecond_WhenPrefixed_ThenMilliMicroPicoOnly)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("\xE2\x80\xB3", script::international, found)); // U+2033
  EXPECT_EQ (pair_of (unit_id::arcsecond), found);
  ASSERT_TRUE (find ("mas", script::international, found));
  ASSERT_TRUE (find (U_MICRO "as", script::international, found));
  ASSERT_TRUE (find ("pas", script::international, found));
  EXPECT_FALSE (find ("kas", script::international, found));
  EXPECT_FALSE (find ("nas", script::international, found));
  EXPECT_FALSE (find ("m\xE2\x80\xB3", script::international, found));
  // The Russian sign takes no prefix (GOST 8.417-2024 table 5 note 2).
  ASSERT_TRUE (find ("\xE2\x80\xB3", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::arcsecond), found);
  EXPECT_FALSE (find ("\xD0\xBC\xE2\x80\xB3", script::russian, found));
  EXPECT_FALSE (find ("\xD0\xBA\xE2\x80\xB3", script::russian, found));
  EXPECT_FALSE (descriptor (unit_id::arcsecond).russian.prefixable);
  EXPECT_FALSE (descriptor (unit_id::arcsecond).international.prefixable);
  EXPECT_FALSE (format_symbol (pair_of (unit_id::arcsecond, prefix_id::milli),
                               script::russian)
                    .has_value ());
}

TEST (LumexUnitsLookupTest,
      GivenEncodingAliases_WhenLookedUp_ThenTheyFindTheSameUnit)
{
  prefixed_unit_t found = pair_of (unit_id ());
  // micro: U+00B5 (output), U+03BC and the ASCII u (input)
  for (char const *text : { U_MICRO "m", U_MU "m", "um" })
    {
      SCOPED_TRACE (text);
      ASSERT_TRUE (find (text, script::international, found));
      EXPECT_EQ (pair_of (unit_id::metre, prefix_id::micro), found);
    }
  // ohm: U+03A9 (output), U+2126 and the ASCII word
  for (char const *text : { U_OHM, U_OHM_SIGN, "ohm" })
    {
      SCOPED_TRACE (text);
      ASSERT_TRUE (find (text, script::international, found));
      EXPECT_EQ (pair_of (unit_id::ohm), found);
    }
  for (char const *text : { "k" U_OHM, "k" U_OHM_SIGN, "kohm" })
    {
      SCOPED_TRACE (text);
      ASSERT_TRUE (find (text, script::international, found));
      EXPECT_EQ (pair_of (unit_id::ohm, prefix_id::kilo), found);
    }
  // angstrom: U+00C5 and U+212B, in both scripts
  for (script in : { script::international, script::russian })
    {
      ASSERT_TRUE (find (U_ANGSTROM, in, found));
      EXPECT_EQ (pair_of (unit_id::angstrom), found);
      ASSERT_TRUE (find (U_ANGSTROM_SIGN, in, found));
      EXPECT_EQ (pair_of (unit_id::angstrom), found);
    }
  // degree Celsius: the degree sign and C, or U+2103, in both scripts
  for (script in : { script::international, script::russian })
    {
      ASSERT_TRUE (find (U_DEGREE "C", in, found));
      EXPECT_EQ (pair_of (unit_id::degree_celsius), found);
      ASSERT_TRUE (find (U_CELSIUS_SIGN, in, found));
      EXPECT_EQ (pair_of (unit_id::degree_celsius), found);
    }
  // kelvin: K and U+212A
  ASSERT_TRUE (find ("K", script::international, found));
  EXPECT_EQ (pair_of (unit_id::kelvin), found);
  ASSERT_TRUE (find (U_KELVIN_SIGN, script::international, found));
  EXPECT_EQ (pair_of (unit_id::kelvin), found);
  ASSERT_TRUE (find ("m" U_KELVIN_SIGN, script::international, found));
  EXPECT_EQ (pair_of (unit_id::kelvin, prefix_id::milli), found);
  // micron: U+00B5 and U+03BC as a bare unit
  ASSERT_TRUE (find (U_MICRO, script::international, found));
  EXPECT_EQ (pair_of (unit_id::micron), found);
  ASSERT_TRUE (find (U_MU, script::international, found));
  EXPECT_EQ (pair_of (unit_id::micron), found);
  // the astronomical unit: au (SI Brochure) and ua (GOST)
  ASSERT_TRUE (find ("au", script::international, found));
  EXPECT_EQ (pair_of (unit_id::astronomical_unit), found);
  ASSERT_TRUE (find ("ua", script::international, found));
  EXPECT_EQ (pair_of (unit_id::astronomical_unit), found);
  // the litre: L and l, prefixed
  ASSERT_TRUE (find ("mL", script::international, found));
  EXPECT_EQ (pair_of (unit_id::litre, prefix_id::milli), found);
  ASSERT_TRUE (find ("ml", script::international, found));
  EXPECT_EQ (pair_of (unit_id::litre, prefix_id::milli), found);
}

TEST (LumexUnitsLookupTest,
      GivenCyrillicLetterInCelsius_WhenLookedUp_ThenItIsRejected)
{
  prefixed_unit_t found = pair_of (unit_id ());
  EXPECT_FALSE (find (U_DEGREE U_CYRILLIC_ES, script::international, found));
  EXPECT_FALSE (find (U_DEGREE U_CYRILLIC_ES, script::russian, found));
  EXPECT_FALSE (lookup (view_of (U_DEGREE U_CYRILLIC_ES)).has_value ());
  // also after a prefix of either script, and the capital letter
  for (char const *const text :
       { "m" U_DEGREE U_CYRILLIC_ES, "\xD0\xBC" U_DEGREE U_CYRILLIC_ES,
         U_DEGREE "\xD1\x81", "k" U_DEGREE "\xD1\x81" })
    {
      SCOPED_TRACE (text);
      EXPECT_FALSE (find (text, script::international, found));
      EXPECT_FALSE (find (text, script::russian, found));
      EXPECT_FALSE (lookup (view_of (text)).has_value ());
    }
}

TEST (
    LumexUnitsLookupTest,
    GivenCyrillicPrefixAndLatinDegreeCelsius_WhenLookedUp_ThenItIsTheOneAcceptedMixture)
{
  // The Russian column of the standard writes the Latin C after the degree
  // sign, so the Cyrillic prefixes go with it: U+043C U+00B0 C is the
  // millidegree Celsius, and so is U+043A (kilo). The Latin letters after a
  // Cyrillic prefix of any other unit are still a mixture of scripts.
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("\xD0\xBC" U_DEGREE "C", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::degree_celsius, prefix_id::milli), found);
  ASSERT_TRUE (find ("\xD0\xBA" U_DEGREE "C", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::degree_celsius, prefix_id::kilo), found);
  EXPECT_EQ (pair_of (unit_id::degree_celsius, prefix_id::milli),
             lookup (view_of ("\xD0\xBC" U_DEGREE "C")).value ());
  // not in the international script, where the prefix is Latin
  EXPECT_FALSE (find ("\xD0\xBC" U_DEGREE "C", script::international, found));
  // a Cyrillic prefix with a Latin symbol of another unit stays unknown
  EXPECT_FALSE (lookup (view_of ("\xD0\xBC"
                                 "m"))
                    .has_value ());
  EXPECT_FALSE (lookup (view_of ("\xD0\xBC"
                                 "K"))
                    .has_value ());
}

TEST (LumexUnitsLookupTest, GivenMixedScripts_WhenLookedUp_ThenTheyAreUnknown)
{
  char const *const texts[] = {
    "\xD0\xBC\xD0\xBA"
    "g",         // Cyrillic micro prefix, Latin g
    "m\xD0\xBC", // Latin milli, Cyrillic metre
    "\xD0\xBC"
    "m",         // Cyrillic milli, Latin metre
    "k\xD0\xB3", // Latin kilo, Cyrillic gram
    "\xD0\xBA"
    "g", // Cyrillic kilo, Latin gram
    "\xD0\x9F"
    "a", // Cyrillic Pa letter, Latin a
  };
  for (char const *text : texts)
    {
      SCOPED_TRACE (text);
      EXPECT_FALSE (lookup (view_of (text)).has_value ());
      prefixed_unit_t found = pair_of (unit_id ());
      EXPECT_FALSE (find (text, script::international, found));
      EXPECT_FALSE (find (text, script::russian, found));
    }
}

TEST (LumexUnitsLookupTest,
      GivenEmptyAndUnknownText_WhenLookedUp_ThenTheErrorTellsWhich)
{
  result_t<prefixed_unit_t, lookup_error> const empty
      = lookup (text_view_t ());
  ASSERT_FALSE (empty.has_value ());
  EXPECT_EQ (lookup_error::empty_text, empty.error ());
  result_t<prefixed_unit_t, lookup_error> const unknown
      = lookup (text_view_t ("xyz"));
  ASSERT_FALSE (unknown.has_value ());
  EXPECT_EQ (lookup_error::unknown_symbol, unknown.error ());
  EXPECT_FALSE (lookup (text_view_t (" m")).has_value ());
  EXPECT_FALSE (lookup (text_view_t ("m ")).has_value ());
  EXPECT_FALSE (lookup (text_view_t ("m"), script::russian).has_value ());
  EXPECT_FALSE (
      lookup (text_view_t ("\xD0\xBC"), script::international).has_value ());
}

TEST (LumexUnitsLookupTest,
      GivenNulInsideTheView_WhenLookedUp_ThenTheNulIsAnOrdinaryByte)
{
  char const with_nul[] = { 'm', '\0', 'm' };
  EXPECT_FALSE (lookup (text_view_t (with_nul, 3)).has_value ());
  ASSERT_TRUE (lookup (text_view_t (with_nul, 1)).has_value ());
  EXPECT_EQ (unit_id::metre, lookup (text_view_t (with_nul, 1)).value ().unit);
}

TEST (
    LumexUnitsLookupTest,
    GivenEitherScript_WhenLookedUp_ThenATextOfBothScriptsFindsTheSameUnitInBoth)
{
  EXPECT_EQ (pair_of (unit_id::pascal_unit, prefix_id::kilo),
             lookup (text_view_t ("kPa")).value ());
  EXPECT_EQ (pair_of (unit_id::pascal_unit, prefix_id::kilo),
             lookup (text_view_t ("\xD0\xBA\xD0\x9F\xD0\xB0")).value ());
  // A text that is a symbol of both scripts finds one unit in both.
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const unit = unit_id_at (index);
      for (unit_symbol_t const &symbol : symbols_of (unit, script::russian))
        {
          prefixed_unit_t international = pair_of (unit_id ());
          if (find (symbol.text, script::international, international))
            {
              prefixed_unit_t russian = pair_of (unit_id ());
              ASSERT_TRUE (find (symbol.text, script::russian, russian));
              EXPECT_EQ (international, russian) << symbol.text;
            }
        }
    }
}

TEST (LumexUnitsLookupTest,
      GivenByteSizes_WhenViewsAreBuilt_ThenTheyCountBytesNotCharacters)
{
  EXPECT_EQ (2u, text_view_t (U_MICRO).size ());
  EXPECT_EQ (3u, text_view_t (U_MICRO "m").size ());
  EXPECT_EQ (2u,
             canonical_symbol (unit_id::ohm, script::international).size ());
  EXPECT_EQ (4u, canonical_symbol (unit_id::ohm, script::russian).size ());
  EXPECT_EQ (
      2u, canonical_symbol (prefix_id::micro, script::international).size ());
  EXPECT_EQ (4u, canonical_symbol (prefix_id::micro, script::russian).size ());
  EXPECT_EQ (
      0u, canonical_symbol (unit_id::diopter, script::international).size ());
}

// --- formatting -------------------------------------------------------------

namespace
{
std::string
formatted (prefixed_unit_t pair, script in)
{
  result_t<symbol_buffer_t, format_error> const result
      = format_symbol (pair, in);
  if (!result.has_value ())
    return std::string ("error:") + error_name (result.error ());
  return text_of (result.value ().view ());
}
} // namespace

TEST (LumexUnitsFormatTest,
      GivenCanonicalForms_WhenFormatted_ThenTheyAreTheOnesOfTheStandards)
{
  EXPECT_EQ ("kg", formatted (pair_of (unit_id::gram, prefix_id::kilo),
                              script::international));
  EXPECT_EQ (
      "\xD0\xBA\xD0\xB3",
      formatted (pair_of (unit_id::gram, prefix_id::kilo), script::russian));
  EXPECT_EQ ("g", formatted (pair_of (unit_id::gram), script::international));
  EXPECT_EQ (U_MICRO "m",
             formatted (pair_of (unit_id::metre, prefix_id::micro),
                        script::international));
  EXPECT_EQ (
      "\xD0\xBC\xD0\xBA\xD0\xBC",
      formatted (pair_of (unit_id::metre, prefix_id::micro), script::russian));
  EXPECT_EQ (U_OHM, formatted (pair_of (unit_id::ohm), script::international));
  EXPECT_EQ ("M" U_OHM, formatted (pair_of (unit_id::ohm, prefix_id::mega),
                                   script::international));
  EXPECT_EQ ("\xD0\xBC\xD0\x9F\xD0\xB0",
             formatted (pair_of (unit_id::pascal_unit, prefix_id::milli),
                        script::russian));
  EXPECT_EQ ("L", formatted (pair_of (unit_id::litre), script::international));
  EXPECT_EQ ("mL", formatted (pair_of (unit_id::litre, prefix_id::milli),
                              script::international));
  EXPECT_EQ ("Gs",
             formatted (pair_of (unit_id::gauss), script::international));
  EXPECT_EQ ("mGs", formatted (pair_of (unit_id::gauss, prefix_id::milli),
                               script::international));
  EXPECT_EQ ("au", formatted (pair_of (unit_id::astronomical_unit),
                              script::international));
  EXPECT_EQ (U_DEGREE "C",
             formatted (pair_of (unit_id::degree_celsius), script::russian));
  EXPECT_EQ ("mm Hg", formatted (pair_of (unit_id::millimetre_of_mercury),
                                 script::international));
  EXPECT_EQ ("\xD0\xB3\xD0\x9F\xD0\xB0",
             formatted (pair_of (unit_id::pascal_unit, prefix_id::hecto),
                        script::russian));
  EXPECT_EQ (
      std::string ("\xD0\xA0\xD0\xBD") + "\xD0\x93\xD1\x86",
      formatted (pair_of (unit_id::hertz, prefix_id::ronna), script::russian));
}

TEST (LumexUnitsFormatTest,
      GivenAnAliasThatTakesPrefixes_WhenCanonicalDoesNot_ThenTheAliasIsUsed)
{
  EXPECT_EQ ("\xE2\x80\xB3",
             formatted (pair_of (unit_id::arcsecond), script::international));
  EXPECT_EQ ("mas", formatted (pair_of (unit_id::arcsecond, prefix_id::milli),
                               script::international));
  EXPECT_EQ (U_MICRO "as",
             formatted (pair_of (unit_id::arcsecond, prefix_id::micro),
                        script::international));
}

TEST (LumexUnitsFormatTest,
      GivenRefusedPairs_WhenFormatted_ThenTheErrorTellsWhy)
{
  EXPECT_EQ ("error:prefix_not_allowed",
             formatted (pair_of (unit_id::minute, prefix_id::kilo),
                        script::international));
  EXPECT_EQ ("error:prefix_not_allowed",
             formatted (pair_of (unit_id::arcsecond, prefix_id::kilo),
                        script::international));
  EXPECT_EQ ("error:no_symbol",
             formatted (pair_of (unit_id::diopter), script::international));
  EXPECT_EQ ("error:no_symbol",
             formatted (pair_of (unit_id::rad_dose), script::russian));
  EXPECT_EQ ("error:no_symbol",
             formatted (pair_of (unit_id::octave), script::international));
  // A prefix that the Russian symbol of the dalton does not take.
  EXPECT_EQ (
      "error:prefix_not_allowed",
      formatted (pair_of (unit_id::dalton, prefix_id::kilo), script::russian));
  EXPECT_EQ ("error:invalid_prefix",
             formatted (pair_of (unit_id::metre, static_cast<prefix_id> (30)),
                        script::international));
}

TEST (
    LumexUnitsFormatTest,
    GivenEveryAllowedPair_WhenFormatted_ThenLookupGivesTheSamePairOrTheShadowingUnit)
{
  std::size_t longest = 0;
  std::size_t count = 0;
  std::size_t shadowed = 0;
  for (script in : { script::international, script::russian })
    {
      std::map<std::string, unit_id> const bare = bare_symbols (in);
      for (std::size_t index = 0; index < unit_count; ++index)
        {
          unit_id const unit = unit_id_at (index);
          for (std::size_t prefix_index = 0; prefix_index <= prefix_count;
               ++prefix_index)
            {
              prefix_id const prefix
                  = prefix_index == prefix_count
                        ? prefix_id::none
                        : static_cast<prefix_id> (prefix_index);
              if (prefix != prefix_id::none && !takes_prefix (unit, prefix))
                continue;
              result_t<symbol_buffer_t, format_error> const text
                  = format_symbol (pair_of (unit, prefix), in);
              if (!text.has_value ())
                {
                  // A missing symbol, a prefix that the symbols of the unit do
                  // not take, or a pair that a bare symbol shadows.
                  if (text.error () == format_error::shadowed)
                    {
                      ++shadowed;
                      bool listed = false;
                      for (std::size_t row = 0; row < shadow_count; ++row)
                        {
                          shadow_descriptor_t const &entry = shadow_at (row);
                          listed = listed
                                   || (entry.unit == unit
                                       && entry.prefix == prefix
                                       && entry.symbol_script == in);
                        }
                      EXPECT_TRUE (listed) << descriptor (unit).name;
                    }
                  continue;
                }
              ++count;
              std::string const symbol = text_of (text.value ().view ());
              if (symbol.size () > longest)
                longest = symbol.size ();
              SCOPED_TRACE (std::string (script_name (in)) + " " + symbol);
              prefixed_unit_t found = pair_of (unit_id ());
              ASSERT_TRUE (find (symbol, in, found));
              if (bare.find (symbol) != bare.end ())
                EXPECT_EQ (pair_of (bare.find (symbol)->second), found);
              else
                EXPECT_EQ (pair_of (unit, prefix), found);
            }
        }
    }
  EXPECT_GT (count, 2000u);
  EXPECT_EQ (shadow_count, shadowed);
  EXPECT_LE (longest, symbol_buffer_capacity);
  std::cout << "[ units ] formatted symbols: " << count << ", longest "
            << longest << " bytes (capacity " << symbol_buffer_capacity
            << ")\n";
}

TEST (
    LumexUnitsFormatTest,
    GivenEveryUnitThatHasASymbol_WhenFormattedWithoutPrefix_ThenItIsTheCanonicalSymbol)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const unit = unit_id_at (index);
      for (script in : { script::international, script::russian })
        {
          SCOPED_TRACE (std::string (descriptor (unit).name) + " "
                        + script_name (in));
          std::string const canonical = text_of (canonical_symbol (unit, in));
          if (canonical.empty ())
            EXPECT_EQ ("error:no_symbol", formatted (pair_of (unit), in));
          else
            EXPECT_EQ (canonical, formatted (pair_of (unit), in));
        }
    }
}

TEST (LumexUnitsFormatTest, GivenNames_WhenAskedFor_ThenEveryEnumeratorHasOne)
{
  EXPECT_STREQ ("mechanics", domain_name (domain::mechanics));
  EXPECT_STREQ ("general", domain_name (domain::general));
  EXPECT_STREQ ("empty_text", error_name (lookup_error::empty_text));
  EXPECT_STREQ ("unknown_symbol", error_name (lookup_error::unknown_symbol));
  EXPECT_STREQ ("overflow", error_name (conversion_error::overflow));
  EXPECT_STREQ ("offset_unit", error_name (conversion_error::offset_unit));
  EXPECT_STREQ ("no_si_value", error_name (conversion_error::no_si_value));
  EXPECT_STREQ ("different_quantity_kind",
                error_name (conversion_error::different_quantity_kind));
  EXPECT_STREQ ("not_a_temperature",
                error_name (conversion_error::not_a_temperature));
  EXPECT_STREQ ("no_symbol", error_name (format_error::no_symbol));
  EXPECT_STREQ ("too_long", error_name (format_error::too_long));
  for (std::size_t index = 0;
       index <= static_cast<std::size_t> (domain::general); ++index)
    EXPECT_STREQ (domain_names[index],
                  domain_name (static_cast<domain> (index)));
}

// --- the input aliases of the second round ----------------------------------

TEST (LumexUnitsLookupTest,
      GivenAsciiQuotesAndRussianNames_WhenLookedUp_ThenTheyAreInputAliases)
{
  prefixed_unit_t found = pair_of (unit_id ());
  for (script in : { script::international, script::russian })
    {
      ASSERT_TRUE (find ("'", in, found));
      EXPECT_EQ (pair_of (unit_id::arcminute), found);
      ASSERT_TRUE (find ("\"", in, found));
      EXPECT_EQ (pair_of (unit_id::arcsecond), found);
      // the aliases take no prefix
      EXPECT_FALSE (find ("m'", in, found));
      EXPECT_FALSE (find ("k\"", in, found));
    }
  // U+043C U+043C U+002E U+0020 ... as printed in the online text of table 7
  ASSERT_TRUE (find ("\xD0\xBC\xD0\xBC. \xD1\x80\xD1\x82. \xD1\x81\xD1\x82",
                     script::russian, found));
  EXPECT_EQ (pair_of (unit_id::millimetre_of_mercury), found);
  ASSERT_TRUE (find ("\xD0\xBC\xD0\xBC \xD1\x80\xD1\x82. \xD1\x81\xD1\x82.",
                     script::russian, found));
  EXPECT_EQ (pair_of (unit_id::millimetre_of_mercury), found);
  ASSERT_TRUE (find ("\xD0\xB0\xD1\x82\xD0\xBC", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::standard_atmosphere), found);
  ASSERT_TRUE (
      find ("\xD1\x82\xD0\xBE\xD1\x80\xD1\x80", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::torr), found);
  ASSERT_TRUE (find ("\xD0\xBC\xD1\x82\xD0\xBE\xD1\x80\xD1\x80",
                     script::russian, found));
  EXPECT_EQ (pair_of (unit_id::torr, prefix_id::milli), found);
  // the atmosphere takes no prefix
  EXPECT_FALSE (
      find ("\xD0\xBA\xD0\xB0\xD1\x82\xD0\xBC", script::russian, found));
  EXPECT_FALSE (
      find ("\xD0\xB0\xD1\x82\xD0\xBC", script::international, found));
}

TEST (LumexUnitsLookupTest,
      GivenOerstedAndGilbert_WhenPrefixed_ThenAllPrefixesAreTakenButGi)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find ("kOe", script::international, found));
  EXPECT_EQ (pair_of (unit_id::oersted, prefix_id::kilo), found);
  ASSERT_TRUE (find ("\xD0\xBA\xD0\xAD", script::russian, found)); // k + E
  EXPECT_EQ (pair_of (unit_id::oersted, prefix_id::kilo), found);
  ASSERT_TRUE (find ("mGb", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gilbert, prefix_id::milli), found);
  ASSERT_TRUE (find ("GGb", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gilbert, prefix_id::giga), found);
  ASSERT_TRUE (find ("\xD0\xBC\xD0\x93\xD0\xB1", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::gilbert, prefix_id::milli), found);
  // Gi is the NIST alias and also the binary prefix gibi: no prefix on it
  ASSERT_TRUE (find ("Gi", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gilbert), found);
  EXPECT_FALSE (find ("kGi", script::international, found));
  EXPECT_FALSE (find ("mGi", script::international, found));
  EXPECT_EQ (data_prefixes (unit_id::oersted), all_prefixes_mask);
  EXPECT_EQ (data_prefixes (unit_id::gilbert), all_prefixes_mask);
}

TEST (
    LumexUnitsFormatTest,
    GivenAUnitWithoutACanonicalSymbol_WhenFormatted_ThenItsAliasesAreNeverWritten)
{
  std::size_t checked = 0;
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_id const unit = unit_id_at (index);
      for (script in : { script::international, script::russian })
        {
          if (!canonical_symbol (unit, in).empty ())
            continue;
          ++checked;
          SCOPED_TRACE (std::string (descriptor (unit).name) + " "
                        + script_name (in));
          for (std::size_t prefix_index = 0; prefix_index <= prefix_count;
               ++prefix_index)
            {
              prefix_id const prefix
                  = prefix_index == prefix_count
                        ? prefix_id::none
                        : static_cast<prefix_id> (prefix_index);
              EXPECT_FALSE (
                  format_symbol (pair_of (unit, prefix), in).has_value ());
            }
        }
    }
  EXPECT_GT (checked, 5u);
  // the torr and the atmosphere: written only in the international script
  EXPECT_EQ ("Torr",
             formatted (pair_of (unit_id::torr), script::international));
  EXPECT_EQ ("mTorr", formatted (pair_of (unit_id::torr, prefix_id::milli),
                                 script::international));
  EXPECT_EQ (
      "error:no_symbol",
      formatted (pair_of (unit_id::torr, prefix_id::milli), script::russian));
  EXPECT_EQ (
      "error:no_symbol",
      formatted (pair_of (unit_id::standard_atmosphere), script::russian));
}

TEST (
    LumexUnitsFormatTest,
    GivenTheAsciiQuoteAliases_WhenArcUnitsAreFormatted_ThenTheSignsAreWritten)
{
  EXPECT_EQ ("\xE2\x80\xB2",
             formatted (pair_of (unit_id::arcminute), script::international));
  EXPECT_EQ ("\xE2\x80\xB3",
             formatted (pair_of (unit_id::arcsecond), script::russian));
}

TEST (LumexUnitsLookupTest,
      GivenDegreeCelsiusAndGon_WhenPrefixed_ThenTheyTakeThePrefixes)
{
  prefixed_unit_t found = pair_of (unit_id ());
  ASSERT_TRUE (find (U_DEGREE "C", script::international, found));
  EXPECT_EQ (pair_of (unit_id::degree_celsius), found);
  ASSERT_TRUE (find ("m" U_DEGREE "C", script::international, found));
  EXPECT_EQ (pair_of (unit_id::degree_celsius, prefix_id::milli), found);
  ASSERT_TRUE (find ("\xD0\xBC" U_DEGREE "C", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::degree_celsius, prefix_id::milli), found);
  ASSERT_TRUE (find ("mgon", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gon, prefix_id::milli), found);
  ASSERT_TRUE (find ("kgon", script::international, found));
  EXPECT_EQ (pair_of (unit_id::gon, prefix_id::kilo), found);
  // the Russian gon symbol is the hectoradian's: the unit wins, the other
  // prefixes work
  ASSERT_TRUE (
      find ("\xD0\xB3\xD1\x80\xD0\xB0\xD0\xB4", script::russian, found));
  EXPECT_EQ (pair_of (unit_id::gon), found);
  ASSERT_TRUE (find ("\xD0\xBA\xD0\xB3\xD1\x80\xD0\xB0\xD0\xB4",
                     script::russian, found));
  EXPECT_EQ (pair_of (unit_id::gon, prefix_id::kilo), found);
  EXPECT_EQ ("m" U_DEGREE "C",
             formatted (pair_of (unit_id::degree_celsius, prefix_id::milli),
                        script::international));
  EXPECT_EQ ("error:shadowed",
             formatted (pair_of (unit_id::radian, prefix_id::hecto),
                        script::russian));
  EXPECT_EQ (
      "error:shadowed",
      formatted (pair_of (unit_id::litre, prefix_id::tera), script::russian));
}

TEST (LumexUnitsFormatTest,
      GivenUnitsOrPrefixesOutsideTheCatalog_WhenFormatted_ThenTheyAreRefused)
{
  EXPECT_EQ ("error:invalid_unit",
             formatted (pair_of (static_cast<unit_id> (unit_count)),
                        script::international));
  EXPECT_EQ ("error:invalid_prefix",
             formatted (pair_of (unit_id::metre, static_cast<prefix_id> (31)),
                        script::international));
}

TEST (LumexUnitsFormatTest,
      GivenEveryWrittenSymbol_WhenReadBack_ThenItIsTheSamePair)
{
  std::size_t written = 0;
  for (script in : { script::international, script::russian })
    {
      for (std::size_t index = 0; index < unit_count; ++index)
        {
          for (std::size_t prefix_index = 0; prefix_index <= prefix_count;
               ++prefix_index)
            {
              prefixed_unit_t const pair
                  = pair_of (unit_id_at (index),
                             prefix_index == prefix_count
                                 ? prefix_id::none
                                 : static_cast<prefix_id> (prefix_index));
              result_t<symbol_buffer_t, format_error> const text
                  = format_symbol (pair, in);
              if (!text.has_value ())
                continue;
              ++written;
              prefixed_unit_t found = pair_of (unit_id ());
              ASSERT_TRUE (find (text_of (text.value ().view ()), in, found));
              EXPECT_EQ (pair, found);
            }
        }
    }
  EXPECT_GT (written, 2000u);
}

// lumex/tests/core/units/data/LumexUnitsOracle.cxx11.tests.cpp
//
// Every row of the catalog against the oracle (oracle/oracle_units.py, an
// independent transcription of the standards): symbols byte for byte, aliases,
// dimension, canonical scale, power of pi, offset, factor kind, prefix set,
// compound flag, quantity kind, domain, standing and system; and the prefix
// table.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/LumexUnitsTestSupport.hpp"

using namespace lumex_test_units;

TEST (LumexUnitsOracleTest, GivenOracle_WhenCounted_ThenItHasOneRowPerUnit)
{
  EXPECT_EQ (unit_count, oracle_row_count);
  EXPECT_EQ (prefix_count, oracle_prefix_count);
}

TEST (LumexUnitsOracleTest,
      GivenEveryRow_WhenComparedWithTheOracle_ThenNamesAndOrderAgree)
{
  ASSERT_EQ (unit_count, oracle_row_count);
  for (std::size_t index = 0; index < oracle_row_count; ++index)
    {
      oracle_row_t const &expected = oracle_rows[index];
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      EXPECT_STREQ (expected.id, unit.name) << "row " << index;
    }
}

TEST (LumexUnitsOracleTest,
      GivenEveryRow_WhenComparedWithTheOracle_ThenClassificationAgrees)
{
  ASSERT_EQ (unit_count, oracle_row_count);
  for (std::size_t index = 0; index < oracle_row_count; ++index)
    {
      oracle_row_t const &expected = oracle_rows[index];
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      SCOPED_TRACE (expected.id);
      EXPECT_STREQ (expected.domain,
                    domain_names[static_cast<std::size_t> (unit.unit_domain)]);
      EXPECT_STREQ (expected.kind,
                    kind_names[static_cast<std::size_t> (unit.kind)]);
      EXPECT_STREQ (
          expected.standing,
          standing_names[static_cast<std::size_t> (unit.unit_standing)]);
      EXPECT_STREQ (expected.system,
                    system_names[static_cast<std::size_t> (unit.system)]);
      EXPECT_STREQ (expected.factor,
                    factor_names[static_cast<std::size_t> (unit.factor)]);
      EXPECT_EQ (expected.compound, unit.compound);
    }
}

TEST (LumexUnitsOracleTest,
      GivenEveryRow_WhenComparedWithTheOracle_ThenSymbolsAreEqualByte)
{
  ASSERT_EQ (unit_count, oracle_row_count);
  for (std::size_t index = 0; index < oracle_row_count; ++index)
    {
      oracle_row_t const &expected = oracle_rows[index];
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      SCOPED_TRACE (expected.id);
      bool const has_prefixes = std::string (expected.prefixes) != "none";
      struct column_t
      {
        char const *canonical;
        char const *aliases;
        symbol_t const &symbol;
        script in;
      };
      column_t const columns[2]
          = { { expected.intl, expected.intl_aliases, unit.international,
                script::international },
              { expected.ru, expected.ru_aliases, unit.russian,
                script::russian } };
      for (column_t const &column : columns)
        {
          SCOPED_TRACE (script_name (column.in));
          oracle_symbol_t const canonical
              = parse_oracle_symbol (column.canonical);
          EXPECT_EQ (canonical.text, text_of (column.symbol));
          if (!canonical.text.empty ())
            {
              // The oracle's flag is "takes prefixes"; the catalog's is
              // "takes the prefixes of its unit", which an empty set voids.
              EXPECT_EQ (canonical.prefixable && has_prefixes,
                         column.symbol.prefixable && has_prefixes);
            }
          std::vector<unit_symbol_t> const have
              = symbols_of (unit.id, column.in);
          std::vector<std::string> const want = split_list (column.aliases);
          // The first entry of `have` is the canonical symbol, when it is not
          // empty.
          std::size_t const skip = canonical.text.empty () ? 0 : 1;
          ASSERT_EQ (want.size (), have.size () - skip);
          for (std::size_t alias = 0; alias < want.size (); ++alias)
            {
              oracle_symbol_t const wanted = parse_oracle_symbol (want[alias]);
              EXPECT_EQ (wanted.text, have[alias + skip].text);
              EXPECT_EQ (wanted.prefixable && has_prefixes,
                         have[alias + skip].prefixable && has_prefixes);
            }
        }
    }
}

TEST (LumexUnitsOracleTest,
      GivenEveryRow_WhenComparedWithTheOracle_ThenAliasCountsAdd)
{
  std::size_t wanted = 0;
  for (std::size_t index = 0; index < oracle_row_count; ++index)
    {
      wanted += split_list (oracle_rows[index].intl_aliases).size ();
      wanted += split_list (oracle_rows[index].ru_aliases).size ();
    }
  EXPECT_EQ (wanted, alias_count);
}

TEST (LumexUnitsOracleTest,
      GivenEveryRow_WhenComparedWithTheOracle_ThenValuesAreEqual)
{
  ASSERT_EQ (unit_count, oracle_row_count);
  for (std::size_t index = 0; index < oracle_row_count; ++index)
    {
      oracle_row_t const &expected = oracle_rows[index];
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      SCOPED_TRACE (expected.id);
      for (std::size_t axis = 0; axis < 7; ++axis)
        EXPECT_EQ (expected.dimension[axis], unit.dimension.exponent[axis])
            << "base quantity " << axis;
      if (expected.has_value)
        {
          EXPECT_EQ (expected.k, unit.scale.exponent10);
          EXPECT_EQ (expected.p, unit.scale.numerator);
          EXPECT_EQ (expected.q, unit.scale.denominator);
          EXPECT_EQ (expected.pi, unit.scale.pi_power);
        }
      else
        {
          EXPECT_EQ (factor_kind::none, unit.factor);
          EXPECT_EQ (0, unit.scale.numerator);
        }
      if (expected.has_offset)
        {
          EXPECT_EQ (expected.offset_k, unit.offset.exponent10);
          EXPECT_EQ (expected.offset_p, unit.offset.numerator);
          EXPECT_EQ (expected.offset_q, unit.offset.denominator);
        }
      else
        EXPECT_EQ (0, unit.offset.numerator);
      EXPECT_EQ (mask_of_spec (expected.prefixes), unit.prefixes);
    }
}

TEST (
    LumexUnitsOracleTest,
    GivenPrefixTable_WhenComparedWithTheOracle_ThenSymbolsAndExponentsAreEqual)
{
  ASSERT_EQ (prefix_count, oracle_prefix_count);
  for (std::size_t index = 0; index < oracle_prefix_count; ++index)
    {
      oracle_prefix_t const &expected = oracle_prefixes[index];
      prefix_descriptor_t const &prefix = prefix_at (index);
      SCOPED_TRACE (expected.name);
      EXPECT_STREQ (expected.name, prefix.name);
      EXPECT_EQ (expected.exponent, prefix.exponent);
      EXPECT_EQ (std::string (expected.intl), text_of (prefix.international));
      EXPECT_EQ (std::string (expected.ru), text_of (prefix.russian));
      std::vector<std::string> const want = split_list (expected.intl_aliases);
      std::vector<std::string> const have
          = prefix_symbols_of (prefix.id, script::international);
      ASSERT_EQ (want.size () + 1, have.size ());
      for (std::size_t alias = 0; alias < want.size (); ++alias)
        EXPECT_EQ (want[alias], have[alias + 1]);
      EXPECT_EQ (1u, prefix_symbols_of (prefix.id, script::russian).size ());
    }
  EXPECT_EQ (2u, prefix_alias_count);
}

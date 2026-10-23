// Size limits of the DSL (verification F2 of the design): a row list of 100
// rows and a layout of 40 fields whose types carry their own commas
// (scale<std::ratio<1, 100> >) compile, and every row and field keeps its
// name and position.

#include <cstddef>
#include <ratio>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolTestTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::protocol_tables;
typedef tables::large_protocol large;
typedef field::u16_le_<field::scale<std::ratio<1, 100>>> hundredths;

std::string
text (char const *value)
{
  return value;
}
} // namespace

TEST (LumexProtocolLargeTableTest,
      GivenARowListOf100Rows_WhenDefined_ThenTheProtocolHas100Commands)
{
  EXPECT_EQ (dsl::command_count<large>::value, 100u);
  EXPECT_EQ (dsl::command_entries<large> ().size (), 100u);
}

TEST (LumexProtocolLargeTableTest,
      GivenARowListOf100Rows_WhenListed_ThenEveryRowKeepsItsPosition)
{
  typedef dsl::commands_of<large>::type commands;
  EXPECT_TRUE ((
      std::is_same<dsl::list_at<0, commands>::type, large::CMD_000_t>::value));
  EXPECT_TRUE ((std::is_same<dsl::list_at<57, commands>::type,
                             large::CMD_057_t>::value));
  EXPECT_TRUE ((std::is_same<dsl::list_at<99, commands>::type,
                             large::CMD_099_t>::value));
  auto const entries = dsl::command_entries<large> ();
  for (std::size_t index = 0; index < entries.size (); ++index)
    {
      char expected[8];
      expected[0] = 'C';
      expected[1] = 'M';
      expected[2] = 'D';
      expected[3] = '_';
      expected[4] = static_cast<char> ('0' + index / 100);
      expected[5] = static_cast<char> ('0' + index / 10 % 10);
      expected[6] = static_cast<char> ('0' + index % 10);
      expected[7] = '\0';
      EXPECT_EQ (text (entries[index].identifier), text (expected));
      EXPECT_EQ (entries[index].key.integer, 0x100u + index);
    }
}

TEST (
    LumexProtocolLargeTableTest,
    GivenRowsOfTheLargeList_WhenRead_ThenFieldsNamesAndOptionsAreThoseOfTheRow)
{
  EXPECT_EQ (large::CMD_000_t::fields_type::count::value, 2u);
  EXPECT_EQ (text (large::CMD_000_t::field_names ()[1]), "value");
  EXPECT_TRUE (
      (std::is_same<large::CMD_000_t::reply_type, tables::wide_t>::value));
  EXPECT_EQ (large::CMD_001_t::fields_type::count::value, 0u);
  EXPECT_EQ (dsl::get_option<dsl::kind::timeout> (large::CMD_093_t::options ())
                 .milliseconds,
             193u);
  EXPECT_EQ (dsl::flags_of (large::CMD_097_t::options ()),
             static_cast<unsigned> (dsl::row_flags::idempotent));
}

TEST (LumexProtocolLargeTableTest,
      GivenALayoutOf40ScaledFields_WhenDefined_ThenItHas40FieldsAndNames)
{
  EXPECT_EQ (tables::wide_t::fields_type::count::value, 40u);
  dsl::names_t const names = tables::wide_t::field_names ();
  ASSERT_EQ (names.size (), 40u);
  EXPECT_EQ (text (names[0]), "f00");
  EXPECT_EQ (text (names[39]), "f39");
  typedef tables::wide_t::fields_type::types types;
  EXPECT_TRUE (
      (std::is_same<dsl::list_at<0, types>::type, hundredths>::value));
  EXPECT_TRUE (
      (std::is_same<dsl::list_at<39, types>::type, hundredths>::value));
  EXPECT_TRUE ((std::is_same<dsl::list_at<39, types>::type::scale_type,
                             std::ratio<1, 100>>::value));
}

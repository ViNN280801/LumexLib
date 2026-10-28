// LUMEX_PROTOCOL_DEFINE and LUMEX_PROTOCOL_LAYOUT (lumex/core/protocol/dsl):
// what a row generates (identifier, specification name, description, field
// types and names, key, reply), the command list of a protocol, the traits
// and the client methods named after the rows.

#include <cstring>
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
using v1 = tables::widget_v1;

std::string
name_at (dsl::names_t const &names, std::size_t index)
{
  return names[index];
}

// A client that records which row a generated method invoked, with which
// arguments.
struct recorder : v1::lumex_methods<recorder>
{
  std::string row;
  int sum = 0;

  template <typename Row, typename... Args>
  std::string
  lumex_invoke (Args... args)
  {
    row = Row::identifier ();
    int const values[] = { 0, args... };
    for (int value : values)
      sum += value;
    return Row::specification_name ();
  }
};
} // namespace

TEST (LumexProtocolDefineTest,
      GivenAnXRow_WhenNamed_ThenIdentifierAndSpecificationNameAreTheIdentifier)
{
  EXPECT_STREQ (v1::PING_t::identifier (), "PING");
  EXPECT_STREQ (v1::PING_t::specification_name (), "PING");
  EXPECT_STREQ (v1::SET_RATE_t::identifier (), "SET_RATE");
  EXPECT_STREQ (v1::SET_RATE_t::specification_name (), "SET_RATE");
}

TEST (LumexProtocolDefineTest,
      GivenAnXAsRow_WhenNamed_ThenTheSpecificationNameIsTheGivenString)
{
  EXPECT_STREQ (v1::ERASE_LOG_t::identifier (), "ERASE_LOG");
  EXPECT_STREQ (v1::ERASE_LOG_t::specification_name (), "DELETE");
  EXPECT_STREQ (v1::READ_TEXT_t::identifier (), "READ_TEXT");
  EXPECT_STREQ (v1::READ_TEXT_t::specification_name (), "read-text");
}

TEST (LumexProtocolDefineTest,
      GivenRows_WhenDescribed_ThenTheDescriptionIsTheTableText)
{
  EXPECT_STREQ (v1::PING_t::description (), "Liveness check");
  EXPECT_STREQ (v1::ERASE_LOG_t::description (), "Erase the log");
  EXPECT_STREQ (v1::SAMPLE_t::description (), "Sample stream");
}

TEST (LumexProtocolDefineTest,
      GivenARowWithFields_WhenItsNamesAreRead_ThenTheyFollowTheFieldOrder)
{
  dsl::names_t const names = v1::SET_RATE_t::field_names ();
  ASSERT_EQ (names.size (), 2u);
  EXPECT_EQ (name_at (names, 0), "rate");
  EXPECT_EQ (name_at (names, 1), "channel");
  EXPECT_EQ (v1::SET_RATE_t::fields_type::count::value, 2u);
}

TEST (LumexProtocolDefineTest,
      GivenARowWithoutFields_WhenItsNamesAreRead_ThenThereAreNone)
{
  EXPECT_EQ (v1::PING_t::field_names ().size (), 0u);
  EXPECT_EQ (v1::PING_t::fields_type::count::value, 0u);
}

TEST (LumexProtocolDefineTest,
      GivenARow_WhenItsFieldTypesAreRead_ThenTheyAreTheTableTypesInOrder)
{
  using types = v1::SET_RATE_t::fields_type::types;
  EXPECT_TRUE ((
      std::is_same<types, dsl::type_list<
                              field::u16_le_<field::scale<std::ratio<1, 100>>>,
                              field::u8_<>>>::value));
  EXPECT_TRUE (
      (std::is_same<v1::PING_t::fields_type::types, dsl::type_list<>>::value));
}

TEST (LumexProtocolDefineTest,
      GivenRowKeys_WhenRead_ThenIntegerCharacterAndTextKeysKeepTheirValue)
{
  EXPECT_TRUE ((std::is_same<v1::PING_t::key_type, int>::value));
  EXPECT_EQ (v1::PING_t::key (), 0x01);
  EXPECT_TRUE ((std::is_same<v1::SAMPLE_t::key_type, char>::value));
  EXPECT_EQ (v1::SAMPLE_t::key (), 'S');
  EXPECT_TRUE (
      (std::is_same<v1::READ_TEXT_t::key_type, dsl::text_key>::value));
  EXPECT_EQ (v1::READ_TEXT_t::key ().size, 2u);
  EXPECT_EQ (std::string (v1::READ_TEXT_t::key ().text, 2), "RT");
}

TEST (LumexProtocolDefineTest,
      GivenReplyGroups_WhenRead_ThenEmptyIsNoReplyAndOneTypeIsThatType)
{
  EXPECT_TRUE ((std::is_same<v1::PING_t::reply_type, dsl::no_reply>::value));
  EXPECT_TRUE (
      (std::is_same<v1::READ_STATUS_t::reply_type, tables::status_t>::value));
}

TEST (LumexProtocolDefineTest,
      GivenALayout_WhenRead_ThenItIsANamedFieldWithItsOwnFieldsAndNames)
{
  EXPECT_STREQ (tables::status_t::layout_name (), "status_t");
  EXPECT_TRUE (field::is_field<tables::status_t>::value);
  EXPECT_TRUE (dsl::is_layout<tables::status_t>::value);
  dsl::names_t const names = tables::status_t::field_names ();
  ASSERT_EQ (names.size (), 3u);
  EXPECT_EQ (name_at (names, 0), "mode");
  EXPECT_EQ (name_at (names, 1), "temperature");
  EXPECT_EQ (name_at (names, 2), "counter");
  EXPECT_TRUE ((std::is_same<
                tables::status_t::fields_type::types,
                dsl::type_list<field::u8_<>,
                               field::u16_le_<field::scale<std::ratio<1, 10>>>,
                               field::u16_be_<>>>::value));
}

TEST (LumexProtocolDefineTest,
      GivenAProtocol_WhenItsCommandsAreListed_ThenTheyFollowTheTableOrder)
{
  using commands = dsl::commands_of<v1>::type;
  EXPECT_TRUE (
      (std::is_same<commands,
                    dsl::type_list<v1::PING_t, v1::READ_STATUS_t,
                                   v1::SET_RATE_t, v1::ERASE_LOG_t,
                                   v1::READ_TEXT_t, v1::SAMPLE_t>>::value));
  EXPECT_EQ (dsl::command_count<v1>::value, 6u);
}

TEST (LumexProtocolDefineTest,
      GivenAProtocol_WhenInspected_ThenItsNameCodecAndHeaderAreTheDefinition)
{
  EXPECT_STREQ (v1::protocol_name (), "widget_v1");
  EXPECT_TRUE ((std::is_same<v1::framing_type, tables::widget_codec>::value));
  EXPECT_TRUE ((std::is_same<v1::header_type, tables::widget_header>::value));
  EXPECT_TRUE ((std::is_same<v1::base_protocol, void>::value));
}

TEST (LumexProtocolDefineTest,
      GivenAnEmptyRowList_WhenDefined_ThenTheProtocolHasNoCommands)
{
  EXPECT_EQ (dsl::command_count<tables::empty_protocol>::value, 0u);
  EXPECT_STREQ (tables::empty_protocol::protocol_name (), "empty_protocol");
}

TEST (LumexProtocolDefineTest,
      GivenTypes_WhenAskedWhatTheyAre_ThenCommandsProtocolsAndLayoutsAreTold)
{
  EXPECT_TRUE (dsl::is_command<v1::PING_t>::value);
  EXPECT_FALSE (dsl::is_command<v1>::value);
  EXPECT_FALSE (dsl::is_command<tables::status_t>::value);
  EXPECT_TRUE (dsl::is_protocol<v1>::value);
  EXPECT_TRUE (dsl::is_protocol<tables::widget_v3>::value);
  EXPECT_FALSE (dsl::is_protocol<v1::PING_t>::value);
  EXPECT_FALSE (dsl::is_layout<v1::PING_t>::value);
  EXPECT_FALSE (dsl::is_command<int>::value);
}

TEST (LumexProtocolDefineTest,
      GivenRowNames_WhenUsedInConstantExpressions_ThenTheyAreConstants)
{
  static_assert (v1::PING_t::key () == 0x01, "key is a constant");
  static_assert (v1::SAMPLE_t::key () == 'S', "key is a constant");
  static_assert (v1::READ_TEXT_t::key ().size == 2, "text key size");
  static_assert (v1::ERASE_LOG_t::specification_name ()[0] == 'D',
                 "names are constants");
  SUCCEED ();
}

TEST (LumexProtocolDefineTest,
      GivenTheClientMixin_WhenARowMethodIsCalled_ThenItInvokesThatRow)
{
  recorder client;
  EXPECT_EQ (client.PING (), "PING");
  EXPECT_EQ (client.row, "PING");
  EXPECT_EQ (client.SET_RATE (20, 3), "SET_RATE");
  EXPECT_EQ (client.row, "SET_RATE");
  EXPECT_EQ (client.sum, 23);
  EXPECT_EQ (client.ERASE_LOG (), "DELETE");
  EXPECT_EQ (client.row, "ERASE_LOG");
}

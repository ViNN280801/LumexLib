// LUMEX_PROTOCOL_EXTEND and LUMEX_PROTOCOL_EXTEND_FRAMED
// (lumex/core/protocol/dsl): a layer overrides rows of its base in place,
// adds new rows after them, keeps the other rows of the base, works over
// several levels, and its client methods follow its own rows.

#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolTestTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace tables = lumex_tests::protocol_tables;
typedef tables::widget_v1 v1;
typedef tables::widget_v2 v2;
typedef tables::widget_v3 v3;

template <typename Protocol>
struct key_recorder : Protocol::template lumex_methods<key_recorder<Protocol>>
{
  template <typename Row, typename... Args>
  int
  lumex_invoke (Args...)
  {
    return static_cast<int> (Row::key ());
  }
};
} // namespace

TEST (LumexProtocolExtendTest,
      GivenALayer_WhenItOverridesARow_ThenTheLayerRowIsADifferentType)
{
  EXPECT_FALSE ((std::is_same<v2::SET_RATE_t, v1::SET_RATE_t>::value));
  EXPECT_EQ (v2::SET_RATE_t::key (), 0x13);
  EXPECT_EQ (v1::SET_RATE_t::key (), 0x03);
  EXPECT_EQ (v2::SET_RATE_t::fields_type::count::value, 3u);
  EXPECT_STREQ (v2::SET_RATE_t::description (), "Set the sample rate (wide)");
}

TEST (LumexProtocolExtendTest,
      GivenALayer_WhenARowIsNotOverridden_ThenItIsTheBaseType)
{
  EXPECT_TRUE ((std::is_same<v2::PING_t, v1::PING_t>::value));
  EXPECT_TRUE ((std::is_same<v2::ERASE_LOG_t, v1::ERASE_LOG_t>::value));
}

TEST (LumexProtocolExtendTest,
      GivenALayer_WhenListed_ThenOverridesKeepTheBasePositionAndNewRowsFollow)
{
  EXPECT_TRUE ((std::is_same<
                dsl::commands_of<v2>::type,
                dsl::type_list<v1::PING_t, v1::READ_STATUS_t, v2::SET_RATE_t,
                               v1::ERASE_LOG_t, v1::READ_TEXT_t, v1::SAMPLE_t,
                               v2::CALIBRATE_t>>::value));
  EXPECT_EQ (dsl::command_count<v2>::value, 7u);
}

TEST (LumexProtocolExtendTest,
      GivenALayer_WhenTheBaseIsListed_ThenTheBaseIsUnchanged)
{
  EXPECT_TRUE (
      (std::is_same<dsl::commands_of<v1>::type,
                    dsl::type_list<v1::PING_t, v1::READ_STATUS_t,
                                   v1::SET_RATE_t, v1::ERASE_LOG_t,
                                   v1::READ_TEXT_t, v1::SAMPLE_t>>::value));
}

TEST (LumexProtocolExtendTest,
      GivenTwoLevelsOfLayers_WhenListed_ThenEveryRowIsTheMostDerivedOne)
{
  EXPECT_TRUE ((std::is_same<
                dsl::commands_of<v3>::type,
                dsl::type_list<v3::PING_t, v1::READ_STATUS_t, v2::SET_RATE_t,
                               v1::ERASE_LOG_t, v1::READ_TEXT_t, v1::SAMPLE_t,
                               v2::CALIBRATE_t, v3::SELF_TEST_t>>::value));
  EXPECT_EQ (v3::PING_t::key (), 0x31);
  EXPECT_TRUE ((std::is_same<v3::SET_RATE_t, v2::SET_RATE_t>::value));
}

TEST (LumexProtocolExtendTest,
      GivenALayer_WhenInspected_ThenItNamesItselfAndItsBase)
{
  EXPECT_STREQ (v2::protocol_name (), "widget_v2");
  EXPECT_STREQ (v3::protocol_name (), "widget_v3");
  EXPECT_TRUE ((std::is_same<v2::base_protocol, v1>::value));
  EXPECT_TRUE ((std::is_same<v3::base_protocol, v2>::value));
  EXPECT_TRUE (dsl::is_protocol<v2>::value);
}

TEST (LumexProtocolExtendTest,
      GivenLayers_WhenTheirCodecsAreRead_ThenExtendKeepsAndFramedReplacesThem)
{
  EXPECT_TRUE ((std::is_same<v2::framing_type, tables::widget_codec>::value));
  EXPECT_TRUE ((std::is_same<v3::framing_type, tables::other_codec>::value));
  EXPECT_TRUE ((std::is_same<v3::header_type, tables::widget_header>::value));
}

TEST (LumexProtocolExtendTest,
      GivenALayerClient_WhenMethodsAreCalled_ThenEachInvokesTheLayersRow)
{
  key_recorder<v1> base;
  key_recorder<v2> layer;
  key_recorder<v3> top;
  EXPECT_EQ (base.SET_RATE (1, 2), 0x03);
  EXPECT_EQ (layer.SET_RATE (1, 2, 3), 0x13);
  EXPECT_EQ (layer.PING (), 0x01);
  EXPECT_EQ (layer.CALIBRATE (5), 0x20);
  EXPECT_EQ (top.PING (), 0x31);
  EXPECT_EQ (top.SET_RATE (), 0x13);
  EXPECT_EQ (top.SELF_TEST (), 0x32);
  EXPECT_EQ (top.ERASE_LOG (), 0x04);
}

// The row options (lumex/core/protocol/dsl/LumexProtocolOptions.hpp): every
// option of a table row is a typed constant that has_option, get_option and
// flags_of read back, in constant expressions too.

#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolTestTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace kind = lumex::core::protocol::dsl::kind;
namespace tables = lumex_tests::protocol_tables;
typedef tables::widget_v1 v1;
typedef dsl::row_flags flags;
} // namespace

TEST (LumexProtocolOptionsTest,
      GivenTimeoutAndRetries_WhenRead_ThenTheTableValuesComeBack)
{
  EXPECT_TRUE (dsl::has_option<kind::timeout> (v1::SET_RATE_t::options ()));
  EXPECT_EQ (
      dsl::get_option<kind::timeout> (v1::SET_RATE_t::options ()).milliseconds,
      500u);
  EXPECT_EQ (dsl::get_option<kind::retries> (v1::SET_RATE_t::options ()).count,
             2u);
}

TEST (LumexProtocolOptionsTest,
      GivenAcceptedIn_WhenRead_ThenTheStatesKeepTheirEnumerationType)
{
  typedef decltype (dsl::get_option<kind::accepted_in> (
      v1::SET_RATE_t::options ())) accepted;
  EXPECT_TRUE (
      (std::is_same<accepted::state_type, tables::widget_state>::value));
  accepted const value
      = dsl::get_option<kind::accepted_in> (v1::SET_RATE_t::options ());
  EXPECT_EQ (sizeof (value.states) / sizeof (value.states[0]), 2u);
  EXPECT_TRUE (value.states[0] == tables::widget_state::idle);
  EXPECT_TRUE (value.states[1] == tables::widget_state::service);
}

TEST (LumexProtocolOptionsTest,
      GivenStreamLogLevelFirmwareAndModel_WhenRead_ThenTheTableValuesComeBack)
{
  EXPECT_EQ (
      dsl::get_option<kind::stream> (v1::SAMPLE_t::options ()).interval_ms,
      100u);
  EXPECT_TRUE (
      dsl::get_option<kind::log_level> (v1::READ_STATUS_t::options ()).value
      == dsl::level::trace);
  dsl::firmware_t const firmware
      = dsl::get_option<kind::firmware> (v1::ERASE_LOG_t::options ());
  EXPECT_EQ (firmware.from, 100u);
  EXPECT_EQ (firmware.to, 199u);
  dsl::model_t<2> const models
      = dsl::get_option<kind::model> (v1::READ_TEXT_t::options ());
  EXPECT_EQ (std::string (models.names[0]), "W1");
  EXPECT_EQ (std::string (models.names[1]), "W2");
}

TEST (LumexProtocolOptionsTest,
      GivenFlags_WhenCombined_ThenEachRowHasExactlyItsFlags)
{
  EXPECT_EQ (dsl::flags_of (v1::PING_t::options ()),
             static_cast<unsigned> (flags::idempotent));
  EXPECT_EQ (dsl::flags_of (v1::ERASE_LOG_t::options ()),
             static_cast<unsigned> (flags::dangerous));
  EXPECT_EQ (dsl::flags_of (v1::READ_TEXT_t::options ()),
             static_cast<unsigned> (flags::deprecated));
  EXPECT_EQ (dsl::flags_of (v1::SET_RATE_t::options ()), 0u);
  EXPECT_EQ (
      dsl::flags_of (dsl::make_options (flags::idempotent, flags::dangerous)),
      static_cast<unsigned> (flags::idempotent | flags::dangerous));
}

TEST (LumexProtocolOptionsTest,
      GivenARowWithoutAnOption_WhenAsked_ThenHasOptionIsFalse)
{
  EXPECT_FALSE (dsl::has_option<kind::timeout> (v1::PING_t::options ()));
  EXPECT_FALSE (dsl::has_option<kind::flag> (v1::SET_RATE_t::options ()));
  EXPECT_FALSE (dsl::has_option<kind::model> (v1::SAMPLE_t::options ()));
  EXPECT_TRUE (dsl::has_option<kind::flag> (v1::PING_t::options ()));
  EXPECT_FALSE ((
      dsl::has_option_type<kind::retries, v1::SAMPLE_t::options_type>::value));
}

TEST (LumexProtocolOptionsTest,
      GivenOptions_WhenUsedInConstantExpressions_ThenTheyAreConstants)
{
  static_assert (
      dsl::get_option<kind::timeout> (v1::SET_RATE_t::options ()).milliseconds
          == 500,
      "timeout is a constant");
  static_assert (
      (dsl::flags_of (v1::ERASE_LOG_t::options ()) & flags::dangerous) != 0,
      "flags are constants");
  static_assert (
      dsl::get_option<kind::accepted_in> (v1::SET_RATE_t::options ()).states[1]
          == tables::widget_state::service,
      "states are constants");
  static_assert (!dsl::has_option<kind::stream> (v1::PING_t::options ()),
                 "absence is a constant");
  SUCCEED ();
}

TEST (LumexProtocolOptionsTest,
      GivenTheOptionTypes_WhenRead_ThenTheRowsKeepTheOrderOfTheTable)
{
  EXPECT_TRUE (
      (std::is_same<v1::SET_RATE_t::options_type,
                    dsl::option_set<
                        dsl::timeout_t, dsl::retries_t,
                        dsl::accepted_in_t<tables::widget_state, 2>>>::value));
  EXPECT_TRUE ((std::is_same<v1::PING_t::options_type,
                             dsl::option_set<flags::value>>::value));
  EXPECT_TRUE ((std::is_same<tables::widget_v3::PING_t::options_type,
                             dsl::option_set<>>::value));
}

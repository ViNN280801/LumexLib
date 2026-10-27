// The session context (lumex/core/protocol/index/LumexProtocolSession.hpp):
// what identification learned, the view the decoders read, and the filters
// by firmware and model.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace index = lumex::core::protocol::index;

index::row_t const &
flash_row ()
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget_v2> ();
  EXPECT_TRUE (result.has_value ());
  index::row_t const *const row = result->find_name ("FLASH");
  EXPECT_NE (row, nullptr);
  return *row;
}
} // namespace

TEST (LumexProtocolSessionTest,
      GivenANewContext_WhenAsked_ThenNothingIsIdentified)
{
  index::session_context_t const session;
  EXPECT_TRUE (session.model ().empty ());
  EXPECT_TRUE (session.layer ().empty ());
  EXPECT_EQ (session.firmware (), 0u);
  std::int64_t value = 0;
  EXPECT_FALSE (session.find ("anything", value));
  EXPECT_FALSE (session.find ("firmware", value));
}

TEST (LumexProtocolSessionTest,
      GivenIdentificationResults_WhenSet_ThenTheContextHoldsThem)
{
  index::session_context_t session;
  session.set_model ("A200");
  session.set_firmware (5);
  session.set_layer ("widget_v2");
  EXPECT_EQ (session.model (), "A200");
  EXPECT_EQ (session.firmware (), 5u);
  EXPECT_EQ (session.layer (), "widget_v2");
  std::int64_t value = 0;
  ASSERT_TRUE (session.find ("firmware", value));
  EXPECT_EQ (value, 5);
}

TEST (LumexProtocolSessionTest,
      GivenNamedValues_WhenSetReplacedAndErased_ThenTheLatestIsRead)
{
  index::session_context_t session;
  session.set ("thermostat", 2);
  session.set ("valve", 7);
  session.set ("thermostat", 3);
  std::int64_t value = 0;
  ASSERT_TRUE (session.find ("thermostat", value));
  EXPECT_EQ (value, 3);
  ASSERT_TRUE (session.find ("valve", value));
  EXPECT_EQ (value, 7);
  EXPECT_TRUE (session.erase ("valve"));
  EXPECT_FALSE (session.erase ("valve"));
  EXPECT_FALSE (session.find ("valve", value));
  EXPECT_TRUE (session.find ("thermostat", value));
}

TEST (LumexProtocolSessionTest,
      GivenANamedValueCalledFirmware_WhenAsked_ThenTheNamedValueWins)
{
  index::session_context_t session;
  session.set_firmware (5);
  session.set ("firmware", 9);
  std::int64_t value = 0;
  ASSERT_TRUE (session.find ("firmware", value));
  EXPECT_EQ (value, 9);
}

TEST (LumexProtocolSessionTest,
      GivenAFilledContext_WhenCleared_ThenItIsEmptyAgain)
{
  index::session_context_t session;
  session.set_model ("A");
  session.set_firmware (3);
  session.set_layer ("x");
  session.set ("k", 1);
  session.clear ();
  std::int64_t value = 0;
  EXPECT_TRUE (session.model ().empty ());
  EXPECT_TRUE (session.layer ().empty ());
  EXPECT_EQ (session.firmware (), 0u);
  EXPECT_FALSE (session.find ("k", value));
}

TEST (LumexProtocolSessionTest,
      GivenTheDecodeView_WhenAKeyIsLookedUp_ThenItAnswersFromTheContext)
{
  index::session_context_t session;
  session.set ("kind", -4);
  lumex::core::protocol::field::decode_context_t const view
      = session.decode_context ();
  std::int64_t value = 0;
  ASSERT_TRUE (view.find ("kind", value));
  EXPECT_EQ (value, -4);
  EXPECT_FALSE (view.find ("other", value));
  // The view follows later changes of the context.
  session.set ("other", 8);
  EXPECT_TRUE (view.find ("other", value));
  EXPECT_EQ (value, 8);
}

TEST (
    LumexProtocolSessionTest,
    GivenARowLimitedToAFirmwareRangeAndAModel_WhenTheContextFilters_ThenBothMustMatch)
{
  index::row_t const &flash = flash_row ();
  index::session_context_t session;
  // Nothing identified: the row stays available.
  EXPECT_TRUE (session.available (*flash.entry));
  session.set_firmware (2);
  session.set_model ("A");
  EXPECT_TRUE (session.available (*flash.entry));
  session.set_firmware (9);
  EXPECT_TRUE (session.available (*flash.entry));
  session.set_firmware (10);
  EXPECT_FALSE (session.available (*flash.entry));
  session.set_firmware (1);
  EXPECT_FALSE (session.available (*flash.entry));
  session.set_firmware (5);
  session.set_model ("B");
  EXPECT_FALSE (session.available (*flash.entry));
}

TEST (LumexProtocolSessionTest,
      GivenARowWithoutRanges_WhenTheContextFilters_ThenItIsAlwaysAvailable)
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget> ();
  ASSERT_TRUE (result.has_value ());
  index::session_context_t session;
  session.set_firmware (99);
  session.set_model ("Z");
  EXPECT_TRUE (session.available (*result->find_name ("PING")->entry));
}

TEST (LumexProtocolSessionTest,
      GivenARegistryEntry_WhenTheContextFilters_ThenFirmwareAndModelDecide)
{
  lumex::core::protocol::registry::registry_entry_t const &frequency
      = tables::lamp::entries ().data ()[0];
  lumex::core::protocol::registry::registry_entry_t const &offset
      = tables::lamp::entries ().data ()[2];
  index::session_context_t session;
  EXPECT_TRUE (session.available (frequency));
  session.set_firmware (8);
  EXPECT_FALSE (session.available (frequency));
  session.set_firmware (4);
  EXPECT_TRUE (session.available (frequency));
  session.set_model ("C");
  EXPECT_FALSE (session.available (offset));
  session.set_model ("B");
  EXPECT_TRUE (session.available (offset));
}

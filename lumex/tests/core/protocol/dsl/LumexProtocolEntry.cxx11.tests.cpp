// The run-time entries of the rows (lumex/core/protocol/dsl/
// LumexProtocolEntry.hpp): one entry per command in list order, with the
// names, the key, the field names and the options as plain values.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolTestTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace tables = lumex_tests::protocol_tables;
using v1 = tables::widget_v1;

using entries_t = lumex::core::span::view::span<dsl::command_entry_t const>;

std::string
text (char const *value)
{
  return value;
}
} // namespace

TEST (LumexProtocolEntryTest,
      GivenAProtocol_WhenItsEntriesAreRead_ThenThereIsOnePerCommandInOrder)
{
  entries_t const entries = dsl::command_entries<v1> ();
  ASSERT_EQ (entries.size (), 6u);
  EXPECT_EQ (text (entries[0].identifier), "PING");
  EXPECT_EQ (text (entries[1].identifier), "READ_STATUS");
  EXPECT_EQ (text (entries[2].identifier), "SET_RATE");
  EXPECT_EQ (text (entries[3].identifier), "ERASE_LOG");
  EXPECT_EQ (text (entries[4].identifier), "READ_TEXT");
  EXPECT_EQ (text (entries[5].identifier), "SAMPLE");
}

TEST (LumexProtocolEntryTest,
      GivenAnXAsRow_WhenItsEntryIsRead_ThenItHasBothNamesAndTheDescription)
{
  dsl::command_entry_t const entry = dsl::make_entry<v1::ERASE_LOG_t> ();
  EXPECT_EQ (text (entry.identifier), "ERASE_LOG");
  EXPECT_EQ (text (entry.specification_name), "DELETE");
  EXPECT_EQ (text (entry.description), "Erase the log");
  EXPECT_TRUE (entry.has_reply);
}

TEST (LumexProtocolEntryTest,
      GivenRowKeys_WhenEntriesAreRead_ThenIntegerAndTextKeysAreKept)
{
  dsl::command_entry_t const ping = dsl::make_entry<v1::PING_t> ();
  EXPECT_TRUE (ping.key.kind == dsl::key_kind::integer);
  EXPECT_EQ (ping.key.integer, 1u);
  dsl::command_entry_t const sample = dsl::make_entry<v1::SAMPLE_t> ();
  EXPECT_TRUE (sample.key.kind == dsl::key_kind::integer);
  EXPECT_EQ (sample.key.integer, static_cast<std::uint64_t> ('S'));
  dsl::command_entry_t const read_text = dsl::make_entry<v1::READ_TEXT_t> ();
  EXPECT_TRUE (read_text.key.kind == dsl::key_kind::text);
  EXPECT_EQ (std::string (read_text.key.text, read_text.key.text_size), "RT");
  EXPECT_FALSE (read_text.has_reply);
}

TEST (LumexProtocolEntryTest,
      GivenARowWithFields_WhenItsEntryIsRead_ThenItHasTheFieldNames)
{
  dsl::command_entry_t const entry = dsl::make_entry<v1::SET_RATE_t> ();
  ASSERT_EQ (entry.field_count, 2u);
  ASSERT_EQ (entry.field_names.size (), 2u);
  EXPECT_EQ (text (entry.field_names[0]), "rate");
  EXPECT_EQ (text (entry.field_names[1]), "channel");
}

TEST (LumexProtocolEntryTest,
      GivenRowOptions_WhenEntriesAreRead_ThenTheViewHasEveryValue)
{
  dsl::options_view_t const rate = dsl::make_entry<v1::SET_RATE_t> ().options;
  EXPECT_TRUE (rate.has_timeout);
  EXPECT_EQ (rate.timeout_ms, 500u);
  EXPECT_TRUE (rate.has_retries);
  EXPECT_EQ (rate.retries, 2u);
  EXPECT_FALSE (rate.has_stream);
  ASSERT_EQ (rate.accepted_states.size (), 2u);
  EXPECT_EQ (rate.accepted_states[0],
             static_cast<std::int64_t> (tables::widget_state::idle));
  EXPECT_EQ (rate.accepted_states[1],
             static_cast<std::int64_t> (tables::widget_state::service));

  dsl::options_view_t const erase
      = dsl::make_entry<v1::ERASE_LOG_t> ().options;
  EXPECT_TRUE (erase.has_firmware);
  EXPECT_EQ (erase.firmware_from, 100u);
  EXPECT_EQ (erase.firmware_to, 199u);
  EXPECT_EQ (erase.flags, static_cast<unsigned> (dsl::row_flags::dangerous));

  dsl::options_view_t const read_text
      = dsl::make_entry<v1::READ_TEXT_t> ().options;
  ASSERT_EQ (read_text.models.size (), 2u);
  EXPECT_EQ (text (read_text.models[0]), "W1");
  EXPECT_EQ (text (read_text.models[1]), "W2");

  dsl::options_view_t const sample = dsl::make_entry<v1::SAMPLE_t> ().options;
  EXPECT_TRUE (sample.has_stream);
  EXPECT_EQ (sample.stream_interval_ms, 100u);

  dsl::options_view_t const status
      = dsl::make_entry<v1::READ_STATUS_t> ().options;
  EXPECT_TRUE (status.has_log_level);
  EXPECT_TRUE (status.log_level == dsl::level::trace);
}

TEST (LumexProtocolEntryTest,
      GivenARowWithoutOptions_WhenItsEntryIsRead_ThenNothingIsSet)
{
  dsl::options_view_t const view
      = dsl::make_entry<tables::widget_v3::PING_t> ().options;
  EXPECT_FALSE (view.has_timeout);
  EXPECT_FALSE (view.has_retries);
  EXPECT_FALSE (view.has_stream);
  EXPECT_FALSE (view.has_log_level);
  EXPECT_FALSE (view.has_firmware);
  EXPECT_EQ (view.timeout_ms, 0u);
  EXPECT_EQ (view.flags, 0u);
  EXPECT_TRUE (view.log_level == dsl::level::debug);
  EXPECT_EQ (view.models.size (), 0u);
  EXPECT_EQ (view.accepted_states.size (), 0u);
}

TEST (LumexProtocolEntryTest,
      GivenALayer_WhenItsEntriesAreRead_ThenOverriddenRowsShowTheLayerRow)
{
  entries_t const entries = dsl::command_entries<tables::widget_v2> ();
  ASSERT_EQ (entries.size (), 7u);
  EXPECT_EQ (text (entries[2].identifier), "SET_RATE");
  EXPECT_EQ (entries[2].key.integer, 0x13u);
  EXPECT_EQ (entries[2].field_count, 3u);
  EXPECT_EQ (entries[2].options.timeout_ms, 800u);
  EXPECT_EQ (text (entries[6].identifier), "CALIBRATE");
  EXPECT_EQ (dsl::command_entries<v1> ()[2].key.integer, 0x03u);
}

TEST (LumexProtocolEntryTest,
      GivenTheEntries_WhenReadTwice_ThenTheSameArrayIsReturned)
{
  EXPECT_EQ (dsl::command_entries<v1> ().data (),
             dsl::command_entries<v1> ().data ());
  EXPECT_NE (dsl::command_entries<v1> ().data (),
             dsl::command_entries<tables::widget_v2> ().data ());
}

TEST (LumexProtocolEntryTest,
      GivenAnEmptyProtocol_WhenItsEntriesAreRead_ThenThereAreNone)
{
  EXPECT_EQ (dsl::command_entries<tables::empty_protocol> ().size (), 0u);
}

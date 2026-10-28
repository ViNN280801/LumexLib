// The run-time index (lumex/core/protocol/index): the rows of a layer with
// their names, keys, field descriptors, replies and options; lookup by key,
// name and received frame; layers; the key readers; duplicate keys.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace index = lumex::core::protocol::index;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes_of;
using tables::view_of;

index::protocol_index_t const &
widget_index ()
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

index::row_t const &
row_named (index::protocol_index_t const &where, char const *name)
{
  index::row_t const *const row = where.find_name (name);
  EXPECT_NE (row, nullptr) << name;
  return row != nullptr ? *row : where.rows ().data ()[0];
}

std::vector<std::uint8_t>
bytes (std::initializer_list<int> values)
{
  std::vector<std::uint8_t> out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

// The key of a frame in the second byte, little endian over two bytes.
struct second_byte_header
{
  using key_reader = index::key_at<1, 2, index::key_byte_order::little>;
  static constexpr std::size_t key_width = 2;
};

// clang-format off
#define OFFSET_ROWS(X, X_AS) \
  X (FIRST, 0x0102, "First", (u8_), ("value"), (), ()) \
  X (SECOND, 0x0A0B, "Second", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (offset_protocol, tables::codec, second_byte_header,
                       OFFSET_ROWS);

struct hex_header
{
  using layout_type = layout::fixed_width_text_layout<16>;
  static constexpr std::size_t key_width = 1;
};

// A qualified base is fine; the row list is a macro and is never qualified.
LUMEX_PROTOCOL_EXTEND_FRAMED (widget_hex, tables::widget, tables::codec,
                              hex_header, NO_ROWS);
} // namespace

TEST (LumexProtocolIndexTest,
      GivenAProtocol_WhenIndexed_ThenTheRowsAreInTableOrderWithNames)
{
  index::protocol_index_t const &where = widget_index ();
  ASSERT_EQ (where.rows ().size (), 8u);
  EXPECT_STREQ (where.protocol_name (), "widget");
  EXPECT_STREQ (where.base_name (), "");
  EXPECT_STREQ (where.rows ().data ()[0].entry->identifier, "PING");
  EXPECT_STREQ (where.rows ().data ()[1].entry->identifier, "SET_SPEED");
  EXPECT_STREQ (where.rows ().data ()[1].entry->description, "Set the speed");
  EXPECT_STREQ (where.rows ().data ()[4].entry->identifier, "ERASE");
  EXPECT_STREQ (where.rows ().data ()[4].entry->specification_name, "DELETE");
  EXPECT_EQ (where.key_width (), 1u);
}

TEST (LumexProtocolIndexTest,
      GivenTheFieldsOfARow_WhenIndexed_ThenEveryFieldHasANameKindScaleAndUnit)
{
  index::row_t const &row = row_named (widget_index (), "SET_SPEED");
  ASSERT_EQ (row.request_count, 1u);
  index::field_descriptor_t const &speed = row.request[0];
  EXPECT_STREQ (speed.name, "speed");
  EXPECT_TRUE (speed.info.kind == field::field_kind::integer);
  EXPECT_EQ (speed.info.binary_size, 2u);
  EXPECT_TRUE (speed.info.order == field::byte_order::little);
  EXPECT_EQ (speed.info.scale_num, 1);
  EXPECT_EQ (speed.info.scale_den, 10);
  EXPECT_STREQ (speed.info.unit, "rpm");
  EXPECT_EQ (speed.child_count, 0u);
  EXPECT_STREQ (index::field_kind_name (speed.info.kind), "integer");
}

TEST (LumexProtocolIndexTest,
      GivenAGenericCommand_WhenIndexed_ThenItsFieldsNameTheirRegistry)
{
  index::row_t const &row = row_named (widget_index (), "SET_PARAMETER");
  ASSERT_EQ (row.request_count, 2u);
  EXPECT_TRUE (row.request[0].info.kind == field::field_kind::registry_key);
  EXPECT_STREQ (row.request[0].registry_name, "lamp");
  EXPECT_STREQ (row.request[0].name, "parameter");
  EXPECT_TRUE (row.request[1].info.kind == field::field_kind::registry_value);
  EXPECT_STREQ (row.request[1].registry_name, "lamp");
  EXPECT_EQ (row.request[0].info.count, 5u);
}

TEST (LumexProtocolIndexTest,
      GivenTheRepliesOfRows_WhenIndexed_ThenTheFormAndTheFieldsAreThere)
{
  index::protocol_index_t const &where = widget_index ();
  index::row_t const &ping = row_named (where, "PING");
  EXPECT_TRUE (ping.reply == index::reply_form::none);
  EXPECT_EQ (ping.reply_count, 0u);
  index::row_t const &status = row_named (where, "STATUS");
  EXPECT_TRUE (status.reply == index::reply_form::layout);
  EXPECT_STREQ (status.reply_layout, "state_t");
  ASSERT_EQ (status.reply_count, 2u);
  EXPECT_STREQ (status.reply_fields[0].name, "mode");
  EXPECT_TRUE (status.reply_fields[0].info.kind
               == field::field_kind::enumeration);
  EXPECT_STREQ (status.reply_fields[1].name, "level");
  index::row_t const &temperature = row_named (where, "READ_TEMP");
  EXPECT_TRUE (temperature.reply == index::reply_form::single_field);
  ASSERT_EQ (temperature.reply_count, 1u);
  EXPECT_STREQ (temperature.reply_fields[0].name, "reply");
  EXPECT_STREQ (temperature.reply_fields[0].info.unit, "degC");
  EXPECT_EQ (temperature.reply_fields[0].info.scale_den, 100);
}

TEST (LumexProtocolIndexTest,
      GivenAStreamRow_WhenIndexed_ThenItIsAnEventAndTheOthersAreNot)
{
  index::protocol_index_t const &where = widget_index ();
  EXPECT_TRUE (row_named (where, "TICK").is_event);
  EXPECT_FALSE (row_named (where, "STATUS").is_event);
  EXPECT_EQ (row_named (where, "TICK").entry->options.stream_interval_ms,
             100u);
}

TEST (LumexProtocolIndexTest,
      GivenTheOptionsOfRows_WhenIndexed_ThenTheEntryCarriesThem)
{
  index::protocol_index_t const &where = widget_index ();
  dsl::options_view_t const &speed
      = row_named (where, "SET_SPEED").entry->options;
  EXPECT_TRUE (speed.has_timeout);
  EXPECT_EQ (speed.timeout_ms, 500u);
  EXPECT_EQ (speed.retries, 2u);
  EXPECT_TRUE (row_named (where, "ERASE").entry->options.flags
               & dsl::row_flags::dangerous);
  EXPECT_EQ (
      row_named (where, "STATUS").entry->options.accepted_states.size (), 2u);
}

TEST (LumexProtocolIndexTest, GivenAnIntegerKey_WhenLookedUp_ThenTheRowIsFound)
{
  index::protocol_index_t const &where = widget_index ();
  index::row_t const *const row = where.find_integer (0x02);
  ASSERT_NE (row, nullptr);
  EXPECT_STREQ (row->entry->identifier, "SET_SPEED");
  EXPECT_EQ (where.find_integer (0x77), nullptr);
  EXPECT_EQ (where.find_integer (0x0102), nullptr);
}

TEST (LumexProtocolIndexTest,
      GivenAKeyOfTheKeyType_WhenLookedUp_ThenItIsCutToTheWireFirst)
{
  index::protocol_index_t const &where = widget_index ();
  // The keys of widget are ints on a one-byte wire.
  index::row_t const *const direct
      = where.find (dsl::key_value (dsl::make_key (0x03)));
  ASSERT_NE (direct, nullptr);
  EXPECT_STREQ (direct->entry->identifier, "STATUS");
  index::row_t const *const wider
      = where.find (dsl::key_value (dsl::make_key (0x0103)));
  ASSERT_NE (wider, nullptr);
  EXPECT_STREQ (wider->entry->identifier, "STATUS");
  EXPECT_EQ (where.find (dsl::key_value (dsl::make_key ("AZ"))), nullptr);
  EXPECT_EQ (where.find (dsl::key_value (dsl::make_key (1.5))), nullptr);
}

TEST (LumexProtocolIndexTest,
      GivenNames_WhenLookedUp_ThenTheSpecificationNameAndTheIdentifierBothFind)
{
  index::protocol_index_t const &where = widget_index ();
  index::row_t const *const by_spec = where.find_name ("DELETE");
  ASSERT_NE (by_spec, nullptr);
  EXPECT_STREQ (by_spec->entry->identifier, "ERASE");
  EXPECT_EQ (where.find_name ("ERASE"), by_spec);
  EXPECT_EQ (where.find_name ("MISSING"), nullptr);
}

TEST (LumexProtocolIndexTest,
      GivenABinaryFrame_WhenMatched_ThenTheKeyByteGivesTheRowAndTheOffset)
{
  index::protocol_index_t const &where = widget_index ();
  layout::bytes_t const frame = bytes ({ 0x02, 0x7B, 0x00 });
  index::frame_match_t const found = where.match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_TRUE (found.has_key);
  EXPECT_STREQ (found.row->entry->identifier, "SET_SPEED");
  EXPECT_EQ (found.payload_offset, 1u);
  EXPECT_EQ (found.key.integer, 0x02u);
}

TEST (LumexProtocolIndexTest,
      GivenAFrameWithAnUnknownKeyOrNoBytes_WhenMatched_ThenThereIsNoRow)
{
  index::protocol_index_t const &where = widget_index ();
  layout::bytes_t const unknown = bytes ({ 0x99, 0x01 });
  index::frame_match_t const none = where.match (view_of (unknown));
  EXPECT_EQ (none.row, nullptr);
  EXPECT_TRUE (none.has_key);
  EXPECT_EQ (none.key.integer, 0x99u);
  layout::bytes_t const empty;
  index::frame_match_t const nothing = where.match (view_of (empty));
  EXPECT_EQ (nothing.row, nullptr);
  EXPECT_FALSE (nothing.has_key);
}

TEST (LumexProtocolIndexTest,
      GivenATokenizedLayer_WhenAFrameIsMatched_ThenTheFirstTokenIsTheKey)
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget_text> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const frame = bytes_of ("2,12.5");
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "SET_SPEED");
  EXPECT_EQ (found.payload_offset, 0u);
  EXPECT_TRUE (found.key.kind == dsl::key_kind::text);
  // The spaces around the key are not part of it.
  layout::bytes_t const spaced = bytes_of (" 3 ,1");
  index::frame_match_t const again = result->match (view_of (spaced));
  ASSERT_NE (again.row, nullptr);
  EXPECT_STREQ (again.row->entry->identifier, "STATUS");
  // A frame of the key alone is the key.
  layout::bytes_t const alone = bytes_of ("1");
  ASSERT_NE (result->match (view_of (alone)).row, nullptr);
  EXPECT_STREQ (result->match (view_of (alone)).row->entry->identifier,
                "PING");
  layout::bytes_t const unknown = bytes_of ("77,1");
  EXPECT_EQ (result->match (view_of (unknown)).row, nullptr);
}

TEST (LumexProtocolIndexTest,
      GivenTextKeys_WhenAFrameIsMatched_ThenTheMnemonicFindsTheRow)
{
  index::index_result_t const &result
      = index::protocol_index<tables::washer> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const frame = bytes_of ("AZ,3");
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "AZ");
  layout::bytes_t const other = bytes_of ("BY");
  EXPECT_STREQ (result->match (view_of (other)).row->entry->identifier, "BY");
  EXPECT_NE (result->find (dsl::key_value (dsl::make_key ("BY"))), nullptr);
  EXPECT_EQ (result->find_text ("AZ", 1), nullptr);
  layout::bytes_t const lower = bytes_of ("az");
  EXPECT_EQ (result->match (view_of (lower)).row, nullptr);
}

TEST (LumexProtocolIndexTest,
      GivenACompositeKey_WhenLookedUp_ThenTheKeyAndTheFrameFindTheRow)
{
  index::index_result_t const &result
      = index::protocol_index<tables::pairs> ();
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result->key_width (), 2u);
  index::row_t const *const row = result->find (
      dsl::key_value (dsl::make_key (tables::pair_key (3, 6))));
  ASSERT_NE (row, nullptr);
  EXPECT_STREQ (row->entry->identifier, "COOL");
  layout::bytes_t const frame = bytes ({ 0x03, 0x05, 0x09 });
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "WARM");
  EXPECT_EQ (found.payload_offset, 2u);
}

TEST (LumexProtocolIndexTest,
      GivenALayerThatOverridesARow_WhenIndexed_ThenItReplacesTheRowInPlace)
{
  index::index_result_t const &base = index::protocol_index<tables::widget> ();
  index::index_result_t const &layer
      = index::protocol_index<tables::widget_v2> ();
  ASSERT_TRUE (base.has_value ());
  ASSERT_TRUE (layer.has_value ());
  EXPECT_EQ (layer->rows ().size (), 9u);
  EXPECT_STREQ (layer->protocol_name (), "widget_v2");
  EXPECT_STREQ (layer->base_name (), "widget");
  EXPECT_STREQ (layer->rows ().data ()[1].entry->identifier, "SET_SPEED");
  EXPECT_EQ (layer->rows ().data ()[1].request[0].info.scale_den, 100);
  EXPECT_EQ (base->rows ().data ()[1].request[0].info.scale_den, 10);
  EXPECT_STREQ (layer->rows ().data ()[8].entry->identifier, "FLASH");
  EXPECT_EQ (base->find_name ("FLASH"), nullptr);
  index::row_t const *const flash = layer->find_integer (0x07);
  ASSERT_NE (flash, nullptr);
  EXPECT_TRUE (flash->entry->options.has_firmware);
  EXPECT_EQ (flash->entry->options.models.size (), 1u);
}

TEST (LumexProtocolIndexTest,
      GivenAnIndexOfTheSameLayer_WhenAskedTwice_ThenItIsOneObject)
{
  EXPECT_EQ (&index::protocol_index<tables::widget> (),
             &index::protocol_index<tables::widget> ());
  EXPECT_NE (
      static_cast<void const *> (&index::protocol_index<tables::widget> ()),
      static_cast<void const *> (
          &index::protocol_index<tables::widget_v2> ()));
}

TEST (LumexProtocolIndexTest,
      GivenRegistries_WhenAttached_ThenTheyAreListedAndFound)
{
  index::index_result_t const result
      = index::build_index<tables::widget, tables::lamp, tables::tuning> ();
  ASSERT_TRUE (result.has_value ());
  ASSERT_EQ (result->registries ().size (), 2u);
  index::registry_info_t const *const lamp = result->find_registry ("lamp");
  ASSERT_NE (lamp, nullptr);
  EXPECT_EQ (lamp->entries.size (), 5u);
  EXPECT_NE (result->find_registry ("tuning"), nullptr);
  EXPECT_EQ (result->find_registry ("nothing"), nullptr);
  EXPECT_TRUE (widget_index ().registries ().empty ());
}

TEST (
    LumexProtocolIndexTest,
    GivenAHeaderWithAKeyReader_WhenAFrameIsMatched_ThenThatReaderLocatesTheKey)
{
  index::index_result_t const result = index::build_index<offset_protocol> ();
  ASSERT_TRUE (result.has_value ());
  // A byte of header, then the key 0x0102 little endian, then the field.
  layout::bytes_t const frame = bytes ({ 0xEE, 0x02, 0x01, 0x2A });
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "FIRST");
  EXPECT_EQ (found.payload_offset, 3u);
  EXPECT_EQ (found.key.width, 2u);
}

TEST (LumexProtocolIndexTest,
      GivenAHexadecimalLayer_WhenAFrameIsMatched_ThenTheLeadingDigitsAreTheKey)
{
  index::index_result_t const &result = index::protocol_index<widget_hex> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const frame = bytes_of ("0200C8");
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "SET_SPEED");
  EXPECT_EQ (found.payload_offset, 2u);
  layout::bytes_t const bad = bytes_of ("0X00C8");
  EXPECT_FALSE (result->match (view_of (bad)).has_key);
  layout::bytes_t const lower = bytes_of ("0a");
  EXPECT_TRUE (result->match (view_of (lower)).has_key);
}

TEST (LumexProtocolIndexTest,
      GivenTheKeyReaders_WhenFramesAreShortOrDamaged_ThenTheyRefuseThem)
{
  index::key_read_t read = index::key_read_t ();
  layout::bytes_t const two = bytes ({ 0x01, 0x02 });
  EXPECT_TRUE ((index::key_at<0, 2>::read (view_of (two), read)));
  EXPECT_EQ (read.key.integer, 0x0102u);
  EXPECT_TRUE ((index::key_at<0, 2, index::key_byte_order::little>::read (
      view_of (two), read)));
  EXPECT_EQ (read.key.integer, 0x0201u);
  EXPECT_FALSE ((index::key_at<1, 2>::read (view_of (two), read)));
  EXPECT_FALSE ((index::key_at<0, 3>::read (view_of (two), read)));
  layout::bytes_t const digits = bytes_of ("1f");
  EXPECT_TRUE (
      (index::key_at_digits<0, 2, 16>::read (view_of (digits), read)));
  EXPECT_EQ (read.key.integer, 0x1Fu);
  EXPECT_FALSE (
      (index::key_at_digits<0, 2, 10>::read (view_of (digits), read)));
  layout::bytes_t const decimal = bytes_of ("42");
  EXPECT_TRUE (
      (index::key_at_digits<0, 2, 10>::read (view_of (decimal), read)));
  EXPECT_EQ (read.key.integer, 42u);
  EXPECT_FALSE (
      (index::key_at_digits<0, 3, 10>::read (view_of (decimal), read)));
}

TEST (
    LumexProtocolIndexTest,
    GivenATokenizedKeyReader_WhenTrimmingIsOffOrThereIsNoSeparator_ThenTheKeyIsTheWholeToken)
{
  index::key_read_t read = index::key_read_t ();
  layout::bytes_t const spaced = bytes_of (" 7 ,1");
  ASSERT_TRUE (
      (index::tokenized_key<',', true>::read (view_of (spaced), read)));
  EXPECT_EQ (std::string (read.key.text, read.key.text_size), "7");
  ASSERT_TRUE (
      (index::tokenized_key<',', false>::read (view_of (spaced), read)));
  EXPECT_EQ (std::string (read.key.text, read.key.text_size), " 7 ");
  layout::bytes_t const alone = bytes_of ("AZ");
  ASSERT_TRUE (
      (index::tokenized_key<',', true>::read (view_of (alone), read)));
  EXPECT_EQ (std::string (read.key.text, read.key.text_size), "AZ");
  EXPECT_EQ (read.payload_offset, 0u);
  layout::bytes_t const empty;
  ASSERT_TRUE (
      (index::tokenized_key<',', true>::read (view_of (empty), read)));
  EXPECT_EQ (read.key.text_size, 0u);
  EXPECT_FALSE (index::no_key_reader::read (view_of (alone), read));
}

TEST (
    LumexProtocolIndexTest,
    GivenTheTablesOfTheTests_WhenTheKeyWidthIsAsked_ThenTheHeaderOrTheWidestKeyDecides)
{
  EXPECT_EQ (index::key_width_of<tables::widget>::value, 1u);
  // A tokenized layer without a width in its header: the widest int key.
  EXPECT_EQ (index::key_width_of<tables::widget_text>::value, 4u);
  EXPECT_EQ (index::key_width_of<tables::pairs>::value, 2u);
  // No width in the header: the widest integer key, an int.
  EXPECT_EQ (index::key_width_of<tables::clash_wide>::value, 4u);
  // No integer key and no width: one byte.
  EXPECT_EQ (index::key_width_of<tables::washer>::value, 1u);
}

TEST (
    LumexProtocolIndexTest,
    GivenTablesWithEqualIntegerKeys_WhenCheckedAtCompileTime_ThenTheyAreNotUnique)
{
  EXPECT_TRUE (index::keys_unique<tables::widget>::value);
  EXPECT_TRUE (index::keys_unique<tables::widget_v2>::value);
  EXPECT_TRUE (index::keys_unique<tables::washer>::value);
  EXPECT_FALSE (index::keys_unique<tables::clash_int>::value);
  // '\xFF' and -1 are one key on a one-byte wire, two on a four-byte one.
  EXPECT_FALSE (index::keys_unique<tables::clash_narrow>::value);
  EXPECT_TRUE (index::keys_unique<tables::clash_wide>::value);
}

TEST (LumexProtocolIndexTest,
      GivenTwoRowsWithOneKey_WhenIndexed_ThenTheErrorNamesBothRowsAndTheKey)
{
  index::index_result_t const result
      = index::build_index<tables::clash_int> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, protocol_errc::duplicate_key);
  EXPECT_EQ (result.error ().message,
             "protocol clash_int: rows ONE and TWO have the same key 0x07 on "
             "the wire");
}

TEST (LumexProtocolIndexTest,
      GivenACharacterAndAnIntOnAOneByteWire_WhenIndexed_ThenTheyClash)
{
  index::index_result_t const result
      = index::build_index<tables::clash_narrow> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, protocol_errc::duplicate_key);
  EXPECT_EQ (result.error ().message,
             "protocol clash_narrow: rows LETTER and NUMBER have the same key "
             "0xFF on the wire");
}

TEST (LumexProtocolIndexTest,
      GivenACharacterAndAnIntOnAWideWire_WhenIndexed_ThenTheyAreTwoKeys)
{
  index::index_result_t const result
      = index::build_index<tables::clash_wide> ();
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result->find_integer (0xFF)->entry->identifier,
             std::string ("LETTER"));
  EXPECT_EQ (result->find_integer (0xFFFFFFFFu)->entry->identifier,
             std::string ("NUMBER"));
}

TEST (LumexProtocolIndexTest,
      GivenTwoTextKeysAlike_WhenIndexed_ThenTheErrorQuotesTheKey)
{
  index::index_result_t const result
      = index::build_index<tables::clash_text> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, protocol_errc::duplicate_key);
  EXPECT_EQ (result.error ().message,
             "protocol clash_text: rows FIRST and SECOND have the same key "
             "\"AZ\"");
}

TEST (
    LumexProtocolIndexTest,
    GivenAnIntegerKeyAndATextKeyThatAreTheSameTokenizedText_WhenIndexed_ThenTheyClash)
{
  index::index_result_t const result
      = index::build_index<tables::clash_decimal> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().message,
             "protocol clash_decimal: rows NUMERIC and TEXTUAL have the same "
             "key \"5\"");
}

TEST (LumexProtocolIndexTest,
      GivenARowWhoseIdentifierIsTheNameOfAnother_WhenIndexed_ThenTheNamesClash)
{
  index::index_result_t const result
      = index::build_index<tables::clash_name> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, protocol_errc::duplicate_key);
  EXPECT_EQ (result.error ().message,
             "protocol clash_name: rows ALPHA and BETA have the same name "
             "ALPHA");
}

TEST (LumexProtocolIndexTest,
      GivenARegistryWithEqualKeys_WhenAttached_ThenTheIndexIsRefused)
{
  index::index_result_t const result
      = index::build_index<tables::widget, tables::clashing> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, protocol_errc::duplicate_key);
  EXPECT_EQ (result.error ().message,
             "registry clashing: entries FIRST and SECOND have the same key "
             "index 7");
}

TEST (LumexProtocolIndexTest, GivenAnEmptyIndex_WhenAsked_ThenItFindsNothing)
{
  index::protocol_index_t const empty;
  EXPECT_EQ (empty.rows ().size (), 0u);
  EXPECT_EQ (empty.find_integer (1), nullptr);
  EXPECT_EQ (empty.find_name ("PING"), nullptr);
  layout::bytes_t const frame = bytes ({ 1 });
  index::frame_match_t const found = empty.match (view_of (frame));
  EXPECT_EQ (found.row, nullptr);
  EXPECT_FALSE (found.has_key);
}

TEST (
    LumexProtocolIndexTest,
    GivenALayerAboveTheCore_WhenIndexed_ThenItsReplyKindAndItsOptionAreInTheRow)
{
  index::index_result_t const &result
      = index::protocol_index<tables::dialogue::dialogue_protocol> ();
  ASSERT_TRUE (result.has_value ());
  index::row_t const &start = row_named (*result, "START");
  EXPECT_TRUE (start.reply == index::reply_form::dialogue);
  EXPECT_EQ (start.reply_count, 0u);
  EXPECT_EQ (start.describe_reply, nullptr);
  EXPECT_NE (start.describe_request, nullptr);
  ASSERT_EQ (start.entry->options.extra.size (), 1u);
  EXPECT_STREQ (start.entry->options.extra.data ()[0].name, "rate_limit");
  EXPECT_EQ (start.entry->options.extra.data ()[0].value, 50);
  index::row_t const &stop = row_named (*result, "STOP");
  EXPECT_TRUE (stop.reply == index::reply_form::single_field);
  EXPECT_NE (stop.describe_reply, nullptr);
}

TEST (
    LumexProtocolIndexTest,
    GivenCompositeKeysAndNoKeyWidth_WhenIndexed_ThenTheWidthOfTheKeyTypeIsTheWireWidth)
{
  EXPECT_EQ (index::key_width_of<tables::pairs_wide>::value, 2u);
  EXPECT_TRUE (index::keys_unique<tables::pairs_wide>::value);
  index::index_result_t const result
      = index::build_index<tables::pairs_wide> ();
  ASSERT_TRUE (result.has_value ()) << result.error ().message;
  EXPECT_EQ (result->key_width (), 2u);
  index::row_t const *const heat = result->find (
      dsl::key_value (dsl::make_key (tables::pair_key (4, 5))));
  ASSERT_NE (heat, nullptr);
  EXPECT_STREQ (heat->entry->identifier, "HEAT");
  layout::bytes_t const frame = bytes ({ 0x03, 0x05 });
  index::frame_match_t const found = result->match (view_of (frame));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "WARM");
}

TEST (
    LumexProtocolIndexTest,
    GivenACharacterKeyAndAnIntegerKeyInATokenizedLayer_WhenIndexed_ThenTheyAreTwoKeysOnTheWire)
{
  EXPECT_TRUE (index::keys_unique<tables::letters_text>::value);
  index::index_result_t const &result
      = index::protocol_index<tables::letters_text> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const letter = bytes_of ("A");
  layout::bytes_t const number = bytes_of ("65");
  ASSERT_NE (result->match (view_of (letter)).row, nullptr);
  EXPECT_STREQ (result->match (view_of (letter)).row->entry->identifier,
                "LETTER");
  EXPECT_STREQ (result->match (view_of (number)).row->entry->identifier,
                "NUMBER");
}

TEST (LumexProtocolIndexTest,
      GivenTwoIntegerKeysOfOneTextInATokenizedLayer_WhenIndexed_ThenTheyClash)
{
  EXPECT_FALSE (index::keys_unique<tables::same_text>::value);
  index::index_result_t const result
      = index::build_index<tables::same_text> ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().message,
             "protocol same_text: rows SIGNED and UNSIGNED have the same key "
             "\"7\"");
}

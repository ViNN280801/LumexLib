// Registries (lumex/core/protocol/registry): the table macro, the run-time
// entries, the checks of a write (access, limits, raw value), availability by
// firmware and model, the typed generic commands and their keys, and the
// comparison with the self-description of a device.

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace registry = lumex::core::protocol::registry;
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes_of;
using tables::lamp;
using tables::view_of;

registry::registry_entry_t const &
entry_of (registry::entries_t const &entries, char const *identifier)
{
  for (std::size_t i = 0; i < entries.size (); ++i)
    if (std::string (entries.data ()[i].identifier) == identifier)
      return entries.data ()[i];
  ADD_FAILURE () << "no entry " << identifier;
  return entries.data ()[0];
}

std::vector<std::uint8_t>
bytes (std::initializer_list<int> values)
{
  std::vector<std::uint8_t> out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

// Generic commands over four registries, one per kind of key on the wire.
struct registry_header
{
  typedef registry::table_vocabulary lumex_vocabulary;
};

// clang-format off
#define GENERIC_ROWS(X, X_AS) \
  X (SET_LAMP, 0x20, "Write a lamp parameter", \
     (registry_key_<tables::lamp, u8_>, registry_value_<tables::lamp, 0, i32_le_>), \
     ("parameter", "value"), (), ()) \
  X (SET_TUNING, 0x30, "Write a tuning parameter", \
     (registry_key_<tables::tuning, text_<2> >, \
      registry_value_<tables::tuning, 0, i16_le_>), \
     ("parameter", "value"), (), ()) \
  X (SET_TIMER, 0x31, "Write a timer", \
     (registry_key_<tables::timers, u8_, u16_le_>, \
      registry_value_<tables::timers, 0, u32_le_>), \
     ("parameter", "value"), (), ()) \
  X (SET_HOLD, 0x32, "Write a register", \
     (registry_key_<tables::holding, u16_be_>, \
      registry_value_<tables::holding, 0, i32_be_>), \
     ("parameter", "value"), (), ()) \
  X (REPORT_LAMP, 0x33, "Report a lamp parameter", \
     (registry_key_<tables::lamp, u8_>, \
      registry_value_<tables::lamp, 0, i32_le_, registry::access_t::read>), \
     ("parameter", "value"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (generic, tables::codec, registry_header, GENERIC_ROWS);

typedef layout::request_of<generic::SET_LAMP_t>::type lamp_request;
} // namespace

TEST (
    LumexProtocolRegistryTest,
    GivenARegistry_WhenItsEntriesAreRead_ThenTheyAreInTableOrderWithTheirNames)
{
  registry::entries_t const entries = lamp::entries ();
  ASSERT_EQ (entries.size (), 5u);
  EXPECT_STREQ (entries.data ()[0].identifier, "FREQUENCY");
  EXPECT_STREQ (entries.data ()[0].specification_name, "FREQUENCY");
  EXPECT_STREQ (entries.data ()[0].description, "Flash frequency");
  EXPECT_STREQ (entries.data ()[3].identifier, "SERIAL");
  EXPECT_STREQ (entries.data ()[3].specification_name, "SN");
  EXPECT_STREQ (entries.data ()[4].identifier, "REMOVE_ALL");
  EXPECT_STREQ (entries.data ()[4].specification_name, "DELETE");
  EXPECT_STREQ (lamp::registry_name (), "lamp");
  EXPECT_TRUE (registry::is_registry<lamp>::value);
  EXPECT_FALSE (registry::is_registry<tables::widget>::value);
}

TEST (LumexProtocolRegistryTest,
      GivenTheEnumerators_WhenUsedAsIndexes_ThenTheyAreThePositionsInTheTable)
{
  registry::entries_t const entries = lamp::entries ();
  EXPECT_EQ (static_cast<unsigned> (lamp::FREQUENCY), 0u);
  EXPECT_EQ (static_cast<unsigned> (lamp::SERIAL), 3u);
  EXPECT_EQ (static_cast<unsigned> (lamp::lumex_end_of_entries),
             static_cast<unsigned> (entries.size ()));
  EXPECT_STREQ (entries.data ()[lamp::OFFSET].identifier, "OFFSET");
}

TEST (LumexProtocolRegistryTest,
      GivenEveryKindOfKey_WhenTheEntriesAreRead_ThenEachKeyHoldsItsParts)
{
  registry::entries_t const entries = tables::mixed::entries ();
  ASSERT_EQ (entries.size (), 5u);
  registry::registry_key_t const index = entries.data ()[0].key;
  EXPECT_TRUE (index.kind == registry::registry_key_kind::index);
  EXPECT_EQ (index.first, 5u);
  registry::registry_key_t const pair = entries.data ()[1].key;
  EXPECT_TRUE (pair.kind == registry::registry_key_kind::typed);
  EXPECT_EQ (pair.first, 3u);
  EXPECT_EQ (pair.second, 5u);
  registry::registry_key_t const unit = entries.data ()[2].key;
  EXPECT_TRUE (unit.kind == registry::registry_key_kind::unit_param);
  EXPECT_EQ (unit.first, 2u);
  EXPECT_EQ (unit.second, 9u);
  registry::registry_key_t const mnemonic = entries.data ()[3].key;
  EXPECT_TRUE (mnemonic.kind == registry::registry_key_kind::mnemonic);
  EXPECT_EQ (std::string (mnemonic.mnemonic), "AZ");
  registry::registry_key_t const address = entries.data ()[4].key;
  EXPECT_TRUE (address.kind == registry::registry_key_kind::modbus);
  EXPECT_EQ (address.first, 0x0100u);
}

TEST (LumexProtocolRegistryTest,
      GivenTheOptionsOfAnEntry_WhenRead_ThenTypeLimitsAccessAndRangesAreThere)
{
  registry::entries_t const entries = lamp::entries ();
  registry::registry_entry_t const &frequency = entries.data ()[0];
  EXPECT_TRUE (frequency.type.kind == field::field_kind::integer);
  EXPECT_EQ (frequency.type.binary_size, 2u);
  EXPECT_FALSE (frequency.type.is_signed);
  EXPECT_EQ (frequency.type.scale_num, 1);
  EXPECT_EQ (frequency.type.scale_den, 10);
  EXPECT_STREQ (frequency.type.unit, "Hz");
  EXPECT_TRUE (frequency.access == registry::access_t::read_write);
  EXPECT_TRUE (frequency.has_limits);
  EXPECT_EQ (frequency.lower, 1.0);
  EXPECT_EQ (frequency.upper, 100.0);
  EXPECT_TRUE (frequency.has_firmware);
  EXPECT_EQ (frequency.firmware_from, 2u);
  EXPECT_EQ (frequency.firmware_to, 7u);
  EXPECT_EQ (frequency.models.size (), 0u);
  registry::registry_entry_t const &level = entries.data ()[1];
  EXPECT_TRUE (level.access == registry::access_t::read);
  EXPECT_FALSE (level.has_limits);
  EXPECT_FALSE (level.has_firmware);
  registry::registry_entry_t const &offset = entries.data ()[2];
  EXPECT_TRUE (offset.type.is_signed);
  EXPECT_EQ (offset.models.size (), 2u);
  EXPECT_STREQ (offset.models.data ()[1], "B");
  EXPECT_TRUE (entries.data ()[4].access == registry::access_t::write);
}

TEST (LumexProtocolRegistryTest,
      GivenEntries_WhenLookedUpByKey_ThenTheMatchingEntryIsFound)
{
  registry::entries_t const entries = tables::mixed::entries ();
  registry::registry_key_t const pair = registry::make_typed_key (3, 5);
  registry::registry_entry_t const *const found
      = registry::find_entry (entries, pair);
  ASSERT_NE (found, nullptr);
  EXPECT_STREQ (found->identifier, "BY_PAIR");
  EXPECT_EQ (registry::find_entry (entries, registry::make_typed_key (3, 6)),
             nullptr);
  // The same numbers under another kind are another key.
  EXPECT_EQ (
      registry::find_entry (entries, registry::make_unit_param_key (3, 5)),
      nullptr);
  EXPECT_TRUE (registry::same_key (registry::make_mnemonic_key ("AZ"),
                                   entries.data ()[3].key));
  EXPECT_FALSE (registry::same_key (registry::make_mnemonic_key ("AY"),
                                    entries.data ()[3].key));
}

TEST (LumexProtocolRegistryTest,
      GivenAccess_WhenChecked_ThenOnlyTheAllowedAccessPasses)
{
  registry::registry_entry_t const &both = lamp::entries ().data ()[0];
  registry::registry_entry_t const &read = lamp::entries ().data ()[1];
  registry::registry_entry_t const &write = lamp::entries ().data ()[4];
  EXPECT_FALSE (registry::check_access (both, registry::access_t::read));
  EXPECT_FALSE (registry::check_access (both, registry::access_t::write));
  EXPECT_FALSE (registry::check_access (read, registry::access_t::read));
  EXPECT_EQ (registry::check_access (read, registry::access_t::write),
             protocol_errc::access_denied);
  EXPECT_EQ (registry::check_access (write, registry::access_t::read),
             protocol_errc::access_denied);
  EXPECT_FALSE (registry::check_access (write, registry::access_t::write));
}

TEST (LumexProtocolRegistryTest,
      GivenLimits_WhenAValueIsChecked_ThenTheBordersPassAndOutsideFails)
{
  registry::registry_entry_t const &frequency = lamp::entries ().data ()[0];
  EXPECT_FALSE (registry::check_limits (frequency, 1.0));
  EXPECT_FALSE (registry::check_limits (frequency, 100.0));
  EXPECT_EQ (registry::check_limits (frequency, 0.99),
             protocol_errc::out_of_limits);
  EXPECT_EQ (registry::check_limits (frequency, 100.01),
             protocol_errc::out_of_limits);
  EXPECT_EQ (registry::check_limits (frequency, std::nan ("")),
             protocol_errc::invalid_value);
  // Without limits every value passes the limit check.
  EXPECT_FALSE (registry::check_limits (lamp::entries ().data ()[1], 1e9));
}

TEST (LumexProtocolRegistryTest,
      GivenAnEngineeringValue_WhenConvertedToRaw_ThenItIsScaledAndRounded)
{
  registry::entries_t const entries = lamp::entries ();
  std::int64_t raw = 0;
  ASSERT_TRUE (registry::to_raw (entries.data ()[0], 20.0, raw));
  EXPECT_EQ (raw, 200);
  // Halves round away from zero.
  ASSERT_TRUE (registry::to_raw (entries.data ()[0], 0.05, raw));
  EXPECT_EQ (raw, 1);
  registry::registry_entry_t const &offset = entries.data ()[2];
  ASSERT_TRUE (registry::to_raw (offset, -0.005, raw));
  EXPECT_EQ (raw, -1);
  ASSERT_TRUE (registry::to_raw (offset, -3.0, raw));
  EXPECT_EQ (raw, -300);
  EXPECT_EQ (registry::to_engineering (offset, -300), -3.0);
}

TEST (LumexProtocolRegistryTest,
      GivenAValueTheTypeCannotHold_WhenConvertedToRaw_ThenItIsRefused)
{
  registry::entries_t const entries = lamp::entries ();
  std::int64_t raw = 0;
  // u16 with a scale of 0.1 ends at 6553.5.
  EXPECT_TRUE (registry::to_raw (entries.data ()[0], 6553.5, raw));
  EXPECT_FALSE (registry::to_raw (entries.data ()[0], 6553.6, raw));
  EXPECT_FALSE (registry::to_raw (entries.data ()[0], -0.1, raw));
  // i16 with a scale of 0.01 ends at 327.67 and 327.68 below zero.
  registry::registry_entry_t const &offset = entries.data ()[2];
  EXPECT_TRUE (registry::to_raw (offset, 327.67, raw));
  EXPECT_FALSE (registry::to_raw (offset, 327.68, raw));
  EXPECT_TRUE (registry::to_raw (offset, -327.68, raw));
  EXPECT_EQ (raw, -32768);
  EXPECT_FALSE (registry::to_raw (offset, -327.69, raw));
  EXPECT_FALSE (registry::to_raw (offset, std::nan (""), raw));
}

TEST (
    LumexProtocolRegistryTest,
    GivenFloatingSignMagnitudeAndWideEntries_WhenConvertedToRaw_ThenTheirRulesApply)
{
  registry::entries_t const entries = tables::odd::entries ();
  std::int64_t raw = 0;
  // A floating entry has no raw value.
  EXPECT_FALSE (registry::to_raw (entries.data ()[0], 1.5, raw));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[0], 1));
  // A sign-magnitude entry keeps the sign in the top bit: no -32768.
  EXPECT_TRUE (registry::to_raw (entries.data ()[1], -32767.0, raw));
  EXPECT_EQ (raw, -32767);
  EXPECT_FALSE (registry::to_raw (entries.data ()[1], -32768.0, raw));
  EXPECT_TRUE (registry::raw_in_range (entries.data ()[1], -32767));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[1], -32768));
  // A 64-bit unsigned entry holds what an int64 holds.
  EXPECT_TRUE (registry::to_raw (entries.data ()[2], 9.0e15, raw));
  EXPECT_FALSE (registry::to_raw (entries.data ()[2], 1.0e19, raw));
  EXPECT_TRUE (registry::raw_in_range (
      entries.data ()[2], (std::numeric_limits<std::int64_t>::max) ()));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[2], -1));
}

TEST (LumexProtocolRegistryTest,
      GivenARawValue_WhenCheckedAgainstTheType_ThenWidthAndSignDecide)
{
  registry::entries_t const entries = lamp::entries ();
  EXPECT_TRUE (registry::raw_in_range (entries.data ()[0], 65535));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[0], 65536));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[0], -1));
  EXPECT_TRUE (registry::raw_in_range (entries.data ()[2], -32768));
  EXPECT_FALSE (registry::raw_in_range (entries.data ()[2], 32768));
}

TEST (LumexProtocolRegistryTest,
      GivenAFirmwareAndAModel_WhenAvailabilityIsAsked_ThenTheRangesDecide)
{
  registry::entries_t const entries = lamp::entries ();
  registry::registry_entry_t const &frequency = entries.data ()[0];
  EXPECT_TRUE (registry::entry_available (frequency, 2, ""));
  EXPECT_TRUE (registry::entry_available (frequency, 7, ""));
  EXPECT_FALSE (registry::entry_available (frequency, 1, ""));
  EXPECT_FALSE (registry::entry_available (frequency, 8, ""));
  // Not identified yet keeps everything.
  EXPECT_TRUE (registry::entry_available (frequency, 0, nullptr));
  registry::registry_entry_t const &offset = entries.data ()[2];
  EXPECT_TRUE (registry::entry_available (offset, 0, "A"));
  EXPECT_TRUE (registry::entry_available (offset, 0, "B"));
  EXPECT_FALSE (registry::entry_available (offset, 0, "C"));
  EXPECT_FALSE (registry::entry_available (offset, 0, "AB"));
  EXPECT_TRUE (registry::entry_available (offset, 0, ""));
  EXPECT_TRUE (registry::entry_available (offset, 0, nullptr));
}

TEST (LumexProtocolRegistryTest,
      GivenAKey_WhenWrittenAsText_ThenEveryKindHasItsSpelling)
{
  registry::entries_t const entries = tables::mixed::entries ();
  EXPECT_EQ (registry::key_to_string (entries.data ()[0].key), "index 5");
  EXPECT_EQ (registry::key_to_string (entries.data ()[1].key), "typed 3/5");
  EXPECT_EQ (registry::key_to_string (entries.data ()[2].key),
             "unit_param 2/9");
  EXPECT_EQ (registry::key_to_string (entries.data ()[3].key),
             "mnemonic \"AZ\"");
  EXPECT_EQ (registry::key_to_string (entries.data ()[4].key),
             "modbus 0x0100");
  EXPECT_STREQ (registry::key_kind_name (registry::registry_key_kind::typed),
                "typed");
  EXPECT_STREQ (registry::access_to_string (registry::access_t::read), "R");
  EXPECT_STREQ (registry::access_to_string (registry::access_t::write), "W");
  EXPECT_STREQ (registry::access_to_string (registry::access_t::read_write),
                "RW");
}

TEST (LumexProtocolRegistryTest,
      GivenTwoEntriesWithOneKey_WhenSearched_ThenTheFirstPairIsFound)
{
  std::size_t first = 99;
  std::size_t second = 99;
  EXPECT_FALSE (
      registry::find_duplicate_key (lamp::entries (), first, second));
  ASSERT_TRUE (registry::find_duplicate_key (tables::clashing::entries (),
                                             first, second));
  EXPECT_EQ (first, 0u);
  EXPECT_EQ (second, 1u);
}

TEST (
    LumexProtocolRegistryTest,
    GivenAGenericCommand_WhenEncodedWithAnEntry_ThenTheKeyAndTheRawValueAreWritten)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode_request<generic, generic::SET_LAMP_t> (
          out, lamp::FREQUENCY, 20.0);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (out, bytes ({ 0x10, 0xC8, 0x00, 0x00, 0x00 }));
}

TEST (LumexProtocolRegistryTest,
      GivenALimitBorder_WhenEncoded_ThenBothBordersPassAndBothSidesOutsideFail)
{
  for (double inside : { 1.0, 100.0 })
    {
      layout::bytes_t out;
      EXPECT_TRUE ((layout::encode_request<generic, generic::SET_LAMP_t> (
                        out, lamp::FREQUENCY, inside))
                       .has_value ());
    }
  for (double outside : { 0.9, 100.1 })
    {
      layout::bytes_t out;
      layout::encode_result_t const result
          = layout::encode_request<generic, generic::SET_LAMP_t> (
              out, lamp::FREQUENCY, outside);
      ASSERT_FALSE (result.has_value ());
      EXPECT_EQ (result.error (), protocol_errc::out_of_limits);
      EXPECT_TRUE (out.empty ());
    }
}

TEST (LumexProtocolRegistryTest,
      GivenAReadOnlyEntry_WhenWritten_ThenAccessDeniedAndNothingIsWritten)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode_request<generic, generic::SET_LAMP_t> (out, lamp::LEVEL,
                                                              50);
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), protocol_errc::access_denied);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexProtocolRegistryTest,
      GivenAWriteOnlyEntry_WhenAReplyThatReadsItIsEncoded_ThenAccessDenied)
{
  layout::bytes_t out;
  layout::encode_result_t const denied
      = layout::encode_request<generic, generic::REPORT_LAMP_t> (
          out, lamp::REMOVE_ALL, 1);
  ASSERT_FALSE (denied.has_value ());
  EXPECT_EQ (denied.error (), protocol_errc::access_denied);
  // The same entry is writable by the command that writes.
  layout::encode_result_t const allowed
      = layout::encode_request<generic, generic::SET_LAMP_t> (
          out, lamp::REMOVE_ALL, 1);
  EXPECT_TRUE (allowed.has_value ());
}

TEST (LumexProtocolRegistryTest,
      GivenAValueTheEntryTypeCannotHold_WhenEncoded_ThenInvalidValue)
{
  for (int value : { 300, -1 })
    {
      layout::bytes_t out;
      layout::encode_result_t const result
          = layout::encode_request<generic, generic::SET_LAMP_t> (
              out, lamp::REMOVE_ALL, value);
      ASSERT_FALSE (result.has_value ());
      EXPECT_EQ (result.error (), protocol_errc::invalid_value);
      EXPECT_TRUE (out.empty ());
    }
  layout::bytes_t out;
  EXPECT_EQ ((layout::encode_request<generic, generic::SET_LAMP_t> (
                  out, lamp::REMOVE_ALL, std::nan (""))
                  .error ()),
             protocol_errc::invalid_value);
}

TEST (LumexProtocolRegistryTest,
      GivenARawValue_WhenEncoded_ThenItIsCheckedAgainstTheEntryAndWritten)
{
  layout::bytes_t out;
  EXPECT_TRUE (
      (layout::encode_request<generic, generic::SET_LAMP_t> (
           out, lamp::FREQUENCY, registry::parameter_value_t::of_raw (250)))
          .has_value ());
  EXPECT_EQ (out, bytes ({ 0x10, 0xFA, 0x00, 0x00, 0x00 }));
  // 250 raw is 25 Hz: inside. 2000 raw is 200 Hz: outside the limits.
  layout::bytes_t none;
  EXPECT_EQ (
      (layout::encode_request<generic, generic::SET_LAMP_t> (
           none, lamp::FREQUENCY, registry::parameter_value_t::of_raw (2000))
           .error ()),
      protocol_errc::out_of_limits);
  // A raw value that does not fit the type of the entry.
  EXPECT_EQ (
      (layout::encode_request<generic, generic::SET_LAMP_t> (
           none, lamp::REMOVE_ALL, registry::parameter_value_t::of_raw (256))
           .error ()),
      protocol_errc::invalid_value);
}

TEST (
    LumexProtocolRegistryTest,
    GivenTheBytesOfAGenericCommand_WhenDecoded_ThenTheEntryAndTheValueAreRecovered)
{
  layout::bytes_t const wire = bytes ({ 0x10, 0xC8, 0x00, 0x00, 0x00 });
  layout::decode_result_t<lamp_request> const decoded
      = layout::decode_request<generic, generic::SET_LAMP_t> (view_of (wire));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (static_cast<unsigned> (field::get<0> (*decoded)),
             static_cast<unsigned> (lamp::FREQUENCY));
  registry::parameter_value_t const value = field::get<1> (*decoded);
  EXPECT_EQ (value.raw, 200);
  EXPECT_TRUE (value.has_raw);
  EXPECT_EQ (value.engineering, 20.0);
  ASSERT_NE (value.entry, nullptr);
  EXPECT_STREQ (value.entry->identifier, "FREQUENCY");
}

TEST (LumexProtocolRegistryTest,
      GivenAnEncodedCommand_WhenDecodedAndEncodedAgain_ThenTheBytesAreTheSame)
{
  layout::bytes_t once;
  ASSERT_TRUE ((layout::encode_request<generic, generic::SET_LAMP_t> (
                    once, lamp::OFFSET, -3.25))
                   .has_value ());
  layout::decode_result_t<lamp_request> const decoded
      = layout::decode_request<generic, generic::SET_LAMP_t> (view_of (once));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<1> (*decoded).engineering, -3.25);
  layout::bytes_t twice;
  ASSERT_TRUE ((layout::encode_request_record<generic, generic::SET_LAMP_t> (
                    twice, *decoded))
                   .has_value ());
  EXPECT_EQ (once, twice);
}

TEST (LumexProtocolRegistryTest,
      GivenAKeyTheRegistryDoesNotHave_WhenDecoded_ThenUnknownCase)
{
  layout::bytes_t const wire = bytes ({ 0x99, 0x01, 0x00, 0x00, 0x00 });
  layout::decode_result_t<lamp_request> const decoded
      = layout::decode_request<generic, generic::SET_LAMP_t> (view_of (wire));
  ASSERT_FALSE (decoded.has_value ());
  EXPECT_EQ (decoded.error (), protocol_errc::unknown_case);
}

TEST (LumexProtocolRegistryTest,
      GivenARawValueTheEntryCannotHold_WhenDecoded_ThenInvalidValue)
{
  // 65536 does not fit the u16 of FREQUENCY; -1 is below its range.
  for (layout::bytes_t const &wire :
       { bytes ({ 0x10, 0x00, 0x00, 0x01, 0x00 }),
         bytes ({ 0x10, 0xFF, 0xFF, 0xFF, 0xFF }) })
    {
      layout::decode_result_t<lamp_request> const decoded
          = layout::decode_request<generic, generic::SET_LAMP_t> (
              view_of (wire));
      ASSERT_FALSE (decoded.has_value ());
      EXPECT_EQ (decoded.error (), protocol_errc::invalid_value);
    }
}

TEST (
    LumexProtocolRegistryTest,
    GivenAMnemonicRegistry_WhenACommandIsEncodedAndDecoded_ThenTheTwoLettersAreTheKey)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<generic, generic::SET_TUNING_t> (
                    out, tables::tuning::GAIN, 2.5))
                   .has_value ());
  EXPECT_EQ (out, bytes ({ 'G', 'N', 25, 0 }));
  layout::decode_result_t<
      layout::request_of<generic::SET_TUNING_t>::type> const decoded
      = layout::decode_request<generic, generic::SET_TUNING_t> (view_of (out));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (static_cast<unsigned> (field::get<0> (*decoded)),
             static_cast<unsigned> (tables::tuning::GAIN));
  EXPECT_EQ (field::get<1> (*decoded).engineering, 2.5);
  layout::bytes_t const unknown = bytes ({ 'Z', 'Z', 1, 0 });
  EXPECT_EQ ((layout::decode_request<generic, generic::SET_TUNING_t> (
                  view_of (unknown))
                  .error ()),
             protocol_errc::unknown_case);
  layout::bytes_t none;
  EXPECT_EQ ((layout::encode_request<generic, generic::SET_TUNING_t> (
                  none, tables::tuning::GAIN, 11.0)
                  .error ()),
             protocol_errc::out_of_limits);
}

TEST (
    LumexProtocolRegistryTest,
    GivenATypedKeyRegistry_WhenACommandIsEncodedAndDecoded_ThenBothPartsAreWritten)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<generic, generic::SET_TIMER_t> (
                    out, tables::timers::COOL_DOWN, 7))
                   .has_value ());
  // The type 3 as one byte, the index 6 as a little-endian u16, then the
  // 32-bit carrier.
  EXPECT_EQ (out, bytes ({ 3, 6, 0, 7, 0, 0, 0 }));
  layout::decode_result_t<layout::request_of<generic::SET_TIMER_t>::type> const
      decoded
      = layout::decode_request<generic, generic::SET_TIMER_t> (view_of (out));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (static_cast<unsigned> (field::get<0> (*decoded)),
             static_cast<unsigned> (tables::timers::COOL_DOWN));
  layout::bytes_t const unknown = bytes ({ 3, 9, 0, 7, 0, 0, 0 });
  EXPECT_EQ ((layout::decode_request<generic, generic::SET_TIMER_t> (
                  view_of (unknown))
                  .error ()),
             protocol_errc::unknown_case);
}

TEST (LumexProtocolRegistryTest,
      GivenAModbusRegistry_WhenACommandIsEncoded_ThenTheAddressIsBigEndian)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<generic, generic::SET_HOLD_t> (
                    out, tables::holding::HOLD_TIME, 90))
                   .has_value ());
  EXPECT_EQ (out, bytes ({ 0x01, 0x00, 0, 0, 0, 90 }));
  layout::bytes_t none;
  EXPECT_EQ ((layout::encode_request<generic, generic::SET_HOLD_t> (
                  none, tables::holding::HOLD_TIME, 3601)
                  .error ()),
             protocol_errc::out_of_limits);
}

namespace
{
struct device
{
  std::vector<registry::device_entry_t> entries;

  bool
  operator() (std::size_t position, registry::device_entry_t &out) const
  {
    if (position >= entries.size ())
      return false;
    out = entries[position];
    return true;
  }
};

registry::device_entry_t
device_entry (registry::registry_entry_t const &table)
{
  registry::device_entry_t entry = registry::device_entry_t ();
  entry.key = table.key;
  entry.name = table.specification_name;
  entry.size = static_cast<std::uint32_t> (table.type.binary_size);
  entry.has_signedness = true;
  entry.is_signed = table.type.is_signed;
  entry.has_scale = true;
  entry.scale_num = table.type.scale_num;
  entry.scale_den = table.type.scale_den;
  entry.has_limits = table.has_limits;
  entry.lower = table.lower;
  entry.upper = table.upper;
  entry.has_access = true;
  entry.access = table.access;
  return entry;
}

device
device_of_lamp ()
{
  device result;
  for (std::size_t i = 0; i < lamp::entries ().size (); ++i)
    result.entries.push_back (device_entry (lamp::entries ().data ()[i]));
  return result;
}
} // namespace

TEST (
    LumexProtocolRegistryTest,
    GivenADeviceThatDescribesTheSameTable_WhenVerified_ThenThereIsNoDifference)
{
  registry::verify_report_t const report
      = registry::verify_registry<lamp> (device_of_lamp ());
  EXPECT_TRUE (report.ok ());
  EXPECT_EQ (report.device_entries, 5u);
  EXPECT_EQ (report.compared, 5u);
}

TEST (LumexProtocolRegistryTest,
      GivenADeviceWithoutAnEntry_WhenVerified_ThenTheMissingEntryIsReported)
{
  device remote = device_of_lamp ();
  remote.entries.erase (remote.entries.begin () + 1);
  registry::verify_report_t const report
      = registry::verify_registry<lamp> (remote);
  ASSERT_EQ (report.mismatches.size (), 1u);
  EXPECT_TRUE (report.mismatches[0].kind
               == registry::mismatch_kind::missing_in_device);
  EXPECT_EQ (report.mismatches[0].entry, "LEVEL");
  EXPECT_EQ (report.compared, 4u);
}

TEST (LumexProtocolRegistryTest,
      GivenADeviceWithAnEntryTheTableLacks_WhenVerified_ThenItIsReportedByKey)
{
  device remote = device_of_lamp ();
  registry::device_entry_t extra = remote.entries[0];
  extra.key = registry::make_index_key (0x7F);
  extra.name = "SPARE";
  remote.entries.push_back (extra);
  registry::verify_report_t const report
      = registry::verify_registry<lamp> (remote);
  ASSERT_EQ (report.mismatches.size (), 1u);
  EXPECT_TRUE (report.mismatches[0].kind
               == registry::mismatch_kind::missing_in_table);
  EXPECT_EQ (report.mismatches[0].entry, "index 127");
  EXPECT_EQ (registry::to_string (report.mismatches[0]),
             "missing_in_table of index 127: the device reports SPARE");
}

TEST (
    LumexProtocolRegistryTest,
    GivenADeviceWithDifferentProperties_WhenVerified_ThenEachDifferenceIsReported)
{
  device remote = device_of_lamp ();
  remote.entries[0].name = "FREQ";
  remote.entries[0].size = 4;
  remote.entries[0].is_signed = true;
  remote.entries[0].scale_den = 100;
  remote.entries[0].upper = 200;
  remote.entries[0].access = registry::access_t::read;
  registry::verify_report_t const report
      = registry::verify_registry<lamp> (remote);
  ASSERT_EQ (report.mismatches.size (), 6u);
  EXPECT_TRUE (report.mismatches[0].kind == registry::mismatch_kind::name);
  EXPECT_TRUE (report.mismatches[1].kind == registry::mismatch_kind::size);
  EXPECT_TRUE (report.mismatches[2].kind
               == registry::mismatch_kind::signedness);
  EXPECT_TRUE (report.mismatches[3].kind == registry::mismatch_kind::scale);
  EXPECT_TRUE (report.mismatches[4].kind == registry::mismatch_kind::limits);
  EXPECT_TRUE (report.mismatches[5].kind == registry::mismatch_kind::access);
  EXPECT_EQ (report.mismatches[1].detail, "table 2 bytes, device 4 bytes");
  EXPECT_EQ (report.mismatches[3].detail, "table 1/10, device 1/100");
  EXPECT_EQ (report.mismatches[4].detail, "table 1..100, device 1..200");
  EXPECT_EQ (report.mismatches[5].detail, "table RW, device R");
}

TEST (LumexProtocolRegistryTest,
      GivenADeviceThatDoesNotTellSomething_WhenVerified_ThenItIsNotCompared)
{
  device remote = device_of_lamp ();
  for (registry::device_entry_t &entry : remote.entries)
    {
      entry.name = nullptr;
      entry.size = 0;
      entry.has_signedness = false;
      entry.has_scale = false;
      entry.has_limits = false;
      entry.has_access = false;
    }
  EXPECT_TRUE (registry::verify_registry<lamp> (remote).ok ());
}

TEST (LumexProtocolRegistryTest,
      GivenADeviceThatNeverEnds_WhenVerified_ThenTheAskedEntriesAreBounded)
{
  std::size_t asked = 0;
  registry::verify_report_t const report = registry::verify_registry<lamp> (
      [&asked] (std::size_t, registry::device_entry_t &entry)
        {
          ++asked;
          entry = registry::device_entry_t ();
          entry.key = registry::make_index_key (0x10);
          return true;
        },
      8);
  EXPECT_EQ (asked, 8u);
  EXPECT_EQ (report.device_entries, 8u);
  EXPECT_FALSE (report.ok ());
}

TEST (LumexProtocolRegistryTest,
      GivenTheDifferenceKinds_WhenNamed_ThenEachHasItsName)
{
  EXPECT_STREQ (registry::mismatch_kind_name (
                    registry::mismatch_kind::missing_in_device),
                "missing_in_device");
  EXPECT_STREQ (registry::mismatch_kind_name (registry::mismatch_kind::access),
                "access");
  EXPECT_STREQ (registry::mismatch_kind_name (registry::mismatch_kind::scale),
                "scale");
}

TEST (
    LumexProtocolRegistryTest,
    GivenMnemonicsOfThreeLettersThatDifferInTheLast_WhenCompared_ThenTheyAreTwoKeys)
{
  registry::registry_key_t const first = registry::make_mnemonic_key ("ABC");
  registry::registry_key_t const second = registry::make_mnemonic_key ("ABD");
  EXPECT_FALSE (registry::same_key (first, second));
  EXPECT_TRUE (
      registry::same_key (first, registry::make_mnemonic_key ("ABC")));
  registry::entries_t const entries = tables::triple::entries ();
  registry::registry_entry_t const *const found
      = registry::find_entry (entries, second);
  ASSERT_NE (found, nullptr);
  EXPECT_STREQ (found->identifier, "SECOND_ONE");
  EXPECT_EQ (registry::key_to_string (first), "mnemonic \"ABC\"");
}

TEST (
    LumexProtocolRegistryTest,
    GivenARegistryWithAFloatingEntry_WhenWrittenWithoutThePairOfFields_ThenItStillHasItsTable)
{
  // A registry with a floating entry serves the exports and verify_registry;
  // registry_value_ refuses it at compile time (a compile-check case).
  EXPECT_EQ (tables::odd::entries ().size (), 3u);
  EXPECT_TRUE (tables::odd::entries ().data ()[0].type.kind
               == field::field_kind::floating);
}

TEST (
    LumexProtocolRegistryTest,
    GivenASessionWithoutTheFirmwareOfAnEntry_WhenVerified_ThenTheEntryIsSkippedAndListed)
{
  device remote = device_of_lamp ();
  // The device does not have FREQUENCY (firmware 2 to 7) as the table says.
  remote.entries[0].size = 4;
  index::session_context_t session;
  session.set_firmware (8);
  session.set_model ("B");
  registry::verify_report_t const report
      = registry::verify_registry<lamp> (remote, 65536, &session);
  EXPECT_TRUE (report.ok ()) << registry::to_string (report.mismatches[0]);
  ASSERT_EQ (report.skipped.size (), 1u);
  EXPECT_EQ (report.skipped[0], "FREQUENCY");
  EXPECT_EQ (report.compared, 4u);
  // Without the session the same device differs.
  EXPECT_FALSE (registry::verify_registry<lamp> (remote).ok ());
  // A model the entry does not list: it is skipped as well.
  session.set_model ("C");
  registry::verify_report_t const other
      = registry::verify_registry<lamp> (device_of_lamp (), 65536, &session);
  ASSERT_EQ (other.skipped.size (), 2u);
  EXPECT_EQ (other.skipped[1], "OFFSET");
  EXPECT_TRUE (other.ok ());
}

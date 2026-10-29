// The check of a decoded record against its registries
// (lumex/core/protocol/registry/LumexProtocolRegistryFields.hpp): the
// decoders accept a value outside the limits of its entry and an entry
// without the access of the command (a client reads what the device
// reports); registry::check_record tells a simulator or the dialogue engine
// which of them the encoder would refuse, in the fields of the record and of
// its nested layouts.

#include <cstdint>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace check_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

// clang-format off
#define CHECK_PARAMS(P, P_AS) \
  P (LOCKED, (0x01), "Read only", (u8_), R, ()) \
  P (LEVEL, (0x02), "Limited", (i16_le_), RW, (limits (5, 10))) \
  P (SECRET, (0x03), "Write only", (u8_), W, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (check_registry, CHECK_PARAMS);

struct codec
{
};

struct header
{
  using lumex_vocabulary = registry::table_vocabulary;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// A reply that reports an entry: a read is enough.
LUMEX_PROTOCOL_LAYOUT (report_t,
                       (registry::registry_key_<check_registry, u8_>,
                        registry::registry_value_<check_registry, 0, i16_le_,
                                                  registry::access_t::read>),
                       ("parameter", "value"));

// A write inside a nested layout.
LUMEX_PROTOCOL_LAYOUT (setting_t,
                       (registry::registry_key_<check_registry, u8_>,
                        registry::registry_value_<check_registry, 0, i16_le_>),
                       ("parameter", "value"));

// clang-format off
#define CHECK_ROWS(X, X_AS) \
  X (SET_PARAMETER, 0x01, "Write a parameter", \
     (registry_key_<check_registry, u8_>, \
      registry_value_<check_registry, 0, i16_le_>), \
     ("parameter", "value"), (report_t), ()) \
  X (SET_SLOT, 0x02, "Write a parameter of a slot", (u8_, setting_t), \
     ("slot", "setting"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (check_protocol, codec, header, CHECK_ROWS);
} // namespace check_tables

namespace
{
namespace tables = check_tables;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
using lumex::core::protocol::error::protocol_errc;

using set_row = tables::check_protocol::SET_PARAMETER_t;
using slot_row = tables::check_protocol::SET_SLOT_t;

layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

// The request of SET_PARAMETER decoded from the parameter key and the raw
// value (little endian).
layout::request_of<set_row>::type
decoded_request (int key, int low, int high)
{
  layout::bytes_t const message{ 0x01, static_cast<std::uint8_t> (key),
                                 static_cast<std::uint8_t> (low),
                                 static_cast<std::uint8_t> (high) };
  layout::decode_result_t<layout::request_of<set_row>::type> const decoded
      = layout::decode_message<tables::check_protocol, set_row> (
          view_of (message));
  EXPECT_TRUE (decoded.has_value ());
  return decoded.has_value () ? *decoded
                              : layout::request_of<set_row>::type ();
}
} // namespace

TEST (
    LumexProtocolRegistryCheckTest,
    GivenAValueOutsideTheLimits_WhenDecoded_ThenItDecodesAndTheCheckSaysOutOfLimits)
{
  layout::request_of<set_row>::type const record = decoded_request (2, 20, 0);
  EXPECT_EQ (field::get<1> (record).raw, 20);
  std::error_code const error
      = registry::check_record<set_row::fields_type> (record);
  EXPECT_TRUE (error == protocol_errc::out_of_limits);
  // The encoder refuses the same value.
  layout::bytes_t out;
  layout::encode_result_t const encoded
      = layout::encode_message_record<tables::check_protocol, set_row> (
          out, record);
  ASSERT_FALSE (encoded.has_value ());
  EXPECT_TRUE (encoded.error () == protocol_errc::out_of_limits);
}

TEST (
    LumexProtocolRegistryCheckTest,
    GivenAWriteOfAReadOnlyEntry_WhenDecoded_ThenItDecodesAndTheCheckSaysAccessDenied)
{
  layout::request_of<set_row>::type const record = decoded_request (1, 5, 0);
  EXPECT_TRUE (registry::check_record<set_row::fields_type> (record)
               == protocol_errc::access_denied);
}

TEST (LumexProtocolRegistryCheckTest,
      GivenAValueInsideTheLimits_WhenChecked_ThenNoError)
{
  layout::request_of<set_row>::type const record = decoded_request (2, 7, 0);
  EXPECT_FALSE (registry::check_record<set_row::fields_type> (record));
  // A record that was never decoded holds an engineering value.
  layout::request_of<set_row>::type made
      = layout::request_of<set_row>::type ();
  field::get<0> (made) = tables::check_registry::LEVEL;
  field::get<1> (made) = registry::parameter_value_t::of_engineering (11.0);
  EXPECT_TRUE (registry::check_record<set_row::fields_type> (made)
               == protocol_errc::out_of_limits);
  field::get<1> (made) = registry::parameter_value_t::of_engineering (10.0);
  EXPECT_FALSE (registry::check_record<set_row::fields_type> (made));
}

TEST (LumexProtocolRegistryCheckTest,
      GivenAReplyThatReportsAnEntry_WhenChecked_ThenAReadNeedsTheReadAccess)
{
  layout::bytes_t const secret{ 0x01, 0x03, 0x09, 0x00 };
  layout::decode_result_t<layout::reply_value_of<set_row>::type> const reported
      = layout::decode_reply_message<tables::check_protocol, set_row> (
          view_of (secret));
  ASSERT_TRUE (reported.has_value ());
  EXPECT_TRUE (registry::check_record<tables::report_t> (*reported)
               == protocol_errc::access_denied);
  layout::bytes_t const locked{ 0x01, 0x01, 0x09, 0x00 };
  layout::decode_result_t<layout::reply_value_of<set_row>::type> const readable
      = layout::decode_reply_message<tables::check_protocol, set_row> (
          view_of (locked));
  ASSERT_TRUE (readable.has_value ());
  EXPECT_FALSE (registry::check_record<tables::report_t> (*readable));
}

TEST (LumexProtocolRegistryCheckTest,
      GivenAWriteInsideANestedLayout_WhenChecked_ThenTheNestedValueIsChecked)
{
  layout::bytes_t const message{ 0x02, 0x07, 0x02, 0x63, 0x00 };
  layout::decode_result_t<layout::request_of<slot_row>::type> const decoded
      = layout::decode_message<tables::check_protocol, slot_row> (
          view_of (message));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_TRUE (registry::check_record<slot_row::fields_type> (*decoded)
               == protocol_errc::out_of_limits);
  layout::bytes_t const fine{ 0x02, 0x07, 0x02, 0x06, 0x00 };
  layout::decode_result_t<layout::request_of<slot_row>::type> const good
      = layout::decode_message<tables::check_protocol, slot_row> (
          view_of (fine));
  ASSERT_TRUE (good.has_value ());
  EXPECT_FALSE (registry::check_record<slot_row::fields_type> (*good));
}

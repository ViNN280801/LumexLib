// The fields inside a word and the enumerations
// (lumex/core/protocol/field/LumexProtocolBitFields.hpp): bits_ (runs,
// signed, bool, enumeration parts), mask_ (interleaved masks gathered and
// scattered), split_ (several parts of one word, the raw word kept, named
// parts through a layout) and enum_ (reflected enumerations are strict,
// character codes).

#include <cstdint>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;
using lumex::core::protocol::error::protocol_errc;
using tables::view_of;

using binary = layout::binary_layout<>;

template <typename F>
layout::bytes_t
encoded (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  EXPECT_TRUE ((layout::encode<binary, F> (out, value).has_value ()));
  return out;
}

template <typename F>
layout::decode_result_t<typename field::value_of<F>::type>
decoded (layout::bytes_t const &bytes)
{
  return layout::decode<binary, F> (view_of (bytes));
}

enum class lamp : std::uint8_t
{
  off = 0,
  on = 1,
  blink = 2
};

LUMEX_PROTOCOL_LAYOUT (motor_parts_t,
                       (bits_<u16_le_, 0, 12>, bits_<u16_le_, 12, 3, lamp>,
                        bits_<u16_le_, 15, 1, bool>),
                       ("position", "lamp", "busy"));
} // namespace

TEST (LumexProtocolBitFieldsTest,
      GivenARunOfBits_WhenDecoded_ThenTheValueIsShiftedDown)
{
  using high_nibble = field::bits_<field::u8_<>, 4, 4>;
  EXPECT_EQ (*decoded<high_nibble> (layout::bytes_t{ 0xA5 }), 0xAu);
  EXPECT_EQ (encoded<high_nibble> (0xC), (layout::bytes_t{ 0xC0 }));
  using signed_run = field::bits_<field::u16_be_<>, 3, 5, std::int8_t>;
  // bits 3..7 of 0x00F8 are 11111: -1.
  EXPECT_EQ (*decoded<signed_run> (layout::bytes_t{ 0x00, 0xF8 }), -1);
  EXPECT_EQ (encoded<signed_run> (-16), (layout::bytes_t{ 0x00, 0x80 }));
  EXPECT_EQ (encoded<signed_run> (15), (layout::bytes_t{ 0x00, 0x78 }));
  EXPECT_TRUE (high_nibble::info ().kind == field::field_kind::bits);
  EXPECT_EQ (high_nibble::info ().count, 4u);
}

TEST (LumexProtocolBitFieldsTest,
      GivenAValueWiderThanItsBits_WhenEncodedOrAssigned_ThenInvalidValue)
{
  using high_nibble = field::bits_<field::u8_<>, 4, 4>;
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, high_nibble> (out, 0x10);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  std::uint8_t value = 0;
  EXPECT_FALSE (field::impl_of<high_nibble>::type::assign (value, 16));
  EXPECT_TRUE (field::impl_of<high_nibble>::type::assign (value, 15));
  using signed_run = field::bits_<field::u16_be_<>, 3, 5, std::int8_t>;
  std::int8_t small = 0;
  EXPECT_FALSE (field::impl_of<signed_run>::type::assign (small, 16));
  EXPECT_FALSE (field::impl_of<signed_run>::type::assign (small, -17));
}

TEST (LumexProtocolBitFieldsTest,
      GivenInterleavedMasks_WhenDecoded_ThenTheirBitsArePackedInMaskOrder)
{
  using even = field::mask_<field::u8_<>, 0x55>;
  using odd = field::mask_<field::u8_<>, 0xAA>;
  // 0b0110_1001: even bits (0, 2, 4, 6) are 1, 0, 0, 1; odd bits are 0, 1,
  // 1, 0.
  EXPECT_EQ (*decoded<even> (layout::bytes_t{ 0x69 }), 0x9u);
  EXPECT_EQ (*decoded<odd> (layout::bytes_t{ 0x69 }), 0x6u);
  EXPECT_EQ (encoded<even> (0xF), (layout::bytes_t{ 0x55 }));
  EXPECT_EQ (encoded<odd> (0x5), (layout::bytes_t{ 0x22 }));
  using one = field::mask_<field::u8_<>, 0x40>;
  EXPECT_TRUE (
      (std::is_same<field::impl_of<one>::type::value_type, bool>::value));
  EXPECT_TRUE (*decoded<one> (layout::bytes_t{ 0x40 }));
  EXPECT_FALSE (*decoded<one> (layout::bytes_t{ 0xBF }));
  layout::bytes_t out;
  EXPECT_FALSE ((layout::encode<binary, even> (out, 0x10).has_value ()));
  EXPECT_TRUE (even::info ().kind == field::field_kind::mask);
  EXPECT_EQ (even::info ().count, 4u);
}

TEST (LumexProtocolBitFieldsTest,
      GivenASplitWord_WhenRoundTripped_ThenThePartsAndTheUncoveredBitsAreKept)
{
  using split
      = field::split_<field::u16_le_<>, field::bits_<field::u16_le_<>, 0, 4>,
                      field::mask_<field::u16_le_<>, 0x0F00>>;
  using value_type = field::value_of<split>::type;
  // Bits 4..7 and 12..15 belong to no part.
  layout::bytes_t const wire{ 0xA3, 0x5B };
  layout::decode_result_t<value_type> const value = decoded<split> (wire);
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (value->raw, 0x5BA3u);
  EXPECT_EQ (field::get<0> (value->parts), 0x3u);
  EXPECT_EQ (field::get<1> (value->parts), 0xBu);
  EXPECT_EQ (encoded<split> (*value), wire);
  value_type changed = *value;
  field::get<0> (changed.parts) = 0x7;
  EXPECT_EQ (encoded<split> (changed), (layout::bytes_t{ 0xA7, 0x5B }));
  field::get<1> (changed.parts) = 0x1F;
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, split> (out, changed);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (field::impl_of<split>::type::info ().kind
               == field::field_kind::split);
  EXPECT_EQ (field::impl_of<split>::type::info ().count, 2u);
}

TEST (LumexProtocolBitFieldsTest,
      GivenASplitWithNamedParts_WhenDecoded_ThenEveryPartHasItsType)
{
  using motor = field::split_<field::u16_le_<>, motor_parts_t>;
  using value_type = field::value_of<motor>::type;
  // position 0x123, lamp 2 (blink), busy 1: 0x123 | 2 << 12 | 1 << 15.
  layout::decode_result_t<value_type> const value
      = decoded<motor> (layout::bytes_t{ 0x23, 0xA1 });
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (field::get<0> (value->parts), 0x123u);
  EXPECT_TRUE (field::get<1> (value->parts) == lamp::blink);
  EXPECT_TRUE (field::get<2> (value->parts));
  EXPECT_EQ (std::string (motor_parts_t::field_names ()[1]), "lamp");
  EXPECT_EQ (encoded<motor> (*value), (layout::bytes_t{ 0x23, 0xA1 }));
}

TEST (LumexProtocolBitFieldsTest,
      GivenAReflectedEnumeration_WhenAnUnknownValueComes_ThenInvalidValue)
{
  using mode = field::enum_<field::u8_<>, tables::gauge_mode>;
  EXPECT_TRUE (*decoded<mode> (layout::bytes_t{ 5 })
               == tables::gauge_mode::hold);
  EXPECT_TRUE (decoded<mode> (layout::bytes_t{ 2 }).error ()
               == protocol_errc::invalid_value);
  EXPECT_EQ (encoded<mode> (tables::gauge_mode::run), (layout::bytes_t{ 1 }));
  layout::bytes_t out;
  EXPECT_FALSE (
      (layout::encode<binary, mode> (out, static_cast<tables::gauge_mode> (3))
           .has_value ()));
  tables::gauge_mode value = tables::gauge_mode::idle;
  EXPECT_FALSE (field::impl_of<mode>::type::assign (
      value, static_cast<tables::gauge_mode> (4)));
  EXPECT_STREQ (field::enum_name (tables::gauge_mode::hold), "hold");
  EXPECT_EQ (field::enum_name (static_cast<tables::gauge_mode> (9)), nullptr);
  EXPECT_TRUE (mode::info ().kind == field::field_kind::enumeration);
}

TEST (LumexProtocolBitFieldsTest,
      GivenAPlainEnumeration_WhenAnyValueComes_ThenItIsKept)
{
  using lamp_field = field::enum_<field::u8_<>, lamp>;
  EXPECT_EQ (static_cast<int> (*decoded<lamp_field> (layout::bytes_t{ 7 })),
             7);
  EXPECT_EQ (field::enum_name (lamp::on), nullptr);
}

TEST (LumexProtocolBitFieldsTest,
      GivenACharacterCodeEnumeration_WhenRoundTripped_ThenTheCodeIsTheLetter)
{
  using letter = field::enum_<field::char_, tables::gauge_letter>;
  EXPECT_EQ (encoded<letter> (tables::gauge_letter::start),
             (layout::bytes_t{ 'S' }));
  EXPECT_TRUE (*decoded<letter> (layout::bytes_t{ 'R' })
               == tables::gauge_letter::rest);
  EXPECT_EQ (letter::info ().binary_size, 1u);
}

TEST (
    LumexProtocolBitFieldsTest,
    GivenAReflectedEnumerationInARunOfBits_WhenAnUnknownValueComes_ThenInvalidValue)
{
  using mode = field::bits_<field::u8_<>, 0, 3, tables::gauge_mode>;
  EXPECT_TRUE (*decoded<mode> (layout::bytes_t{ 0xF5 })
               == tables::gauge_mode::hold);
  layout::decode_result_t<tables::gauge_mode> const unknown
      = decoded<mode> (layout::bytes_t{ 0x02 });
  ASSERT_FALSE (unknown.has_value ());
  EXPECT_TRUE (unknown.error () == protocol_errc::invalid_value);
  EXPECT_EQ (encoded<mode> (tables::gauge_mode::run), (layout::bytes_t{ 1 }));
  layout::bytes_t out;
  layout::encode_result_t const refused = layout::encode<binary, mode> (
      out, static_cast<tables::gauge_mode> (3));
  ASSERT_FALSE (refused.has_value ());
  EXPECT_TRUE (refused.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  tables::gauge_mode value = tables::gauge_mode::idle;
  EXPECT_FALSE (field::impl_of<mode>::type::assign (
      value, static_cast<tables::gauge_mode> (4)));
  EXPECT_TRUE (
      field::impl_of<mode>::type::assign (value, tables::gauge_mode::hold));
}

TEST (
    LumexProtocolBitFieldsTest,
    GivenAReflectedEnumerationUnderAMask_WhenAnUnknownValueComes_ThenInvalidValue)
{
  // Bits 1, 3 and 5 carry the value: 0x22 is 0b101 (hold), 0x08 is 0b010.
  using mode = field::mask_<field::u8_<>, 0x2A, tables::gauge_mode>;
  EXPECT_TRUE (*decoded<mode> (layout::bytes_t{ 0x22 })
               == tables::gauge_mode::hold);
  layout::decode_result_t<tables::gauge_mode> const unknown
      = decoded<mode> (layout::bytes_t{ 0x08 });
  ASSERT_FALSE (unknown.has_value ());
  EXPECT_TRUE (unknown.error () == protocol_errc::invalid_value);
  layout::bytes_t out;
  EXPECT_FALSE (
      (layout::encode<binary, mode> (out, static_cast<tables::gauge_mode> (2))
           .has_value ()));
}

TEST (
    LumexProtocolBitFieldsTest,
    GivenAReflectedEnumerationAsAPartOfASplit_WhenAnUnknownValueComes_ThenInvalidValue)
{
  using parts
      = field::split_<field::u8_<>, field::bits_<field::u8_<>, 0, 3>,
                      field::bits_<field::u8_<>, 4, 3, tables::gauge_mode>>;
  using value_type = field::value_of<parts>::type;
  layout::decode_result_t<value_type> const known
      = decoded<parts> (layout::bytes_t{ 0x52 });
  ASSERT_TRUE (known.has_value ());
  EXPECT_TRUE (field::get<1> (known->parts) == tables::gauge_mode::hold);
  layout::decode_result_t<value_type> const unknown
      = decoded<parts> (layout::bytes_t{ 0x32 });
  ASSERT_FALSE (unknown.has_value ());
  EXPECT_TRUE (unknown.error () == protocol_errc::invalid_value);
  value_type value = *known;
  field::get<1> (value.parts) = static_cast<tables::gauge_mode> (6);
  layout::bytes_t out;
  layout::encode_result_t const refused
      = layout::encode<binary, parts> (out, value);
  ASSERT_FALSE (refused.has_value ());
  EXPECT_TRUE (refused.error () == protocol_errc::invalid_value);
}

TEST (LumexProtocolBitFieldsTest,
      GivenAPlainEnumerationInARunOfBits_WhenAnyValueComes_ThenItIsKept)
{
  using lamp_bits = field::bits_<field::u8_<>, 0, 3, lamp>;
  EXPECT_EQ (static_cast<int> (*decoded<lamp_bits> (layout::bytes_t{ 7 })), 7);
  EXPECT_EQ (encoded<lamp_bits> (static_cast<lamp> (6)),
             (layout::bytes_t{ 6 }));
}

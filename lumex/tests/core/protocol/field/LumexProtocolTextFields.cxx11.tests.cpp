// The text fields (lumex/core/protocol/field/LumexProtocolTextFields.hpp):
// ascii_hex_, dec_text_, char_, text_<N> and token_ in a binary layout,
// their encodings, their limits and the values a decoder refuses.

#include <cstdint>
#include <ratio>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
using lumex::core::protocol::error::protocol_errc;
using lumex_tests::field_tables::bytes_of;
using lumex_tests::field_tables::text_of;
using lumex_tests::field_tables::view_of;

using binary = layout::binary_layout<>;

template <typename F>
std::string
encoded (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  EXPECT_TRUE ((layout::encode<binary, F> (out, value).has_value ()));
  return text_of (out);
}

template <typename F>
std::error_code
encode_error (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, F> (out, value);
  EXPECT_TRUE (out.empty ());
  return result.has_value () ? std::error_code () : result.error ();
}

template <typename F>
layout::decode_result_t<typename field::value_of<F>::type>
decoded (char const *text)
{
  layout::bytes_t const bytes = bytes_of (text);
  return layout::decode<binary, F> (view_of (bytes));
}
} // namespace

TEST (LumexProtocolTextFieldsTest,
      GivenAsciiHex_WhenRoundTripped_ThenUpperCaseZeroPaddedDigits)
{
  using hex4 = field::ascii_hex_<4>;
  EXPECT_EQ (encoded<hex4> (0x1A2F), "1A2F");
  EXPECT_EQ (encoded<hex4> (0x2), "0002");
  EXPECT_EQ (*decoded<hex4> ("1a2f"), 0x1A2Fu);
  EXPECT_EQ (*decoded<hex4> ("FFFF"), 0xFFFFu);
  EXPECT_TRUE (
      (std::is_same<field::ascii_hex_<2>::value_type, std::uint8_t>::value));
  EXPECT_TRUE (
      (std::is_same<field::ascii_hex_<9>::value_type, std::uint64_t>::value));
  EXPECT_EQ ((encoded<field::ascii_hex_<16>> (0xFEDCBA9876543210ull)),
             "FEDCBA9876543210");
  EXPECT_TRUE (field::ascii_hex_<4>::info ().kind
               == field::field_kind::hex_text);
  EXPECT_EQ (field::ascii_hex_<4>::info ().count, 4u);
}

TEST (LumexProtocolTextFieldsTest,
      GivenAsciiHex_WhenTheValueOrTheDigitsAreWrong_ThenInvalidValue)
{
  using hex3 = field::ascii_hex_<3>;
  EXPECT_TRUE (encode_error<hex3> (0x1000) == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<hex3> ("1G2").error () == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<hex3> (" 12").error () == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<hex3> ("12").error () == protocol_errc::truncated);
  using impl = field::impl_of<hex3>::type;
  std::uint16_t value = 0;
  EXPECT_TRUE (impl::assign (value, 0xFFF));
  EXPECT_FALSE (impl::assign (value, 0x1000));
}

TEST (LumexProtocolTextFieldsTest,
      GivenAScaledAsciiHex_WhenRoundTripped_ThenTheRawIsTheDigits)
{
  using temperature = field::ascii_hex_<4, field::scale<std::ratio<1, 10>>,
                                        field::unit::degree_celsius>;
  using impl = field::impl_of<temperature>::type;
  impl::value_type value = impl::value_type ();
  ASSERT_TRUE (impl::assign (value, 25.5));
  EXPECT_EQ (value.raw, 255u);
  EXPECT_EQ (encoded<temperature> (value), "00FF");
  EXPECT_DOUBLE_EQ (decoded<temperature> ("0100")->value (), 25.6);
  EXPECT_EQ (std::string (impl::info ().unit), "degC");
}

TEST (LumexProtocolTextFieldsTest,
      GivenDecimalText_WhenRoundTripped_ThenFixedWidthWithThePoint)
{
  using dec = field::dec_text_<6, 3>;
  using impl = field::impl_of<dec>::type;
  impl::value_type value = impl::value_type ();
  ASSERT_TRUE (impl::assign (value, 500.0));
  EXPECT_EQ (value.raw, 500000);
  EXPECT_EQ (encoded<dec> (value), "000500.000");
  ASSERT_TRUE (impl::assign (value, -12.5));
  EXPECT_EQ (encoded<dec> (value), "-00012.500");
  EXPECT_DOUBLE_EQ (decoded<dec> ("000500.000")->value (), 500.0);
  EXPECT_DOUBLE_EQ (decoded<dec> ("  -12.5   ")->value (), -12.5);
  EXPECT_DOUBLE_EQ (decoded<dec> ("+00001.000")->value (), 1.0);
  EXPECT_EQ (dec::binary_size::value, 10u);
  EXPECT_EQ ((field::dec_text_<4, 0>::binary_size::value), 4u);
  EXPECT_EQ ((encoded<field::dec_text_<4, 0>> (42)), "0042");
}

TEST (LumexProtocolTextFieldsTest,
      GivenDecimalText_WhenItDoesNotFitOrIsNoNumber_ThenInvalidValue)
{
  using dec = field::dec_text_<3, 1>;
  using impl = field::impl_of<dec>::type;
  impl::value_type value = impl::value_type ();
  ASSERT_TRUE (impl::assign (value, 999.9));
  EXPECT_EQ (encoded<dec> (value), "999.9");
  ASSERT_TRUE (impl::assign (value, 1000.0));
  EXPECT_TRUE (encode_error<dec> (value) == protocol_errc::invalid_value);
  ASSERT_TRUE (impl::assign (value, -99.9));
  EXPECT_EQ (encoded<dec> (value), "-99.9");
  ASSERT_TRUE (impl::assign (value, -100.0));
  EXPECT_TRUE (encode_error<dec> (value) == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("12.34").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("1x2.5").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("  .  ").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("   - ").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("1.2.3").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("12.3").error () == protocol_errc::truncated);
  // A number the field cannot write again is no value of it.
  EXPECT_TRUE (decoded<dec> ("12345").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<dec> ("-1000").error ()
               == protocol_errc::invalid_value);
  EXPECT_DOUBLE_EQ (decoded<dec> ("-99.9")->value (), -99.9);
}

TEST (LumexProtocolTextFieldsTest,
      GivenACharacter_WhenRoundTripped_ThenOneByte)
{
  EXPECT_EQ (encoded<field::char_> ('S'), "S");
  EXPECT_EQ (*decoded<field::char_> ("R"), 'R');
  EXPECT_TRUE (decoded<field::char_> ("").error ()
               == protocol_errc::truncated);
  EXPECT_TRUE (decoded<field::char_> ("RS").error ()
               == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolTextFieldsTest,
      GivenAFixedText_WhenRoundTripped_ThenNulPaddedToItsCapacity)
{
  using text6 = field::text_<6>;
  using impl = field::impl_of<text6>::type;
  impl::value_type value = impl::value_type ();
  ASSERT_TRUE (impl::assign (value, "AB"));
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode<binary, text6> (out, value).has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 'A', 'B', 0, 0, 0, 0 }));
  layout::decode_result_t<impl::value_type> const back
      = layout::decode<binary, text6> (view_of (out));
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (back->size (), 2u);
  EXPECT_TRUE (back->view () == field::text_view ("AB"));
  EXPECT_FALSE (impl::assign (value, "ABCDEFG"));
  ASSERT_TRUE (impl::assign (value, "ABCDEF"));
  EXPECT_EQ (encoded<text6> (value), "ABCDEF");
  // A text after the first NUL is not part of the value.
  layout::bytes_t const inner{ 'X', 0, 'Y', 0, 0, 0 };
  EXPECT_TRUE ((layout::decode<binary, text6> (view_of (inner)))->view ()
               == field::text_view ("X"));
  EXPECT_EQ (text6::info ().count, 6u);
}

TEST (LumexProtocolTextFieldsTest,
      GivenATokenInABinaryLayout_WhenDecoded_ThenItIsTheRestOfTheMessage)
{
  // The token is a view into the message: the buffer outlives it.
  layout::bytes_t const message = bytes_of ("rest, of it");
  layout::decode_result_t<field::text_view> const value
      = layout::decode<binary, field::token_> (view_of (message));
  ASSERT_TRUE (value.has_value ());
  EXPECT_TRUE (*value == field::text_view ("rest, of it"));
  EXPECT_EQ (static_cast<void const *> (value->data ()),
             static_cast<void const *> (message.data ()));
  EXPECT_EQ (encoded<field::token_> (field::text_view ("abc")), "abc");
  layout::bytes_t const nothing;
  EXPECT_TRUE (
      (layout::decode<binary, field::token_> (view_of (nothing)))->empty ());
}

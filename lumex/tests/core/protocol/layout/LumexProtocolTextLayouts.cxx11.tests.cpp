// The text layouts (lumex/core/protocol/layout/LumexProtocolTextLayouts.hpp):
// fixed-width hexadecimal and decimal digits, and the tokenized layout with
// its key prefix, separators, trimmed spaces, hexadecimal radix, floating
// tokens and every error path.

#include <cstdint>
#include <limits>
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
using tables::bytes_of;
using tables::text_of;
using tables::view_of;

typedef layout::fixed_width_text_layout<16> hex;
typedef layout::fixed_width_text_layout<10> decimal;
typedef layout::tokenized_text_layout<','> tokens;

LUMEX_PROTOCOL_LAYOUT (mixed_t, (u8_, i16_be_, ascii_hex_<3>, char_, text_<3>),
                       ("a", "b", "c", "d", "e"));
LUMEX_PROTOCOL_LAYOUT (numbers_t, (u8_, i16_be_, dec_text_<3, 1>),
                       ("a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (floats_t, (f32_le_, f64_be_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (words_t, (token_, u8_, token_), ("a", "b", "c"));

typedef field::record_of<mixed_t::fields_type>::type mixed;
typedef field::record_of<numbers_t::fields_type>::type numbers;

mixed
make_mixed ()
{
  mixed value = mixed ();
  field::get<0> (value) = 10;
  field::get<1> (value) = -2;
  field::get<2> (value) = 0xABC;
  field::get<3> (value) = 'X';
  field::get<4> (value).assign ("ok");
  return value;
}

template <typename Layout, typename F>
std::string
encoded (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<Layout, F> (out, value);
  EXPECT_TRUE (result.has_value ());
  return text_of (out);
}

template <typename Layout, typename F>
layout::decode_result_t<typename field::value_of<F>::type>
decoded (char const *text)
{
  layout::bytes_t const bytes = bytes_of (text);
  return layout::decode<Layout, F> (view_of (bytes));
}

typedef tables::gauge_text text_protocol;
typedef tables::gauge::SET_LEVEL_t set_level;

layout::decode_result_t<layout::request_of<set_level>::type>
decode_set_level (char const *text)
{
  layout::bytes_t const bytes = bytes_of (text);
  return layout::decode_request<text_protocol, set_level> (view_of (bytes));
}
} // namespace

TEST (LumexProtocolTextLayoutsTest,
      GivenHexDigits_WhenAMessageIsRoundTripped_ThenEveryFieldHasItsWidth)
{
  mixed const value = make_mixed ();
  std::string const text = encoded<hex, mixed_t> (value);
  EXPECT_EQ (text, std::string ("0AFFFEABCXok\0", 13));
  layout::decode_result_t<mixed> const back
      = layout::decode<hex, mixed_t> (view_of (bytes_of ("0AFFFEABCXok")));
  EXPECT_TRUE (back.error () == protocol_errc::truncated);
  layout::bytes_t const full (text.begin (), text.end ());
  EXPECT_TRUE ((*layout::decode<hex, mixed_t> (view_of (full)) == value));
  EXPECT_TRUE ((decoded<hex, mixed_t> ("0AFFFEABCXok\0\0")).error ()
               == protocol_errc::truncated);
}

TEST (LumexProtocolTextLayoutsTest,
      GivenDecimalDigits_WhenAMessageIsRoundTripped_ThenZeroPaddedWithASign)
{
  numbers value = numbers ();
  field::get<0> (value) = 7;
  field::get<1> (value) = -2;
  field::get<2> (value).raw = 125;
  EXPECT_EQ ((encoded<decimal, numbers_t> (value)), "007-00002012.5");
  EXPECT_TRUE ((*decoded<decimal, numbers_t> ("007-00002012.5") == value));
  numbers positive = value;
  field::get<1> (positive) = 2;
  EXPECT_TRUE ((*decoded<decimal, numbers_t> ("007+00002012.5") == positive));
  EXPECT_TRUE ((*decoded<decimal, numbers_t> ("007 -0002012.5") == value));
  EXPECT_TRUE ((decoded<decimal, numbers_t> ("007-000x2012.5")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<decimal, numbers_t> ("256-00002012.5")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<decimal, numbers_t> ("007-00002012.5 ")).error ()
               == protocol_errc::trailing_bytes);
  EXPECT_EQ (decimal::integer_width (3, false), 8u);
  EXPECT_EQ (decimal::integer_width (8, true), 21u);
  EXPECT_EQ (hex::integer_width (3, true), 6u);
}

TEST (LumexProtocolTextLayoutsTest,
      GivenATokenizedRow_WhenEncoded_ThenTheKeyAndTheRawValuesAsTokens)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<text_protocol, set_level> (out, 12.5, 3, -2)
           .has_value ()));
  EXPECT_EQ (text_of (out), "2,1250,3,-2");
  layout::request_of<set_level>::type const value
      = *decode_set_level ("2, 1250 ,3,\t-2");
  EXPECT_DOUBLE_EQ (field::get<0> (value).value (), 12.5);
  EXPECT_EQ (field::get<2> (value), -2);
  out.clear ();
  ASSERT_TRUE (
      (layout::encode_request<text_protocol, tables::gauge::PING_t> (out)
           .has_value ()));
  EXPECT_EQ (text_of (out), "1");
  layout::bytes_t const ping = bytes_of ("1");
  EXPECT_TRUE ((layout::decode_request<text_protocol, tables::gauge::PING_t> (
                    view_of (ping)))
                   .has_value ());
  layout::bytes_t const ping_comma = bytes_of ("1,");
  EXPECT_TRUE ((layout::decode_request<text_protocol, tables::gauge::PING_t> (
                    view_of (ping_comma)))
                   .error ()
               == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolTextLayoutsTest,
      GivenATokenizedRow_WhenTheTextIsWrong_ThenEachErrorHasItsCode)
{
  EXPECT_TRUE (decode_set_level ("3,1250,3,-2").error ()
               == protocol_errc::unknown_frame);
  EXPECT_TRUE (decode_set_level ("").error () == protocol_errc::unknown_frame);
  EXPECT_TRUE (decode_set_level ("2").error () == protocol_errc::truncated);
  EXPECT_TRUE (decode_set_level ("2,1250,3").error ()
               == protocol_errc::truncated);
  EXPECT_TRUE (decode_set_level ("2,1250,3,-2,9").error ()
               == protocol_errc::trailing_bytes);
  EXPECT_TRUE (decode_set_level ("2,,3,-2").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,70000,3,-2").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,-1,3,-2").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,12.5,3,-2").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,1250,3,-2147483649").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,1250,3,99999999999999999999999").error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (decode_set_level ("2,1250,3,-2147483648").has_value ());
}

TEST (LumexProtocolTextLayoutsTest,
      GivenATextKeyRow_WhenRoundTripped_ThenTheMnemonicIsThePrefix)
{
  typedef tables::gauge::SET_NAME_t set_name;
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<text_protocol, set_name> (out, "ABC", 12.5)
           .has_value ()));
  EXPECT_EQ (text_of (out), "NM,ABC,0012.5");
  layout::request_of<set_name>::type const value
      = *layout::decode_request<text_protocol, set_name> (view_of (out));
  EXPECT_TRUE (field::get<0> (value).view () == field::text_view ("ABC"));
  EXPECT_EQ (field::get<1> (value).raw, 125);
  layout::bytes_t const loose = bytes_of ("NM , A ,12.5");
  layout::request_of<set_name>::type const trimmed
      = *layout::decode_request<text_protocol, set_name> (view_of (loose));
  EXPECT_TRUE (field::get<0> (trimmed).view () == field::text_view ("A"));
  EXPECT_EQ (field::get<1> (trimmed).raw, 125);
  // A token never holds the separator; a text longer than its field is
  // refused.
  out.clear ();
  EXPECT_TRUE (
      (layout::encode_request<text_protocol, set_name> (out, "A,B", 1.0))
          .error ()
      == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  layout::bytes_t const long_name = bytes_of ("NM,ABCDEFG,1.0");
  EXPECT_TRUE (
      (layout::decode_request<text_protocol, set_name> (view_of (long_name)))
          .error ()
      == protocol_errc::invalid_value);
  layout::bytes_t const wide_number = bytes_of ("NM,A,12345.0");
  EXPECT_TRUE (
      (layout::decode_request<text_protocol, set_name> (view_of (wide_number)))
          .error ()
      == protocol_errc::invalid_value);
}

TEST (LumexProtocolTextLayoutsTest,
      GivenFloatingTokens_WhenRoundTripped_ThenTheValuesComeBackExactly)
{
  typedef field::record_of<floats_t::fields_type>::type floats;
  floats value = floats ();
  field::get<0> (value) = 1.5f;
  field::get<1> (value) = -1e-300;
  std::string const text = encoded<tokens, floats_t> (value);
  EXPECT_EQ (text.substr (0, 5), "1.5,-");
  EXPECT_TRUE ((*decoded<tokens, floats_t> (text.c_str ()) == value));
  field::get<0> (value) = 0.1f;
  field::get<1> (value) = std::numeric_limits<double>::max ();
  std::string const extreme = encoded<tokens, floats_t> (value);
  EXPECT_TRUE ((*decoded<tokens, floats_t> (extreme.c_str ()) == value));
  EXPECT_TRUE ((decoded<tokens, floats_t> ("1.5,abc")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<tokens, floats_t> ("1.5,1e")).error ()
               == protocol_errc::invalid_value);
  // A number out of the range of its type is no value of it, and an
  // infinity has no token (mutation M25).
  EXPECT_TRUE ((decoded<tokens, floats_t> ("1.5,1e400")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<tokens, floats_t> ("1e39,1.5")).error ()
               == protocol_errc::invalid_value);
  field::get<1> (value) = std::numeric_limits<double>::infinity ();
  layout::bytes_t out;
  EXPECT_TRUE ((layout::encode<tokens, floats_t> (out, value)).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexProtocolTextLayoutsTest,
      GivenAHexRadixAndAKeySeparator_WhenEncoded_ThenTheBitsAreHexTokens)
{
  typedef layout::tokenized_text_layout<',', ':', 16> hex_tokens;
  mixed const value = make_mixed ();
  std::string const text = encoded<hex_tokens, mixed_t> (value);
  EXPECT_EQ (text, "A,FFFE,ABC,X,ok");
  EXPECT_TRUE ((*decoded<hex_tokens, mixed_t> ("a,fffe,abc,X,ok") == value));
  EXPECT_TRUE ((decoded<hex_tokens, mixed_t> ("100,FFFE,ABC,X,ok")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<hex_tokens, mixed_t> ("A,FFFE,ABC,XY,ok")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<hex_tokens, mixed_t> ("A,FFFE,ABC,X,okay")).error ()
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolTextLayoutsTest,
      GivenTokensWithoutTrimming_WhenSpacesSurroundThem_ThenTheyAreRefused)
{
  typedef layout::tokenized_text_layout<';', ';', 10, false> strict;
  EXPECT_EQ (
      (encoded<strict, numbers_t> (*decoded<strict, numbers_t> ("7;-2;12.5"))),
      "7;-2;012.5");
  EXPECT_TRUE ((decoded<strict, numbers_t> ("7; -2;12.5")).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decoded<tokens, numbers_t> ("7, -2 ,12.5")).has_value ());
}

TEST (LumexProtocolTextLayoutsTest,
      GivenTextTokens_WhenDecoded_ThenTheyAreViewsIntoTheMessage)
{
  layout::bytes_t const bytes = bytes_of ("first,9,last one");
  layout::decode_result_t<field::record_of<words_t::fields_type>::type> const
      value
      = layout::decode<tokens, words_t> (view_of (bytes));
  ASSERT_TRUE (value.has_value ());
  EXPECT_TRUE (field::get<0> (*value) == field::text_view ("first"));
  EXPECT_EQ (field::get<1> (*value), 9u);
  EXPECT_TRUE (field::get<2> (*value) == field::text_view ("last one"));
  EXPECT_EQ (static_cast<void const *> (field::get<2> (*value).data ()),
             static_cast<void const *> (bytes.data () + 8));
  EXPECT_EQ ((encoded<tokens, words_t> (*value)), "first,9,last one");
}

// A finding of fuzz.protocol.layouts: a NUL inside a text token cut the
// value before a space, and the space was trimmed when the encoded value was
// read again. Such a token is refused now, and so is a value with a space at
// an end when the layout trims.
TEST (LumexProtocolTextLayoutsTest,
      GivenANulOrAnEdgeSpaceInATextToken_WhenDecodedOrEncoded_ThenInvalidValue)
{
  typedef tables::gauge::SET_NAME_t set_name;
  layout::bytes_t const with_nul{ 'N', 'M', ',', '5', ' ',
                                  0,   'N', 'M', ',', '5' };
  EXPECT_TRUE (
      (layout::decode_request<text_protocol, set_name> (view_of (with_nul)))
          .error ()
      == protocol_errc::invalid_value);
  layout::bytes_t out;
  EXPECT_TRUE (
      (layout::encode_request<text_protocol, set_name> (out, "5 ", 1.0))
          .error ()
      == protocol_errc::invalid_value);
  EXPECT_TRUE (
      (layout::encode_request<text_protocol, set_name> (out, " 5", 1.0))
          .error ()
      == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  ASSERT_TRUE (
      (layout::encode_request<text_protocol, set_name> (out, "a b", 1.0))
          .has_value ());
  EXPECT_EQ (text_of (out), "NM,a b,0001.0");
  // Without trimming, spaces are part of the tokens (the views point into
  // the message, which outlives them).
  typedef layout::tokenized_text_layout<';', ';', 10, false> strict;
  layout::bytes_t const spaced = bytes_of (" x ;7; y");
  layout::decode_result_t<field::record_of<words_t::fields_type>::type> const
      words
      = layout::decode<strict, words_t> (view_of (spaced));
  ASSERT_TRUE (words.has_value ());
  EXPECT_EQ ((encoded<strict, words_t> (*words)), " x ;7; y");
}

// Edge cases of the layouts found by the review of the field types and
// layouts (lumex/core/protocol/layout): the largest float as a token, an
// empty message as one empty token, the side of by_length_ checked on encode,
// a NUL inside a fixed text, the 64-bit decimal limit, an array value above
// its capacity, a key or a value holding a separator, trailing spaces.

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

using binary = layout::binary_layout<>;
using hex = layout::fixed_width_text_layout<16>;
using decimal = layout::fixed_width_text_layout<10>;
using tokens = layout::tokenized_text_layout<','>;

LUMEX_PROTOCOL_LAYOUT (f32_value_t, (f32_le_), ("value"));
LUMEX_PROTOCOL_LAYOUT (one_token_t, (token_), ("value"));
LUMEX_PROTOCOL_LAYOUT (one_text_t, (text_<4>), ("value"));
LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, u8_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (quad_t, (u8_, u8_, u8_, u8_), ("a", "b", "c", "d"));
// The short form is the longer one: a value of either form whose encoding
// lands on the other side of Min cannot be read back.
LUMEX_PROTOCOL_LAYOUT (crossed_t, (u8_, by_length_<4, quad_t, pair_t>),
                       ("tag", "body"));
LUMEX_PROTOCOL_LAYOUT (sized_t, (u8_, by_length_<3, pair_t, quad_t>),
                       ("tag", "body"));
LUMEX_PROTOCOL_LAYOUT (wide_t, (u64_be_), ("value"));
LUMEX_PROTOCOL_LAYOUT (counted_t, (counted_<u8_, u8_, 2>), ("values"));
LUMEX_PROTOCOL_LAYOUT (computed_t,
                       (u8_, computed_count_<u8_, count_from_<0>, 2>),
                       ("count", "values"));
LUMEX_PROTOCOL_LAYOUT (signed_count_t, (counted_<u8_, i8_, 4>), ("values"));
LUMEX_PROTOCOL_LAYOUT (letter_t, (char_, token_), ("letter", "word"));

struct colon_header
{
  using layout_type = layout::tokenized_text_layout<',', ':'>;
};

// clang-format off
#define COLON_ROWS(X, X_AS) \
  X_AS (SPLIT_KEY, "A:B", "A:B", "A key holding the key separator", (u8_), \
        ("value"), (), ()) \
  X_AS (PLAIN_KEY, "AB", "AB", "A plain key", (u8_), ("value"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (colon_protocol, tables::codec, colon_header,
                       COLON_ROWS);

template <typename Layout, typename F>
std::error_code
encode_error (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<Layout, F> (out, value);
  EXPECT_TRUE (result.has_value () || out.empty ());
  return result.has_value () ? std::error_code () : result.error ();
}
} // namespace

TEST (LumexProtocolLayoutEdgesTest,
      GivenTheLargestFloat_WhenRoundTrippedAsAToken_ThenItComesBack)
{
  using value_type = field::record_of<f32_value_t::fields_type>::type;
  float const extremes[] = { (std::numeric_limits<float>::max) (),
                             -(std::numeric_limits<float>::max) (),
                             (std::numeric_limits<float>::min) () };
  for (float extreme : extremes)
    {
      value_type value = value_type ();
      field::get<0> (value) = extreme;
      layout::bytes_t out;
      ASSERT_TRUE (
          (layout::encode<tokens, f32_value_t> (out, value)).has_value ());
      layout::decode_result_t<value_type> const back
          = layout::decode<tokens, f32_value_t> (view_of (out));
      ASSERT_TRUE (back.has_value ()) << text_of (out);
      EXPECT_EQ (field::get<0> (*back), extreme) << text_of (out);
    }
  layout::bytes_t const above = bytes_of ("3.5e38");
  EXPECT_TRUE ((layout::decode<tokens, f32_value_t> (view_of (above))).error ()
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenAnEmptyMessage_WhenTheLayoutExpectsOneToken_ThenItIsAnEmptyToken)
{
  using token_value = field::record_of<one_token_t::fields_type>::type;
  token_value token = token_value ();
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode<tokens, one_token_t> (out, token)).has_value ());
  EXPECT_TRUE (out.empty ());
  layout::decode_result_t<token_value> const back
      = layout::decode<tokens, one_token_t> (view_of (out));
  ASSERT_TRUE (back.has_value ());
  EXPECT_TRUE (field::get<0> (*back).empty ());

  using text_value = field::record_of<one_text_t::fields_type>::type;
  layout::bytes_t const spaces = bytes_of ("   ");
  layout::decode_result_t<text_value> const trimmed
      = layout::decode<tokens, one_text_t> (view_of (spaces));
  ASSERT_TRUE (trimmed.has_value ());
  EXPECT_EQ (field::get<0> (*trimmed).size (), 0u);
  layout::bytes_t again;
  ASSERT_TRUE (
      (layout::encode<tokens, one_text_t> (again, *trimmed)).has_value ());
  EXPECT_TRUE (
      (layout::decode<tokens, one_text_t> (view_of (again))).has_value ());
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenAByLengthValueOnTheWrongSideOfMin_WhenEncoded_ThenInvalidValue)
{
  using crossed = field::record_of<crossed_t::fields_type>::type;
  crossed value = crossed ();
  field::get<1> (value).emplace<0> ();
  EXPECT_TRUE ((encode_error<binary, crossed_t> (value))
               == protocol_errc::invalid_value);
  field::get<1> (value).emplace<1> ();
  EXPECT_TRUE ((encode_error<binary, crossed_t> (value))
               == protocol_errc::invalid_value);
  // The units are those of the layout: bytes, characters, tokens.
  using sized = field::record_of<sized_t::fields_type>::type;
  sized fine = sized ();
  field::get<1> (fine).emplace<1> ();
  EXPECT_FALSE ((encode_error<binary, sized_t> (fine)));
  EXPECT_FALSE ((encode_error<tokens, sized_t> (fine)));
  EXPECT_FALSE ((encode_error<hex, sized_t> (fine)));
  field::get<1> (fine).emplace<0> ();
  EXPECT_FALSE ((encode_error<binary, sized_t> (fine)));
  EXPECT_FALSE ((encode_error<tokens, sized_t> (fine)));
  // Two bytes are four hexadecimal characters: the short form is long in a
  // hexadecimal layer.
  EXPECT_TRUE ((encode_error<hex, sized_t> (fine))
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenANulInsideAFixedText_WhenAssignedOrEncoded_ThenInvalidValue)
{
  using record_type = field::impl_of<one_text_t>::type::value_type;
  record_type value = record_type ();
  EXPECT_FALSE (field::get<0> (value).assign (field::text_view ("A\0B", 3)));
  EXPECT_FALSE (field::impl_of<field::text_<4>>::type::assign (
      field::get<0> (value), field::text_view ("A\0B", 3)));
  field::get<0> (value).chars[0] = 'A';
  field::get<0> (value).chars[1] = '\0';
  field::get<0> (value).chars[2] = 'B';
  field::get<0> (value).length = 3;
  EXPECT_TRUE ((encode_error<binary, one_text_t> (value))
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((encode_error<tokens, one_text_t> (value))
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenTheLargest64BitDecimal_WhenDecoded_ThenOneMoreIsRefused)
{
  layout::bytes_t const largest = bytes_of ("18446744073709551615");
  layout::bytes_t const above = bytes_of ("18446744073709551616");
  layout::decode_result_t<field::record_of<wide_t::fields_type>::type> const
      top
      = layout::decode<decimal, wide_t> (view_of (largest));
  ASSERT_TRUE (top.has_value ());
  EXPECT_EQ (field::get<0> (*top),
             (std::numeric_limits<std::uint64_t>::max) ());
  EXPECT_TRUE ((layout::decode<decimal, wide_t> (view_of (above))).error ()
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((layout::decode<tokens, wide_t> (view_of (above))).error ()
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenAnArrayValueAboveItsCapacity_WhenEncoded_ThenInvalidValue)
{
  using record_type = field::record_of<counted_t::fields_type>::type;
  record_type value = record_type ();
  field::get<0> (value).count = 3;
  EXPECT_TRUE ((encode_error<binary, counted_t> (value))
               == protocol_errc::invalid_value);
  // The earlier field agrees with the size, both above the capacity: the
  // elements past the capacity are never read.
  using computed = field::record_of<computed_t::fields_type>::type;
  computed many = computed ();
  field::get<0> (many) = 3;
  field::get<1> (many).count = 3;
  EXPECT_TRUE ((encode_error<binary, computed_t> (many))
               == protocol_errc::invalid_value);
  // A negative count is invalid, not a large one.
  layout::bytes_t const negative{ 0xFE, 1, 2 };
  EXPECT_TRUE (
      (layout::decode<binary, signed_count_t> (view_of (negative))).error ()
      == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenAKeyOrAValueHoldingASeparator_WhenEncoded_ThenInvalidValue)
{
  layout::bytes_t out;
  EXPECT_TRUE (
      (layout::encode_request<colon_protocol, colon_protocol::SPLIT_KEY_t> (
           out, 1))
          .error ()
      == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  ASSERT_TRUE (
      (layout::encode_request<colon_protocol, colon_protocol::PLAIN_KEY_t> (
           out, 1))
          .has_value ());
  EXPECT_EQ (text_of (out), "AB:1");
  using letter = field::record_of<letter_t::fields_type>::type;
  letter value = letter ();
  field::get<0> (value) = ',';
  field::get<1> (value) = field::text_view ("word");
  EXPECT_TRUE ((encode_error<tokens, letter_t> (value))
               == protocol_errc::invalid_value);
  field::get<0> (value) = 'x';
  field::get<1> (value) = field::text_view ("a,b");
  EXPECT_TRUE ((encode_error<tokens, letter_t> (value))
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolLayoutEdgesTest,
      GivenTrailingSpacesInATokenizedMessage_WhenDecoded_ThenTheyAreTrimmed)
{
  layout::bytes_t const message = bytes_of ("NM,AB  ,1.0");
  layout::decode_result_t<
      layout::request_of<tables::gauge::SET_NAME_t>::type> const value
      = layout::decode_request<tables::gauge_text, tables::gauge::SET_NAME_t> (
          view_of (message));
  ASSERT_TRUE (value.has_value ());
  EXPECT_TRUE (field::get<0> (*value).view () == field::text_view ("AB"));
  layout::bytes_t const words = bytes_of ("x,last  ");
  layout::decode_result_t<field::record_of<letter_t::fields_type>::type> const
      word
      = layout::decode<tokens, letter_t> (view_of (words));
  ASSERT_TRUE (word.has_value ());
  EXPECT_TRUE (field::get<1> (*word) == field::text_view ("last"));
}

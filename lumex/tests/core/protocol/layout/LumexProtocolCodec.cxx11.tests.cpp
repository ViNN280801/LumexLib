// The encode and decode functions of a protocol row
// (lumex/core/protocol/layout/LumexProtocolCodec.hpp): the layout comes
// from the header type of the layer, so the same logical command (the same
// command type) is bytes in one layer and digits or tokens in another; the
// replies; and the extension points of another layer (its vocabulary, its
// reply kinds, its options in the run-time entries).

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;
using lumex::core::protocol::error::protocol_errc;
using tables::text_of;
using tables::view_of;

using set_level = tables::gauge::SET_LEVEL_t;
using set_level_request = layout::request_of<set_level>::type;

template <typename P>
layout::bytes_t
encode_set_level ()
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode_request<P, set_level> (out, 12.5, 3, -2);
  EXPECT_TRUE (result.has_value ());
  return out;
}

template <typename P>
set_level_request
decode_set_level (layout::bytes_t const &bytes)
{
  layout::decode_result_t<set_level_request> const result
      = layout::decode_request<P, set_level> (view_of (bytes));
  EXPECT_TRUE (result.has_value ());
  return result.has_value () ? *result : set_level_request ();
}

// A layer above the core: a reply kind, an option and their vocabulary.
namespace dialogue
{
template <typename Ack, typename Nak> struct ack_nak
{
  using lumex_protocol_reply = dsl::reply_tag;
  using ack_type = Ack;
  using nak_type = Nak;
};

struct rate_limit_kind
{
};

struct rate_limit_t
{
  using option_kind = rate_limit_kind;
  std::uint32_t milliseconds;

  static LUMEX_CONSTEXPR char const *
  option_name ()
  {
    return "rate_limit";
  }

  std::int64_t
  view_value () const
  {
    return milliseconds;
  }
};

struct vocabulary : dsl::vocabulary
{
  template <typename Ack, typename Nak>
  using ack_nak = dialogue::ack_nak<Ack, Nak>;

  // The options of a table are constant expressions: the maker is
  // constexpr and the option a literal type.
  static constexpr rate_limit_t
  rate_limit (std::uint32_t milliseconds)
  {
    return rate_limit_t{ milliseconds };
  }
};

struct header
{
  using lumex_vocabulary = vocabulary;
};

// clang-format off
#define DIALOGUE_ROWS(X, X_AS) \
  X (START, 0x01, "Start", (u8_), ("channel"), (ack_nak<u8_, u16_le_>), \
     (rate_limit (50), timeout (200))) \
  X (STOP, 0x02, "Stop", (), (), (u8_), ())
#define DIALOGUE_LAYER_ROWS(X, X_AS) \
  X (PAUSE, 0x03, "Pause", (), (), (ack_nak<u8_, u8_>), (rate_limit (75)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (dialogue_protocol, tables::codec, header,
                       DIALOGUE_ROWS);
LUMEX_PROTOCOL_EXTEND (dialogue_layer, dialogue_protocol, DIALOGUE_LAYER_ROWS);
} // namespace dialogue
} // namespace

TEST (LumexProtocolCodecTest,
      GivenTheLayersOfAProtocol_WhenTheirLayoutIsRead_ThenTheHeaderChoosesIt)
{
  EXPECT_TRUE ((std::is_same<layout::layout_of<tables::gauge>::type,
                             layout::binary_layout<>>::value));
  EXPECT_TRUE ((std::is_same<layout::layout_of<tables::gauge_hex>::type,
                             layout::fixed_width_text_layout<16>>::value));
  EXPECT_TRUE ((std::is_same<layout::layout_of<tables::gauge_text>::type,
                             layout::tokenized_text_layout<','>>::value));
  // The layers re-bind the rows without restating them.
  EXPECT_TRUE (
      (std::is_same<tables::gauge_hex::SET_LEVEL_t, set_level>::value));
  EXPECT_TRUE ((std::is_same<tables::gauge_text::READ_SINK_t,
                             tables::gauge::READ_SINK_t>::value));
  EXPECT_EQ (dsl::command_count<tables::gauge_decimal>::value, 5u);
}

TEST (
    LumexProtocolCodecTest,
    GivenOneLogicalCommand_WhenEncodedInEveryLayer_ThenEachLayoutDecodesTheSameValues)
{
  layout::bytes_t const binary = encode_set_level<tables::gauge> ();
  layout::bytes_t const hex = encode_set_level<tables::gauge_hex> ();
  layout::bytes_t const decimal = encode_set_level<tables::gauge_decimal> ();
  layout::bytes_t const text = encode_set_level<tables::gauge_text> ();
  EXPECT_EQ (binary,
             (layout::bytes_t{ 0x04, 0xE2, 3, 0xFE, 0xFF, 0xFF, 0xFF }));
  EXPECT_EQ (text_of (hex), "04E203FFFFFFFE");
  EXPECT_EQ (text_of (decimal), "01250003-0000000002");
  EXPECT_EQ (text_of (text), "2,1250,3,-2");
  set_level_request const expected = decode_set_level<tables::gauge> (binary);
  EXPECT_DOUBLE_EQ (field::get<0> (expected).value (), 12.5);
  EXPECT_TRUE (decode_set_level<tables::gauge_hex> (hex) == expected);
  EXPECT_TRUE (decode_set_level<tables::gauge_decimal> (decimal) == expected);
  EXPECT_TRUE (decode_set_level<tables::gauge_text> (text) == expected);
  // A layer reads its own encoding only.
  EXPECT_FALSE (
      (layout::decode_request<tables::gauge_hex, set_level> (view_of (binary)))
          .has_value ());
}

TEST (
    LumexProtocolCodecTest,
    GivenRowReplies_WhenRoundTripped_ThenAFieldReplyIsItsValueAndALayoutARecord)
{
  using read_word = tables::gauge::READ_WORD_t;
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_reply<tables::gauge, read_word> (out, 0xBEEF)
                    .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0xEF, 0xBE }));
  EXPECT_EQ ((*layout::decode_reply<tables::gauge, read_word> (view_of (out))),
             0xBEEFu);
  out.clear ();
  ASSERT_TRUE (
      (layout::encode_reply<tables::gauge_text, read_word> (out, 0xBEEF)
           .has_value ()));
  EXPECT_EQ (text_of (out), "4,48879");
  layout::reply_value_of<set_level>::type point
      = layout::reply_value_of<set_level>::type ();
  field::get<0> (point) = -7;
  field::get<1> (point) = 1;
  out.clear ();
  ASSERT_TRUE ((layout::encode_reply<tables::gauge_hex, set_level> (out, point)
                    .has_value ()));
  EXPECT_EQ (text_of (out), "FFF901");
  EXPECT_TRUE (
      (*layout::decode_reply<tables::gauge_hex, set_level> (view_of (out))
       == point));
}

TEST (LumexProtocolCodecTest,
      GivenAnArgumentThatDoesNotFit_WhenARequestIsMade_ThenInvalidValue)
{
  layout::decode_result_t<set_level_request> const request
      = layout::make_request<set_level> (700.0, 3, -2);
  ASSERT_FALSE (request.has_value ());
  EXPECT_TRUE (request.error () == protocol_errc::invalid_value);
  layout::decode_result_t<set_level_request> const fits
      = layout::make_request<set_level> (655.35, 255, -2147483647 - 1);
  EXPECT_TRUE (fits.has_value ());
}

TEST (
    LumexProtocolCodecTest,
    GivenAnotherLayersVocabulary_WhenATableUsesIt_ThenItsWordsNeedNoNamespace)
{
  using dialogue::dialogue_layer;
  using dialogue::dialogue_protocol;
  EXPECT_TRUE ((std::is_same<dsl::vocabulary_of<dialogue::header>::type,
                             dialogue::vocabulary>::value));
  EXPECT_TRUE ((std::is_same<dsl::vocabulary_of<tables::binary_header>::type,
                             dsl::vocabulary>::value));
  // The reply kind gets the names of the table as written; the dialogue
  // resolves them (field::resolve, field::impl_of).
  using start_reply = dialogue_protocol::START_t::reply_type;
  EXPECT_TRUE (
      (std::is_same<start_reply,
                    dialogue::ack_nak<dsl::plain::u8_<>,
                                      dsl::plain::u16_le_<>>>::value));
  EXPECT_TRUE ((std::is_same<field::resolve<start_reply::nak_type>::type,
                             field::u16_le_<>>::value));
  EXPECT_TRUE (
      dsl::is_reply_kind<dialogue_protocol::START_t::reply_type>::value);
  EXPECT_FALSE (
      dsl::is_reply_kind<dialogue_protocol::STOP_t::reply_type>::value);
  EXPECT_EQ (dsl::get_option<dialogue::rate_limit_kind> (
                 dialogue_protocol::START_t::options ())
                 .milliseconds,
             50u);
  // The layer keeps the vocabulary of its base header.
  EXPECT_EQ (dsl::command_count<dialogue_layer>::value, 3u);
  EXPECT_EQ (dsl::get_option<dialogue::rate_limit_kind> (
                 dialogue_layer::PAUSE_t::options ())
                 .milliseconds,
             75u);
}

TEST (LumexProtocolCodecTest,
      GivenOptionsOfAnotherLayer_WhenEntriesAreRead_ThenTheyAreListedByName)
{
  auto const entries = dsl::command_entries<dialogue::dialogue_layer> ();
  ASSERT_EQ (entries.size (), 3u);
  ASSERT_EQ (entries[0].options.extra.size (), 1u);
  EXPECT_EQ (std::string (entries[0].options.extra[0].name), "rate_limit");
  EXPECT_EQ (entries[0].options.extra[0].value, 50);
  EXPECT_EQ (entries[0].options.timeout_ms, 200u);
  EXPECT_EQ (entries[1].options.extra.size (), 0u);
  ASSERT_EQ (entries[2].options.extra.size (), 1u);
  EXPECT_EQ (entries[2].options.extra[0].value, 75);
  EXPECT_TRUE (entries[0].has_reply);
}

TEST (LumexProtocolCodecTest,
      GivenTheSinkLayout_WhenRead_ThenItHasFortyFieldsOfEveryKind)
{
  using fields = tables::sink_t::fields_type;
  EXPECT_EQ (fields::count::value, 40u);
  EXPECT_EQ (tables::sink_t::field_names ().size (), 40u);
  EXPECT_EQ (std::string (tables::sink_t::field_names ()[39]), "tail");
  field::field_info_t const info
      = field::impl_of<tables::sink_t>::type::info ();
  EXPECT_TRUE (info.kind == field::field_kind::group);
  EXPECT_EQ (info.count, 40u);
  EXPECT_EQ (info.binary_size, 0u);
}

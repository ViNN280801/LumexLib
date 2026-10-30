// The whole message of a protocol row
// (lumex/core/protocol/layout/LumexProtocolMessage.hpp): the header and the
// key as the key reader of the layer reads them, then the fields, in every
// kind of layout (binary with the key first, after a header, wider than a
// byte, a composite key in little-endian order, hexadecimal and decimal
// fixed-width digits, tokenized text); the header type that writes the
// header itself from the session; the decoders that check the key of the
// row; and the writer twins of the key readers.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace message_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;

struct codec
{
};

// The key is the first byte.
struct first_header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The key is the first two bytes, most significant first.
struct wide_header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 2;
};

// Three header bytes (an address, a process, an attribute), then the key.
struct after_header
{
  using key_reader = index::key_at<3, 1>;
};

// The key as two hexadecimal digits, the fields as digits.
struct hex_header
{
  using layout_type = layout::fixed_width_text_layout<16>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// One header character, then the key as three decimal digits.
struct decimal_header
{
  using layout_type = layout::fixed_width_text_layout<10>;
  using key_reader = index::key_at_digits<1, 3, 10>;
};

// The key as the first token.
struct text_header
{
  using layout_type = layout::tokenized_text_layout<','>;
};

// The address of the device comes from the session and precedes the key.
struct addressed_header
{
  using key_reader = index::key_at<1, 1>;

  static layout::encode_result_t
  write_header (layout::bytes_t &out, dsl::key_value_t const &key,
                field::decode_context_t const *context)
  {
    std::int64_t address = 0;
    if (context == nullptr || !context->find ("address", address)
        || address < 0 || address > 0xFF)
      return lumex::core::expected::error::unexpected<std::error_code> (
          lumex::core::protocol::error::make_error_code (
              lumex::core::protocol::error::protocol_errc::invalid_value));
    out.push_back (static_cast<std::uint8_t> (address));
    out.push_back (static_cast<std::uint8_t> (key.integer));
    return layout::encode_result_t ();
  }
};

// A header that writes a byte, then fails.
struct failing_header
{
  using key_reader = index::key_at<1, 1>;

  static layout::encode_result_t
  write_header (layout::bytes_t &out, dsl::key_value_t const &,
                field::decode_context_t const *)
  {
    out.push_back (0xAA);
    return lumex::core::expected::error::unexpected<std::error_code> (
        lumex::core::protocol::error::make_error_code (
            lumex::core::protocol::error::protocol_errc::not_supported));
  }
};

// A final header with its own write_header: the encoders use it all the
// same (they look for a valid call first).
struct final_header final
{
  using key_reader = index::key_at<1, 1>;

  static layout::encode_result_t
  write_header (layout::bytes_t &out, dsl::key_value_t const &key,
                field::decode_context_t const *)
  {
    out.push_back (0x7E);
    out.push_back (static_cast<std::uint8_t> (key.integer));
    return layout::encode_result_t ();
  }
};

// A strict enumeration: a value that is no enumerator fails at encode.
LUMEX_DEFINE_REFLECTED_ENUM (lamp, std::uint8_t, (on, 1), (off, 2))

// A key of two numbers (a function and a count).
struct pair_key
{
  using lumex_composite_key = dsl::composite_key_tag;
  std::uint8_t function;
  std::uint8_t count;

  LUMEX_CONSTEXPR_CTOR
  pair_key (std::uint8_t f, std::uint8_t c) : function (f), count (c) {}

  static LUMEX_CONSTEXPR std::size_t
  lumex_key_width ()
  {
    return 2;
  }

  LUMEX_CONSTEXPR std::uint64_t
  lumex_key_bits () const
  {
    return static_cast<std::uint64_t> ((function << 8) | count);
  }
};

// A variant of two bytes, then the composite key, least significant byte
// first.
struct pair_header
{
  using key_reader = index::key_at<2, 2, index::key_byte_order::little>;
};

// clang-format off
#define MESSAGE_ROWS(X, X_AS) \
  X (SET, 0x21, "Set a value", (u16_be_, u8_), ("value", "channel"), (u8_), \
     ()) \
  X (PING, 0x05, "Liveness", (), (), (), ()) \
  X (LAMP, 0x07, "Lamp", (enum_<u8_, lamp>), ("lamp"), (enum_<u8_, lamp>), \
     ()) \
  X (NAME, 0x09, "Name", (text_<4>, u8_), ("name", "slot"), (), ())
#define PAIR_ROWS(X, X_AS) \
  X (READ, pair_key (3, 4), "Read", (u16_be_, u8_), ("value", "channel"), \
     (u8_), ()) \
  X (WRITE, pair_key (6, 4), "Write", (), (), (), ())
#define NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_DEFINE (first, codec, first_header, MESSAGE_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (wide, first, codec, wide_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (after, first, codec, after_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (hex, first, codec, hex_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (decimal, first, codec, decimal_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (text, first, codec, text_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (addressed, first, codec, addressed_header,
                              NO_ROWS);
LUMEX_PROTOCOL_DEFINE (pairs, codec, pair_header, PAIR_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (failing, first, codec, failing_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (final_framed, first, codec, final_header,
                              NO_ROWS);

// An address byte, then a key of one byte; the key of a row is -1, an int.
struct negative_header
{
  using key_reader = index::key_at<1, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The wire carries two bytes of key, the reader reads one.
struct narrow_reader_header
{
  using key_reader = index::key_at<0, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 2;
};

// clang-format off
#define NEGATIVE_ROWS(X, X_AS) \
  X (MINUS_ONE, -1, "Minus one", (u8_), ("a"), (u8_), ()) \
  X (PLAIN, 0x7E, "Plain", (u8_), ("a"), (u8_), ())
#define NARROW_ROWS(X, X_AS) \
  X (SMALL, 0x0012, "Fits the reader", (), (), (), ()) \
  X (BIG, 0x1234, "Wider than the reader", (u8_), ("a"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (negative, codec, negative_header, NEGATIVE_ROWS);
LUMEX_PROTOCOL_DEFINE (narrow, codec, narrow_reader_header, NARROW_ROWS);
} // namespace message_tables

namespace
{
namespace tables = message_tables;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;

using set_row = tables::first::SET_t;
using ping_row = tables::first::PING_t;
using lamp_row = tables::first::LAMP_t;
using name_row = tables::first::NAME_t;

layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

layout::bytes_t
text_bytes (char const *text)
{
  return layout::bytes_t (text, text + std::strlen (text));
}

// The request of SET with the value 0x1234 on channel 7.
template <typename P>
layout::bytes_t
set_message ()
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode_message<P, set_row> (out, 0x1234, 7);
  EXPECT_TRUE (result.has_value ());
  return out;
}

// The index of `P` reads the key of the message SET wrote and finds SET.
template <typename P>
::testing::AssertionResult
index_finds_set (layout::bytes_t const &message, std::size_t offset)
{
  index::index_result_t const &built = index::protocol_index<P> ();
  if (!built)
    return ::testing::AssertionFailure () << built.error ().message;
  index::frame_match_t const found = built->match (view_of (message));
  if (found.row == nullptr)
    return ::testing::AssertionFailure () << "no row";
  if (std::string (found.row->entry->identifier) != "SET")
    return ::testing::AssertionFailure () << found.row->entry->identifier;
  if (found.payload_offset != offset)
    return ::testing::AssertionFailure ()
           << "fields at " << found.payload_offset;
  return ::testing::AssertionSuccess ();
}

// Counts what dispatch_message calls.
struct row_seen
{
  std::string row;

  template <typename Row, typename Record>
  void
  request (Record const &)
  {
    row = Row::identifier ();
  }

  template <typename Row, typename Value>
  void
  reply (Value const &)
  {
    row = std::string ("reply ") + Row::identifier ();
  }
};

// A session that knows "address".
struct address_session
{
  std::int64_t address;

  static bool
  lookup (void const *state, char const *key, std::int64_t &value)
  {
    if (std::strcmp (key, "address") != 0)
      return false;
    value = static_cast<address_session const *> (state)->address;
    return true;
  }

  field::decode_context_t
  context () const
  {
    field::decode_context_t const result = { this, &address_session::lookup };
    return result;
  }
};
} // namespace

TEST (LumexProtocolMessageTest,
      GivenTheKeyFirst_WhenTheMessageIsEncoded_ThenTheKeyPrecedesTheFields)
{
  layout::bytes_t const message = set_message<tables::first> ();
  EXPECT_EQ (message, (layout::bytes_t{ 0x21, 0x12, 0x34, 0x07 }));
  EXPECT_TRUE (index_finds_set<tables::first> (message, 1));
}

TEST (LumexProtocolMessageTest,
      GivenAKeyOfTwoBytes_WhenTheMessageIsEncoded_ThenTheKeyTakesBothBytes)
{
  layout::bytes_t const message = set_message<tables::wide> ();
  EXPECT_EQ (message, (layout::bytes_t{ 0x00, 0x21, 0x12, 0x34, 0x07 }));
  EXPECT_TRUE (index_finds_set<tables::wide> (message, 2));
}

TEST (
    LumexProtocolMessageTest,
    GivenTheKeyAfterAHeader_WhenTheMessageIsEncoded_ThenTheHeaderIsZeroAndTheKeyIsWhereTheReaderLooks)
{
  layout::bytes_t const message = set_message<tables::after> ();
  EXPECT_EQ (message,
             (layout::bytes_t{ 0x00, 0x00, 0x00, 0x21, 0x12, 0x34, 0x07 }));
  EXPECT_TRUE (index_finds_set<tables::after> (message, 4));
}

TEST (
    LumexProtocolMessageTest,
    GivenALittleEndianCompositeKey_WhenTheMessageIsEncoded_ThenItsLowByteComesFirst)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_message<tables::pairs, tables::pairs::READ_t> (
                    out, 0x1234, 7)
                    .has_value ()));
  EXPECT_EQ (out,
             (layout::bytes_t{ 0x00, 0x00, 0x04, 0x03, 0x12, 0x34, 0x07 }));
  index::index_result_t const &built = index::protocol_index<tables::pairs> ();
  ASSERT_TRUE (built.has_value ());
  index::frame_match_t const found = built->match (view_of (out));
  ASSERT_NE (found.row, nullptr);
  EXPECT_STREQ (found.row->entry->identifier, "READ");
  EXPECT_EQ (found.payload_offset, 4u);
}

TEST (
    LumexProtocolMessageTest,
    GivenHexadecimalDigits_WhenTheMessageIsEncoded_ThenTheKeyDigitsPrecedeTheFieldDigits)
{
  layout::bytes_t const message = set_message<tables::hex> ();
  EXPECT_EQ (message, text_bytes ("21123407"));
  EXPECT_TRUE (index_finds_set<tables::hex> (message, 2));
}

TEST (
    LumexProtocolMessageTest,
    GivenDecimalDigitsAfterAHeaderCharacter_WhenTheMessageIsEncoded_ThenTheHeaderIsADigitZero)
{
  layout::bytes_t const message = set_message<tables::decimal> ();
  EXPECT_EQ (message, text_bytes ("003304660007"));
  EXPECT_TRUE (index_finds_set<tables::decimal> (message, 4));
}

TEST (
    LumexProtocolMessageTest,
    GivenATokenizedLayer_WhenTheMessageIsEncoded_ThenTheKeyIsTheFirstTokenOnce)
{
  layout::bytes_t const message = set_message<tables::text> ();
  EXPECT_EQ (message, text_bytes ("33,4660,7"));
  EXPECT_TRUE (index_finds_set<tables::text> (message, 0));
}

TEST (
    LumexProtocolMessageTest,
    GivenARowWithoutFields_WhenTheMessageIsEncoded_ThenItIsTheHeaderAndTheKey)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_message<tables::after, ping_row> (out).has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x00, 0x00, 0x00, 0x05 }));
  layout::bytes_t header;
  ASSERT_TRUE (
      (layout::encode_header<tables::after, ping_row> (header).has_value ()));
  EXPECT_EQ (header, out);
}

TEST (
    LumexProtocolMessageTest,
    GivenAHeaderThatWritesItself_WhenTheSessionKnowsTheAddress_ThenTheAddressPrecedesTheKey)
{
  address_session const session = { 0x2A };
  field::decode_context_t const context = session.context ();
  layout::decode_result_t<layout::request_of<set_row>::type> const record
      = layout::make_request<set_row> (0x1234, 7);
  ASSERT_TRUE (record.has_value ());
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_message_record<tables::addressed, set_row> (
                    out, *record, &context)
                    .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x2A, 0x21, 0x12, 0x34, 0x07 }));
  EXPECT_TRUE (index_finds_set<tables::addressed> (out, 2));
  layout::bytes_t reply;
  ASSERT_TRUE ((layout::encode_reply_message<tables::addressed, set_row> (
                    reply, 9, &context)
                    .has_value ()));
  EXPECT_EQ (reply, (layout::bytes_t{ 0x2A, 0x21, 0x09 }));
  // The index reads the key through index::session_context_t as well.
  index::session_context_t known;
  known.set ("address", 0x11);
  field::decode_context_t const from_index = known.decode_context ();
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_message_record<tables::addressed, set_row> (
                    again, *record, &from_index)
                    .has_value ()));
  EXPECT_EQ (again, (layout::bytes_t{ 0x11, 0x21, 0x12, 0x34, 0x07 }));
}

TEST (
    LumexProtocolMessageTest,
    GivenAHeaderThatWritesItself_WhenTheSessionDoesNotKnowTheAddress_ThenItsErrorAndTheBufferIsUnchanged)
{
  layout::bytes_t out{ 0xEE };
  layout::encode_result_t const result
      = layout::encode_message<tables::addressed, set_row> (out, 0x1234, 7);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
}

TEST (
    LumexProtocolMessageTest,
    GivenAFieldThatDoesNotFit_WhenTheMessageIsEncoded_ThenInvalidValueAndTheBufferIsUnchanged)
{
  layout::bytes_t out{ 0xEE };
  layout::encode_result_t const result
      = layout::encode_message<tables::after, set_row> (out, 0x10000, 7);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
}

TEST (
    LumexProtocolMessageTest,
    GivenAHeaderThatFailsAfterWritingAByte_WhenTheMessageIsEncoded_ThenItsErrorAndTheBufferIsUnchanged)
{
  layout::bytes_t out{ 0xEE };
  layout::encode_result_t const header
      = layout::encode_header<tables::failing, ping_row> (out);
  ASSERT_FALSE (header.has_value ());
  EXPECT_TRUE (header.error () == protocol_errc::not_supported);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  layout::encode_result_t const message
      = layout::encode_message<tables::failing, set_row> (out, 0x1234, 7);
  ASSERT_FALSE (message.has_value ());
  EXPECT_TRUE (message.error () == protocol_errc::not_supported);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  layout::encode_result_t const reply
      = layout::encode_reply_message<tables::failing, set_row> (out, 9);
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
}

TEST (
    LumexProtocolMessageTest,
    GivenAFieldThatFailsAfterTheHeaderWasWritten_WhenTheMessageIsEncoded_ThenTheBufferIsUnchanged)
{
  layout::request_of<lamp_row>::type record
      = layout::request_of<lamp_row>::type ();
  field::get<0> (record) = static_cast<tables::lamp> (7);
  layout::bytes_t out{ 0xEE };
  layout::encode_result_t const request
      = layout::encode_message_record<tables::after, lamp_row> (out, record);
  ASSERT_FALSE (request.has_value ());
  EXPECT_TRUE (request.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  layout::encode_result_t const reply
      = layout::encode_reply_message<tables::after, lamp_row> (
          out, static_cast<tables::lamp> (7));
  ASSERT_FALSE (reply.has_value ());
  EXPECT_TRUE (reply.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  ASSERT_TRUE ((layout::encode_reply_message<tables::after, lamp_row> (
                    out, tables::lamp::off)
                    .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE, 0x00, 0x00, 0x00, 0x07, 0x02 }));
}

TEST (
    LumexProtocolMessageTest,
    GivenAMessageWithAppendedBytes_WhenEncoded_ThenTheMessageIsAppendedToTheBuffer)
{
  layout::bytes_t out{ 0xEE };
  ASSERT_TRUE (
      (layout::encode_message<tables::first, ping_row> (out).has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE, 0x05 }));
}

TEST (
    LumexProtocolMessageTest,
    GivenTheMessageOfTheRow_WhenItsRequestAndReplyAreDecoded_ThenTheValuesComeBack)
{
  layout::bytes_t const message = set_message<tables::after> ();
  layout::decode_result_t<layout::request_of<set_row>::type> const record
      = layout::decode_message<tables::after, set_row> (view_of (message));
  ASSERT_TRUE (record.has_value ());
  EXPECT_EQ (field::get<0> (*record), 0x1234u);
  EXPECT_EQ (field::get<1> (*record), 7u);
  layout::bytes_t reply;
  ASSERT_TRUE ((layout::encode_reply_message<tables::after, set_row> (reply, 9)
                    .has_value ()));
  EXPECT_EQ (reply, (layout::bytes_t{ 0x00, 0x00, 0x00, 0x21, 0x09 }));
  layout::decode_result_t<std::uint8_t> const value
      = layout::decode_reply_message<tables::after, set_row> (view_of (reply));
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (*value, 9u);
}

TEST (
    LumexProtocolMessageTest,
    GivenTheReplyOfAnotherRow_WhenDecodedAsTheReplyOfARow_ThenUnexpectedReply)
{
  layout::bytes_t const other{ 0x00, 0x00, 0x00, 0x05 };
  layout::decode_result_t<std::uint8_t> const value
      = layout::decode_reply_message<tables::after, set_row> (view_of (other));
  ASSERT_FALSE (value.has_value ());
  EXPECT_TRUE (value.error () == protocol_errc::unexpected_reply);
  // A key no row has is unexpected too.
  layout::bytes_t const stranger{ 0x00, 0x00, 0x00, 0x77, 0x09 };
  layout::decode_result_t<std::uint8_t> const none
      = layout::decode_reply_message<tables::after, set_row> (
          view_of (stranger));
  ASSERT_FALSE (none.has_value ());
  EXPECT_TRUE (none.error () == protocol_errc::unexpected_reply);
  // The same in a tokenized layer, where the key is text.
  layout::bytes_t const ping = text_bytes ("5");
  layout::decode_result_t<std::uint8_t> const text_value
      = layout::decode_reply_message<tables::text, set_row> (view_of (ping));
  ASSERT_FALSE (text_value.has_value ());
  EXPECT_TRUE (text_value.error () == protocol_errc::unexpected_reply);
}

TEST (
    LumexProtocolMessageTest,
    GivenTheRequestOfAnotherRow_WhenDecodedAsTheRequestOfARow_ThenUnknownFrame)
{
  layout::bytes_t const other{ 0x05 };
  layout::decode_result_t<layout::request_of<set_row>::type> const record
      = layout::decode_message<tables::first, set_row> (view_of (other));
  ASSERT_FALSE (record.has_value ());
  EXPECT_TRUE (record.error () == protocol_errc::unknown_frame);
}

TEST (LumexProtocolMessageTest,
      GivenAMessageShorterThanItsKey_WhenDecoded_ThenUnknownFrame)
{
  layout::bytes_t const short_message{ 0x00, 0x00 };
  layout::decode_result_t<std::uint8_t> const value
      = layout::decode_reply_message<tables::after, set_row> (
          view_of (short_message));
  ASSERT_FALSE (value.has_value ());
  EXPECT_TRUE (value.error () == protocol_errc::unknown_frame);
  layout::decode_result_t<layout::request_of<set_row>::type> const record
      = layout::decode_message<tables::after, set_row> (
          layout::bytes_view_t ());
  ASSERT_FALSE (record.has_value ());
  EXPECT_TRUE (record.error () == protocol_errc::unknown_frame);
}

TEST (
    LumexProtocolMessageTest,
    GivenTheKeyOfTheRow_WhenTheFieldsAreShortOrLong_ThenTruncatedOrTrailingBytes)
{
  layout::bytes_t const shorter{ 0x21, 0x12, 0x34 };
  layout::decode_result_t<layout::request_of<set_row>::type> const cut
      = layout::decode_message<tables::first, set_row> (view_of (shorter));
  ASSERT_FALSE (cut.has_value ());
  EXPECT_TRUE (cut.error () == protocol_errc::truncated);
  layout::bytes_t const longer{ 0x21, 0x12, 0x34, 0x07, 0x00 };
  layout::decode_result_t<layout::request_of<set_row>::type> const extra
      = layout::decode_message<tables::first, set_row> (view_of (longer));
  ASSERT_FALSE (extra.has_value ());
  EXPECT_TRUE (extra.error () == protocol_errc::trailing_bytes);
}

TEST (
    LumexProtocolMessageTest,
    GivenATokenizedMessage_WhenDecodedBack_ThenTheKeyIsCheckedOnceAndTheFieldsComeBack)
{
  layout::bytes_t const message = set_message<tables::text> ();
  layout::decode_result_t<layout::request_of<set_row>::type> const record
      = layout::decode_message<tables::text, set_row> (view_of (message));
  ASSERT_TRUE (record.has_value ());
  EXPECT_EQ (field::get<0> (*record), 0x1234u);
  layout::bytes_t reply;
  ASSERT_TRUE ((layout::encode_reply_message<tables::text, set_row> (reply, 9)
                    .has_value ()));
  EXPECT_EQ (reply, text_bytes ("33,9"));
  layout::decode_result_t<std::uint8_t> const value
      = layout::decode_reply_message<tables::text, set_row> (view_of (reply));
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (*value, 9u);
}

TEST (
    LumexProtocolMessageTest,
    GivenARowWithoutReply_WhenItsReplyMessageIsEncoded_ThenItIsTheKeyAndAnEmptyRecordDecodes)
{
  layout::bytes_t reply;
  ASSERT_TRUE ((layout::encode_reply_message<tables::first, ping_row> (
                    reply, field::record<> ())
                    .has_value ()));
  EXPECT_EQ (reply, (layout::bytes_t{ 0x05 }));
  EXPECT_TRUE (
      (layout::decode_reply_message<tables::first, ping_row> (view_of (reply))
           .has_value ()));
  layout::bytes_t const longer{ 0x05, 0x01 };
  layout::decode_result_t<field::record<>> const extra
      = layout::decode_reply_message<tables::first, ping_row> (
          view_of (longer));
  ASSERT_FALSE (extra.has_value ());
  EXPECT_TRUE (extra.error () == protocol_errc::trailing_bytes);
}

TEST (
    LumexProtocolMessageTest,
    GivenTheKeyReaders_WhenTheirWriterTwinsWriteAKey_ThenTheReaderReadsItBack)
{
  dsl::key_value_t const key
      = { dsl::key_kind::integer, 2, 0xA1B2, nullptr, 0 };
  layout::bytes_t big;
  ASSERT_TRUE ((index::key_at<1, 2>::write (big, key)));
  EXPECT_EQ (big, (layout::bytes_t{ 0x00, 0xA1, 0xB2 }));
  index::key_read_t read = index::key_read_t ();
  ASSERT_TRUE ((index::key_at<1, 2>::read (view_of (big), read)));
  EXPECT_EQ (read.key.integer, 0xA1B2u);
  layout::bytes_t little;
  ASSERT_TRUE ((index::key_at<0, 2, index::key_byte_order::little>::write (
      little, key)));
  EXPECT_EQ (little, (layout::bytes_t{ 0xB2, 0xA1 }));
  layout::bytes_t digits;
  ASSERT_TRUE ((index::key_at_digits<2, 4, 16>::write (digits, key)));
  EXPECT_EQ (digits, text_bytes ("00A1B2"));
  ASSERT_TRUE (
      (index::key_at_digits<2, 4, 16>::read (view_of (digits), read)));
  EXPECT_EQ (read.key.integer, 0xA1B2u);
  layout::bytes_t decimal;
  dsl::key_value_t const small = { dsl::key_kind::integer, 1, 42, nullptr, 0 };
  ASSERT_TRUE ((index::key_at_digits<0, 3, 10>::write (decimal, small)));
  EXPECT_EQ (decimal, text_bytes ("042"));
  // The tokenized layout writes the key itself.
  layout::bytes_t token;
  ASSERT_TRUE ((index::tokenized_key<','>::write (token, small)));
  EXPECT_TRUE (token.empty ());
}

TEST (LumexProtocolMessageTest,
      GivenAKeyTheWriterCannotCarry_WhenWritten_ThenFalseAndNothingIsWritten)
{
  dsl::key_value_t const wide
      = { dsl::key_kind::integer, 2, 0x1FF, nullptr, 0 };
  layout::bytes_t out{ 0xEE };
  EXPECT_FALSE ((index::key_at<0, 1>::write (out, wide)));
  EXPECT_FALSE ((index::key_at_digits<0, 2, 16>::write (out, wide)));
  EXPECT_FALSE ((index::key_at_digits<0, 2, 10>::write (
      out, dsl::key_value_t{ dsl::key_kind::integer, 1, 100, nullptr, 0 })));
  dsl::key_value_t const text = { dsl::key_kind::text, 0, 0, "AZ", 2 };
  EXPECT_FALSE ((index::key_at<0, 2>::write (out, text)));
  EXPECT_FALSE ((index::key_at_digits<0, 2, 16>::write (out, text)));
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
}

TEST (
    LumexProtocolMessageTest,
    GivenASessionBeforeTheArguments_WhenTheMessageIsEncoded_ThenTheHeaderReadsItAndTheArgumentsAreTheFields)
{
  address_session const session = { 0x2A };
  field::decode_context_t const context = session.context ();
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_message<tables::addressed, set_row> (
                    out, context, 0x1234, 7)
                    .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x2A, 0x21, 0x12, 0x34, 0x07 }));
  // A row without fields takes the session alone.
  layout::bytes_t ping;
  ASSERT_TRUE (
      (layout::encode_message<tables::addressed, ping_row> (ping, context)
           .has_value ()));
  EXPECT_EQ (ping, (layout::bytes_t{ 0x2A, 0x05 }));
  // The session of the index, through its decode context.
  index::session_context_t known;
  known.set ("address", 0x11);
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_message<tables::addressed, set_row> (
                    again, known.decode_context (), 0x1234, 7)
                    .has_value ()));
  EXPECT_EQ (again, (layout::bytes_t{ 0x11, 0x21, 0x12, 0x34, 0x07 }));
  // A field that does not fit is refused with the session too.
  layout::bytes_t refused{ 0xEE };
  layout::encode_result_t const result
      = layout::encode_message<tables::addressed, set_row> (refused, context,
                                                            0x10000, 7);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_EQ (refused, (layout::bytes_t{ 0xEE }));
}

TEST (
    LumexProtocolMessageTest,
    GivenARowWhoseFirstFieldIsAText_WhenEncodedWithAndWithoutASession_ThenTheFirstArgumentChoosesTheOverload)
{
  layout::bytes_t const expected{ 0x00, 0x00, 0x00, 0x09, 'A',
                                  'B',  0x00, 0x00, 0x03 };
  layout::bytes_t plain;
  ASSERT_TRUE (
      (layout::encode_message<tables::after, name_row> (plain, "AB", 3)
           .has_value ()));
  EXPECT_EQ (plain, expected);
  layout::bytes_t view;
  ASSERT_TRUE ((layout::encode_message<tables::after, name_row> (
                    view, field::text_view ("AB"), 3)
                    .has_value ()));
  EXPECT_EQ (view, expected);
  address_session const session = { 0x2A };
  field::decode_context_t const context = session.context ();
  layout::bytes_t with_session;
  ASSERT_TRUE ((layout::encode_message<tables::after, name_row> (
                    with_session, context, "AB", 3)
                    .has_value ()));
  EXPECT_EQ (with_session, expected);
  // The session reaches the header of a layer that reads it.
  layout::bytes_t addressed;
  ASSERT_TRUE ((layout::encode_message<tables::addressed, name_row> (
                    addressed, context, "AB", 3)
                    .has_value ()));
  EXPECT_EQ (addressed,
             (layout::bytes_t{ 0x2A, 0x09, 'A', 'B', 0x00, 0x00, 0x03 }));
}

TEST (
    LumexProtocolMessageTest,
    GivenTheKeyMinusOneOnAOneByteWire_WhenEncodedAndDispatched_ThenItIsTheByteFFAndTheSameRow)
{
  layout::bytes_t out;
  ASSERT_TRUE ((
      layout::encode_message<tables::negative, tables::negative::MINUS_ONE_t> (
          out, 5)
          .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x00, 0xFF, 0x05 }));
  row_seen seen;
  EXPECT_FALSE ((layout::dispatch_message<tables::negative> (
      view_of (out), index::frame_direction::tx, seen)));
  EXPECT_EQ (seen.row, "MINUS_ONE");
  EXPECT_TRUE ((
      layout::decode_message<tables::negative, tables::negative::MINUS_ONE_t> (
          view_of (out))
          .has_value ()));
}

TEST (
    LumexProtocolMessageTest,
    GivenAKeyWiderThanTheReader_WhenTheMessageIsEncoded_ThenInvalidValueAndTheBufferIsUnchanged)
{
  layout::bytes_t out{ 0xEE };
  layout::encode_result_t const message
      = layout::encode_message<tables::narrow, tables::narrow::BIG_t> (out, 1);
  ASSERT_FALSE (message.has_value ());
  EXPECT_TRUE (message.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  layout::encode_result_t const header
      = layout::encode_header<tables::narrow, tables::narrow::BIG_t> (out);
  ASSERT_FALSE (header.has_value ());
  EXPECT_TRUE (header.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE }));
  // A key that fits the reader is written.
  ASSERT_TRUE (
      (layout::encode_message<tables::narrow, tables::narrow::SMALL_t> (out)
           .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE, 0x12 }));
}

TEST (
    LumexProtocolMessageTest,
    GivenMessagesOfEveryLayout_WhenTheirFieldsAreAsked_ThenTheyAreTheBytesAfterTheHeaderAndTheKey)
{
  layout::bytes_t const after = set_message<tables::after> ();
  layout::decode_result_t<layout::bytes_view_t> const binary_fields
      = layout::message_fields<tables::after> (view_of (after));
  ASSERT_TRUE (binary_fields.has_value ());
  EXPECT_EQ (layout::bytes_t (binary_fields->begin (), binary_fields->end ()),
             (layout::bytes_t{ 0x12, 0x34, 0x07 }));
  layout::bytes_t const hex = set_message<tables::hex> ();
  layout::decode_result_t<layout::bytes_view_t> const digits
      = layout::message_fields<tables::hex> (view_of (hex));
  ASSERT_TRUE (digits.has_value ());
  EXPECT_EQ (layout::bytes_t (digits->begin (), digits->end ()),
             text_bytes ("123407"));
  layout::bytes_t const tokens = set_message<tables::text> ();
  layout::decode_result_t<layout::bytes_view_t> const after_key
      = layout::message_fields<tables::text> (view_of (tokens));
  ASSERT_TRUE (after_key.has_value ());
  EXPECT_EQ (layout::bytes_t (after_key->begin (), after_key->end ()),
             text_bytes ("4660,7"));
  // A message of the key alone has no fields.
  layout::bytes_t const ping = text_bytes ("5");
  layout::decode_result_t<layout::bytes_view_t> const none
      = layout::message_fields<tables::text> (view_of (ping));
  ASSERT_TRUE (none.has_value ());
  EXPECT_EQ (none->size (), 0u);
  // A message without a readable key has no fields either.
  layout::bytes_t const short_message{ 0x00, 0x00 };
  layout::decode_result_t<layout::bytes_view_t> const unreadable
      = layout::message_fields<tables::after> (view_of (short_message));
  ASSERT_FALSE (unreadable.has_value ());
  EXPECT_TRUE (unreadable.error () == protocol_errc::unknown_frame);
}

TEST (
    LumexProtocolMessageTest,
    GivenRowsWithAndWithoutAReply_WhenTheirSampleReplyMessageIsEncoded_ThenItIsTheHeaderTheKeyAndTheSampleReply)
{
  layout::bytes_t set;
  ASSERT_TRUE ((layout::sample_reply_message<tables::after, set_row> (set)
                    .has_value ()));
  EXPECT_EQ (set, (layout::bytes_t{ 0x00, 0x00, 0x00, 0x21, 0x00 }));
  layout::bytes_t ping;
  ASSERT_TRUE ((layout::sample_reply_message<tables::after, ping_row> (ping)
                    .has_value ()));
  EXPECT_EQ (ping, (layout::bytes_t{ 0x00, 0x00, 0x00, 0x05 }));
  // The first enumerator of a reflected enumeration without 0.
  layout::bytes_t lamp;
  ASSERT_TRUE ((layout::sample_reply_message<tables::first, lamp_row> (lamp)
                    .has_value ()));
  EXPECT_EQ (lamp, (layout::bytes_t{ 0x07, 0x01 }));
  // The session reaches the header.
  address_session const session = { 0x2A };
  field::decode_context_t const context = session.context ();
  layout::bytes_t addressed;
  ASSERT_TRUE ((layout::sample_reply_message<tables::addressed, set_row> (
                    addressed, &context)
                    .has_value ()));
  EXPECT_EQ (addressed, (layout::bytes_t{ 0x2A, 0x21, 0x00 }));
  static_assert (layout::has_core_reply<set_row>::value,
                 "a field reply is the core's");
  static_assert (layout::has_core_reply<ping_row>::value,
                 "no reply is the core's too");
}

TEST (LumexProtocolMessageTest,
      GivenAFinalHeaderWithItsWriteHeader_WhenEncoded_ThenItWritesTheHeader)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_message<tables::final_framed, tables::first::PING_t> (
          out)));
  EXPECT_EQ (out, (layout::bytes_t{ 0x7E, 0x05 }));
}

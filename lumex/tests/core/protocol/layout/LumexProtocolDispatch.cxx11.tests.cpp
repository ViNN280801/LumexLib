// Dispatch of a received message to its typed row
// (lumex/core/protocol/layout/LumexProtocolDispatch.hpp, dispatch_message):
// the key of the message chooses the row, the visitor gets the request record
// or the reply value of that row; an unknown key, a message without a readable
// key and a decode error reach no visitor; a row without a reply gets an empty
// record, a reply kind of another layer the message itself; the session values
// reach the decoders.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"

namespace dispatch_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;

struct codec
{
};

// A reply kind of another layer (the dialogue decodes it).
template <typename Ack, typename Nak> struct ack_nak
{
  using lumex_protocol_reply = dsl::reply_tag;
};

struct vocabulary : dsl::vocabulary
{
  template <typename Ack, typename Nak>
  using ack_nak = dispatch_tables::ack_nak<Ack, Nak>;
};

LUMEX_PROTOCOL_CONTEXT_KEY (panel_kind);

// An address byte, then the key.
struct binary_header
{
  using lumex_vocabulary = vocabulary;
  using key_reader = index::key_at<1, 1>;
};

struct text_header
{
  using lumex_vocabulary = vocabulary;
  using layout_type = layout::tokenized_text_layout<','>;
};

// FIRST and SECOND have the same fields: only the key tells them apart.
// clang-format off
#define DISPATCH_ROWS(X, X_AS) \
  X (FIRST, 0x10, "First", (u8_), ("a"), (u8_), ()) \
  X (SECOND, 0x20, "Second", (u8_), ("b"), (u16_be_), ()) \
  X (QUIET, 0x30, "Without a reply", (u16_le_), ("c"), (), ()) \
  X (ACKED, 0x40, "A reply of the dialogue", (), (), (ack_nak<u8_, u8_>), \
     ()) \
  X (BY_SESSION, 0x50, "A reply chosen by the session", (), (), \
     (union_by_context_<panel_kind, case_<1, u8_>, case_<2, u16_be_> >), ()) \
  X (PICK, 0x60, "A request chosen by the session", \
     (union_by_context_<panel_kind, case_<1, u8_>, case_<2, u16_be_> >), \
     ("choice"), (), ())
#define PREFIX_ROWS(X, X_AS) \
  X_AS (SHORT, "NM", "NM", "The shorter key", (u8_), ("a"), (u8_), ()) \
  X_AS (LONG, "NMX", "NMX", "The longer key", (u8_), ("a"), (u8_), ())
#define NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_DEFINE (binary, codec, binary_header, DISPATCH_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (text, binary, codec, text_header, NO_ROWS);
LUMEX_PROTOCOL_DEFINE (prefixes, codec, text_header, PREFIX_ROWS);
} // namespace dispatch_tables

namespace
{
namespace tables = dispatch_tables;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;

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

// What the visitor saw: the row, the kind of call, and the message encoded
// again from the value it got.
template <typename P> struct recorder
{
  std::string row;
  std::string call;
  layout::bytes_t again;
  std::size_t calls = 0;

  template <typename Row>
  void
  request (typename layout::request_of<Row>::type const &record)
  {
    ++calls;
    row = Row::identifier ();
    call = "request";
    EXPECT_TRUE (
        (layout::encode_message_record<P, Row> (again, record).has_value ()));
  }

  template <typename Row, typename Value>
  void
  reply (Value const &value)
  {
    ++calls;
    row = Row::identifier ();
    call = "reply";
    encode_again<Row> (value, std::is_same<Value, layout::bytes_view_t> ());
  }

private:
  template <typename Row, typename Value>
  void
  encode_again (Value const &value, std::false_type)
  {
    EXPECT_TRUE (
        (layout::encode_reply_message<P, Row> (again, value).has_value ()));
  }

  template <typename Row>
  void
  encode_again (layout::bytes_view_t const &message, std::true_type)
  {
    call = "reply message";
    again.assign (message.begin (), message.end ());
  }
};

template <typename P>
std::error_code
dispatched (layout::bytes_t const &message, index::frame_direction direction,
            recorder<P> &seen,
            field::decode_context_t const *context = nullptr)
{
  return layout::dispatch_message<P> (view_of (message), direction, seen,
                                      context);
}

// A visitor passed as a temporary: it counts the calls.
struct counter
{
  int *calls;

  template <typename Row, typename Record>
  void
  request (Record const &)
  {
    ++*calls;
  }

  template <typename Row, typename Value>
  void
  reply (Value const &)
  {
    ++*calls;
  }
};

// A session that knows panel_kind.
struct panel_session
{
  std::int64_t kind;

  static bool
  lookup (void const *state, char const *key, std::int64_t &value)
  {
    if (std::strcmp (key, "panel_kind") != 0)
      return false;
    value = static_cast<panel_session const *> (state)->kind;
    return true;
  }

  field::decode_context_t
  context () const
  {
    field::decode_context_t const result = { this, &panel_session::lookup };
    return result;
  }
};
} // namespace

TEST (
    LumexProtocolDispatchTest,
    GivenTheRequestOfARow_WhenDispatched_ThenTheVisitorGetsThatRowAndItsRecord)
{
  layout::bytes_t const second{ 0x00, 0x20, 0x07 };
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (second, index::frame_direction::tx, seen).operator bool ());
  EXPECT_EQ (seen.calls, 1u);
  EXPECT_EQ (seen.row, "SECOND");
  EXPECT_EQ (seen.call, "request");
  EXPECT_EQ (seen.again, second);
  layout::bytes_t const first{ 0x00, 0x10, 0x07 };
  recorder<tables::binary> other;
  EXPECT_FALSE (
      dispatched (first, index::frame_direction::tx, other).operator bool ());
  EXPECT_EQ (other.row, "FIRST");
  EXPECT_EQ (other.again, first);
}

TEST (
    LumexProtocolDispatchTest,
    GivenTheReplyOfARow_WhenDispatched_ThenTheVisitorGetsTheReplyValueOfThatRow)
{
  layout::bytes_t const second{ 0x00, 0x20, 0x12, 0x34 };
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (second, index::frame_direction::rx, seen).operator bool ());
  EXPECT_EQ (seen.calls, 1u);
  EXPECT_EQ (seen.row, "SECOND");
  EXPECT_EQ (seen.call, "reply");
  EXPECT_EQ (seen.again, second);
}

TEST (
    LumexProtocolDispatchTest,
    GivenATokenizedLayer_WhenARequestAndAReplyAreDispatched_ThenTheTextKeyChoosesTheRow)
{
  layout::bytes_t const request = text_bytes ("32,7");
  recorder<tables::text> seen;
  EXPECT_FALSE (
      dispatched (request, index::frame_direction::tx, seen).operator bool ());
  EXPECT_EQ (seen.row, "SECOND");
  EXPECT_EQ (seen.again, request);
  layout::bytes_t const reply = text_bytes ("16,9");
  recorder<tables::text> answer;
  EXPECT_FALSE (
      dispatched (reply, index::frame_direction::rx, answer).operator bool ());
  EXPECT_EQ (answer.row, "FIRST");
  EXPECT_EQ (answer.again, reply);
}

TEST (LumexProtocolDispatchTest,
      GivenAKeyNoRowHas_WhenDispatched_ThenUnknownFrameAndNoCall)
{
  recorder<tables::binary> seen;
  std::error_code const error = dispatched (
      layout::bytes_t{ 0x00, 0x77, 0x01 }, index::frame_direction::tx, seen);
  EXPECT_TRUE (error == protocol_errc::unknown_frame);
  EXPECT_EQ (seen.calls, 0u);
  recorder<tables::text> text_seen;
  EXPECT_TRUE (
      dispatched (text_bytes ("99,1"), index::frame_direction::rx, text_seen)
      == protocol_errc::unknown_frame);
  EXPECT_EQ (text_seen.calls, 0u);
}

TEST (LumexProtocolDispatchTest,
      GivenAMessageShorterThanItsKey_WhenDispatched_ThenUnknownFrameAndNoCall)
{
  recorder<tables::binary> seen;
  EXPECT_TRUE (
      dispatched (layout::bytes_t{ 0x00 }, index::frame_direction::tx, seen)
      == protocol_errc::unknown_frame);
  EXPECT_TRUE (
      dispatched (layout::bytes_t (), index::frame_direction::rx, seen)
      == protocol_errc::unknown_frame);
  EXPECT_EQ (seen.calls, 0u);
}

TEST (LumexProtocolDispatchTest,
      GivenTheKeyOfARowAndBadFields_WhenDispatched_ThenTheDecodeErrorAndNoCall)
{
  recorder<tables::binary> seen;
  EXPECT_TRUE (dispatched (layout::bytes_t{ 0x00, 0x20 },
                           index::frame_direction::tx, seen)
               == protocol_errc::truncated);
  EXPECT_TRUE (dispatched (layout::bytes_t{ 0x00, 0x20, 0x01, 0x02, 0x03 },
                           index::frame_direction::rx, seen)
               == protocol_errc::trailing_bytes);
  EXPECT_EQ (seen.calls, 0u);
}

TEST (
    LumexProtocolDispatchTest,
    GivenARowWithoutReply_WhenItsReplyIsDispatched_ThenTheVisitorGetsAnEmptyRecord)
{
  layout::bytes_t const ack{ 0x00, 0x30 };
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (ack, index::frame_direction::rx, seen).operator bool ());
  EXPECT_EQ (seen.row, "QUIET");
  EXPECT_EQ (seen.call, "reply");
  EXPECT_EQ (seen.again, ack);
  recorder<tables::binary> longer;
  EXPECT_TRUE (dispatched (layout::bytes_t{ 0x00, 0x30, 0x01 },
                           index::frame_direction::rx, longer)
               == protocol_errc::trailing_bytes);
  EXPECT_EQ (longer.calls, 0u);
}

TEST (
    LumexProtocolDispatchTest,
    GivenAReplyKindOfAnotherLayer_WhenItsReplyIsDispatched_ThenTheVisitorGetsTheMessage)
{
  layout::bytes_t const ack{ 0x00, 0x40, 0x06 };
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (ack, index::frame_direction::rx, seen).operator bool ());
  EXPECT_EQ (seen.row, "ACKED");
  EXPECT_EQ (seen.call, "reply message");
  EXPECT_EQ (seen.again, ack);
}

TEST (
    LumexProtocolDispatchTest,
    GivenAReplyChosenByTheSession_WhenDispatchedWithAndWithoutTheSession_ThenOnlyTheSessionDecodesIt)
{
  layout::bytes_t const reply{ 0x00, 0x50, 0x12, 0x34 };
  panel_session const session = { 2 };
  field::decode_context_t const context = session.context ();
  recorder<tables::binary> seen;
  EXPECT_FALSE (dispatched (reply, index::frame_direction::rx, seen, &context)
                    .operator bool ());
  EXPECT_EQ (seen.row, "BY_SESSION");
  recorder<tables::binary> blind;
  EXPECT_TRUE (dispatched (reply, index::frame_direction::rx, blind)
               == protocol_errc::unknown_case);
  EXPECT_EQ (blind.calls, 0u);
}

TEST (LumexProtocolDispatchTest,
      GivenATemporaryVisitor_WhenDispatched_ThenItIsCalled)
{
  int calls = 0;
  layout::bytes_t const first{ 0x00, 0x10, 0x07 };
  EXPECT_FALSE (
      (layout::dispatch_message<tables::binary> (
           view_of (first), index::frame_direction::tx, counter{ &calls })
           .operator bool ()));
  EXPECT_EQ (calls, 1);
}

TEST (
    LumexProtocolDispatchTest,
    GivenTheRowsOfBothLayers_WhenTheirReplyIsTheCoresOrTheDialogues_ThenHasCoreReplySaysWhich)
{
  static_assert (layout::has_core_reply<tables::binary::FIRST_t>::value,
                 "a field reply");
  static_assert (layout::has_core_reply<tables::binary::QUIET_t>::value,
                 "no reply");
  static_assert (layout::has_core_reply<tables::binary::BY_SESSION_t>::value,
                 "a session union is a field");
  static_assert (!layout::has_core_reply<tables::binary::ACKED_t>::value,
                 "a reply kind of the dialogue");
  EXPECT_TRUE (layout::has_core_reply<tables::binary::FIRST_t>::value);
  EXPECT_FALSE (layout::has_core_reply<tables::binary::ACKED_t>::value);
}

TEST (
    LumexProtocolDispatchTest,
    GivenTheReplyMessageOfADialogueRow_WhenDispatched_ThenItsFieldsAreTheBytesAfterTheKey)
{
  layout::bytes_t const ack{ 0x00, 0x40, 0x06, 0x07 };
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (ack, index::frame_direction::rx, seen).operator bool ());
  layout::decode_result_t<layout::bytes_view_t> const fields
      = layout::message_fields<tables::binary> (view_of (seen.again));
  ASSERT_TRUE (fields.has_value ());
  EXPECT_EQ (layout::bytes_t (fields->begin (), fields->end ()),
             (layout::bytes_t{ 0x06, 0x07 }));
}

TEST (
    LumexProtocolDispatchTest,
    GivenEveryRowOfTheTables_WhenTheirMessagesAreChecked_ThenTheDialogueRowIsDispatchedToo)
{
  panel_session const session = { 1 };
  field::decode_context_t const context = session.context ();
  EXPECT_EQ (lumex_tests::devices::expect_every_row_message<tables::binary> (
                 &context),
             6u);
  EXPECT_EQ (
      lumex_tests::devices::expect_every_row_message<tables::text> (&context),
      6u);
}

TEST (
    LumexProtocolDispatchTest,
    GivenTextKeysThatArePrefixesOfEachOther_WhenDispatched_ThenOnlyTheWholeKeyMatches)
{
  struct expected_t
  {
    char const *message;
    char const *row;
  };
  expected_t const cases[] = { { "NM,1", "NM_SHORT" },
                               { "NMX,1", "NM_LONG" },
                               { "NMXY,1", "" },
                               { "N,1", "" } };
  for (expected_t const &item : cases)
    {
      layout::bytes_t const message = text_bytes (item.message);
      recorder<tables::prefixes> seen;
      std::error_code const error
          = dispatched (message, index::frame_direction::tx, seen);
      std::string const expected (item.row);
      if (expected.empty ())
        {
          EXPECT_TRUE (error == protocol_errc::unknown_frame) << item.message;
          EXPECT_EQ (seen.calls, 0u) << item.message;
        }
      else
        {
          EXPECT_FALSE (error) << item.message;
          EXPECT_EQ (std::string ("NM_") + seen.row, expected) << item.message;
          EXPECT_EQ (seen.again, message) << item.message;
        }
    }
}

TEST (
    LumexProtocolDispatchTest,
    GivenARequestChosenByTheSession_WhenEncodedDecodedAndDispatched_ThenTheSessionDecidesEveryStep)
{
  using pick = tables::binary::PICK_t;
  layout::request_of<pick>::type record = layout::request_of<pick>::type ();
  field::get<0> (record).discriminant = 2;
  field::get<0> (record).cases.set<1> (0x1234);
  panel_session const two = { 2 };
  field::decode_context_t const two_context = two.context ();
  panel_session const one = { 1 };
  field::decode_context_t const one_context = one.context ();
  // The encoder refuses a case the session does not hold.
  layout::bytes_t refused;
  layout::encode_result_t const mismatch
      = layout::encode_message_record<tables::binary, pick> (refused, record,
                                                             &one_context);
  ASSERT_FALSE (mismatch.has_value ());
  EXPECT_TRUE (mismatch.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (refused.empty ());
  layout::bytes_t message;
  ASSERT_TRUE ((layout::encode_message_record<tables::binary, pick> (
                    message, record, &two_context)
                    .has_value ()));
  EXPECT_EQ (message, (layout::bytes_t{ 0x00, 0x60, 0x12, 0x34 }));
  // Only the session tells the decoder which case it is.
  layout::decode_result_t<layout::request_of<pick>::type> const decoded
      = layout::decode_message<tables::binary, pick> (view_of (message),
                                                      &two_context);
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<0> (*decoded).cases.index (), 1u);
  EXPECT_TRUE ((
      layout::decode_message<tables::binary, pick> (view_of (message)).error ()
      == protocol_errc::unknown_case));
  recorder<tables::binary> seen;
  EXPECT_FALSE (
      dispatched (message, index::frame_direction::tx, seen, &two_context)
          .operator bool ());
  EXPECT_EQ (seen.row, "PICK");
  recorder<tables::binary> blind;
  EXPECT_TRUE (dispatched (message, index::frame_direction::tx, blind)
               == protocol_errc::unknown_case);
  EXPECT_EQ (blind.calls, 0u);
}

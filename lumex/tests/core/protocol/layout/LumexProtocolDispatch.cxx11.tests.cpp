// Dispatch of a received message to its typed row
// (lumex/core/protocol/layout/LumexProtocolDispatch.hpp): the key of the
// message chooses the row, the visitor gets the request record or the reply
// value of that row; an unknown key, a message without a readable key and a
// decode error reach no visitor; a row without a reply gets an empty record,
// a reply kind of another layer the message itself; the session values reach
// the decoders.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

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
     (union_by_context_<panel_kind, case_<1, u8_>, case_<2, u16_be_> >), ())
#define NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_DEFINE (binary, codec, binary_header, DISPATCH_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (text, binary, codec, text_header, NO_ROWS);
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
  return layout::dispatch<P> (view_of (message), direction, seen, context);
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
  EXPECT_FALSE ((layout::dispatch<tables::binary> (view_of (first),
                                                   index::frame_direction::tx,
                                                   counter{ &calls })
                     .operator bool ()));
  EXPECT_EQ (calls, 1);
}

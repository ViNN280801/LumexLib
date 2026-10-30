// decode_reply_kind and decode_reply_fields of the dialogue vocabulary
// (lumex/applied/transport/protocol/dsl): the part a reply message of a reply
// kind is and its value, by the fields of the row itself and by the row of a
// part, and the agreement of that classification with the log line of
// describe on every reply kind of the synthetic pump table.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/dsl/LumexDialogueTestTables.hpp"

namespace
{
namespace tables = lumex_tests::dialogue_tables;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::view_of;
using pump = tables::pump_t;

std::string
describe_rx (index::protocol_index_t const &where,
             layout::bytes_t const &frame)
{
  return where.describe (view_of (frame), index::frame_direction::rx);
}

// The describe line and the classification of `frame` as a reply of Row in
// layer P agree: the line names the part the decoder found, or carries the
// error the decoder returned.
template <typename P, typename Row>
void
expect_agreement (std::initializer_list<int> values)
{
  index::index_result_t const &built = index::protocol_index<P> ();
  ASSERT_TRUE (built.has_value ());
  layout::bytes_t const frame = bytes (values);
  std::string const line = describe_rx (*built, frame);
  std::string const head = std::string ("RX ") + P::protocol_name () + "."
                           + Row::specification_name () + " ("
                           + Row::description () + ")";
  ASSERT_EQ (line.find (head), 0u) << line;
  std::string const rest = line.substr (head.size ());
  auto const decoded = dialogue::decode_reply_kind<P, Row> (view_of (frame));
  if (decoded.has_value ())
    {
      std::string const word
          = std::string (" ") + dialogue::reply_part_name (decoded->part);
      EXPECT_EQ (rest.compare (0, word.size (), word), 0)
          << line << " (decoded as " << word << ")";
      EXPECT_EQ (rest.find ("[decode error"), std::string::npos) << line;
    }
  else
    EXPECT_NE (
        rest.find ("[decode error: " + decoded.error ().message () + "]"),
        std::string::npos)
        << line;
}
} // namespace

TEST (
    LumexDialogueReplyKindTest,
    GivenEveryReplyKindOfThePump_WhenAReplyIsDescribedAndDecoded_ThenBothNameTheSamePart)
{
  // ack_nak of two field parts: ACK, NAK, neither.
  expect_agreement<pump, pump::START_t> ({ 0x01, 0x07 });
  expect_agreement<pump, pump::START_t> ({ 0x01, 0x05, 0x34, 0x12 });
  expect_agreement<pump, pump::START_t> ({ 0x01, 0x05, 0x34 });
  // ack_nak of two rows: the row's own key is no part.
  expect_agreement<pump, pump::STOP_t> ({ 0x02 });
  // two_phase with a row and a field part: the field part is the
  // completion.
  expect_agreement<pump, pump::PRIME_t> ({ 0x03, 0xE8, 0x03 });
  expect_agreement<pump, pump::PRIME_t> ({ 0x03, 0xE8 });
  // The echo kinds.
  expect_agreement<pump, pump::SET_FLOW_t> ({ 0x04, 0x64, 0x00 });
  expect_agreement<pump, pump::SET_FLOW_t> ({ 0x04, 0x64 });
  expect_agreement<pump, pump::SET_LIMIT_t> ({ 0x05, 0x09 });
  expect_agreement<pump, pump::SET_LIMIT_t> ({ 0x05, 0x00 });
  expect_agreement<pump, pump::SET_MODE_t> ({ 0x07, 0x01 });
  expect_agreement<pump, pump::SET_MODE_t> ({ 0x07, 0x09 });
  expect_agreement<pump, pump::SET_STATE_t> ({ 0x16, 0x00 });
  expect_agreement<pump, pump::SET_STATE_t> ({ 0x16, 0x01 });
  expect_agreement<pump, pump::SET_GAIN_t> ({ 0x17, 0x00, 0x00 });
  expect_agreement<pump, pump::SET_GAIN_t> ({ 0x17, 0xFA, 0x00 });
  expect_agreement<pump, pump::SET_RATE_t> ({ 0x18, 0x00, 0x00, 0x00, 0x80 });
  expect_agreement<pump, pump::SET_RATE_t> ({ 0x18, 0x00, 0x00, 0x80, 0x3F });
  // An empty layout ACK and a row NAK; two parts of one shape.
  expect_agreement<pump, pump::TOUCH_t> ({ 0x0B });
  expect_agreement<pump, pump::TOUCH_t> ({ 0x0B, 0x04 });
  expect_agreement<pump, pump::BOTH_t> ({ 0x0D, 0x05 });
  // A tokenized layer.
  expect_agreement<tables::text_pump_t, tables::text_pump_t::GO_t> (
      { 'G', 'O' });
  expect_agreement<tables::text_pump_t, tables::text_pump_t::GO_t> (
      { 'G', 'O', ',', '3' });
}

TEST (LumexDialogueReplyKindTest,
      GivenAckNakFieldParts_WhenDecoded_ThenThePartAndItsValueComeBack)
{
  layout::bytes_t const ack = bytes ({ 0x01, 0x07 });
  auto const as_ack
      = dialogue::decode_reply_kind<pump, pump::START_t> (view_of (ack));
  ASSERT_TRUE (as_ack.has_value ());
  EXPECT_TRUE (as_ack->part == dialogue::reply_part::ack);
  EXPECT_EQ (as_ack->ack, 7u);
  layout::bytes_t const nak = bytes ({ 0x01, 0x05, 0x34, 0x12 });
  auto const as_nak
      = dialogue::decode_reply_kind<pump, pump::START_t> (view_of (nak));
  ASSERT_TRUE (as_nak.has_value ());
  EXPECT_TRUE (as_nak->part == dialogue::reply_part::nak);
  EXPECT_EQ (lumex::core::protocol::field::get<0> (as_nak->nak), 5u);
  EXPECT_EQ (lumex::core::protocol::field::get<1> (as_nak->nak), 0x1234u);
  // The fields alone (what describe reads) give the same.
  layout::bytes_t const fields = bytes ({ 0x05, 0x34, 0x12 });
  auto const by_fields
      = dialogue::decode_reply_fields<pump, pump::START_t> (view_of (fields));
  ASSERT_TRUE (by_fields.has_value ());
  EXPECT_TRUE (by_fields->part == dialogue::reply_part::nak);
}

TEST (LumexDialogueReplyKindTest,
      GivenTheMessageOfARowPart_WhenDecodedForTheRow_ThenItIsThatPart)
{
  layout::bytes_t const ack = bytes ({ 0x06 });
  auto const stop_ack
      = dialogue::decode_reply_kind<pump, pump::STOP_t> (view_of (ack));
  ASSERT_TRUE (stop_ack.has_value ());
  EXPECT_TRUE (stop_ack->part == dialogue::reply_part::ack);
  layout::bytes_t const nak = bytes ({ 0x15, 0x05 });
  auto const stop_nak
      = dialogue::decode_reply_kind<pump, pump::STOP_t> (view_of (nak));
  ASSERT_TRUE (stop_nak.has_value ());
  EXPECT_TRUE (stop_nak->part == dialogue::reply_part::nak);
  EXPECT_EQ (stop_nak->nak, 5u);
  auto const prime_ack
      = dialogue::decode_reply_kind<pump, pump::PRIME_t> (view_of (ack));
  ASSERT_TRUE (prime_ack.has_value ());
  EXPECT_TRUE (prime_ack->part == dialogue::reply_part::ack);
  layout::bytes_t const done = bytes ({ 0x03, 0xE8, 0x03 });
  auto const prime_done
      = dialogue::decode_reply_kind<pump, pump::PRIME_t> (view_of (done));
  ASSERT_TRUE (prime_done.has_value ());
  EXPECT_TRUE (prime_done->part == dialogue::reply_part::completion);
  EXPECT_EQ (prime_done->completion, 1000u);
  // An empty layout ACK of the row itself, a NAK row with its value.
  layout::bytes_t const touch = bytes ({ 0x0B });
  auto const touch_ack
      = dialogue::decode_reply_kind<pump, pump::TOUCH_t> (view_of (touch));
  ASSERT_TRUE (touch_ack.has_value ());
  EXPECT_TRUE (touch_ack->part == dialogue::reply_part::ack);
  auto const touch_nak
      = dialogue::decode_reply_kind<pump, pump::TOUCH_t> (view_of (nak));
  ASSERT_TRUE (touch_nak.has_value ());
  EXPECT_TRUE (touch_nak->part == dialogue::reply_part::nak);
  EXPECT_EQ (touch_nak->nak, 5u);
}

TEST (LumexDialogueReplyKindTest,
      GivenAMessageThatIsNoPart_WhenDecoded_ThenItIsUnexpectedOrUnknown)
{
  // The own key of a row whose parts are both rows.
  layout::bytes_t const own = bytes ({ 0x02 });
  auto const stop
      = dialogue::decode_reply_kind<pump, pump::STOP_t> (view_of (own));
  ASSERT_FALSE (stop.has_value ());
  EXPECT_EQ (stop.error (), protocol_errc::unexpected_reply);
  // Another row.
  layout::bytes_t const poll = bytes ({ 0x0C, 0x01 });
  auto const other
      = dialogue::decode_reply_kind<pump, pump::START_t> (view_of (poll));
  ASSERT_FALSE (other.has_value ());
  EXPECT_EQ (other.error (), protocol_errc::unexpected_reply);
  auto const echo_other
      = dialogue::decode_reply_kind<pump, pump::SET_FLOW_t> (view_of (poll));
  ASSERT_FALSE (echo_other.has_value ());
  EXPECT_EQ (echo_other.error (), protocol_errc::unexpected_reply);
  // A key of no row.
  layout::bytes_t const unknown = bytes ({ 0x7E, 0x01 });
  auto const nothing
      = dialogue::decode_reply_kind<pump, pump::START_t> (view_of (unknown));
  ASSERT_FALSE (nothing.has_value ());
  EXPECT_EQ (nothing.error (), protocol_errc::unknown_frame);
  // Fields that fit neither field part: the error of the last one tried.
  layout::bytes_t const short_nak = bytes ({ 0x01, 0x05, 0x34 });
  auto const neither
      = dialogue::decode_reply_kind<pump, pump::START_t> (view_of (short_nak));
  ASSERT_FALSE (neither.has_value ());
  EXPECT_EQ (neither.error (), protocol_errc::truncated);
}

TEST (LumexDialogueReplyKindTest,
      GivenTheEchoKinds_WhenDecoded_ThenAZeroIsARefusalOnlyWhereTheKindSaysSo)
{
  layout::bytes_t const zero_limit = bytes ({ 0x05, 0x00 });
  auto const refused = dialogue::decode_reply_kind<pump, pump::SET_LIMIT_t> (
      view_of (zero_limit));
  ASSERT_TRUE (refused.has_value ());
  EXPECT_TRUE (refused->part == dialogue::reply_part::refused);
  EXPECT_EQ (refused->value, 0u);
  layout::bytes_t const limit = bytes ({ 0x05, 0x09 });
  auto const taken
      = dialogue::decode_reply_kind<pump, pump::SET_LIMIT_t> (view_of (limit));
  ASSERT_TRUE (taken.has_value ());
  EXPECT_TRUE (taken->part == dialogue::reply_part::echo);
  EXPECT_EQ (taken->value, 9u);
  layout::bytes_t const zero_flow = bytes ({ 0x04, 0x00, 0x00 });
  auto const flow = dialogue::decode_reply_kind<pump, pump::SET_FLOW_t> (
      view_of (zero_flow));
  ASSERT_TRUE (flow.has_value ());
  EXPECT_TRUE (flow->part == dialogue::reply_part::echo);
  layout::bytes_t const mode = bytes ({ 0x07, 0x02 });
  auto const state
      = dialogue::decode_reply_kind<pump, pump::SET_MODE_t> (view_of (mode));
  ASSERT_TRUE (state.has_value ());
  EXPECT_TRUE (state->part == dialogue::reply_part::echo);
  EXPECT_TRUE (state->value == tables::pump_state::fault);
  EXPECT_EQ (std::string (dialogue::reply_part_name (
                 static_cast<dialogue::reply_part> (9))),
             "unknown");
}

// The dialogue vocabulary (lumex/applied/transport/protocol/dsl): the reply
// kinds as the index describes and exports them, the row options in the
// run-time entries, the traits of a row and of a protocol the engine reads,
// the protocol options of a header (both forms, a layer, none), the values
// a reply kind gives the client, and the words of the registries next to
// those of the dialogue.

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/dsl/LumexDialogueTestTables.hpp"

namespace
{
namespace tables = lumex_tests::dialogue_tables;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
using tables::bytes;
using tables::view_of;
using pump = tables::pump_t;

index::protocol_index_t const &
pump_index ()
{
  index::index_result_t const &result = index::protocol_index<pump> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

std::string
rx (index::protocol_index_t const &where,
    std::initializer_list<int> const &frame)
{
  layout::bytes_t const message = bytes (frame);
  return where.describe (view_of (message), index::frame_direction::rx);
}

index::row_t const &
row_named (index::protocol_index_t const &where, char const *name)
{
  index::row_t const *const row = where.find_name (name);
  EXPECT_NE (row, nullptr) << name;
  return row != nullptr ? *row : where.rows ().data ()[0];
}

std::string
text_or_empty (char const *text)
{
  return text != nullptr ? std::string (text) : std::string ();
}

template <typename Row> using row_of = dialogue::row_traits_t<pump, Row>;
using protocol_of = dialogue::protocol_traits_t<pump>;
} // namespace

// The whole table passes the checks the index asks the vocabulary for.
static_assert (dialogue::check_protocol_t<tables::pump_t>::value,
               "the pump table is a valid dialogue table");
static_assert (dialogue::check_protocol_t<tables::pump_v2_t>::value,
               "its layer too");
static_assert (dialogue::check_protocol_t<tables::text_pump_t>::value,
               "and its tokenized twin");

TEST (LumexDialogueVocabularyTest,
      GivenAnAckNakOfTwoFields_WhenTheReplyIsDescribed_ThenItSaysAckOrNak)
{
  EXPECT_EQ (rx (pump_index (), { 0x01, 0x07 }),
             "RX pump_t.START (Start) ACK value=7");
  EXPECT_EQ (rx (pump_index (), { 0x01, 0x05, 0x34, 0x12 }),
             "RX pump_t.START (Start) NAK code=5 detail=4660");
  EXPECT_EQ (rx (pump_index (), { 0x01, 0x05, 0x34 }),
             "RX pump_t.START (Start) [decode error: message shorter than "
             "its fields] len=3: 01 05 34");
  layout::bytes_t const request = bytes ({ 0x01, 0x07 });
  EXPECT_EQ (
      pump_index ().describe (view_of (request), index::frame_direction::tx),
      "TX pump_t.START (Start) channel=7");
}

TEST (
    LumexDialogueVocabularyTest,
    GivenAnAckNakOfTwoMessages_WhenTheyAreDescribed_ThenTheyAreTheirRowsAndTheOwnKeyIsUnexpected)
{
  EXPECT_EQ (rx (pump_index (), { 0x06 }), "RX pump_t.ACK (Accepted)");
  EXPECT_EQ (rx (pump_index (), { 0x15, 0x03 }),
             "RX pump_t.NAK (Refused) reply=3");
  EXPECT_EQ (rx (pump_index (), { 0x02 }),
             "RX pump_t.STOP (Stop) [decode error: a reply other than the "
             "one expected] len=1: 02");
}

TEST (LumexDialogueVocabularyTest,
      GivenTwoFieldPartsOfOneShape_WhenTheReplyIsDescribed_ThenTheFirstWins)
{
  EXPECT_EQ (rx (pump_index (), { 0x0D, 0x05 }),
             "RX pump_t.BOTH (Two parts of one shape) ACK value=5");
}

TEST (
    LumexDialogueVocabularyTest,
    GivenALayerThatReplacesANamedRowAndListsTheNamingRow_WhenRead_ThenItNamesTheRowOfTheLayer)
{
  using v2 = tables::pump_v2_t;
  index::index_result_t const &result = index::protocol_index<v2> ();
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (
      text_or_empty (
          row_named (*result, "WRITE").entry->options.extra.data ()[0].text),
      "CHANGE");
  EXPECT_TRUE (
      (std::is_same<dialogue::row_traits_t<v2, v2::WRITE_t>::reply_row,
                    v2::CHANGE_t>::value));
  EXPECT_FALSE ((std::is_same<v2::WRITE_t, pump::WRITE_t>::value));
  EXPECT_TRUE (
      (std::is_same<dialogue::reply_value_of_t<v2::WRITE_t>::type,
                    layout::reply_value_of<v2::CHANGE_t>::type>::value));
}

TEST (LumexDialogueVocabularyTest,
      GivenCorrelationsNamedByTheRow_WhenEntered_ThenEachHasItsName)
{
  dsl::extra_options_t const query
      = row_named (pump_index (), "QUERY_NAMED").entry->options.extra;
  ASSERT_EQ (query.size (), 2u);
  EXPECT_EQ (std::string (query.data ()[1].name), "correlation");
  EXPECT_EQ (query.data ()[1].value, 3);
  EXPECT_EQ (text_or_empty (query.data ()[1].text), "custom");
  dsl::extra_options_t const write
      = row_named (pump_index (), "WRITE_NAMED").entry->options.extra;
  ASSERT_EQ (write.size (), 2u);
  EXPECT_EQ (write.data ()[1].value, 2);
  EXPECT_EQ (text_or_empty (write.data ()[1].text), "by_reply_key");
  EXPECT_TRUE (row_of<pump::QUERY_NAMED_t>::how ()
               == dialogue::correlation::custom);
  EXPECT_TRUE (row_of<pump::WRITE_NAMED_t>::how ()
               == dialogue::correlation::by_reply_key);
  EXPECT_EQ (
      std::string (dialogue::correlation_name (dialogue::correlation::by_key)),
      "by_key");
}

TEST (
    LumexDialogueVocabularyTest,
    GivenEchoZeroOfAnEnumAScaledNumberAndAFloat_WhenDescribed_ThenZeroIsTheRefusal)
{
  EXPECT_EQ (rx (pump_index (), { 0x16, 0x00 }),
             "RX pump_t.SET_STATE (Set the state) REFUSED value=0");
  EXPECT_EQ (rx (pump_index (), { 0x16, 0x01 }),
             "RX pump_t.SET_STATE (Set the state) ECHO value=1");
  EXPECT_EQ (rx (pump_index (), { 0x17, 0x00, 0x00 }),
             "RX pump_t.SET_GAIN (Set the gain) REFUSED value=0");
  EXPECT_EQ (rx (pump_index (), { 0x17, 0xFA, 0x00 }),
             "RX pump_t.SET_GAIN (Set the gain) ECHO value=2.5");
  EXPECT_EQ (rx (pump_index (), { 0x18, 0x00, 0x00, 0x00, 0x00 }),
             "RX pump_t.SET_RATE (Set the rate) REFUSED value=0");
  EXPECT_EQ (rx (pump_index (), { 0x18, 0x00, 0x00, 0x00, 0x80 })
                 .find ("RX pump_t.SET_RATE (Set the rate) REFUSED value="),
             0u);
  EXPECT_EQ (rx (pump_index (), { 0x18, 0x00, 0x00, 0x80, 0x3F }),
             "RX pump_t.SET_RATE (Set the rate) ECHO value=1");
  EXPECT_TRUE (
      (std::is_same<dialogue::reply_value_of_t<pump::SET_RATE_t>::type,
                    float>::value));
}

TEST (LumexDialogueVocabularyTest,
      GivenAReplyKindOfAnotherLayerAndAFinalHeader_WhenRead_ThenBothAreSeen)
{
  EXPECT_TRUE ((dialogue::row_traits_t<tables::foreign_t,
                                       tables::foreign_t::ODD_t>::kind ()
                == dialogue::reply_kind::other));
  EXPECT_EQ (dialogue::protocol_traits_t<tables::foreign_t>::watchdog_ms (),
             700u);
  index::index_result_t const &result
      = index::protocol_index<tables::foreign_t> ();
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (rx (*result, { 0x01, 0x02 }),
             "RX foreign_t.ODD (A reply of another layer) len=2: 01 02");
}

TEST (LumexDialogueVocabularyTest,
      GivenAnEmptyAck_WhenTheReplyIsDescribed_ThenItIsTheWordAlone)
{
  EXPECT_EQ (rx (pump_index (), { 0x0B }), "RX pump_t.TOUCH (Touch) ACK");
  EXPECT_EQ (rx (pump_index (), { 0x0B, 0x04 }),
             "RX pump_t.TOUCH (Touch) [decode error: bytes after the last "
             "field of a message] len=2: 0B 04");
}

TEST (LumexDialogueVocabularyTest,
      GivenATwoPhaseReply_WhenTheCompletionIsDescribed_ThenItIsNamed)
{
  EXPECT_EQ (rx (pump_index (), { 0x03, 0xE8, 0x03 }),
             "RX pump_t.PRIME (Prime) COMPLETION value=1000");
}

TEST (LumexDialogueVocabularyTest,
      GivenTheEchoKinds_WhenTheirRepliesAreDescribed_ThenAZeroEchoIsARefusal)
{
  EXPECT_EQ (rx (pump_index (), { 0x04, 0x64, 0x00 }),
             "RX pump_t.SET_FLOW (Set the flow) ECHO value=100");
  EXPECT_EQ (rx (pump_index (), { 0x05, 0x09 }),
             "RX pump_t.SET_LIMIT (Set the limit) ECHO value=9");
  EXPECT_EQ (rx (pump_index (), { 0x05, 0x00 }),
             "RX pump_t.SET_LIMIT (Set the limit) REFUSED value=0");
  EXPECT_EQ (rx (pump_index (), { 0x07, 0x01 }),
             "RX pump_t.SET_MODE (Set the mode) ECHO value=1");
}

TEST (LumexDialogueVocabularyTest,
      GivenATokenizedLayer_WhenAnAckNakIsDescribed_ThenTheKeyTokenIsRead)
{
  index::index_result_t const &result
      = index::protocol_index<tables::text_pump_t> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const ack = bytes ({ 'G', 'O' });
  EXPECT_EQ (result->describe (view_of (ack), index::frame_direction::rx),
             "RX text_pump_t.GO (Go) ACK");
  layout::bytes_t const nak = bytes ({ 'G', 'O', ',', '3' });
  EXPECT_EQ (result->describe (view_of (nak), index::frame_direction::rx),
             "RX text_pump_t.GO (Go) NAK code=3");
}

TEST (LumexDialogueVocabularyTest,
      GivenTheRowsOfThePump_WhenIndexed_ThenEachReplyKindNamesItsParts)
{
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "START").reply_kind),
             "ack_nak (ACK: one field; NAK: layout `refusal_t`)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "STOP").reply_kind),
             "ack_nak (ACK: message `ACK`; NAK: message `NAK`)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "PRIME").reply_kind),
             "two_phase (ACK: message `ACK`; completion: one field)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "SET_FLOW").reply_kind),
             "echo_set (one field)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "SET_LIMIT").reply_kind),
             "echo_zero_on_error (one field)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "SET_MODE").reply_kind),
             "state_instead_of_refusal (one field)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "TOUCH").reply_kind),
             "ack_nak (ACK: layout `nothing_t`; NAK: message `NAK`)");
  EXPECT_EQ (text_or_empty (row_named (pump_index (), "POLL").reply_kind), "");
}

TEST (LumexDialogueVocabularyTest,
      GivenTheRowOptionsOfTheDialogue_WhenEntered_ThenTheyAreListedInExtra)
{
  dsl::extra_options_t const status
      = row_named (pump_index (), "STATUS").entry->options.extra;
  ASSERT_EQ (status.size (), 3u);
  EXPECT_EQ (std::string (status.data ()[0].name), "edge_event");
  EXPECT_EQ (status.data ()[0].value, 0);
  EXPECT_TRUE (status.data ()[0].repeats);
  EXPECT_EQ (text_or_empty (status.data ()[0].text), "field 0: 0 -> 1");
  EXPECT_EQ (text_or_empty (status.data ()[1].text), "field 0: 1 -> 0");
  EXPECT_EQ (status.data ()[2].value, 1);
  EXPECT_EQ (text_or_empty (status.data ()[2].text), "field 1: -1 -> 2");
  dsl::extra_options_t const start
      = row_named (pump_index (), "START").entry->options.extra;
  ASSERT_EQ (start.size (), 1u);
  EXPECT_EQ (std::string (start.data ()[0].name), "min_interval");
  EXPECT_EQ (start.data ()[0].value, 50);
  EXPECT_EQ (start.data ()[0].text, nullptr);
  dsl::extra_options_t const write
      = row_named (pump_index (), "WRITE").entry->options.extra;
  ASSERT_EQ (write.size (), 1u);
  EXPECT_EQ (std::string (write.data ()[0].name), "reply_row");
  EXPECT_EQ (text_or_empty (write.data ()[0].text), "CHANGE");
  dsl::extra_options_t const read
      = row_named (pump_index (), "READ").entry->options.extra;
  ASSERT_EQ (read.size (), 1u);
  EXPECT_EQ (std::string (read.data ()[0].name), "correlation");
  EXPECT_EQ (read.data ()[0].value, 1);
  EXPECT_EQ (text_or_empty (read.data ()[0].text), "by_order");
  dsl::extra_options_t const query
      = row_named (pump_index (), "QUERY").entry->options.extra;
  ASSERT_EQ (query.size (), 1u);
  EXPECT_EQ (std::string (query.data ()[0].name), "match_with");
  EXPECT_EQ (text_or_empty (query.data ()[0].text), "custom");
  EXPECT_EQ (row_named (pump_index (), "POLL").entry->options.extra.size (),
             0u);
}

TEST (LumexDialogueVocabularyTest,
      GivenThePump_WhenExportedAsMarkdown_ThenTheReplyKindsAndOptionsAreShown)
{
  std::string const text = index::export_markdown (pump_index ());
  EXPECT_NE (text.find ("### STOP\n\n"
                        "Command `STOP`, key `0x02`.\n\n"
                        "> Stop\n\n"
                        "Reply: a reply kind of the dialogue, ack_nak (ACK: "
                        "message `ACK`; NAK: message `NAK`).\n\n"
                        "Request fields:\n\n"
                        "none\n\n"),
             std::string::npos)
      << text;
  EXPECT_NE (text.find ("Reply: a reply kind of the dialogue, ack_nak (ACK: "
                        "one field; NAK: layout `refusal_t`).\n\n"
                        "Options: min_interval 50.\n\n"),
             std::string::npos);
  EXPECT_NE (text.find ("Options: stream every 100 ms; edge_event field 0: "
                        "0 -> 1; edge_event field 0: 1 -> 0; edge_event field "
                        "1: -1 -> 2.\n\n"),
             std::string::npos);
  EXPECT_NE (text.find ("Options: reply_row CHANGE.\n\n"), std::string::npos);
  EXPECT_NE (text.find ("Options: correlation by_order.\n\n"),
             std::string::npos);
}

TEST (LumexDialogueVocabularyTest,
      GivenEveryRow_WhenItsCorrelationIsAsked_ThenTheDefaultFollowsTheReply)
{
  EXPECT_TRUE (row_of<pump::POLL_t>::how () == dialogue::correlation::by_key);
  EXPECT_TRUE (row_of<pump::START_t>::how () == dialogue::correlation::by_key);
  // A row part anywhere (here the NAK) means the reply may carry no key.
  EXPECT_TRUE (row_of<pump::TOUCH_t>::how ()
               == dialogue::correlation::by_order);
  EXPECT_TRUE (row_of<pump::STOP_t>::how ()
               == dialogue::correlation::by_order);
  EXPECT_TRUE (row_of<pump::PRIME_t>::how ()
               == dialogue::correlation::by_order);
  EXPECT_TRUE (row_of<pump::WRITE_t>::how ()
               == dialogue::correlation::by_reply_key);
  EXPECT_TRUE (row_of<pump::READ_t>::how ()
               == dialogue::correlation::by_order);
  EXPECT_TRUE (row_of<pump::QUERY_t>::how () == dialogue::correlation::custom);
  EXPECT_TRUE (row_of<pump::SET_FLOW_t>::how ()
               == dialogue::correlation::by_key);
}

TEST (LumexDialogueVocabularyTest,
      GivenEveryRow_WhenItsReplyKindIsAsked_ThenItIsTheKindOfItsReplyGroup)
{
  EXPECT_TRUE (row_of<pump::CLOCK_t>::kind () == dialogue::reply_kind::none);
  EXPECT_TRUE (row_of<pump::POLL_t>::kind () == dialogue::reply_kind::message);
  EXPECT_TRUE (row_of<pump::STATUS_t>::kind ()
               == dialogue::reply_kind::message);
  EXPECT_TRUE (row_of<pump::START_t>::kind ()
               == dialogue::reply_kind::ack_nak);
  EXPECT_TRUE (row_of<pump::PRIME_t>::kind ()
               == dialogue::reply_kind::two_phase);
  EXPECT_TRUE (row_of<pump::SET_FLOW_t>::kind ()
               == dialogue::reply_kind::echo_set);
  EXPECT_TRUE (row_of<pump::SET_LIMIT_t>::kind ()
               == dialogue::reply_kind::echo_zero_on_error);
  EXPECT_TRUE (row_of<pump::SET_MODE_t>::kind ()
               == dialogue::reply_kind::state_instead_of_refusal);
}

TEST (LumexDialogueVocabularyTest,
      GivenTheRowOptions_WhenTheTraitsAreRead_ThenEveryValueIsThere)
{
  EXPECT_TRUE (
      (std::is_same<row_of<pump::WRITE_t>::reply_row, pump::CHANGE_t>::value));
  EXPECT_TRUE ((std::is_same<row_of<pump::READ_t>::reply_row, void>::value));
  EXPECT_EQ (row_of<pump::START_t>::min_interval_ms (), 50u);
  EXPECT_EQ (row_of<pump::STOP_t>::min_interval_ms (), 0u);
  EXPECT_TRUE (row_of<pump::QUERY_t>::matcher () == &tables::match_any);
  EXPECT_TRUE (row_of<pump::READ_t>::matcher () == nullptr);
  using edges = row_of<pump::STATUS_t>::edge_events;
  EXPECT_EQ (dsl::list_size<edges>::value, 3u);
  using third = dsl::list_at<2, edges>::type;
  EXPECT_EQ (third::field (), 1u);
  EXPECT_EQ (third::from (), -1);
  EXPECT_EQ (third::to (), 2);
  using first = dsl::list_at<0, edges>::type;
  using second = dsl::list_at<1, edges>::type;
  EXPECT_EQ (first::field (), 0u);
  EXPECT_EQ (first::from (), 0);
  EXPECT_EQ (first::to (), 1);
  EXPECT_EQ (second::from (), 1);
  EXPECT_EQ (second::to (), 0);
  EXPECT_EQ (dsl::list_size<row_of<pump::START_t>::edge_events>::value, 0u);
}

TEST (LumexDialogueVocabularyTest,
      GivenATemplateOfProtocolOptions_WhenRead_ThenEveryOptionIsThere)
{
  EXPECT_EQ (protocol_of::max_in_flight (), 1u);
  EXPECT_EQ (protocol_of::watchdog_ms (), 3000u);
  EXPECT_EQ (protocol_of::min_interval_ms (), 20u);
  EXPECT_EQ (protocol_of::default_timeout_ms (), 400u);
  EXPECT_EQ (protocol_of::default_retries (), 1u);
  EXPECT_EQ (protocol_of::late_reply_window_ms (), 150u);
  EXPECT_EQ (protocol_of::keepalive_period_ms (), 1000u);
  EXPECT_TRUE (
      (std::is_same<protocol_of::keepalive_row, pump::CLOCK_t>::value));
  EXPECT_TRUE ((std::is_same<protocol_of::state_row, pump::STATUS_t>::value));
  EXPECT_EQ (protocol_of::state_field (), 1u);
  EXPECT_TRUE ((std::is_same<protocol_of::identify_row, pump::INFO_t>::value));
}

TEST (LumexDialogueVocabularyTest,
      GivenALayer_WhenItsProtocolOptionsAreRead_ThenTheyNameItsOwnRows)
{
  using layer = dialogue::protocol_traits_t<tables::pump_v2_t>;
  EXPECT_TRUE (
      (std::is_same<layer::state_row, tables::pump_v2_t::STATUS_t>::value));
  EXPECT_FALSE ((std::is_same<layer::state_row, pump::STATUS_t>::value));
  EXPECT_TRUE ((std::is_same<layer::identify_row, pump::INFO_t>::value));
  EXPECT_EQ (layer::watchdog_ms (), 3000u);
}

TEST (LumexDialogueVocabularyTest,
      GivenAPlainMemberOrNoProtocolOptions_WhenRead_ThenTheDefaultsFillTheRest)
{
  using fixed = dialogue::protocol_traits_t<tables::fixed_t>;
  EXPECT_EQ (fixed::watchdog_ms (), 500u);
  EXPECT_EQ (fixed::max_in_flight (), 4u);
  EXPECT_EQ (fixed::min_interval_ms (), 0u);
  EXPECT_TRUE ((std::is_same<fixed::keepalive_row, void>::value));
  using plain = dialogue::protocol_traits_t<tables::plain_t>;
  EXPECT_TRUE ((std::is_same<plain::options, dialogue::options<>>::value));
  EXPECT_EQ (plain::max_in_flight (), 1u);
  EXPECT_EQ (plain::watchdog_ms (), 0u);
  EXPECT_EQ (plain::default_timeout_ms (), 0u);
  EXPECT_EQ (plain::default_retries (), 0u);
  EXPECT_EQ (plain::late_reply_window_ms (), 0u);
  EXPECT_EQ (plain::keepalive_period_ms (), 0u);
  EXPECT_TRUE ((std::is_same<plain::state_row, void>::value));
  EXPECT_TRUE ((std::is_same<plain::identify_row, void>::value));
}

TEST (LumexDialogueVocabularyTest,
      GivenEveryReplyKind_WhenItsValueIsAsked_ThenItIsWhatTheClientReturns)
{
  EXPECT_TRUE ((std::is_same<dialogue::reply_value_of_t<pump::START_t>::type,
                             std::uint8_t>::value));
  EXPECT_TRUE ((std::is_same<dialogue::reply_value_of_t<pump::STOP_t>::type,
                             field::record<>>::value));
  EXPECT_TRUE ((std::is_same<dialogue::reply_value_of_t<pump::PRIME_t>::type,
                             std::uint16_t>::value));
  EXPECT_TRUE (
      (std::is_same<dialogue::reply_value_of_t<pump::SET_MODE_t>::type,
                    tables::pump_state>::value));
  EXPECT_TRUE (
      (std::is_same<dialogue::reply_value_of_t<pump::POLL_t>::type,
                    layout::reply_value_of<pump::POLL_t>::type>::value));
  EXPECT_TRUE ((std::is_same<dialogue::reply_value_of_t<pump::CLOCK_t>::type,
                             field::record<>>::value));
  EXPECT_TRUE (
      (std::is_same<dialogue::reply_value_of_t<pump::WRITE_t>::type,
                    layout::reply_value_of<pump::CHANGE_t>::type>::value));
}

TEST (LumexDialogueVocabularyTest,
      GivenTheWordsOfARegistry_WhenUsedWithTheDialogue_ThenBothWork)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<tables::fixed_t, tables::fixed_t::SET_VALVE_t> (
          out, tables::valve_registry::OPENING, 40)));
  layout::bytes_t frame = bytes ({ 0x20 });
  frame.insert (frame.end (), out.begin (), out.end ());
  index::index_result_t const result
      = index::build_index<tables::fixed_t, tables::valve_registry> ();
  ASSERT_TRUE (result.has_value ());
  EXPECT_NE (result->describe (view_of (frame), index::frame_direction::tx)
                 .find ("TX fixed_t.SET_VALVE (Set a valve parameter) "
                        "parameter=OPENING"),
             std::string::npos);
  layout::bytes_t const echo = bytes ({ 0x20, 0x28 });
  EXPECT_EQ (result->describe (view_of (echo), index::frame_direction::rx),
             "RX fixed_t.SET_VALVE (Set a valve parameter) ECHO value=40");
}

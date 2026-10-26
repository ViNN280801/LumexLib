// describe (lumex/core/protocol/index): the log line of a frame in both
// directions, for every kind of field and in every layout, the unknown and
// the damaged frames, the options, the session context and the falsifier of
// the design (a name or a scale changed in one place only).

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace gauge_tables = lumex_tests::field_tables;
namespace index = lumex::core::protocol::index;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
using tables::bytes_of;
using tables::view_of;

LUMEX_PROTOCOL_CONTEXT_KEY (probe_kind);

LUMEX_PROTOCOL_LAYOUT (
    probe_t, (union_by_context_<probe_kind, case_<1, u8_>, case_<2, u16_be_>>),
    ("sample"));

// clang-format off
#define PROBE_ROWS(X, X_AS) \
  X (READ_PROBE, 0x40, "Read the probe", (), (), (probe_t), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (probe_protocol, tables::codec, tables::widget_header,
                       PROBE_ROWS);

std::vector<std::uint8_t>
bytes (std::initializer_list<int> values)
{
  std::vector<std::uint8_t> out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

index::protocol_index_t const &
widget_index ()
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

std::string
describe_tx (layout::bytes_t const &frame)
{
  return widget_index ().describe (view_of (frame),
                                   index::frame_direction::tx);
}

std::string
describe_rx (layout::bytes_t const &frame)
{
  return widget_index ().describe (view_of (frame),
                                   index::frame_direction::rx);
}

// The frame of a request of widget: the key byte, then the fields.
template <typename Row, typename... Args>
layout::bytes_t
widget_request (std::uint8_t key, Args const &...args)
{
  return tables::request_frame<tables::widget, Row> (key, args...);
}
} // namespace

TEST (
    LumexProtocolDescribeTest,
    GivenARequest_WhenDescribed_ThenTheLineHasTheNameTheDescriptionAndTheEngineeringValue)
{
  layout::bytes_t const frame
      = widget_request<tables::widget::SET_SPEED_t> (0x02, 12.5);
  EXPECT_EQ (describe_tx (frame),
             "TX widget.SET_SPEED (Set the speed) speed=12.5 rpm");
}

TEST (LumexProtocolDescribeTest,
      GivenARowWithNoFields_WhenDescribed_ThenTheLineEndsAfterTheDescription)
{
  layout::bytes_t const frame = bytes ({ 0x01 });
  EXPECT_EQ (describe_tx (frame), "TX widget.PING (Liveness check)");
  EXPECT_EQ (describe_rx (frame), "RX widget.PING (Liveness check)");
}

TEST (LumexProtocolDescribeTest,
      GivenARowWithAnXAsName_WhenDescribed_ThenTheSpecificationNameIsUsed)
{
  layout::bytes_t const frame = bytes ({ 0x05 });
  EXPECT_EQ (describe_tx (frame), "TX widget.DELETE (Erase the log)");
}

TEST (
    LumexProtocolDescribeTest,
    GivenALayoutReply_WhenDescribed_ThenTheFieldsAreNamedByTheLayoutAndTheEnumByItsName)
{
  layout::bytes_t const frame = bytes ({ 0x03, 0x01, 0x03 });
  EXPECT_EQ (describe_rx (frame),
             "RX widget.STATUS (Read the state) mode=run level=3");
}

TEST (LumexProtocolDescribeTest,
      GivenASingleFieldReply_WhenDescribed_ThenItIsCalledReplyAndHasItsUnit)
{
  layout::bytes_t const frame = bytes ({ 0x04, 0x08, 0x66 });
  EXPECT_EQ (describe_rx (frame),
             "RX widget.READ_TEMP (Read the temperature) reply=21.5 degC");
  layout::bytes_t const request = bytes ({ 0x04, 0x02 });
  EXPECT_EQ (describe_tx (request),
             "TX widget.READ_TEMP (Read the temperature) channel=2");
}

TEST (LumexProtocolDescribeTest,
      GivenAnEvent_WhenDescribed_ThenItIsAnRxLineOfItsRow)
{
  layout::bytes_t const frame = bytes ({ 0x06, 0x02, 0x09 });
  EXPECT_EQ (describe_rx (frame),
             "RX widget.TICK (Periodic state) mode=hold level=9");
}

TEST (
    LumexProtocolDescribeTest,
    GivenAGenericCommand_WhenDescribed_ThenTheEntryIsNamedAndTheValueHasTheEntryUnit)
{
  layout::bytes_t const request
      = widget_request<tables::widget::SET_PARAMETER_t> (
          0x20, tables::lamp::FREQUENCY, 20.0);
  EXPECT_EQ (describe_tx (request),
             "TX widget.SET_PARAMETER (Write a parameter) "
             "parameter=FREQUENCY value=20 Hz");
  layout::bytes_t const reply = bytes ({ 0x21, 0x12, 0x2B, 0xFE, 0xFF, 0xFF });
  EXPECT_EQ (describe_rx (reply), "RX widget.GET_PARAMETER (Read a parameter) "
                                  "parameter=OFFSET value=-4.69");
}

TEST (
    LumexProtocolDescribeTest,
    GivenAFrameWithAnUnknownKey_WhenDescribed_ThenTheLineHasTheKeyTheLengthAndTheDump)
{
  layout::bytes_t const frame = bytes ({ 0x99, 0x01, 0x02 });
  EXPECT_EQ (describe_tx (frame), "TX unknown key=0x99 len=3: 99 01 02");
  EXPECT_EQ (describe_rx (frame), "RX unknown key=0x99 len=3: 99 01 02");
  layout::bytes_t const seven = bytes ({ 0x15, 1, 2, 3, 4, 5, 6 });
  EXPECT_EQ (describe_tx (seven),
             "TX unknown key=0x15 len=7: 15 01 02 03 04 05 06");
}

TEST (LumexProtocolDescribeTest,
      GivenAnEmptyFrame_WhenDescribed_ThenThereIsNoKeyAndNoDump)
{
  layout::bytes_t const frame;
  EXPECT_EQ (describe_rx (frame), "RX unknown len=0");
}

TEST (LumexProtocolDescribeTest,
      GivenALongUnknownFrame_WhenDescribed_ThenTheDumpIsCutAtTheLimit)
{
  layout::bytes_t const frame = bytes ({ 0x99, 1, 2, 3, 4 });
  index::describe_options_t options;
  options.dump_limit = 3;
  EXPECT_EQ (widget_index ().describe (view_of (frame),
                                       index::frame_direction::tx, nullptr,
                                       options),
             "TX unknown key=0x99 len=5: 99 01 02 ...");
  options.dump_limit = 5;
  EXPECT_EQ (widget_index ().describe (view_of (frame),
                                       index::frame_direction::tx, nullptr,
                                       options),
             "TX unknown key=0x99 len=5: 99 01 02 03 04");
}

TEST (
    LumexProtocolDescribeTest,
    GivenATruncatedKnownFrame_WhenDescribed_ThenTheErrorAndTheDumpFollowTheName)
{
  layout::bytes_t const frame = bytes ({ 0x02, 0x7D });
  EXPECT_EQ (describe_tx (frame),
             "TX widget.SET_SPEED (Set the speed) [decode error: message "
             "shorter than its fields] len=2: 02 7D");
}

TEST (LumexProtocolDescribeTest,
      GivenTrailingBytes_WhenDescribed_ThenTheErrorSaysSo)
{
  layout::bytes_t const frame = bytes ({ 0x01, 0x55 });
  EXPECT_EQ (describe_rx (frame),
             "RX widget.PING (Liveness check) [decode error: bytes after the "
             "last field of a message] len=2: 01 55");
}

TEST (
    LumexProtocolDescribeTest,
    GivenAnEnumerationNoEnumeratorHas_WhenDescribed_ThenTheDecodeErrorIsShown)
{
  layout::bytes_t const frame = bytes ({ 0x03, 0x07, 0x03 });
  EXPECT_EQ (describe_rx (frame),
             "RX widget.STATUS (Read the state) [decode error: field value "
             "the field type cannot represent] len=3: 03 07 03");
}

TEST (
    LumexProtocolDescribeTest,
    GivenTheRawOption_WhenDescribed_ThenTheRawValueFollowsTheEngineeringValue)
{
  layout::bytes_t const frame
      = widget_request<tables::widget::SET_SPEED_t> (0x02, 12.5);
  index::describe_options_t options;
  options.show_raw = true;
  EXPECT_EQ (widget_index ().describe (view_of (frame),
                                       index::frame_direction::tx, nullptr,
                                       options),
             "TX widget.SET_SPEED (Set the speed) speed=12.5 rpm (raw 125)");
}

TEST (
    LumexProtocolDescribeTest,
    GivenALayerThatChangesTheScale_WhenTheSameValueIsDescribed_ThenTheLineKeepsTheEngineeringValue)
{
  index::index_result_t const &layer
      = index::protocol_index<tables::widget_v2> ();
  ASSERT_TRUE (layer.has_value ());
  layout::bytes_t const frame
      = tables::request_frame<tables::widget_v2,
                              tables::widget_v2::SET_SPEED_t> (0x02, 12.5);
  // 12.5 is 1250 at a scale of 0.01: the line shows the engineering value.
  EXPECT_EQ (frame, bytes ({ 0x02, 0xE2, 0x04 }));
  EXPECT_EQ (layer->describe (view_of (frame), index::frame_direction::tx),
             "TX widget_v2.SET_SPEED (Set the speed) speed=12.5 rpm");
}

TEST (LumexProtocolDescribeTest,
      GivenATokenizedLayer_WhenDescribed_ThenTheLineIsTheSameAsInBinary)
{
  index::index_result_t const &text
      = index::protocol_index<tables::widget_text> ();
  ASSERT_TRUE (text.has_value ());
  layout::bytes_t frame;
  ASSERT_TRUE (
      (layout::encode_request<tables::widget_text,
                              tables::widget::SET_SPEED_t> (frame, 12.5))
          .has_value ());
  EXPECT_EQ (std::string (frame.begin (), frame.end ()), "2,125");
  EXPECT_EQ (text->describe (view_of (frame), index::frame_direction::tx),
             "TX widget_text.SET_SPEED (Set the speed) speed=12.5 rpm");
}

TEST (
    LumexProtocolDescribeTest,
    GivenASessionValueTheDecoderReads_WhenDescribed_ThenTheCaseFollowsTheSession)
{
  index::index_result_t const &probe
      = index::protocol_index<probe_protocol> ();
  ASSERT_TRUE (probe.has_value ());
  layout::bytes_t const one = bytes ({ 0x40, 0x07 });
  layout::bytes_t const two = bytes ({ 0x40, 0x01, 0x02 });
  index::session_context_t session;
  session.set ("probe_kind", 1);
  EXPECT_EQ (
      probe->describe (view_of (one), index::frame_direction::rx, &session),
      "RX probe_protocol.READ_PROBE (Read the probe) sample=case 1: 7");
  session.set ("probe_kind", 2);
  EXPECT_EQ (
      probe->describe (view_of (two), index::frame_direction::rx, &session),
      "RX probe_protocol.READ_PROBE (Read the probe) sample=case 2: "
      "258");
}

TEST (
    LumexProtocolDescribeTest,
    GivenNoSessionOrAnUnknownValue_WhenAContextUnionIsDescribed_ThenTheErrorIsShown)
{
  index::index_result_t const &probe
      = index::protocol_index<probe_protocol> ();
  ASSERT_TRUE (probe.has_value ());
  layout::bytes_t const frame = bytes ({ 0x40, 0x07 });
  std::string const expected
      = "RX probe_protocol.READ_PROBE (Read the probe) [decode error: "
        "discriminant matches no case of the union] len=2: 40 07";
  EXPECT_EQ (probe->describe (view_of (frame), index::frame_direction::rx),
             expected);
  index::session_context_t session;
  EXPECT_EQ (
      probe->describe (view_of (frame), index::frame_direction::rx, &session),
      expected);
  session.set ("probe_kind", 3);
  EXPECT_EQ (
      probe->describe (view_of (frame), index::frame_direction::rx, &session),
      expected);
}

namespace
{
namespace gt = lumex_tests::field_tables;

typedef field::record_of<gt::sink_t::fields_type>::type sink;
typedef field::record_of<gt::point_t::fields_type>::type point;

void
set_point (point &value, std::int16_t x, std::uint8_t flags)
{
  field::get<0> (value) = x;
  field::get<1> (value) = flags;
}

// A sink with a known value in every one of its 40 fields; the payload of
// the nested message of the long tail lives in `payload`.
sink
make_sink (bool long_tail, std::vector<std::uint8_t> &payload)
{
  sink value = sink ();
  field::get<0> (value) = 200;
  field::get<1> (value) = -5;
  field::get<2> (value).raw = 2345;
  field::get<3> (value) = 513;
  field::get<4> (value) = -300;
  field::get<5> (value).raw = 125;
  field::get<6> (value) = 70000;
  field::get<7> (value) = -70000;
  field::get<8> (value) = 4000000000u;
  field::get<9> (value).raw = 3300;
  field::get<10> (value) = 12345678901234ull;
  field::get<11> (value) = -9876543210ll;
  field::get<12> (value) = 254.5f;
  field::get<13> (value) = 0.0625;
  field::get<14> (value) = -1234;
  field::get<15> (value).raw = -250;
  field::get<16> (value) = 0xBEEF;
  field::get<17> (value).raw = 25;
  field::get<18> (value).raw = 123456;
  field::get<19> (value) = 'Q';
  field::get<20> (value).assign (field::text_view ("abc"));
  field::get<21> (value) = 9;
  field::get<22> (value) = 5;
  field::get<23> (value).raw = 0x5234;
  field::get<0> (field::get<23> (value).parts) = 0x234;
  field::get<1> (field::get<23> (value).parts) = 5;
  field::get<24> (value) = gt::gauge_mode::run;
  field::get<25> (value) = gt::gauge_letter::start;
  for (std::size_t i = 0; i < 3; ++i)
    field::get<26> (value)[i] = static_cast<std::uint16_t> (i + 1);
  set_point (field::get<27> (value)[0], 1, 2);
  set_point (field::get<27> (value)[1], -3, 4);
  field::get<28> (value).push_back (7);
  field::get<28> (value).push_back (8);
  field::get<29> (value) = 2;
  field::get<30> (value).push_back (10);
  field::get<30> (value).push_back (-20);
  field::get<31> (value) = 2;
  field::get<32> (value).discriminant = 2;
  field::get<32> (value).cases.set<1> (99999u);
  field::get<33> (value).discriminant = 9;
  field::get<33> (value).cases.emplace<1> ().assign (field::text_view ("hi"));
  field::get<34> (value).discriminant = 1;
  field::get<34> (value).cases.set<0> (5);
  field::get<35> (value) = -3;
  field::get<36> (value) = 1;
  set_point (field::get<37> (value), -7, 1);
  field::get<38> (value) = 50;
  auto &tail = field::get<39> (value);
  tail.present = long_tail;
  if (!long_tail)
    return value;
  field::get<0> (tail.fields) = 500;
  auto &long_form = field::get<1> (tail.fields).emplace<1> ();
  field::get<0> (long_form) = 7;
  payload.assign (3, 0);
  payload[0] = 1;
  payload[1] = 0;
  payload[2] = 9;
  field::get<1> (long_form).key = 'L';
  field::get<1> (long_form).payload
      = field::bytes_view_t (payload.data (), payload.size ());
  return value;
}

// The key of READ_SINK as the layer's key reader wants it.
std::string
sink_line (index::protocol_index_t const &where, layout::bytes_t const &frame,
           index::session_context_t const &session)
{
  return where.describe (view_of (frame), index::frame_direction::rx,
                         &session);
}

// What is left of a line after the layer name: the rest is the same in
// every layout of the same rows.
std::string
after_name (std::string const &line)
{
  std::size_t const dot = line.find ('.');
  return dot == std::string::npos ? line : line.substr (dot + 1u);
}
} // namespace

TEST (LumexProtocolDescribeTest,
      GivenEveryKindOfField_WhenALargeReplyIsDescribed_ThenEachValueHasItsText)
{
  index::index_result_t const &where = index::protocol_index<gt::gauge> ();
  ASSERT_TRUE (where.has_value ());
  std::vector<std::uint8_t> payload;
  sink const value = make_sink (true, payload);
  index::session_context_t session;
  session.set ("gauge_kind", 1);
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t frame = bytes ({ 0, 0, 0, 3 });
  ASSERT_TRUE ((layout::encode_reply<gt::gauge, gt::gauge::READ_SINK_t> (
                    frame, value, &context))
                   .has_value ());
  EXPECT_EQ (
      sink_line (*where, frame, session),
      "RX gauge.READ_SINK (Read everything) u8=200 i8=-5 temperature=23.45 "
      "degC "
      "u16be=513 i16le=-300 i16be=12.5 u24le=70000 i24be=-70000 "
      "u32le=4000000000 voltage=3.3 mV u64be=12345678901234 "
      "i64le=-9876543210 wavelength=254.5 nm f64be=0.0625 sm32=-1234 "
      "sm16=-2.5 hex4=48879 hex2=2.5 dec=123.456 letter='Q' name=\"abc\" "
      "low_nibble=9 even=5 split={564 5} mode=run code='S' triple=[1, 2, 3] "
      "points=[{x=1 flags=2}, {x=-3 flags=4}] counted=[7, 8] count=2 "
      "computed=[10, -20] selector=2 by_field=case 2: 99999 by_lead=case 9: "
      "\"hi\" by_context=case 1: 5 signed_bits=-3 odd=1 point={x=-7 flags=1} "
      "percent=50 % tail={500 {code=7 inner={key='L' len=3}}}");
}

TEST (
    LumexProtocolDescribeTest,
    GivenTheSameValueInThreeLayouts_WhenDescribed_ThenTheTextAfterTheNameIsTheSame)
{
  index::index_result_t const &binary = index::protocol_index<gt::gauge> ();
  index::index_result_t const &hex = index::protocol_index<gt::gauge_hex> ();
  index::index_result_t const &text = index::protocol_index<gt::gauge_text> ();
  ASSERT_TRUE (binary.has_value ());
  ASSERT_TRUE (hex.has_value ());
  ASSERT_TRUE (text.has_value ());
  index::session_context_t session;
  session.set ("gauge_kind", 1);
  field::decode_context_t const context = session.decode_context ();
  // A tokenized message cannot carry the raw bytes of a nested message, so
  // the three layouts share the value without the tail.
  std::vector<std::uint8_t> payload;
  sink const value = make_sink (false, payload);
  layout::bytes_t binary_frame = bytes ({ 0, 0, 0, 3 });
  ASSERT_TRUE ((layout::encode_reply<gt::gauge, gt::gauge::READ_SINK_t> (
                    binary_frame, value, &context))
                   .has_value ());
  layout::bytes_t hex_frame = bytes_of ("00000003");
  ASSERT_TRUE ((layout::encode_reply<gt::gauge_hex, gt::gauge::READ_SINK_t> (
                    hex_frame, value, &context))
                   .has_value ());
  layout::bytes_t text_frame;
  layout::encode_result_t const text_result
      = layout::encode_reply<gt::gauge_text, gt::gauge::READ_SINK_t> (
          text_frame, value, &context);
  ASSERT_TRUE (text_result.has_value ()) << text_result.error ().message ();
  std::string const expected
      = after_name (sink_line (*binary, binary_frame, session));
  EXPECT_NE (expected.find ("temperature=23.45 degC"), std::string::npos);
  EXPECT_EQ (after_name (sink_line (*hex, hex_frame, session)), expected);
  EXPECT_EQ (after_name (sink_line (*text, text_frame, session)), expected);
}

TEST (
    LumexProtocolDescribeTest,
    GivenTheLongTailInBinaryAndHex_WhenDescribed_ThenTheTextAfterTheNameIsTheSame)
{
  index::index_result_t const &binary = index::protocol_index<gt::gauge> ();
  index::index_result_t const &hex = index::protocol_index<gt::gauge_hex> ();
  ASSERT_TRUE (binary.has_value ());
  ASSERT_TRUE (hex.has_value ());
  index::session_context_t session;
  session.set ("gauge_kind", 1);
  field::decode_context_t const context = session.decode_context ();
  std::vector<std::uint8_t> payload;
  sink const value = make_sink (true, payload);
  layout::bytes_t binary_frame = bytes ({ 0, 0, 0, 3 });
  ASSERT_TRUE ((layout::encode_reply<gt::gauge, gt::gauge::READ_SINK_t> (
                    binary_frame, value, &context))
                   .has_value ());
  layout::bytes_t hex_frame = bytes_of ("00000003");
  ASSERT_TRUE ((layout::encode_reply<gt::gauge_hex, gt::gauge::READ_SINK_t> (
                    hex_frame, value, &context))
                   .has_value ());
  EXPECT_EQ (after_name (sink_line (*hex, hex_frame, session)),
             after_name (sink_line (*binary, binary_frame, session)));
}

TEST (
    LumexProtocolDescribeTest,
    GivenATailThatTheMessageCarries_WhenDescribed_ThenTheNestedMessageIsSummarized)
{
  index::index_result_t const &where = index::protocol_index<gt::gauge> ();
  ASSERT_TRUE (where.has_value ());
  std::vector<std::uint8_t> payload;
  sink const value = make_sink (true, payload);
  index::session_context_t session;
  session.set ("gauge_kind", 1);
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t frame = bytes ({ 0, 0, 0, 3 });
  ASSERT_TRUE ((layout::encode_reply<gt::gauge, gt::gauge::READ_SINK_t> (
                    frame, value, &context))
                   .has_value ());
  std::string const line = sink_line (*where, frame, session);
  std::string const tail = " tail={500 {code=7 inner={key='L' len=3}}}";
  ASSERT_GE (line.size (), tail.size ());
  EXPECT_EQ (line.substr (line.size () - tail.size ()), tail);
}

TEST (LumexProtocolDescribeTest,
      GivenAMessageWithoutItsTail_WhenDescribed_ThenTheTailIsAbsent)
{
  index::index_result_t const &where = index::protocol_index<gt::gauge> ();
  ASSERT_TRUE (where.has_value ());
  std::vector<std::uint8_t> payload;
  sink const value = make_sink (false, payload);
  index::session_context_t session;
  session.set ("gauge_kind", 1);
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t frame = bytes ({ 0, 0, 0, 3 });
  ASSERT_TRUE ((layout::encode_reply<gt::gauge, gt::gauge::READ_SINK_t> (
                    frame, value, &context))
                   .has_value ());
  std::string const line = sink_line (*where, frame, session);
  std::string const tail = " tail=absent";
  ASSERT_GE (line.size (), tail.size ());
  EXPECT_EQ (line.substr (line.size () - tail.size ()), tail);
}

// The falsifier of the design: the describe line is built from the same
// table as the encoder, so a name or a scale changed in only one place (the
// field name list, the scale of the field) changes the line. The snapshot
// above fails with it; these two checks name the two ways it can happen.
namespace
{
LUMEX_PROTOCOL_LAYOUT (renamed_t, (u8_, u8_), ("mode", "grade"));
LUMEX_PROTOCOL_LAYOUT (original_t, (u8_, u8_), ("mode", "level"));
LUMEX_PROTOCOL_LAYOUT (rescaled_t,
                       (u16_le_<scale<std::ratio<1, 10>>, unit::rpm>),
                       ("speed"));
LUMEX_PROTOCOL_LAYOUT (original_speed_t,
                       (u16_le_<scale<std::ratio<1, 100>>, unit::rpm>),
                       ("speed"));

// clang-format off
#define VARIANT_ROWS(X, X_AS) \
  X (ROW_A, 1, "Row A", (), (), (renamed_t), ()) \
  X (ROW_B, 2, "Row B", (), (), (rescaled_t), ())
#define ORIGINAL_ROWS(X, X_AS) \
  X (ROW_A, 1, "Row A", (), (), (original_t), ()) \
  X (ROW_B, 2, "Row B", (), (), (original_speed_t), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (variant_a, tables::codec, tables::widget_header,
                       VARIANT_ROWS);
LUMEX_PROTOCOL_DEFINE (variant_b, tables::codec, tables::widget_header,
                       ORIGINAL_ROWS);
} // namespace

TEST (
    LumexProtocolDescribeTest,
    GivenTwoTablesThatDifferInOneFieldNameOrOneScale_WhenTheSameBytesAreDescribed_ThenTheLinesDiffer)
{
  index::index_result_t const &first = index::protocol_index<variant_a> ();
  index::index_result_t const &second = index::protocol_index<variant_b> ();
  ASSERT_TRUE (first.has_value ());
  ASSERT_TRUE (second.has_value ());
  layout::bytes_t const name_frame = bytes ({ 0x01, 0x01, 0x03 });
  EXPECT_EQ (after_name (first->describe (view_of (name_frame),
                                          index::frame_direction::rx)),
             "ROW_A (Row A) mode=1 grade=3");
  EXPECT_EQ (after_name (second->describe (view_of (name_frame),
                                           index::frame_direction::rx)),
             "ROW_A (Row A) mode=1 level=3");
  layout::bytes_t const scale_frame = bytes ({ 0x02, 0x7D, 0x00 });
  EXPECT_EQ (after_name (first->describe (view_of (scale_frame),
                                          index::frame_direction::rx)),
             "ROW_B (Row B) speed=12.5 rpm");
  EXPECT_EQ (after_name (second->describe (view_of (scale_frame),
                                           index::frame_direction::rx)),
             "ROW_B (Row B) speed=1.25 rpm");
}

TEST (
    LumexProtocolDescribeTest,
    GivenAReplyKindOfTheDialogue_WhenDescribed_ThenTheRequestIsDecodedAndTheReplyIsADump)
{
  index::index_result_t const &result
      = index::protocol_index<tables::dialogue::dialogue_protocol> ();
  ASSERT_TRUE (result.has_value ());
  layout::bytes_t const request = bytes ({ 0x01, 0x07 });
  EXPECT_EQ (result->describe (view_of (request), index::frame_direction::tx),
             "TX dialogue_protocol.START (Start) channel=7");
  layout::bytes_t const reply = bytes ({ 0x01, 0x07, 0x00 });
  EXPECT_EQ (result->describe (view_of (reply), index::frame_direction::rx),
             "RX dialogue_protocol.START (Start) len=3: 01 07 00");
}

namespace
{
index::protocol_index_t const &
notes_index ()
{
  index::index_result_t const &result
      = index::protocol_index<tables::notes> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}
} // namespace

TEST (LumexProtocolDescribeTest,
      GivenALongText_WhenDescribed_ThenItIsCutWithItsFullLength)
{
  layout::bytes_t frame (301, 'a');
  frame[0] = 0x50;
  std::string const line
      = notes_index ().describe (view_of (frame), index::frame_direction::tx);
  EXPECT_EQ (line, "TX notes.NOTE (Note) text=\"" + std::string (256, 'a')
                       + "\"... (300 chars)");
  index::describe_options_t options;
  options.text_limit = 3;
  EXPECT_EQ (notes_index ().describe (view_of (frame),
                                      index::frame_direction::tx, nullptr,
                                      options),
             "TX notes.NOTE (Note) text=\"aaa\"... (300 chars)");
  options.text_limit = 300;
  EXPECT_EQ (notes_index ().describe (view_of (frame),
                                      index::frame_direction::tx, nullptr,
                                      options),
             "TX notes.NOTE (Note) text=\"" + std::string (300, 'a') + "\"");
}

TEST (LumexProtocolDescribeTest,
      GivenAFrameOfAMillionBytes_WhenDescribed_ThenTheLineStaysShort)
{
  layout::bytes_t frame (1000000, 0x7E);
  frame[0] = 0x50;
  EXPECT_LT (notes_index ()
                 .describe (view_of (frame), index::frame_direction::tx)
                 .size (),
             400u);
  layout::bytes_t unknown (1000000, 0x7E);
  EXPECT_LT (notes_index ()
                 .describe (view_of (unknown), index::frame_direction::rx)
                 .size (),
             2000u);
}

TEST (LumexProtocolDescribeTest,
      GivenAQuoteAndABackslashInAText_WhenDescribed_ThenBothAreEscaped)
{
  layout::bytes_t const frame
      = bytes ({ 0x51, 'a', '"', '\\', 'b', 0, 0, 0, 0 });
  EXPECT_EQ (
      notes_index ().describe (view_of (frame), index::frame_direction::tx),
      "TX notes.LABEL (Label) text=\"a\\\"\\\\b\"");
}

TEST (LumexProtocolDescribeTest,
      GivenAScaleOfOneThird_WhenDescribed_ThenSixDecimalsAreWritten)
{
  layout::bytes_t const one = bytes ({ 0x52, 0x01, 0x00 });
  EXPECT_EQ (
      notes_index ().describe (view_of (one), index::frame_direction::tx),
      "TX notes.THIRD (Third) third=0.333333");
  layout::bytes_t const three = bytes ({ 0x52, 0x03, 0x00 });
  EXPECT_EQ (
      notes_index ().describe (view_of (three), index::frame_direction::tx),
      "TX notes.THIRD (Third) third=1");
}

TEST (
    LumexProtocolDescribeTest,
    GivenADumpLimitOfZero_WhenAnUnknownFrameIsDescribed_ThenThereIsNoDoubleSpace)
{
  index::describe_options_t options;
  options.dump_limit = 0;
  layout::bytes_t const frame = bytes ({ 0x99, 0x01 });
  EXPECT_EQ (widget_index ().describe (view_of (frame),
                                       index::frame_direction::tx, nullptr,
                                       options),
             "TX unknown key=0x99 len=2: ...");
}

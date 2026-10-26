// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device C (LumexProtocolDeviceC.hpp): every request with its process
// header and every reply on reference vectors, the parameters whose numbers
// overlap between the types, the write answered by a change report, the
// packed measurement with its 24-bit time, the memory blocks, and the
// accessory commands nested in a bus transaction.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_c/LumexProtocolDeviceC.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace registry = lumex::core::protocol::registry;
namespace error = lumex::core::protocol::error;
namespace c = lumex_tests::device_c;
using c::c_i2c;
using c::c_int;
using c::c_long;
using c::c_real;
using c::device_c;
using c::device_c_rx;
using c::kAddress;
using dev::bytes;
using dev::bytes_t;
using dev::join;
using dev::tail_of;
using lumex_tests::device_a::old_header;
using lumex_tests::device_a::request_frame;

bytes_t
display (std::uint8_t message)
{
  return old_header (kAddress, c::process::display, 0x00, message);
}

template <typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
reply_of (bytes_t const &frame)
{
  // A view into the frame itself: the values of a token or a nested
  // message point into it.
  return layout::decode_reply<device_c_rx, Row> (
      layout::bytes_view_t (frame.data () + 2, frame.size () - 2));
}
} // namespace

TEST (LumexProtocolDeviceCTest,
      GivenTheThreeParameterTypes_WhenIndexed_ThenTheirNumbersMayOverlap)
{
  static_assert (index::keys_unique<device_c>::value, "unique keys");
  EXPECT_EQ (c_int::entries ()
                 .data ()[static_cast<std::size_t> (c_int::filter)]
                 .key.first,
             c_real::entries ()
                 .data ()[static_cast<std::size_t> (c_real::excitation)]
                 .key.first);
  index::index_result_t const built
      = index::build_index<device_c, c_int, c_long, c_real, c::c_typed> ();
  ASSERT_TRUE (built.has_value ());
  EXPECT_EQ (built->registries ().size (), 4u);
  std::size_t first = 0;
  std::size_t second = 0;
  EXPECT_FALSE (
      registry::find_duplicate_key (c::c_typed::entries (), first, second));
  // The same number with two types: two keys.
  registry::registry_entry_t const *const typed
      = c::c_typed::entries ().data ();
  EXPECT_EQ (typed[0].key.second, typed[2].key.second);
  EXPECT_NE (typed[0].key.first, typed[2].key.first);
  EXPECT_EQ (dev::index_of<device_c> ().rows ().size (), 22u);
}

TEST (LumexProtocolDeviceCTest,
      GivenAParameterWrite_WhenAnswered_ThenTheAnswerIsAChangeReport)
{
  bytes_t const set = request_frame<device_c, device_c::param_set_fa_t> (
      display (0xFA /*@r04*/), 450, c_int::sensitivity);
  EXPECT_EQ (set, bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0xFA /*@r04*/, 0xC2,
                           0x01, 0xF4 /*@r04*/ }));
  EXPECT_TRUE (dev::finds<device_c::param_set_fa_t> (
      dev::index_of<device_c> (), set, 4));
  bytes_t const get = request_frame<device_c, device_c::param_get_f4_t> (
      display (0xF4 /*@r04*/), c_int::sensitivity);
  EXPECT_EQ (get, bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0xF4 /*@r04*/,
                           0xF4 /*@r04*/ }));
  bytes_t const change
      = bytes ({ kAddress, 0x35 /*@r04*/, 0xC2, 0x01, 0xF4 /*@r04*/ });
  index::protocol_index_t const &rx = dev::index_of<device_c_rx> ();
  EXPECT_TRUE (dev::finds<device_c::report_35_t> (rx, change, 2));
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_35_t>::type> const changed
      = reply_of<device_c::report_35_t> (change);
  ASSERT_TRUE (changed.has_value ());
  EXPECT_EQ (field::get<0> (*changed), 450);
  EXPECT_TRUE (field::get<1> (*changed) == c_int::sensitivity);
  EXPECT_EQ (rx.describe (dev::view_of (change), index::frame_direction::rx),
             "RX device_c_rx.report_35 (16-bit parameter changed) value=450 "
             "parameter=sensitivity");
  EXPECT_TRUE ((dev::request_round_trip<device_c, device_c::param_set_fa_t> (
      tail_of (set, 4))));
  // A number the 16-bit table does not have.
  EXPECT_FALSE ((reply_of<device_c::report_35_t> (bytes (
                     { kAddress, 0x35 /*@r04*/, 0x00, 0x00, 0xD9 /*@r04*/ })))
                    .has_value ());
}

TEST (LumexProtocolDeviceCTest,
      GivenTheLongAndFloatingParameters_WhenRoundTripped_ThenTheBytesAgree)
{
  bytes_t const get_long = request_frame<device_c, device_c::param_get_4b_t> (
      display (0x4B /*@r04*/), c_long::serial);
  EXPECT_EQ (tail_of (get_long, 3), bytes ({ 0x4B /*@r04*/, 0x72 /*@r04*/ }));
  bytes_t const serial = bytes (
      { kAddress, 0x38 /*@r04*/, 0x40, 0xE2, 0x01, 0x00, 0x72 /*@r04*/ });
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_38_t>::type> const number
      = reply_of<device_c::report_38_t> (serial);
  ASSERT_TRUE (number.has_value ());
  EXPECT_EQ (field::get<0> (*number), 123456);
  EXPECT_TRUE (field::get<1> (*number) == c_long::serial);

  bytes_t const set_real = request_frame<device_c, device_c::param_set_47_t> (
      display (0x47 /*@r04*/), 350.5f, c_real::target);
  EXPECT_EQ (tail_of (set_real, 4),
             bytes ({ 0x00, 0x40, 0xAF, 0x43, 0xD9 /*@r04*/ }));
  bytes_t const get_real = request_frame<device_c, device_c::param_get_44_t> (
      display (0x44 /*@r04*/), c_real::excitation);
  EXPECT_EQ (tail_of (get_real, 4), bytes ({ 0xF3 /*@r04*/ }));
  bytes_t const current = bytes (
      { kAddress, 0xE0 /*@r04*/, 0x00, 0x40, 0xAF, 0x43, 0xF3 /*@r04*/ });
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_e0_t>::type> const wavelength
      = reply_of<device_c::report_e0_t> (current);
  ASSERT_TRUE (wavelength.has_value ());
  EXPECT_FLOAT_EQ (field::get<0> (*wavelength), 350.5f);
  EXPECT_TRUE (field::get<1> (*wavelength) == c_real::excitation);
  EXPECT_TRUE ((dev::reply_round_trip<device_c_rx, device_c_rx::report_e0_t> (
      tail_of (current, 2))));
  EXPECT_TRUE ((dev::request_round_trip<device_c, device_c::param_set_67_t> (
      bytes ({ 0x40, 0xE2, 0x01, 0x00, 0x65 /*@r04*/ }))));
}

TEST (LumexProtocolDeviceCTest,
      GivenTheMeasurement_WhenDecoded_ThenTheTimeIsA24BitInteger)
{
  bytes_t const frame = bytes (
      { kAddress, 0xDD /*@r04*/, 0xE8, 0x03, 0xFE, 0xFF, 0xB8, 0x0B, 0x01,
        0x05, 0x01, 0x8F /*@r04*/, 0x64, 0x00, 0xC8, 0x00, 0x45, 0x23, 0x01 });
  EXPECT_EQ (frame.size () - 2u, 17u);
  layout::decode_result_t<
      layout::reply_value_of<device_c::stream_dd_t>::type> const measured
      = reply_of<device_c::stream_dd_t> (frame);
  ASSERT_TRUE (measured.has_value ());
  EXPECT_EQ (field::get<1> (*measured), -2);
  EXPECT_TRUE (field::get<6> (*measured) == c::measurement_kind::flash);
  EXPECT_EQ (field::get<9> (*measured), 0x012345u);
  EXPECT_EQ (dev::index_of<device_c_rx> ().describe (
                 dev::view_of (frame), index::frame_direction::rx),
             "RX device_c_rx.stream_dd (Measurement) fluorescence=1000 "
             "photometry=-2 reference=3000 one=1 delay=5 one_again=1 "
             "kind=flash excitation_step=100 step emission_step=200 step "
             "time=74565 ms");
  EXPECT_TRUE ((dev::reply_round_trip<device_c_rx, device_c_rx::stream_dd_t> (
      tail_of (frame, 2))));
  // One byte short of the time.
  EXPECT_FALSE ((reply_of<device_c::stream_dd_t> (
                     bytes_t (frame.begin (), frame.end () - 1)))
                    .has_value ());
}

TEST (LumexProtocolDeviceCTest,
      GivenTheSimpleRequests_WhenEncoded_ThenTheBytesAreTheReference)
{
  index::protocol_index_t const &tx = dev::index_of<device_c> ();
  bytes_t const reset = request_frame<device_c, device_c::reset_ad_t> (
      display (0xAD /*@r04*/));
  EXPECT_TRUE (dev::finds<device_c::reset_ad_t> (tx, reset, 4));
  // Key 0x1C released (the high bit).
  bytes_t const key
      = bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0x65 /*@r04*/, 0x9C });
  layout::decode_result_t<layout::request_of<device_c::set_65_t>::type> const
      press
      = layout::decode_request<device_c, device_c::set_65_t> (
          dev::view_of (tail_of (key, 4)));
  ASSERT_TRUE (press.has_value ());
  EXPECT_EQ (field::get<0> (field::get<0> (*press).parts), 0x1Cu);
  EXPECT_TRUE (field::get<1> (field::get<0> (*press).parts));
  EXPECT_TRUE (dev::finds<device_c::set_65_t> (tx, key, 4));
  // Key 0x41 released: the key code takes all seven low bits.
  layout::decode_result_t<layout::request_of<device_c::set_65_t>::type> const
      high
      = layout::decode_request<device_c, device_c::set_65_t> (
          dev::view_of (bytes ({ 0xC1 })));
  ASSERT_TRUE (high.has_value ());
  EXPECT_EQ (field::get<0> (field::get<0> (*high).parts), 0x41u);
  EXPECT_TRUE (field::get<1> (field::get<0> (*high).parts));

  EXPECT_EQ (
      (request_frame<device_c, device_c::start_83_t> (display (0x83 /*@r04*/),
                                                      0, 1)),
      bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0x83 /*@r04*/, 0x00, 0x01 }));
  EXPECT_EQ ((request_frame<device_c, device_c::stop_cc_t> (
                 display (0xCC /*@r04*/), 0)),
             bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0xCC /*@r04*/, 0x00 }));
  EXPECT_TRUE (dev::finds<device_c::stop_0f_t> (
      tx,
      request_frame<device_c, device_c::stop_0f_t> (display (0x0F /*@r04*/)),
      4));
  EXPECT_EQ ((request_frame<device_c, device_c::stream_af_t> (
                 display (0xAF /*@r04*/), 0xDD /*@r04*/, 1)),
             bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0xAF /*@r04*/,
                      0xDD /*@r04*/, 0x01 }));
  bytes_t const stopped = bytes ({ kAddress, 0x87 /*@r04*/, 0x2C, 0x01 });
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_87_t>::type> const steps
      = reply_of<device_c::report_87_t> (stopped);
  ASSERT_TRUE (steps.has_value ());
  EXPECT_EQ (*steps, 300);
}

TEST (LumexProtocolDeviceCTest,
      GivenAMemoryBlock_WhenWrittenAndRead_ThenTheSizeCountsTheData)
{
  bytes_t const write
      = bytes ({ kAddress, 0xFA /*@r04*/, 0x00, 0x8D /*@r04*/, 0x04, 0x00,
                 0x10, 0x00, 0x11, 0x22, 0x33, 0x44 });
  EXPECT_TRUE (
      dev::finds<device_c::write_8d_t> (dev::index_of<device_c> (), write, 4));
  EXPECT_TRUE ((dev::request_round_trip<device_c, device_c::write_8d_t> (
      tail_of (write, 4))));
  bytes_t const read = request_frame<device_c, device_c::query_c9_t> (
      old_header (kAddress, c::process::memory, 0x00, 0xC9 /*@r04*/), 4, 0,
      0x10);
  EXPECT_EQ (read, bytes ({ kAddress, 0xFA /*@r04*/, 0x00, 0xC9 /*@r04*/, 0x04,
                            0x00, 0x10, 0x00 }));
  bytes_t const block = bytes ({ kAddress, 0xBC /*@r04*/, 0x04, 0x00, 0x10,
                                 0x00, 0x11, 0x22, 0x33, 0x44 });
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_bc_t>::type> const data
      = reply_of<device_c::report_bc_t> (block);
  ASSERT_TRUE (data.has_value ());
  EXPECT_EQ (field::get<3> (*data).count, 4u);
  EXPECT_EQ (field::get<3> (*data)[3], 0x44u);
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_bc_t>::type> const short_block
      = reply_of<device_c::report_bc_t> (
          bytes_t (block.begin (), block.end () - 1));
  ASSERT_FALSE (short_block.has_value ());
  EXPECT_TRUE (short_block.error () == error::protocol_errc::truncated);
}

TEST (LumexProtocolDeviceCTest,
      GivenAnAccessoryCommand_WhenSentThroughTheBus_ThenItIsNestedInTheMessage)
{
  bytes_t inner;
  ASSERT_TRUE ((layout::encode_request<c_i2c, c_i2c::command_b_t> (
                    inner, 400.0, 0x19 /*@r26*/))
                   .has_value ());
  field::nested_value<char> command;
  command.key = 'Z' /*@r11*/;
  command.payload = dev::view_of (inner);
  bytes_t const frame = request_frame<device_c, device_c::write_8e_t> (
      old_header (kAddress, c::process::bus, 0x00, 0x8E /*@r04*/), 4, 0, 20,
      0x18 /*@r26*/, 3, command);
  EXPECT_EQ (frame, bytes ({ kAddress, 0x27 /*@r04*/, 0x00, 0x8E /*@r04*/,
                             0x04, 0x00, 0x14, 0x18 /*@r26*/, 0x03,
                             'Z' /*@r11*/, 0xA0, 0x0F, 0x19 /*@r26*/ }));
  // The nested message is a view into the buffer: keep it alive.
  bytes_t const fields = tail_of (frame, 4);
  layout::decode_result_t<layout::request_of<device_c::write_8e_t>::type> const
      transaction
      = layout::decode_request<device_c, device_c::write_8e_t> (
          dev::view_of (fields));
  ASSERT_TRUE (transaction.has_value ());
  layout::decode_result_t<layout::request_of<c_i2c::command_b_t>::type> const
      value_b
      = layout::decode_nested<c_i2c, c_i2c::command_b_t> (
          field::get<5> (*transaction));
  ASSERT_TRUE (value_b.has_value ());
  EXPECT_DOUBLE_EQ (field::get<0> (*value_b).value (), 400.0);
  EXPECT_EQ (field::get<1> (*value_b), 0x19u /*@r26*/);
  EXPECT_TRUE ((layout::decode_nested<c_i2c, c_i2c::command_a_t> (
                    field::get<5> (*transaction)))
                   .error ()
               == error::protocol_errc::unknown_frame);

  bytes_t const frame_a = bytes (
      { 0x03, 0x05, 0x02, 0x18 /*@r26*/, 0x01, 'C' /*@r11*/, 0x19 /*@r26*/ });
  EXPECT_TRUE (
      (dev::request_round_trip<device_c, device_c::write_8e_t> (frame_a)));
  // The read address of the accessory is its address with the low bit set.
  EXPECT_EQ (frame_a[6], frame_a[3] | 1);

  bytes_t const result
      = bytes ({ kAddress, 0x7B /*@r04*/, 0x80, 0x00, 0x02, 0x18 /*@r26*/,
                 0x01, 'C' /*@r11*/, 0x10, 0x20, 0x30 });
  layout::decode_result_t<
      layout::reply_value_of<device_c::report_7b_t>::type> const answer
      = reply_of<device_c::report_7b_t> (result);
  ASSERT_TRUE (answer.has_value ());
  EXPECT_TRUE (field::get<1> (field::get<0> (*answer).parts));
  EXPECT_FALSE (field::get<1> (field::get<1> (*answer).parts));
  EXPECT_EQ (field::get<3> (*answer), 0x18u /*@r26*/);
  EXPECT_EQ (field::get<5> (*answer), 'C' /*@r11*/);
  EXPECT_EQ (field::get<6> (*answer).size (), 3u);
  EXPECT_TRUE ((dev::reply_round_trip<device_c_rx, device_c_rx::report_7b_t> (
      tail_of (result, 2))));
}

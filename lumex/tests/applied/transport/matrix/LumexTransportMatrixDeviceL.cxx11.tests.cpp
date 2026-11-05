// The transport matrix over device L: layer a l_requests (Modbus RTU with the
// options of the table); layer b l_read, the read path of the table: its
// request of registers with the state block of l_replies as the reply, in
// one layer (the table keeps requests and replies in two layers, so that no
// client of l_requests reads a reply), keyed by the function of both
// directions, by order, with a window for late replies and no retries for a
// row that names none. Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_l/LumexProtocolDeviceL.hpp"

namespace lumex_tests
{
namespace device_l
{
/// The header of l_read: the requests' (the address, then the function) and
/// a window for late replies.
struct read_header : request_header
{
  using lumex_vocabulary
      = lumex::applied::transport::protocol::dialogue::vocabulary_t;

  template <typename P>
  using dialogue_options
      = lumex::applied::transport::protocol::dialogue::options<
          lumex::applied::transport::protocol::dialogue::late_reply_window_ms<
              500>>;
};

/// The reply of l_read: the byte count, then the state block of
/// report_032e (state_t).
LUMEX_PROTOCOL_LAYOUT (read_reply_t,
                       (u8_, u16_be_, u16_be_, u16_be_, array_<u16_be_, 4>,
                        u16_be_, u16_be_, u16_be_, u16_be_, u16_be_, u16_be_,
                        u16_be_, u16_be_, array_<u16_be_, 4>, u32_be_,
                        u32_be_),
                       ("bytes", "work", "process", "error", "unused", "io",
                        "valve_1", "valve_2", "unused_2", "tray_state",
                        "tray_target", "tray_current", "tray_ready",
                        "solvents", "switches_1", "switches_2"));

// clang-format off
#define MATRIX_L_READ_ROWS(X, X_AS) \
  X (query_03, 0x03, "Read registers", \
     (registry_key_<l_registers, u16_be_>, u16_be_), ("first", "count"), \
     (read_reply_t), \
     (idempotent, timeout (1000), correlate (correlation::by_order)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (l_read, codec, read_header, MATRIX_L_READ_ROWS);
} // namespace device_l
} // namespace lumex_tests

namespace lumex_matrix_test
{
/// The wire of device L: Modbus RTU, the device side reading requests.
struct l_wire_t : wire_base_t
{
  using codec_type = fr::modbus_rtu::modbus_rtu_codec;

  static codec_type
  host ()
  {
    return codec_type (lumex_tests::device_l::codec_options (false));
  }

  static codec_type
  device ()
  {
    return codec_type (lumex_tests::device_l::codec_options (true));
  }

  /// A reply that announces 255 bytes of registers: a frame of 260 bytes,
  /// above the 256 of Modbus RTU (a block of one byte value reads as short
  /// frames with a wrong check, never as one above the limit).
  static bytes_t
  oversized_block ()
  {
    fr::modbus_rtu::modbus_rtu_options_t options
        = lumex_tests::device_l::codec_options (false);
    options.max_frame_size = 1024;
    bytes_t payload (3 + 255, 0x00);
    payload[0] = lumex_tests::device_l::kSlave;
    payload[1] = 0x03;
    payload[2] = 0xFF;
    bytes_t out;
    codec_type (options).encode (
        fr::codec::bytes_view_t (payload.data (), payload.size ()), out);
    return out;
  }

  static LUMEX_CONSTEXPR bool
  oversized_reads_too_large () LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <> struct wire_of<lumex_tests::device_l::l_requests> : l_wire_t
{
};

template <> struct wire_of<lumex_tests::device_l::l_read> : l_wire_t
{
  static LUMEX_CONSTEXPR std::size_t
  default_retries () LUMEX_NOEXCEPT
  {
    return 0;
  }

  /// 51 bytes on the wire at 25 ms a byte: after the timeout of 1000 ms,
  /// inside the window of 500 ms.
  static LUMEX_CONSTEXPR std::int64_t
  late_trickle_ms () LUMEX_NOEXCEPT
  {
    return 25;
  }

  /// The byte count of the reply is that of the state block.
  template <typename Device>
  static void
  prepare_device (Device &device)
  {
    using row = lumex_tests::device_l::l_read::query_03_t;
    typename Device::template reply_of<row> reply;
    field::get<0> (reply) = 0x2E;
    device.template set_reply<row> (reply);
  }
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::device_l;

std::vector<mx::cell_t> const cells
    = mx::join (mx::cells_of<dev::l_requests> ("device_l", "a"),
                mx::cells_of<dev::l_read> ("device_l", "b"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

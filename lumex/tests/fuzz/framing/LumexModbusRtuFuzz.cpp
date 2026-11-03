// libFuzzer target of lumex/core/framing/modbus_rtu (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: the response and
// the request table, the CRC in either byte order, an address filter, a
// small and the default limit. The stream as a payload is not checked (a
// frame needs a known function code); the frames the decoder delivers are.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/modbus_rtu/LumexModbusRtuCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;
namespace mb = lumex::core::framing::modbus_rtu;

struct modbus_traits_t : default_traits_t
{
  using codec_t = mb::modbus_rtu_codec;

  static unsigned
  variants ()
  {
    return 16;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return (variant & 8U) != 0 ? 16 : 256;
  }
  static codec_t
  make (unsigned variant)
  {
    mb::modbus_rtu_options_t options;
    if ((variant & 1U) != 0)
      options.functions = mb::modbus_rtu_requests ();
    options.crc_order = (variant & 2U) != 0 ? codec::byte_order::big
                                            : codec::byte_order::little;
    options.filter_address = (variant & 4U) != 0;
    options.address = 0x11;
    options.max_frame_size = frame_limit (variant);
    return codec_t (options);
  }
  // With the address filter on, only frames for the address come out.
  static void
  check_frame (unsigned variant, bytes_t const &frame)
  {
    if ((variant & 4U) != 0)
      LUMEX_FUZZ_REQUIRE (!frame.empty () && frame[0] == 0x11);
  }
  static bool
  payload_ok (unsigned, bytes_t const &)
  {
    return false;
  }
  static bool
  grows ()
  {
    return false;
  }
  static bool
  canonical ()
  {
    return true;
  }
  static std::vector<bytes_t>
  payloads ()
  {
    std::vector<bytes_t> result;
    std::uint8_t const counted[]
        = { 0x11, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78 };
    std::uint8_t const fixed[] = { 0x11, 0x06, 0x00, 0x01, 0x00, 0x02 };
    std::uint8_t const exception[] = { 0x11, 0x83, 0x02 };
    std::uint8_t const request[]
        = { 0x11, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04, 0x00, 0x0A, 0x01, 0x02 };
    result.push_back (bytes_t (counted, counted + sizeof counted));
    result.push_back (bytes_t (fixed, fixed + sizeof fixed));
    result.push_back (bytes_t (exception, exception + sizeof exception));
    result.push_back (bytes_t (request, request + sizeof request));
    return result;
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (modbus_traits_t)

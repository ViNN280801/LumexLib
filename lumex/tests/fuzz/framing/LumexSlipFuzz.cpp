// libFuzzer target of lumex/core/framing/slip (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: strict and lenient
// escape handling, a small and the default limit of the frame.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/slip/LumexSlipCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;

struct slip_traits_t : default_traits_t
{
  using codec_t = lumex::core::framing::slip::slip_codec;

  static unsigned
  variants ()
  {
    return 4;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return (variant & 2U) != 0 ? 16 : 4096;
  }
  static codec_t
  make (unsigned variant)
  {
    lumex::core::framing::slip::slip_options_t options;
    options.lenient_escape = (variant & 1U) != 0;
    options.max_frame_size = frame_limit (variant);
    return codec_t (options);
  }
  // A bad escape (ESC and a byte that is neither ESC_END nor ESC_ESC) in the
  // middle of a frame: strict SLIP drops the frame and reports bad_escape.
  static void
  extra (unsigned variant, bytes_t const &stream)
  {
    if ((variant & 1U) != 0)
      return;
    std::uint8_t bad = stream.empty () ? 0x01 : stream[0];
    if (bad == 0xDC || bad == 0xDD)
      bad = 0x01;
    bytes_t const wire = { 0x41, 0xDB, bad, 0xC0 };
    codec_t decoder = make (variant);
    events_t events;
    recorder_t sink = { &events };
    decoder.decode (codec::bytes_view_t (wire.data (), wire.size ()), sink);
    LUMEX_FUZZ_REQUIRE (
        events.size () == 1
        && events[0].error
               == make_error_code (
                   lumex::core::framing::codec::framing_errc::bad_escape));
  }
  static bool
  payload_ok (unsigned, bytes_t const &payload)
  {
    // An empty frame is skipped by the decoder.
    return !payload.empty ();
  }
  static bool
  grows ()
  {
    return false;
  }
  static bool
  canonical ()
  {
    return false;
  }
  static std::vector<bytes_t>
  payloads ()
  {
    return common_payloads ();
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (slip_traits_t)

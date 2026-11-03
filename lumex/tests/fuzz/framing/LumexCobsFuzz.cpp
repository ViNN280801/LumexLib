// libFuzzer target of lumex/core/framing/cobs (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: the delimiter 0x00
// and 0x7E, a small limit of the frame and 300.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/cobs/LumexCobsCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;

struct cobs_traits_t
{
  using codec_t = lumex::core::framing::cobs::cobs_codec;

  static unsigned
  variants ()
  {
    return 4;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return (variant & 2U) != 0 ? 16 : 300;
  }
  static codec_t
  make (unsigned variant)
  {
    lumex::core::framing::cobs::cobs_options_t options;
    options.delimiter = (variant & 1U) != 0 ? 0x7E : 0x00;
    options.max_frame_size = frame_limit (variant);
    return codec_t (options);
  }
  static bool
  payload_ok (unsigned, bytes_t const &)
  {
    return true;
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
    std::vector<bytes_t> result = common_payloads ();
    result.push_back (bytes_t (254, 0x11));
    result.push_back (bytes_t (255, 0x11));
    // Zeros: one code byte each, so that the encoded frame is longer than
    // the decoded limit allows by the room the buffer keeps for the codes.
    result.push_back (bytes_t (17, 0x00));
    result.push_back (bytes_t (301, 0x00));
    return result;
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (cobs_traits_t)

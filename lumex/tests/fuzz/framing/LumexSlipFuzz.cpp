// libFuzzer target of lumex/core/framing/slip (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: strict and lenient
// escape handling, a small and the default limit of the frame.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/slip/LumexSlipCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;

struct slip_traits_t
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

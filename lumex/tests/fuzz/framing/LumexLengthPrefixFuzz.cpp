// libFuzzer target of lumex/core/framing/length (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: the width of the
// prefix (1, 2, 4), its byte order, whether it counts itself, a small and
// the default limit of the frame.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/length/LumexLengthPrefixCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;

struct length_traits_t : default_traits_t
{
  using codec_t = lumex::core::framing::length::length_prefix_codec;

  static unsigned
  variants ()
  {
    return 24;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return variant >= 12U ? 16 : 300;
  }
  static codec_t
  make (unsigned variant)
  {
    static std::size_t const widths[] = { 1, 2, 4 };
    lumex::core::framing::length::length_prefix_options_t options;
    options.width = widths[(variant / 4U) % 3U];
    options.order = (variant & 1U) != 0 ? codec::byte_order::little
                                        : codec::byte_order::big;
    options.length_includes_prefix = (variant & 2U) != 0;
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
    return true;
  }
  static std::vector<bytes_t>
  payloads ()
  {
    return common_payloads ();
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (length_traits_t)

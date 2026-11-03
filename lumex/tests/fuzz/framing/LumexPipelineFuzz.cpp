// libFuzzer target of lumex/core/framing/stage (see
// LumexFramingFuzzSupport.hpp for the checks). Three pipelines: text lines
// (delimiter codec, a prefix character, an ASCII hex XOR byte, hex digits),
// the same with the XOR byte optional, and binary frames (length prefix, a
// prefix character, a CRC-16 and a word swap). The variant picks the
// pipeline and the limit of the frame.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/delimiter/LumexDelimiterCodec.hpp"
#include "lumex/core/framing/length/LumexLengthPrefixCodec.hpp"
#include "lumex/core/framing/stage/LumexFramingPipeline.hpp"
#include "lumex/core/framing/stage/LumexFramingStages.hpp"

namespace
{
using namespace lumex_framing_fuzz;
namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;

using strict_t
    = st::pipeline<fr::delimiter::delimiter_codec, st::prefix_char_stage<'%'>,
                   st::checksum_stage<st::xor8, st::ascii_hex_render>,
                   st::ascii_hex_stage>;
using optional_t = st::pipeline<
    fr::delimiter::delimiter_codec, st::prefix_char_stage<'%'>,
    st::checksum_stage<st::xor8, st::ascii_hex_render, st::checksum_optional>,
    st::ascii_hex_stage>;
using binary_t = st::pipeline<
    fr::length::length_prefix_codec, st::prefix_char_stage<0x7E>,
    st::checksum_stage<
        st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
                codec::byte_order::little>>,
    st::word_reverse_stage<2>>;

struct pipeline_traits_t
{
  using codec_t = select_t<strict_t, optional_t, binary_t>;

  static unsigned
  variants ()
  {
    return 6;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return (variant / 3U) != 0 ? 16 : 600;
  }
  static codec_t
  make (unsigned variant)
  {
    std::size_t const limit = frame_limit (variant);
    fr::delimiter::delimiter_options_t text;
    text.max_frame_size = limit;
    fr::length::length_prefix_options_t binary;
    binary.width = 2;
    binary.max_frame_size = limit;
    return codec_t (variant, strict_t (text), optional_t (text),
                    binary_t (binary));
  }
  static bool
  payload_ok (unsigned, bytes_t const &)
  {
    return true;
  }
  static bool
  grows ()
  {
    return true;
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
    result.push_back (bytes_t (2, 0x12));
    result.push_back (bytes_t (4, 0x34));
    return result;
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (pipeline_traits_t)

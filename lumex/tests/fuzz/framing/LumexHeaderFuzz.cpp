// libFuzzer target of lumex/core/framing/header (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: no check value, a
// XOR byte, and a CRC-32 read as 4-byte words; a one-byte and a two-byte
// preamble with a 1, 2 or 4 byte length field of either byte order that
// counts the data or the whole frame; a small and the default limit.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/header/LumexHeaderCodec.hpp"
#include "lumex/core/framing/stage/LumexFramingStages.hpp"

namespace
{
using namespace lumex_framing_fuzz;
namespace hd = lumex::core::framing::header;
namespace st = lumex::core::framing::stage;

using plain_t = hd::header_codec<hd::no_check>;
using xor_t = hd::header_codec<st::xor8>;
using crc_t = hd::header_codec<
    st::word_reversed<st::crc<lumex::core::crc::parametric::crc32_mpeg2_spec_t,
                              codec::byte_order::little>,
                      4>>;

// The variant is [width of the length field][limit][counts][order][preamble]
// above the kind of the check value (variant % 3).
hd::header_options_t
options_of (unsigned variant)
{
  static std::size_t const widths[] = { 1, 2, 4 };
  unsigned const v = variant / 3U;
  hd::header_options_t options;
  std::size_t const width = widths[(v / 16U) % 3U];
  options.tx_preamble = bytes_t ((v & 1U) != 0 ? 2 : 1, 0xA5);
  if ((v & 1U) != 0)
    options.tx_preamble[1] = 0x5A;
  options.rx_preamble = options.tx_preamble;
  // A one-byte field before the length, then the length, then two bytes.
  options.length_offset = options.tx_preamble.size () + 1;
  options.header_size = options.length_offset + width + 2;
  options.length_width = width;
  options.length_order
      = (v & 2U) != 0 ? codec::byte_order::big : codec::byte_order::little;
  options.check_from = (v & 2U) != 0 ? 1 : 0;
  options.counts
      = (v & 4U) != 0 ? hd::length_counts::frame : hd::length_counts::data;
  options.max_frame_size = (v & 8U) != 0 ? 16 : 300;
  return options;
}

struct header_traits_t : default_traits_t
{
  using codec_t = select_t<plain_t, xor_t, crc_t>;

  static unsigned
  variants ()
  {
    return 144;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    hd::header_options_t const options = options_of (variant);
    return options.max_frame_size + options.header_size;
  }
  static codec_t
  make (unsigned variant)
  {
    hd::header_options_t const options = options_of (variant);
    return codec_t (variant, plain_t (options), xor_t (options),
                    crc_t (options));
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

LUMEX_FRAMING_FUZZ_TARGET (header_traits_t)

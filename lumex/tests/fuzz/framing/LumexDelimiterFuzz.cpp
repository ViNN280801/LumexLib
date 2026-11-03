// libFuzzer target of lumex/core/framing/delimiter (see
// LumexFramingFuzzSupport.hpp for the checks). Variants: a one-byte
// delimiter (CR), a two-byte one (CR LF) and a repeating one, each with a
// small and the default limit of the frame. A payload that holds the
// delimiter is not carried by this codec and is left out of the payload
// check.

#include "LumexFramingFuzzSupport.hpp"
#include "lumex/core/framing/delimiter/LumexDelimiterCodec.hpp"

namespace
{
using namespace lumex_framing_fuzz;

bytes_t
delimiter_of (unsigned variant)
{
  switch ((variant >> 1U) % 3U)
    {
    case 0:
      return bytes_t (1, '\r');
    case 1:
      {
        std::uint8_t const crlf[] = { '\r', '\n' };
        return bytes_t (crlf, crlf + sizeof crlf);
      }
    default:
      return bytes_t (3, 'a');
    }
}

struct delimiter_traits_t : default_traits_t
{
  using codec_t = lumex::core::framing::delimiter::delimiter_codec;

  static unsigned
  variants ()
  {
    return 6;
  }
  static std::size_t
  frame_limit (unsigned variant)
  {
    return (variant & 1U) != 0 ? 16 : 4096;
  }
  static codec_t
  make (unsigned variant)
  {
    lumex::core::framing::delimiter::delimiter_options_t options;
    options.rx_delimiter = delimiter_of (variant);
    options.tx_suffix = options.rx_delimiter;
    options.max_frame_size = frame_limit (variant);
    return codec_t (options);
  }
  static bool
  payload_ok (unsigned variant, bytes_t const &payload)
  {
    // The first delimiter in the frame must be the suffix itself, and an
    // empty frame is skipped.
    if (payload.empty ())
      return false;
    bytes_t const delimiter = delimiter_of (variant);
    bytes_t frame (payload);
    frame.insert (frame.end (), delimiter.begin (), delimiter.end ());
    return std::search (frame.begin (), frame.end (), delimiter.begin (),
                        delimiter.end ())
           == frame.begin () + static_cast<std::ptrdiff_t> (payload.size ());
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
    result.push_back (bytes_t ());
    result.push_back (bytes_t (1, 0x41));
    std::uint8_t const text[] = { '%', '8', 'D', '0', '0', '\n', 'a', 'a' };
    result.push_back (bytes_t (text, text + sizeof text));
    result.push_back (bytes_t (200, 0x42));
    return result;
  }
};
} // namespace

LUMEX_FRAMING_FUZZ_TARGET (delimiter_traits_t)

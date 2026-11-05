// The stages of lumex::framing: small transformations of a whole frame that a
// pipeline puts around a codec that finds the frames. A stage has two
// functions, encode and decode, and works alone; the pipeline lists the
// stages from the outside in and applies them in that order when decoding and
// in the opposite order when encoding. Shown here: a prefix character, ASCII
// hex, three check values (XOR, sum and CRC-16, in binary and as text), a
// check value that may be missing, the reversal of 16-bit words and a pipeline
// of several stages for an invented line protocol.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/framing/LumexFraming"

namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;
using fr::codec::bytes_t;
using fr::codec::bytes_view_t;

namespace
{
std::string
hex (bytes_t const &bytes)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < bytes.size (); ++i)
    {
      out += digits[bytes[i] >> 4];
      out += digits[bytes[i] & 0x0F];
      out += ' ';
    }
  return out;
}

std::string
text_of (bytes_t const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}

// What a stage makes of a frame going out, and what it says of a frame
// coming in: the stage is used on its own, with no codec around it.
template <typename Stage>
bytes_t
encoded (Stage const &stage, bytes_t const &in)
{
  bytes_t out;
  stage.encode (bytes_view_t (in), out);
  return out;
}

template <typename Stage>
std::string
decoded (Stage const &stage, bytes_t const &in)
{
  bytes_t out;
  std::error_code const error = stage.decode (bytes_view_t (in), out);
  return error ? "refused: " + error.message ()
               : "frame " + hex (out) + "= '" + text_of (out) + "'";
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::framing: stages and pipelines ===\n\n";
  bool ok = true;
  bytes_t const data = { 'O', 'K', '1', '2' };

  std::cout << "--- 1. A prefix character, and the frame that lacks it\n";
  st::prefix_char_stage<'$'> prefix;
  bytes_t const with_prefix = encoded (prefix, data);
  std::cout << "  encode: '" << text_of (with_prefix)
            << "'\n  decode: " << decoded (prefix, with_prefix)
            << "\n  decode of a frame "
            << "without it: " << decoded (prefix, data) << '\n';

  std::cout << "\n--- 2. ASCII hex: every byte is two digits\n";
  st::ascii_hex_stage digits;
  bytes_t const as_digits = encoded (digits, data);
  std::cout << "  encode: '" << text_of (as_digits)
            << "'\n  decode: " << decoded (digits, as_digits)
            << "\n  an odd number of digits: "
            << decoded (digits, bytes_t ({ '4', '1', '4' })) << '\n';

  std::cout << "\n--- 3. Check values: XOR, sum, CRC-16\n";
  st::checksum_stage<st::xor8> const xor_stage;
  st::checksum_stage<st::sum8> const sum_stage;
  st::checksum_stage<st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
                             fr::codec::byte_order::little>> const crc_stage;
  std::cout << "  XOR      " << hex (encoded (xor_stage, data)) << '\n'
            << "  sum      " << hex (encoded (sum_stage, data)) << '\n'
            << "  CRC-16   " << hex (encoded (crc_stage, data))
            << "(low byte first)\n";
  bytes_t flipped = encoded (crc_stage, data);
  flipped[1] ^= 0x01;
  std::cout << "  CRC-16 of a damaged frame: " << decoded (crc_stage, flipped)
            << '\n';
  // The check value as text: the same value as two digits per byte.
  st::checksum_stage<st::xor8, st::ascii_hex_render> const as_text;
  std::cout << "  XOR as digits: '" << text_of (encoded (as_text, data))
            << "'\n";

  std::cout << "\n--- 4. A check value that the device sometimes leaves "
               "out\n";
  st::checksum_stage<st::xor8, st::ascii_hex_render,
                     st::checksum_optional> const optional;
  bytes_t const bare = { 'O', 'K', '1', '2' };
  std::cout << "  without it: " << decoded (optional, bare) << '\n'
            << "  with it:    " << decoded (optional, encoded (as_text, data))
            << '\n'
            << "  (an optional check also lets a damaged frame through: use "
               "it for such a device only)\n";

  std::cout << "\n--- 5. Words of 16 bits that arrive with the bytes "
               "swapped\n";
  st::word_reverse_stage<2> swap;
  bytes_t const words = { 0x12, 0x34, 0x56, 0x78 };
  std::cout << "  encode: " << hex (encoded (swap, words)) << '\n'
            << "  an odd number of bytes: "
            << decoded (swap, bytes_t ({ 1, 2, 3 })) << '\n';

  std::cout << "\n--- 6. A pipeline: line, '$', the digits, a check value\n";
  // A made-up line protocol: '$', the bytes as hex digits, the sum of the
  // characters as two hex digits, CR. The stages go from the outside in.
  using line_protocol = st::pipeline<
      fr::delimiter::delimiter_codec, st::prefix_char_stage<'$'>,
      st::checksum_stage<st::sum8, st::ascii_hex_render>, st::ascii_hex_stage>;
  line_protocol codec;
  bytes_t frame;
  codec.encode (bytes_view_t (data), frame);
  std::string line (frame.begin (), frame.end () - 1);
  std::cout << "  on wire: " << line << "<CR>\n";
  std::string stream = line + "\r$4F4B313200\r" + line + "\r";
  std::size_t good = 0;
  codec.decode (bytes_view_t (bytes_t (stream.begin (), stream.end ())),
                [&good] (bytes_view_t payload, std::error_code error)
                  {
                    if (error)
                      std::cout << "  dropped: " << error.message () << '\n';
                    else
                      {
                        ++good;
                        std::cout
                            << "  frame: '"
                            << std::string (payload.begin (), payload.end ())
                            << "'\n";
                      }
                  });
  ok = good == 2 && ok;
  return ok ? 0 : 1;
}

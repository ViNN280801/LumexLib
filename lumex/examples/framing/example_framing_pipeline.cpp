// A pipeline of lumex::framing for a made-up text protocol: '#', the payload
// as hex digits, the XOR of those digits as two hex digits, CR. The stages are
// listed from the outside in; encode applies them from the inside out.
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include "lumex/core/framing/LumexFraming"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;
using fr::codec::bytes_t;
using fr::codec::bytes_view_t;

using text_protocol
    = st::pipeline<fr::delimiter::delimiter_codec, st::prefix_char_stage<'#'>,
                   st::checksum_stage<st::xor8, st::ascii_hex_render>,
                   st::ascii_hex_stage>;

int
main ()
{
  std::cout << "=== lumex::framing pipeline ===\n\n";
  text_protocol codec;

  std::cout << "--- 1. Encode a command of two bytes ---\n";
  bytes_t const command = { 0x2A, 0x01 };
  bytes_t frame;
  fr::codec::encode_result_t const result
      = codec.encode (bytes_view_t (command), frame);
  if (!result.has_value ())
    {
      std::cout << "encode failed: " << result.error ().message () << '\n';
      return 1;
    }
  std::string text (frame.begin (), frame.end () - 1);
  std::cout << "on the wire: " << text << "<CR>\n";

  std::cout << "\n--- 2. Decode it back, and a reply with a bad check ---\n";
  std::string const stream = text + "\r#2A0100\r";
  codec.decode (bytes_view_t (bytes_t (stream.begin (), stream.end ())),
                [] (bytes_view_t payload, std::error_code error)
                  {
                    if (error)
                      {
                        std::cout << "dropped: " << error.message () << '\n';
                        return;
                      }
                    std::cout << "payload bytes: " << payload.size ()
                              << ", first 0x" << std::hex << int (payload[0])
                              << std::dec << '\n';
                  });
  return 0;
}

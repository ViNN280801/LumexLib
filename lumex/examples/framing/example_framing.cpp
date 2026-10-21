// Frame codecs of lumex::framing: SLIP for a binary link, delimited lines for
// a text device, and what the decoder reports when a frame is damaged.
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

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
using fr::codec::bytes_t;
using fr::codec::bytes_view_t;

namespace
{
std::string
hex (bytes_view_t bytes)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::uint8_t value : bytes)
    {
      out += digits[value >> 4];
      out += digits[value & 0x0F];
      out += ' ';
    }
  return out;
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::framing ===\n\n";

  std::cout << "--- 1. SLIP: END and ESC in the payload are escaped ---\n";
  fr::slip::slip_codec slip;
  bytes_t const payload = { 0x01, 0xC0, 0x02, 0xDB, 0x03 };
  bytes_t frame;
  slip.encode (bytes_view_t (payload), frame);
  std::cout << "frame:   " << hex (bytes_view_t (frame)) << '\n';
  slip.decode (bytes_view_t (frame), [] (bytes_view_t decoded)
                 { std::cout << "decoded: " << hex (decoded) << '\n'; });

  std::cout << "\n--- 2. Lines: CR LF ends a reply, LF ends a request ---\n";
  fr::delimiter::delimiter_options_t options;
  options.rx_delimiter = { '\r', '\n' };
  options.tx_suffix = { '\n' };
  fr::delimiter::delimiter_codec lines (options);
  std::string const request = "VERSION?";
  bytes_t out;
  lines.encode (bytes_view_t (bytes_t (request.begin (), request.end ())),
                out);
  std::cout << "request bytes: " << hex (bytes_view_t (out)) << '\n';
  std::string const replies = "VER 1.2\r\nOK\r\nPART";
  lines.decode (bytes_view_t (bytes_t (replies.begin (), replies.end ())),
                [] (bytes_view_t line)
                  {
                    std::cout << "reply: "
                              << std::string (line.begin (), line.end ())
                              << '\n';
                  });
  std::cout << "\"PART\" waits for its delimiter in the next chunk\n";

  std::cout << "\n--- 3. A damaged frame is reported to an error sink ---\n";
  fr::slip::slip_codec strict;
  bytes_t const damaged = { 0xC0, 0x01, 0xDB, 0x41, 0xC0, 0x07, 0xC0 };
  strict.decode (bytes_view_t (damaged),
                 [] (bytes_view_t decoded, std::error_code error)
                   {
                     if (error)
                       std::cout << "dropped: " << error.message () << '\n';
                     else
                       std::cout << "frame:   " << hex (decoded) << '\n';
                   });
  return 0;
}

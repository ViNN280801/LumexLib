// The binary codecs of lumex::framing besides SLIP, one made-up protocol each:
// COBS (frames that never hold a zero byte, so a zero ends a frame), a length
// prefix, a header with a preamble, a length and a CRC-16, and Modbus RTU. All
// follow one contract: encode appends a frame to a buffer, decode takes the
// bytes as they arrive, in chunks of any size, and calls the sink once per
// frame (and once per dropped frame, with the reason). The last section feeds
// each stream one byte at a time to show that the chunks do not matter.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/framing/LumexFraming"

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
  for (std::size_t i = 0; i < bytes.size (); ++i)
    {
      out += digits[bytes[i] >> 4];
      out += digits[bytes[i] & 0x0F];
      out += ' ';
    }
  return out.empty () ? std::string ("(empty) ") : out;
}

// What a decoder reported for a stream.
struct tally_t
{
  tally_t () : frames (0), dropped (0) {}

  std::size_t frames;
  std::size_t dropped;
};

// Feeds the stream in chunks of `chunk` bytes and reports every frame and the
// reason for every frame the decoder dropped; with `quiet` only the counts.
template <typename Codec>
tally_t
decode_and_print (Codec &codec, bytes_t const &stream, std::size_t chunk,
                  bool quiet = false)
{
  tally_t tally;
  for (std::size_t at = 0; at < stream.size (); at += chunk)
    {
      std::size_t const size
          = at + chunk <= stream.size () ? chunk : stream.size () - at;
      codec.decode (bytes_view_t (stream.data () + at, size),
                    [&tally, quiet] (bytes_view_t frame, std::error_code error)
                      {
                        if (error)
                          {
                            ++tally.dropped;
                            if (!quiet)
                              std::cout << "    dropped: " << error.message ()
                                        << '\n';
                          }
                        else
                          {
                            ++tally.frames;
                            if (!quiet)
                              std::cout << "    frame: " << hex (frame)
                                        << '\n';
                          }
                      });
    }
  return tally;
}

// The stream a codec makes of the payloads, and the same stream with a
// damaged byte.
template <typename Codec>
bytes_t
encode_all (Codec const &codec, std::vector<bytes_t> const &payloads)
{
  bytes_t stream;
  for (std::size_t i = 0; i < payloads.size (); ++i)
    codec.encode (bytes_view_t (payloads[i]), stream);
  return stream;
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::framing: COBS, length prefix, header, Modbus RTU "
               "===\n\n";
  bool ok = true;

  std::cout << "--- 1. COBS: no zero byte inside a frame, a zero ends it\n";
  fr::cobs::cobs_codec cobs;
  bytes_t const readings = { 0x11, 0x00, 0x00, 0x22, 0x33, 0x00 };
  bytes_t cobs_frame;
  cobs.encode (bytes_view_t (readings), cobs_frame);
  std::cout << "  payload  " << hex (bytes_view_t (readings)) << '\n'
            << "  on wire  " << hex (bytes_view_t (cobs_frame))
            << "(six bytes grew by two: the codes of the groups and the "
               "delimiter)\n";
  ok = decode_and_print (cobs, cobs_frame, cobs_frame.size ()).frames == 1
       && ok;
  // A code byte that points past the end of the frame is a damaged frame.
  bytes_t const cut = { 0x07, 0x11, 0x22, 0x00 };
  std::cout << "  a frame whose first code promises 6 bytes and has 2:\n";
  fr::cobs::cobs_codec cobs_cut;
  decode_and_print (cobs_cut, cut, cut.size ());

  std::cout << "\n--- 2. Length prefix: two bytes, big-endian, then the "
               "payload\n";
  fr::length::length_prefix_options_t prefix_options;
  prefix_options.width = 2;
  prefix_options.order = fr::codec::byte_order::big;
  prefix_options.max_frame_size = 8;
  fr::length::length_prefix_codec prefix (prefix_options);
  std::vector<bytes_t> payloads;
  payloads.push_back (bytes_t ({ 0xAA, 0xBB, 0xCC }));
  payloads.push_back (bytes_t ({ 0x01 }));
  bytes_t const stream = encode_all (prefix, payloads);
  std::cout << "  two frames on the wire: " << hex (bytes_view_t (stream))
            << '\n';
  ok = decode_and_print (prefix, stream, stream.size ()).frames == 2 && ok;
  // The limit protects against a length that was damaged to a huge value.
  bytes_t const huge = { 0xFF, 0xF0, 1, 2, 3 };
  std::cout << "  a length of 65520 with max_frame_size 8:\n";
  fr::length::length_prefix_codec prefix_huge (prefix_options);
  decode_and_print (prefix_huge, huge, huge.size ());
  bytes_t too_big (9, 0x55);
  fr::codec::encode_result_t const refused
      = prefix.encode (bytes_view_t (too_big), too_big);
  std::cout << "  encoding 9 bytes: " << refused.error ().message ()
            << " (the payload is never cut)\n";

  std::cout << "\n--- 3. Header: preamble, length, command, data, CRC-16\n";
  // [C3 3C][length: 2 bytes, little-endian][command][data...][CRC-16/MODBUS,
  // low byte first over the whole frame]. The payload of encode is the
  // header fields without the preamble and the length: the command, then the
  // data.
  fr::header::header_options_t header_options;
  header_options.tx_preamble = bytes_t ({ 0xC3, 0x3C });
  header_options.rx_preamble = bytes_t ({ 0xC3, 0x3C });
  header_options.header_size = 5;
  header_options.length_offset = 2;
  header_options.length_width = 2;
  header_options.max_frame_size = 32;
  using header_codec_t = fr::header::header_codec<
      fr::stage::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
                     fr::codec::byte_order::little>>;
  header_codec_t header (header_options);
  bytes_t const command = { 0x05, 0x10, 0x20, 0x30 };
  bytes_t header_frame;
  header.encode (bytes_view_t (command), header_frame);
  std::cout << "  command 05 with the data 10 20 30:\n  on wire  "
            << hex (bytes_view_t (header_frame)) << '\n';
  // Noise before the preamble is skipped without a word; a damaged byte is a
  // frame dropped with its reason.
  bytes_t noisy = { 0x00, 0x7F };
  noisy.insert (noisy.end (), header_frame.begin (), header_frame.end ());
  bytes_t damaged = header_frame;
  damaged[6] ^= 0x01;
  noisy.insert (noisy.end (), damaged.begin (), damaged.end ());
  noisy.insert (noisy.end (), header_frame.begin (), header_frame.end ());
  std::cout << "  two noise bytes, a good frame, a damaged one, a good one:\n";
  ok = decode_and_print (header, noisy, noisy.size ()).frames == 2 && ok;

  std::cout << "\n--- 4. Modbus RTU: the length comes from the function "
               "code\n";
  fr::modbus_rtu::modbus_rtu_codec modbus;
  // address 17, function 3 (read holding registers), 3 registers: the
  // reply holds a byte count (6) and 6 bytes. The codec adds the CRC.
  bytes_t const reply
      = { 0x11, 0x03, 0x06, 0xAE, 0x41, 0x56, 0x52, 0x43, 0x40 };
  bytes_t modbus_frame;
  modbus.encode (bytes_view_t (reply), modbus_frame);
  std::cout << "  reply on wire  " << hex (bytes_view_t (modbus_frame))
            << "(the CRC is the last two, low byte first)\n";
  ok = decode_and_print (modbus, modbus_frame, modbus_frame.size ()).frames
           == 1
       && ok;
  bytes_t two = modbus_frame;
  two[4] ^= 0xFF;
  two.insert (two.end (), modbus_frame.begin (), modbus_frame.end ());
  // After a damaged frame the decoder drops one byte and looks again, so the
  // reply that follows is found; the dropped bytes are reported one by one.
  fr::modbus_rtu::modbus_rtu_codec modbus_again;
  tally_t const found
      = decode_and_print (modbus_again, two, two.size (), true);
  std::cout << "  a damaged reply and a good one in one stream: "
            << found.frames << " frame, " << found.dropped
            << " drops reported on the way\n";
  ok = found.frames == 1 && ok;

  std::cout << "\n--- 5. One byte at a time gives the same frames\n";
  fr::cobs::cobs_codec cobs_again;
  fr::length::length_prefix_codec prefix_again (prefix_options);
  header_codec_t header_again (header_options);
  std::size_t const a = decode_and_print (cobs_again, cobs_frame, 1).frames;
  std::size_t const b = decode_and_print (prefix_again, stream, 1).frames;
  std::size_t const c = decode_and_print (header_again, noisy, 1).frames;
  std::cout << "  frames: COBS " << a << ", length prefix " << b << ", header "
            << c << '\n';
  ok = a == 1 && b == 2 && c == 2 && ok;
  return ok ? 0 : 1;
}

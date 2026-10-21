// Every codec, stage and pipeline of lumex/core/framing, instantiated with a
// frame-only sink and with an error sink, so that a strict consumer compiles
// every template of the module. Exit code 0 when every round trip worked.

#include <cstdio>
#include <system_error>
#include <vector>

#include "lumex/core/framing/LumexFraming"

namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;
using fr::codec::bytes_t;
using fr::codec::bytes_view_t;

namespace
{
struct error_sink
{
  int *frames;
  int *errors;

  void
  operator() (bytes_view_t, std::error_code error) const
  {
    if (error)
      ++*errors;
    else
      ++*frames;
  }
};

template <typename Codec>
bool
round_trip (Codec &codec, bytes_t const &payload)
{
  bytes_t frame;
  if (!codec.encode (bytes_view_t (payload.data (), payload.size ()), frame)
           .has_value ())
    return false;
  int frames = 0;
  int errors = 0;
  error_sink sink = { &frames, &errors };
  codec.decode (bytes_view_t (frame.data (), frame.size ()), sink);
  int plain = 0;
  codec.reset ();
  codec.decode (bytes_view_t (frame.data (), frame.size ()),
                [&plain] (bytes_view_t) { ++plain; });
  return frames == 1 && errors == 0 && plain == 1;
}
} // namespace

int
main ()
{
  bytes_t const payload = { 0x01, 0x03, 0x02, 0xAB, 0xCD };
  int failures = 0;

  fr::slip::slip_codec slip;
  failures += round_trip (slip, payload) ? 0 : 1;

  fr::cobs::cobs_codec cobs;
  failures += round_trip (cobs, payload) ? 0 : 1;

  fr::delimiter::delimiter_codec delimiter;
  failures += round_trip (delimiter, payload) ? 0 : 1;

  fr::length::length_prefix_codec length;
  failures += round_trip (length, payload) ? 0 : 1;

  fr::modbus_rtu::modbus_rtu_codec modbus;
  failures += round_trip (modbus, payload) ? 0 : 1;

  fr::header::header_options_t header_options;
  header_options.tx_preamble = bytes_t (2, 0x7E);
  header_options.rx_preamble = bytes_t (2, 0x7E);
  header_options.header_size = 4;
  header_options.length_offset = 2;
  fr::header::header_codec<st::word_reversed<
      st::crc<lumex::core::crc::parametric::crc32_mpeg2_spec_t>, 4>>
      header (header_options);
  failures += round_trip (header, payload) ? 0 : 1;
  fr::header::header_codec<> plain_header (header_options);
  failures += round_trip (plain_header, payload) ? 0 : 1;

  st::pipeline<fr::delimiter::delimiter_codec, st::prefix_char_stage<'%'>,
               st::checksum_stage<st::xor8, st::ascii_hex_render,
                                  st::checksum_optional>,
               st::ascii_hex_stage>
      hex_text;
  failures += round_trip (hex_text, payload) ? 0 : 1;

  st::pipeline<fr::length::length_prefix_codec, st::checksum_stage<st::sum8>,
               st::word_reverse_stage<2>,
               st::checksum_stage<
                   st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
                           fr::codec::byte_order::little>>>
      binary;
  bytes_t const even_payload = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
  failures += round_trip (binary, even_payload) ? 0 : 1;

  std::printf ("framing_warnings: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}

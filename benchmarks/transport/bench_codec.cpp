// bench_codec.cpp
// Throughput and latency of the frame codecs of lumex/core/framing: SLIP,
// COBS, delimiter, length prefix, header and Modbus RTU, for an encode, a
// decode of a whole frame in one chunk and a decode of the same frame fed in
// chunks of 64 bytes (the size of a USB serial read).
//
// Clock: none. No timer, thread or transport takes part; the numbers are the
// CPU cost of the codec alone, measured with steady_clock.
//
// Equal work: before timing, every case round-trips its payload (encode, then
// decode, and the frame must equal the payload) and the run stops when it
// does not. The loops keep the payload, the wire and the number of delivered
// frames alive with optimizer barriers (bench::keep_object,
// bench::keep_value), and after the timing a decode case stops the run unless
// the codec delivered exactly one frame per decode call and no error.
//
// Usage: LumexTransportBenchCodec [output.csv] [--quick] [--filter text]
//        [--list]
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <system_error>
#include <vector>

#include "bench_transport_common.hpp"
#include "lumex/core/framing/LumexFraming"

namespace
{
namespace fr = lumex::core::framing;
using fr::codec::bytes_t;
using fr::codec::bytes_view_t;

[[noreturn]] void
fail (std::string const &why)
{
  std::fprintf (stderr, "bench_codec: %s\n", why.c_str ());
  std::exit (1);
}

// Frames and bytes a decoder delivered.
struct counter_t
{
  std::uint64_t frames = 0;
  std::uint64_t bytes = 0;
  std::uint64_t errors = 0;

  void
  operator() (bytes_view_t frame)
  {
    ++frames;
    bytes += frame.size ();
  }

  void
  operator() (bytes_view_t frame, std::error_code error)
  {
    if (error)
      ++errors;
    else
      (*this) (frame);
  }
};

// The payload of `size` bytes without the bytes `avoid` (which a delimiter
// codec could not carry).
bytes_t
payload_of (std::size_t size, int avoid = -1)
{
  bytes_t payload = bench::pattern_bytes (size);
  for (std::size_t i = 0; i < payload.size (); ++i)
    if (static_cast<int> (payload[i]) == avoid)
      payload[i] = static_cast<std::uint8_t> (avoid + 1);
  return payload;
}

template <typename Codec>
void
check_round_trip (char const *name, Codec &codec, bytes_t const &payload,
                  bytes_t const &wire)
{
  bytes_t got;
  bool error = false;
  codec.decode (bytes_view_t (wire.data (), wire.size ()),
                [&] (bytes_view_t frame, std::error_code code)
                  {
                    if (code)
                      error = true;
                    else
                      got.assign (frame.begin (), frame.end ());
                  });
  if (error || got != payload)
    fail (std::string (name) + ": the round trip of "
          + std::to_string (payload.size ()) + " bytes differs");
}

// Runs the three cases of one codec at one payload size.
template <typename Codec>
void
run_codec (bench::reporter_t &report, bench::options_t const &options,
           char const *name, Codec &codec, bytes_t const &payload)
{
  bytes_t wire;
  if (!codec.encode (bytes_view_t (payload.data (), payload.size ()), wire))
    fail (std::string (name) + ": encode refused the payload");
  check_round_trip (name, codec, payload, wire);

  std::string const label
      = std::string (name) + "_" + std::to_string (payload.size ());
  double const megabytes = static_cast<double> (payload.size ()) / 1e6;

  std::string const encode_name = label + "_encode";
  if (report.wanted (encode_name))
    {
      bytes_t out;
      out.reserve (wire.size () + 16);
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  out.clear ();
                  bench::keep_object (payload);
                  codec.encode (
                      bytes_view_t (payload.data (), payload.size ()), out);
                  bench::keep_object (out);
                }
            });
      report.add (encode_name, "none", "ns/frame", timed.median, timed.minimum,
                  timed.passes, timed.iterations,
                  "MB/s="
                      + std::to_string (static_cast<long> (
                          megabytes / (timed.median * 1e-9))));
    }

  std::string const decode_name = label + "_decode";
  if (report.wanted (decode_name))
    {
      counter_t counted;
      std::uint64_t calls = 0;
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  bench::keep_object (wire);
                  codec.decode (bytes_view_t (wire.data (), wire.size ()),
                                counted);
                }
              calls += n;
              bench::keep_value (counted.frames);
            });
      if (counted.frames != calls || counted.errors != 0)
        fail (decode_name
              + ": the frames delivered differ from the decode "
                "calls, or an error was delivered");
      report.add (decode_name, "none", "ns/frame", timed.median, timed.minimum,
                  timed.passes, timed.iterations,
                  "MB/s="
                      + std::to_string (static_cast<long> (
                          megabytes / (timed.median * 1e-9))));
    }

  std::string const chunked_name = label + "_decode_chunks64";
  if (report.wanted (chunked_name))
    {
      counter_t counted;
      std::uint64_t calls = 0;
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                for (std::size_t at = 0; at < wire.size (); at += 64)
                  {
                    std::size_t const size
                        = std::min<std::size_t> (64, wire.size () - at);
                    bench::keep_object (wire);
                    codec.decode (bytes_view_t (wire.data () + at, size),
                                  counted);
                  }
              calls += n;
              bench::keep_value (counted.frames);
            });
      if (counted.frames != calls || counted.errors != 0)
        fail (chunked_name
              + ": the frames delivered differ from the frames "
                "fed, or an error was delivered");
      report.add (chunked_name, "none", "ns/frame", timed.median,
                  timed.minimum, timed.passes, timed.iterations,
                  "MB/s="
                      + std::to_string (static_cast<long> (
                          megabytes / (timed.median * 1e-9))));
    }
}

std::vector<std::size_t> const kSizes = { 16, 100, 2420 };
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("codec", options);

  for (std::size_t size : kSizes)
    {
      bytes_t const payload = payload_of (size);
      {
        fr::slip::slip_codec codec;
        run_codec (report, options, "slip", codec, payload);
      }
      {
        fr::cobs::cobs_codec codec;
        run_codec (report, options, "cobs", codec, payload);
      }
      {
        fr::delimiter::delimiter_codec codec;
        run_codec (report, options, "delimiter", codec,
                   payload_of (size, '\r'));
      }
      {
        fr::length::length_prefix_options_t prefix;
        prefix.width = 2;
        fr::length::length_prefix_codec codec (prefix);
        run_codec (report, options, "length_prefix", codec, payload);
      }
      {
        fr::header::header_options_t header;
        header.tx_preamble = bytes_t{ 0xA5, 0x5A };
        header.rx_preamble = header.tx_preamble;
        header.header_size = 4;
        header.length_offset = 2;
        header.length_width = 2;
        fr::header::header_codec<> codec (header);
        run_codec (report, options, "header", codec, payload);
      }
      if (size <= 100)
        {
          // address, function 3 (read reply), byte count, data.
          bytes_t modbus
              = bytes_t{ 0x11, 0x03, static_cast<std::uint8_t> (size - 3) };
          bytes_t const data = payload_of (size - 3);
          modbus.insert (modbus.end (), data.begin (), data.end ());
          fr::modbus_rtu::modbus_rtu_codec codec;
          run_codec (report, options, "modbus_rtu", codec, modbus);
        }
    }

  if (!report.write (options.output, bench::machine_comments ("codec")))
    fail ("cannot write " + options.output);
  return 0;
}

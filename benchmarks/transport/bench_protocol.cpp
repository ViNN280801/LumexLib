// bench_protocol.cpp
// The cost of a protocol message of lumex/core/protocol: the encode and the
// decode of a reply message of 6, 100 and 2420 bytes, the match of a frame
// against the run-time index, and `describe` (the log line of a frame) of a
// 6-byte request, a 100-byte reply and a 2420-byte reply.
//
// Clock: none. No timer, thread or transport takes part; steady_clock times
// the CPU work.
//
// Equal work: every case first checks its result (the decoded message
// encodes back to the same bytes, `describe` names the row and writes a line
// at least as long as the frame). The loops keep the frames and the results
// alive with optimizer barriers, so a call cannot be deleted.
//
// Usage: LumexTransportBenchProtocol [output.csv] [--quick] [--filter text]
//        [--list]
#include <cstdint>
#include <cstdio>
#include <string>

#include "bench_transport_common.hpp"
#include "bench_transport_tables.hpp"

namespace
{
namespace bt = bench_transport;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
using layout::bytes_t;
using layout::bytes_view_t;
using P = bt::bench_protocol_t;

[[noreturn]] void
fail (std::string const &why)
{
  std::fprintf (stderr, "bench_protocol: %s\n", why.c_str ());
  std::exit (1);
}

template <typename Row>
void
run_message (bench::reporter_t &report, bench::options_t const &options,
             char const *name)
{
  bytes_t wire;
  if (!layout::sample_reply_message<P, Row> (wire))
    fail (std::string (name) + ": no sample message");
  auto decoded = layout::decode_reply_message<P, Row> (
      bytes_view_t (wire.data (), wire.size ()));
  if (!decoded)
    fail (std::string (name) + ": the sample does not decode");
  bytes_t again;
  if (!layout::encode_reply_message<P, Row> (again, *decoded) || again != wire)
    fail (std::string (name) + ": the message does not encode back");
  std::string const note = "bytes=" + std::to_string (wire.size ());

  std::string const decode_name = std::string ("message_decode_") + name;
  if (report.wanted (decode_name))
    {
      std::uint64_t sink = 0;
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  bench::keep_object (wire);
                  auto got = layout::decode_reply_message<P, Row> (
                      bytes_view_t (wire.data (), wire.size ()));
                  bench::keep_object (got);
                  sink += got.has_value () ? 1u : 0u;
                }
            });
      bench::keep_value (sink);
      report.add (decode_name, "none", "ns/message", timed.median,
                  timed.minimum, timed.passes, timed.iterations, note);
    }

  std::string const encode_name = std::string ("message_encode_") + name;
  if (report.wanted (encode_name))
    {
      bytes_t out;
      out.reserve (wire.size ());
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  out.clear ();
                  bench::keep_object (*decoded);
                  layout::encode_reply_message<P, Row> (out, *decoded);
                  bench::keep_object (out);
                }
            });
      report.add (encode_name, "none", "ns/message", timed.median,
                  timed.minimum, timed.passes, timed.iterations, note);
    }

  std::string const match_name = std::string ("index_match_") + name;
  std::string const describe_name = std::string ("describe_rx_") + name;
  index::index_result_t const &made = index::protocol_index<P> ();
  if (!made)
    fail ("the index does not build");
  index::protocol_index_t const &table = *made;
  if (report.wanted (match_name))
    {
      std::uint64_t sink = 0;
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  bench::keep_object (wire);
                  index::frame_match_t const found = table.match (
                      bytes_view_t (wire.data (), wire.size ()));
                  bench::keep_object (found);
                  sink += 1u;
                }
            });
      bench::keep_value (sink);
      report.add (match_name, "none", "ns/frame", timed.median, timed.minimum,
                  timed.passes, timed.iterations, note);
    }
  if (report.wanted (describe_name))
    {
      std::string const line
          = table.describe (bytes_view_t (wire.data (), wire.size ()),
                            index::frame_direction::rx);
      if (line.find (Row::identifier ()) == std::string::npos
          || line.size () < wire.size ())
        fail (describe_name + ": the line does not name the row: " + line);
      std::uint64_t sink = 0;
      bench::timing_t const timed = bench::measure (
          options,
          [&] (std::uint64_t n)
            {
              for (std::uint64_t i = 0; i < n; ++i)
                {
                  bench::keep_object (wire);
                  std::string text = table.describe (
                      bytes_view_t (wire.data (), wire.size ()),
                      index::frame_direction::rx);
                  bench::keep_object (text);
                  sink += text.size ();
                }
            });
      bench::keep_value (sink);
      report.add (describe_name, "none", "ns/frame", timed.median,
                  timed.minimum, timed.passes, timed.iterations,
                  note + " chars=" + std::to_string (line.size ()));
    }
}
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("protocol", options);

  run_message<P::WIDE_t> (report, options, "100");
  run_message<P::BLOCK_t> (report, options, "2420");
  run_message<P::PING_t> (report, options, "6");

  // The request of a 6-byte exchange: its describe line is the TX form.
  {
    bytes_t request;
    if (!layout::encode_message<P, P::PING_t> (request, std::uint32_t (7),
                                               std::uint8_t (1)))
      fail ("no request message");
    index::index_result_t const &made = index::protocol_index<P> ();
    if (!made)
      fail ("the index does not build");
    std::string const name = "describe_tx_6";
    if (report.wanted (name))
      {
        std::uint64_t sink = 0;
        bench::timing_t const timed = bench::measure (
            options,
            [&] (std::uint64_t n)
              {
                for (std::uint64_t i = 0; i < n; ++i)
                  {
                    bench::keep_object (request);
                    std::string text = made->describe (
                        bytes_view_t (request.data (), request.size ()),
                        index::frame_direction::tx);
                    bench::keep_object (text);
                    sink += text.size ();
                  }
              });
        bench::keep_value (sink);
        report.add (name, "none", "ns/frame", timed.median, timed.minimum,
                    timed.passes, timed.iterations, "bytes=6");
      }
  }

  if (!report.write (options.output, bench::machine_comments ("protocol")))
    fail ("cannot write " + options.output);
  return 0;
}

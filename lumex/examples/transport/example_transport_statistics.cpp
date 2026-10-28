// The counters of lumex::transport: byte_stream::statistics () and
// framed_stream::statistics () return a snapshot of atomic counters (bytes,
// operations, errors by transport_errc, timeouts, cancellations, the last
// activity). Shown on a mock stream, which a program drives itself, so every
// number below is exact: a successful and a failed read, a deadline, a
// disconnect, write_all in chunks (it counts as ONE write, also when it fails
// half way) and the framing layer (frames, framing errors, dropped frames).
// The same calls work on a serial port, a TCP stream and a UDP socket.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#endif
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
#include "lumex/core/framing/LumexFraming"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
namespace
{
using ms = std::chrono::milliseconds;
using tr::error::transport_errc;

// The warnings of the framing layer (a bad line, a full queue) go to the
// same stream as the numbers, in order.
void
print_record (tr::log::log_record_t const &record)
{
  std::cout << "  [log " << record.component << "] " << record.message << '\n';
}

std::vector<std::uint8_t>
bytes_of (std::string const &text)
{
  return std::vector<std::uint8_t> (text.begin (), text.end ());
}

// The non-zero error counters by name: errors_of takes one transport_errc
// value at a time, total_errors () sums them all and the other_errors of
// codes of the operating system.
void
print_errors (tr::statistics::statistics_t const &stats)
{
  transport_errc const all[]
      = { transport_errc::timeout,         transport_errc::cancelled,
          transport_errc::closed,          transport_errc::not_open,
          transport_errc::disconnected,    transport_errc::would_deadlock,
          transport_errc::invalid_endpoint };
  std::cout << "    errors " << stats.total_errors () << " in all";
  for (std::size_t i = 0; i < sizeof all / sizeof all[0]; ++i)
    if (stats.errors_of (all[i]) != 0)
      std::cout << ", " << tr::error::make_error_code (all[i]).message ()
                << " x" << stats.errors_of (all[i]);
  if (stats.other_errors != 0)
    std::cout << ", other (system) x" << stats.other_errors;
  std::cout << '\n';
}

void
print_statistics (char const *title, tr::statistics::statistics_t const &stats)
{
  std::cout << "  " << title << '\n'
            << "    opens " << stats.opens << ", reads " << stats.reads << " ("
            << stats.bytes_read << " B), writes " << stats.writes << " ("
            << stats.bytes_written << " B)\n"
            << "    timeouts " << stats.timeouts << ", cancellations "
            << stats.cancellations << '\n';
  print_errors (stats);
}

// Polls a condition for at most two seconds: the framing decoder runs on the
// stream's strand, so its effect on the counters is a moment away.
template <typename Condition>
bool
eventually (Condition condition)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (2);
  while (!condition ())
    {
      if (std::chrono::steady_clock::now () >= end)
        return false;
      std::this_thread::sleep_for (ms (1));
    }
  return true;
}

bool
stream_counters (tr::stream::io_runner &runner)
{
  std::cout << "--- 1. byte_stream::statistics ()\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));

  std::vector<std::uint8_t> const request = bytes_of ("STATUS?\n");
  stream.write_all (tr::const_bytes_t (request), tr::deadline_t (500));
  mock.control.feed (bytes_of ("OK 12\n"));
  std::vector<std::uint8_t> reply (32);
  stream.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (500));
  print_statistics ("after an open, a write and a read", stream.statistics ());

  // A deadline: the mock has nothing to read, the call cancels its own
  // operation and returns timeout. It counts once, as a timeout, and not as
  // a cancellation as well.
  tr::io_result_t const late
      = stream.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (50));
  std::cout << "  a read with a 50 ms deadline: " << late.error ().message ()
            << '\n';

  // An error of the operating system is counted apart from transport_errc.
  mock.control.fail_next_read (std::make_error_code (std::errc::io_error));
  stream.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (500));

  // The peer goes: reads end with disconnected.
  mock.control.disconnect ();
  stream.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (500));

  tr::statistics::statistics_t const stats = stream.statistics ();
  print_statistics ("after a deadline, a system error and a disconnect",
                    stats);
  bool const recent = stats.last_activity > std::chrono::system_clock::now ()
                                                - std::chrono::seconds (10);
  std::cout << "    last activity within 10 s: " << (recent ? "yes" : "no")
            << '\n';

  // The counters of the transport are the only state reset_statistics ()
  // touches. A snapshot is a copy: `stats` above did not change.
  stream.reset_statistics ();
  std::cout << "  after reset_statistics (): " << stream.statistics ().reads
            << " reads, " << stream.statistics ().total_errors ()
            << " errors; the earlier snapshot still says " << stats.reads
            << " read(s)\n";
  return stats.opens == 1 && stats.reads == 1 && stats.writes == 1
         && stats.timeouts == 1 && stats.cancellations == 0
         && stats.errors_of (transport_errc::disconnected) == 1
         && stats.other_errors == 1 && stats.bytes_read == 6
         && stats.bytes_written == 8;
}

bool
write_all_counters (tr::stream::io_runner &runner)
{
  std::cout << "--- 2. write_all: one write, the bytes that went\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  tr::stream::byte_stream &stream = mock.stream;
  stream.open (tr::deadline_t (500));

  // The device takes at most 3 bytes per write: write_all loops over four
  // writes of the transport and counts the call once.
  mock.control.set_write_chunk (3);
  std::vector<std::uint8_t> const block = bytes_of ("0123456789");
  stream.write_all (tr::const_bytes_t (block), tr::deadline_t (500));
  tr::statistics::statistics_t stats = stream.statistics ();
  std::cout << "  10 B in chunks of 3: " << stats.writes << " write, "
            << stats.bytes_written << " B\n";

  // The second chunk fails: the handler learns how much went before the
  // error, and the counters keep those bytes.
  mock.control.fail_write_after (
      1, std::make_error_code (std::errc::connection_reset));
  std::promise<std::pair<std::error_code, std::size_t>> done;
  stream.async_write_all (
      tr::const_bytes_t (block),
      [&done] (std::error_code error, std::size_t written)
        { done.set_value (std::make_pair (error, written)); });
  std::pair<std::error_code, std::size_t> const outcome
      = done.get_future ().get ();
  stats = stream.statistics ();
  std::cout << "  a write_all that fails after one chunk: "
            << outcome.first.message () << ", " << outcome.second
            << " B written before it; counters: " << stats.bytes_written
            << " B in all, " << stats.writes << " write(s), "
            << stats.other_errors << " system error(s)\n";
  return outcome.second == 3 && stats.bytes_written == 13 && stats.writes == 1
         && stats.other_errors == 1;
}

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
namespace fr = lumex::core::framing;

bool
framed_counters (tr::stream::io_runner &runner)
{
  std::cout << "--- 3. framed_stream::statistics ()\n";
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "bench");
  mock.stream.open (tr::deadline_t (500));

  fr::delimiter::delimiter_options_t line_options;
  line_options.rx_delimiter = std::vector<std::uint8_t> (1, '\n');
  line_options.tx_suffix = std::vector<std::uint8_t> (1, '\n');
  line_options.max_frame_size = 8; // a longer line is a framing error
  tr::framed::framed_options_t limits;
  limits.queue_bound = 2; // frames that wait for the reader
  tr::framed::framed_stream<fr::delimiter::delimiter_codec> link (
      std::move (mock.stream), fr::delimiter::delimiter_codec (line_options),
      limits);

  // One line too long between two good ones: the decoder counts the error
  // and finds the next frame by itself, the reader never sees the bad line.
  link.start ();
  mock.control.feed (bytes_of ("OK 1\nTHIS LINE IS FAR TOO LONG\nOK 2\n"));
  tr::framed::frame_result_t const first
      = link.read_frame (tr::deadline_t (1000));
  tr::framed::frame_result_t const second
      = link.read_frame (tr::deadline_t (1000));
  if (!first.has_value () || !second.has_value ())
    return false;
  std::cout << "  frames read: '"
            << std::string (first.value ().begin (), first.value ().end ())
            << "', '"
            << std::string (second.value ().begin (), second.value ().end ())
            << "'\n";

  // Five lines and a reader that does not come: the queue holds two, the
  // oldest are dropped (and counted), the newest are kept.
  mock.control.feed (bytes_of ("L1\nL2\nL3\nL4\nL5\n"));
  eventually ([&link] { return link.statistics ().frames_in >= 7; });
  eventually ([&link] { return link.queued_frames () == 2; });

  tr::framed::framed_statistics_t const stats = link.statistics ();
  std::cout << "  framing layer: " << stats.frames_in << " frames in, "
            << stats.bytes_in << " wire B in, " << stats.frames_out
            << " frames out\n"
            << "    framing errors " << stats.total_errors ()
            << " (frame_too_large "
            << stats.errors_of (fr::codec::framing_errc::frame_too_large)
            << "), dropped " << stats.dropped_frames << ", queued "
            << link.queued_frames () << ", timeouts " << stats.timeouts
            << '\n';

  // The transport underneath keeps its own counters (the number of reads
  // depends on how the chunks fell, the bytes do not).
  tr::statistics::statistics_t const under = link.transport_statistics ();
  std::cout << "  the stream under the adapter: " << under.bytes_read
            << " B read, " << under.total_errors () << " errors\n";

  // A synchronous read that waits for a frame that never comes counts in the
  // adapter's timeouts.
  link.read_frame (tr::deadline_t (10));
  link.read_frame (tr::deadline_t (10));
  link.read_frame (tr::deadline_t (50));
  std::cout << "  after reading the queue empty and one more deadline: "
            << link.statistics ().timeouts << " timeout\n";

  link.reset_statistics (); // the adapter's counters and the transport's
  std::cout << "  after reset_statistics (): " << link.statistics ().frames_in
            << " frames in, " << link.transport_statistics ().reads
            << " reads\n";
  link.close ();
  return stats.frames_in == 7
         && stats.errors_of (fr::codec::framing_errc::frame_too_large) == 1
         && stats.dropped_frames == 3;
}
#endif
} // namespace

int
main ()
{
  tr::log::set_log_sink (&print_record);
  tr::stream::io_runner runner (1);
  if (!stream_counters (runner))
    return 1;
  if (!write_all_counters (runner))
    return 1;
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
  if (!framed_counters (runner))
    return 1;
#else
  std::cout << "--- 3. built without the framing module "
               "(LUMEX_BUILD_FRAMING): framed_stream is not available\n";
#endif
  return 0;
}
#else
int
main ()
{
  std::cout << "built without the mock transports "
               "(LUMEX_TRANSPORT_WITH_TESTING): nothing to show\n";
  return 0;
}
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// Helpers of the framed adapter tests. The frames are synthetic: letters and
// made-up bytes, never a real device protocol. The codec cases give every
// codec of core/framing the same face (make, payloads, oversized payload, the
// wire of a frame beyond the limit); the oracle decodes a stream with the
// codec alone, so a test only has to show that the adapter delivers what the
// codec does. The scripted stream is a stream_impl the test drives by hand
// (reads wait for the test, writes stay pending until it completes them), for
// the tests that run on an io_context the test polls itself.

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_FRAMED_TEST_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_FRAMED_TEST_SUPPORT_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#include "lumex/core/crc/LumexCrc"
#include "lumex/core/framing/LumexFraming"

namespace lumex_framed_test
{
namespace tr = lumex::applied::transport;
namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;
using fr::codec::framing_errc;
using bytes_t = std::vector<std::uint8_t>;
using clock_type = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;

/// Waits until `condition` holds, at most `limit`; returns its last value.
template <typename Condition>
bool
eventually (Condition condition, ms limit = ms (5000))
{
  clock_type::time_point const end = clock_type::now () + limit;
  // Yield first: a sleep of a thread lasts a timer tick (15 ms on Windows).
  for (unsigned spins = 0;; ++spins)
    {
      if (condition ())
        return true;
      if (clock_type::now () >= end)
        return condition ();
      if (spins < 4000)
        std::this_thread::yield ();
      else
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}

/// `size` letters 'a'..'z' in a row, from the letter `first`.
inline bytes_t
letters (std::size_t size, std::size_t first = 0)
{
  bytes_t out;
  for (std::size_t i = 0; i < size; ++i)
    out.push_back (static_cast<std::uint8_t> ('a' + (first + i) % 26));
  return out;
}

/// Pseudo-random bytes, the same for a seed everywhere.
inline bytes_t
noise (std::size_t size, std::uint32_t seed)
{
  bytes_t out;
  std::uint32_t state = seed * 2654435761u + 12345u;
  for (std::size_t i = 0; i < size; ++i)
    {
      state = state * 1664525u + 1013904223u;
      out.push_back (static_cast<std::uint8_t> (state >> 24));
    }
  return out;
}

inline bytes_t
concat (bytes_t first, bytes_t const &second)
{
  first.insert (first.end (), second.begin (), second.end ());
  return first;
}

inline fr::codec::bytes_view_t
view_of (bytes_t const &bytes)
{
  return fr::codec::bytes_view_t (bytes.data (), bytes.size ());
}

// --- the log
// -----------------------------------------------------------------

/// Captures the records of the transport log of this binary while it lives,
/// at every level; restores the sink and the level afterwards.
class log_capture
{
public:
  struct record
  {
    tr::log::log_level level;
    std::string component;
    std::string message;
  };

  log_capture ()
      : level_ (tr::log::get_log_level ()), sink_ (tr::log::get_log_sink ())
  {
    records ().clear ();
    tr::log::set_log_sink (&log_capture::capture);
    tr::log::set_log_level (tr::log::log_level::trace);
  }

  ~log_capture ()
  {
    tr::log::set_log_sink (sink_);
    tr::log::set_log_level (level_);
  }

  log_capture (log_capture const &) = delete;
  log_capture &operator= (log_capture const &) = delete;

  /// How many records of `level` hold `needle` in their message.
  static std::size_t
  count (tr::log::log_level level, std::string const &needle)
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    std::size_t found = 0;
    for (record const &item : records ())
      if (item.level == level
          && item.message.find (needle) != std::string::npos)
        ++found;
    return found;
  }

  /// All the messages of `level`, joined by newlines (for failure output).
  static std::string
  dump ()
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    std::string text;
    for (record const &item : records ())
      text += std::to_string (static_cast<int> (item.level)) + " "
              + item.component + ": " + item.message + "\n";
    return text;
  }

private:
  static std::mutex &
  mutex ()
  {
    static std::mutex instance;
    return instance;
  }

  static std::vector<record> &
  records ()
  {
    static std::vector<record> instance;
    return instance;
  }

  static void
  capture (tr::log::log_record_t const &item)
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    record entry;
    entry.level = item.level;
    entry.component = item.component;
    entry.message = item.message;
    records ().push_back (entry);
  }

  tr::log::log_level level_;
  tr::log::log_sink_t sink_;
};

// --- the oracle
// --------------------------------------------------------------

/// What a codec alone makes of a stream.
struct decoded_t
{
  std::vector<bytes_t> frames;
  std::size_t errors[6] = { 0, 0, 0, 0, 0, 0 };
  std::size_t other_errors = 0;

  std::size_t
  error_total () const
  {
    std::size_t total = other_errors;
    for (std::size_t count : errors)
      total += count;
    return total;
  }
};

struct decoded_sink
{
  decoded_t *where;

  void
  operator() (fr::codec::bytes_view_t frame, std::error_code error) const
  {
    if (!error)
      {
        where->frames.push_back (bytes_t (frame.begin (), frame.end ()));
        return;
      }
    if (error.category () == fr::codec::framing_category ()
        && error.value () >= 1 && error.value () <= 6)
      ++where->errors[error.value () - 1];
    else
      ++where->other_errors;
  }
};

template <typename Codec>
decoded_t
decode_direct (Codec codec, bytes_t const &stream)
{
  decoded_t result;
  decoded_sink sink = { &result };
  codec.decode (view_of (stream), sink);
  return result;
}

template <typename Codec>
bytes_t
encode_direct (Codec const &codec, bytes_t const &payload, bool *ok = nullptr)
{
  bytes_t wire;
  fr::codec::encode_result_t const result
      = codec.encode (view_of (payload), wire);
  if (ok)
    *ok = result.has_value ();
  if (!result.has_value ())
    wire.clear ();
  return wire;
}

// --- the codec cases
// ---------------------------------------------------------

/// Every case: `make (limit)`, `limit ()` (the max_frame_size the tests use),
/// `payload (i)` for i in 0..3 (valid, at most limit bytes), `oversized ()`
/// (a payload `encode` refuses at `limit ()`), `beyond_limit ()` (the wire of
/// a valid frame whose payload the decoder at `limit ()` refuses) and
/// `name ()`.
struct slip_case
{
  using codec_t = fr::slip::slip_codec;
  static char const *
  name ()
  {
    return "slip";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::slip::slip_options_t options;
    options.max_frame_size = limit;
    return codec_t (options);
  }
  static bytes_t
  payload (std::size_t i)
  {
    switch (i)
      {
      case 0:
        return letters (1);
      case 1:
        return bytes_t{ 0xC0, 0xDB, 0x00, 0xDC, 0xDD };
      case 2:
        return letters (17, 3);
      default:
        return bytes_t (24, 0xC0);
      }
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (100));
  }
};

struct cobs_case
{
  using codec_t = fr::cobs::cobs_codec;
  static char const *
  name ()
  {
    return "cobs";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::cobs::cobs_options_t options;
    options.max_frame_size = limit;
    return codec_t (options);
  }
  static bytes_t
  payload (std::size_t i)
  {
    switch (i)
      {
      case 0:
        return letters (1);
      case 1:
        return bytes_t{ 0x00, 0x11, 0x00, 0x00, 0x22 };
      case 2:
        return letters (17, 3);
      default:
        return bytes_t (24, 0x00);
      }
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (100));
  }
};

struct delimiter_case
{
  using codec_t = fr::delimiter::delimiter_codec;
  static char const *
  name ()
  {
    return "delimiter";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::delimiter::delimiter_options_t options;
    options.max_frame_size = limit;
    options.rx_delimiter = bytes_t{ 0x0D, 0x0A };
    options.tx_suffix = bytes_t{ 0x0D, 0x0A };
    return codec_t (options);
  }
  static bytes_t
  payload (std::size_t i)
  {
    return letters (1 + i * 7, i);
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (100));
  }
};

struct length_case
{
  using codec_t = fr::length::length_prefix_codec;
  static char const *
  name ()
  {
    return "length_prefix";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::length::length_prefix_options_t options;
    options.max_frame_size = limit;
    options.width = 2;
    return codec_t (options);
  }
  static bytes_t
  payload (std::size_t i)
  {
    switch (i)
      {
      case 0:
        return letters (1);
      case 1:
        return bytes_t{ 0x00, 0xFF, 0x00 };
      case 2:
        return letters (17, 3);
      default:
        return noise (24, 7);
      }
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (100));
  }
};

inline fr::header::header_options_t
header_options (std::size_t limit)
{
  fr::header::header_options_t options;
  options.max_frame_size = limit;
  options.tx_preamble = bytes_t{ 0xA5, 0x5A };
  options.rx_preamble = bytes_t{ 0xA5, 0x5A };
  options.header_size = 6;
  options.length_offset = 2;
  options.length_width = 2;
  options.length_order = fr::codec::byte_order::little;
  return options;
}

/// A header frame without a check value: [A5 5A][2 bytes][length:2][data].
struct header_case
{
  using codec_t = fr::header::header_codec<>;
  static char const *
  name ()
  {
    return "header";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    return codec_t (header_options (limit));
  }
  // The first two bytes are the header fields, the rest the data.
  static bytes_t
  payload (std::size_t i)
  {
    return letters (2 + i * 6, i);
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (102));
  }
};

/// A header frame with a CRC-16 check value.
struct header_crc_case
{
  using codec_t = fr::header::header_codec<
      st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
              fr::codec::byte_order::little>>;
  static char const *
  name ()
  {
    return "header_crc";
  }
  static std::size_t
  limit ()
  {
    return 32;
  }
  static codec_t
  make (std::size_t limit)
  {
    return codec_t (header_options (limit));
  }
  static bytes_t
  payload (std::size_t i)
  {
    return letters (2 + i * 6, i + 5);
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (102));
  }
};

/// Modbus RTU frames (a public format) with made-up register values.
struct modbus_case
{
  using codec_t = fr::modbus_rtu::modbus_rtu_codec;
  static char const *
  name ()
  {
    return "modbus_rtu";
  }
  static std::size_t
  limit ()
  {
    return 40;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::modbus_rtu::modbus_rtu_options_t options;
    options.max_frame_size = limit;
    return codec_t (options);
  }
  // The replies of function 3: address, function, byte count, registers.
  static bytes_t
  payload (std::size_t i)
  {
    bytes_t out;
    out.push_back (0x11);
    out.push_back (0x03);
    out.push_back (static_cast<std::uint8_t> (2 * (i + 1)));
    for (std::size_t k = 0; k < 2 * (i + 1); ++k)
      out.push_back (static_cast<std::uint8_t> (0x30 + k));
    return out;
  }
  static bytes_t
  oversized ()
  {
    return letters (300);
  }
  static bytes_t
  beyond_limit ()
  {
    bytes_t reply;
    reply.push_back (0x11);
    reply.push_back (0x03);
    reply.push_back (100);
    reply.insert (reply.end (), 100, 0xFF);
    return encode_direct (make (4096), reply);
  }
};

/// A pipeline: CR-delimited text, a '$' prefix, a sum check as hex digits,
/// the payload as hex digits.
struct pipeline_case
{
  using codec_t = st::pipeline<
      fr::delimiter::delimiter_codec, st::prefix_char_stage<'$'>,
      st::checksum_stage<st::sum8, st::ascii_hex_render>, st::ascii_hex_stage>;
  static char const *
  name ()
  {
    return "pipeline";
  }
  static std::size_t
  limit ()
  {
    return 64;
  }
  static codec_t
  make (std::size_t limit)
  {
    fr::delimiter::delimiter_options_t options;
    options.max_frame_size = limit;
    return codec_t (options);
  }
  static bytes_t
  payload (std::size_t i)
  {
    return noise (1 + i * 6, i + 11);
  }
  static bytes_t
  oversized ()
  {
    return letters (200);
  }
  static bytes_t
  beyond_limit ()
  {
    return encode_direct (make (4096), letters (100));
  }
};

/// The SLIP wire of `n` frames whose payloads are the one bytes `first`,
/// `first + 1`, ...
inline bytes_t
slip_wire (std::size_t n, std::uint8_t first = 0)
{
  slip_case::codec_t const codec = slip_case::make (slip_case::limit ());
  bytes_t wire;
  for (std::size_t i = 0; i < n; ++i)
    wire = concat (
        wire, encode_direct (
                  codec, bytes_t (1, static_cast<std::uint8_t> (first + i))));
  return wire;
}

// --- rigs
// --------------------------------------------------------------------

/// A framed_stream over a mock stream on an io_runner of two threads.
template <typename Case> struct stream_rig
{
  explicit stream_rig (std::size_t limit = Case::limit (),
                       tr::framed::framed_options_t options
                       = tr::framed::framed_options_t ())
      : runner (2), mock (tr::testing::make_mock_stream (
                        runner.get_executor (), Case::name ())),
        link (std::move (mock.stream), Case::make (limit), options)
  {
  }

  void
  open ()
  {
    ASSERT_TRUE (link.transport ().open (tr::deadline_t (2000)).has_value ());
  }

  tr::stream::io_runner runner;
  tr::testing::mock_stream mock;
  tr::framed::framed_stream<typename Case::codec_t> link;
};

/// A framed_datagram over a mock datagram transport on an io_runner.
template <typename Case> struct datagram_rig
{
  explicit datagram_rig (std::size_t limit = Case::limit (),
                         tr::framed::framed_options_t options
                         = tr::framed::framed_options_t ())
      : runner (2), mock (tr::testing::make_mock_datagram (
                        runner.get_executor (), Case::name ())),
        link (std::move (mock.transport), Case::make (limit), options)
  {
  }

  void
  open ()
  {
    ASSERT_TRUE (link.transport ().open (tr::deadline_t (2000)).has_value ());
  }

  tr::stream::io_runner runner;
  tr::testing::mock_datagram mock;
  tr::framed::framed_datagram<typename Case::codec_t> link;
};

/// Feeds `stream` to a framed_stream over the mock in two parts, split at
/// `split` (0: whole), with reads of at most `chunk` bytes, and checks that
/// the frames and the error counts equal those of the codec alone.
template <typename Case>
::testing::AssertionResult
framed_equals_codec (bytes_t const &stream, std::size_t split,
                     std::size_t chunk = 4096)
{
  decoded_t const expected
      = decode_direct (Case::make (Case::limit ()), stream);
  tr::framed::framed_options_t options;
  options.queue_bound = 4096;
  options.read_chunk_size = chunk;
  stream_rig<Case> rig (Case::limit (), options);
  if (!rig.link.transport ().open (tr::deadline_t (2000)).has_value ())
    return ::testing::AssertionFailure () << "open failed";
  rig.link.start ();
  if (split > 0 && split < stream.size ())
    {
      rig.mock.control.feed (
          bytes_t (stream.begin (), stream.begin () + split));
      if (!eventually ([&rig, split]
                         { return rig.link.statistics ().bytes_in == split; }))
        return ::testing::AssertionFailure ()
               << "the first part was not consumed (split " << split << ")";
      rig.mock.control.feed (bytes_t (stream.begin () + split, stream.end ()));
    }
  else
    rig.mock.control.feed (stream);
  bool const settled = eventually (
      [&]
        {
          tr::framed::framed_statistics_t const now = rig.link.statistics ();
          return now.bytes_in == stream.size ()
                 && now.frames_in == expected.frames.size ()
                 && now.total_errors () == expected.error_total ();
        });
  tr::framed::framed_statistics_t const got = rig.link.statistics ();
  if (!settled)
    return ::testing::AssertionFailure ()
           << "split " << split << ": frames_in " << got.frames_in
           << " (codec alone: " << expected.frames.size () << "), errors "
           << got.total_errors ()
           << " (codec alone: " << expected.error_total () << "), bytes_in "
           << got.bytes_in << " of " << stream.size ();
  for (std::size_t k = 0; k < 6; ++k)
    if (got.errors[k] != expected.errors[k])
      return ::testing::AssertionFailure ()
             << "split " << split << ": errors[" << k << "] " << got.errors[k]
             << " (codec alone: " << expected.errors[k] << ")";
  for (std::size_t i = 0; i < expected.frames.size (); ++i)
    {
      tr::framed::frame_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      if (!frame.has_value ())
        return ::testing::AssertionFailure ()
               << "split " << split << ": frame " << i << ": "
               << frame.error ().message ();
      if (frame.value () != expected.frames[i])
        return ::testing::AssertionFailure ()
               << "split " << split << ": frame " << i << " differs";
    }
  if (rig.link.queued_frames () != 0)
    return ::testing::AssertionFailure () << "frames are left in the queue";
  return ::testing::AssertionSuccess ();
}

// --- a stream the test drives by hand ---------------------------------------

/// A stream_impl whose reads wait for the test and whose writes stay pending
/// until the test completes them; cancel and close end both with `cancelled`.
/// Everything runs on the strand: drive it from the thread that polls the
/// io_context.
class scripted_impl final : public tr::stream::stream_impl
{
public:
  explicit scripted_impl (tr::executor_type const &executor)
      : stream_impl (executor, "scripted")
  {
  }

  /// Writes started and not completed yet.
  std::size_t
  writes_pending () const
  {
    return writes_.size ();
  }

  /// A read is waiting for bytes.
  bool
  read_pending () const
  {
    return static_cast<bool> (read_.handler);
  }

  /// How many reads the stream was asked for.
  int read_calls = 0;
  /// How many times the stream was cancelled.
  int cancel_calls = 0;

  /// The bytes of the writes the test completed.
  bytes_t written;

  /// The device has `bytes` to give: a waiting read takes them at once, else
  /// the next read does.
  void
  device_delivers (bytes_t const &bytes)
  {
    incoming_.insert (incoming_.end (), bytes.begin (), bytes.end ());
    serve_read ();
  }

  /// The oldest pending write is done: all of its bytes (or `bytes` of them).
  void
  device_completes_write (std::size_t bytes = static_cast<std::size_t> (-1))
  {
    pending_write item = std::move (writes_.front ());
    writes_.erase (writes_.begin ());
    std::size_t const done = (std::min)(bytes, item.data.size ());
    written.insert (written.end (), item.data.begin (),
                    item.data.begin () + static_cast<std::ptrdiff_t> (done));
    complete (std::move (item.handler), std::error_code (), done);
  }

  /// The oldest pending write fails with `error`.
  void
  device_fails_write (std::error_code error)
  {
    pending_write item = std::move (writes_.front ());
    writes_.erase (writes_.begin ());
    complete (std::move (item.handler), error, 0);
  }

  /// The device reports `error` to the waiting read.
  void
  device_fails_read (std::error_code error)
  {
    if (!read_.handler)
      return;
    tr::io_handler_t handler = std::move (read_.handler);
    read_.handler = tr::io_handler_t ();
    complete (std::move (handler), error, 0);
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    set_open (true);
    complete (std::move (handler), std::error_code ());
  }

  void
  do_read_some (tr::mutable_bytes_t buffer, tr::io_handler_t handler) override
  {
    ++read_calls;
    read_.buffer = buffer;
    read_.handler = std::move (handler);
    serve_read ();
  }

  void
  do_write_some (tr::const_bytes_t data, tr::io_handler_t handler) override
  {
    pending_write item;
    item.data.assign (data.begin (), data.end ());
    item.handler = std::move (handler);
    writes_.push_back (std::move (item));
  }

  void
  do_cancel () override
  {
    ++cancel_calls;
    end_all (
        tr::error::make_error_code (tr::error::transport_errc::cancelled));
  }

  void
  do_close () override
  {
    end_all (
        tr::error::make_error_code (tr::error::transport_errc::cancelled));
  }

private:
  struct pending_read
  {
    tr::mutable_bytes_t buffer;
    tr::io_handler_t handler;
  };

  struct pending_write
  {
    bytes_t data;
    tr::io_handler_t handler;
  };

  void
  serve_read ()
  {
    if (!read_.handler || incoming_.empty ())
      return;
    std::size_t const count
        = (std::min)(incoming_.size (), read_.buffer.size ());
    std::copy (incoming_.begin (),
               incoming_.begin () + static_cast<std::ptrdiff_t> (count),
               read_.buffer.data ());
    incoming_.erase (incoming_.begin (),
                     incoming_.begin () + static_cast<std::ptrdiff_t> (count));
    tr::io_handler_t handler = std::move (read_.handler);
    read_.handler = tr::io_handler_t ();
    complete (std::move (handler), std::error_code (), count);
  }

  void
  end_all (std::error_code error)
  {
    if (read_.handler)
      {
        tr::io_handler_t handler = std::move (read_.handler);
        read_.handler = tr::io_handler_t ();
        complete (std::move (handler), error, 0);
      }
    std::vector<pending_write> pending;
    pending.swap (writes_);
    for (pending_write &item : pending)
      complete (std::move (item.handler), error, 0);
  }

  pending_read read_;
  std::vector<pending_write> writes_;
  bytes_t incoming_;
};

/// A byte stream over a scripted device, and the device.
struct scripted_stream
{
  scripted_stream (tr::executor_type const &executor)
      : device (std::make_shared<scripted_impl> (executor)), stream (device)
  {
  }

  std::shared_ptr<scripted_impl> device;
  tr::stream::byte_stream stream;
};

/// What a handler saw.
struct seen
{
  int calls = 0;
  std::error_code error;
  bytes_t frame;
  std::uint8_t report_id = 0;
};

struct frame_recorder
{
  seen *where;
  void
  operator() (std::error_code error, tr::framed::frame_t frame)
  {
    ++where->calls;
    where->error = error;
    where->frame = std::move (frame);
  }
};

struct message_recorder
{
  seen *where;
  void
  operator() (std::error_code error, tr::datagram::message_t message)
  {
    ++where->calls;
    where->error = error;
    where->frame = std::move (message.payload);
    where->report_id = message.report_id;
  }
};

struct status_recorder
{
  seen *where;
  void
  operator() (std::error_code error)
  {
    ++where->calls;
    where->error = error;
  }
};
} // namespace lumex_framed_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_FRAMED_TEST_SUPPORT_HPP

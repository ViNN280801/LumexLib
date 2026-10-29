// Every public header of the transport and every token style in one
// translation unit, built with strict warnings (see CMakeLists.txt). At run
// time a mock stream and a mock datagram answer each style once, directly and
// through a session (stream_session, message_session), and a steady and a
// virtual timer wait with each token style; exit code 0 when every exchange
// returned what was fed, 1 otherwise. With the logger adapter
// (LUMEX_TRANSPORT_WITH_LOGGER) the run goes through the LumexLogger sink and
// a lumex_logger_watch_timer. With the framing module
// (LUMEX_TRANSPORT_WITH_FRAMING) every token style also runs through
// framed_stream, and framed_datagram answers a round. With the HID transport
// (LUMEX_TRANSPORT_WITH_HID) a synthetic descriptor is analyzed, the real
// hidapi enumerates, and a device on a path that does not exist fails to
// open under a hid_runtime owner object.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

// The umbrella alone: it brings the logger adapter (LUMEX_BUILD_LOGGER) and
// the mocks (LUMEX_TRANSPORT_WITH_TESTING) behind their options.
#include "lumex/applied/transport/LumexTransport"
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
#include "lumex/core/framing/LumexFraming"
#endif

namespace
{
namespace tr = lumex::applied::transport;

std::vector<std::uint8_t>
reply ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

template <typename Stream>
bool
by_callback (Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  stream.async_read_some (tr::mutable_bytes_t (buffer),
                          [&done] (std::error_code error, std::size_t bytes)
                            { done.set_value (error ? 0 : bytes); });
  return done.get_future ().get () == 3 && buffer == reply ();
}

template <typename Stream>
bool
by_future (Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::size_t const bytes
      = stream.async_read_some (tr::mutable_bytes_t (buffer), asio::use_future)
            .get ();
  std::size_t const written
      = stream.async_write_all (tr::const_bytes_t (buffer), asio::use_future)
            .get ();
  return bytes == 3 && written == buffer.size () && buffer == reply ();
}

// A deferred operation launched later, with a future.
bool
by_deferred (tr::stream::byte_stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  auto read
      = stream.async_read_some (tr::mutable_bytes_t (buffer), asio::deferred);
  std::size_t const bytes = std::move (read) (asio::use_future).get ();
  return bytes == 3 && buffer == reply ();
}

// A cancellation slot (asio::cancel_after) and a handler bound to a strand.
bool
by_cancel_after (tr::stream::io_runner &runner,
                 tr::stream::byte_stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (
          std::chrono::seconds (10),
          asio::bind_executor (
              asio::make_strand (runner.get_executor ()),
              [&done] (std::error_code error, std::size_t bytes)
                { done.set_value (error ? 0 : bytes); })));
  return done.get_future ().get () == 3 && buffer == reply ();
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
template <typename Stream>
bool
by_yield (tr::stream::io_runner &runner, Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  asio::spawn (
      runner.get_executor (),
      [&stream, &buffer, &done] (asio::yield_context yield)
        {
          std::error_code error;
          std::size_t const bytes = stream.async_read_some (
              tr::mutable_bytes_t (buffer), yield[error]);
          done.set_value (error ? 0 : bytes);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3 && buffer == reply ();
}
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
// GCC lowers the switch of a coroutine's state machine itself and reports it
// as a switch without a default (-Wswitch-default) at the lambda: the
// fixture's own code, not the library's.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
template <typename Stream>
bool
by_awaitable (tr::stream::io_runner &runner, Stream &stream)
{
  std::vector<std::uint8_t> buffer (3);
  std::promise<std::size_t> done;
  asio::co_spawn (
      runner.get_executor (),
      [&stream, &buffer, &done] () -> asio::awaitable<void>
        {
          std::size_t const bytes = co_await stream.async_read_some (
              tr::mutable_bytes_t (buffer), asio::use_awaitable);
          done.set_value (bytes);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3 && buffer == reply ();
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

bool
datagram_round (tr::stream::io_runner &runner)
{
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (runner.get_executor (), "datagram");
  if (!mock.transport.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::datagram::message_t message;
  message.payload = reply ();
  mock.control.deliver (message);
  tr::datagram::message_t const received
      = mock.transport.async_receive (asio::use_future).get ();
  mock.transport.async_send (received, asio::use_future).get ();
  return received.payload == reply ();
}
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
// --- the framed adapters (framed_stream, framed_datagram) ------------------

using codec_t = lumex::core::framing::slip::slip_codec;
using link_t = tr::framed::framed_stream<codec_t>;

std::vector<std::uint8_t>
frame_payload ()
{
  return std::vector<std::uint8_t> (5, std::uint8_t (0x46));
}

std::vector<std::uint8_t>
frame_wire ()
{
  std::vector<std::uint8_t> const payload = frame_payload ();
  std::vector<std::uint8_t> wire;
  codec_t const codec;
  if (!codec
           .encode (lumex::core::framing::codec::bytes_view_t (
                        payload.data (), payload.size ()),
                    wire)
           .has_value ())
    wire.clear ();
  return wire;
}

bool
framed_by_callback (link_t &link)
{
  std::promise<std::size_t> done;
  link.async_read_frame (
      [&done] (std::error_code error, tr::framed::frame_t frame)
        { done.set_value (error ? 0 : frame.size ()); });
  return done.get_future ().get () == 5;
}

bool
framed_by_future (link_t &link)
{
  std::vector<std::uint8_t> const payload = frame_payload ();
  link.async_write_frame (tr::const_bytes_t (payload), asio::use_future)
      .get ();
  return link.async_read_frame (asio::use_future).get () == payload;
}

bool
framed_by_deferred (link_t &link)
{
  auto read = link.async_read_frame (asio::deferred);
  return std::move (read) (asio::use_future).get () == frame_payload ();
}

bool
framed_by_cancel_after (tr::stream::io_runner &runner, link_t &link)
{
  std::promise<std::size_t> done;
  link.async_read_frame (asio::cancel_after (
      std::chrono::seconds (10),
      asio::bind_executor (
          asio::make_strand (runner.get_executor ()),
          [&done] (std::error_code error, tr::framed::frame_t frame)
            { done.set_value (error ? 0 : frame.size ()); })));
  return done.get_future ().get () == 5;
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
bool
framed_by_yield (tr::stream::io_runner &runner, link_t &link)
{
  std::promise<std::size_t> done;
  asio::spawn (
      runner.get_executor (),
      [&link, &done] (asio::yield_context yield)
        {
          std::error_code error;
          tr::framed::frame_t const frame
              = link.async_read_frame (yield[error]);
          done.set_value (error ? 0 : frame.size ());
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 5;
}
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
// GCC lowers the switch of a coroutine's state machine itself and reports it
// as a switch without a default (-Wswitch-default) at the lambda: the
// fixture's own code, not the library's.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
bool
framed_by_awaitable (tr::stream::io_runner &runner, link_t &link)
{
  std::promise<std::size_t> done;
  asio::co_spawn (
      runner.get_executor (),
      [&link, &done] () -> asio::awaitable<void>
        {
          tr::framed::frame_t const frame
              = co_await link.async_read_frame (asio::use_awaitable);
          done.set_value (frame.size ());
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 5;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

// Every token style through framed_stream over a mock stream, the
// synchronous calls, the statistics, and a framed_datagram round.
bool
framed_round (tr::stream::io_runner &runner)
{
  namespace codec = lumex::core::framing::codec;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "framed");
  tr::framed::framed_options_t options;
  options.queue_bound = 8;
  link_t link = tr::framed::make_framed_stream (std::move (mock.stream),
                                                codec_t (), options);
  if (!link.transport ().open (tr::deadline_t (1000)).has_value ())
    return false;
  bool fine = true;
  mock.control.feed (frame_wire ());
  fine = framed_by_callback (link) && fine;
  mock.control.feed (frame_wire ());
  fine = framed_by_deferred (link) && fine;
  mock.control.feed (frame_wire ());
  fine = framed_by_cancel_after (runner, link) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  mock.control.feed (frame_wire ());
  fine = framed_by_yield (runner, link) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  mock.control.feed (frame_wire ());
  fine = framed_by_awaitable (runner, link) && fine;
#endif
  // The write of the future style is read back as its echo.
  mock.control.feed (frame_wire ());
  fine = framed_by_future (link) && fine;
  // The synchronous API.
  std::vector<std::uint8_t> const payload = frame_payload ();
  fine = link.write_frame (tr::const_bytes_t (payload), tr::deadline_t (1000))
             .has_value ()
         && fine;
  mock.control.feed (frame_wire ());
  tr::framed::frame_result_t const frame
      = link.read_frame (tr::deadline_t (1000));
  // operator* and not value (): the latter trips C4702 at /W4 in expected.
  fine = frame.has_value () && *frame == payload && fine;
  tr::framed::frame_result_t const none
      = link.read_frame (tr::deadline_t (20));
  fine = !none.has_value ()
         && none.error () == tr::error::transport_errc::timeout && fine;
  // A payload beyond the limit of the codec is refused and not written.
  std::vector<std::uint8_t> const big (
      codec::Constants::KDEFAULT_MAX_FRAME_SIZE + 1, std::uint8_t (1));
  tr::status_result_t const refused
      = link.write_frame (tr::const_bytes_t (big), tr::deadline_t (1000));
  fine = !refused.has_value ()
         && refused.error () == codec::framing_errc::payload_too_large && fine;
  tr::framed::framed_statistics_t const counted = link.statistics ();
  fine = counted.frames_in >= 5 && counted.frames_out == 2
         && counted.errors_of (codec::framing_errc::payload_too_large) == 1
         && counted.dropped_frames == 0 && link.queued_frames () == 0 && fine;
  fine = link.transport_statistics ().total_errors () == 0 && fine;
  link.reset_statistics ();
  fine = link.statistics ().frames_in == 0 && fine;

  tr::testing::mock_datagram datagrams
      = tr::testing::make_mock_datagram (runner.get_executor (), "framed");
  tr::framed::framed_datagram<codec_t> messages (
      std::move (datagrams.transport), codec_t ());
  if (!messages.transport ().open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::datagram::message_t message;
  message.payload = frame_wire ();
  message.report_id = 2;
  datagrams.control.deliver (message);
  tr::datagram::message_t const got
      = messages.async_read_frame (asio::use_future).get ();
  fine = got.payload == payload && got.report_id == 2 && fine;
  messages.async_write_frame (3, tr::const_bytes_t (payload), asio::use_future)
      .get ();
  fine = datagrams.control.sent ().size () == 1
         && datagrams.control.sent ()[0].report_id == 3 && fine;
  return fine;
}
#endif

// The mock streams a session's factory made, for the script.
struct made_streams
{
  std::mutex mutex;
  std::vector<tr::testing::mock_stream_control> controls;
};

// Every token style again through a stream_session, then a state wait and a
// message_session round.
bool
session_round (tr::stream::io_runner &runner)
{
  std::shared_ptr<made_streams> const made = std::make_shared<made_streams> ();
  tr::session::stream_session session (
      runner.get_executor (),
      [made] (tr::executor_type const &executor) -> tr::stream::byte_stream
        {
          tr::testing::mock_stream mock
              = tr::testing::make_mock_stream (executor, "session");
          std::lock_guard<std::mutex> const lock (made->mutex);
          made->controls.push_back (mock.control);
          return std::move (mock.stream);
        });
  if (!session.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::testing::mock_stream_control control = made->controls.back ();
  bool fine = true;
  control.feed (reply ());
  fine = by_callback (session) && fine;
  control.feed (reply ());
  fine = by_future (session) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  control.feed (reply ());
  fine = by_yield (runner, session) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  control.feed (reply ());
  fine = by_awaitable (runner, session) && fine;
#endif
  // A read through asio::deferred, launched later.
  control.feed (reply ());
  std::vector<std::uint8_t> deferred_buffer (3);
  auto deferred_read = session.async_read_some (
      tr::mutable_bytes_t (deferred_buffer), asio::deferred);
  std::promise<std::size_t> deferred_done;
  std::move (deferred_read) (
      [&deferred_done] (std::error_code error, std::size_t bytes)
        { deferred_done.set_value (error ? 0 : bytes); });
  fine = deferred_done.get_future ().get () == 3 && fine;
  // The close ends the pending waits with cancelled (the wait from a
  // snapshot misses no change).
  std::promise<std::error_code> changed;
  session.async_wait_state_change (
      tr::session::channel_state::open,
      [&changed] (std::error_code error, tr::session::channel_state)
        { changed.set_value (error); });
  tr::session::state_snapshot_t const seen = session.state_snapshot ();
  std::promise<std::error_code> snapshot_changed;
  session.async_wait_state_change (
      seen, [&snapshot_changed] (std::error_code error,
                                 tr::session::state_snapshot_t)
        { snapshot_changed.set_value (error); });
  session.close ();
  fine = changed.get_future ().get () == tr::error::transport_errc::cancelled
         && snapshot_changed.get_future ().get ()
                == tr::error::transport_errc::cancelled
         && session.session_statistics ().attempts == 1 && fine;

  tr::session::message_session messages (
      runner.get_executor (),
      [] (tr::executor_type const &executor) -> tr::datagram::message_transport
        {
          tr::testing::mock_datagram mock
              = tr::testing::make_mock_datagram (executor, "messages");
          return std::move (mock.transport);
        });
  if (!messages.open (tr::deadline_t (1000)).has_value ())
    return false;
  tr::datagram::message_t message;
  message.payload = reply ();
  messages.async_send (message, asio::use_future).get ();
  return fine;
}
// The timer seam: a steady timer with each token style (a callback, a
// future, a deferred wait, cancel_after started in its strand) and a timer of
// the virtual clock, advanced by hand and through run_until.
bool
timer_round (tr::stream::io_runner &runner)
{
  namespace ttime = tr::time;
  using ms = std::chrono::milliseconds;
  ttime::timer steady = ttime::steady_timer_source ().make_timer (
      asio::make_strand (runner.get_executor ()));
  std::promise<std::error_code> called;
  steady.async_wait_for (ms (1), [&called] (std::error_code error)
                           { called.set_value (error); });
  bool fine = !called.get_future ().get ();
  steady.async_wait_for (ms (1), asio::use_future).get ();
  auto deferred = steady.async_wait_until (steady.now (), asio::deferred);
  std::move (deferred) (asio::use_future).get ();
  std::promise<std::error_code> cut;
  asio::post (steady.get_executor (),
              [&steady, &cut] ()
                {
                  steady.async_wait_for (
                      ms (60000),
                      asio::cancel_after (ms (1), [&cut] (std::error_code e)
                                            { cut.set_value (e); }));
                });
  fine = cut.get_future ().get () == tr::error::transport_errc::cancelled
         && fine;

  tr::testing::virtual_clock clock;
  ttime::timer virtual_timer = clock.source ().make_timer (
      asio::make_strand (runner.get_executor ()));
  std::future<void> waited
      = virtual_timer.async_wait_for (ms (100), asio::use_future);
  fine = clock.advance (ms (100)) == 1 && fine;
  waited.get ();
  asio::io_context context;
  ttime::timer polled = clock.source ().make_timer (context.get_executor ());
  bool ended = false;
  polled.async_wait_for (ms (50),
                         [&ended] (std::error_code error) { ended = !error; });
  fine = tr::testing::run_until (
             context, clock, [&ended] { return ended; }, ms (1000))
         && fine;
  return fine;
}

#if defined(LUMEX_TRANSPORT_WITH_HID)
// --- the HID transport (hid_device over hidapi)
// ------------------------------
bool
hid_round (tr::stream::io_runner &runner)
{
  // A synthetic vendor-defined device: report 1 with 4 bytes out.
  std::uint8_t const descriptor[]
      = { 0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85,
          0x01, 0x75, 0x08, 0x95, 0x04, 0x91, 0x02, 0xC0 };
  tr::hid::report_layout_result_t const layout
      = tr::hid::analyze_report_descriptor (
          tr::const_bytes_t (descriptor, sizeof descriptor));
  bool fine = layout.has_value () && layout->numbered
              && layout->output_size (1) == 4;
  tr::hid::hid_runtime const runtime;
  // Whatever is plugged in: the call runs, the result is not checked.
  std::vector<tr::hid::hid_device_info_t> const found
      = tr::hid::enumerate_hid_devices (tr::hid::hid_filter_t ());
  static_cast<void> (found);
  tr::hid::hid_options_t options;
  options.path = "lumex-no-such-hid-device";
  tr::hid::hid_device device
      = tr::hid::make_hid_device (runner.get_executor (), options);
  fine = !device.open (tr::deadline_t (2000)).has_value () && fine;
  fine = tr::hid::opened_report_layout (device).input_reports.empty () && fine;
  std::promise<std::error_code> sent;
  tr::datagram::message_t message;
  device.async_send (message, [&sent] (std::error_code error)
                       { sent.set_value (error); });
  fine = sent.get_future ().get () == tr::error::transport_errc::not_open
         && fine;
  return tr::hid::hid_runtime::shutdown (tr::deadline_t (2000)) && fine;
}
#endif
} // namespace

int
main ()
{
  tr::stream::io_runner runner (2);
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
  tr::log::install_lumex_logger_sink ();
  tr::log::lumex_logger_watch_timer const watch (
      runner, std::chrono::milliseconds (50));
  if (!watch.is_running ())
    return 1;
#endif
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "stream");
  if (!mock.stream.open (tr::deadline_t (1000)).has_value ())
    return 1;

  bool fine = true;
  mock.control.feed (reply ());
  fine = by_callback (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_future (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_deferred (mock.stream) && fine;
  mock.control.feed (reply ());
  fine = by_cancel_after (runner, mock.stream) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  mock.control.feed (reply ());
  fine = by_yield (runner, mock.stream) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  mock.control.feed (reply ());
  fine = by_awaitable (runner, mock.stream) && fine;
#endif
  fine = datagram_round (runner) && fine;
  fine = session_round (runner) && fine;
  fine = timer_round (runner) && fine;
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
  fine = framed_round (runner) && fine;
#endif
#if defined(LUMEX_TRANSPORT_WITH_HID)
  fine = hid_round (runner) && fine;
#endif
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
  tr::log::uninstall_lumex_logger_sink ();
  fine = tr::log::get_log_sink () == nullptr && fine;
#endif

  // The counters: the exchanges above were counted (a snapshot and a reset
  // compile and run under the strict warnings too).
  tr::statistics::statistics_t const counted = mock.stream.statistics ();
  fine = counted.opens == 1 && counted.reads >= 4 && counted.writes >= 1
         && counted.total_errors () == 0 && fine;
  mock.stream.reset_statistics ();
  fine = mock.stream.statistics ().reads == 0 && fine;

  std::cout << "transport_warnings: " << (fine ? "fine" : "FAILED") << '\n';
  return fine ? 0 : 1;
}

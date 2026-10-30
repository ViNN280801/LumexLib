// Every public header of the transport and every token style in one
// translation unit, built with strict warnings (see CMakeLists.txt). At run
// time a mock stream and a mock datagram answer each style once, directly and
// through a session (stream_session, message_session), a steady and a
// virtual timer wait with each token style, and the scripted mocks (options,
// observers, timed delivery, held writes) run on a virtual clock; exit code 0
// when every exchange returned what was fed, 1 otherwise. With the logger
// adapter (LUMEX_TRANSPORT_WITH_LOGGER) the run goes through the LumexLogger
// sink and a lumex_logger_watch_timer. With the framing module
// (LUMEX_TRANSPORT_WITH_FRAMING) every token style also runs through
// framed_stream, and framed_datagram answers a round. With the HID transport
// (LUMEX_TRANSPORT_WITH_HID) a synthetic descriptor is analyzed, the real
// hidapi enumerates, and a device on a path that does not exist fails to
// open under a hid_runtime owner object. With the protocol module
// (LUMEX_FIXTURE_WITH_PROTOCOL) a table with every word of the dialogue
// vocabulary is indexed, described, exported and read through its traits,
// and with the dialogue engine (LUMEX_TRANSPORT_WITH_PROTOCOL) every token
// style runs through dialogue_engine and frame_link over a framed mock
// stream.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// The umbrella alone: it brings the logger adapter (LUMEX_BUILD_LOGGER) and
// the mocks (LUMEX_TRANSPORT_WITH_TESTING) behind their options.
#include "lumex/applied/transport/LumexTransport"
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
#include "lumex/core/framing/LumexFraming"
#endif
#if defined(LUMEX_FIXTURE_WITH_PROTOCOL)
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"
#endif

#if defined(LUMEX_FIXTURE_WITH_PROTOCOL)
// --- the dialogue vocabulary over a protocol table: every reply kind, row
// option and protocol option, the hooks of the index and the traits
// ------------------------------------------------------------------------
namespace dialogue_fixture
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

struct codec_t
{
};

inline bool
match_any (index::row_t const *, layout::bytes_view_t, layout::bytes_view_t)
{
  return true;
}

LUMEX_PROTOCOL_LAYOUT (refusal_t, (u8_, u16_le_), ("code", "detail"));
LUMEX_PROTOCOL_LAYOUT (state_t, (u8_, u8_), ("valve", "mode"));

struct header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
  template <typename P>
  using dialogue_options = dialogue::options<
      dialogue::keepalive<typename P::TICK_t, 1000>,
      dialogue::watchdog_ms<3000>, dialogue::min_interval_ms<20>,
      dialogue::state_from<typename P::STATE_t, 1>,
      dialogue::identify_with<typename P::STATE_t>, dialogue::max_in_flight<1>,
      dialogue::default_timeout_ms<400>, dialogue::default_retries<1>,
      dialogue::late_reply_window_ms<100>>;
};

// clang-format off
#define FIXTURE_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (NAK, 0x15, "Refused", (), (), (u8_), ()) \
  X (CHANGE, 0x30, "Changed", (), (), (state_t), ()) \
  X (TICK, 0x11, "Tick", (u32_le_), ("seconds"), (), (idempotent)) \
  X (STATE, 0x12, "State", (), (), (state_t), \
     (stream (100), edge_event<0, 0, 1> ())) \
  X (START, 0x01, "Start", (u8_), ("channel"), (ack_nak<u8_, refusal_t>), \
     (min_interval (50))) \
  X (STOP, 0x02, "Stop", (), (), (ack_nak<ACK_t, NAK_t>), ()) \
  X (PRIME, 0x03, "Prime", (u16_le_), ("volume"), \
     (two_phase<ACK_t, u16_le_>), ()) \
  X (SET_FLOW, 0x04, "Flow", (u16_le_), ("flow"), (echo_set<u16_le_>), ()) \
  X (SET_LIMIT, 0x05, "Limit", (u8_), ("limit"), \
     (echo_zero_on_error<u8_>), ()) \
  X (SET_MODE, 0x07, "Mode", (u8_), ("mode"), \
     (state_instead_of_refusal<u8_>), ()) \
  X (WRITE, 0x08, "Write", (u8_), ("value"), (), (reply_row<CHANGE_t> ())) \
  X (READ, 0x09, "Read", (u8_), ("parameter"), (u16_le_), \
     (correlate (correlation::by_order))) \
  X (QUERY, 0x0A, "Query", (), (), (u8_), (match_with (&match_any)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (fixture_t, codec_t, header_t, FIXTURE_ROWS);

inline std::string
rx (index::protocol_index_t const &where, std::vector<std::uint8_t> frame)
{
  return where.describe (layout::bytes_view_t (frame.data (), frame.size ()),
                         index::frame_direction::rx);
}

inline bool
dialogue_round ()
{
  static_assert (dialogue::check_protocol_t<fixture_t>::value,
                 "the fixture table is valid");
  index::index_result_t const &result = index::protocol_index<fixture_t> ();
  if (!result.has_value ())
    return false;
  index::protocol_index_t const &where = *result;
  bool fine = rx (where, { 0x01, 0x07 })
              == "RX fixture_t.START (Start) ACK "
                 "value=7";
  fine = rx (where, { 0x01, 0x05, 0x34, 0x12 })
             == "RX fixture_t.START (Start) NAK code=5 detail=4660"
         && fine;
  fine = rx (where, { 0x02 }).find ("decode error") != std::string::npos
         && fine;
  fine = rx (where, { 0x03, 0xE8, 0x03 })
             == "RX fixture_t.PRIME (Prime) COMPLETION value=1000"
         && fine;
  fine = rx (where, { 0x04, 0x01, 0x00 })
             == "RX fixture_t.SET_FLOW (Flow) ECHO value=1"
         && fine;
  fine = rx (where, { 0x05, 0x00 })
             == "RX fixture_t.SET_LIMIT (Limit) REFUSED value=0"
         && fine;
  fine = rx (where, { 0x07, 0x02 })
             == "RX fixture_t.SET_MODE (Mode) ECHO value=2"
         && fine;
  std::string const document = index::export_markdown (where);
  fine = document.find ("ack_nak (ACK: message `ACK`; NAK: message `NAK`)")
             != std::string::npos
         && document.find ("edge_event field 0: 0 -> 1") != std::string::npos
         && document.find ("reply_row CHANGE") != std::string::npos
         && document.find ("correlation by_order") != std::string::npos
         && document.find ("match_with custom") != std::string::npos && fine;
  using traits = dialogue::protocol_traits_t<fixture_t>;
  fine = traits::max_in_flight () == 1 && traits::watchdog_ms () == 3000
         && traits::keepalive_period_ms () == 1000
         && traits::min_interval_ms () == 20 && traits::state_field () == 1
         && traits::default_timeout_ms () == 400
         && traits::default_retries () == 1
         && traits::late_reply_window_ms () == 100 && fine;
  fine = dialogue::row_traits_t<fixture_t, fixture_t::STOP_t>::how ()
             == dialogue::correlation::by_order
         && dialogue::row_traits_t<fixture_t, fixture_t::WRITE_t>::how ()
                == dialogue::correlation::by_reply_key
         && dialogue::row_traits_t<fixture_t, fixture_t::QUERY_t>::matcher ()
                != nullptr
         && dialogue::row_traits_t<fixture_t, fixture_t::START_t>::kind ()
                == dialogue::reply_kind::ack_nak
         && dialogue::row_traits_t<fixture_t,
                                   fixture_t::START_t>::min_interval_ms ()
                == 50
         && fine;
  dialogue::reply_value_of_t<fixture_t::PRIME_t>::type const completion = 1;
  return completion == 1 && fine;
}
} // namespace dialogue_fixture
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

#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)                                    \
    && defined(LUMEX_FIXTURE_WITH_PROTOCOL)
// --- the dialogue engine over a framed mock stream: every token style of
// dialogue_engine and frame_link, decode_reply_kind ------------------------
namespace pr = lumex::applied::transport::protocol;

pr::exchange_t
state_exchange ()
{
  namespace fx = dialogue_fixture;
  pr::exchange_t out;
  if (!fx::layout::encode_message<fx::fixture_t, fx::fixture_t::STATE_t> (
           out.message)
           .has_value ())
    out.message.clear ();
  out.row = fx::index::protocol_index<fx::fixture_t> ()->find_name ("STATE");
  out.how = fx::dialogue::row_traits_t<fx::fixture_t,
                                       fx::fixture_t::STATE_t>::how ();
  return out;
}

bool
engine_by_callback (pr::dialogue_engine &engine)
{
  std::promise<std::size_t> done;
  engine.async_exchange (state_exchange (), [&done] (std::error_code error,
                                                     pr::reply_frame_t frame)
                           { done.set_value (error ? 0 : frame.size ()); });
  return done.get_future ().get () == 3;
}

bool
engine_by_future (pr::dialogue_engine &engine)
{
  return engine.async_exchange (state_exchange (), asio::use_future)
             .get ()
             .size ()
         == 3;
}

bool
engine_by_deferred (pr::dialogue_engine &engine)
{
  auto exchange = engine.async_exchange (state_exchange (), asio::deferred);
  return std::move (exchange) (asio::use_future).get ().size () == 3;
}

bool
engine_by_cancel_after (tr::stream::io_runner &runner,
                        pr::dialogue_engine &engine)
{
  std::promise<std::size_t> done;
  engine.async_exchange (
      state_exchange (),
      asio::cancel_after (
          std::chrono::seconds (10),
          asio::bind_executor (
              asio::make_strand (runner.get_executor ()),
              [&done] (std::error_code error, pr::reply_frame_t frame)
                { done.set_value (error ? 0 : frame.size ()); })));
  return done.get_future ().get () == 3;
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
bool
engine_by_yield (tr::stream::io_runner &runner, pr::dialogue_engine &engine)
{
  std::promise<std::size_t> done;
  asio::spawn (
      runner.get_executor (),
      [&engine, &done] (asio::yield_context yield)
        {
          std::error_code error;
          pr::reply_frame_t const frame
              = engine.async_exchange (state_exchange (), yield[error]);
          done.set_value (error ? 0 : frame.size ());
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3;
}
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
bool
engine_by_awaitable (tr::stream::io_runner &runner,
                     pr::dialogue_engine &engine)
{
  std::promise<std::size_t> done;
  asio::co_spawn (
      runner.get_executor (),
      [&engine, &done] () -> asio::awaitable<void>
        {
          pr::reply_frame_t const frame = co_await engine.async_exchange (
              state_exchange (), asio::use_awaitable);
          done.set_value (frame.size ());
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  return done.get_future ().get () == 3;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

// The engine over a framed mock stream whose device answers every request
// with the state, a frame_link of its own, and the classification of a
// reply kind.
bool
engine_round (tr::stream::io_runner &runner)
{
  namespace codec = lumex::core::framing::codec;
  namespace fx = dialogue_fixture;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "engine");
  if (!mock.stream.open (tr::deadline_t (1000)).has_value ())
    return false;
  std::shared_ptr<codec_t> const decoder = std::make_shared<codec_t> ();
  tr::testing::mock_stream_control control = mock.control;
  mock.control.on_write (
      [decoder, control] (std::vector<std::uint8_t> const &chunk) mutable
        {
          decoder->decode (
              codec::bytes_view_t (chunk.data (), chunk.size ()),
              [&control] (codec::bytes_view_t)
                {
                  std::vector<std::uint8_t> const state = { 0x12, 0x01, 0x02 };
                  std::vector<std::uint8_t> wire;
                  if (codec_t ()
                          .encode (codec::bytes_view_t (state.data (),
                                                        state.size ()),
                                   wire)
                          .has_value ())
                    control.feed (wire);
                });
        });
  pr::dialogue_options_t options;
  options.name = "engine";
  options.defaults.timeout_ms = 5000;
  pr::dialogue_engine engine (
      pr::frame_link (tr::framed::framed_stream<codec_t> (
          std::move (mock.stream), codec_t ())),
      *fx::index::protocol_index<fx::fixture_t> (), options);
  bool fine = engine_by_callback (engine);
  fine = engine_by_future (engine) && fine;
  fine = engine_by_deferred (engine) && fine;
  fine = engine_by_cancel_after (runner, engine) && fine;
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  fine = engine_by_yield (runner, engine) && fine;
#endif
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
  fine = engine_by_awaitable (runner, engine) && fine;
#endif
  pr::dialogue_statistics_t const counted = engine.statistics ();
  fine = counted.requests >= 4 && counted.replies == counted.requests
         && counted.timeouts == 0 && fine;
  engine.close ();
  mock.control.on_write (tr::testing::write_observer_t ());

  // A frame_link of its own: a write and a read by future.
  tr::testing::mock_stream other
      = tr::testing::make_mock_stream (runner.get_executor (), "link");
  if (!other.stream.open (tr::deadline_t (1000)).has_value ())
    return false;
  pr::frame_link link (tr::framed::framed_stream<codec_t> (
      std::move (other.stream), codec_t ()));
  link.async_write_frame (frame_payload (), asio::use_future).get ();
  other.control.feed (frame_wire ());
  fine = link.async_read_frame (asio::use_future).get () == frame_payload ()
         && fine;
  link.close ();

  std::vector<std::uint8_t> const nak = { 0x01, 0x05, 0x34, 0x12 };
  auto const decoded
      = fx::dialogue::decode_reply_kind<fx::fixture_t,
                                        fx::fixture_t::START_t> (
          fx::layout::bytes_view_t (nak.data (), nak.size ()));
  fine = decoded.has_value () && decoded->part == fx::dialogue::reply_part::nak
         && fine;
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
  namespace timing = tr::timing;
  using ms = std::chrono::milliseconds;
  timing::timer steady = timing::steady_timer_source ().make_timer (
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
  timing::timer virtual_timer = clock.source ().make_timer (
      asio::make_strand (runner.get_executor ()));
  std::future<void> waited
      = virtual_timer.async_wait_for (ms (100), asio::use_future);
  fine = clock.advance (ms (100)) == 1 && fine;
  waited.get ();
  asio::io_context context;
  timing::timer polled = clock.source ().make_timer (context.get_executor ());
  bool ended = false;
  polled.async_wait_for (ms (50),
                         [&ended] (std::error_code error) { ended = !error; });
  fine = tr::testing::run_until (
             context, clock, [&ended] { return ended; }, ms (1000))
         && fine;
  return fine;
}

// --- the scripted mocks: options with a timer source, the write and send
// observers, timed delivery on the virtual clock, held writes, an empty read,
// a held open and a send that fails after some succeeded.
bool
scripted_round ()
{
  using ms = std::chrono::milliseconds;
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t options;
  options.name = "scripted";
  options.timers = clock.source ();
  tr::testing::mock_stream stream
      = tr::testing::make_mock_stream (context.get_executor (), options);
  tr::testing::mock_datagram datagrams
      = tr::testing::make_mock_datagram (context.get_executor (), options);

  bool opened = false;
  bool datagram_opened = false;
  stream.stream.async_open ([&opened] (std::error_code error)
                              { opened = !error; });
  datagrams.transport.async_open ([&datagram_opened] (std::error_code error)
                                    { datagram_opened = !error; });
  bool fine = tr::testing::run_until (
      context, clock, [&] { return opened && datagram_opened; }, ms (10));

  std::size_t seen = 0;
  stream.control.on_write ([&seen] (std::vector<std::uint8_t> const &chunk)
                             { seen += chunk.size (); });
  stream.control.set_read_chunk (2);
  stream.control.feed_after (ms (5), reply ());
  std::vector<std::uint8_t> buffer (8);
  std::size_t got = 0;
  stream.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      [&got] (std::error_code error, std::size_t bytes)
        { got = error ? 0 : bytes; });
  fine = tr::testing::run_until (
             context, clock, [&got] { return got != 0; }, ms (100))
         && got == 2 && fine;

  stream.control.hold_writes ();
  std::vector<std::uint8_t> const payload (2, std::uint8_t (1));
  bool wrote = false;
  stream.stream.async_write_some (tr::const_bytes_t (payload),
                                  [&wrote] (std::error_code error, std::size_t)
                                    { wrote = !error; });
  fine = !tr::testing::run_until (
             context, clock, [&wrote] { return wrote; }, ms (1))
         && fine;
  stream.control.release_writes ();
  fine = tr::testing::run_until (
             context, clock, [&wrote] { return wrote; }, ms (10))
         && seen == payload.size () && fine;

  stream.control.complete_next_read_empty ();
  std::size_t empty_count = 1;
  bool empty_done = false;
  stream.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      [&empty_count, &empty_done] (std::error_code error, std::size_t bytes)
        {
          empty_count = error ? 1 : bytes;
          empty_done = true;
        });
  fine = tr::testing::run_until (
             context, clock, [&empty_done] { return empty_done; }, ms (10))
         && empty_count == 0 && fine;
  stream.control.on_write (tr::testing::write_observer_t ());

  std::size_t sent = 0;
  datagrams.control.on_send ([&sent] (tr::datagram::message_t const &)
                               { ++sent; });
  datagrams.control.fail_send_after (1, make_error_code (std::errc::io_error));
  tr::datagram::message_t message;
  message.payload = reply ();
  std::error_code first_send = make_error_code (std::errc::io_error);
  std::error_code second_send;
  bool sends_done = false;
  datagrams.transport.async_send (message,
                                  [&] (std::error_code error)
                                    {
                                      first_send = error;
                                      datagrams.transport.async_send (
                                          message,
                                          [&] (std::error_code again)
                                            {
                                              second_send = again;
                                              sends_done = true;
                                            });
                                    });
  fine = tr::testing::run_until (
             context, clock, [&sends_done] { return sends_done; }, ms (10))
         && !first_send && second_send && sent == 1 && fine;
  datagrams.control.on_send (tr::testing::send_observer_t ());
  datagrams.control.hold_sends ();
  datagrams.control.release_sends ();

  datagrams.control.deliver_after (ms (7), message);
  bool received = false;
  datagrams.transport.async_receive (
      [&received] (std::error_code error, tr::datagram::message_t)
        { received = !error; });
  fine = tr::testing::run_until (
             context, clock, [&received] { return received; }, ms (100))
         && fine;

  datagrams.control.hold_next_open ();
  datagrams.transport.close ();
  std::error_code held_open;
  bool held_done = false;
  datagrams.transport.async_open (
      [&held_open, &held_done] (std::error_code error)
        {
          held_open = error;
          held_done = true;
        });
  bool const still_held = !tr::testing::run_until (
      context, clock, [&held_done] { return held_done; }, ms (1));
  datagrams.transport.cancel ();
  fine = still_held
         && tr::testing::run_until (
             context, clock, [&held_done] { return held_done; }, ms (10))
         && held_open == tr::error::transport_errc::cancelled && fine;

  stream.stream.close ();
  datagrams.transport.close ();
  static_cast<void> (
      tr::testing::run_until (context, clock, [] { return false; }, ms (0)));
  return fine;
}

// --- the behaviours: a plan, a mock that carries it out, the timed script
bool
behavior_round ()
{
  using ms = std::chrono::milliseconds;
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t options;
  options.name = "behaviors";
  options.timers = clock.source ();
  auto stream
      = tr::testing::make_mock_stream (context.get_executor (), options);

  bool opened = false;
  stream.stream.async_open ([&opened] (std::error_code error)
                              { opened = !error; });
  bool fine = tr::testing::run_until (
      context, clock, [&opened] { return opened; }, ms (10));

  tr::testing::behavior_applier applier (
      tr::testing::behavior::fragmented (1, 3, ms (2))
          .then (tr::testing::behavior::repeating (1, ms (3))),
      clock.source ());
  tr::testing::reply_plan_t const plan = applier.plan (reply (), reply ());
  fine = !plan.steps.empty () && plan.request_processed && fine;
  applier.respond (stream.control, reply (), reply ());
  std::vector<std::uint8_t> buffer (8);
  std::size_t got = 0;
  stream.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      [&got] (std::error_code error, std::size_t bytes)
        { got = error ? 0 : bytes; });
  stream.control.empty_read_after (ms (50));
  stream.control.disconnect_after (ms (60));
  stream.control.fail_opens_for (ms (70),
                                 make_error_code (std::errc::io_error));
  fine = !tr::testing::run_until (
             context, clock, [] { return false; }, ms (100))
         && got > 0 && fine;

  tr::testing::row_shape_t shape;
  shape.retries = 1;
  shape.idempotent = true;
  fine = tr::testing::expected_outcome (tr::testing::behavior_kind::silent,
                                        shape)
             == tr::testing::outcome_class::retries_exhausted
         && std::string (tr::testing::behavior_kind_name (
                tr::testing::behavior::of (tr::testing::behavior_kind::echo)
                    .kind ()))
                == "echo"
         && tr::testing::all_behavior_kinds ().size () == 18 && fine;
  stream.stream.close ();
  static_cast<void> (
      tr::testing::run_until (context, clock, [] { return false; }, ms (0)));
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
  fine = scripted_round () && fine;
  fine = behavior_round () && fine;
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
  fine = framed_round (runner) && fine;
#endif
#if defined(LUMEX_TRANSPORT_WITH_HID)
  fine = hid_round (runner) && fine;
#endif
#if defined(LUMEX_FIXTURE_WITH_PROTOCOL)
  fine = dialogue_fixture::dialogue_round () && fine;
#endif
#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)                                    \
    && defined(LUMEX_FIXTURE_WITH_PROTOCOL)
  fine = engine_round (runner) && fine;
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

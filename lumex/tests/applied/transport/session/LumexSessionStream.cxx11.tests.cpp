// A session as a transport: stream_session::as_byte_stream and
// message_session::as_message_transport: the state and the events, the close
// count that ends a partial frame, what a read of the connection that is over
// hands on, open, close, cancel, the drop, the frames kept by an adapter.

#include <chrono>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"

namespace
{
namespace tr = lumex::applied::transport;
namespace ss = lumex::applied::transport::session;
namespace fr = lumex::core::framing;
using ms = std::chrono::milliseconds;
using tr::error::transport_errc;

ss::session_options_t
options_on (tr::testing::virtual_clock &clock)
{
  ss::session_options_t options;
  options.policy.initial_delay = ms (100);
  options.policy.jitter = 0;
  options.timers = clock.source ();
  return options;
}

std::vector<std::uint8_t>
slip_of (std::vector<std::uint8_t> const &payload)
{
  std::vector<std::uint8_t> wire;
  EXPECT_TRUE (fr::slip::slip_codec ()
                   .encode (lumex::core::framing::codec::bytes_view_t (
                                payload.data (), payload.size ()),
                            wire)
                   .has_value ());
  return wire;
}

struct stream_bench_t
{
  stream_bench_t ()
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (tr::testing::make_mock_stream (
        context.get_executor (), mock_options)));
    std::shared_ptr<tr::stream::stream_impl> const impl = mock->stream.impl ();
    session.reset (new ss::stream_session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::stream::byte_stream (impl); }, options_on (clock)));
  }

  ~stream_bench_t ()
  {
    session.reset ();
    settle ();
    mock.reset ();
    settle ();
  }

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (10000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  void
  open ()
  {
    session->start ();
    settle ();
    advance (ms (200));
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<ss::stream_session> session;
};

struct message_bench_t
{
  message_bench_t ()
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    mock.reset (
        new tr::testing::mock_datagram (tr::testing::make_mock_datagram (
            context.get_executor (), mock_options)));
    std::shared_ptr<tr::datagram::message_impl> const impl
        = mock->transport.impl ();
    session.reset (new ss::message_session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::datagram::message_transport (impl); },
        options_on (clock)));
  }

  ~message_bench_t ()
  {
    session.reset ();
    settle ();
    mock.reset ();
    settle ();
  }

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (10000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  void
  open ()
  {
    session->start ();
    settle ();
    advance (ms (200));
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_datagram> mock;
  std::unique_ptr<ss::message_session> session;
};

TEST (LumexSessionStream, AsByteStream_IsTheStateOfTheSession)
{
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.settle ();
  EXPECT_TRUE (view.impl ()->reconnects ());
  EXPECT_FALSE (view.is_open ());
  bench.open ();
  EXPECT_TRUE (view.is_open ());
  bench.mock->control.fail_opens_for (
      ms (1000), make_error_code (transport_errc::disconnected));
  std::size_t const epoch = view.impl ()->connection_epoch ();
  // The read lets the session see the loss.
  bool read_done = false;
  std::uint8_t byte = 0;
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code, std::size_t)
                          { read_done = true; });
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_TRUE (read_done);
  EXPECT_FALSE (view.is_open ());
  EXPECT_EQ (view.impl ()->connection_epoch (), epoch + 1);
  bench.advance (ms (2000));
  EXPECT_TRUE (view.is_open ());
  EXPECT_EQ (view.impl ()->connection_epoch (), epoch + 1);
}

TEST (LumexSessionStream, AsByteStream_ASessionOpenBeforeTheViewReadsOpen)
{
  stream_bench_t bench;
  bench.open ();
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  EXPECT_TRUE (view.is_open ());
  bench.settle ();
  EXPECT_TRUE (view.is_open ());
}

TEST (LumexSessionStream, AsByteStream_MadeWhileTheSessionClosesReadsItsState)
{
  // Falsified by a view that only hears the changes after it was added: the
  // close whose step runs between its making and that step would be lost.
  stream_bench_t bench;
  bench.open ();
  bench.session->close ();
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  EXPECT_TRUE (view.is_open ());
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
  EXPECT_FALSE (view.is_open ());
}

TEST (LumexSessionStream, AsByteStream_OpenStartsCloseCloses)
{
  // Falsified by a view that keeps a state of its own.
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  std::error_code opened = make_error_code (transport_errc::timeout);
  int calls = 0;
  view.async_open (
      [&] (std::error_code error)
        {
          ++calls;
          opened = error;
        });
  EXPECT_EQ (calls, 0);
  EXPECT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_FALSE (opened);
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  view.close ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
  EXPECT_FALSE (view.is_open ());
}

TEST (LumexSessionStream, AsByteStream_FrontGoneEndsTheStream)
{
  stream_bench_t bench;
  bench.open ();
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.session.reset ();
  bench.settle ();
  EXPECT_FALSE (view.is_open ());
  std::error_code error;
  std::uint8_t byte = 0;
  bool done = false;
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code ended, std::size_t)
                          {
                            error = ended;
                            done = true;
                          });
  bench.settle ();
  EXPECT_TRUE (done);
  EXPECT_EQ (error, transport_errc::not_open);
}

TEST (LumexSessionStream, AsByteStream_AMovedFromSessionGivesAnEmptyStream)
{
  stream_bench_t bench;
  ss::stream_session taken (std::move (*bench.session));
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  EXPECT_EQ (view.impl (), nullptr);
}

TEST (LumexSessionStream, AsByteStream_BytesOfAConnectionThatIsOverAreDropped)
{
  // The read took its bytes just as the session gave the link up: they would
  // glue to the first frame of the next connection.
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.open ();
  std::uint8_t buffer[8] = { 0 };
  std::error_code error;
  std::size_t bytes = 99;
  view.async_read_some (tr::mutable_bytes_t (buffer, 8),
                        [&] (std::error_code ended, std::size_t count)
                          {
                            error = ended;
                            bytes = count;
                          });
  bench.settle ();
  bench.mock->control.feed ({ 1, 2, 3 });
  view.impl ()->drop_connection ();
  bench.settle ();
  EXPECT_EQ (error, transport_errc::cancelled);
  EXPECT_EQ (bytes, 0u);
}

TEST (LumexSessionStream,
      AsMessageTransport_AMessageOfAConnectionThatIsOverIsDropped)
{
  message_bench_t bench;
  tr::datagram::message_transport view
      = bench.session->as_message_transport ();
  bench.open ();
  std::error_code error;
  std::vector<std::uint8_t> got = { 9 };
  view.async_receive (
      [&] (std::error_code ended, tr::datagram::message_t message)
        {
          error = ended;
          got = message.payload;
        });
  bench.settle ();
  tr::datagram::message_t old;
  old.payload = { 1 };
  bench.mock->control.deliver (old);
  view.impl ()->drop_connection ();
  bench.settle ();
  EXPECT_EQ (error, transport_errc::cancelled);
  EXPECT_TRUE (got.empty ());
}

TEST (LumexSessionStream, CancelEndsTheIoAndNotTheConnecting)
{
  // Falsified by a cancel that goes to the session: it would end in closed.
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.open ();
  std::error_code error;
  bool done = false;
  std::uint8_t byte = 0;
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code ended, std::size_t)
                          {
                            error = ended;
                            done = true;
                          });
  bench.settle ();
  view.cancel ();
  bench.settle ();
  EXPECT_TRUE (done);
  EXPECT_EQ (error, transport_errc::cancelled);
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  bench.mock->control.fail_opens_for (
      ms (1000), make_error_code (transport_errc::disconnected));
  done = false;
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code, std::size_t) { done = true; });
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::reconnecting);
  view.cancel ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::reconnecting);
}

TEST (LumexSessionStream, DropConnectionReconnects)
{
  // Falsified by a drop that closes.
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.open ();
  std::size_t const opens = bench.mock->control.open_count ();
  std::size_t const epoch = view.impl ()->connection_epoch ();
  view.impl ()->drop_connection ();
  bench.settle ();
  EXPECT_EQ (view.impl ()->connection_epoch (), epoch + 1);
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (bench.mock->control.open_count (), opens + 1);
  EXPECT_EQ (bench.session->session_statistics ().reconnects, 1u);
}

TEST (LumexSessionStream, DropConnectionOfASessionNotOpenDoesNothing)
{
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.settle ();
  view.impl ()->drop_connection ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
  EXPECT_EQ (bench.mock->control.open_count (), 0u);
}

TEST (LumexSessionStream, AStreamThatDoesNotReconnectSaysSoAndCloses)
{
  // The defaults of stream_impl, for the links that ask.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  EXPECT_FALSE (mock.stream.impl ()->reconnects ());
  mock.stream.impl ()->watch_connection (tr::connection_handler_t ());
  bool opened = false;
  mock.stream.async_open ([&] (std::error_code) { opened = true; });
  context.poll ();
  context.restart ();
  EXPECT_TRUE (opened);
  EXPECT_TRUE (mock.stream.is_open ());
  mock.stream.impl ()->drop_connection ();
  context.poll ();
  context.restart ();
  EXPECT_FALSE (mock.stream.is_open ());
}

TEST (LumexSessionStream, WatchConnection_HearsEveryChangeInOrderNeverInline)
{
  // Falsified by an event posted after the read's end, or told inline.
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.settle ();
  std::vector<tr::connection_event> events;
  std::vector<std::error_code> reasons;
  view.impl ()->watch_connection (
      [&] (tr::connection_event event, std::error_code const &reason)
        {
          events.push_back (event);
          reasons.push_back (reason);
        });
  EXPECT_TRUE (events.empty ());
  bench.session->start ();
  EXPECT_TRUE (events.empty ());
  bench.settle ();
  bench.advance (ms (200));
  ASSERT_EQ (events.size (), 1u);
  EXPECT_EQ (events[0], tr::connection_event::opened);
  EXPECT_FALSE (reasons[0]);
  // A read sees the loss: the event comes first.
  std::size_t at_read = 99;
  std::uint8_t byte = 0;
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code, std::size_t)
                          { at_read = events.size (); });
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  ASSERT_EQ (events.size (), 3u);
  EXPECT_EQ (events[1], tr::connection_event::lost);
  EXPECT_EQ (reasons[1], transport_errc::disconnected);
  EXPECT_EQ (events[2], tr::connection_event::opened);
  EXPECT_GE (at_read, 2u);
  bench.session->close ();
  bench.settle ();
  ASSERT_EQ (events.size (), 4u);
  EXPECT_EQ (events[3], tr::connection_event::gave_up);
  // An empty handler removes it.
  view.impl ()->watch_connection (tr::connection_handler_t ());
  bench.session->start ();
  bench.settle ();
  EXPECT_EQ (events.size (), 4u);
}

TEST (LumexSessionStream, WatchConnection_ASessionThatFaultsGivesUp)
{
  stream_bench_t bench;
  ss::session_options_t options = options_on (bench.clock);
  options.policy.max_attempts = 1;
  std::shared_ptr<tr::stream::stream_impl> const impl
      = bench.mock->stream.impl ();
  bench.session.reset (new ss::stream_session (
      bench.context.get_executor (), [impl] (tr::executor_type const &)
        { return tr::stream::byte_stream (impl); }, options));
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.settle ();
  std::vector<tr::connection_event> events;
  view.impl ()->watch_connection (
      [&] (tr::connection_event event, std::error_code const &)
        { events.push_back (event); });
  bench.mock->control.fail_opens_for (
      ms (100000), make_error_code (transport_errc::disconnected));
  bench.session->start ();
  bench.settle ();
  ASSERT_EQ (events.size (), 1u);
  EXPECT_EQ (events[0], tr::connection_event::gave_up);
  EXPECT_EQ (bench.session->state (), ss::channel_state::faulted);
}

// A handler whose destructor calls the view back: under its lock it would wait
// for itself.
struct calls_back_t
{
  std::shared_ptr<tr::stream::stream_impl> impl;
  bool armed;

  calls_back_t (std::shared_ptr<tr::stream::stream_impl> made, bool arm)
      : impl (std::move (made)), armed (arm)
  {
  }

  calls_back_t (calls_back_t const &other)
      : impl (other.impl), armed (other.armed)
  {
  }

  ~calls_back_t ()
  {
    if (armed)
      impl->watch_connection (tr::connection_handler_t ());
  }

  void
  operator() (tr::connection_event, std::error_code const &) const
  {
  }
};

TEST (LumexSessionStream, WatchConnection_AReplacedHandlerMayCallBack)
{
  stream_bench_t bench;
  tr::stream::byte_stream view = bench.session->as_byte_stream ();
  bench.settle ();
  std::shared_ptr<tr::stream::stream_impl> const impl = view.impl ();
  view.impl ()->watch_connection (calls_back_t (impl, true));
  view.impl ()->watch_connection (tr::connection_handler_t ());
  SUCCEED ();
}

TEST (LumexSessionStream, AHandlerKeptByAViewIsNoLeakAtTheEndOfTheContext)
{
  // LeakSanitizer: the handler owns the view, which owns the session.
  auto bench = std::unique_ptr<stream_bench_t> (new stream_bench_t ());
  tr::stream::byte_stream view = bench->session->as_byte_stream ();
  bench->open ();
  std::shared_ptr<tr::stream::stream_impl> const impl = view.impl ();
  std::weak_ptr<tr::stream::stream_impl> const watched = impl;
  bool read_ended = false;
  std::uint8_t byte = 0;
  // A pending operation enlists the view with the shutdown.
  view.async_read_some (tr::mutable_bytes_t (&byte, 1),
                        [&] (std::error_code, std::size_t)
                          { read_ended = true; });
  impl->watch_connection (calls_back_t (impl, false));
  bench->settle ();
  EXPECT_FALSE (watched.expired ());
  (void)read_ended;
}

TEST (LumexSessionStream, FramedOverAView_APartialFrameEndsWithTheConnection)
{
  // The epoch: half a frame of the old connection glued to the new one would
  // give a frame that fails its check or a wrong one.
  stream_bench_t bench;
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      bench.session->as_byte_stream (), fr::slip::slip_codec ());
  bench.open ();
  std::vector<std::vector<std::uint8_t>> frames;
  std::function<void ()> read;
  read = [&] ()
    {
      framed.async_read_frame (
          [&] (std::error_code error, tr::framed::frame_t frame)
            {
              if (!error)
                {
                  frames.push_back (frame);
                  read ();
                }
            });
    };
  read ();
  bench.settle ();
  std::vector<std::uint8_t> half = slip_of ({ 1, 2, 3 });
  half.pop_back ();
  bench.mock->control.feed (half);
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  read ();
  bench.settle ();
  bench.mock->control.feed (slip_of ({ 9, 8 }));
  bench.settle ();
  ASSERT_EQ (frames.size (), 1u);
  EXPECT_EQ (frames[0], (std::vector<std::uint8_t>{ 9, 8 }));
  EXPECT_EQ (framed.statistics ().frames_in, 1u);
}

TEST (LumexSessionStream, FramedOverAView_QueuedFramesEndWithTheConnection)
{
  // Falsified by a queue that keeps them: the reply of the old device answers
  // a request on the new connection.
  stream_bench_t bench;
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      bench.session->as_byte_stream (), fr::slip::slip_codec ());
  bench.open ();
  framed.start ();
  bench.settle ();
  bench.mock->control.feed (slip_of ({ 1 }));
  bench.settle ();
  EXPECT_EQ (framed.queued_frames (), 1u);
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  std::vector<std::uint8_t> got;
  std::error_code error;
  bool done = false;
  framed.async_read_frame (
      [&] (std::error_code ended, tr::framed::frame_t frame)
        {
          error = ended;
          got = frame;
          done = true;
        });
  bench.settle ();
  EXPECT_FALSE (done);
  bench.mock->control.feed (slip_of ({ 2 }));
  bench.settle ();
  ASSERT_TRUE (done);
  EXPECT_FALSE (error) << error.message ();
  EXPECT_EQ (got, (std::vector<std::uint8_t>{ 2 }));
}

TEST (LumexSessionStream, FramedOverAView_AQueuedWriteIsNotCarriedOver)
{
  // Falsified by an adapter that goes on with its queue: a request the old
  // connection did not take goes out on the new one, past every check.
  stream_bench_t bench;
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      bench.session->as_byte_stream (), fr::slip::slip_codec ());
  bench.open ();
  framed.start ();
  bench.settle ();
  bench.mock->control.hold_writes ();
  std::vector<std::uint8_t> const first = { 1 };
  std::vector<std::uint8_t> const second = { 2 };
  std::error_code first_error;
  std::error_code second_error;
  int ended = 0;
  framed.async_write_frame (tr::const_bytes_t (first.data (), first.size ()),
                            [&] (std::error_code error)
                              {
                                first_error = error;
                                ++ended;
                              });
  framed.async_write_frame (tr::const_bytes_t (second.data (), second.size ()),
                            [&] (std::error_code error)
                              {
                                second_error = error;
                                ++ended;
                              });
  bench.settle ();
  EXPECT_EQ (ended, 0);
  bench.mock->control.disconnect ();
  bench.settle ();
  bench.mock->control.release_writes ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (ended, 2);
  EXPECT_TRUE (first_error);
  EXPECT_EQ (second_error, transport_errc::cancelled);
  EXPECT_TRUE (bench.mock->control.written ().empty ());
  // A frame of the new connection goes.
  std::vector<std::uint8_t> const third = { 3 };
  bool wrote = false;
  framed.async_write_frame (tr::const_bytes_t (third.data (), third.size ()),
                            [&] (std::error_code error)
                              {
                                EXPECT_FALSE (error) << error.message ();
                                wrote = true;
                              });
  bench.settle ();
  EXPECT_TRUE (wrote);
  EXPECT_EQ (bench.mock->control.written (), slip_of (third));
}

TEST (LumexSessionStream, FramedOverAMock_APartialFrameEndsWithCloseAndOpen)
{
  // The same on a mock opened again by hand: every close counts.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
  tr::stream::byte_stream shared (impl);
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      std::move (shared), fr::slip::slip_codec ());
  bool opened = false;
  framed.transport ().async_open ([&] (std::error_code) { opened = true; });
  context.poll ();
  context.restart ();
  ASSERT_TRUE (opened);
  std::vector<std::vector<std::uint8_t>> frames;
  std::function<void ()> read;
  read = [&] ()
    {
      framed.async_read_frame (
          [&] (std::error_code error, tr::framed::frame_t frame)
            {
              if (!error)
                frames.push_back (frame);
              read ();
            });
    };
  framed.start ();
  context.poll ();
  context.restart ();
  std::vector<std::uint8_t> half = slip_of ({ 1, 2, 3 });
  half.pop_back ();
  mock.control.feed (half);
  context.poll ();
  context.restart ();
  framed.transport ().close ();
  framed.transport ().async_open ([&] (std::error_code) {});
  context.poll ();
  context.restart ();
  mock.control.feed (slip_of ({ 7 }));
  tr::framed::frame_t got;
  bool done = false;
  framed.async_read_frame (
      [&] (std::error_code, tr::framed::frame_t frame)
        {
          got = frame;
          done = true;
        });
  context.poll ();
  ASSERT_TRUE (done);
  EXPECT_EQ (got, (tr::framed::frame_t{ 7 }));
  framed.close ();
  context.restart ();
  context.poll ();
}

TEST (LumexSessionStream, FramedOverAMock_QueuedFramesStayAfterAClose)
{
  // Only a transport that connects again by itself takes them: what a plain
  // one decoded before its close is still read.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
  tr::stream::byte_stream shared (impl);
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      std::move (shared), fr::slip::slip_codec ());
  framed.transport ().async_open ([] (std::error_code) {});
  framed.start ();
  context.poll ();
  context.restart ();
  mock.control.feed (slip_of ({ 7 }));
  context.poll ();
  context.restart ();
  EXPECT_EQ (framed.queued_frames (), 1u);
  framed.transport ().close ();
  context.poll ();
  context.restart ();
  tr::framed::frame_t got;
  framed.async_read_frame ([&] (std::error_code, tr::framed::frame_t frame)
                             { got = frame; });
  context.poll ();
  EXPECT_EQ (got, (tr::framed::frame_t{ 7 }));
  framed.close ();
  context.restart ();
  context.poll ();
}

TEST (LumexSessionStream, AMockForgetsUnreadBytesAtAClose)
{
  // Falsified by a mock that keeps them: a partial frame of the old connection
  // is read on the new one, past the epoch.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  bool opened = false;
  mock.stream.async_open ([&] (std::error_code) { opened = true; });
  context.poll ();
  context.restart ();
  ASSERT_TRUE (opened);
  mock.control.feed ({ 1, 2, 3 });
  context.poll ();
  context.restart ();
  mock.stream.close ();
  mock.stream.async_open ([&] (std::error_code) {});
  context.poll ();
  context.restart ();
  mock.control.feed ({ 7 });
  std::uint8_t buffer[8] = { 0 };
  std::size_t bytes = 0;
  mock.stream.async_read_some (tr::mutable_bytes_t (buffer, 8),
                               [&] (std::error_code, std::size_t count)
                                 { bytes = count; });
  context.poll ();
  EXPECT_EQ (bytes, 1u);
  EXPECT_EQ (buffer[0], 7);
}

TEST (LumexSessionStream, AMockDatagramForgetsUnreadMessagesAtAClose)
{
  asio::io_context context;
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (context.get_executor ());
  mock.transport.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  tr::datagram::message_t old;
  old.payload = { 1, 2, 3 };
  mock.control.deliver (old);
  context.poll ();
  context.restart ();
  mock.transport.close ();
  mock.transport.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  tr::datagram::message_t fresh;
  fresh.payload = { 7 };
  mock.control.deliver (fresh);
  std::vector<std::uint8_t> got;
  mock.transport.async_receive (
      [&] (std::error_code, tr::datagram::message_t message)
        { got = message.payload; });
  context.poll ();
  EXPECT_EQ (got, (std::vector<std::uint8_t>{ 7 }));
}

TEST (LumexSessionStream, AsMessageTransport_IsTheSessionWithItsEpoch)
{
  message_bench_t bench;
  tr::datagram::message_transport view
      = bench.session->as_message_transport ();
  bench.settle ();
  EXPECT_TRUE (view.impl ()->reconnects ());
  std::vector<tr::connection_event> events;
  view.impl ()->watch_connection (
      [&] (tr::connection_event event, std::error_code const &)
        { events.push_back (event); });
  bench.open ();
  EXPECT_TRUE (view.is_open ());
  ASSERT_EQ (events.size (), 1u);
  std::size_t const epoch = view.impl ()->connection_epoch ();
  std::error_code error;
  bool done = false;
  view.async_receive (
      [&] (std::error_code ended, tr::datagram::message_t)
        {
          error = ended;
          done = true;
        });
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_TRUE (done);
  EXPECT_EQ (view.impl ()->connection_epoch (), epoch + 1);
  EXPECT_TRUE (view.is_open ());
  ASSERT_EQ (events.size (), 3u);
  EXPECT_EQ (events[1], tr::connection_event::lost);
  bench.advance (ms (200));
  view.impl ()->drop_connection ();
  bench.settle ();
  EXPECT_EQ (view.impl ()->connection_epoch (), epoch + 2);
  EXPECT_EQ (bench.session->session_statistics ().reconnects, 2u);
  tr::datagram::message_t message;
  message.payload = { 1 };
  bool sent = false;
  view.async_send (message,
                   [&] (std::error_code ended)
                     {
                       error = ended;
                       sent = true;
                     });
  bench.settle ();
  EXPECT_TRUE (sent);
  EXPECT_FALSE (error) << error.message ();
  EXPECT_EQ (bench.mock->control.sent ().size (), 1u);
  std::error_code closed_error;
  view.close ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
  view.async_open ([&] (std::error_code ended) { closed_error = ended; });
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
}

TEST (LumexSessionStream,
      FramedDatagramOverAView_AMessageOfTheOldConnectionIsGone)
{
  // A frame decoded before the loss is not handed on the new connection.
  message_bench_t bench;
  tr::framed::framed_datagram<fr::slip::slip_codec> framed (
      bench.session->as_message_transport (), fr::slip::slip_codec ());
  bench.open ();
  framed.start ();
  bench.settle ();
  tr::datagram::message_t old;
  old.payload = slip_of ({ 1 });
  bench.mock->control.deliver (old);
  bench.settle ();
  EXPECT_EQ (framed.queued_frames (), 1u);
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  std::vector<std::uint8_t> got;
  bool done = false;
  framed.async_read_frame (
      [&] (std::error_code, tr::datagram::message_t frame)
        {
          got = frame.payload;
          done = true;
        });
  bench.settle ();
  EXPECT_FALSE (done);
  tr::datagram::message_t fresh;
  fresh.payload = slip_of ({ 2 });
  bench.mock->control.deliver (fresh);
  bench.settle ();
  ASSERT_TRUE (done);
  EXPECT_EQ (got, (std::vector<std::uint8_t>{ 2 }));
}
} // namespace

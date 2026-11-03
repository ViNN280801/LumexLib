// The device simulator on a bare mock, with no dialogue engine: bytes and
// datagrams written by hand (split, joined, damaged, of another report), the
// life of the simulator (destroyed inside a handler or a generator, while a
// stream runs, after or before its io_context, a handler that throws, no
// capture cycle) and its use from other threads. The lab table of the
// simulator tests.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace fr = lumex::core::framing;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::simulator_tables;
using lab = tables::lab_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using device_t = tr::testing::device_simulator<lab>;
using tables::bytes;

bytes_t
framed (bytes_t const &payload)
{
  bytes_t wire;
  fr::slip::slip_codec const codec;
  EXPECT_TRUE (
      codec
          .encode (fr::codec::bytes_view_t (payload.data (), payload.size ()),
                   wire)
          .has_value ());
  return wire;
}

// The frames the device sent, from the bytes read so far.
std::vector<bytes_t>
frames_of (bytes_t const &wire)
{
  std::vector<bytes_t> out;
  fr::slip::slip_codec codec;
  codec.decode (fr::codec::bytes_view_t (wire.data (), wire.size ()),
                [&out] (fr::codec::bytes_view_t frame)
                  { out.push_back (bytes_t (frame.begin (), frame.end ())); });
  return out;
}

void
drain (asio::io_context &context)
{
  for (;;)
    {
      if (context.stopped ())
        context.restart ();
      if (context.poll () == 0)
        return;
    }
}

// A mock stream with a simulator on it; the test writes request frames and
// reads what the device sent.
class raw_bench_t
{
public:
  raw_bench_t ()
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (
        tr::testing::make_mock_stream (context.get_executor (), options)));
    mock->stream.async_open ([] (std::error_code) {});
    drain (context);
    device.reset (
        new device_t (mock->control, fr::slip::slip_codec (), clock));
    read_again ();
    drain (context);
  }

  ~raw_bench_t ()
  {
    stopped = true;
    mock->stream.close ();
    device.reset ();
    drain (context);
  }

  // Writes `wire` as one write.
  void
  write (bytes_t const &wire)
  {
    std::shared_ptr<bytes_t> const kept = std::make_shared<bytes_t> (wire);
    mock->stream.async_write_all (
        tr::const_bytes_t (kept->data (), kept->size ()),
        [kept] (std::error_code error, std::size_t) { EXPECT_FALSE (error); });
    drain (context);
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  std::vector<bytes_t>
  replies () const
  {
    return frames_of (received);
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<device_t> device;
  bytes_t received;
  bool stopped = false;

private:
  void
  read_again ()
  {
    mock->stream.async_read_some (
        tr::mutable_bytes_t (chunk, sizeof (chunk)),
        [this] (std::error_code error, std::size_t count)
          {
            if (error || stopped)
              return;
            received.insert (received.end (), chunk, chunk + count);
            read_again ();
          });
  }

  std::uint8_t chunk[512];
};

// A mock datagram transport with a simulator on it.
class datagram_bench_t
{
public:
  explicit datagram_bench_t (std::uint8_t report_id)
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_datagram (
        tr::testing::make_mock_datagram (context.get_executor (), options)));
    mock->transport.async_open ([] (std::error_code) {});
    drain (context);
    device.reset (new device_t (mock->control, fr::slip::slip_codec (),
                                report_id, clock));
    receive_again ();
    drain (context);
  }

  ~datagram_bench_t ()
  {
    stopped = true;
    mock->transport.close ();
    device.reset ();
    drain (context);
  }

  void
  send (std::uint8_t report_id, bytes_t const &payload)
  {
    tr::datagram::message_t message;
    message.report_id = report_id;
    message.payload = payload;
    mock->transport.async_send (std::move (message), [] (std::error_code error)
                                  { EXPECT_FALSE (error); });
    drain (context);
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_datagram> mock;
  std::unique_ptr<device_t> device;
  std::vector<tr::datagram::message_t> received;
  bool stopped = false;

private:
  void
  receive_again ()
  {
    mock->transport.async_receive (
        [this] (std::error_code error, tr::datagram::message_t message)
          {
            if (error || stopped)
              return;
            received.push_back (std::move (message));
            receive_again ();
          });
  }
};

bytes_t
info_request ()
{
  return bytes ({ 0x10 });
}
} // namespace

// --- the wire

TEST (LumexDeviceSimulatorWire, Simulator_RequestSplitInManyWritesIsAnswered)
{
  raw_bench_t bench;
  bytes_t const wire = framed (info_request ());
  for (std::size_t i = 0; i < wire.size (); ++i)
    bench.write (bytes_t (1, wire[i]));
  ASSERT_EQ (bench.replies ().size (), 1u);
  bytes_t sample;
  layout::sample_reply_message<lab, lab::INFO_t> (sample);
  EXPECT_EQ (bench.replies ()[0], sample);
}

TEST (LumexDeviceSimulatorWire, Simulator_TwoRequestsInOneWriteAreBothAnswered)
{
  raw_bench_t bench;
  bytes_t wire = framed (info_request ());
  bytes_t const second = framed (bytes ({ 0x22 }));
  wire.insert (wire.end (), second.begin (), second.end ());
  bench.write (wire);
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 2u);
  EXPECT_EQ (replies[1], bytes ({ 0x06 }));
  EXPECT_EQ (bench.device->statistics ().requests, 2u);
}

TEST (LumexDeviceSimulatorWire, Simulator_NoiseAndDamagedFramesAreCounted)
{
  raw_bench_t bench;
  // A frame longer than any the codec keeps, with no delimiter in it.
  bench.write (bytes_t (70000, 0xAB));
  bench.write (bytes ({ 0xC0 }));
  EXPECT_GE (bench.device->statistics ().framing_errors, 1u);
  EXPECT_TRUE (bench.replies ().empty ());
  // Bytes of every value, in short frames: the device survives them and
  // still answers the next good request.
  bytes_t noise;
  for (int value = 0; value < 256; ++value)
    {
      noise.push_back (static_cast<std::uint8_t> (value));
      if (value % 7 == 0)
        noise.push_back (0xC0);
    }
  bench.write (noise);
  bench.write (framed (info_request ()));
  ASSERT_FALSE (bench.replies ().empty ());
  bytes_t sample;
  layout::sample_reply_message<lab, lab::INFO_t> (sample);
  EXPECT_EQ (bench.replies ().back (), sample);
}

TEST (LumexDeviceSimulatorWire,
      Simulator_DatagramOfAnotherReportIsNotForTheDevice)
{
  datagram_bench_t bench (7);
  bench.send (8, framed (info_request ()));
  EXPECT_TRUE (bench.received.empty ());
  EXPECT_EQ (bench.device->statistics ().foreign_reports, 1u);
  bench.send (7, framed (info_request ()));
  ASSERT_EQ (bench.received.size (), 1u);
  EXPECT_EQ (bench.received[0].report_id, 7);
  EXPECT_EQ (bench.device->statistics ().requests, 1u);
}

TEST (LumexDeviceSimulatorWire,
      Simulator_DatagramHoldsWholeFramesAndTheCodecStartsAfresh)
{
  datagram_bench_t bench (3);
  // Two frames in one datagram: two replies, in two datagrams.
  bytes_t two = framed (info_request ());
  bytes_t const stop = framed (bytes ({ 0x22 }));
  two.insert (two.end (), stop.begin (), stop.end ());
  bench.send (3, two);
  EXPECT_EQ (bench.received.size (), 2u);
  // The head of a frame in one datagram is not completed by the next one.
  bytes_t const whole = framed (info_request ());
  bench.send (3, bytes_t (whole.begin (), whole.begin () + 2));
  bench.send (3, bytes_t (whole.begin () + 2, whole.end ()));
  EXPECT_EQ (bench.received.size (), 2u);
  EXPECT_GE (bench.device->statistics ().framing_errors, 1u);
  // An empty datagram is nothing.
  bench.send (3, bytes_t ());
  EXPECT_EQ (bench.received.size (), 2u);
}

// --- the life of the simulator

TEST (LumexDeviceSimulatorLifetime,
      Simulator_DestroyedInsideAHandlerAnswersNothing)
{
  raw_bench_t bench;
  bench.device->on<lab::INFO_t> (
      [&bench] (layout::request_of<lab::INFO_t>::type const &)
        { bench.device.reset (); });
  bench.write (framed (info_request ()));
  bench.advance (ms (100));
  EXPECT_FALSE (bench.device);
  EXPECT_TRUE (bench.replies ().empty ());
  // The mock is free of the observer: a further write is just a write.
  bench.write (framed (info_request ()));
  EXPECT_TRUE (bench.replies ().empty ());
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_DestroyedInsideAStreamGeneratorEndsTheStream)
{
  raw_bench_t bench;
  int made = 0;
  bench.device->start_stream_with<lab::LEVEL_t> (
      [&bench, &made] ()
        {
          if (++made == 2)
            bench.device.reset ();
          return std::uint8_t (1);
        });
  bench.advance (ms (2000));
  EXPECT_EQ (made, 2);
  EXPECT_FALSE (bench.device);
  // The first message went out; the one being made when the device died did
  // not.
  EXPECT_EQ (bench.replies ().size (), 1u);
  EXPECT_EQ (bench.clock.pending (), 0u);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_DestroyedWhileAStreamRunsLeavesNoWait)
{
  raw_bench_t bench;
  bench.device->start_streams ();
  bench.device->after (ms (5000), [] () {});
  bench.advance (ms (350));
  std::size_t const before = bench.replies ().size ();
  EXPECT_GE (before, 3u);
  bench.device.reset ();
  bench.advance (ms (6000));
  EXPECT_EQ (bench.replies ().size (), before);
  EXPECT_EQ (bench.clock.pending (), 0u);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_HandlerThatThrowsIsCountedAndTheDeviceGoesOn)
{
  raw_bench_t bench;
  bool throw_now = true;
  bench.device->on<lab::INFO_t> (
      [&throw_now] (layout::request_of<lab::INFO_t>::type const &)
        {
          if (throw_now)
            throw std::runtime_error ("handler");
        });
  bytes_t const wire = framed (info_request ());
  std::shared_ptr<bytes_t> const kept = std::make_shared<bytes_t> (wire);
  bench.mock->stream.async_write_all (
      tr::const_bytes_t (kept->data (), kept->size ()),
      [kept] (std::error_code, std::size_t) {});
  // The exception leaves the run () of the strand, as the mock documents for
  // an observer that throws; the write itself completed.
  EXPECT_THROW (drain (bench.context), std::runtime_error);
  drain (bench.context);
  tr::testing::simulator_statistics_t counted = bench.device->statistics ();
  EXPECT_EQ (counted.handler_errors, 1u);
  EXPECT_EQ (counted.replies, 0u);
  EXPECT_TRUE (bench.replies ().empty ());
  throw_now = false;
  bench.write (wire);
  EXPECT_EQ (bench.replies ().size (), 1u);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_ThrowingHandlerDoesNotTakeTheOtherFramesOfTheChunk)
{
  raw_bench_t bench;
  bench.device->on<lab::STOP_t> (
      [] (layout::request_of<lab::STOP_t>::type const &)
        { throw std::runtime_error ("first frame"); });
  bytes_t wire = framed (bytes ({ 0x22 }));
  bytes_t const second = framed (info_request ());
  wire.insert (wire.end (), second.begin (), second.end ());
  std::shared_ptr<bytes_t> const kept = std::make_shared<bytes_t> (wire);
  bench.mock->stream.async_write_all (
      tr::const_bytes_t (kept->data (), kept->size ()),
      [kept] (std::error_code, std::size_t) {});
  EXPECT_THROW (drain (bench.context), std::runtime_error);
  drain (bench.context);
  // The second request of the chunk was served all the same.
  EXPECT_EQ (bench.replies ().size (), 1u);
  EXPECT_EQ (bench.device->statistics ().handler_errors, 1u);
  EXPECT_EQ (bench.device->statistics ().requests, 2u);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_ThrowingGeneratorDoesNotEndTheStream)
{
  raw_bench_t bench;
  int calls = 0;
  bench.device->start_stream_with<lab::LEVEL_t> (
      [&calls] ()
        {
          if (++calls == 1)
            throw std::runtime_error ("generator");
          return std::uint8_t (4);
        });
  EXPECT_THROW (bench.advance (ms (250)), std::runtime_error);
  bench.advance (ms (1000));
  EXPECT_GE (calls, 2);
  EXPECT_FALSE (bench.replies ().empty ());
  EXPECT_EQ (bench.device->statistics ().handler_errors, 1u);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_HandlersAreReleasedWithTheSimulator)
{
  std::shared_ptr<int> const token = std::make_shared<int> (0);
  {
    raw_bench_t bench;
    bench.device->on<lab::INFO_t> (
        [token] (layout::request_of<lab::INFO_t>::type const &) {});
    bench.device->start_stream_with<lab::LEVEL_t> (
        [token] () { return std::uint8_t (0); });
    bench.device->after (ms (10), [token] () {});
    EXPECT_GT (token.use_count (), 1);
    bench.device.reset ();
    EXPECT_EQ (token.use_count (), 1);
  }
  EXPECT_EQ (token.use_count (), 1);
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_HandlersAreDroppedWhenTheIoContextShutsDown)
{
  std::weak_ptr<device_t> weak;
  std::shared_ptr<int> const token = std::make_shared<int> (0);
  {
    std::unique_ptr<asio::io_context> context (new asio::io_context ());
    tr::testing::virtual_clock clock;
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    std::shared_ptr<device_t> device;
    {
      tr::testing::mock_stream mock
          = tr::testing::make_mock_stream (context->get_executor (), options);
      mock.stream.async_open ([] (std::error_code) {});
      drain (*context);
      device.reset (
          new device_t (mock.control, fr::slip::slip_codec (), clock));
    }
    weak = device;
    // A cycle: the handler holds the simulator that holds the handler. And
    // a stream and a step in flight, with a function that holds the token.
    device->on<lab::INFO_t> (
        [device] (layout::request_of<lab::INFO_t>::type const &) {});
    device->start_stream_with<lab::LEVEL_t> ([token] ()
                                               { return std::uint8_t (0); });
    device->after (ms (100), [token] () {});
    device.reset ();
    EXPECT_FALSE (weak.expired ());
    EXPECT_GT (token.use_count (), 1);
    // The shutdown of the context drops what the simulator keeps, and with
    // it the last owner of the simulator.
    context.reset ();
    EXPECT_TRUE (weak.expired ());
    EXPECT_EQ (token.use_count (), 1);
  }
}

TEST (LumexDeviceSimulatorLifetime,
      Simulator_ConfiguredFromAnotherThreadWhileServing)
{
  raw_bench_t bench;
  std::atomic<bool> stop (false);
  std::thread configurer (
      [&bench, &stop] ()
        {
          int round = 0;
          while (!stop.load ())
            {
              bench.device->set_state (round % 3);
              bench.device->refuse<lab::STOP_t> (round % 2 == 0);
              bench.device->set_reply<lab::INFO_t> (
                  layout::reply_message_of<lab::INFO_t>::type ());
              bench.device->start_stream<lab::LEVEL_t> (ms (100 + round % 5));
              bench.device->set_behavior (tr::testing::behavior::perfect ());
              ++round;
            }
        });
  bytes_t const wire = framed (info_request ());
  for (int i = 0; i < 200; ++i)
    {
      bench.write (wire);
      bench.advance (ms (10));
    }
  stop.store (true);
  configurer.join ();
  EXPECT_GE (bench.device->statistics ().requests, 200u);
}

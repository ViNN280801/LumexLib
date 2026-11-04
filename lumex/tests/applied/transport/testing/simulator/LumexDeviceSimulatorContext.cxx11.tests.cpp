// The device simulator and the life of its io_context: a simulator that is
// the only owner of the control of its mock and dies after the context, or
// is used after the context is gone; functions that keep their owner alive;
// handlers and generators that keep a state of their own; streams of rows
// outside the session; stale ticks; steps of `after` that throw or destroy
// the simulator. The lab table of the simulator tests.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorRawBench.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace fr = lumex::core::framing;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::simulator_tables;
using lab = tables::lab_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using device_t = tr::testing::device_simulator<lab>;
using lumex_simulator_test::drain;
using lumex_simulator_test::framed;
using lumex_simulator_test::raw_bench_t;
using tables::bytes;
using tr::testing::behavior;

// The simulator is the only owner of the control of its mock: the mock itself
// dies in the inner scope.
std::shared_ptr<device_t>
make_alone (asio::io_context &context, tr::testing::virtual_clock &clock,
            bool virtual_timers = true)
{
  tr::testing::mock_options_t options;
  if (virtual_timers)
    options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), options);
  mock.stream.async_open ([] (std::error_code) {});
  drain (context);
  return std::make_shared<device_t> (mock.control, fr::slip::slip_codec (),
                                     clock);
}

struct counting_t
{
  int n = 0;

  std::uint8_t
  operator() (layout::request_of<lab::LEVEL_t>::type const &)
  {
    return static_cast<std::uint8_t> (++n);
  }
};
} // namespace

// --- the simulator and its io_context

TEST (LumexDeviceSimulatorContext, Simulator_PlainDestroyedAfterItsContext)
{
  tr::testing::virtual_clock clock;
  std::unique_ptr<asio::io_context> context (new asio::io_context ());
  std::shared_ptr<device_t> device = make_alone (*context, clock);
  context.reset ();
  device.reset ();
}

TEST (LumexDeviceSimulatorContext,
      Simulator_WithAStreamAndAStepDestroyedAfterItsContext)
{
  tr::testing::virtual_clock clock;
  std::unique_ptr<asio::io_context> context (new asio::io_context ());
  std::shared_ptr<device_t> device = make_alone (*context, clock);
  EXPECT_TRUE (device->start_stream<lab::LEVEL_t> (ms (10)));
  device->after (ms (10), [] () {});
  context.reset ();
  clock.advance (ms (100));
  device.reset ();
  EXPECT_EQ (clock.pending (), 0u);
}

TEST (LumexDeviceSimulatorContext,
      Simulator_OnASteadyMockDestroyedAfterItsContext)
{
  tr::testing::virtual_clock clock;
  std::unique_ptr<asio::io_context> context (new asio::io_context ());
  std::shared_ptr<device_t> device = make_alone (*context, clock, false);
  context.reset ();
  device.reset ();
}

TEST (LumexDeviceSimulatorContext,
      Simulator_UsedAfterItsContextIsGoneDoesNothing)
{
  tr::testing::virtual_clock clock;
  std::unique_ptr<asio::io_context> context (new asio::io_context ());
  std::shared_ptr<device_t> device = make_alone (*context, clock);
  context.reset ();
  bool ran = false;
  // Everything the interface offers: the streams and steps are refused, the
  // rest changes nothing that is served.
  EXPECT_FALSE (device->start_stream<lab::LEVEL_t> (ms (10)));
  EXPECT_FALSE (device->start_stream_with<lab::LEVEL_t> (
      [] () { return std::uint8_t (1); }, ms (10)));
  EXPECT_EQ (device->start_streams (), 0u);
  device->after (ms (10), [&ran] () { ran = true; });
  device->on<lab::INFO_t> (
      [] (layout::request_of<lab::INFO_t>::type const &) {});
  device->set_reply<lab::LEVEL_t> (std::uint8_t (1));
  device->refuse<lab::STOP_t> ();
  device->set_state (1);
  device->set_behavior (behavior::perfect ());
  device->stop_streams ();
  device->statistics ();
  device->reset_statistics ();
  clock.advance (ms (100));
  EXPECT_FALSE (ran);
  EXPECT_EQ (clock.pending (), 0u);
  device.reset ();
}

TEST (LumexDeviceSimulatorContext,
      Simulator_FunctionsThatOwnTheSimulatorAreDroppedWithTheContext)
{
  // One path at a time: each keeps the simulator alive only through the
  // function it gave, so only the drop at the shutdown frees it.
  for (int path = 0; path < 3; ++path)
    {
      tr::testing::virtual_clock clock;
      std::unique_ptr<asio::io_context> context (new asio::io_context ());
      std::shared_ptr<device_t> device = make_alone (*context, clock);
      std::weak_ptr<device_t> weak = device;
      if (path == 0)
        device->on<lab::INFO_t> (
            [device] (layout::request_of<lab::INFO_t>::type const &) {});
      else if (path == 1)
        device->start_stream_with<lab::LEVEL_t> (
            [device] () { return std::uint8_t (1); }, ms (10));
      else
        device->after (ms (10), [device] () {});
      device.reset ();
      EXPECT_FALSE (weak.expired ()) << path;
      context.reset ();
      EXPECT_TRUE (weak.expired ()) << path;
      clock.advance (ms (100));
    }
}

// --- state of the functions of the test

TEST (LumexDeviceSimulatorContext, Simulator_HandlerWithStateKeepsIt)
{
  raw_bench_t bench;
  bench.device->on<lab::LEVEL_t> (counting_t ());
  for (int i = 0; i < 3; ++i)
    bench.write (framed (bytes ({ 0x13 })));
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 3u);
  EXPECT_EQ (replies[0][1], 1);
  EXPECT_EQ (replies[1][1], 2);
  EXPECT_EQ (replies[2][1], 3);
}

TEST (LumexDeviceSimulatorContext, Simulator_GeneratorWithStateKeepsIt)
{
  raw_bench_t bench;
  int n = 0;
  bench.device->start_stream_with<lab::LEVEL_t> (
      [n] () mutable { return static_cast<std::uint8_t> (++n); }, ms (10));
  bench.advance (ms (30));
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_GE (replies.size (), 3u);
  EXPECT_EQ (replies[0][1], 1);
  EXPECT_EQ (replies[1][1], 2);
  EXPECT_EQ (replies[2][1], 3);
}

// --- the streams

TEST (LumexDeviceSimulatorContext,
      Simulator_StaleTickOfARestartedStreamSendsNothing)
{
  raw_bench_t bench;
  bench.device->start_stream<lab::LEVEL_t> (ms (100));
  drain (bench.context);
  // The wait is due: its completion is posted and not run yet.
  bench.clock.advance (ms (100));
  bench.device->start_stream<lab::LEVEL_t> (ms (100));
  drain (bench.context);
  EXPECT_EQ (bench.replies ().size (), 0u);
  bench.advance (ms (100));
  EXPECT_EQ (bench.replies ().size (), 1u);
}

TEST (LumexDeviceSimulatorContext,
      Simulator_StaleTickOfAStoppedStreamSendsNothing)
{
  raw_bench_t bench;
  bench.device->start_stream<lab::LEVEL_t> (ms (100));
  drain (bench.context);
  bench.clock.advance (ms (100));
  bench.device->stop_stream<lab::LEVEL_t> ();
  drain (bench.context);
  EXPECT_EQ (bench.replies ().size (), 0u);
}

TEST (LumexDeviceSimulatorContext,
      Simulator_StreamOfARowOutsideTheSessionSendsNothing)
{
  // The session is set before the streams start.
  {
    raw_bench_t bench;
    bench.device->session ().set_firmware (50);
    EXPECT_EQ (bench.device->start_streams (), 3u);
    bench.advance (ms (100));
    for (bytes_t const &frame : bench.replies ())
      EXPECT_NE (frame[0], 0x58);
    EXPECT_EQ (bench.replies ().size (), 1u);
  }
  // The session changes after the start, and back.
  raw_bench_t bench;
  bench.device->start_streams ();
  auto old_stream = [&bench] ()
    {
      std::size_t found = 0;
      for (bytes_t const &frame : bench.replies ())
        if (frame[0] == 0x58)
          ++found;
      return found;
    };
  bench.advance (ms (100));
  EXPECT_EQ (old_stream (), 1u);
  bench.device->session ().set_firmware (50);
  bench.advance (ms (200));
  EXPECT_EQ (old_stream (), 1u);
  bench.device->session ().set_firmware (5);
  bench.advance (ms (100));
  EXPECT_EQ (old_stream (), 2u);
}

// --- the steps and the functions that throw or destroy

TEST (LumexDeviceSimulatorContext, Simulator_DestroyedInAStepEndsTheLaterSteps)
{
  raw_bench_t bench;
  bench.device->start_stream<lab::LEVEL_t> (ms (10));
  bench.device->after (ms (25), [&bench] () { bench.device.reset (); });
  bench.device->after (ms (30),
                       [] () { ADD_FAILURE () << "a later step ran"; });
  bench.advance (ms (100));
  EXPECT_FALSE (bench.device);
  EXPECT_EQ (bench.replies ().size (), 2u);
}

TEST (LumexDeviceSimulatorContext,
      Simulator_StepThatThrowsIsCountedLeavesRunAndTheStreamGoesOn)
{
  raw_bench_t bench;
  bench.device->start_stream<lab::LEVEL_t> (ms (10));
  bench.device->after (ms (15), [] () { throw std::runtime_error ("step"); });
  bool caught = false;
  try
    {
      bench.advance (ms (20));
    }
  catch (std::runtime_error const &)
    {
      caught = true;
    }
  bench.advance (ms (50));
  EXPECT_TRUE (caught);
  EXPECT_EQ (bench.device->statistics ().handler_errors, 1u);
  EXPECT_GE (bench.replies ().size (), 3u);
}

TEST (LumexDeviceSimulatorContext,
      Simulator_DestroyedInTheHandlerOfALaterFrameServesNoMore)
{
  raw_bench_t bench;
  int later = 0;
  bench.device->on<lab::LEVEL_t> (
      [&bench] (layout::request_of<lab::LEVEL_t>::type const &)
        { bench.device.reset (); });
  bench.device->on<lab::RESET_t> (
      [&later] (layout::request_of<lab::RESET_t>::type const &) { ++later; });
  bytes_t wire = framed (bytes ({ 0x10 }));
  bytes_t const second = framed (bytes ({ 0x13 }));
  bytes_t const third = framed (bytes ({ 0x20 }));
  wire.insert (wire.end (), second.begin (), second.end ());
  wire.insert (wire.end (), third.begin (), third.end ());
  bench.write (wire);
  EXPECT_FALSE (bench.device);
  EXPECT_EQ (later, 0);
  EXPECT_EQ (bench.replies ().size (), 1u);
}

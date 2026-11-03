// The device simulator under the behaviours of the matrix: a silent, frozen,
// echoing, damaged, fragmented, unplugged or flooding device. A device that
// does not process a request does not run its handler or touch its state;
// the other behaviours shape the bytes of replies and of stream messages.
// The lab table of the simulator tests, through the dialogue engine.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestSupport.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace tables = lumex_tests::simulator_tables;
namespace field = lumex::core::protocol::field;
using lab = tables::lab_t;
using bench_t = st::bench_t<lab>;
using st::bytes_t;
using st::ms;
using st::protocol_errc;
using tables::bytes;
namespace layout = st::layout;
namespace tr = st::tr;
using tr::testing::behavior;

std::error_code
code (protocol_errc value)
{
  return lumex::core::protocol::error::make_error_code (value);
}

template <typename Row>
bytes_t
sample_frame ()
{
  bytes_t out;
  EXPECT_TRUE ((layout::sample_reply_message<lab, Row> (out).has_value ()));
  return out;
}

std::int64_t
raw_of (bytes_t const &frame)
{
  auto const value = layout::decode_reply_message<lab, lab::PARAM_GET_t> (
      st::view_of (frame));
  EXPECT_TRUE (value.has_value ());
  return value ? field::get<1> (*value).raw : -1;
}
} // namespace

TEST (LumexDeviceSimulatorBehaviors, Behavior_SilentDeviceNeverRunsItsHandler)
{
  bench_t bench;
  int handled = 0;
  bench.device->on<lab::INFO_t> (
      [&handled] (layout::request_of<lab::INFO_t>::type const &)
        { ++handled; });
  bench.device->set_behavior (behavior::silent ());
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  EXPECT_EQ (out->error, code (protocol_errc::timeout));
  EXPECT_EQ (handled, 0);
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.requests, 1u);
  EXPECT_EQ (counted.not_processed, 1u);
  EXPECT_EQ (counted.replies, 0u);
  // It answers again when it hears again.
  bench.device->set_behavior (behavior::perfect ());
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_EQ (handled, 1);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_SilentDeviceDoesNotKeepAWrittenParameter)
{
  bench_t bench;
  bench.device->set_behavior (behavior::silent ());
  EXPECT_EQ (
      bench.call<lab::PARAM_SET_t> (tables::lab_registry::LEVEL, 60.0)->error,
      code (protocol_errc::timeout));
  bench.device->set_behavior (behavior::perfect ());
  EXPECT_EQ (
      raw_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame),
      0);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_FrozenDeviceAnswersSomeRepliesThenNothing)
{
  bench_t bench;
  int handled = 0;
  bench.device->on<lab::INFO_t> (
      [&handled] (layout::request_of<lab::INFO_t>::type const &)
        { ++handled; });
  bench.device->set_behavior (behavior::frozen (2));
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_EQ (bench.call<lab::INFO_t> ()->error, code (protocol_errc::timeout));
  EXPECT_EQ (handled, 2);
  EXPECT_EQ (bench.device->statistics ().not_processed, 1u);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_TwoPhaseFreezesBetweenTheAcknowledgementAndTheCompletion)
{
  bench_t bench;
  std::vector<bytes_t> acks;
  bench.device->set_behavior (behavior::frozen (1));
  st::outcome_ptr const out
      = bench.call<lab::PRIME_t> (static_cast<std::uint16_t> (5));
  // The acknowledgement went out and counted as the one reply; the
  // completion did not.
  EXPECT_EQ (out->error, code (protocol_errc::timeout));
  EXPECT_EQ (bench.device->statistics ().replies, 1u);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_EchoSendsTheRequestBackAndRunsNothing)
{
  bench_t bench;
  int handled = 0;
  bench.device->on<lab::INFO_t> (
      [&handled] (layout::request_of<lab::INFO_t>::type const &)
        { ++handled; });
  bench.device->set_behavior (behavior::echo ());
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  // The key of the request is the key of the reply: the engine takes the
  // echo for the reply.
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, bytes ({ 0x10 }));
  EXPECT_EQ (handled, 0);
}

TEST (LumexDeviceSimulatorBehaviors, Behavior_CorruptedFrameChangesTheReply)
{
  bench_t bench;
  bench.device->set_reply<lab::INFO_t> (
      [] ()
        {
          layout::reply_message_of<lab::INFO_t>::type value
              = layout::reply_message_of<lab::INFO_t>::type ();
          field::get<1> (value) = 0x1234;
          return value;
        }());
  bytes_t const clean = bench.call<lab::INFO_t> ()->frame;
  bench.device->set_behavior (behavior::corrupted_crc (3, 1000, 0));
  st::outcome_ptr const damaged = bench.call<lab::INFO_t> ();
  ASSERT_FALSE (damaged->error) << damaged->error.message ();
  EXPECT_NE (damaged->frame, clean);
  EXPECT_EQ (damaged->frame.size (), clean.size ());
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_FragmentedReplyArrivesInPiecesOnTheVirtualClock)
{
  bench_t bench;
  bench.device->set_behavior (behavior::fragmented (5, 2, ms (7)));
  tr::timing::time_point_t const start = bench.clock.now ();
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, sample_frame<lab::INFO_t> ());
  EXPECT_GE (std::chrono::duration_cast<ms> (out->at - start).count (), 7);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_StreamMessagesGoThroughTheBehaviourToo)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  bench.device->start_stream<lab::STATE_t> ();
  bench.device->set_behavior (behavior::silent ());
  bench.advance (ms (300));
  EXPECT_TRUE (seen.empty ());
  EXPECT_EQ (bench.device->statistics ().stream_messages, 0u);
  bench.device->set_behavior (behavior::perfect ());
  bench.advance (ms (300));
  EXPECT_EQ (seen.size (), 3u);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_FloodAndGarbageBeforeTheReplyAreSurvived)
{
  bench_t bench;
  bench.device->set_behavior (
      behavior::flood (200).then (behavior::garbage (9, 40)));
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, sample_frame<lab::INFO_t> ());
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_UnplugCableHangsTheLinkUpForAWhile)
{
  bench_t bench;
  bench.device->set_behavior (behavior::cable_unplugged (ms (500)));
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  EXPECT_TRUE (out->error);
  EXPECT_EQ (bench.device->statistics ().replies, 0u);
}

TEST (LumexDeviceSimulatorBehaviors,
      Behavior_UnknownRequestsMeetTheBehaviourToo)
{
  tr::testing::simulator_options_t options;
  options.answer_unknown = true;
  bench_t bench (options);
  std::vector<bytes_t> back;
  bench.engine->on_unknown ([&back] (st::pr::reply_frame_t const &frame)
                              { back.push_back (frame); });
  bench.device->set_behavior (behavior::silent ());
  bench.send_payload<lab::RESET_t> (bytes ({ 0xEE }));
  bench.advance (ms (100));
  EXPECT_TRUE (back.empty ());
  bench.device->set_behavior (behavior::perfect ());
  bench.send_payload<lab::RESET_t> (bytes ({ 0xEE }));
  bench.advance (ms (100));
  EXPECT_EQ (back.size (), 1u);
}

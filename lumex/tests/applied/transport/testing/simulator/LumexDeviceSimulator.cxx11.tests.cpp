// The device simulator over the lab table: the default and the chosen
// replies of every reply kind, the refusals by state, flag and registry, the
// generic get and set rows, the streams on the virtual clock, unknown and
// malformed requests, the rows a session does not have, a scripted
// sequence, the report ID of a datagram transport and the counters. Every
// request goes through the dialogue engine, so a reply the engine completes
// is one a client decodes.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
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
namespace dialogue = st::dialogue;
namespace layout = st::layout;
namespace tr = st::tr;

template <typename Row>
bytes_t
sample_frame ()
{
  bytes_t out;
  EXPECT_TRUE ((layout::sample_reply_message<lab, Row> (out).has_value ()));
  return out;
}

std::error_code
code (protocol_errc value)
{
  return lumex::core::protocol::error::make_error_code (value);
}

using info_value = layout::reply_message_of<lab::INFO_t>::type;

info_value
make_info (std::string const &model, std::uint16_t firmware)
{
  info_value value = info_value ();
  field::get<0> (value).assign (
      field::text_view (model.data (), model.size ()));
  field::get<1> (value) = firmware;
  return value;
}

std::string
model_of (bytes_t const &frame)
{
  layout::decode_result_t<info_value> const value
      = layout::decode_reply_message<lab, lab::INFO_t> (st::view_of (frame));
  EXPECT_TRUE (value.has_value ());
  if (!value)
    return std::string ();
  field::text_view const text = field::get<0> (*value).view ();
  return std::string (text.data (), text.size ());
}

// The value the generic get row reports, and the entry it names.
struct param_reading_t
{
  std::int64_t raw = -1;
  std::size_t entry = 0;
};

template <typename Row = lab::PARAM_GET_t>
param_reading_t
param_of (bytes_t const &frame)
{
  param_reading_t out;
  auto const value
      = layout::decode_reply_message<lab, Row> (st::view_of (frame));
  EXPECT_TRUE (value.has_value ());
  if (value)
    {
      out.entry = static_cast<std::size_t> (field::get<0> (*value));
      out.raw = field::get<1> (*value).raw;
    }
  return out;
}

// The status a STATE message carries.
tables::lab_state
state_of (bytes_t const &frame)
{
  auto const value
      = layout::decode_reply_message<lab, lab::STATE_t> (st::view_of (frame));
  EXPECT_TRUE (value.has_value ());
  return value ? field::get<0> (*value) : tables::lab_state::fault;
}

std::int64_t const kFrequency
    = static_cast<std::int64_t> (tables::lab_registry::FREQUENCY);
} // namespace

TEST (LumexDeviceSimulator, Simulator_AnswersWithTheSampleByDefault)
{
  bench_t bench;
  st::outcome_ptr const info = bench.call<lab::INFO_t> ();
  EXPECT_FALSE (info->error) << info->error.message ();
  EXPECT_EQ (info->frame, sample_frame<lab::INFO_t> ());
  st::outcome_ptr const param
      = bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL);
  EXPECT_FALSE (param->error) << param->error.message ();
  EXPECT_TRUE (bench.device->valid ());
}

TEST (LumexDeviceSimulator, Simulator_RowWithoutAReplySendsNothing)
{
  bench_t bench;
  st::outcome_ptr const reset = bench.call<lab::RESET_t> ();
  EXPECT_FALSE (reset->error) << reset->error.message ();
  bench.advance (ms (500));
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.requests, 1u);
  EXPECT_EQ (counted.replies, 0u);
}

TEST (LumexDeviceSimulator, Simulator_HandlerAnswersAndSeesTheRequest)
{
  bench_t bench;
  std::vector<int> targets;
  bench.device->on<lab::MOVE_t> (
      [&targets] (layout::request_of<lab::MOVE_t>::type const &request,
                  lumex::core::protocol::index::session_context_t &)
        {
          targets.push_back (static_cast<int> (field::get<0> (request)));
          return static_cast<std::uint8_t> (5);
        });
  st::outcome_ptr const out = bench.call<lab::MOVE_t> (std::uint8_t (9));
  ASSERT_FALSE (out->error) << out->error.message ();
  ASSERT_EQ (targets.size (), 1u);
  EXPECT_EQ (targets[0], 9);
  auto const decoded = dialogue::decode_reply_kind<lab, lab::MOVE_t> (
      st::view_of (out->frame));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (decoded->part, dialogue::reply_part::ack);
  EXPECT_EQ (decoded->ack, 5);
}

TEST (LumexDeviceSimulator,
      Simulator_HandlerOfOneArgumentAndOneThatOnlyWatches)
{
  bench_t bench;
  int seen = 0;
  // Returns nothing: the default reply goes out.
  bench.device->on<lab::INFO_t> (
      [&seen] (layout::request_of<lab::INFO_t>::type const &) { ++seen; });
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  EXPECT_FALSE (out->error);
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (out->frame, sample_frame<lab::INFO_t> ());
}

TEST (LumexDeviceSimulator, Simulator_FixedReplyAndGoingBackToTheDefault)
{
  bench_t bench;
  EXPECT_TRUE (bench.device->set_reply<lab::INFO_t> (make_info ("bench", 7)));
  EXPECT_EQ (model_of (bench.call<lab::INFO_t> ()->frame), "bench");
  EXPECT_TRUE (bench.device->clear_reply<lab::INFO_t> ());
  EXPECT_EQ (model_of (bench.call<lab::INFO_t> ()->frame), "");
}

TEST (LumexDeviceSimulator, Simulator_HandlerGetsTheSession)
{
  bench_t bench;
  bench.device->session ().set_model ("Z1");
  std::string model;
  bench.device->on<lab::INFO_t> (
      [&model] (layout::request_of<lab::INFO_t>::type const &,
                lumex::core::protocol::index::session_context_t &session)
        {
          model = session.model ();
          return make_info (session.model (), 1);
        });
  EXPECT_EQ (model_of (bench.call<lab::INFO_t> ()->frame), "Z1");
  EXPECT_EQ (model, "Z1");
}

TEST (LumexDeviceSimulator,
      Simulator_AckNakOfRowsAnswersWithTheAcknowledgement)
{
  bench_t bench;
  st::outcome_ptr const out = bench.call<lab::STOP_t> ();
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, bytes ({ 0x06 }));
}

TEST (LumexDeviceSimulator, Simulator_RefusesOutsideAcceptedState)
{
  bench_t bench;
  // In the right state every kind answers as the kind says.
  EXPECT_FALSE (bench.call<lab::MOVE_t> (std::uint8_t (1))->error);
  st::outcome_ptr const limit
      = bench.call<lab::SET_LIMIT_t> (std::uint8_t (7));
  EXPECT_FALSE (limit->error) << limit->error.message ();
  EXPECT_FALSE (bench.call<lab::SET_MODE_t> (tables::lab_state::busy)->error);
  EXPECT_FALSE (
      bench.call<lab::SET_FLOW_t> (static_cast<std::uint16_t> (300))->error);
  EXPECT_EQ (bench.device->statistics ().refused, 0u);

  // Busy: ack_nak answers with the negative acknowledgement, the zero echo
  // refuses, the state comes instead of the value, and a kind with no
  // refusal (echo_set) says nothing.
  bench.device->set_state (
      static_cast<std::int64_t> (tables::lab_state::busy));
  EXPECT_EQ (bench.call<lab::MOVE_t> (std::uint8_t (1))->error,
             code (protocol_errc::nak));
  EXPECT_EQ (bench.call<lab::SET_LIMIT_t> (std::uint8_t (7))->error,
             code (protocol_errc::refused));
  EXPECT_EQ (bench.call<lab::SET_MODE_t> (tables::lab_state::fault)->error,
             code (protocol_errc::refused));
  st::outcome_ptr const silent
      = bench.call<lab::SET_FLOW_t> (static_cast<std::uint16_t> (300));
  EXPECT_EQ (silent->error, code (protocol_errc::timeout));
  EXPECT_EQ (bench.device->statistics ().refused, 4u);

  // Idle again: accepted.
  bench.device->set_state (0);
  EXPECT_FALSE (bench.call<lab::MOVE_t> (std::uint8_t (1))->error);
}

TEST (LumexDeviceSimulator, Simulator_RefusedWriteZeroEchoIsTheEchoOfZero)
{
  bench_t bench;
  bench.device->set_state (1);
  st::outcome_ptr const out = bench.call<lab::SET_LIMIT_t> (std::uint8_t (7));
  EXPECT_EQ (out->error, code (protocol_errc::refused));
}

TEST (LumexDeviceSimulator, Simulator_RefuseFlagRefusesARowWithoutAcceptedIn)
{
  bench_t bench;
  EXPECT_TRUE (bench.device->refuse<lab::STOP_t> ());
  st::outcome_ptr const refused = bench.call<lab::STOP_t> ();
  EXPECT_EQ (refused->error, code (protocol_errc::nak));
  // The negative acknowledgement is the row REJECT with its sample.
  EXPECT_EQ (bench.device->statistics ().refused, 1u);
  EXPECT_TRUE (bench.device->refuse<lab::STOP_t> (false));
  EXPECT_FALSE (bench.call<lab::STOP_t> ()->error);
}

TEST (LumexDeviceSimulator, Simulator_EchoKindsEchoTheValueSent)
{
  bench_t bench;
  st::outcome_ptr const flow
      = bench.call<lab::SET_FLOW_t> (static_cast<std::uint16_t> (300));
  ASSERT_FALSE (flow->error) << flow->error.message ();
  auto const flow_echo = dialogue::decode_reply_kind<lab, lab::SET_FLOW_t> (
      st::view_of (flow->frame));
  ASSERT_TRUE (flow_echo.has_value ());
  EXPECT_EQ (flow_echo->value, 300);

  st::outcome_ptr const zero = bench.call<lab::SET_LIMIT_t> (std::uint8_t (0));
  EXPECT_FALSE (zero->error) << zero->error.message ();
  st::outcome_ptr const mode
      = bench.call<lab::SET_MODE_t> (tables::lab_state::fault);
  ASSERT_FALSE (mode->error) << mode->error.message ();
  auto const mode_echo = dialogue::decode_reply_kind<lab, lab::SET_MODE_t> (
      st::view_of (mode->frame));
  ASSERT_TRUE (mode_echo.has_value ());
  EXPECT_EQ (mode_echo->value, tables::lab_state::fault);
}

TEST (LumexDeviceSimulator, Simulator_TwoPhaseAcknowledgesThenCompletes)
{
  bench_t bench;
  bench.device->on<lab::PRIME_t> (
      [] (layout::request_of<lab::PRIME_t>::type const &request)
        { return static_cast<std::uint16_t> (field::get<0> (request) + 1); });
  st::outcome_ptr const out
      = bench.call<lab::PRIME_t> (static_cast<std::uint16_t> (500));
  ASSERT_FALSE (out->error) << out->error.message ();
  auto const decoded = dialogue::decode_reply_kind<lab, lab::PRIME_t> (
      st::view_of (out->frame));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (decoded->part, dialogue::reply_part::completion);
  EXPECT_EQ (decoded->completion, 501);
  // The acknowledgement and the completion are two replies.
  EXPECT_EQ (bench.device->statistics ().replies, 2u);
}

TEST (LumexDeviceSimulator, Simulator_ReplyRowAnswersWithTheMessageOfThatRow)
{
  bench_t bench;
  layout::reply_message_of<lab::CHANGE_t>::type change
      = layout::reply_message_of<lab::CHANGE_t>::type ();
  field::get<0> (change) = 3;
  field::get<1> (change) = 77;
  bench.device->set_reply<lab::WRITE_t> (change);
  st::outcome_ptr const out = bench.call<lab::WRITE_t> (
      std::uint8_t (3), static_cast<std::uint16_t> (77));
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, bytes ({ 0x30, 0x03, 0x4D, 0x00 }));
}

TEST (LumexDeviceSimulator, Simulator_RegistryGetAfterSet)
{
  bench_t bench;
  // Before any set, an entry shows its first acceptable value: the lower
  // limit of the frequency, zero of the level.
  param_reading_t before = param_of (
      bench.call<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY)->frame);
  EXPECT_EQ (before.entry, static_cast<std::size_t> (kFrequency));
  EXPECT_EQ (before.raw, 10);
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame)
          .raw,
      0);

  // A set reports the value it kept.
  param_reading_t const set = param_of<lab::PARAM_SET_t> (
      bench.call<lab::PARAM_SET_t> (tables::lab_registry::FREQUENCY, 120.0)
          ->frame);
  EXPECT_EQ (set.entry, static_cast<std::size_t> (kFrequency));
  EXPECT_EQ (set.raw, 120);

  // The get that follows shows it, another entry is untouched.
  EXPECT_EQ (
      param_of (bench.call<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY)
                    ->frame)
          .raw,
      120);
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame)
          .raw,
      0);
  bench.call<lab::PARAM_SET_t> (tables::lab_registry::LEVEL, 55.0);
  bench.call<lab::PARAM_SET_t> (tables::lab_registry::FREQUENCY, 250.0);
  EXPECT_EQ (
      param_of (bench.call<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY)
                    ->frame)
          .raw,
      250);
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame)
          .raw,
      55);
}

TEST (LumexDeviceSimulator, Simulator_RegistryRefusesWhatTheEntryDoesNotAllow)
{
  bench_t bench;
  // The requests the encoder of a client would not write: a value for a
  // read-only entry, and one above the limit of the frequency.
  st::outcome_ptr denied;
  {
    st::pr::exchange_t request = bench.make_exchange<lab::PARAM_SET_t> (
        tables::lab_registry::FREQUENCY, 100.0);
    request.message = bytes ({ 0x41, 0x03, 0xD2, 0x04 });
    denied = bench.start_exchange (request);
    ASSERT_TRUE (bench.run_until ([&] () { return denied->calls != 0; }));
  }
  EXPECT_EQ (denied->error, code (protocol_errc::timeout));
  st::outcome_ptr beyond;
  {
    st::pr::exchange_t request = bench.make_exchange<lab::PARAM_SET_t> (
        tables::lab_registry::FREQUENCY, 100.0);
    request.message = bytes ({ 0x41, 0x01, 0xE8, 0x03 });
    beyond = bench.start_exchange (request);
    ASSERT_TRUE (bench.run_until ([&] () { return beyond->calls != 0; }));
  }
  EXPECT_EQ (beyond->error, code (protocol_errc::timeout));
  EXPECT_EQ (bench.device->statistics ().refused, 2u);

  // Nothing was kept.
  EXPECT_EQ (
      param_of (bench.call<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY)
                    ->frame)
          .raw,
      10);
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::SERIAL)->frame)
          .raw,
      0);
}

TEST (LumexDeviceSimulator,
      Simulator_GetOfAWriteOnlyEntryIsRefusedAndNamesNoOtherKey)
{
  bench_t bench;
  // ARM can be written but not read: the reply cannot name it, and the
  // device does not name another entry instead.
  st::outcome_ptr const out
      = bench.call<lab::PARAM_GET_t> (tables::lab_registry::ARM);
  EXPECT_EQ (out->error, code (protocol_errc::timeout));
  EXPECT_TRUE (out->frame.empty ());
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.refused, 1u);
  EXPECT_EQ (counted.replies, 0u);
  EXPECT_EQ (counted.encode_errors, 0u);
  // A readable entry is answered as before.
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame)
          .entry,
      static_cast<std::size_t> (tables::lab_registry::LEVEL));
}

TEST (LumexDeviceSimulator,
      Simulator_GetOfAWriteOnlyEntryByAckNakIsNakedAndKeepsNothing)
{
  bench_t bench;
  st::outcome_ptr const refused
      = bench.call<lab::PARAM_ASK_t> (tables::lab_registry::ARM);
  EXPECT_EQ (refused->error, code (protocol_errc::nak));
  EXPECT_EQ (bench.device->statistics ().refused, 1u);
  // A readable entry is acknowledged with the entry named.
  st::outcome_ptr const ok
      = bench.call<lab::PARAM_ASK_t> (tables::lab_registry::LEVEL);
  ASSERT_FALSE (ok->error) << ok->error.message ();
  EXPECT_EQ (ok->frame[0], 0x42);
  EXPECT_EQ (ok->frame[1], static_cast<std::uint8_t> (0x02));
}

TEST (LumexDeviceSimulator,
      Simulator_SetOfAWriteOnlyEntryWhoseReplyCannotBeIsRefusedAndKeepsNothing)
{
  bench_t bench;
  // The write is allowed, the reply of the row would have to report a value
  // that cannot be read: no reply, and the write is not kept.
  EXPECT_EQ (
      bench.call<lab::PARAM_SET_t> (tables::lab_registry::ARM, 9.0)->error,
      code (protocol_errc::timeout));
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.refused, 1u);
  EXPECT_EQ (counted.registry_writes, 0u);
  // A write whose reply can exist is kept.
  bench.call<lab::PARAM_SET_t> (tables::lab_registry::LEVEL, 7.0);
  EXPECT_EQ (bench.device->statistics ().registry_writes, 1u);
  EXPECT_EQ (
      param_of (
          bench.call<lab::PARAM_GET_t> (tables::lab_registry::LEVEL)->frame)
          .raw,
      7);
}

TEST (LumexDeviceSimulator, Simulator_StreamAtItsInterval)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  std::vector<std::int64_t> times;
  bench.engine->subscribe (
      bench_t::row_of<lab::STATE_t> (),
      [&] (st::pr::reply_frame_t const &frame)
        {
          seen.push_back (frame);
          times.push_back (
              std::chrono::duration_cast<ms> (bench.clock.now ()
                                              - tr::timing::time_point_t ())
                  .count ());
        });
  ASSERT_TRUE (bench.device->start_stream<lab::STATE_t> ());
  std::int64_t const start
      = std::chrono::duration_cast<ms> (bench.clock.now ()
                                        - tr::timing::time_point_t ())
            .count ();
  bench.advance (ms (99));
  EXPECT_TRUE (seen.empty ());
  bench.advance (ms (1));
  EXPECT_EQ (seen.size (), 1u);
  bench.advance (ms (300));
  ASSERT_EQ (seen.size (), 4u);
  for (std::size_t i = 0; i < times.size (); ++i)
    EXPECT_EQ (times[i], start + 100 * static_cast<std::int64_t> (i + 1));
  EXPECT_EQ (seen[0], sample_frame<lab::STATE_t> ());
  bench.device->stop_stream<lab::STATE_t> ();
  bench.advance (ms (500));
  EXPECT_EQ (seen.size (), 4u);
  EXPECT_EQ (bench.device->statistics ().stream_messages, 4u);
}

TEST (LumexDeviceSimulator, Simulator_StreamWithAnIntervalOfItsOwn)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  EXPECT_TRUE (bench.device->start_stream<lab::STATE_t> (ms (40)));
  bench.advance (ms (200));
  EXPECT_EQ (seen.size (), 5u);
  // A row with no interval needs one.
  EXPECT_FALSE (bench.device->start_stream<lab::INFO_t> ());
  EXPECT_TRUE (bench.device->start_stream<lab::INFO_t> (ms (50)));
}

TEST (LumexDeviceSimulator, Simulator_StreamShowsTheStateOfTheDevice)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  bench.device->start_stream<lab::STATE_t> ();
  bench.advance (ms (100));
  bench.device->set_state (1);
  bench.advance (ms (100));
  bench.device->set_state (2);
  bench.advance (ms (100));
  ASSERT_EQ (seen.size (), 3u);
  EXPECT_EQ (state_of (seen[0]), tables::lab_state::idle);
  EXPECT_EQ (state_of (seen[1]), tables::lab_state::busy);
  EXPECT_EQ (state_of (seen[2]), tables::lab_state::fault);
}

TEST (LumexDeviceSimulator, Simulator_StreamOfAValueAndOfAGenerator)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::LEVEL_t> (seen);
  EXPECT_TRUE (bench.device->start_stream<lab::LEVEL_t> (
      static_cast<std::uint8_t> (42)));
  bench.advance (ms (500));
  ASSERT_EQ (seen.size (), 2u);
  EXPECT_EQ (seen[0], bytes ({ 0x13, 42 }));
  int count = 0;
  EXPECT_TRUE (bench.device->start_stream_with<lab::LEVEL_t> (
      [&count] () { return static_cast<std::uint8_t> (++count); }));
  bench.advance (ms (500));
  ASSERT_EQ (seen.size (), 4u);
  EXPECT_EQ (seen[2], bytes ({ 0x13, 1 }));
  EXPECT_EQ (seen[3], bytes ({ 0x13, 2 }));
}

TEST (LumexDeviceSimulator, Simulator_StartStreamsStartsEveryStreamRow)
{
  bench_t bench;
  std::vector<bytes_t> states;
  std::vector<bytes_t> levels;
  bench.subscribe<lab::STATE_t> (states);
  bench.subscribe<lab::LEVEL_t> (levels);
  // STATE, LEVEL and OLD_STREAM (no firmware is identified yet).
  EXPECT_EQ (bench.device->start_streams (), 3u);
  bench.advance (ms (500));
  EXPECT_EQ (states.size (), 5u);
  EXPECT_EQ (levels.size (), 2u);
  bench.device->stop_streams ();
  bench.advance (ms (500));
  EXPECT_EQ (states.size (), 5u);
  EXPECT_EQ (levels.size (), 2u);
}

TEST (LumexDeviceSimulator, Simulator_StartingAStreamAgainReplacesIt)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  bench.device->start_stream<lab::STATE_t> (ms (100));
  bench.advance (ms (150));
  bench.device->start_stream<lab::STATE_t> (ms (300));
  bench.advance (ms (600));
  // One at 100; the new stream: 450 and 750.
  EXPECT_EQ (seen.size (), 3u);
}

TEST (LumexDeviceSimulator,
      Simulator_AStaleTickOfAReplacedOrStoppedStreamSendsNothing)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  // The wait of the stream came due (its handler is on the way), and the
  // stream is replaced before the handler runs.
  bench.device->start_stream<lab::STATE_t> (ms (100));
  bench.clock.advance (ms (100));
  bench.device->start_stream<lab::STATE_t> (ms (300));
  bench.settle ();
  EXPECT_TRUE (seen.empty ());
  // The same for a stream that is stopped.
  bench.clock.advance (ms (300));
  bench.device->stop_stream<lab::STATE_t> ();
  bench.settle ();
  EXPECT_TRUE (seen.empty ());
  bench.advance (ms (1000));
  EXPECT_TRUE (seen.empty ());
}

TEST (LumexDeviceSimulator, Simulator_UnknownRequestIsCountedAndNotAnswered)
{
  bench_t bench;
  st::outcome_ptr const sent
      = bench.send_payload<lab::RESET_t> (bytes ({ 0xEE }));
  EXPECT_FALSE (sent->error);
  bench.advance (ms (500));
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.unknown_requests, 1u);
  EXPECT_EQ (counted.requests, 0u);
  EXPECT_EQ (counted.replies, 0u);
  // The device goes on.
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
}

TEST (LumexDeviceSimulator, Simulator_AnswerUnknownSendsTheFrameBack)
{
  tr::testing::simulator_options_t options;
  options.answer_unknown = true;
  bench_t bench (options);
  std::vector<bytes_t> back;
  bench.engine->on_unknown ([&back] (st::pr::reply_frame_t const &frame)
                              { back.push_back (frame); });
  bench.send_payload<lab::RESET_t> (bytes ({ 0xEE, 0x01, 0x02 }));
  bench.advance (ms (100));
  ASSERT_EQ (back.size (), 1u);
  EXPECT_EQ (back[0], bytes ({ 0xEE, 0x01, 0x02 }));
  EXPECT_EQ (bench.device->statistics ().unknown_requests, 1u);
}

TEST (LumexDeviceSimulator, Simulator_MalformedRequestsAreCountedAndHarmless)
{
  bench_t bench;
  // The key of MOVE without its field, with a field too many, and a text
  // of nonsense.
  bench.send_payload<lab::RESET_t> (bytes ({ 0x21 }));
  bench.send_payload<lab::RESET_t> (bytes ({ 0x21, 0x01, 0x02 }));
  bench.send_payload<lab::RESET_t> (bytes ({ 0x41, 0x09, 0x00, 0x00 }));
  bench.send_payload<lab::RESET_t> (bytes ({ 0x10, 0x00 }));
  bench.advance (ms (300));
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.malformed_requests, 4u);
  EXPECT_EQ (counted.replies, 0u);
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
}

TEST (LumexDeviceSimulator,
      Simulator_RowOutsideTheFirmwareOfTheSessionIsUnknown)
{
  bench_t bench;
  bench.device->session ().set_firmware (12);
  EXPECT_EQ (bench.call<lab::LEGACY_t> ()->error,
             code (protocol_errc::timeout));
  EXPECT_EQ (bench.call<lab::NEWER_t> ()->error,
             code (protocol_errc::timeout));
  EXPECT_EQ (bench.device->statistics ().unknown_requests, 2u);
  bench.device->session ().set_firmware (5);
  EXPECT_FALSE (bench.call<lab::LEGACY_t> ()->error);
  EXPECT_EQ (bench.call<lab::NEWER_t> ()->error,
             code (protocol_errc::timeout));
}

TEST (LumexDeviceSimulator, Simulator_ScenarioScript)
{
  bench_t bench;
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  // A plain sequence of the simulator's own calls at virtual times.
  bench.device->start_stream<lab::STATE_t> ();
  bench.device->after (
      ms (100), [&bench] ()
        { bench.device->set_reply<lab::INFO_t> (make_info ("late", 2)); });
  bench.device->after (ms (250), [&bench] () { bench.device->set_state (1); });
  bench.device->after (ms (450), [&bench] ()
                         { bench.device->stop_stream<lab::STATE_t> (); });
  EXPECT_EQ (model_of (bench.call<lab::INFO_t> ()->frame), "");
  bench.advance (ms (150));
  EXPECT_EQ (model_of (bench.call<lab::INFO_t> ()->frame), "late");
  bench.advance (ms (700));
  // 100, 200 idle; 300, 400 busy; stopped at 450.
  ASSERT_EQ (seen.size (), 4u);
  EXPECT_EQ (state_of (seen[1]), tables::lab_state::idle);
  EXPECT_EQ (state_of (seen[2]), tables::lab_state::busy);
  EXPECT_EQ (state_of (seen[3]), tables::lab_state::busy);
}

TEST (LumexDeviceSimulator, Simulator_StatisticsCountByRowAndReset)
{
  bench_t bench;
  bench.call<lab::INFO_t> ();
  bench.call<lab::INFO_t> ();
  bench.call<lab::STOP_t> ();
  tr::testing::simulator_statistics_t counted = bench.device->statistics ();
  EXPECT_EQ (counted.requests, 3u);
  EXPECT_EQ (counted.replies, 3u);
  EXPECT_EQ (counted.requests_of ("INFO"), 2u);
  EXPECT_EQ (counted.requests_of ("STOP"), 1u);
  EXPECT_EQ (counted.requests_of ("MOVE"), 0u);
  ASSERT_EQ (counted.by_row.size (), 2u);
  bench.device->reset_statistics ();
  counted = bench.device->statistics ();
  EXPECT_EQ (counted.requests, 0u);
  EXPECT_TRUE (counted.by_row.empty ());
}

TEST (LumexDeviceSimulator, Simulator_DatagramReportId)
{
  bench_t bench (std::uint8_t (7));
  st::outcome_ptr const info = bench.call<lab::INFO_t> ();
  ASSERT_FALSE (info->error) << info->error.message ();
  EXPECT_EQ (info->frame, sample_frame<lab::INFO_t> ());
  std::vector<tr::datagram::message_t> const sent
      = bench.datagram_control->sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 7);
  std::vector<bytes_t> seen;
  bench.subscribe<lab::STATE_t> (seen);
  bench.device->start_stream<lab::STATE_t> ();
  bench.advance (ms (300));
  EXPECT_EQ (seen.size (), 3u);
}

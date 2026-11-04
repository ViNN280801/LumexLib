// The device simulator over the reply kinds and the counters: what a client
// can and cannot tell (an acknowledgement and a refusal of one wire shape, a
// state equal to the value sent), the state row in every form it can take,
// the counters against their documentation, the registry in the reply kinds
// that name an entry, an echo of a whole record, a request that depends on
// the session, and the example of the header. The lab table of the simulator
// tests and a table of one field for the state row.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorRawBench.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestSupport.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace lumex_tests
{
namespace one_field_tables
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
using simulator_tables::lab_state;

struct codec_t
{
};

struct one_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::state_from<typename P::STAT_t, 0>>;
};

// A state row whose reply is one field, not a layout.
// clang-format off
#define ONE_ROWS(X, X_AS) \
  X (STAT, 0x12, "State", (), (), (enum_<u8_, lab_state>), (stream (100)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (one_t, codec_t, one_header_t, ONE_ROWS);

struct note_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::state_from<typename P::STAT_t, 0>>;
};

// A state row that does not stream: a reply kind may name it.
// clang-format off
#define NOTE_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (STAT, 0x12, "State", (), (), (enum_<u8_, lab_state>), ()) \
  X (PHASE, 0x55, "Two phases", (), (), (two_phase<ACK_t, STAT_t>), ()) \
  X (ASK, 0x56, "Acknowledged", (), (), (ack_nak<STAT_t, ACK_t>), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (note_t, codec_t, note_header_t, NOTE_ROWS);
} // namespace one_field_tables
} // namespace lumex_tests

namespace
{
namespace st = lumex_simulator_test;
namespace tables = lumex_tests::simulator_tables;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tr = lumex::applied::transport;
using lab = tables::lab_t;
using one = lumex_tests::one_field_tables::one_t;
using note = lumex_tests::one_field_tables::note_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using st::framed;
using st::protocol_errc;
using tables::bytes;
using tr::testing::behavior;

std::error_code
code (protocol_errc value)
{
  return lumex::core::protocol::error::make_error_code (value);
}
} // namespace

// --- what a client cannot tell

TEST (LumexDeviceSimulatorKinds,
      Simulator_RefusalOfOneShapeReadsAsAcknowledgement)
{
  // ACK is a byte and NAK is a signed byte: one shape on the wire, and the
  // reader takes the first part that decodes. The device refuses; the client
  // sees success. A table gives the two parts different shapes.
  st::bench_t<lab> bench;
  bench.device->refuse<lab::BOTH_t> ();
  st::outcome_ptr const out = bench.call<lab::BOTH_t> (std::uint8_t (5));
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (bench.device->statistics ().refused, 1u);
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_StateEqualToTheValueSentReadsAsSuccess)
{
  // state_instead_of_refusal: the refusal is the state; when the state is
  // the value that was sent, it cannot be told from the echo.
  st::bench_t<lab> bench;
  bench.device->set_state (2);
  st::outcome_ptr const same
      = bench.call<lab::SET_MODE_t> (tables::lab_state::fault);
  EXPECT_FALSE (same->error) << same->error.message ();
  EXPECT_EQ (bench.device->statistics ().refused, 1u);
  st::outcome_ptr const other
      = bench.call<lab::SET_MODE_t> (tables::lab_state::busy);
  EXPECT_EQ (other->error, code (protocol_errc::refused));
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_RefusalOfAStateKindIsTheStateNotTheSample)
{
  st::raw_bench_t bench;
  bench.device->set_state (1);
  bench.write (framed (bytes ({ 0x24, 0x02 })));
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 1u);
  EXPECT_EQ (replies[0], bytes ({ 0x24, 0x01 }));
}

// --- the state row

TEST (LumexDeviceSimulatorKinds,
      Simulator_StateRowShowsTheStateInEveryFormItTakes)
{
  st::raw_bench_t bench;
  bench.device->set_state (2);
  bench.write (framed (bytes ({ 0x12 }))); // its own reply
  bench.write (framed (bytes ({ 0x54 }))); // reply_row<STATE>
  // A reply kind cannot name the state row (it streams: the table would not
  // compile), so a completion and an acknowledgement are rows of their own and
  // carry no state.
  bench.write (framed (bytes ({ 0x55, 0x01 }))); // two_phase completion
  bench.write (framed (bytes ({ 0x56 })));       // ack_nak acknowledgement
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 5u);
  EXPECT_EQ (replies[0], bytes ({ 0x12, 0x02, 0x00 }));
  EXPECT_EQ (replies[1], bytes ({ 0x12, 0x02, 0x00 }));
  EXPECT_EQ (replies[2], bytes ({ 0x06 }));
  EXPECT_EQ (replies[3], bytes ({ 0x5a, 0x00, 0x00 }));
  EXPECT_EQ (replies[4], bytes ({ 0x5a, 0x00, 0x00 }));
}

TEST (LumexDeviceSimulatorKinds, Simulator_StateRowOfOneFieldShowsTheState)
{
  st::bench_t<one> bench;
  bench.device->set_state (2);
  st::outcome_ptr const out = bench.call<one::STAT_t> ();
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->frame, bytes ({ 0x12, 0x02 }));
}

// A completion or an acknowledgement by the state row shows the state, when
// that row does not stream.
TEST (LumexDeviceSimulatorKinds, Simulator_StateRowAsAPartOfAKindShowsTheState)
{
  st::bench_t<note> bench;
  bench.device->set_state (2);
  st::outcome_ptr const phase = bench.call<note::PHASE_t> ();
  ASSERT_FALSE (phase->error) << phase->error.message ();
  EXPECT_EQ (phase->frame, bytes ({ 0x12, 0x02 }));
  st::outcome_ptr const ask = bench.call<note::ASK_t> ();
  ASSERT_FALSE (ask->error) << ask->error.message ();
  EXPECT_EQ (ask->frame, bytes ({ 0x12, 0x02 }));
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_StateSetInAWatchingHandlerShowsInTheNextReply)
{
  // A handler that only watches gets the default reply made after it ran, so
  // a state it sets is not the one in the reply of that request.
  st::raw_bench_t bench;
  bench.device->on<lab::STATE_t> (
      [&bench] (layout::request_of<lab::STATE_t>::type const &)
        { bench.device->set_state (2); });
  bench.write (framed (bytes ({ 0x12 })));
  bench.write (framed (bytes ({ 0x12 })));
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 2u);
  EXPECT_EQ (replies[0][1], 0x00);
  EXPECT_EQ (replies[1][1], 0x02);
}

// --- the counters

TEST (LumexDeviceSimulatorKinds,
      Simulator_AStreamMessageTheBehaviourDropsIsNoRequestNotProcessed)
{
  st::raw_bench_t bench;
  bench.device->set_behavior (behavior::frozen (0));
  bench.device->start_stream<lab::LEVEL_t> (ms (10));
  bench.advance (ms (55));
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.not_processed, 0u);
  EXPECT_EQ (counted.stream_messages, 0u);
  bench.write (framed (bytes ({ 0x13 })));
  EXPECT_EQ (bench.device->statistics ().not_processed, 1u);
}

TEST (LumexDeviceSimulatorKinds, Simulator_AReplyTheBehaviourDropsIsNoReply)
{
  st::raw_bench_t bench;
  bench.device->set_behavior (behavior::intermittent (1, 1000, 0, ms (1000)));
  bench.write (framed (bytes ({ 0x13 })));
  bench.advance (ms (10));
  EXPECT_EQ (bench.replies ().size (), 0u);
  EXPECT_EQ (bench.device->statistics ().replies, 0u);
  EXPECT_EQ (bench.device->statistics ().requests, 1u);
}

TEST (LumexDeviceSimulatorKinds, Simulator_ADroppedFurtherMessageIsNoReply)
{
  // A reply of two messages under an intermittent device: the replies counted
  // are the frames on the line, whichever of the two the device lost.
  int acknowledged_only = 0;
  for (std::uint32_t seed = 1; seed < 200; ++seed)
    {
      st::raw_bench_t bench;
      bench.device->set_behavior (
          behavior::intermittent (seed, 500, 0, ms (0)));
      bench.write (framed (bytes ({ 0x26, 0x05, 0x00 })));
      std::size_t const frames = bench.replies ().size ();
      EXPECT_EQ (bench.device->statistics ().replies, frames) << seed;
      if (frames == 1u)
        ++acknowledged_only;
    }
  EXPECT_GT (acknowledged_only, 0);
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_AThrowingHandlerKeepsNoRegistryValue)
{
  st::raw_bench_t bench;
  bench.device->on<lab::PARAM_SET_t> (
      [] (layout::request_of<lab::PARAM_SET_t>::type const &)
        { throw std::runtime_error ("handler"); });
  EXPECT_THROW (bench.write (framed (bytes ({ 0x41, 0x01, 0x20, 0x00 }))),
                std::runtime_error);
  st::drain (bench.context);
  EXPECT_EQ (bench.device->statistics ().registry_writes, 0u);
  bench.write (framed (bytes ({ 0x40, 0x01 })));
  std::vector<bytes_t> const replies = bench.replies ();
  ASSERT_EQ (replies.size (), 1u);
  // The first acceptable value of the frequency: its lower limit, 10.
  EXPECT_EQ (replies[0], bytes ({ 0x40, 0x01, 0x0A, 0x00 }));
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_EmptyDatagramsAreCountedAsWhatTheyAre)
{
  st::datagram_bench_t bench (7);
  bench.send (7, bytes_t ());
  EXPECT_EQ (bench.device->statistics ().framing_errors, 1u);
  EXPECT_EQ (bench.device->statistics ().foreign_reports, 0u);
  bench.send (8, bytes_t ());
  EXPECT_EQ (bench.device->statistics ().foreign_reports, 1u);
  EXPECT_EQ (bench.device->statistics ().framing_errors, 1u);
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_ACutFrameAfterAWholeOneInADatagramIsNotReported)
{
  // The codecs do not tell of a partial frame that follows a complete one
  // (see framed_datagram): the whole frame is served, the rest is forgotten.
  st::datagram_bench_t bench (7);
  bytes_t payload = framed (bytes ({ 0x13 }));
  bytes_t cut = framed (bytes ({ 0x10 }));
  cut.pop_back ();
  payload.insert (payload.end (), cut.begin (), cut.end ());
  bench.send (7, payload);
  EXPECT_EQ (bench.received.size (), 1u);
  EXPECT_EQ (bench.device->statistics ().requests, 1u);
  EXPECT_EQ (bench.device->statistics ().framing_errors, 0u);
}

// --- the unknown, the session, the registry in the kinds

TEST (LumexDeviceSimulatorKinds,
      Simulator_MalformedRequestOfARowWithoutAReplyIsSentBack)
{
  tr::testing::simulator_options_t options;
  options.answer_unknown = true;
  st::bench_t<lab> bench (options);
  std::vector<bytes_t> back;
  bench.subscribe<lab::RESET_t> (back);
  bench.send_payload<lab::RESET_t> (bytes ({ 0x20, 0x01 }));
  bench.advance (ms (50));
  ASSERT_EQ (back.size (), 1u);
  EXPECT_EQ (back[0], bytes ({ 0x20, 0x01 }));
  EXPECT_EQ (bench.device->statistics ().malformed_requests, 1u);
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_RequestThatDependsOnTheSessionIsDecodedWithIt)
{
  st::raw_bench_t bench;
  int seen = 0;
  bench.device->on<lab::CTX_SET_t> (
      [&seen] (layout::request_of<lab::CTX_SET_t>::type const &) { ++seen; });
  // Without the session value the layout of the form is unknown.
  bench.write (framed (bytes ({ 0x59, 0x34, 0x12, 0x07 })));
  EXPECT_EQ (seen, 0);
  EXPECT_EQ (bench.device->statistics ().malformed_requests, 1u);
  bench.device->session ().set ("lab_mode", 1);
  bench.write (framed (bytes ({ 0x59, 0x34, 0x12, 0x07 })));
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (bench.device->statistics ().malformed_requests, 1u);
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_FirstAcceptableValueMayBeTheUpperLimit)
{
  // ODD: 0 and the lower limit round to a raw value below the limit; the
  // upper limit is the first one the entry accepts (raw 5 of tenths).
  st::bench_t<lab> bench;
  st::outcome_ptr const out
      = bench.call<lab::PARAM_GET_t> (tables::lab_registry::ODD);
  ASSERT_FALSE (out->error) << out->error.message ();
  auto const value = layout::decode_reply_message<lab, lab::PARAM_GET_t> (
      st::view_of (out->frame));
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (field::get<1> (*value).raw, 5);
}

TEST (LumexDeviceSimulatorKinds, Simulator_EchoOfAWholeRecordIsTheRecord)
{
  st::bench_t<lab> bench;
  st::outcome_ptr const out = bench.call<lab::ECHO_WHOLE_t> (
      std::uint8_t (5), static_cast<std::uint16_t> (300));
  EXPECT_FALSE (out->error) << out->error.message ();
}

TEST (LumexDeviceSimulatorKinds,
      Simulator_TwoPhaseAndReplyRowRefuseAnEntryTheyCannotReport)
{
  st::bench_t<lab> bench;
  EXPECT_EQ (bench.call<lab::PARAM_PHASE_t> (tables::lab_registry::ARM)->error,
             code (protocol_errc::timeout));
  EXPECT_EQ (bench.call<lab::PARAM_NOTE_t> (tables::lab_registry::ARM)->error,
             code (protocol_errc::timeout));
  EXPECT_EQ (bench.device->statistics ().refused, 2u);
  st::outcome_ptr const phase
      = bench.call<lab::PARAM_PHASE_t> (tables::lab_registry::LEVEL);
  ASSERT_FALSE (phase->error) << phase->error.message ();
  EXPECT_EQ (phase->frame[0], 0x43);
  EXPECT_EQ (phase->frame[1], 0x02);
  st::outcome_ptr const note
      = bench.call<lab::PARAM_NOTE_t> (tables::lab_registry::LEVEL);
  ASSERT_FALSE (note->error) << note->error.message ();
  EXPECT_EQ (note->frame[0], 0x45);
  EXPECT_EQ (note->frame[1], 0x02);
}

TEST (LumexDeviceSimulatorKinds, Simulator_CableUnpluggedHangsTheLinkUp)
{
  st::bench_t<lab> bench;
  bench.device->set_behavior (behavior::cable_unplugged (ms (500)));
  st::outcome_ptr const out = bench.call<lab::INFO_t> ();
  EXPECT_EQ (out->error, code (protocol_errc::link_lost));
}

// --- the example of the header

TEST (LumexDeviceSimulatorKinds, Simulator_TheExampleOfTheHeader)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t options;
  options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), options);
  mock.stream.async_open ([] (std::error_code) {});
  st::drain (context);
  tr::testing::device_simulator<lab> device (
      mock.control, lumex::core::framing::slip::slip_codec (), clock);
  device.on<lab::INFO_t> (
      [] (tr::testing::device_simulator<lab>::request_of<lab::INFO_t> const &)
        {
          return tr::testing::device_simulator<lab>::reply_of<lab::INFO_t> ();
        });
  EXPECT_TRUE (device.start_stream<lab::STATE_t> ());
  EXPECT_TRUE (device.valid ());
}

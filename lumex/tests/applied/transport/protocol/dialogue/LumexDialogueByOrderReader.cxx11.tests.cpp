// A by_order exchange with a reader asks it whether a frame is a part of the
// reply: a frame that is not goes on as a frame nobody waits for, and the
// exchange keeps waiting. Each reply kind, the retries and the late window,
// the counters, and one call of the reader per frame.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace layout = ft::layout;
namespace field = lumex::core::protocol::field;
using ft::bytes;
using ft::bytes_t;
using ft::manual_bench_t;
using ft::ms;
using ft::outcome_ptr;
using ft::protocol_errc;
using ft::quiet_t;
using ft::status_frame;

// A report of a known row that no exchange waits for.
bytes_t const report = bytes ({ 0x33, 0x09 });

template <typename Row, typename... Args>
pr::exchange_t
exchange_of (std::uint32_t timeout_ms, std::uint32_t retries,
             Args const &...args)
{
  pr::exchange_t out = ft::make_exchange<quiet_t, Row> (args...);
  out.policy = lumex_dialogue_test::policy (timeout_ms, retries);
  return out;
}

// What a frame moved in the counters.
std::uint64_t
frames_counted (pr::dialogue_statistics_t const &s)
{
  return s.replies + s.late_replies + s.unknown_frames + s.events
         + s.unexpected_replies;
}

// A reader of one's own: counts its calls.
struct counting_reader_t
{
  static int &
  calls ()
  {
    static int count = 0;
    return count;
  }

  static pr::reply_reading_t &
  answer ()
  {
    static pr::reply_reading_t reading;
    return reading;
  }

  static pr::reply_reading_t
  read (layout::bytes_view_t, field::decode_context_t const *,
        layout::bytes_view_t)
  {
    ++calls ();
    return answer ();
  }

  static pr::reply_reading_t
  read_throwing (layout::bytes_view_t, field::decode_context_t const *,
                 layout::bytes_view_t)
  {
    ++calls ();
    throw std::bad_alloc ();
  }

  static void
  reset (pr::reply_reading_t reading)
  {
    calls () = 0;
    answer () = reading;
  }
};

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AStreamFrameReachesItsSubscriberAndTheExchangeThenSucceeds)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                           [&seen] (bytes_t const &) { ++seen; });
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (1000, 0));
  bench.settle ();
  bench.deliver (status_frame (0));
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->frame, bytes ({ 0x06 }));
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AReportReachesItsSubscriberAndTheExchangeThenSucceeds)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  std::vector<bytes_t> seen;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::QUERY_t> (),
                           [&seen] (bytes_t const &frame)
                             { seen.push_back (frame); });
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (1000, 0));
  bench.settle ();
  bench.deliver (report);
  ASSERT_EQ (seen.size (), 1u);
  EXPECT_EQ (seen[0], report);
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 1u);
  EXPECT_EQ (bench.engine->statistics ().replies, 0u);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.engine->statistics ().replies, 1u);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AFrameOfNoRowGoesToOnUnknownAndTheExchangeWaits)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int unknown = 0;
  bench.engine->on_unknown ([&unknown] (bytes_t const &) { ++unknown; });
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (1000, 0));
  bench.settle ();
  bench.deliver (bytes ({ 0x7E, 0x01 }));
  EXPECT_EQ (unknown, 1);
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().unknown_frames, 1u);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueByOrderReaderTest, ByOrder_AWrongShapeEndsInTimeout)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (100, 0));
  bench.settle ();
  // The key of the row itself is no part when both parts are rows.
  bench.deliver (bytes ({ 0x40 }));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (99));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_TRUE (out->frame.empty ());
}

TEST (LumexDialogueByOrderReaderTest, ByOrder_WithoutAReaderTakesTheNextFrame)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::QUERY_t> (),
                           [&seen] (bytes_t const &) { ++seen; });
  outcome_ptr const known
      = bench.start_exchange (exchange_of<quiet_t::ORDERED_t> (1000, 0));
  bench.settle ();
  bench.deliver (report);
  ASSERT_EQ (known->calls, 1);
  EXPECT_FALSE (known->error);
  EXPECT_EQ (known->frame, report);
  EXPECT_EQ (seen, 0);
  // A frame of no row too; only an event goes past.
  outcome_ptr const nothing
      = bench.start_exchange (exchange_of<quiet_t::ORDERED_t> (1000, 0));
  bench.settle ();
  bench.deliver (status_frame (0));
  EXPECT_EQ (nothing->calls, 0);
  bench.deliver (bytes ({ 0x7E }));
  ASSERT_EQ (nothing->calls, 1);
  EXPECT_FALSE (nothing->error);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AWrongShapeThenTheTimeoutRepeatsAndTheRepeatSucceeds)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (100, 1));
  bench.settle ();
  bench.deliver (report);
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x40), 2u);
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().timeouts, 1u);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AWrongShapeOfEveryAttemptEndsInRetriesExhausted)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (100, 1));
  bench.settle ();
  bench.deliver (report);
  bench.advance (ms (100));
  bench.deliver (report);
  bench.advance (ms (100));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::retries_exhausted);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_TheLateWindowStillDropsAndTheNextAttemptAsksAgain)
{
  manual_bench_t<quiet_t> bench;
  bench.options.late_reply_window_ms = 50;
  bench.start ();
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::QUERY_t> (),
                           [&seen] (bytes_t const &) { ++seen; });
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (100, 1));
  bench.settle ();
  bench.advance (ms (100));
  // In the window: a part and another frame are both late; nothing is sent.
  EXPECT_EQ (bench.count (0x40), 1u);
  bench.deliver (bytes ({ 0x06 }));
  bench.deliver (report);
  EXPECT_EQ (bench.engine->statistics ().late_replies, 2u);
  EXPECT_EQ (seen, 0);
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x40), 2u);
  // After it a report is passed on again, and the part answers.
  bench.deliver (report);
  EXPECT_EQ (seen, 1);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_ALateRowIsCountedLateAndNotShownToTheReader)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.start_exchange (exchange_of<quiet_t::QUERY_t> (100, 0));
  bench.settle ();
  bench.advance (ms (100));
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (1000, 0));
  bench.settle ();
  bench.deliver (report);
  EXPECT_EQ (bench.engine->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 0u);
  // Only the first: the next one is a report.
  bench.deliver (report);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 1u);
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueByOrderReaderTest,
      ByOrder_AnEventOrALateRowDoesNotReachTheReader)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.start_exchange (exchange_of<quiet_t::QUERY_t> (100, 0));
  bench.settle ();
  bench.advance (ms (100));
  counting_reader_t::reset (pr::reply_reading_t ());
  pr::exchange_t request = exchange_of<quiet_t::STOP_t> (1000, 0);
  request.reader = &counting_reader_t::read;
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (status_frame (0));
  EXPECT_EQ (counting_reader_t::calls (), 0);
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueByOrderReaderTest,
      AckNak_FramesThatAreNoPartLeaveTheAttemptsAsTheyWere)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::STOP_t> (1000, 1));
  bench.settle ();
  bench.deliver (report);
  EXPECT_EQ (bench.count (0x40), 1u);
  EXPECT_EQ (bench.engine->statistics ().naks, 0u);
  // The NAK is a part: the request goes again.
  bench.deliver (bytes ({ 0x15, 0x05 }));
  EXPECT_EQ (bench.engine->statistics ().naks, 1u);
  EXPECT_EQ (bench.count (0x40), 2u);
  bench.deliver (report);
  EXPECT_EQ (bench.count (0x40), 2u);
  EXPECT_EQ (bench.engine->statistics ().naks, 1u);
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.engine->statistics ().replies, 2u);
}

TEST (LumexDialogueByOrderReaderTest,
      TwoPhase_AFrameThatIsNoPartBeforeTheAckLeavesItUnacknowledged)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out = bench.start_exchange (
      exchange_of<quiet_t::PRIME_t> (100, 1, std::uint16_t (5)));
  bench.settle ();
  bench.advance (ms (50));
  bench.deliver (report);
  // Its timer still counts from the write, and the request may go again.
  bench.advance (ms (50));
  EXPECT_EQ (bench.count (0x41), 2u);
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueByOrderReaderTest,
      TwoPhase_AFrameThatIsNoPartAfterTheAckNeitherEndsNorExtendsTheWait)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out = bench.start_exchange (
      exchange_of<quiet_t::PRIME_t> (100, 1, std::uint16_t (5)));
  bench.settle ();
  bench.advance (ms (50));
  bench.deliver (bytes ({ 0x06 }));
  // A new time from the acknowledgement; a report does not move it.
  bench.advance (ms (50));
  bench.deliver (report);
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (49));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (bench.count (0x41), 1u);
}

TEST (LumexDialogueByOrderReaderTest,
      TwoPhase_TheCompletionComesThroughFramesThatAreNoPart)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out = bench.start_exchange (
      exchange_of<quiet_t::PRIME_t> (1000, 0, std::uint16_t (5)));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (bytes ({ 0x06 }));
  bench.deliver (report);
  // Too short for the completion: no part either.
  bench.deliver (bytes ({ 0x41, 0x01 }));
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x41, 0x01, 0x00 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->frame, bytes ({ 0x41, 0x01, 0x00 }));
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 3u);
  EXPECT_EQ (bench.engine->statistics ().replies, 2u);
}

TEST (LumexDialogueByOrderReaderTest,
      Echo_AnotherRowOrAShortFrameIsNoPartAndAWrongValueStillRefuses)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const refused
      = bench.start_exchange (exchange_of<quiet_t::ECHOED_t> (1000, 0, 7));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (bytes ({ 0x42 }));
  EXPECT_EQ (refused->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 2u);
  // It decodes: a part, and the value is not the one sent.
  bench.deliver (bytes ({ 0x42, 0x08 }));
  ASSERT_EQ (refused->calls, 1);
  EXPECT_EQ (refused->error, protocol_errc::not_confirmed);
  outcome_ptr const good
      = bench.start_exchange (exchange_of<quiet_t::ECHOED_t> (1000, 0, 7));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (bytes ({ 0x42, 0x07 }));
  ASSERT_EQ (good->calls, 1);
  EXPECT_FALSE (good->error);
}

TEST (LumexDialogueByOrderReaderTest,
      Echo_ZeroOnErrorAndStateInsteadOfRefusalPassOnWhatDoesNotDecode)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const zero
      = bench.start_exchange (exchange_of<quiet_t::ZEROED_t> (1000, 0, 7));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (bytes ({ 0x43 }));
  EXPECT_EQ (zero->calls, 0);
  bench.deliver (bytes ({ 0x43, 0x00 }));
  ASSERT_EQ (zero->calls, 1);
  EXPECT_EQ (zero->error, protocol_errc::refused);
  outcome_ptr const stated
      = bench.start_exchange (exchange_of<quiet_t::STATED_t> (1000, 0, 7));
  bench.settle ();
  bench.deliver (report);
  bench.deliver (bytes ({ 0x44 }));
  EXPECT_EQ (stated->calls, 0);
  bench.deliver (bytes ({ 0x44, 0x03 }));
  ASSERT_EQ (stated->calls, 1);
  EXPECT_EQ (stated->error, protocol_errc::refused);
}

TEST (LumexDialogueByOrderReaderTest,
      Reader_ASingleCallDecidesTheFrameAndWhatItDoes)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::reply_reading_t reading;
  counting_reader_t::reset (reading);
  pr::exchange_t request = exchange_of<quiet_t::STOP_t> (1000, 0);
  request.reader = &counting_reader_t::read;
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (counting_reader_t::calls (), 1);
}

TEST (LumexDialogueByOrderReaderTest, Reader_ASingleCallForAFrameItPassesOn)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::reply_reading_t reading;
  reading.effect = pr::reply_effect::fail;
  reading.error = lumex::core::protocol::error::make_error_code (
      protocol_errc::unexpected_reply);
  reading.is_part = false;
  counting_reader_t::reset (reading);
  pr::exchange_t request = exchange_of<quiet_t::STOP_t> (1000, 0);
  request.reader = &counting_reader_t::read;
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  bench.deliver (report);
  EXPECT_EQ (counting_reader_t::calls (), 1);
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 1u);
}

TEST (LumexDialogueByOrderReaderTest,
      Reader_AFailureThatIsAPartEndsTheExchangeWithItsError)
{
  // A reader of one's own says nothing of is_part: its verdict stands, also
  // unexpected_reply.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::reply_reading_t reading;
  reading.effect = pr::reply_effect::fail;
  reading.error = lumex::core::protocol::error::make_error_code (
      protocol_errc::unexpected_reply);
  counting_reader_t::reset (reading);
  pr::exchange_t request = exchange_of<quiet_t::STOP_t> (1000, 0);
  request.reader = &counting_reader_t::read;
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  bench.deliver (report);
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::unexpected_reply);
  EXPECT_EQ (counting_reader_t::calls (), 1);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 0u);
}

TEST (LumexDialogueByOrderReaderTest,
      Reader_AThrowingReaderEndsTheExchangeAndIsCalledOnce)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  counting_reader_t::reset (pr::reply_reading_t ());
  pr::exchange_t request = exchange_of<quiet_t::STOP_t> (1000, 0);
  request.reader = &counting_reader_t::read_throwing;
  outcome_ptr const out = bench.start_exchange (std::move (request));
  bench.settle ();
  bench.deliver (bytes ({ 0x06 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, std::make_error_code (std::errc::not_enough_memory));
  EXPECT_EQ (counting_reader_t::calls (), 1);
}

TEST (LumexDialogueByOrderReaderTest,
      Reader_AByKeyRowStillEndsWithTheErrorOfTheDecoder)
{
  // Its frame is known to be its reply: a wrong shape is no reason to wait.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out
      = bench.start_exchange (exchange_of<quiet_t::RETRIED_t> (1000, 0));
  bench.settle ();
  bench.deliver (bytes ({ 0x36 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_TRUE (static_cast<bool> (out->error));
  EXPECT_NE (out->error, protocol_errc::timeout);
}

TEST (LumexDialogueByOrderReaderTest,
      Counters_EachFrameMovesExactlyOneDuringAnExchange)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  outcome_ptr const out = bench.start_exchange (
      exchange_of<quiet_t::PRIME_t> (100000, 0, std::uint16_t (5)));
  bench.settle ();
  std::vector<bytes_t> const frames
      = { report,           status_frame (0),    bytes ({ 0x7E }),
          bytes ({ 0x41 }), bytes ({ 0x06 }),    bytes ({ 0x06 }),
          report,           bytes ({ 0x15, 1 }), bytes ({ 0x41, 1, 0 }) };
  std::uint64_t counted = frames_counted (bench.engine->statistics ());
  for (std::size_t i = 0; i < frames.size (); ++i)
    {
      bench.deliver (frames[i]);
      std::uint64_t const now = frames_counted (bench.engine->statistics ());
      EXPECT_EQ (now, counted + 1) << "frame " << i;
      counted = now;
    }
  EXPECT_EQ (out->calls, 1);
}
} // namespace

// The frame queue of framed_stream (bounded, drop the oldest, one warning per
// burst, a counter), the framing errors (counted per framing_errc and logged,
// never handed to the reader), the statistics (frames, bytes, reset, the
// transport's own), and the log records, over the mock stream with SLIP, a
// header codec with a CRC and a pipeline. Synthetic frames.

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace
{
using namespace lumex_framed_test;
using tr::log::log_level;

using queue_case = slip_case;

bytes_t
wire_of_frames (std::size_t n, std::uint8_t first = 0)
{
  return slip_wire (n, first);
}

class LumexFramedStreamQueueTest : public ::testing::Test
{
protected:
  void
  make_rig (std::size_t bound)
  {
    tr::framed::framed_options_t options;
    options.queue_bound = bound;
    rig_.reset (new stream_rig<queue_case> (queue_case::limit (), options));
    rig_->open ();
  }

  bool
  frames_seen (std::uint64_t count)
  {
    return eventually (
        [this, count] { return rig_->link.statistics ().frames_in == count; });
  }

  std::uint8_t
  read_value ()
  {
    tr::framed::frame_result_t const frame
        = rig_->link.read_frame (tr::deadline_t (2000));
    EXPECT_TRUE (frame.has_value ());
    if (!frame.has_value () || frame.value ().size () != 1)
      return 0xEE;
    return frame.value ()[0];
  }

  log_capture log_;
  std::unique_ptr<stream_rig<queue_case>> rig_;
};
} // namespace

TEST_F (LumexFramedStreamQueueTest,
        GivenAFullQueue_WhenMoreFramesArrive_ThenTheOldestAreDroppedAndCounted)
{
  make_rig (3);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (10));
  ASSERT_TRUE (frames_seen (10));
  tr::framed::framed_statistics_t const stats = rig_->link.statistics ();
  EXPECT_EQ (stats.frames_in, 10u);
  EXPECT_EQ (stats.dropped_frames, 7u);
  EXPECT_EQ (rig_->link.queued_frames (), 3u);
  EXPECT_EQ (read_value (), 7);
  EXPECT_EQ (read_value (), 8);
  EXPECT_EQ (read_value (), 9);
  EXPECT_EQ (rig_->link.queued_frames (), 0u);
}

TEST_F (LumexFramedStreamQueueTest,
        GivenTenOverflowingFrames_WhenTheyArrive_ThenOneWarningIsLogged)
{
  make_rig (3);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (10));
  ASSERT_TRUE (frames_seen (10));
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 1u)
      << log_capture::dump ();
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenABurstThatEnded_WhenTheQueueOverflowsAgain_ThenASecondWarningIsLogged)
{
  make_rig (4);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (6));
  ASSERT_TRUE (frames_seen (6));
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 1u);
  // The reader takes the queue down to half of the bound: the burst is over.
  EXPECT_EQ (read_value (), 2);
  EXPECT_EQ (read_value (), 3);
  EXPECT_EQ (read_value (), 4);
  EXPECT_EQ (rig_->link.queued_frames (), 1u);
  rig_->mock.control.feed (wire_of_frames (4, 20));
  ASSERT_TRUE (frames_seen (10));
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 3u);
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 2u)
      << log_capture::dump ();
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenABurstThatTheReaderHasNotEnded_WhenMoreFramesOverflow_ThenNoSecondWarning)
{
  make_rig (4);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (6));
  ASSERT_TRUE (frames_seen (6));
  // One frame taken: three are left, above half of the bound.
  EXPECT_EQ (read_value (), 2);
  rig_->mock.control.feed (wire_of_frames (3, 30));
  ASSERT_TRUE (frames_seen (9));
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 4u);
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 1u)
      << log_capture::dump ();
}

TEST_F (LumexFramedStreamQueueTest,
        GivenAQueueBoundOfZero_WhenFramesArrive_ThenOneFrameIsKept)
{
  make_rig (0);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (3));
  ASSERT_TRUE (frames_seen (3));
  EXPECT_EQ (rig_->link.queued_frames (), 1u);
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 2u);
  EXPECT_EQ (read_value (), 2);
}

TEST_F (LumexFramedStreamQueueTest,
        GivenAReaderThatKeepsUp_WhenFramesArriveOneByOne_ThenNothingIsDropped)
{
  make_rig (1);
  for (std::uint8_t i = 0; i < 20; ++i)
    {
      rig_->mock.control.feed (wire_of_frames (1, i));
      EXPECT_EQ (read_value (), i);
    }
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 0u);
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 0u);
}

TEST_F (LumexFramedStreamQueueTest,
        GivenFramesBeforeAnyRead_WhenStartedEarly_ThenTheyWaitInOrder)
{
  make_rig (16);
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (5));
  ASSERT_TRUE (frames_seen (5));
  EXPECT_EQ (rig_->link.queued_frames (), 5u);
  for (std::uint8_t i = 0; i < 5; ++i)
    EXPECT_EQ (read_value (), i);
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 0u);
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenNoStartAndNoRead_WhenBytesArrive_ThenTheDecoderLeavesThemInTheStream)
{
  make_rig (16);
  rig_->mock.control.feed (wire_of_frames (2));
  std::this_thread::sleep_for (ms (40));
  EXPECT_EQ (rig_->link.statistics ().bytes_in, 0u);
  EXPECT_EQ (read_value (), 0);
  EXPECT_EQ (read_value (), 1);
}

// --- the framing errors
// ------------------------------------------------------

TEST (
    LumexFramedStreamErrorsTest,
    GivenFramesWithBadChecksums_WhenTheyArrive_ThenTheyAreCountedLoggedOnceAndNotHandedToTheReader)
{
  log_capture log;
  stream_rig<header_crc_case> rig;
  rig.open ();
  header_crc_case::codec_t const codec
      = header_crc_case::make (header_crc_case::limit ());
  bytes_t const good = encode_direct (codec, header_crc_case::payload (1));
  bytes_t broken = good;
  broken[broken.size () - 4] ^= 0x55;
  bytes_t stream = good;
  for (int i = 0; i < 3; ++i)
    stream = concat (stream, broken);
  stream = concat (stream, good);
  rig.mock.control.feed (stream);
  for (int i = 0; i < 2; ++i)
    {
      tr::framed::frame_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
      EXPECT_EQ (frame.value (), header_crc_case::payload (1));
    }
  EXPECT_FALSE (rig.link.read_frame (tr::deadline_t (30)).has_value ());
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.errors_of (framing_errc::bad_checksum), 3u);
  EXPECT_EQ (stats.total_errors (), 3u);
  EXPECT_EQ (stats.frames_in, 2u);
  // One warning for the burst, a debug line for each of the others.
  EXPECT_EQ (log_capture::count (log_level::warning, "dropped a frame"), 1u)
      << log_capture::dump ();
  EXPECT_EQ (log_capture::count (log_level::debug, "dropped a frame"), 2u);
}

TEST (
    LumexFramedStreamErrorsTest,
    GivenAGoodFrameBetweenTwoBursts_WhenErrorsRepeat_ThenEachBurstGetsItsWarning)
{
  log_capture log;
  stream_rig<header_crc_case> rig;
  rig.open ();
  header_crc_case::codec_t const codec
      = header_crc_case::make (header_crc_case::limit ());
  bytes_t const good = encode_direct (codec, header_crc_case::payload (2));
  bytes_t broken = good;
  broken[broken.size () - 3] ^= 0x01;
  rig.mock.control.feed (
      concat (concat (broken, broken), concat (good, concat (broken, good))));
  ASSERT_TRUE (rig.link.read_frame (tr::deadline_t (2000)).has_value ());
  ASSERT_TRUE (rig.link.read_frame (tr::deadline_t (2000)).has_value ());
  EXPECT_EQ (rig.link.statistics ().errors_of (framing_errc::bad_checksum),
             3u);
  EXPECT_EQ (log_capture::count (log_level::warning, "dropped a frame"), 2u)
      << log_capture::dump ();
}

TEST (
    LumexFramedStreamErrorsTest,
    GivenEveryKindOfBadFrame_WhenTheyArrive_ThenEachErrorIsCountedUnderItsCode)
{
  log_capture log;
  stream_rig<pipeline_case> rig;
  rig.open ();
  std::string text;
  text += "AB\r";      // no '$' prefix: bad_header
  text += "$0\r";      // shorter than the check: truncated
  text += "$ZZB4\r";   // a good sum of 'ZZ', which is no hex: bad_escape
  text += "$0000FF\r"; // a wrong sum: bad_checksum
  bytes_t stream (text.begin (), text.end ());
  stream = concat (stream, pipeline_case::beyond_limit ());
  bytes_t const good
      = encode_direct (pipeline_case::make (pipeline_case::limit ()),
                       pipeline_case::payload (2));
  stream = concat (stream, good);
  rig.mock.control.feed (stream);
  tr::framed::frame_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (frame.value (), pipeline_case::payload (2));
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.errors_of (framing_errc::bad_header), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::truncated), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::bad_escape), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::bad_checksum), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::frame_too_large), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::payload_too_large), 0u);
  EXPECT_EQ (stats.total_errors (), 5u);
  EXPECT_EQ (stats.other_errors, 0u);
  // The refusal of encode is the sixth code.
  bytes_t const big = pipeline_case::oversized ();
  EXPECT_FALSE (
      rig.link.write_frame (tr::const_bytes_t (big), tr::deadline_t (2000))
          .has_value ());
  EXPECT_EQ (
      rig.link.statistics ().errors_of (framing_errc::payload_too_large), 1u);
}

TEST (LumexFramedStreamErrorsTest,
      GivenAnErrorCodeOfAnotherCategory_WhenCounted_ThenItGoesToOtherErrors)
{
  tr::framed::framed_counters counters;
  counters.add_error (std::make_error_code (std::errc::invalid_argument));
  counters.add_error (
      tr::error::make_error_code (tr::error::transport_errc::timeout));
  counters.add_error (fr::codec::make_error_code (framing_errc::bad_header));
  // A value that is in the range of framing_errc, with another category.
  counters.add_error (std::error_code (2, std::generic_category ()));
  tr::framed::framed_statistics_t const stats = counters.snapshot ();
  EXPECT_EQ (stats.other_errors, 3u);
  EXPECT_EQ (stats.errors_of (framing_errc::bad_header), 1u);
  EXPECT_EQ (stats.errors_of (framing_errc::bad_checksum), 0u);
  EXPECT_EQ (stats.total_errors (), 4u);
}

// --- statistics and the log
// --------------------------------------------------

TEST_F (LumexFramedStreamQueueTest,
        GivenTraffic_WhenTheStatisticsAreRead_ThenFramesAndBytesCountBothWays)
{
  make_rig (16);
  bytes_t const wire = wire_of_frames (3);
  rig_->mock.control.feed (wire);
  for (int i = 0; i < 3; ++i)
    read_value ();
  bytes_t const payload (4, 'x');
  ASSERT_TRUE (
      rig_->link
          .write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  tr::framed::framed_statistics_t const stats = rig_->link.statistics ();
  EXPECT_EQ (stats.frames_in, 3u);
  EXPECT_EQ (stats.bytes_in, wire.size ());
  EXPECT_EQ (stats.frames_out, 1u);
  EXPECT_EQ (stats.bytes_out, rig_->mock.control.written ().size ());
  EXPECT_EQ (stats.bytes_out, 6u);
  // The transport underneath counts its own side.
  tr::statistics::statistics_t const transport
      = rig_->link.transport_statistics ();
  EXPECT_EQ (transport.bytes_read, wire.size ());
  EXPECT_EQ (transport.bytes_written, 6u);
  EXPECT_EQ (transport.writes, 1u);
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenTraffic_WhenTheStatisticsAreReset_ThenBothTheAdapterAndTheTransportStartAtZero)
{
  make_rig (16);
  rig_->mock.control.feed (wire_of_frames (2));
  read_value ();
  read_value ();
  bytes_t const payload (4, 'x');
  ASSERT_TRUE (
      rig_->link
          .write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  ASSERT_NE (rig_->link.statistics ().frames_in, 0u);
  rig_->link.reset_statistics ();
  tr::framed::framed_statistics_t const stats = rig_->link.statistics ();
  EXPECT_EQ (stats.frames_in, 0u);
  EXPECT_EQ (stats.frames_out, 0u);
  EXPECT_EQ (stats.bytes_in, 0u);
  EXPECT_EQ (stats.bytes_out, 0u);
  EXPECT_EQ (stats.dropped_frames, 0u);
  EXPECT_EQ (stats.total_errors (), 0u);
  EXPECT_EQ (rig_->link.transport_statistics ().bytes_written, 0u);
  // Counting goes on after the reset.
  rig_->mock.control.feed (wire_of_frames (1));
  read_value ();
  EXPECT_EQ (rig_->link.statistics ().frames_in, 1u);
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenFrames_WhenTheyAreDecoded_ThenTheLogHasADebugLineAndATraceDumpPerChunk)
{
  make_rig (16);
  rig_->mock.control.feed (wire_of_frames (2));
  read_value ();
  read_value ();
  EXPECT_EQ (log_capture::count (log_level::debug, "decoded a frame of 1 B"),
             2u)
      << log_capture::dump ();
  EXPECT_GE (log_capture::count (log_level::trace, "received"), 1u);
  EXPECT_EQ (log_capture::count (log_level::warning, ""), 0u);
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenTheFramedComponentIsDenied_WhenTheQueueOverflows_ThenNothingIsLogged)
{
  make_rig (2);
  tr::log::set_component_filter (
      std::vector<std::string> (),
      std::vector<std::string> (1, "transport.framed"));
  rig_->link.start ();
  rig_->mock.control.feed (wire_of_frames (5));
  bool const seen = frames_seen (5);
  tr::log::set_component_filter (std::vector<std::string> (),
                                 std::vector<std::string> ());
  EXPECT_TRUE (seen);
  EXPECT_EQ (log_capture::count (log_level::warning, ""), 0u);
  EXPECT_EQ (rig_->link.statistics ().dropped_frames, 3u);
}

TEST_F (
    LumexFramedStreamQueueTest,
    GivenTheStreamEnds_WhenAReaderWaits_ThenItGetsTheErrorAndTheNextReadRestartsTheDecoder)
{
  make_rig (4);
  rig_->mock.control.disconnect ();
  tr::framed::frame_result_t first
      = rig_->link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (first.has_value ());
  EXPECT_TRUE (first.error () == tr::error::transport_errc::disconnected)
      << first.error ().message ();
  // A framing error is not the reader's, a transport error is; a reader that
  // comes later gets the same answer from the stream.
  tr::framed::frame_result_t second
      = rig_->link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (second.has_value ());
  EXPECT_TRUE (second.error () == tr::error::transport_errc::disconnected);
  EXPECT_EQ (rig_->link.statistics ().total_errors (), 0u);
}

TEST (
    LumexFramedStreamOpenTest,
    GivenAStreamThatWasNeverOpened_WhenRead_ThenNotOpenComesBackAndOpeningFixesIt)
{
  stream_rig<queue_case> rig;
  tr::framed::frame_result_t const first
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (first.has_value ());
  EXPECT_TRUE (first.error () == tr::error::transport_errc::not_open)
      << first.error ().message ();
  rig.open ();
  rig.mock.control.feed (wire_of_frames (1, 7));
  tr::framed::frame_result_t const second
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (second.has_value ()) << second.error ().message ();
  EXPECT_EQ (second.value (), bytes_t (1, 7));
}

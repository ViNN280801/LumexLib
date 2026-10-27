// framed_datagram over the mock datagram transport (synthetic frames): the
// codec is applied to each datagram on its own (a partial frame is never
// completed by the next datagram), a datagram may hold several frames, the
// report ID travels with the frame both ways, a datagram without a complete
// frame counts truncated, an empty one is ignored, the queue drops the
// oldest, and the contracts of the adapters (tokens, cancellation, close)
// hold as for the stream.

#include <cstddef>
#include <cstdint>
#include <future>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace
{
using namespace lumex_framed_test;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::log::log_level;

template <typename Case>
class LumexFramedDatagramCodecTest : public ::testing::Test
{
protected:
  log_capture log_;
};

typedef ::testing::Types<slip_case, delimiter_case, length_case,
                         header_crc_case, pipeline_case>
    datagram_cases;
TYPED_TEST_SUITE (LumexFramedDatagramCodecTest, datagram_cases);

message_t
message_of (bytes_t payload, std::uint8_t report_id = 0)
{
  message_t message;
  message.report_id = report_id;
  message.payload = std::move (payload);
  return message;
}
} // namespace

TYPED_TEST (
    LumexFramedDatagramCodecTest,
    GivenOneFramePerDatagram_WhenTheyArrive_ThenEveryPayloadComesBackWithItsReportId)
{
  datagram_rig<TypeParam> rig;
  rig.open ();
  for (std::size_t i = 0; i < 4; ++i)
    rig.mock.control.deliver (
        message_of (encode_direct (TypeParam::make (TypeParam::limit ()),
                                   TypeParam::payload (i)),
                    static_cast<std::uint8_t> (i + 1)));
  for (std::size_t i = 0; i < 4; ++i)
    {
      tr::datagram::message_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
      EXPECT_EQ (frame.value ().payload, TypeParam::payload (i));
      EXPECT_EQ (frame.value ().report_id, i + 1);
    }
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.frames_in, 4u);
  EXPECT_EQ (stats.total_errors (), 0u);
}

TYPED_TEST (LumexFramedDatagramCodecTest,
            GivenAPayload_WhenWritten_ThenOneMessageHoldsTheFrameOfTheCodec)
{
  datagram_rig<TypeParam> rig;
  rig.open ();
  bytes_t const payload = TypeParam::payload (1);
  ASSERT_TRUE (
      rig.link.write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  ASSERT_TRUE (
      rig.link
          .write_frame (7, tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  std::vector<message_t> const sent = rig.mock.control.sent ();
  ASSERT_EQ (sent.size (), 2u);
  bytes_t const wire
      = encode_direct (TypeParam::make (TypeParam::limit ()), payload);
  EXPECT_EQ (sent[0].payload, wire);
  EXPECT_EQ (sent[0].report_id, 0u);
  EXPECT_EQ (sent[1].payload, wire);
  EXPECT_EQ (sent[1].report_id, 7u);
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.frames_out, 2u);
  EXPECT_EQ (stats.bytes_out, 2 * wire.size ());
}

TYPED_TEST (LumexFramedDatagramCodecTest,
            GivenAPayloadTheCodecRefuses_WhenWritten_ThenNothingIsSent)
{
  datagram_rig<TypeParam> rig;
  rig.open ();
  bytes_t const big = TypeParam::oversized ();
  tr::status_result_t const result
      = rig.link.write_frame (tr::const_bytes_t (big), tr::deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (rig.mock.control.sent ().empty ());
  EXPECT_EQ (
      rig.link.statistics ().errors_of (framing_errc::payload_too_large), 1u);
}

TYPED_TEST (
    LumexFramedDatagramCodecTest,
    GivenAFrameBeyondTheLimit_WhenAGoodDatagramFollows_ThenTheErrorIsCountedAndTheGoodFrameArrives)
{
  datagram_rig<TypeParam> rig;
  rig.open ();
  rig.mock.control.deliver (message_of (TypeParam::beyond_limit ()));
  rig.mock.control.deliver (message_of (encode_direct (
      TypeParam::make (TypeParam::limit ()), TypeParam::payload (0))));
  tr::datagram::message_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (frame.value ().payload, TypeParam::payload (0));
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_GE (stats.total_errors (), 1u);
  EXPECT_EQ (stats.frames_in, 1u);
}

TEST (
    LumexFramedDatagramTest,
    GivenAFrameCutIntoTwoDatagrams_WhenTheyArrive_ThenNeitherCompletesItAndBothCountTruncated)
{
  log_capture log;
  datagram_rig<header_case> rig;
  rig.open ();
  rig.link.start ();
  bytes_t const wire = encode_direct (
      header_case::make (header_case::limit ()), header_case::payload (2));
  ASSERT_GT (wire.size (), 10u);
  // The header and two bytes of the data, then the rest of the data.
  rig.mock.control.deliver (
      message_of (bytes_t (wire.begin (), wire.begin () + 8)));
  rig.mock.control.deliver (
      message_of (bytes_t (wire.begin () + 8, wire.end ())));
  ASSERT_TRUE (eventually (
      [&rig]
        {
          return rig.link.statistics ().errors_of (framing_errc::truncated)
                 == 2u;
        }));
  EXPECT_EQ (rig.link.statistics ().frames_in, 0u);
  EXPECT_FALSE (rig.link.read_frame (tr::deadline_t (30)).has_value ());
  // The decoder was not left in the middle of the first frame: a whole
  // frame in the next datagram is read.
  rig.mock.control.deliver (message_of (wire));
  tr::datagram::message_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (frame.value ().payload, header_case::payload (2));
}

TEST (
    LumexFramedDatagramTest,
    GivenADatagramWithTwoFrames_WhenItArrives_ThenTheyAreTwoFramesWithTheSameReportId)
{
  log_capture log;
  datagram_rig<slip_case> rig;
  rig.open ();
  slip_case::codec_t const codec = slip_case::make (slip_case::limit ());
  rig.mock.control.deliver (
      message_of (concat (encode_direct (codec, bytes_t (1, 'a')),
                          encode_direct (codec, bytes_t (2, 'b'))),
                  9));
  tr::datagram::message_result_t const first
      = rig.link.read_frame (tr::deadline_t (2000));
  tr::datagram::message_result_t const second
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (first.has_value ());
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (first.value ().payload, bytes_t (1, 'a'));
  EXPECT_EQ (second.value ().payload, bytes_t (2, 'b'));
  EXPECT_EQ (first.value ().report_id, 9u);
  EXPECT_EQ (second.value ().report_id, 9u);
}

TEST (
    LumexFramedDatagramTest,
    GivenADatagramWithoutAFrame_WhenItArrives_ThenItCountsTruncatedAndAnEmptyOneNothing)
{
  log_capture log;
  datagram_rig<delimiter_case> rig;
  rig.open ();
  rig.mock.control.deliver (message_of (letters (10)));
  rig.mock.control.deliver (message_of (bytes_t ()));
  // A frame behind them proves both were taken.
  rig.mock.control.deliver (message_of (
      encode_direct (delimiter_case::make (delimiter_case::limit ()),
                     delimiter_case::payload (0))));
  tr::datagram::message_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ());
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.errors_of (framing_errc::truncated), 1u);
  EXPECT_EQ (stats.total_errors (), 1u);
  EXPECT_EQ (stats.frames_in, 1u);
}

TEST (
    LumexFramedDatagramTest,
    GivenAFullQueue_WhenMoreFramesArrive_ThenTheOldestAreDroppedWithOneWarning)
{
  log_capture log;
  tr::framed::framed_options_t options;
  options.queue_bound = 3;
  datagram_rig<slip_case> rig (slip_case::limit (), options);
  rig.open ();
  rig.link.start ();
  slip_case::codec_t const codec = slip_case::make (slip_case::limit ());
  for (std::uint8_t i = 0; i < 9; ++i)
    rig.mock.control.deliver (
        message_of (encode_direct (codec, bytes_t (1, i)),
                    static_cast<std::uint8_t> (i + 20)));
  ASSERT_TRUE (
      eventually ([&rig] { return rig.link.statistics ().frames_in == 9u; }));
  EXPECT_EQ (rig.link.statistics ().dropped_frames, 6u);
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 1u)
      << log_capture::dump ();
  for (std::uint8_t i = 6; i < 9; ++i)
    {
      tr::datagram::message_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      ASSERT_TRUE (frame.has_value ());
      EXPECT_EQ (frame.value ().payload, bytes_t (1, i));
      EXPECT_EQ (frame.value ().report_id, i + 20);
    }
}

TEST (LumexFramedDatagramTest,
      GivenTraffic_WhenTheStatisticsAreRead_ThenTheTransportCountsItsMessages)
{
  log_capture log;
  datagram_rig<slip_case> rig;
  rig.open ();
  slip_case::codec_t const codec = slip_case::make (slip_case::limit ());
  bytes_t const wire = encode_direct (codec, bytes_t (3, 'x'));
  rig.mock.control.deliver (message_of (wire));
  ASSERT_TRUE (rig.link.read_frame (tr::deadline_t (2000)).has_value ());
  bytes_t const payload (2, 'y');
  ASSERT_TRUE (
      rig.link.write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  tr::statistics::statistics_t const transport
      = rig.link.transport_statistics ();
  EXPECT_EQ (transport.messages_received, 1u);
  EXPECT_EQ (transport.messages_sent, 1u);
  EXPECT_EQ (rig.link.statistics ().bytes_in, wire.size ());
}

TEST (LumexFramedDatagramTest,
      GivenTheTransportEnds_WhenAReaderWaits_ThenItGetsTheError)
{
  log_capture log;
  datagram_rig<slip_case> rig;
  rig.open ();
  rig.mock.control.disconnect ();
  tr::datagram::message_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (frame.has_value ());
  EXPECT_TRUE (frame.error () == transport_errc::disconnected)
      << frame.error ().message ();
}

TEST (LumexFramedDatagramTest,
      GivenASendThatFails_WhenWritten_ThenTheErrorComesBackAndTheNextIsSent)
{
  log_capture log;
  datagram_rig<slip_case> rig;
  rig.open ();
  bytes_t const payload (2, 'y');
  rig.mock.control.fail_next_send (std::make_error_code (std::errc::io_error));
  tr::status_result_t const failed = rig.link.write_frame (
      tr::const_bytes_t (payload), tr::deadline_t (2000));
  ASSERT_FALSE (failed.has_value ());
  EXPECT_TRUE (failed.error () == std::errc::io_error);
  EXPECT_EQ (rig.link.statistics ().frames_out, 0u);
  EXPECT_TRUE (
      rig.link.write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
          .has_value ());
  EXPECT_EQ (rig.mock.control.sent ().size (), 1u);
}

// --- tokens, cancellation and destruction on an io_context the test polls
// ----

namespace
{
class LumexFramedDatagramPolledTest : public ::testing::Test
{
protected:
  void
  settle ()
  {
    context_.restart ();
    context_.poll ();
  }

  void
  make ()
  {
    mock_.reset (new tr::testing::mock_datagram (
        tr::testing::make_mock_datagram (context_.get_executor (), "dg")));
    link_.reset (new tr::framed::framed_datagram<slip_case::codec_t> (
        std::move (mock_->transport), slip_case::make (slip_case::limit ())));
    seen opened;
    link_->transport ().async_open (status_recorder{ &opened });
    settle ();
    ASSERT_EQ (opened.calls, 1);
  }

  asio::io_context context_;
  log_capture log_;
  std::unique_ptr<tr::testing::mock_datagram> mock_;
  std::unique_ptr<tr::framed::framed_datagram<slip_case::codec_t>> link_;
  asio::cancellation_signal signal_;
  bytes_t payload_ = bytes_t (3, 'k');
};
} // namespace

TEST_F (
    LumexFramedDatagramPolledTest,
    GivenAPendingRead_WhenTerminalIsEmitted_ThenItEndsCancelledAndTheNextFrameStillComes)
{
  make ();
  seen read;
  link_->async_read_frame (asio::bind_cancellation_slot (
      signal_.slot (), message_recorder{ &read }));
  settle ();
  ASSERT_EQ (read.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  mock_->control.deliver (message_of (slip_wire (1, 5), 3));
  settle ();
  seen next;
  link_->async_read_frame (message_recorder{ &next });
  settle ();
  ASSERT_EQ (next.calls, 1);
  EXPECT_FALSE (next.error);
  EXPECT_EQ (next.frame, bytes_t (1, 5));
  EXPECT_EQ (next.report_id, 3u);
}

TEST_F (
    LumexFramedDatagramPolledTest,
    GivenAFinishedWrite_WhenItsSignalIsEmittedLater_ThenAPendingReadStaysPending)
{
  make ();
  seen read;
  seen write;
  link_->async_read_frame (message_recorder{ &read });
  settle ();
  // The mock sends at once, so the write completes; a signal after that
  // reaches nothing, the read stays pending.
  link_->async_write_frame (4, tr::const_bytes_t (payload_),
                            asio::bind_cancellation_slot (
                                signal_.slot (), status_recorder{ &write }));
  settle ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_FALSE (write.error);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (read.calls, 0);
  std::vector<message_t> const sent = mock_->control.sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 4u);
}

TEST_F (LumexFramedDatagramPolledTest,
        GivenPendingReads_WhenClosedAndDestroyed_ThenEachRunsOnceWithCancelled)
{
  make ();
  seen read1;
  seen read2;
  link_->async_read_frame (message_recorder{ &read1 });
  link_->async_read_frame (message_recorder{ &read2 });
  settle ();
  link_->close ();
  settle ();
  EXPECT_EQ (read1.calls, 1);
  EXPECT_EQ (read2.calls, 1);
  EXPECT_TRUE (read1.error == transport_errc::cancelled);
  seen third;
  link_->async_read_frame (message_recorder{ &third });
  link_.reset ();
  settle ();
  EXPECT_EQ (third.calls, 1);
  EXPECT_TRUE (third.error == transport_errc::cancelled
               || third.error == transport_errc::not_open);
}

TEST_F (
    LumexFramedDatagramPolledTest,
    GivenTheHandlerOfAReadDestroysTheAdapter_WhenItRuns_ThenNothingIsTouched)
{
  make ();
  seen first;
  link_->async_read_frame (
      [this, &first] (std::error_code error, message_t message)
        {
          ++first.calls;
          first.error = error;
          first.frame = std::move (message.payload);
          link_.reset ();
        });
  settle ();
  mock_->control.deliver (message_of (slip_wire (1, 7)));
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_EQ (first.frame, bytes_t (1, 7));
  EXPECT_EQ (link_.get (), nullptr);
  settle ();
}

TEST_F (LumexFramedDatagramPolledTest,
        GivenUseFuture_WhenWriteAndRead_ThenTheFrameComesBack)
{
  tr::stream::io_runner runner (2);
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (runner.get_executor (), "future");
  tr::framed::framed_datagram<slip_case::codec_t> link (
      std::move (mock.transport), slip_case::make (slip_case::limit ()));
  link.transport ().async_open (asio::use_future).get ();
  link.async_write_frame (5, tr::const_bytes_t (payload_), asio::use_future)
      .get ();
  std::vector<message_t> const sent = mock.control.sent ();
  ASSERT_EQ (sent.size (), 1u);
  mock.control.deliver (sent[0]);
  message_t const frame = link.async_read_frame (asio::use_future).get ();
  EXPECT_EQ (frame.payload, payload_);
  EXPECT_EQ (frame.report_id, 5u);
}

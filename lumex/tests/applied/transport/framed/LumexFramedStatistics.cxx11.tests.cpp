// framed_counters and framed_statistics_t (lumex/applied/transport/framed):
// what each call counts, errors per framing_errc and the others, the lookup
// for a value outside the enum, snapshot and reset, and counting from many
// threads.

#include <cstdint>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/core/framing/codec/LumexFramingErrc.hpp"

namespace
{
namespace tf = lumex::applied::transport::framed;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::codec::make_error_code;
}

TEST (LumexFramedStatisticsTest,
      GivenNewCounters_WhenSnapshotted_ThenEverythingIsZero)
{
  tf::framed_counters counters;
  tf::framed_statistics_t const stats = counters.snapshot ();
  EXPECT_EQ (stats.frames_in, 0u);
  EXPECT_EQ (stats.frames_out, 0u);
  EXPECT_EQ (stats.bytes_in, 0u);
  EXPECT_EQ (stats.bytes_out, 0u);
  EXPECT_EQ (stats.dropped_frames, 0u);
  EXPECT_EQ (stats.other_errors, 0u);
  EXPECT_EQ (stats.total_errors (), 0u);
}

TEST (LumexFramedStatisticsTest,
      GivenEachCall_WhenCounted_ThenItMovesItsOwnCounter)
{
  tf::framed_counters counters;
  counters.add_frame_in ();
  counters.add_frame_in ();
  counters.add_frame_out (10);
  counters.add_bytes_in (7);
  counters.add_dropped_frame ();
  counters.convert_cancel_to_timeout ();
  counters.convert_cancel_to_timeout ();
  tf::framed_statistics_t const stats = counters.snapshot ();
  EXPECT_EQ (stats.timeouts, 2u);
  EXPECT_EQ (stats.frames_in, 2u);
  EXPECT_EQ (stats.frames_out, 1u);
  EXPECT_EQ (stats.bytes_out, 10u);
  EXPECT_EQ (stats.bytes_in, 7u);
  EXPECT_EQ (stats.dropped_frames, 1u);
  EXPECT_EQ (stats.total_errors (), 0u);
}

TEST (LumexFramedStatisticsTest,
      GivenEveryFramingErrc_WhenCounted_ThenEachHasItsSlot)
{
  tf::framed_counters counters;
  framing_errc const all[]
      = { framing_errc::frame_too_large, framing_errc::bad_checksum,
          framing_errc::bad_escape,      framing_errc::bad_header,
          framing_errc::truncated,       framing_errc::payload_too_large };
  int times = 1;
  for (framing_errc value : all)
    {
      for (int i = 0; i < times; ++i)
        counters.add_error (make_error_code (value));
      ++times;
    }
  tf::framed_statistics_t const stats = counters.snapshot ();
  times = 1;
  for (framing_errc value : all)
    EXPECT_EQ (stats.errors_of (value), static_cast<std::uint64_t> (times++));
  EXPECT_EQ (stats.total_errors (), 21u);
  EXPECT_EQ (stats.other_errors, 0u);
}

TEST (LumexFramedStatisticsTest,
      GivenAValueOutsideTheEnum_WhenLookedUp_ThenZero)
{
  tf::framed_statistics_t stats;
  stats.errors[0] = 5;
  EXPECT_EQ (stats.errors_of (static_cast<framing_errc> (0)), 0u);
  EXPECT_EQ (stats.errors_of (static_cast<framing_errc> (7)), 0u);
  EXPECT_EQ (stats.errors_of (static_cast<framing_errc> (-1)), 0u);
  EXPECT_EQ (stats.errors_of (framing_errc::frame_too_large), 5u);
}

TEST (LumexFramedStatisticsTest,
      GivenCounts_WhenReset_ThenEverythingIsZeroAgain)
{
  tf::framed_counters counters;
  counters.add_frame_in ();
  counters.add_frame_out (3);
  counters.add_bytes_in (4);
  counters.add_dropped_frame ();
  counters.add_error (make_error_code (framing_errc::bad_header));
  counters.add_error (std::make_error_code (std::errc::io_error));
  counters.convert_cancel_to_timeout ();
  counters.reset ();
  tf::framed_statistics_t const stats = counters.snapshot ();
  EXPECT_EQ (stats.frames_in + stats.frames_out + stats.bytes_in
                 + stats.bytes_out + stats.dropped_frames,
             0u);
  EXPECT_EQ (stats.total_errors (), 0u);
  EXPECT_EQ (stats.timeouts, 0u);
}

TEST (LumexFramedStatisticsTest,
      GivenManyThreads_WhenTheyCount_ThenNothingIsLost)
{
  tf::framed_counters counters;
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t)
    threads.emplace_back (
        [&counters]
          {
            for (int i = 0; i < 5000; ++i)
              {
                counters.add_frame_in ();
                counters.add_bytes_in (2);
                counters.add_error (make_error_code (framing_errc::truncated));
              }
          });
  for (std::thread &thread : threads)
    thread.join ();
  tf::framed_statistics_t const stats = counters.snapshot ();
  EXPECT_EQ (stats.frames_in, 20000u);
  EXPECT_EQ (stats.bytes_in, 40000u);
  EXPECT_EQ (stats.errors_of (framing_errc::truncated), 20000u);
}

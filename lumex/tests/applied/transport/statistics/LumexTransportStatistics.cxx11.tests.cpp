// statistics_counters (lumex/applied/transport/statistics) on their own: what
// each add_* function moves, the classification of an error code (also of
// another binary's category), the timeout conversion, snapshot and reset, and
// snapshots read from other threads while one thread counts.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::error::transport_errc;
using tr::statistics::statistics_counters;
using tr::statistics::statistics_t;

typedef std::chrono::system_clock clock_type;

// A category of another binary: the same name, another object.
class foreign_category final : public std::error_category
{
public:
  char const *
  name () const noexcept override
  {
    return tr::error::KTRANSPORT_CATEGORY_NAME;
  }
  std::string
  message (int) const override
  {
    return "foreign";
  }
};

std::error_category const &
foreign ()
{
  static foreign_category const category;
  return category;
}

void
expect_all_zero (statistics_t const &s)
{
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.reads, 0u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.messages_received, 0u);
  EXPECT_EQ (s.messages_sent, 0u);
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_EQ (s.other_errors, 0u);
  EXPECT_EQ (s.timeouts, 0u);
  EXPECT_EQ (s.cancellations, 0u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
}
} // namespace

TEST (LumexTransportStatisticsTest, GivenNewCounters_WhenSnapshot_ThenAllZero)
{
  statistics_counters counters;
  expect_all_zero (counters.snapshot ());
  expect_all_zero (statistics_t ());
}

TEST (LumexTransportStatisticsTest,
      GivenSuccessfulOperations_WhenCounted_ThenTheirCountersMove)
{
  statistics_counters counters;
  clock_type::time_point const before = clock_type::now ();
  counters.add_open (std::error_code ());
  counters.add_read (std::error_code (), 5);
  counters.add_read (std::error_code (), 0);
  counters.add_write (std::error_code (), 7);
  counters.add_receive (std::error_code (), 11);
  counters.add_send (std::error_code (), 13);
  clock_type::time_point const after = clock_type::now ();
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.opens, 1u);
  EXPECT_EQ (s.reads, 2u);
  EXPECT_EQ (s.writes, 1u);
  EXPECT_EQ (s.messages_received, 1u);
  EXPECT_EQ (s.messages_sent, 1u);
  EXPECT_EQ (s.bytes_read, 16u);
  EXPECT_EQ (s.bytes_written, 20u);
  EXPECT_EQ (s.total_errors (), 0u);
  EXPECT_FALSE (s.last_activity < before);
  EXPECT_FALSE (after < s.last_activity);
}

TEST (LumexTransportStatisticsTest,
      GivenAFailedWriteWithBytes_WhenCounted_ThenBytesCountButNotTheWrite)
{
  statistics_counters counters;
  counters.add_write (make_error_code (transport_errc::disconnected), 3);
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.bytes_written, 3u);
  EXPECT_EQ (s.writes, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::disconnected), 1u);
}

TEST (LumexTransportStatisticsTest,
      GivenFailedOperations_WhenCounted_ThenNoSuccessCounterMoves)
{
  statistics_counters counters;
  std::error_code const error = make_error_code (transport_errc::closed);
  counters.add_open (error);
  counters.add_read (error, 0);
  counters.add_receive (error, 0);
  counters.add_send (error, 99);
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.reads, 0u);
  EXPECT_EQ (s.messages_received, 0u);
  EXPECT_EQ (s.messages_sent, 0u);
  EXPECT_EQ (s.bytes_written, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::closed), 4u);
  EXPECT_EQ (s.total_errors (), 4u);
  EXPECT_TRUE (s.last_activity == clock_type::time_point ());
}

TEST (LumexTransportStatisticsTest,
      GivenEveryTransportErrc_WhenCounted_ThenItsOwnSlotMoves)
{
  for (int value = 1; value <= 10; ++value)
    {
      statistics_counters counters;
      counters.add_error (
          make_error_code (static_cast<transport_errc> (value)));
      statistics_t const s = counters.snapshot ();
      EXPECT_EQ (s.total_errors (), 1u) << value;
      EXPECT_EQ (s.other_errors, 0u) << value;
      EXPECT_EQ (s.errors_of (static_cast<transport_errc> (value)), 1u)
          << value;
      EXPECT_EQ (s.timeouts, value == 1 ? 1u : 0u) << value;
      EXPECT_EQ (s.cancellations, value == 2 ? 1u : 0u) << value;
    }
}

TEST (LumexTransportStatisticsTest,
      GivenASystemError_WhenCounted_ThenItIsAnOtherError)
{
  statistics_counters counters;
  counters.add_error (std::make_error_code (std::errc::connection_reset));
  // The same value as transport_errc::timeout, but a system category.
  counters.add_error (std::error_code (1, std::system_category ()));
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.other_errors, 2u);
  EXPECT_EQ (s.timeouts, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::timeout), 0u);
  EXPECT_EQ (s.total_errors (), 2u);
}

TEST (LumexTransportStatisticsTest,
      GivenACodeOfAnotherBinarysTransportCategory_WhenCounted_ThenItMatches)
{
  statistics_counters counters;
  counters.add_error (std::error_code (
      static_cast<int> (transport_errc::cancelled), foreign ()));
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.errors_of (transport_errc::cancelled), 1u);
  EXPECT_EQ (s.cancellations, 1u);
  EXPECT_EQ (s.other_errors, 0u);
}

TEST (LumexTransportStatisticsTest,
      GivenACodeOutsideTheEnum_WhenCounted_ThenItIsAnOtherError)
{
  statistics_counters counters;
  counters.add_error (std::error_code (99, tr::error::transport_category ()));
  counters.add_error (std::error_code (-1, tr::error::transport_category ()));
  EXPECT_EQ (counters.snapshot ().other_errors, 2u);
}

TEST (LumexTransportStatisticsTest,
      GivenACancellation_WhenConvertedToATimeout_ThenItCountsOnlyAsTimeout)
{
  statistics_counters counters;
  counters.add_read (make_error_code (transport_errc::cancelled), 0);
  counters.convert_cancel_to_timeout ();
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.cancellations, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::cancelled), 0u);
  EXPECT_EQ (s.timeouts, 1u);
  EXPECT_EQ (s.errors_of (transport_errc::timeout), 1u);
  EXPECT_EQ (s.total_errors (), 1u);
}

TEST (LumexTransportStatisticsTest,
      GivenAResetBetween_WhenAConversionRuns_ThenNothingWrapsAround)
{
  statistics_counters counters;
  counters.convert_cancel_to_timeout ();
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.cancellations, 0u);
  EXPECT_EQ (s.errors_of (transport_errc::cancelled), 0u);
  EXPECT_EQ (s.timeouts, 1u);
}

TEST (LumexTransportStatisticsTest,
      GivenCountedTraffic_WhenReset_ThenEverythingIsZeroAndCountingContinues)
{
  statistics_counters counters;
  counters.add_open (std::error_code ());
  counters.add_read (std::error_code (), 4);
  counters.add_write (std::error_code (), 4);
  counters.add_receive (std::error_code (), 4);
  counters.add_send (std::error_code (), 4);
  counters.add_error (make_error_code (transport_errc::timeout));
  counters.add_error (make_error_code (transport_errc::cancelled));
  counters.add_error (std::make_error_code (std::errc::io_error));
  counters.reset ();
  expect_all_zero (counters.snapshot ());
  counters.add_read (std::error_code (), 2);
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.reads, 1u);
  EXPECT_EQ (s.bytes_read, 2u);
}

TEST (LumexTransportStatisticsTest,
      GivenOneCountingThread_WhenOthersTakeSnapshots_ThenValuesNeverGoBack)
{
  statistics_counters counters;
  std::size_t const rounds = 20000;
  std::atomic<bool> stop (false);
  std::atomic<int> violations (0);
  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i)
    readers.emplace_back (
        [&counters, &stop, &violations]
          {
            std::uint64_t last_reads = 0;
            std::uint64_t last_bytes = 0;
            while (!stop.load ())
              {
                statistics_t const s = counters.snapshot ();
                if (s.reads < last_reads || s.bytes_read < last_bytes)
                  violations.fetch_add (1);
                last_reads = s.reads;
                last_bytes = s.bytes_read;
              }
          });
  for (std::size_t round = 0; round < rounds; ++round)
    counters.add_read (std::error_code (), 2);
  stop.store (true);
  for (std::thread &reader : readers)
    reader.join ();
  EXPECT_EQ (violations.load (), 0);
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.reads, rounds);
  EXPECT_EQ (s.bytes_read, rounds * 2);
}

TEST (LumexTransportStatisticsTest,
      GivenManyCountingThreads_WhenAllFinish_ThenNoIncrementIsLost)
{
  statistics_counters counters;
  std::size_t const threads = 4;
  std::size_t const rounds = 10000;
  std::vector<std::thread> workers;
  for (std::size_t i = 0; i < threads; ++i)
    workers.emplace_back (
        [&counters, rounds]
          {
            for (std::size_t round = 0; round < rounds; ++round)
              {
                counters.add_write (std::error_code (), 1);
                counters.add_error (
                    make_error_code (transport_errc::disconnected));
              }
          });
  for (std::thread &worker : workers)
    worker.join ();
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.writes, threads * rounds);
  EXPECT_EQ (s.bytes_written, threads * rounds);
  EXPECT_EQ (s.errors_of (transport_errc::disconnected), threads * rounds);
}

TEST (LumexTransportStatisticsTest,
      GivenAWrappedHandler_WhenCalled_ThenItCountsBeforeTheHandlerRuns)
{
  std::shared_ptr<statistics_counters> const counters
      = std::make_shared<statistics_counters> ();
  std::uint64_t seen_reads = 99;
  auto wrapped = tr::detail::count_with (
      counters, tr::detail::read_recorder (),
      [&counters, &seen_reads] (std::error_code, std::size_t)
        { seen_reads = counters->snapshot ().reads; });
  wrapped (std::error_code (), 3);
  EXPECT_EQ (seen_reads, 1u);
  EXPECT_EQ (counters->snapshot ().bytes_read, 3u);
}

TEST (LumexTransportStatisticsTest,
      GivenTheLastTransportErrc_WhenCompared_ThenTheSlotsMatchTheEnum)
{
  EXPECT_EQ (static_cast<std::size_t> (tr::error::KTRANSPORT_ERRC_LAST),
             tr::statistics::KERROR_CODE_COUNT);
  EXPECT_EQ (statistics_t ().errors.size (),
             static_cast<std::size_t> (tr::error::KTRANSPORT_ERRC_LAST));
}

TEST (
    LumexTransportStatisticsTest,
    GivenAValueOutsideTheEnum_WhenErrorsOfIsAsked_ThenZeroAndNoReadPastTheArray)
{
  statistics_counters counters;
  counters.add_error (make_error_code (tr::error::KTRANSPORT_ERRC_LAST));
  statistics_t const s = counters.snapshot ();
  EXPECT_EQ (s.errors_of (tr::error::KTRANSPORT_ERRC_LAST), 1u);
  EXPECT_EQ (s.errors_of (static_cast<transport_errc> (0)), 0u);
  EXPECT_EQ (s.errors_of (static_cast<transport_errc> (-1)), 0u);
  EXPECT_EQ (s.errors_of (static_cast<transport_errc> (
                 static_cast<int> (tr::error::KTRANSPORT_ERRC_LAST) + 1)),
             0u);
  EXPECT_EQ (s.errors_of (static_cast<transport_errc> (1000)), 0u);
}

TEST (LumexTransportStatisticsTest,
      GivenLastActivity_WhenLaterActivityIsCounted_ThenItDoesNotMoveBack)
{
  statistics_counters counters;
  counters.add_open (std::error_code ());
  clock_type::time_point const first = counters.snapshot ().last_activity;
  counters.add_read (std::error_code (), 1);
  EXPECT_FALSE (counters.snapshot ().last_activity < first);
}

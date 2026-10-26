// stream_session over a factory of mock streams: the cycle (attempts,
// delays, max_attempts, a factory that throws, the open timeout), the state
// notifications in order, close and cancel during a cycle, the I/O through
// the session, the device presence and the statistics.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
typedef ss::channel_state state;

class LumexStreamSessionTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  std::shared_ptr<stream_factory_state> factory_
      = std::make_shared<stream_factory_state> ();
  std::shared_ptr<state_recorder> recorder_
      = std::make_shared<state_recorder> ();

  std::unique_ptr<ss::stream_session>
  make_session (ss::session_options_t const &options)
  {
    return std::unique_ptr<ss::stream_session> (new ss::stream_session (
        runner_.get_executor (), make_stream_factory (factory_), options));
  }

  std::unique_ptr<ss::stream_session>
  make_session ()
  {
    return make_session (recorded_options (recorder_));
  }
};

bool
same_states (std::vector<state> const &actual,
             std::vector<state> const &expected)
{
  return actual == expected;
}
} // namespace

TEST_F (LumexStreamSessionTest,
        GivenANewSession_WhenNothingIsCalled_ThenItIsClosedAndMakesNothing)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (session->state (), state::closed);
  EXPECT_FALSE (session->is_open ());
  EXPECT_EQ (factory_->call_count (), 0u);
  EXPECT_EQ (session->name (), "test");
}

TEST_F (LumexStreamSessionTest,
        GivenAFactory_WhenOpened_ThenTheStateGoesConnectingThenOpen)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  tr::status_result_t const opened = session->open (ms (2000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  EXPECT_EQ (session->state (), state::open);
  EXPECT_TRUE (session->is_open ());
  ASSERT_TRUE (eventually ([this] { return recorder_->count () == 2; }));
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  EXPECT_EQ (changes[0].previous, state::closed);
  EXPECT_EQ (changes[0].current, state::connecting);
  EXPECT_EQ (changes[1].previous, state::connecting);
  EXPECT_EQ (changes[1].current, state::open);
  EXPECT_EQ (changes[1].attempt, 1u);
  EXPECT_EQ (factory_->call_count (), 1u);
  ss::session_statistics_t const counts = session->session_statistics ();
  EXPECT_EQ (counts.attempts, 1u);
  EXPECT_EQ (counts.failed_attempts, 0u);
  EXPECT_EQ (counts.reconnects, 0u);
  EXPECT_EQ (session->statistics ().opens, 1u);
}

TEST_F (LumexStreamSessionTest,
        GivenAnOpenSession_WhenStartIsCalledAgain_ThenNoSecondCycleBegins)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  session->start ();
  session->start ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (factory_->call_count (), 1u);
  EXPECT_EQ (recorder_->count (), 2u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAFactoryThatFailsThreeTimes_WhenOpened_ThenItRetriesAfterTheDelaysOfThePolicy)
{
  factory_->open_failures = 3;
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (30), ms (100)));
  tr::status_result_t const opened = session->open (ms (5000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  std::vector<clock_type::time_point> const calls = factory_->call_times ();
  ASSERT_EQ (calls.size (), 4u);
  // 30, 60, then 120 capped to 100 ms (no jitter); a timer never ends early.
  long const expected[] = { 30, 60, 100 };
  for (std::size_t i = 0; i < 3; ++i)
    {
      long const gap = static_cast<long> (
          std::chrono::duration_cast<ms> (calls[i + 1] - calls[i]).count ());
      EXPECT_GE (gap, expected[i] - 1) << "gap " << i;
      EXPECT_LT (gap, expected[i] + 500) << "gap " << i;
    }
  ss::session_statistics_t const counts = session->session_statistics ();
  EXPECT_EQ (counts.attempts, 4u);
  EXPECT_EQ (counts.failed_attempts, 3u);
  EXPECT_TRUE (session->last_error ()
               == std::make_error_code (std::errc::no_such_device));
  // Failed attempts do not change the state: connecting, then open.
  EXPECT_TRUE (
      same_states (recorder_->states (), { state::connecting, state::open }));
  EXPECT_EQ (recorder_->all ()[1].attempt, 4u);
}

TEST_F (LumexStreamSessionTest,
        GivenAnInjectedRandomSource_WhenAnAttemptFails_ThenTheJitterUsesIt)
{
  factory_->open_failures = 1;
  ss::session_options_t options
      = recorded_options (recorder_, ms (100), ms (1000));
  options.policy.jitter = 0.5;
  options.random = [] () { return 0.0; };
  std::unique_ptr<ss::stream_session> session = make_session (options);
  ASSERT_TRUE (session->open (ms (3000)).has_value ());
  std::vector<clock_type::time_point> const calls = factory_->call_times ();
  ASSERT_EQ (calls.size (), 2u);
  long const gap = static_cast<long> (
      std::chrono::duration_cast<ms> (calls[1] - calls[0]).count ());
  // 100 ms * (1 - 0.5): 50 ms, well below the 100 ms without jitter.
  EXPECT_GE (gap, 49);
  EXPECT_LT (gap, 95);
}

TEST_F (
    LumexStreamSessionTest,
    GivenMaxAttempts_WhenEveryAttemptFails_ThenTheSessionFaultsWithTheLastError)
{
  factory_->open_failures = 1000;
  factory_->open_error = std::make_error_code (std::errc::connection_refused);
  ss::session_options_t options = recorded_options (recorder_);
  options.policy.max_attempts = 3;
  std::unique_ptr<ss::stream_session> session = make_session (options);
  tr::status_result_t const opened = session->open (ms (3000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error ()
               == std::make_error_code (std::errc::connection_refused));
  EXPECT_EQ (session->state (), state::faulted);
  std::this_thread::sleep_for (ms (100));
  EXPECT_EQ (factory_->call_count (), 3u);
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  ASSERT_EQ (changes.size (), 2u);
  EXPECT_EQ (changes[1].current, state::faulted);
  EXPECT_TRUE (changes[1].error
               == std::make_error_code (std::errc::connection_refused));
  EXPECT_EQ (changes[1].attempt, 3u);

  // start () begins a new cycle from faulted.
  {
    std::lock_guard<std::mutex> const lock (factory_->mutex);
    factory_->open_failures = 0;
  }
  ASSERT_TRUE (session->open (ms (3000)).has_value ());
  EXPECT_EQ (factory_->call_count (), 4u);
}

TEST_F (LumexStreamSessionTest,
        GivenAFactoryThatThrows_WhenOpened_ThenEachThrowIsAFailedAttempt)
{
  factory_->throws = 2;
  std::unique_ptr<ss::stream_session> session = make_session ();
  tr::status_result_t const opened = session->open (ms (3000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  EXPECT_EQ (factory_->call_count (), 3u);
  ss::session_statistics_t const counts = session->session_statistics ();
  EXPECT_EQ (counts.attempts, 3u);
  EXPECT_EQ (counts.failed_attempts, 2u);
  EXPECT_TRUE (session->last_error () == transport_errc::invalid_endpoint);
}

TEST_F (LumexStreamSessionTest,
        GivenAFactoryThatThrowsASystemError_WhenOpened_ThenItsCodeIsTheError)
{
  ss::session_options_t options = recorded_options (recorder_);
  options.policy.max_attempts = 1;
  ss::stream_session session (
      runner_.get_executor (),
      [] (tr::executor_type const &) -> tr::stream::byte_stream
        {
          throw std::system_error (
              std::make_error_code (std::errc::permission_denied), "denied");
        },
      options);
  tr::status_result_t const opened = session.open (ms (3000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error ()
               == std::make_error_code (std::errc::permission_denied));
}

TEST_F (
    LumexStreamSessionTest,
    GivenAnOpenTimeout_WhenTheOpenNeverCompletes_ThenTheAttemptFailsWithTimeoutAndIsRetried)
{
  factory_->hold_call = 1;
  ss::session_options_t options = recorded_options (recorder_);
  options.open_timeout = ms (40);
  std::unique_ptr<ss::stream_session> session = make_session (options);
  clock_type::time_point const before = clock_type::now ();
  tr::status_result_t const opened = session->open (ms (3000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  EXPECT_GE (clock_type::now () - before, ms (39));
  EXPECT_EQ (factory_->call_count (), 2u);
  EXPECT_EQ (session->session_statistics ().failed_attempts, 1u);
  EXPECT_TRUE (session->last_error () == transport_errc::timeout);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAnOpenSession_WhenTheStreamDisconnects_ThenTheStatesFollowInOrderAndTheReconnectIsCounted)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  outcome<int> state_seen;
  session->async_read_some (
      tr::mutable_bytes_t (buffer),
      [&read, &state_seen, &session] (std::error_code error, std::size_t bytes)
        {
          // The session saw the loss before this handler runs.
          state_seen.set (std::error_code (),
                          static_cast<int> (session->state ()));
          read.set (error, bytes);
        });
  factory_->last ().disconnect ();
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::disconnected);
  EXPECT_NE (state_seen.value, static_cast<int> (state::open));
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        }));
  EXPECT_EQ (session->state (), state::open);
  EXPECT_EQ (factory_->call_count (), 2u);
  session->close ();
  ASSERT_TRUE (eventually ([this] { return recorder_->count () == 5; }));
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  ASSERT_EQ (changes.size (), 5u);
  state const expected[][2] = { { state::closed, state::connecting },
                                { state::connecting, state::open },
                                { state::open, state::reconnecting },
                                { state::reconnecting, state::open },
                                { state::open, state::closed } };
  for (std::size_t i = 0; i < 5; ++i)
    {
      EXPECT_EQ (changes[i].previous, expected[i][0]) << i;
      EXPECT_EQ (changes[i].current, expected[i][1]) << i;
    }
  EXPECT_TRUE (changes[2].error == transport_errc::disconnected);
  EXPECT_TRUE (session->last_error () == transport_errc::disconnected);
}

TEST_F (LumexStreamSessionTest,
        GivenAnOpenSession_WhenAReadEndsWithATimeout_ThenTheLinkStaysOpen)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  factory_->last ().fail_next_read (make_error_code (transport_errc::timeout));
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code error, std::size_t bytes)
                              { read.set (error, bytes); });
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::timeout);
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (session->state (), state::open);
  EXPECT_EQ (factory_->call_count (), 1u);
}

TEST_F (LumexStreamSessionTest,
        GivenAClosedSession_WhenIoStarts_ThenItEndsWithNotOpenAndNothingStarts)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code error, std::size_t bytes)
                              { read.set (error, bytes); });
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::not_open);
  outcome<std::size_t> written;
  session->async_write_all (
      tr::const_bytes_t (buffer),
      [&written] (std::error_code error, std::size_t bytes)
        { written.set (error, bytes); });
  ASSERT_TRUE (written.wait ());
  EXPECT_TRUE (written.error == transport_errc::not_open);
  outcome<std::size_t> some;
  session->async_write_some (tr::const_bytes_t (buffer),
                             [&some] (std::error_code error, std::size_t bytes)
                               { some.set (error, bytes); });
  ASSERT_TRUE (some.wait ());
  EXPECT_TRUE (some.error == transport_errc::not_open);
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (session->state (), state::closed);
  EXPECT_EQ (factory_->call_count (), 0u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAnOpenSession_WhenItReadsAndWrites_ThenTheCurrentStreamCarriesAndCountsThem)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> const sent = { 1, 2, 3 };
  std::size_t const written
      = session->async_write_all (tr::const_bytes_t (sent), asio::use_future)
            .get ();
  EXPECT_EQ (written, 3u);
  std::size_t const chunk
      = session->async_write_some (tr::const_bytes_t (sent), asio::use_future)
            .get ();
  EXPECT_EQ (chunk, 3u);
  std::vector<std::uint8_t> expected = sent;
  expected.insert (expected.end (), sent.begin (), sent.end ());
  EXPECT_EQ (factory_->last ().written (), expected);

  factory_->last ().feed ({ 9, 8 });
  std::vector<std::uint8_t> buffer (4);
  std::size_t const got
      = session
            ->async_read_some (tr::mutable_bytes_t (buffer), asio::use_future)
            .get ();
  EXPECT_EQ (got, 2u);
  EXPECT_EQ (buffer[0], 9);
  EXPECT_EQ (buffer[1], 8);

  tr::statistics::statistics_t const stats = session->statistics ();
  EXPECT_EQ (stats.opens, 1u);
  EXPECT_EQ (stats.writes, 2u);
  EXPECT_EQ (stats.bytes_written, 6u);
  EXPECT_EQ (stats.reads, 1u);
  EXPECT_EQ (stats.bytes_read, 2u);

  session->reset_statistics ();
  EXPECT_EQ (session->statistics ().bytes_written, 0u);
  EXPECT_EQ (session->session_statistics ().attempts, 0u);
}

TEST_F (LumexStreamSessionTest,
        GivenAReconnect_WhenStatisticsAreRead_ThenTheyAreThoseOfTheNewStream)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> const sent = { 1, 2, 3 };
  session->async_write_all (tr::const_bytes_t (sent), asio::use_future).get ();
  EXPECT_EQ (session->statistics ().bytes_written, 3u);
  std::vector<std::uint8_t> buffer (4);
  std::future<std::size_t> lost = session->async_read_some (
      tr::mutable_bytes_t (buffer), asio::use_future);
  factory_->last ().disconnect ();
  EXPECT_THROW (lost.get (), std::system_error);
  ASSERT_TRUE (eventually (
      [&session] { return session->session_statistics ().reconnects == 1; }));
  EXPECT_EQ (session->statistics ().bytes_written, 0u);
  EXPECT_EQ (session->statistics ().opens, 1u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenALongRetryWait_WhenClosedFromAnotherThread_ThenReconnectingEndsAtOnce)
{
  factory_->open_failures = 1000;
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (10000), ms (10000)));
  session->start ();
  ASSERT_TRUE (eventually (
      [&session]
        { return session->session_statistics ().failed_attempts == 1; }));
  outcome<int> wait;
  session->async_wait_state_change (
      state::connecting, [&wait] (std::error_code error, state now)
        { wait.set (error, static_cast<int> (now)); });
  outcome<int> open_wait;
  session->async_open ([&open_wait] (std::error_code error)
                         { open_wait.set (error, 0); });

  clock_type::time_point const before = clock_type::now ();
  std::thread closer ([&session] { session->close (); });
  closer.join ();
  ASSERT_TRUE (wait.wait (ms (200)));
  EXPECT_TRUE (wait.error == transport_errc::cancelled);
  ASSERT_TRUE (open_wait.wait (ms (200)));
  EXPECT_TRUE (open_wait.error == transport_errc::cancelled);
  ASSERT_TRUE (eventually (
      [&session] { return session->state () == state::closed; }, ms (200)));
  EXPECT_LT (clock_type::now () - before, ms (200));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (factory_->call_count (), 1u);
  EXPECT_TRUE (same_states (recorder_->states (),
                            { state::connecting, state::closed }));
}

TEST_F (LumexStreamSessionTest,
        GivenARetryWait_WhenCloseAndStartFollow_ThenTheSessionOpensAgain)
{
  factory_->open_failures = 1;
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (10000), ms (10000)));
  session->start ();
  ASSERT_TRUE (eventually (
      [&session]
        { return session->session_statistics ().failed_attempts == 1; }));
  session->close ();
  tr::status_result_t const opened = session->open (ms (2000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  EXPECT_EQ (factory_->call_count (), 2u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAStartFollowedByAClose_WhenBothRun_ThenNoAttemptStartsAfterTheClose)
{
  // A context that runs only after both calls: the start step is queued
  // when the close bumps the stop epoch, and finds itself void.
  asio::io_context context;
  {
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory_),
                                recorded_options (recorder_));
    session.start ();
    session.close ();
    context.run_for (std::chrono::milliseconds (200));
    EXPECT_EQ (session.state (), state::closed);
  }
  context.run_for (std::chrono::milliseconds (50));
  EXPECT_EQ (factory_->call_count (), 0u);
  // The void start did not even begin a cycle.
  EXPECT_EQ (recorder_->count (), 0u);
}

TEST_F (LumexStreamSessionTest,
        GivenAPendingOpen_WhenCancelled_ThenTheCycleEndsClosedWithCancelled)
{
  factory_->hold_call = 1;
  ss::session_options_t options = recorded_options (recorder_);
  options.open_timeout = ms (0);
  std::unique_ptr<ss::stream_session> session = make_session (options);
  outcome<int> open_wait;
  session->async_open ([&open_wait] (std::error_code error)
                         { open_wait.set (error, 0); });
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () == 1; }));
  std::this_thread::sleep_for (ms (20));
  EXPECT_FALSE (open_wait.is_done ());
  session->cancel ();
  ASSERT_TRUE (open_wait.wait (ms (500)));
  EXPECT_TRUE (open_wait.error == transport_errc::cancelled);
  ASSERT_TRUE (
      eventually ([&session] { return session->state () == state::closed; }));
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  ASSERT_EQ (changes.size (), 2u);
  EXPECT_EQ (changes[1].current, state::closed);
  EXPECT_TRUE (changes[1].error == transport_errc::cancelled);
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (factory_->call_count (), 1u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAnOpenSession_WhenCancelled_ThenPendingIoEndsAndItStaysOpenAndStillReconnects)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code error, std::size_t bytes)
                              { read.set (error, bytes); });
  outcome<int> wait;
  session->async_wait_state_change (
      state::open, [&wait] (std::error_code error, state now)
        { wait.set (error, static_cast<int> (now)); });
  std::this_thread::sleep_for (ms (20));
  session->cancel ();
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  ASSERT_TRUE (wait.wait ());
  EXPECT_TRUE (wait.error == transport_errc::cancelled);
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (session->state (), state::open);

  // Still watching its link after the cancel.
  outcome<std::size_t> lost;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&lost] (std::error_code error, std::size_t bytes)
                              { lost.set (error, bytes); });
  factory_->last ().disconnect ();
  ASSERT_TRUE (lost.wait ());
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        }));
  EXPECT_EQ (session->state (), state::open);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAStateSeen_WhenWaitingForAChange_ThenOnlyAnotherStateCompletesTheWait)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  // The state differs from the one given: at once.
  std::future<state> at_once
      = session->async_wait_state_change (state::open, asio::use_future);
  EXPECT_EQ (at_once.get (), state::closed);

  outcome<int> next;
  session->async_wait_state_change (
      state::closed, [&next] (std::error_code error, state now)
        { next.set (error, static_cast<int> (now)); });
  std::this_thread::sleep_for (ms (20));
  EXPECT_FALSE (next.is_done ());
  session->start ();
  ASSERT_TRUE (next.wait ());
  EXPECT_FALSE (next.error);
  EXPECT_EQ (next.value, static_cast<int> (state::connecting));
}

TEST_F (
    LumexStreamSessionTest,
    GivenAnOpenSession_WhenTheDeviceGoesAndComesBack_ThenItWaitsForItThenReconnects)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  ss::presence_sink_t const sink = session->presence_sink ();
  sink (false);
  ASSERT_TRUE (eventually (
      [&session] { return session->state () == state::reconnecting; }));
  std::this_thread::sleep_for (ms (100));
  // No attempt while the device is away, however short the policy.
  EXPECT_EQ (factory_->call_count (), 1u);
  EXPECT_EQ (session->state (), state::reconnecting);
  EXPECT_TRUE (session->last_error () == transport_errc::disconnected);
  sink (true);
  ASSERT_TRUE (
      eventually ([&session] { return session->state () == state::open; }));
  EXPECT_EQ (factory_->call_count (), 2u);
  EXPECT_EQ (session->session_statistics ().reconnects, 1u);
}

TEST_F (LumexStreamSessionTest,
        GivenAnAbsentDevice_WhenStarted_ThenTheFirstAttemptWaitsForTheArrival)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  session->set_device_present (false);
  session->start ();
  ASSERT_TRUE (eventually (
      [&session] { return session->state () == state::connecting; }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (factory_->call_count (), 0u);
  session->set_device_present (true);
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  EXPECT_EQ (factory_->call_count (), 1u);
}

TEST_F (LumexStreamSessionTest,
        GivenALongRetryWait_WhenTheDeviceArrives_ThenTheNextAttemptRunsAtOnce)
{
  factory_->open_failures = 1;
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (10000), ms (10000)));
  session->start ();
  ASSERT_TRUE (eventually (
      [&session]
        { return session->session_statistics ().failed_attempts == 1; }));
  clock_type::time_point const before = clock_type::now ();
  session->set_device_present (true);
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  EXPECT_LT (clock_type::now () - before, ms (1000));
}

TEST_F (LumexStreamSessionTest,
        GivenARetryWait_WhenTheDeviceGoes_ThenNoAttemptRunsUntilItComesBack)
{
  factory_->open_failures = 1;
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (30), ms (30)));
  session->start ();
  ASSERT_TRUE (eventually (
      [&session]
        { return session->session_statistics ().failed_attempts == 1; }));
  session->set_device_present (false);
  std::this_thread::sleep_for (ms (120));
  EXPECT_EQ (factory_->call_count (), 1u);
  session->set_device_present (true);
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  EXPECT_EQ (factory_->call_count (), 2u);
}

TEST_F (LumexStreamSessionTest,
        GivenAStateCallbackThatThrows_WhenTheStateChanges_ThenTheSessionGoesOn)
{
  ss::session_options_t options = recorded_options (recorder_);
  std::shared_ptr<state_recorder> const recorder = recorder_;
  options.on_state_change = [recorder] (ss::state_change_t const &change)
    {
      recorder->add (change);
      throw std::runtime_error ("callback failure");
    };
  std::unique_ptr<ss::stream_session> session = make_session (options);
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  EXPECT_EQ (recorder_->count (), 2u);
}

TEST_F (
    LumexStreamSessionTest,
    GivenAThreadOfTheExecutor_WhenOpenIsCalledThere_ThenItReturnsWouldDeadlock)
{
  std::unique_ptr<ss::stream_session> session = make_session ();
  std::promise<std::error_code> result;
  asio::post (runner_.get_executor (),
              [&session, &result] ()
                {
                  tr::status_result_t const opened = session->open (ms (1000));
                  result.set_value (opened.has_value () ? std::error_code ()
                                                        : opened.error ());
                });
  std::future<std::error_code> got = result.get_future ();
  ASSERT_EQ (got.wait_for (std::chrono::seconds (3)),
             std::future_status::ready);
  EXPECT_TRUE (got.get () == transport_errc::would_deadlock);
}

TEST_F (
    LumexStreamSessionTest,
    GivenASilentDevice_WhenOpenWaitsPastItsDeadline_ThenItTimesOutAndTheSessionGoesOn)
{
  factory_->hold_call = 1;
  ss::session_options_t options = recorded_options (recorder_);
  options.open_timeout = ms (200);
  std::unique_ptr<ss::stream_session> session = make_session (options);
  tr::status_result_t const first = session->open (ms (30));
  ASSERT_FALSE (first.has_value ());
  EXPECT_TRUE (first.error () == transport_errc::timeout);
  EXPECT_EQ (session->state (), state::connecting);
  // The session went on: the attempt timed out and the next one opened.
  ASSERT_TRUE (session->open (ms (3000)).has_value ());
  EXPECT_EQ (factory_->call_count (), 2u);
}

TEST_F (LumexStreamSessionTest,
        GivenAMovedSession_WhenUsed_ThenTheTargetOwnsTheCycle)
{
  ss::stream_session first (runner_.get_executor (),
                            make_stream_factory (factory_),
                            recorded_options (recorder_));
  ss::stream_session second (std::move (first));
  ASSERT_TRUE (second.open (ms (2000)).has_value ());
  ss::stream_session third (runner_.get_executor (),
                            make_stream_factory (factory_),
                            recorded_options (recorder_));
  third = std::move (second);
  EXPECT_TRUE (third.is_open ());
  EXPECT_FALSE (second.is_open ());
}

TEST_F (
    LumexStreamSessionTest,
    GivenTheStrandBusyInTheStateCallback_WhenClosedFromAnotherThread_ThenTheAttemptThatWouldFollowDoesNotStart)
{
  std::promise<void> entered;
  std::promise<void> release;
  std::shared_future<void> const released = release.get_future ().share ();
  std::atomic<bool> armed (false);
  ss::session_options_t options = recorded_options (recorder_);
  std::shared_ptr<state_recorder> const recorder = recorder_;
  options.on_state_change =
      [recorder, &armed, &entered, released] (ss::state_change_t const &change)
    {
      recorder->add (change);
      // The loss is being handled on the strand: the next attempt is the
      // following step of the same strand call.
      if (change.current == state::reconnecting && armed.exchange (false))
        {
          entered.set_value ();
          released.wait ();
        }
    };
  std::unique_ptr<ss::stream_session> session = make_session (options);
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  armed.store (true);
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  factory_->last ().disconnect ();
  ASSERT_EQ (entered.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  session->close ();
  release.set_value ();
  ASSERT_TRUE (
      eventually ([&session] { return session->state () == state::closed; }));
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (factory_->call_count (), 1u);
}

// The link is established (open longer than initial_delay, see
// reconnect_policy_t): its loss starts a new cycle whose first attempt does
// not wait the policy's delay.
TEST_F (
    LumexStreamSessionTest,
    GivenALongPolicy_WhenAnEstablishedLinkIsLost_ThenTheFirstReconnectAttemptRunsAtOnce)
{
  std::unique_ptr<ss::stream_session> session
      = make_session (recorded_options (recorder_, ms (300), ms (10000)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (350));
  clock_type::time_point const before = clock_type::now ();
  factory_->last ().disconnect ();
  ASSERT_TRUE (eventually (
      [&session] { return session->session_statistics ().reconnects == 1; },
      ms (2000)));
  EXPECT_LT (clock_type::now () - before, ms (200));
}

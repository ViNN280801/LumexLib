// Sessions and device_watcher on the timer seam: on a virtual clock the retry
// delays of the policy, the open timeout, the time a link must stay open to
// count as established and the period of the watcher are exact virtual
// times, and a re-enumeration (the node goes, a new node of the same device
// appears 14 s later) runs without a real wait; without the option they keep
// real time.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexVirtualClock.hpp"
#include "lumex/applied/transport/time/LumexTimerSource.hpp"

namespace
{
using namespace lumex_session_test;
namespace ttime = tr::time;
using state = ss::channel_state;
using tr::testing::virtual_clock;

// A run that needs the virtual delays in real time takes seconds; on the
// virtual clock the whole test takes far less than this.
ms const KREAL_TIME_GUARD = ms (500);

long
since_start (ttime::time_point_t at)
{
  return static_cast<long> (
      std::chrono::duration_cast<ms> (at - ttime::time_point_t ()).count ());
}

// One call of a factory: the node it opened and the virtual time.
struct factory_call
{
  std::string path;
  long at_ms;
};

// Mock streams named by the node the selector finds in a fake listing; the
// first `failures` opens fail. Thread-safe.
struct node_factory_state
{
  std::mutex mutex;
  std::vector<ss::serial_port_info_t> nodes;
  std::vector<factory_call> calls;
  std::vector<tr::testing::mock_stream_control> controls;
  std::size_t failures = 0;

  void
  set_nodes (std::vector<ss::serial_port_info_t> const &now)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    nodes = now;
  }

  std::vector<ss::serial_port_info_t>
  list ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return nodes;
  }

  std::vector<factory_call>
  all_calls ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls;
  }

  std::size_t
  call_count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls.size ();
  }

  tr::testing::mock_stream_control
  last ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    if (controls.empty ())
      throw std::logic_error ("no stream made yet");
    return controls.back ();
  }
};

ss::serial_selector_t
test_device ()
{
  ss::serial_selector_t selector;
  selector.vid = "0F0F";
  selector.pid = "0002";
  selector.serial_number = "UNIT7";
  return selector;
}

ss::serial_port_info_t
node (std::string const &path)
{
  ss::serial_port_info_t port;
  port.path = path;
  port.vid = "0F0F";
  port.pid = "0002";
  port.serial_number = "UNIT7";
  return port;
}

ss::stream_session::factory_t
node_factory (std::shared_ptr<node_factory_state> const &state,
              virtual_clock const &clock)
{
  virtual_clock const *const at = &clock;
  return [state,
          at] (tr::executor_type const &executor) -> tr::stream::byte_stream
           {
             std::string const path
                 = ss::find_serial_port (test_device (), state->list ());
             std::lock_guard<std::mutex> const lock (state->mutex);
             factory_call const call = { path, since_start (at->now ()) };
             state->calls.push_back (call);
             if (path.empty ())
               throw std::runtime_error ("no node");
             tr::testing::mock_stream made
                 = tr::testing::make_mock_stream (executor, path);
             if (state->calls.size () <= state->failures)
               made.control.fail_open (
                   std::make_error_code (std::errc::no_such_device));
             state->controls.push_back (made.control);
             return std::move (made.stream);
           };
}

ss::session_options_t
virtual_options (virtual_clock const &clock)
{
  ss::session_options_t options;
  options.name = "virtual";
  options.policy.initial_delay = ms (100);
  options.policy.max_delay = ms (1000);
  options.policy.multiplier = 2.0;
  options.policy.jitter = 0.0;
  options.timers = clock.source ();
  return options;
}

class LumexSessionVirtualTimeTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  std::shared_ptr<node_factory_state> nodes_
      = std::make_shared<node_factory_state> ();
  clock_type::time_point real_start_ = clock_type::now ();

  bool
  run (std::function<bool ()> const &done, ms budget)
  {
    return tr::testing::run_until (context_, clock_, done, budget);
  }

  // Lets the posted steps of a destroyed session or watcher run.
  void
  settle ()
  {
    run ([] { return false; }, ms (0));
  }

  ms
  real_elapsed () const
  {
    return std::chrono::duration_cast<ms> (clock_type::now () - real_start_);
  }
};
} // namespace

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenAFactoryThatFailsFiveTimes_WhenOpenedOnAVirtualClock_ThenTheAttemptsFollowThePolicyToTheMillisecond)
{
  nodes_->set_nodes ({ node ("/dev/ttyVIRT0") });
  nodes_->failures = 5;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context_.get_executor (), node_factory (nodes_, clock_),
      virtual_options (clock_)));
  ss::stream_session &link = *session;
  link.start ();
  ASSERT_TRUE (
      run ([&link] { return link.state () == state::open; }, ms (60000)));
  std::vector<factory_call> const calls = nodes_->all_calls ();
  ASSERT_EQ (calls.size (), 6u);
  // 100, 200, 400, 800, then 1600 capped to 1000 (no jitter).
  long const expected[] = { 0, 100, 300, 700, 1500, 2500 };
  for (std::size_t i = 0; i < 6; ++i)
    EXPECT_EQ (calls[i].at_ms, expected[i]) << "attempt " << i + 1;
  EXPECT_EQ (since_start (clock_.now ()), 2500);
  ss::session_statistics_t const counts = link.session_statistics ();
  EXPECT_EQ (counts.attempts, 6u);
  EXPECT_EQ (counts.failed_attempts, 5u);
  EXPECT_LT (real_elapsed (), KREAL_TIME_GUARD);
  session.reset ();
  settle ();
}

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenAnOpenThatNeverCompletes_WhenTheVirtualOpenTimeoutPasses_ThenTheAttemptFailsWithTimeoutThenRetries)
{
  nodes_->set_nodes ({ node ("/dev/ttyVIRT0") });
  ss::session_options_t options = virtual_options (clock_);
  options.open_timeout = ms (5000);
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context_.get_executor (),
      [this] (tr::executor_type const &executor) -> tr::stream::byte_stream
        {
          std::lock_guard<std::mutex> const lock (nodes_->mutex);
          factory_call const call = { "held", since_start (clock_.now ()) };
          nodes_->calls.push_back (call);
          tr::testing::mock_stream made
              = tr::testing::make_mock_stream (executor, "held");
          if (nodes_->calls.size () == 1)
            made.control.hold_next_open ();
          return std::move (made.stream);
        },
      options));
  ss::stream_session &link = *session;
  link.start ();
  ASSERT_TRUE (
      run ([&link] { return link.state () == state::open; }, ms (60000)));
  std::vector<factory_call> const calls = nodes_->all_calls ();
  ASSERT_EQ (calls.size (), 2u);
  EXPECT_EQ (calls[0].at_ms, 0);
  // The open timed out at 5000, the retry waited the first delay.
  EXPECT_EQ (calls[1].at_ms, 5100);
  EXPECT_EQ (link.last_error (),
             make_error_code (tr::error::transport_errc::timeout));
  EXPECT_LT (real_elapsed (), KREAL_TIME_GUARD);
  session.reset ();
  settle ();
}

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenALinkThatStayedOpenLongerThanTheInitialDelay_WhenItIsLost_ThenTheNewCycleAttemptsAtOnce)
{
  nodes_->set_nodes ({ node ("/dev/ttyVIRT0") });
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context_.get_executor (), node_factory (nodes_, clock_),
      virtual_options (clock_)));
  ss::stream_session &link = *session;
  link.start ();
  ASSERT_TRUE (
      run ([&link] { return link.state () == state::open; }, ms (1000)));
  // Open for 200 virtual ms (established after initial_delay, 100 ms).
  EXPECT_FALSE (run ([] { return false; }, ms (200)));
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  link.async_read_some (tr::mutable_bytes_t (buffer),
                        [&read] (std::error_code error, std::size_t bytes)
                          { read.set (error, bytes); });
  settle ();
  nodes_->last ().disconnect ();
  ASSERT_TRUE (run (
      [this, &link]
        { return nodes_->call_count () == 2 && link.state () == state::open; },
      ms (10000)));
  std::vector<factory_call> const calls = nodes_->all_calls ();
  EXPECT_EQ (calls[1].at_ms, 200);
  EXPECT_EQ (link.session_statistics ().reconnects, 1u);
  session.reset ();
  settle ();
}

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenALinkLostBeforeTheInitialDelayWithoutData_WhenItIsLost_ThenTheNextAttemptWaitsTheDelay)
{
  nodes_->set_nodes ({ node ("/dev/ttyVIRT0") });
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context_.get_executor (), node_factory (nodes_, clock_),
      virtual_options (clock_)));
  ss::stream_session &link = *session;
  link.start ();
  ASSERT_TRUE (
      run ([&link] { return link.state () == state::open; }, ms (1000)));
  // Lost after 50 virtual ms: not established, no data moved.
  EXPECT_FALSE (run ([] { return false; }, ms (50)));
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  link.async_read_some (tr::mutable_bytes_t (buffer),
                        [&read] (std::error_code error, std::size_t bytes)
                          { read.set (error, bytes); });
  settle ();
  nodes_->last ().disconnect ();
  ASSERT_TRUE (run (
      [this, &link]
        { return nodes_->call_count () == 2 && link.state () == state::open; },
      ms (10000)));
  std::vector<factory_call> const calls = nodes_->all_calls ();
  EXPECT_EQ (calls[1].at_ms, 150);
  session.reset ();
  settle ();
}

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenAWatchedDevice_WhenItsNodeGoesAndANewNodeAppearsFourteenSecondsLater_ThenTheSessionReopensOnTheNewNodeWithoutARealWait)
{
  nodes_->set_nodes ({ node ("/dev/ttyVIRT0") });
  std::shared_ptr<node_factory_state> const nodes = nodes_;
  ss::device_watcher_options_t watching;
  watching.period = ms (1000);
  watching.enumeration = [nodes] () { return nodes->list (); };
  watching.timers = clock_.source ();
  std::unique_ptr<ss::device_watcher> watcher (
      new ss::device_watcher (context_.get_executor (), watching));
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context_.get_executor (), node_factory (nodes_, clock_),
      virtual_options (clock_)));
  ss::stream_session &link = *session;
  watcher->watch_presence (test_device (), link.presence_sink ());
  link.start ();
  ASSERT_TRUE (
      run ([&link] { return link.state () == state::open; }, ms (5000)));
  EXPECT_EQ (since_start (clock_.now ()), 0);

  // Ten quiet seconds, then the node goes.
  EXPECT_FALSE (run ([] { return false; }, ms (10000)));
  EXPECT_EQ (since_start (clock_.now ()), 10000);
  nodes_->set_nodes ({});
  nodes_->last ().disconnect ();
  ASSERT_TRUE (run ([&link] { return link.state () == state::reconnecting; },
                    ms (5000)));
  // The next poll saw it.
  EXPECT_EQ (since_start (clock_.now ()), 11000);

  // Absent for the next polls: the session waits for the device.
  EXPECT_FALSE (run ([] { return false; }, ms (13000)));
  EXPECT_EQ (since_start (clock_.now ()), 24000);
  EXPECT_EQ (nodes_->call_count (), 1u);
  EXPECT_EQ (link.state (), state::reconnecting);

  // 14 s after it went, the device is back under another node.
  nodes_->set_nodes ({ node ("/dev/ttyVIRT1") });
  ASSERT_TRUE (run (
      [this, &link]
        { return nodes_->call_count () == 2 && link.state () == state::open; },
      ms (5000)));
  std::vector<factory_call> const calls = nodes_->all_calls ();
  ASSERT_EQ (calls.size (), 2u);
  EXPECT_EQ (calls[0].path, "/dev/ttyVIRT0");
  EXPECT_EQ (calls[1].path, "/dev/ttyVIRT1");
  EXPECT_EQ (calls[1].at_ms, 25000);
  EXPECT_EQ (link.session_statistics ().reconnects, 1u);
  EXPECT_EQ (link.session_statistics ().failed_attempts, 0u);
  EXPECT_LT (real_elapsed (), KREAL_TIME_GUARD);
  watcher.reset ();
  session.reset ();
  settle ();
}

TEST_F (
    LumexSessionVirtualTimeTest,
    GivenAWatcherOnAVirtualClock_WhenTheClockAdvances_ThenItPollsOncePerPeriodAndPollNowRestartsThePeriod)
{
  std::shared_ptr<std::mutex> const mutex = std::make_shared<std::mutex> ();
  std::shared_ptr<std::vector<long>> const polled
      = std::make_shared<std::vector<long>> ();
  virtual_clock const *const at = &clock_;
  ss::device_watcher_options_t watching;
  watching.period = ms (250);
  watching.enumeration = [mutex, polled, at] ()
    {
      std::lock_guard<std::mutex> const lock (*mutex);
      polled->push_back (since_start (at->now ()));
      return std::vector<ss::serial_port_info_t> ();
    };
  watching.timers = clock_.source ();
  std::unique_ptr<ss::device_watcher> watcher (
      new ss::device_watcher (context_.get_executor (), watching));
  ss::device_watcher &watched = *watcher;
  ASSERT_TRUE (run ([&watched] { return watched.polls () == 1; }, ms (0)));
  for (std::uint64_t i = 2; i <= 4; ++i)
    {
      EXPECT_EQ (clock_.advance_to_next (), 1u);
      ASSERT_TRUE (
          run ([&watched, i] { return watched.polls () == i; }, ms (0)));
    }
  EXPECT_EQ (clock_.advance (ms (100)), 0u);
  watched.poll_now ();
  ASSERT_TRUE (run ([&watched] { return watched.polls () == 5; }, ms (0)));
  EXPECT_EQ (clock_.next_expiry (), ttime::time_point_t () + ms (1100));
  EXPECT_EQ (clock_.advance_to_next (), 1u);
  ASSERT_TRUE (run ([&watched] { return watched.polls () == 6; }, ms (0)));
  {
    std::lock_guard<std::mutex> const lock (*mutex);
    std::vector<long> const expected = { 0, 250, 500, 750, 850, 1100 };
    EXPECT_EQ (*polled, expected);
  }
  EXPECT_EQ (clock_.pending (), 1u);
  EXPECT_LT (real_elapsed (), KREAL_TIME_GUARD);
  watcher.reset ();
  settle ();
  EXPECT_EQ (clock_.pending (), 0u);
}

TEST (
    LumexSessionDefaultTimersTest,
    GivenDefaultOptions_WhenASessionRetries_ThenItWaitsInRealTimeOnTheSteadySource)
{
  ss::session_options_t defaults;
  EXPECT_TRUE (defaults.timers == ttime::steady_timer_source ());
  ss::device_watcher_options_t watching;
  EXPECT_TRUE (watching.timers == ttime::steady_timer_source ());
  EXPECT_EQ (watching.period, ms (1000));

  tr::stream::io_runner runner (2);
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  factory->open_failures = 1;
  std::shared_ptr<state_recorder> const recorder
      = std::make_shared<state_recorder> ();
  // A virtual clock beside it changes nothing: the session never took it.
  virtual_clock unused;
  ss::stream_session session (runner.get_executor (),
                              make_stream_factory (factory),
                              recorded_options (recorder, ms (60), ms (100)));
  tr::status_result_t const opened = session.open (ms (5000));
  ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
  std::vector<clock_type::time_point> const calls = factory->call_times ();
  ASSERT_EQ (calls.size (), 2u);
  EXPECT_GE (calls[1] - calls[0], ms (59));
  EXPECT_EQ (unused.pending (), 0u);
}

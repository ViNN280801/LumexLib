// The regression tests of the review of the sessions (REVIEW13; the id of
// each finding is in the comment above its test): no missed state change
// (RT_A), the open timeout that fails the attempt at once (RT_B), the backoff
// across links that die right after their open (RT_C), close and destruction
// while the factory runs (RT_D to RT_G), the watcher destroyed or unwatched
// inside its callback (RT_H, RT_I), path selectors by another name of the
// node (RT_J), the delay floor (RT_K), races of open waits and stops (RT_L),
// a restart from the state callback (RT_M), a lost link queued behind a
// cancel (RT_P), moved-from sessions and watchers (RT_Q), and one test per
// mutation of the review that no test killed (U01 to U17).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <cstdio>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
typedef ss::channel_state state;

// A stream whose open ignores cancel () like the serial stream (the bounded
// open runs on a worker thread): it completes after `delay`, with
// `cancelled` when a close came first.
class deaf_open_impl : public tr::stream::stream_impl
{
public:
  deaf_open_impl (tr::executor_type const &executor, ms delay)
      : tr::stream::stream_impl (executor, "deaf"), timer_ (strand ()),
        delay_ (delay)
  {
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    pending_ = std::move (handler);
    timer_.expires_after (delay_);
    std::shared_ptr<deaf_open_impl> const self
        = std::static_pointer_cast<deaf_open_impl> (shared_from_this ());
    timer_.async_wait ([self] (std::error_code) { self->finish (); });
  }

  void
  do_read_some (tr::mutable_bytes_t, tr::io_handler_t handler) override
  {
    complete (std::move (handler), transport_errc::disconnected, 0);
  }

  void
  do_write_some (tr::const_bytes_t, tr::io_handler_t handler) override
  {
    complete (std::move (handler), transport_errc::disconnected, 0);
  }

  void
  do_cancel () override
  {
  }

  void
  do_close () override
  {
    close_requested_ = true;
  }

private:
  void
  finish ()
  {
    if (!pending_)
      return;
    if (close_requested_)
      {
        complete (std::move (pending_), transport_errc::cancelled);
        return;
      }
    set_open (true);
    complete (std::move (pending_), std::error_code ());
  }

  asio::steady_timer timer_;
  ms delay_;
  tr::status_handler_t pending_;
  bool close_requested_ = false;
};

// A factory that blocks in its first call until the test releases it.
struct gate
{
  std::promise<void> entered;
  std::promise<void> release;
  std::shared_future<void> released;
  std::atomic<int> calls;
  gate () : released (release.get_future ().share ()), calls (0) {}
};

ss::stream_session::factory_t
gated (std::shared_ptr<gate> const &g, ss::stream_session::factory_t inner)
{
  return [g, inner] (tr::executor_type const &executor)
    {
      if (g->calls.fetch_add (1) == 0)
        {
          g->entered.set_value ();
          g->released.wait ();
        }
      return inner (executor);
    };
}

// A session reached from its own state callback (a client that starts a
// read whenever the link opens).
struct session_slot
{
  std::mutex mutex;
  ss::stream_session *session = nullptr;
  std::vector<std::uint8_t> buffer = std::vector<std::uint8_t> (4);
};

// A factory of mock streams whose first read fails with `disconnected`: a
// link that dies right after each open once the client reads.
ss::stream_session::factory_t
flapping_factory (std::shared_ptr<std::atomic<int>> const &calls)
{
  return [calls] (tr::executor_type const &executor) -> tr::stream::byte_stream
           {
             ++*calls;
             tr::testing::mock_stream made
                 = tr::testing::make_mock_stream (executor, "flap");
             made.control.fail_next_read (
                 make_error_code (transport_errc::disconnected));
             return std::move (made.stream);
           };
}

ss::session_options_t
reading_options (std::shared_ptr<session_slot> const &slot)
{
  ss::session_options_t options;
  options.name = "flap";
  options.policy.jitter = 0.0;
  options.on_state_change = [slot] (ss::state_change_t const &change)
    {
      if (change.current != state::open)
        return;
      std::lock_guard<std::mutex> const lock (slot->mutex);
      if (slot->session)
        slot->session->async_read_some (tr::mutable_bytes_t (slot->buffer),
                                        [] (std::error_code, std::size_t) {});
    };
  return options;
}

class LumexSessionRegressionTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> factory_
      = std::make_shared<stream_factory_state> ();
  std::shared_ptr<scripted_factory_state> scripted_
      = std::make_shared<scripted_factory_state> ();
  std::shared_ptr<state_recorder> recorder_
      = std::make_shared<state_recorder> ();
};
} // namespace

// RT_A: a reconnect (open, reconnecting, open) that completes before the
// wait is reported by the wait from the snapshot seen before it; the
// snapshot of the completion is the start of the next wait.
TEST_F (
    LumexSessionRegressionTest,
    GivenAFastReconnect_WhenWaitingFromTheSnapshotSeenBefore_ThenTheChangeIsReported)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_, ms (5), ms (20))));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  ss::state_snapshot_t const seen = session->state_snapshot ();
  ASSERT_EQ (seen.state, state::open);
  ASSERT_EQ (seen.sequence, 2u) << "closed -> connecting -> open";
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (20));
  factory_->last ().disconnect ();
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        }));
  outcome<int> changed;
  ss::state_snapshot_t now;
  session->async_wait_state_change (
      seen,
      [&changed, &now] (std::error_code e, ss::state_snapshot_t s)
        {
          now = s;
          changed.set (e, 0);
        });
  ASSERT_TRUE (changed.wait (ms (1000)))
      << "two changes happened since the snapshot; the wait heard none";
  EXPECT_FALSE (changed.error);
  EXPECT_EQ (now.state, state::open);
  EXPECT_EQ (now.sequence, seen.sequence + 2);
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  ASSERT_FALSE (changes.empty ());
  EXPECT_EQ (changes.back ().sequence, now.sequence);
  // From the new snapshot: nothing until the next change, then the close.
  outcome<int> closed;
  ss::state_snapshot_t after;
  session->async_wait_state_change (
      now,
      [&closed, &after] (std::error_code e, ss::state_snapshot_t s)
        {
          after = s;
          closed.set (e, 0);
        });
  EXPECT_FALSE (closed.wait (ms (50)));
  session->close ();
  ASSERT_TRUE (closed.wait ());
  EXPECT_TRUE (closed.error == transport_errc::cancelled);
}

// RT_A: the wait from a state reports a change that happened after the call
// even when the state came back to `seen` before the wait's step ran.
TEST_F (
    LumexSessionRegressionTest,
    GivenAWaitFromAState_WhenTheStateChangesAndComesBackBeforeItsStep_ThenTheChangeIsReported)
{
  ss::stream_session session (runner_.get_executor (),
                              make_scripted_factory (scripted_),
                              recorded_options (recorder_));
  session.start ();
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 1
                 && scripted_->at (0)->open_pending ();
        }));
  scripted_->at (0)->finish_open (std::error_code ());
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::open; }));
  // Established (initial_delay 5 ms), then lost through a failed write.
  std::this_thread::sleep_for (ms (20));
  scripted_->at (0)->fail_writes (
      make_error_code (transport_errc::disconnected));
  std::vector<std::uint8_t> const data (2, std::uint8_t (1));
  outcome<int> wrote;
  session.async_write_some (tr::const_bytes_t (data),
                            [&wrote] (std::error_code e, std::size_t n)
                              { wrote.set (e, static_cast<int> (n)); });
  ASSERT_TRUE (wrote.wait ());
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 2
                 && scripted_->at (1)->open_pending ();
        }));
  ASSERT_EQ (session.state (), state::reconnecting);
  // Hold the session strand (a wait from closed completes on it at once).
  std::promise<void> release;
  std::shared_future<void> const released = release.get_future ().share ();
  outcome<int> holding;
  session.async_wait_state_change (
      state::closed,
      [&holding, released] (std::error_code, state)
        {
          holding.set (std::error_code (), 0);
          released.wait ();
        });
  ASSERT_TRUE (holding.wait ());
  // Queued behind it: the open of the second stream (reconnecting -> open),
  // a removal of the device right after (open -> reconnecting), its return.
  scripted_->at (1)->finish_open (std::error_code ());
  std::this_thread::sleep_for (ms (50));
  session.set_device_present (false);
  session.set_device_present (true);
  // Called while the state is still reconnecting; its step runs last.
  outcome<int> changed;
  session.async_wait_state_change (
      state::reconnecting, [&changed] (std::error_code e, state now)
        { changed.set (e, static_cast<int> (now)); });
  release.set_value ();
  ASSERT_TRUE (changed.wait (ms (1000)))
      << "reconnecting -> open -> reconnecting after the call went unheard";
  EXPECT_FALSE (changed.error);
  EXPECT_EQ (static_cast<state> (changed.value), state::reconnecting);
  std::vector<state> const states = recorder_->states ();
  ASSERT_GE (states.size (), 2u);
  EXPECT_EQ (states[states.size () - 2], state::open);
  EXPECT_EQ (states.back (), state::reconnecting);
  session.close ();
}

// RT_B: open_timeout bounds one attempt also when the transport's open
// ignores cancel (): the attempt fails with `timeout` at the timeout, and the
// transport closes what it opens late.
TEST_F (
    LumexSessionRegressionTest,
    GivenATransportWhoseOpenIgnoresCancel_WhenTheOpenTimeoutPasses_ThenTheAttemptFailsAtOnceAndTheLateOpenIsClosed)
{
  ss::session_options_t options = recorded_options (recorder_);
  options.open_timeout = ms (50);
  options.policy.max_attempts = 1;
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  std::shared_ptr<std::vector<std::shared_ptr<deaf_open_impl>>> const made
      = std::make_shared<std::vector<std::shared_ptr<deaf_open_impl>>> ();
  std::shared_ptr<std::mutex> const made_mutex
      = std::make_shared<std::mutex> ();
  ss::stream_session session (
      runner_.get_executor (),
      [calls, made, made_mutex] (
          tr::executor_type const &executor) -> tr::stream::byte_stream
        {
          ++*calls;
          std::shared_ptr<deaf_open_impl> const impl
              = std::make_shared<deaf_open_impl> (executor, ms (400));
          std::lock_guard<std::mutex> const lock (*made_mutex);
          made->push_back (impl);
          return tr::stream::byte_stream (impl);
        },
      options);
  session.start ();
  std::this_thread::sleep_for (ms (250));
  EXPECT_EQ (session.state (), state::faulted)
      << "the attempt still runs 200 ms after its 50 ms open timeout";
  EXPECT_EQ (session.session_statistics ().failed_attempts, 1u);
  EXPECT_TRUE (session.last_error () == transport_errc::timeout);
  std::this_thread::sleep_for (ms (300));
  EXPECT_EQ (session.state (), state::faulted);
  EXPECT_EQ (calls->load (), 1);
  {
    std::lock_guard<std::mutex> const lock (*made_mutex);
    for (std::size_t i = 0; i < made->size (); ++i)
      EXPECT_FALSE ((*made)[i]->is_open ()) << "transport " << i;
  }
  session.close ();
}

// RT_C: a link that opens and dies at once (a TCP peer that accepts and
// closes, a device that resets on open) keeps the backoff of its cycle.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkThatDiesRightAfterEachOpen_WhenTheSessionReadsOnEachOpen_ThenReconnectsFollowThePolicy)
{
  std::shared_ptr<session_slot> const slot = std::make_shared<session_slot> ();
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  ss::session_options_t options = reading_options (slot);
  options.policy.initial_delay = ms (1000);
  options.policy.max_delay = ms (1000);
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), flapping_factory (calls), options));
  {
    std::lock_guard<std::mutex> const lock (slot->mutex);
    slot->session = session.get ();
  }
  session->start ();
  std::this_thread::sleep_for (ms (300));
  int const in_300ms = calls->load ();
  {
    std::lock_guard<std::mutex> const lock (slot->mutex);
    slot->session = nullptr;
  }
  session->close ();
  EXPECT_LE (in_300ms, 2) << "factory calls in 300 ms with a 1000 ms policy";
}

// RT_C: max_attempts ends a cycle of links that die right after their open.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkThatDiesRightAfterEachOpen_WhenMaxAttemptsIsSet_ThenTheSessionFaults)
{
  std::shared_ptr<session_slot> const slot = std::make_shared<session_slot> ();
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  ss::session_options_t options = reading_options (slot);
  options.policy.initial_delay = ms (2);
  options.policy.max_delay = ms (2);
  options.policy.max_attempts = 3;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), flapping_factory (calls), options));
  {
    std::lock_guard<std::mutex> const lock (slot->mutex);
    slot->session = session.get ();
  }
  session->start ();
  bool const faulted = eventually (
      [&session] { return session->state () == state::faulted; }, ms (1000));
  {
    std::lock_guard<std::mutex> const lock (slot->mutex);
    slot->session = nullptr;
  }
  EXPECT_TRUE (faulted) << "factory calls: " << calls->load ();
  EXPECT_EQ (calls->load (), 3);
  EXPECT_TRUE (session->last_error () == transport_errc::disconnected);
  session->close ();
}

// RT_D: close () from another thread while the factory runs on the strand:
// the transport it returns is not opened.
TEST_F (
    LumexSessionRegressionTest,
    GivenABlockedFactory_WhenClosedFromAnotherThread_ThenNothingOpensAndItEndsClosed)
{
  std::shared_ptr<gate> const g = std::make_shared<gate> ();
  ss::stream_session session (runner_.get_executor (),
                              gated (g, make_stream_factory (factory_)),
                              recorded_options (recorder_));
  session.start ();
  ASSERT_EQ (g->entered.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  std::thread closer ([&session] { session.close (); });
  closer.join ();
  g->release.set_value ();
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::closed; }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (g->calls.load (), 1);
  EXPECT_EQ (factory_->last ().open_count (), 0u)
      << "the transport was opened after close () returned";
  std::vector<state> const states = recorder_->states ();
  for (std::size_t i = 0; i < states.size (); ++i)
    EXPECT_NE (states[i], state::open) << i;
}

// RT_E: the front is destroyed inside the factory call.
TEST_F (
    LumexSessionRegressionTest,
    GivenAFactoryThatDestroysTheSession_WhenItRuns_ThenNothingBreaksAndTheImplementationIsReleased)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  std::unique_ptr<ss::stream_session> session;
  ss::stream_session::factory_t const inner = make_stream_factory (factory_);
  std::atomic<int> calls (0);
  session.reset (new ss::stream_session (
      runner_.get_executor (),
      [&session, &calls, inner, sentinel] (tr::executor_type const &executor)
        {
          ++calls;
          session.reset ();
          return inner (executor);
        },
      recorded_options (recorder_)));
  sentinel.reset ();
  session->start ();
  EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (calls.load (), 1);
  EXPECT_EQ (factory_->last ().open_count (), 0u);
}

// RT_F: destroyed from another thread while the factory blocks.
TEST_F (
    LumexSessionRegressionTest,
    GivenABlockedFactory_WhenTheSessionIsDestroyedFromAnotherThread_ThenNothingOpensAndItIsReleased)
{
  std::shared_ptr<gate> const g = std::make_shared<gate> ();
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  ss::stream_session::factory_t const inner
      = gated (g, make_stream_factory (factory_));
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (),
      [inner, sentinel] (tr::executor_type const &executor)
        { return inner (executor); }, recorded_options (recorder_)));
  sentinel.reset ();
  session->start ();
  ASSERT_EQ (g->entered.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  std::thread killer ([&session] { session.reset (); });
  killer.join ();
  g->release.set_value ();
  EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }));
  EXPECT_EQ (g->calls.load (), 1);
  EXPECT_EQ (factory_->last ().open_count (), 0u)
      << "the transport was opened after the session was destroyed";
}

// RT_G: cancel () while the factory blocks ends the cycle with `cancelled`.
TEST_F (LumexSessionRegressionTest,
        GivenABlockedFactory_WhenCancelled_ThenItEndsClosedWithCancelled)
{
  std::shared_ptr<gate> const g = std::make_shared<gate> ();
  ss::stream_session session (runner_.get_executor (),
                              gated (g, make_stream_factory (factory_)),
                              recorded_options (recorder_));
  outcome<int> opened;
  session.async_open ([&opened] (std::error_code e) { opened.set (e, 0); });
  ASSERT_EQ (g->entered.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  session.cancel ();
  g->release.set_value ();
  ASSERT_TRUE (opened.wait ());
  EXPECT_TRUE (opened.error == transport_errc::cancelled);
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::closed; }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (g->calls.load (), 1);
  std::vector<ss::state_change_t> const changes = recorder_->all ();
  ASSERT_FALSE (changes.empty ());
  EXPECT_TRUE (changes.back ().error == transport_errc::cancelled);
}

// RT_H: the watcher destroyed inside its own callback.
TEST_F (LumexSessionRegressionTest,
        GivenAWatcher_WhenDestroyedInsideItsCallback_ThenNoCallbackFollows)
{
  std::shared_ptr<std::vector<ss::serial_port_info_t>> const ports
      = std::make_shared<std::vector<ss::serial_port_info_t>> ();
  ss::serial_port_info_t a;
  a.path = "COM3";
  a.vid = "16E0";
  a.pid = "0001";
  ss::serial_port_info_t b = a;
  b.path = "COM4";
  ports->push_back (a);
  ports->push_back (b);
  std::unique_ptr<ss::device_watcher> watcher (new ss::device_watcher (
      runner_.get_executor (), ms (5), [ports] () { return *ports; }));
  ss::serial_selector_t selector;
  selector.vid = "16E0";
  std::atomic<int> calls (0);
  std::atomic<bool> gone (false);
  std::promise<void> done;
  watcher->watch (selector,
                  [&] (ss::device_event_t const &)
                    {
                      if (gone.load ())
                        {
                          ++calls;
                          return;
                        }
                      watcher.reset ();
                      gone.store (true);
                      done.set_value ();
                    });
  ASSERT_EQ (done.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  std::this_thread::sleep_for (ms (60));
  EXPECT_EQ (calls.load (), 0);
}

// RT_I: a watch ended inside its own callback.
TEST_F (LumexSessionRegressionTest,
        GivenAWatch_WhenUnwatchedInsideItsCallback_ThenLaterPollsCallNothing)
{
  std::shared_ptr<std::mutex> const mutex = std::make_shared<std::mutex> ();
  std::shared_ptr<std::vector<ss::serial_port_info_t>> const ports
      = std::make_shared<std::vector<ss::serial_port_info_t>> ();
  ss::device_watcher watcher (runner_.get_executor (), ms (5),
                              [ports, mutex] ()
                                {
                                  std::lock_guard<std::mutex> const lock (
                                      *mutex);
                                  return *ports;
                                });
  ss::serial_selector_t selector;
  selector.path = "COM3";
  std::atomic<int> calls (0);
  std::shared_ptr<ss::device_watcher::watch_id_t> const id
      = std::make_shared<ss::device_watcher::watch_id_t> (0);
  *id = watcher.watch (selector,
                       [&] (ss::device_event_t const &)
                         {
                           ++calls;
                           watcher.unwatch (*id);
                         });
  for (int i = 0; i < 10; ++i)
    {
      {
        std::lock_guard<std::mutex> const lock (*mutex);
        ports->clear ();
        if (i % 2 == 0)
          {
            ss::serial_port_info_t p;
            p.path = "\\\\.\\COM3";
            ports->push_back (p);
          }
      }
      std::this_thread::sleep_for (ms (20));
    }
  EXPECT_EQ (calls.load (), 1);
}

// RT_J: the other spellings of a Windows port name select it.
TEST (LumexSessionRegressionSelectorTest,
      GivenTheSpellingsOfAComName_WhenMatched_ThenEachSelectsThePort)
{
  ss::serial_port_info_t port;
  port.path = "\\\\.\\COM12";
  char const *const spellings[]
      = { "COM12",        "com12",        "\\\\.\\COM12",
          "\\\\.\\com12", "\\\\?\\COM12", "//./COM12" };
  for (std::size_t i = 0; i < sizeof (spellings) / sizeof (spellings[0]); ++i)
    {
      ss::serial_selector_t selector;
      selector.path = spellings[i];
      EXPECT_TRUE (ss::matches (selector, port)) << spellings[i];
    }
  ss::serial_selector_t other;
  other.path = "COM1";
  EXPECT_FALSE (ss::matches (other, port));
}

#if !defined(_WIN32)
// RT_J: a path selector by a symbolic link (/dev/serial/by-id/...) selects
// the port of its node, so a watcher that follows it reports the device
// present and the open session keeps its link; once the link is gone the
// selector names nothing.
TEST_F (
    LumexSessionRegressionTest,
    GivenAPathSelectorBySymbolicLink_WhenAWatcherFollowsIt_ThenTheOpenSessionKeepsItsLink)
{
  char directory[] = "/tmp/lumex-session-XXXXXX";
  ASSERT_NE (::mkdtemp (directory), nullptr);
  std::string const node = std::string (directory) + "/ttyLUMEX0";
  std::string const link
      = std::string (directory) + "/usb-FTDI_FT232R_B71946KX-if00-port0";
  {
    std::ofstream const created (node.c_str ());
  }
  ASSERT_EQ (::symlink (node.c_str (), link.c_str ()), 0);
  ss::serial_port_info_t port;
  port.path = node;
  port.vid = "0403";
  port.pid = "6001";
  ss::serial_selector_t selector;
  selector.path = link;
  EXPECT_TRUE (ss::matches (selector, port));

  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  {
    ss::device_watcher watcher (
        runner_.get_executor (), ms (3600000),
        [port] () { return std::vector<ss::serial_port_info_t> (1, port); });
    watcher.watch_presence (selector, session->presence_sink ());
    std::this_thread::sleep_for (ms (100));
    EXPECT_EQ (session->state (), state::open);
    EXPECT_EQ (factory_->call_count (), 1u);
  }
  ::unlink (link.c_str ());
  EXPECT_FALSE (ss::matches (selector, port));
  ::unlink (node.c_str ());
  ::rmdir (directory);
  session->close ();
}
#endif

// RT_K: max_delay 0 is legal per the formula; the session still waits 1 ms
// between attempts, other handlers run, and close () ends it promptly.
TEST_F (
    LumexSessionRegressionTest,
    GivenMaxDelayZero_WhenEveryAttemptFails_ThenAttemptsAreAtLeastOneMillisecondApartAndCloseEndsThem)
{
  factory_->open_failures = 100000000;
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_),
                              recorded_options (recorder_, ms (100), ms (0)));
  session.start ();
  std::this_thread::sleep_for (ms (100));
  std::shared_ptr<std::promise<void>> const ran
      = std::make_shared<std::promise<void>> ();
  asio::post (runner_.get_executor (), [ran] () { ran->set_value (); });
  ASSERT_EQ (ran->get_future ().wait_for (std::chrono::milliseconds (200)),
             std::future_status::ready);
  clock_type::time_point const closing = clock_type::now ();
  session.close ();
  ASSERT_TRUE (eventually (
      [&session] { return session.state () == state::closed; }, ms (200)));
  EXPECT_LT (clock_type::now () - closing, ms (100));
  std::uint64_t const at_close = session.session_statistics ().attempts;
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (session.session_statistics ().attempts, at_close)
      << "an attempt started after closed";
  // At least 1 ms between attempts: about 100 in 100 ms, not thousands.
  EXPECT_LE (at_close, 160u);
}

// RT_P (L2): an I/O result that reports a lost link while a cancel () is on
// its way (the stop epoch bumped, its step not run yet) is not lost: the
// session reconnects after the cancel.
TEST_F (LumexSessionRegressionTest,
        GivenALinkLossQueuedBehindACancel_WhenBothRun_ThenTheSessionReconnects)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<int> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, static_cast<int> (n)); });
  std::this_thread::sleep_for (ms (20));
  tr::testing::mock_stream_control control = factory_->last ();
  ss::stream_session *const raw = session.get ();
  outcome<int> waited;
  // A wait from `closed` completes at once; its handler runs on the session
  // strand. The read's disconnect completion queues on the strand behind it,
  // then the step of the cancel.
  session->async_wait_state_change (
      state::closed,
      [&waited, control, raw] (std::error_code e, state now) mutable
        {
          control.disconnect ();
          std::this_thread::sleep_for (ms (50));
          raw->cancel ();
          waited.set (e, static_cast<int> (now));
        });
  ASSERT_TRUE (waited.wait ());
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::disconnected);
  EXPECT_TRUE (eventually (
      [&session] { return session->session_statistics ().reconnects >= 1; },
      ms (1000)))
      << "the lost link was ignored: the session stays open on a dead link";
}

// U17: the same with close (): the stop wins, nothing reconnects.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkLossQueuedBehindAClose_WhenBothRun_ThenTheSessionClosesWithoutReconnecting)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<int> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, static_cast<int> (n)); });
  std::this_thread::sleep_for (ms (20));
  tr::testing::mock_stream_control control = factory_->last ();
  ss::stream_session *const raw = session.get ();
  outcome<int> waited;
  session->async_wait_state_change (
      state::closed,
      [&waited, control, raw] (std::error_code e, state now) mutable
        {
          control.disconnect ();
          std::this_thread::sleep_for (ms (50));
          raw->close ();
          waited.set (e, static_cast<int> (now));
        });
  ASSERT_TRUE (waited.wait ());
  ASSERT_TRUE (read.wait ());
  ASSERT_TRUE (
      eventually ([&session] { return session->state () == state::closed; }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (factory_->call_count (), 1u);
  std::vector<state> const states = recorder_->states ();
  ASSERT_FALSE (states.empty ());
  EXPECT_EQ (states.back (), state::closed);
  for (std::size_t i = 0; i < states.size (); ++i)
    EXPECT_NE (states[i], state::reconnecting) << i;
}

// RT_L: async_open and async_wait_state_change on many sessions with random
// close, cancel, start and destruction: each handler runs exactly once.
TEST (LumexSessionRegressionStressTest,
      GivenManySessions_WhenOpenWaitsAndStopsRace_ThenEveryHandlerEndsOnce)
{
  for (unsigned round = 0; round < 10; ++round)
    {
      std::mt19937 random (1000 + round);
      std::shared_ptr<std::atomic<int>> const issued
          = std::make_shared<std::atomic<int>> (0);
      std::shared_ptr<std::atomic<int>> const completed
          = std::make_shared<std::atomic<int>> (0);
      std::shared_ptr<std::atomic<int>> const twice
          = std::make_shared<std::atomic<int>> (0);
      {
        tr::stream::io_runner runner (2);
        std::vector<std::shared_ptr<stream_factory_state>> factories;
        std::vector<std::unique_ptr<ss::stream_session>> sessions;
        for (int i = 0; i < 16; ++i)
          {
            std::shared_ptr<stream_factory_state> f
                = std::make_shared<stream_factory_state> ();
            f->open_failures = random () % 3;
            f->throws = random () % 2;
            if (random () % 3 == 0)
              f->hold_call = 1 + random () % 2;
            ss::session_options_t o;
            o.policy.initial_delay = ms (1);
            o.policy.max_delay = ms (3);
            o.policy.max_attempts = random () % 2 ? 0u : 3u;
            o.open_timeout = ms (10);
            factories.push_back (f);
            sessions.emplace_back (new ss::stream_session (
                runner.get_executor (), make_stream_factory (f), o));
          }
        for (int step = 0; step < 600; ++step)
          {
            std::size_t const i = random () % sessions.size ();
            if (!sessions[i])
              continue;
            std::shared_ptr<std::atomic<bool>> const once
                = std::make_shared<std::atomic<bool>> (false);
            switch (random () % 8)
              {
              case 0:
              case 1:
                ++*issued;
                sessions[i]->async_open (
                    [once, completed, twice] (std::error_code)
                      {
                        if (once->exchange (true))
                          ++*twice;
                        else
                          ++*completed;
                      });
                break;
              case 2:
                ++*issued;
                sessions[i]->async_wait_state_change (
                    sessions[i]->state (),
                    [once, completed, twice] (std::error_code, state)
                      {
                        if (once->exchange (true))
                          ++*twice;
                        else
                          ++*completed;
                      });
                break;
              case 3:
                sessions[i]->close ();
                break;
              case 4:
                sessions[i]->cancel ();
                break;
              case 5:
                sessions[i]->start ();
                break;
              case 6:
                if (random () % 8 == 0)
                  sessions[i].reset ();
                else
                  sessions[i]->set_device_present (random () % 2 == 0);
                break;
              default:
                try
                  {
                    factories[i]->last ().disconnect ();
                  }
                catch (std::logic_error const &)
                  {
                  }
                break;
              }
            if (random () % 32 == 0)
              std::this_thread::sleep_for (ms (1));
          }
        for (std::size_t i = 0; i < sessions.size (); ++i)
          sessions[i].reset ();
        ASSERT_TRUE (
            eventually ([&] { return completed->load () == issued->load (); }))
            << completed->load () << " of " << issued->load ();
        EXPECT_EQ (twice->load (), 0);
      }
    }
}

// RT_M: start () from inside the state callback when the session closes.
TEST_F (
    LumexSessionRegressionTest,
    GivenAStateCallbackThatRestartsOnClosed_WhenClosed_ThenTheSessionOpensAgain)
{
  struct holder
  {
    std::mutex mutex;
    ss::stream_session *session = nullptr;
    int restarts = 0;
  };
  std::shared_ptr<holder> const h = std::make_shared<holder> ();
  ss::session_options_t options = recorded_options (recorder_);
  std::shared_ptr<state_recorder> const recorder = recorder_;
  options.on_state_change = [h, recorder] (ss::state_change_t const &change)
    {
      recorder->add (change);
      std::lock_guard<std::mutex> const lock (h->mutex);
      if (change.current == state::closed && h->session && h->restarts == 0)
        {
          ++h->restarts;
          h->session->start ();
        }
    };
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_), options));
  {
    std::lock_guard<std::mutex> const lock (h->mutex);
    h->session = session.get ();
  }
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  session->close ();
  EXPECT_TRUE (eventually (
      [&]
        {
          return factory_->call_count () == 2
                 && session->state () == state::open;
        }));
  {
    std::lock_guard<std::mutex> const lock (h->mutex);
    h->session = nullptr;
  }
}

// RT_Q (L3): every member of a moved-from session is safe; the asynchronous
// ones complete with `not_open`.
TEST_F (LumexSessionRegressionTest,
        GivenAMovedFromSession_WhenItsMembersAreCalled_ThenNothingCrashes)
{
  ss::stream_session first (runner_.get_executor (),
                            make_stream_factory (factory_));
  ss::stream_session second (std::move (first));
  EXPECT_FALSE (first.is_open ());
  EXPECT_EQ (first.state (), state::closed);
  EXPECT_TRUE (first.name ().empty ());
  EXPECT_FALSE (first.last_error ());
  EXPECT_EQ (first.statistics ().reads, 0u);
  EXPECT_EQ (first.session_statistics ().attempts, 0u);
  first.reset_statistics ();
  first.start ();
  first.set_device_present (false);
  first.presence_sink () (true);
  tr::status_result_t const opened = first.open (ms (100));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error () == transport_errc::not_open);
  outcome<int> async_opened;
  first.async_open ([&async_opened] (std::error_code e)
                      { async_opened.set (e, 0); });
  ASSERT_TRUE (async_opened.wait ());
  EXPECT_TRUE (async_opened.error == transport_errc::not_open);
  outcome<int> changed;
  first.async_wait_state_change (
      state::closed, [&changed] (std::error_code e, state now)
        { changed.set (e, static_cast<int> (now)); });
  ASSERT_TRUE (changed.wait ());
  EXPECT_TRUE (changed.error == transport_errc::not_open);
  std::vector<std::uint8_t> buffer (4);
  outcome<int> read;
  first.async_read_some (tr::mutable_bytes_t (buffer),
                         [&read] (std::error_code e, std::size_t n)
                           { read.set (e, static_cast<int> (n)); });
  ASSERT_TRUE (read.wait ());
  EXPECT_TRUE (read.error == transport_errc::not_open);
  first.cancel ();
  first.close ();
  ss::stream_session third (std::move (second));
  second = std::move (third);
  EXPECT_EQ (second.state (), state::closed);
}

TEST_F (LumexSessionRegressionTest,
        GivenAMovedFromMessageSession_WhenItIoRuns_ThenItEndsWithNotOpen)
{
  std::shared_ptr<datagram_factory_state> const datagrams
      = std::make_shared<datagram_factory_state> ();
  ss::message_session first (runner_.get_executor (),
                             make_datagram_factory (datagrams));
  ss::message_session second (std::move (first));
  outcome<int> received;
  first.async_receive ([&received] (std::error_code e, tr::datagram::message_t)
                         { received.set (e, 0); });
  ASSERT_TRUE (received.wait ());
  EXPECT_TRUE (received.error == transport_errc::not_open);
  outcome<int> sent;
  first.async_send (tr::datagram::message_t (),
                    [&sent] (std::error_code e) { sent.set (e, 0); });
  ASSERT_TRUE (sent.wait ());
  EXPECT_TRUE (sent.error == transport_errc::not_open);
}

TEST_F (LumexSessionRegressionTest,
        GivenAMovedFromWatcher_WhenItsMembersAreCalled_ThenNothingCrashes)
{
  ss::device_watcher first (
      runner_.get_executor (), ms (1000),
      [] () { return std::vector<ss::serial_port_info_t> (); });
  ss::device_watcher second (std::move (first));
  ss::serial_selector_t selector;
  selector.path = "COM3";
  EXPECT_EQ (first.watch (selector, [] (ss::device_event_t const &) {}), 0u);
  EXPECT_EQ (first.watch_presence (selector, [] (bool) {}), 0u);
  first.unwatch (1);
  first.poll_now ();
  first.stop ();
  EXPECT_EQ (first.polls (), 0u);
  ss::device_watcher third (std::move (second));
  second = std::move (third);
  EXPECT_TRUE (eventually ([&second] { return second.polls () >= 1u; }));
}

// U01: an I/O result of a transport the session already dropped does not
// drop the link that replaced it.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkThatWasReplaced_WhenALateReadOfTheOldTransportFails_ThenTheNewLinkStaysOpen)
{
  ss::stream_session session (runner_.get_executor (),
                              make_scripted_factory (scripted_),
                              recorded_options (recorder_));
  session.start ();
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 1
                 && scripted_->at (0)->open_pending ();
        }));
  scripted_->at (0)->finish_open (std::error_code ());
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::open; }));
  std::vector<std::uint8_t> buffer (4);
  outcome<int> late;
  session.async_read_some (tr::mutable_bytes_t (buffer),
                           [&late] (std::error_code e, std::size_t n)
                             { late.set (e, static_cast<int> (n)); });
  ASSERT_TRUE (
      eventually ([this] { return scripted_->at (0)->read_pending (); }));
  // A failed write loses the first link; its read stays pending.
  scripted_->at (0)->fail_writes (
      make_error_code (transport_errc::disconnected));
  std::vector<std::uint8_t> const data (2, std::uint8_t (1));
  outcome<int> wrote;
  session.async_write_some (tr::const_bytes_t (data),
                            [&wrote] (std::error_code e, std::size_t n)
                              { wrote.set (e, static_cast<int> (n)); });
  ASSERT_TRUE (wrote.wait ());
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 2
                 && scripted_->at (1)->open_pending ();
        }));
  scripted_->at (1)->finish_open (std::error_code ());
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session.state () == state::open
                 && session.session_statistics ().reconnects == 1;
        }));
  // The dropped transport's read ends late with a lost link.
  scripted_->at (0)->finish_read (
      make_error_code (transport_errc::disconnected), 0);
  ASSERT_TRUE (late.wait ());
  EXPECT_TRUE (late.error == transport_errc::disconnected);
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (session.state (), state::open);
  EXPECT_EQ (scripted_->count (), 2u);
  EXPECT_EQ (session.session_statistics ().reconnects, 1u);
  session.close ();
}

// U02: the completion of a retry wait cancelled by an arrival does not make
// an attempt of its own.
TEST_F (
    LumexSessionRegressionTest,
    GivenALongRetryWait_WhenTheDeviceArrivesAndTheAttemptFailsAgain_ThenOnlyOneAttemptRuns)
{
  factory_->throws = 1000000;
  ss::stream_session session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_, ms (60000), ms (60000)));
  session.start ();
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () == 1; }));
  std::this_thread::sleep_for (ms (20));
  session.set_device_present (true);
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () >= 2; }));
  std::this_thread::sleep_for (ms (100));
  EXPECT_EQ (factory_->call_count (), 2u);
  EXPECT_EQ (session.session_statistics ().attempts, 2u);
  session.close ();
}

// U03: the late success of an open the session already gave up on (its
// timeout) is not taken for the open of the attempt that replaced it.
TEST_F (
    LumexSessionRegressionTest,
    GivenAnOpenThatTimedOut_WhenItSucceedsLateWhileTheNextAttemptOpens_ThenTheSessionStaysConnecting)
{
  ss::session_options_t options = recorded_options (recorder_, ms (1), ms (1));
  options.open_timeout = ms (100);
  ss::stream_session session (runner_.get_executor (),
                              make_scripted_factory (scripted_), options);
  session.start ();
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 2
                 && scripted_->at (1)->open_pending ();
        },
      ms (2000)));
  scripted_->at (0)->finish_open (std::error_code ());
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (session.state (), state::connecting);
  std::vector<state> const states = recorder_->states ();
  for (std::size_t i = 0; i < states.size (); ++i)
    EXPECT_NE (states[i], state::open) << i;
  session.close ();
}

// U10: the presence sink does not keep the session alive.
TEST_F (
    LumexSessionRegressionTest,
    GivenAPresenceSink_WhenTheSessionIsDestroyed_ThenItIsReleasedAndTheSinkDoesNothing)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  ss::stream_session::factory_t const inner = make_stream_factory (factory_);
  ss::presence_sink_t sink;
  {
    ss::stream_session session (
        runner_.get_executor (),
        [inner, sentinel] (tr::executor_type const &executor)
          { return inner (executor); }, recorded_options (recorder_));
    sentinel.reset ();
    ASSERT_TRUE (session.open (ms (2000)).has_value ());
    sink = session.presence_sink ();
  }
  EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }))
      << "the presence sink keeps the session implementation alive";
  sink (false);
  sink (true);
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (factory_->call_count (), 1u);
}

// U16: a link that stayed open longer than initial_delay starts a fresh
// cycle when it goes: the first attempt runs at once.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkThatStayedOpen_WhenItGoesAfterAFailedStart_ThenTheFirstReconnectRunsAtOnce)
{
  factory_->open_failures = 2;
  ss::session_options_t options
      = recorded_options (recorder_, ms (100), ms (1000));
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_), options);
  ASSERT_TRUE (session.open (ms (3000)).has_value ());
  ASSERT_EQ (factory_->call_count (), 3u);
  std::this_thread::sleep_for (ms (150));
  std::vector<std::uint8_t> buffer (4);
  session.async_read_some (tr::mutable_bytes_t (buffer),
                           [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (10));
  clock_type::time_point const lost = clock_type::now ();
  factory_->last ().disconnect ();
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () == 4; }));
  std::vector<clock_type::time_point> const calls = factory_->call_times ();
  EXPECT_LT (calls[3] - lost, ms (60))
      << "the reconnect waited the delay of the failed start";
  session.close ();
}

// --- The re-review of the fixes (REVIEW13 re-review: H1, H2, R1 to R7) ---

namespace
{
struct deaf_record
{
  std::atomic<int> closes{ 0 };
  std::atomic<int> late_opens{ 0 };
};

// An open that ignores cancel () (as the serial bounded open): it ends after
// `delay`; when close () came first it ends with cancelled and stays closed.
// Records the closes and the opens that happened anyway.
class recording_deaf_impl : public tr::stream::stream_impl
{
public:
  recording_deaf_impl (tr::executor_type const &executor, ms delay,
                       std::shared_ptr<deaf_record> record)
      : tr::stream::stream_impl (executor, "deaf"), timer_ (strand ()),
        delay_ (delay), record_ (std::move (record))
  {
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    pending_ = std::move (handler);
    timer_.expires_after (delay_);
    std::shared_ptr<recording_deaf_impl> const self
        = std::static_pointer_cast<recording_deaf_impl> (shared_from_this ());
    timer_.async_wait ([self] (std::error_code) { self->finish (); });
  }

  void
  do_read_some (tr::mutable_bytes_t, tr::io_handler_t handler) override
  {
    pending_read_ = std::move (handler);
  }

  void
  do_write_some (tr::const_bytes_t, tr::io_handler_t handler) override
  {
    complete (std::move (handler), std::error_code (), 0);
  }

  void
  do_cancel () override
  {
    if (pending_read_)
      complete (std::move (pending_read_), transport_errc::cancelled, 0);
  }

  void
  do_close () override
  {
    close_requested_ = true;
    record_->closes.fetch_add (1);
    set_open (false);
    if (pending_read_)
      complete (std::move (pending_read_), transport_errc::cancelled, 0);
  }

private:
  void
  finish ()
  {
    if (!pending_)
      return;
    if (close_requested_)
      {
        complete (std::move (pending_), transport_errc::cancelled);
        return;
      }
    record_->late_opens.fetch_add (1);
    set_open (true);
    complete (std::move (pending_), std::error_code ());
  }

  asio::steady_timer timer_;
  ms delay_;
  std::shared_ptr<deaf_record> record_;
  tr::status_handler_t pending_;
  tr::io_handler_t pending_read_;
  bool close_requested_ = false;
};

ss::stream_session::factory_t
recording_deaf_factory (ms delay, std::shared_ptr<deaf_record> const &record)
{
  return [delay, record] (tr::executor_type const &executor)
    {
      return tr::stream::byte_stream (
          std::make_shared<recording_deaf_impl> (executor, delay, record));
    };
}

// A client that owns its session (the "capture self" idiom of a handler).
struct owning_client
{
  std::unique_ptr<ss::stream_session> session;
};

// Runs `body` on a thread; true when it ended within `limit`.
bool
ends_within (std::function<void ()> body, ms limit)
{
  std::shared_ptr<std::promise<void>> const done
      = std::make_shared<std::promise<void>> ();
  std::future<void> ended = done->get_future ();
  std::thread worker (
      [body, done] ()
        {
          body ();
          done->set_value ();
        });
  if (ended.wait_for (limit) == std::future_status::ready)
    {
      worker.join ();
      return true;
    }
  worker.detach ();
  return false;
}
} // namespace

// R1 (RT_A with the fixed API): a wait from the snapshot taken before a fast
// reconnect reports the two changes.
TEST_F (
    LumexSessionRegressionTest,
    GivenASnapshotBeforeAFastReconnect_WhenWaitingFromItAfterwards_ThenBothChangesAreCounted)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  ss::state_snapshot_t const seen = session->state_snapshot ();
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (20));
  factory_->last ().disconnect ();
  ASSERT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        }));
  // Shared: the handler may run after an assertion ended the test.
  std::shared_ptr<std::promise<ss::state_snapshot_t>> const changed
      = std::make_shared<std::promise<ss::state_snapshot_t>> ();
  session->async_wait_state_change (
      seen, [changed] (std::error_code, ss::state_snapshot_t now)
        { changed->set_value (now); });
  std::future<ss::state_snapshot_t> got = changed->get_future ();
  ASSERT_EQ (got.wait_for (std::chrono::milliseconds (300)),
             std::future_status::ready);
  ss::state_snapshot_t const now = got.get ();
  EXPECT_EQ (now.state, state::open);
  EXPECT_EQ (now.sequence, seen.sequence + 2);
}

// R2 (RT_B with a policy that allows one attempt in the window): the attempt
// fails with timeout at the open timeout, its transport is closed, and the
// late open does not happen.
TEST_F (
    LumexSessionRegressionTest,
    GivenAnOpenThatIgnoresCancel_WhenTheOpenTimeoutPasses_ThenItsTransportIsClosedAndNeverOpens)
{
  std::shared_ptr<deaf_record> const record = std::make_shared<deaf_record> ();
  ss::session_options_t options
      = recorded_options (recorder_, ms (2000), ms (2000));
  options.open_timeout = ms (50);
  ss::stream_session session (runner_.get_executor (),
                              recording_deaf_factory (ms (400), record),
                              options);
  session.start ();
  std::this_thread::sleep_for (ms (250));
  EXPECT_EQ (session.session_statistics ().failed_attempts, 1u);
  EXPECT_TRUE (session.last_error () == transport_errc::timeout);
  EXPECT_GE (record->closes.load (), 1);
  std::this_thread::sleep_for (ms (300));
  EXPECT_EQ (session.state (), state::connecting);
  EXPECT_EQ (record->late_opens.load (), 0)
      << "the transport opened after its attempt failed";
  session.close ();
}

// R3 (N2): a link that carried data and went before it was established is
// not a failed attempt: with max_attempts 1 the session reconnects (after
// the delay), it does not fault.
TEST_F (
    LumexSessionRegressionTest,
    GivenAWorkingLinkLostWithinInitialDelay_WhenMaxAttemptsIsOne_ThenTheSessionReconnectsInsteadOfFaulting)
{
  ss::session_options_t options
      = recorded_options (recorder_, ms (300), ms (300));
  options.policy.max_attempts = 1;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_), options));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  std::future<std::size_t> got = session->async_read_some (
      tr::mutable_bytes_t (buffer), asio::use_future);
  factory_->last ().feed (std::vector<std::uint8_t> (4, 7));
  ASSERT_EQ (got.get (), 4u);
  std::this_thread::sleep_for (ms (50));
  outcome<std::size_t> lost;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&lost] (std::error_code e, std::size_t n)
                              { lost.set (e, n); });
  factory_->last ().disconnect ();
  ASSERT_TRUE (lost.wait ());
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (session->state (), state::reconnecting);
  EXPECT_EQ (session->session_statistics ().failed_attempts, 0u);
  EXPECT_TRUE (eventually (
      [&session]
        {
          return session->session_statistics ().reconnects == 1
                 && session->state () == state::open;
        },
      ms (2000)));
  session->close ();
}

// N2: that reconnect waits the delay (no spin through a link that carries a
// little data and goes).
TEST_F (
    LumexSessionRegressionTest,
    GivenAWorkingLinkLostWithinInitialDelay_WhenItIsLost_ThenTheFirstReconnectWaitsTheDelay)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_, ms (300), ms (300))));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  std::future<std::size_t> got = session->async_read_some (
      tr::mutable_bytes_t (buffer), asio::use_future);
  factory_->last ().feed (std::vector<std::uint8_t> (4, 7));
  ASSERT_EQ (got.get (), 4u);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (20));
  clock_type::time_point const lost = clock_type::now ();
  factory_->last ().disconnect ();
  ASSERT_TRUE (
      eventually ([this] { return factory_->call_count () == 2; }, ms (3000)));
  EXPECT_GE (factory_->call_times ().back () - lost, ms (250));
  session->close ();
}

// R3b (N2, the documented rule): a link that carried no data and went before
// it was established is a failed attempt: the reconnect waits the delay.
TEST_F (
    LumexSessionRegressionTest,
    GivenASilentLinkLostWithinInitialDelay_WhenItIsLost_ThenItIsAFailedAttemptAndTheReconnectWaitsTheDelay)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_, ms (1000), ms (1000))));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::this_thread::sleep_for (ms (200));
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (20));
  clock_type::time_point const lost = clock_type::now ();
  factory_->last ().disconnect ();
  ASSERT_TRUE (
      eventually ([this] { return factory_->call_count () == 2; }, ms (3000)));
  EXPECT_GE (factory_->call_times ().back () - lost, ms (900));
  EXPECT_EQ (session->session_statistics ().failed_attempts, 1u);
  session->close ();
}

// R4 (U09): when the session drops a link with a read pending (the device is
// reported gone), the connection is closed: the peer sees its end.
TEST_F (
    LumexSessionRegressionTest,
    GivenAPendingTcpRead_WhenTheSessionDropsTheLink_ThenThePeerSeesTheConnectionClosed)
{
  typedef asio::ip::tcp tcp;
  asio::io_context peer_context;
  tcp::acceptor acceptor (
      peer_context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const port = acceptor.local_endpoint ().port ();
  std::promise<bool> eof;
  std::thread peer (
      [&acceptor, &peer_context, &eof] ()
        {
          tcp::socket accepted (peer_context);
          asio::error_code error;
          acceptor.accept (accepted, error);
          if (error)
            {
              eof.set_value (false);
              return;
            }
          std::uint8_t byte = 0;
          accepted.async_read_some (
              asio::buffer (&byte, 1),
              [&eof] (asio::error_code e, std::size_t)
                {
                  eof.set_value (e == asio::error::eof
                                 || e == asio::error::connection_reset);
                });
          peer_context.run_for (std::chrono::seconds (2));
          peer_context.stop ();
        });
  ss::net_selector_t selector;
  selector.host = "127.0.0.1";
  selector.port = port;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), ss::make_tcp_factory (selector),
      recorded_options (recorder_, ms (5000), ms (5000))));
  bool const opened = session->open (ms (5000)).has_value ();
  std::vector<std::uint8_t> buffer (4);
  if (opened)
    {
      session->async_read_some (tr::mutable_bytes_t (buffer),
                                [] (std::error_code, std::size_t) {});
      std::this_thread::sleep_for (ms (30));
      session->set_device_present (false);
    }
  std::future<bool> seen = eof.get_future ();
  bool const closed
      = seen.wait_for (std::chrono::seconds (3)) == std::future_status::ready
        && seen.get ();
  peer.join ();
  ASSERT_TRUE (opened);
  EXPECT_TRUE (closed) << "the dropped connection stayed open at the peer";
  session.reset ();
}

// R5 (point 5): the enumeration of a selector by USB identity runs on the
// open's worker thread: a slow one does not block the session's runner.
TEST_F (
    LumexSessionRegressionTest,
    GivenASlowEnumeration_WhenASerialSessionByIdentityOpens_ThenTheRunnerStaysFree)
{
  tr::stream::io_runner one (1);
  std::atomic<int> calls (0);
  ss::serial_selector_t selector;
  selector.vid = "16E0";
  selector.pid = "0001";
  selector.open_deadline = tr::deadline_t (200);
  ss::serial_enumeration_t const slow = [&calls] ()
    {
      ++calls;
      std::this_thread::sleep_for (ms (300));
      ss::serial_port_info_t port;
#if defined(_WIN32)
      port.path = "COM250";
#else
      port.path = "/dev/ttyLUMEXABSENT0";
#endif
      port.vid = "16E0";
      port.pid = "0001";
      return std::vector<ss::serial_port_info_t> (1, port);
    };
  ss::stream_session session (
      one.get_executor (), ss::make_serial_factory (selector, slow),
      recorded_options (recorder_, ms (5000), ms (5000)));
  session.start ();
  ASSERT_TRUE (eventually ([&calls] { return calls.load () == 1; }));
  clock_type::time_point const posted = clock_type::now ();
  std::promise<void> ran;
  asio::post (one.get_executor (), [&ran] () { ran.set_value (); });
  ASSERT_EQ (ran.get_future ().wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_LT (clock_type::now () - posted, ms (100))
      << "the enumeration ran on the runner's thread";
  ASSERT_TRUE (eventually (
      [&session]
        { return session.session_statistics ().failed_attempts == 1; }));
  session.close ();
}

// R5b: an enumeration that throws fails the attempt with invalid_endpoint.
TEST_F (
    LumexSessionRegressionTest,
    GivenAnEnumerationThatThrows_WhenASerialSessionByIdentityOpens_ThenTheAttemptFailsWithInvalidEndpoint)
{
  ss::serial_selector_t selector;
  selector.vid = "16E0";
  ss::serial_enumeration_t const broken
      = [] () -> std::vector<ss::serial_port_info_t>
    { throw std::runtime_error ("enumeration broke"); };
  ss::stream_session session (
      runner_.get_executor (), ss::make_serial_factory (selector, broken),
      recorded_options (recorder_, ms (5000), ms (5000)));
  session.start ();
  ASSERT_TRUE (eventually (
      [&session]
        { return session.session_statistics ().failed_attempts == 1; }));
  EXPECT_TRUE (session.last_error () == transport_errc::invalid_endpoint)
      << session.last_error ().message ();
  EXPECT_EQ (session.state (), state::connecting);
  session.close ();
}

namespace
{
// Runs a step on the session strand and waits for it: the steps posted
// before it ran (a wait from `closed` completes on the strand at once).
void
strand_round_trip (ss::stream_session &session)
{
  outcome<int> done;
  session.async_wait_state_change (
      state::closed, [&done] (std::error_code e, state) { done.set (e, 0); });
  done.wait ();
}
} // namespace

// R6 (N3): a removal reported while the open was in flight is not replayed
// by a later cancel (): the open shows the device is there, and cancel ()
// keeps an open link.
TEST_F (
    LumexSessionRegressionTest,
    GivenARemovalDuringTheOpen_WhenTheOpenSucceedsAndCancelIsCalledLater_ThenTheLinkStaysOpen)
{
  ss::stream_session session (
      runner_.get_executor (), make_scripted_factory (scripted_),
      recorded_options (recorder_, ms (5000), ms (5000)));
  session.start ();
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 1
                 && scripted_->at (0)->open_pending ();
        }));
  session.set_device_present (false);
  strand_round_trip (session);
  scripted_->at (0)->finish_open (std::error_code ());
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::open; }));
  session.cancel ();
  strand_round_trip (session);
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (session.state (), state::open);
  EXPECT_FALSE (scripted_->at (0)->closed ());
  EXPECT_EQ (scripted_->count (), 1u);
  session.close ();
}

// N3: after that open the device counts as present: when the link goes, the
// session reconnects instead of waiting for an arrival.
TEST_F (
    LumexSessionRegressionTest,
    GivenARemovalDuringTheOpen_WhenTheOpenedLinkIsLostLater_ThenTheSessionReconnects)
{
  ss::session_options_t options = recorded_options (recorder_);
  ss::stream_session session (runner_.get_executor (),
                              make_scripted_factory (scripted_), options);
  session.start ();
  ASSERT_TRUE (eventually (
      [this]
        {
          return scripted_->count () == 1
                 && scripted_->at (0)->open_pending ();
        }));
  session.set_device_present (false);
  strand_round_trip (session);
  scripted_->at (0)->finish_open (std::error_code ());
  ASSERT_TRUE (
      eventually ([&session] { return session.state () == state::open; }));
  std::this_thread::sleep_for (ms (20));
  scripted_->at (0)->fail_writes (
      make_error_code (transport_errc::disconnected));
  std::vector<std::uint8_t> const data (2, std::uint8_t (1));
  outcome<int> wrote;
  session.async_write_some (tr::const_bytes_t (data),
                            [&wrote] (std::error_code e, std::size_t n)
                              { wrote.set (e, static_cast<int> (n)); });
  ASSERT_TRUE (wrote.wait ());
  EXPECT_TRUE (eventually ([this] { return scripted_->count () == 2; }))
      << "the session waits for an arrival of a device it just opened";
  session.close ();
}

// R7: a session on a user's io_context: after close () the context runs out
// of work and run () returns.
TEST (LumexSessionRegressionWorkTest,
      GivenASessionOnAUserContextWithPendingWaits_WhenClosed_ThenRunReturns)
{
  asio::io_context context;
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  {
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory));
    bool opened = false;
    session.async_open ([&opened] (std::error_code e) { opened = !e; });
    std::vector<std::uint8_t> buffer (4);
    bool read_done = false;
    bool wait_done = false;
    asio::steady_timer closer (context);
    closer.expires_after (ms (100));
    closer.async_wait (
        [&] (std::error_code)
          {
            session.async_read_some (
                tr::mutable_bytes_t (buffer),
                [&read_done] (std::error_code, std::size_t)
                  { read_done = true; });
            session.async_wait_state_change (
                session.state_snapshot (),
                [&wait_done] (std::error_code, ss::state_snapshot_t)
                  { wait_done = true; });
            asio::post (context, [&session] () { session.close (); });
          });
    clock_type::time_point const start = clock_type::now ();
    context.run_for (std::chrono::seconds (3));
    EXPECT_TRUE (opened);
    EXPECT_TRUE (read_done);
    EXPECT_TRUE (wait_done);
    EXPECT_LT (clock_type::now () - start, ms (1000))
        << "run () kept going after the close";
  }
  factory->controls.clear ();
}

// H1 (N1): an open waiter whose handler owns its session (the usual
// "capture self"): the io_context's shutdown destroys the pending waiter,
// which breaks the cycle, and ~io_context returns. The test process exits
// when it hangs.
TEST (
    LumexSessionRegressionShutdownTest,
    GivenAnOpenWaiterThatOwnsItsSession_WhenTheRunnerStopsAndDies_ThenTheContextIsDestroyed)
{
  bool const ended = ends_within (
      [] ()
        {
          tr::stream::io_runner runner (2);
          std::shared_ptr<stream_factory_state> const factory
              = std::make_shared<stream_factory_state> ();
          factory->hold_call = 1;
          std::shared_ptr<owning_client> client
              = std::make_shared<owning_client> ();
          client->session.reset (new ss::stream_session (
              runner.get_executor (), make_stream_factory (factory)));
          client->session->async_open ([client] (std::error_code) {});
          client.reset ();
          std::this_thread::sleep_for (ms (100));
          runner.stop ();
          factory->controls.clear ();
        },
      ms (8000));
  if (!ended)
    {
      std::cout << "~io_runner (~io_context) still running after 8 s: hang"
                << std::endl;
      std::_Exit (3);
    }
}

// H2 (N1): the same with a state waiter.
TEST (
    LumexSessionRegressionShutdownTest,
    GivenAStateWaiterThatOwnsItsSession_WhenTheRunnerStopsAndDies_ThenTheContextIsDestroyed)
{
  bool const ended = ends_within (
      [] ()
        {
          tr::stream::io_runner runner (2);
          std::shared_ptr<stream_factory_state> const factory
              = std::make_shared<stream_factory_state> ();
          std::shared_ptr<owning_client> client
              = std::make_shared<owning_client> ();
          client->session.reset (new ss::stream_session (
              runner.get_executor (), make_stream_factory (factory)));
          client->session->open (ms (2000));
          client->session->async_wait_state_change (
              client->session->state_snapshot (),
              [client] (std::error_code, ss::state_snapshot_t) {});
          client.reset ();
          std::this_thread::sleep_for (ms (50));
          runner.stop ();
          factory->controls.clear ();
        },
      ms (8000));
  if (!ended)
    {
      std::cout << "~io_runner (~io_context) still running after 8 s: hang"
                << std::endl;
      std::_Exit (3);
    }
}

// N1: the same with a read pending on a mock stream (the mock keeps the
// session's handler): the read's handler owns the session.
TEST (
    LumexSessionRegressionShutdownTest,
    GivenAReadWhoseHandlerOwnsItsSession_WhenTheRunnerStopsAndDies_ThenTheContextIsDestroyed)
{
  bool const ended = ends_within (
      [] ()
        {
          tr::stream::io_runner runner (2);
          std::shared_ptr<stream_factory_state> const factory
              = std::make_shared<stream_factory_state> ();
          std::shared_ptr<owning_client> client
              = std::make_shared<owning_client> ();
          client->session.reset (new ss::stream_session (
              runner.get_executor (), make_stream_factory (factory)));
          client->session->open (ms (2000));
          std::shared_ptr<std::vector<std::uint8_t>> const buffer
              = std::make_shared<std::vector<std::uint8_t>> (4);
          client->session->async_read_some (
              tr::mutable_bytes_t (*buffer),
              [client, buffer] (std::error_code, std::size_t) {});
          client.reset ();
          std::this_thread::sleep_for (ms (50));
          runner.stop ();
          factory->controls.clear ();
        },
      ms (8000));
  if (!ended)
    {
      std::cout << "~io_runner (~io_context) still running after 8 s: hang"
                << std::endl;
      std::_Exit (3);
    }
}

// N3: a removal reported while a cancel () is on its way (the stop epoch
// bumped, its step not run yet) is replayed by the cancel's step: the link
// is dropped and the session waits for the device.
TEST_F (LumexSessionRegressionTest,
        GivenARemovalQueuedBehindACancel_WhenBothRun_ThenTheLinkIsDropped)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_),
      recorded_options (recorder_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::this_thread::sleep_for (ms (20));
  ss::stream_session *const raw = session.get ();
  outcome<int> waited;
  // On the session strand: the removal's step queues, then the cancel's.
  session->async_wait_state_change (
      state::closed,
      [&waited, raw] (std::error_code e, state now)
        {
          raw->set_device_present (false);
          raw->cancel ();
          waited.set (e, static_cast<int> (now));
        });
  ASSERT_TRUE (waited.wait ());
  EXPECT_TRUE (eventually (
      [&session] { return session->state () == state::reconnecting; }))
      << "the removal was lost behind the cancel: the link stays open";
  std::this_thread::sleep_for (ms (30));
  EXPECT_EQ (factory_->call_count (), 1u) << "no attempt while absent";
  session->close ();
}

// N2: an I/O that moved no byte (a write of nothing) is not data: a link that
// carried only that and went before it was established is a failed attempt.
TEST_F (
    LumexSessionRegressionTest,
    GivenALinkThatOnlyWroteNothing_WhenLostWithinInitialDelay_ThenItIsAFailedAttempt)
{
  ss::session_options_t options
      = recorded_options (recorder_, ms (500), ms (500));
  options.policy.max_attempts = 1;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_), options));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> const nothing;
  std::future<std::size_t> wrote = session->async_write_some (
      tr::const_bytes_t (nothing), asio::use_future);
  ASSERT_EQ (wrote.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  ASSERT_EQ (wrote.get (), 0u);
  std::vector<std::uint8_t> buffer (4);
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [] (std::error_code, std::size_t) {});
  std::this_thread::sleep_for (ms (20));
  factory_->last ().disconnect ();
  EXPECT_TRUE (
      eventually ([&session] { return session->state () == state::faulted; }))
      << "a write of nothing counted as data";
  EXPECT_EQ (session->session_statistics ().failed_attempts, 1u);
}

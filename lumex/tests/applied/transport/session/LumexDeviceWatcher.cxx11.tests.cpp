// device_watcher over a fake enumeration that changes between polls:
// arrivals and removals per selector (a re-enumeration under another name),
// presence, periodic polling, unwatch, stop, a failing enumeration, and a
// session that waits for its device through the watcher.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using kind = ss::device_event_kind;

// The ports the fake enumeration lists now (thread-safe).
struct fake_ports
{
  std::mutex mutex;
  std::vector<ss::serial_port_info_t> ports;
  std::size_t calls = 0;
  bool fail = false;

  void
  set (std::vector<ss::serial_port_info_t> const &now)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    ports = now;
  }

  void
  set_fail (bool value)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    fail = value;
  }

  std::size_t
  call_count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return calls;
  }
};

ss::serial_enumeration_t
enumeration_of (std::shared_ptr<fake_ports> const &fake)
{
  return [fake] ()
    {
      std::lock_guard<std::mutex> const lock (fake->mutex);
      ++fake->calls;
      if (fake->fail)
        throw std::runtime_error ("enumeration failed");
      return fake->ports;
    };
}

ss::serial_port_info_t
usb_port (std::string const &path, std::string const &pid)
{
  ss::serial_port_info_t port;
  port.path = path;
  port.vid = "16E0";
  port.pid = pid;
  return port;
}

ss::serial_selector_t
lumex_device ()
{
  ss::serial_selector_t selector;
  selector.vid = "16E0";
  selector.pid = "0001";
  return selector;
}

// Records the events of one watch.
struct event_log
{
  std::mutex mutex;
  std::vector<std::string> events;

  void
  add (ss::device_event_t const &event)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    events.push_back ((event.kind == kind::arrived ? "+" : "-")
                      + event.port.path);
  }

  std::vector<std::string>
  all ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return events;
  }

  std::size_t
  count ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return events.size ();
  }
};

struct presence_log
{
  std::mutex mutex;
  std::vector<bool> values;

  void
  add (bool present)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    values.push_back (present);
  }

  std::vector<bool>
  all ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return values;
  }
};

class LumexDeviceWatcherTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  std::shared_ptr<fake_ports> fake_ = std::make_shared<fake_ports> ();
  std::shared_ptr<event_log> log_ = std::make_shared<event_log> ();

  ss::device_callback_t
  recorder ()
  {
    std::shared_ptr<event_log> const log = log_;
    return [log] (ss::device_event_t const &event) { log->add (event); };
  }

  // A watcher that polls only when asked (a period far away), once its
  // first poll (at construction) is done: a later poll () then waits for a
  // poll that runs after the watches added before it.
  std::unique_ptr<ss::device_watcher>
  manual_watcher ()
  {
    std::unique_ptr<ss::device_watcher> watcher (new ss::device_watcher (
        runner_.get_executor (), ms (3600000), enumeration_of (fake_)));
    ss::device_watcher &ref = *watcher;
    EXPECT_TRUE (eventually ([&ref] { return ref.polls () == 1; }));
    return watcher;
  }

  void
  poll (ss::device_watcher &watcher)
  {
    std::uint64_t const before = watcher.polls ();
    watcher.poll_now ();
    ASSERT_TRUE (
        eventually ([&watcher, before] { return watcher.polls () > before; }));
  }
};
} // namespace

TEST_F (LumexDeviceWatcherTest,
        GivenAPresentDevice_WhenWatched_ThenItArrivesAtOnce)
{
  fake_->set ({ usb_port ("COM3", "0001"), usb_port ("COM4", "0002") });
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  ASSERT_TRUE (eventually ([&watcher] { return watcher->polls () == 1; }));
  watcher->watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+COM3" }));
}

TEST_F (
    LumexDeviceWatcherTest,
    GivenAWatchedDevice_WhenItGoesAndComesBackUnderAnotherName_ThenTheRemovalAndTheArrivalAreReported)
{
  fake_->set ({ usb_port ("COM3", "0001") });
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));

  fake_->set ({});
  poll (*watcher);
  fake_->set ({ usb_port ("COM9", "0001"), usb_port ("COM4", "0002") });
  poll (*watcher);
  poll (*watcher);
  EXPECT_EQ (log_->all (),
             std::vector<std::string> ({ "+COM3", "-COM3", "+COM9" }));
}

TEST_F (LumexDeviceWatcherTest,
        GivenASelectorByPath_WhenOtherPortsComeAndGo_ThenItHearsOnlyItsOwn)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  ss::serial_selector_t selector;
  selector.path = "COM5";
  watcher->watch (selector, recorder ());
  fake_->set ({ usb_port ("COM3", "0001") });
  poll (*watcher);
  fake_->set ({ usb_port ("COM3", "0001"), usb_port ("COM5", "0009") });
  poll (*watcher);
  fake_->set ({});
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+COM5", "-COM5" }));
}

TEST_F (
    LumexDeviceWatcherTest,
    GivenAPresenceWatch_WhenMatchingPortsComeAndGo_ThenOnlyTheChangesOfPresenceAreReported)
{
  std::shared_ptr<presence_log> const presence
      = std::make_shared<presence_log> ();
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  poll (*watcher);
  watcher->watch_presence (lumex_device (), [presence] (bool present)
                             { presence->add (present); });
  // The first evaluation reports absence too.
  ASSERT_TRUE (
      eventually ([&presence] { return presence->all ().size () == 1; }));
  fake_->set ({ usb_port ("COM3", "0001") });
  poll (*watcher);
  fake_->set ({ usb_port ("COM3", "0001"), usb_port ("COM7", "0001") });
  poll (*watcher);
  fake_->set ({ usb_port ("COM7", "0001") });
  poll (*watcher);
  fake_->set ({});
  poll (*watcher);
  EXPECT_EQ (presence->all (), std::vector<bool> ({ false, true, false }));
}

TEST_F (LumexDeviceWatcherTest,
        GivenAShortPeriod_WhenTimePasses_ThenTheWatcherPollsByItself)
{
  ss::device_watcher watcher (runner_.get_executor (), ms (20),
                              enumeration_of (fake_));
  watcher.watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return fake_->call_count () >= 3; }));
  fake_->set ({ usb_port ("COM3", "0001") });
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  fake_->set ({});
  ASSERT_TRUE (eventually ([this] { return log_->count () == 2; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+COM3", "-COM3" }));
}

TEST_F (LumexDeviceWatcherTest,
        GivenAWatch_WhenUnwatched_ThenItHearsNothingMore)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  ss::device_watcher::watch_id_t const id
      = watcher->watch (lumex_device (), recorder ());
  fake_->set ({ usb_port ("COM3", "0001") });
  poll (*watcher);
  ASSERT_EQ (log_->count (), 1u);
  watcher->unwatch (id);
  fake_->set ({});
  poll (*watcher);
  EXPECT_EQ (log_->count (), 1u);
}

TEST_F (LumexDeviceWatcherTest,
        GivenAWatcher_WhenDestroyed_ThenItStopsPollingAndCallsNothing)
{
  std::unique_ptr<ss::device_watcher> watcher (new ss::device_watcher (
      runner_.get_executor (), ms (5), enumeration_of (fake_)));
  watcher->watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return fake_->call_count () >= 2; }));
  watcher.reset ();
  std::this_thread::sleep_for (ms (20));
  std::size_t const calls = fake_->call_count ();
  fake_->set ({ usb_port ("COM3", "0001") });
  std::this_thread::sleep_for (ms (60));
  EXPECT_EQ (fake_->call_count (), calls);
  EXPECT_EQ (log_->count (), 0u);
}

TEST_F (
    LumexDeviceWatcherTest,
    GivenAnEnumerationThatFails_WhenPolled_ThenNothingIsReportedGoneAndTheNextPollWorks)
{
  fake_->set ({ usb_port ("COM3", "0001") });
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_EQ (log_->count (), 1u);
  fake_->set_fail (true);
  std::size_t const calls = fake_->call_count ();
  watcher->poll_now ();
  ASSERT_TRUE (
      eventually ([this, calls] { return fake_->call_count () > calls; }));
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (log_->count (), 1u);
  fake_->set_fail (false);
  fake_->set ({});
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+COM3", "-COM3" }));
}

TEST_F (LumexDeviceWatcherTest,
        GivenACallbackThatThrows_WhenAnEventComes_ThenTheWatcherGoesOn)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  std::shared_ptr<event_log> const log = log_;
  watcher->watch (lumex_device (),
                  [log] (ss::device_event_t const &event)
                    {
                      log->add (event);
                      throw std::runtime_error ("callback failure");
                    });
  fake_->set ({ usb_port ("COM3", "0001") });
  poll (*watcher);
  fake_->set ({});
  poll (*watcher);
  EXPECT_EQ (log_->count (), 2u);
}

TEST_F (
    LumexDeviceWatcherTest,
    GivenASessionFollowingTheWatcher_WhenItsDeviceComesGoesAndComesBack_ThenItOpensWaitsAndReconnects)
{
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  std::shared_ptr<state_recorder> const states
      = std::make_shared<state_recorder> ();
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory),
                              recorded_options (states));
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  poll (*watcher);
  // The sink posts to the session before it is recorded: the start below
  // comes after the absence on the session's strand.
  std::shared_ptr<presence_log> const presence
      = std::make_shared<presence_log> ();
  ss::presence_sink_t const sink = session.presence_sink ();
  watcher->watch_presence (lumex_device (),
                           [sink, presence] (bool present)
                             {
                               sink (present);
                               presence->add (present);
                             });
  ASSERT_TRUE (
      eventually ([&presence] { return presence->all ().size () == 1; }));
  session.start ();
  // Absent: the session waits without attempts.
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (factory->call_count (), 0u);
  EXPECT_EQ (session.state (), ss::channel_state::connecting);

  fake_->set ({ usb_port ("COM3", "0001") });
  poll (*watcher);
  ASSERT_TRUE (eventually (
      [&session] { return session.state () == ss::channel_state::open; }));

  fake_->set ({});
  poll (*watcher);
  ASSERT_TRUE (eventually (
      [&session]
        { return session.state () == ss::channel_state::reconnecting; }));
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (factory->call_count (), 1u);

  fake_->set ({ usb_port ("COM9", "0001") });
  poll (*watcher);
  ASSERT_TRUE (eventually (
      [&session] { return session.state () == ss::channel_state::open; }));
  EXPECT_EQ (factory->call_count (), 2u);
  EXPECT_EQ (session.session_statistics ().reconnects, 1u);
}

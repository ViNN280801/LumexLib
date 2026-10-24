// device_watcher over a fake HID enumeration that changes between polls:
// arrivals and removals per HID selector, the re-enumeration of a device under
// another path, presence, the interfaces of one device, periodic polling,
// unwatch, a failing HID enumeration (the serial watches go on), and the HID
// enumeration running only while a HID selector is watched. The devices are
// synthetic (no real product IDs).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFakeHidapi.hpp"
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"
#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace hid = tr::hid;
namespace ss = tr::session;
using lumex_fake_hidapi::fake_device;
using lumex_fake_hidapi::fake_seam;
typedef ss::device_event_kind kind;
typedef std::chrono::milliseconds ms;

std::uint16_t const KVENDOR = 0x1234;
std::uint16_t const KPRODUCT = 0x5678;

template <typename Predicate>
bool
eventually (Predicate predicate, ms limit = ms (5000))
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  while (!predicate ())
    {
      if (std::chrono::steady_clock::now () > end)
        return false;
      std::this_thread::sleep_for (ms (2));
    }
  return true;
}

struct event_log
{
  std::mutex mutex;
  std::vector<std::string> events;

  void
  add (ss::hid_device_event_t const &event)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    events.push_back ((event.kind == kind::arrived ? "+" : "-")
                      + event.device.path);
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

hid::hid_selector_t
lumex_device ()
{
  hid::hid_selector_t selector;
  selector.vendor_id = KVENDOR;
  selector.product_id = KPRODUCT;
  return selector;
}

class LumexHidWatcherTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  std::shared_ptr<fake_seam> seam_ = std::make_shared<fake_seam> ();
  std::shared_ptr<event_log> log_ = std::make_shared<event_log> ();
  std::mutex mutex_;
  std::size_t hid_calls_ = 0;
  std::size_t serial_calls_ = 0;
  bool hid_fails_ = false;
  bool serial_fails_ = false;
  std::vector<ss::serial_port_info_t> serial_ports_;

  std::shared_ptr<fake_device>
  plug (std::string const &path, std::uint16_t product = KPRODUCT,
        int interface_number = 0, std::uint16_t usage_page = 0)
  {
    std::shared_ptr<fake_device> const device
        = seam_->add (path, KVENDOR, product, "S1",
                      lumex_fake_hidapi::unnumbered_descriptor (8, 8));
    device->info.interface_number = interface_number;
    device->info.usage_page = usage_page;
    return device;
  }

  ss::hid_device_callback_t
  recorder ()
  {
    std::shared_ptr<event_log> const log = log_;
    return [log] (ss::hid_device_event_t const &event) { log->add (event); };
  }

  hid::hid_enumeration_t
  hid_enumeration ()
  {
    return [this] ()
      {
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          ++hid_calls_;
          if (hid_fails_)
            throw std::runtime_error ("hid enumeration failed");
        }
        return hid::detail::enumerate_hid_devices_with (*seam_,
                                                        hid::hid_filter_t ());
      };
  }

  ss::serial_enumeration_t
  serial_enumeration ()
  {
    return [this] ()
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        ++serial_calls_;
        if (serial_fails_)
          throw std::runtime_error ("serial enumeration failed");
        return serial_ports_;
      };
  }

  std::size_t
  hid_calls ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return hid_calls_;
  }

  std::size_t
  serial_calls ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return serial_calls_;
  }

  void
  set_serial_fails (bool value)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    serial_fails_ = value;
  }

  void
  set_hid_fails (bool value)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    hid_fails_ = value;
  }

  // A watcher that polls only when asked (a period far away), once its
  // first poll (at construction) is done.
  std::unique_ptr<ss::device_watcher>
  manual_watcher ()
  {
    std::unique_ptr<ss::device_watcher> watcher (
        new ss::device_watcher (runner_.get_executor (), ms (3600000),
                                serial_enumeration (), hid_enumeration ()));
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

TEST_F (LumexHidWatcherTest,
        GivenAPresentDevice_WhenWatched_ThenItArrivesAtOnceAndOthersDoNot)
{
  plug ("hid-a");
  plug ("hid-other", KPRODUCT + 1);
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a" }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenAWatchedDevice_WhenItGoesAndComesBackUnderAnotherPath_ThenTheRemovalAndTheArrivalAreReported)
{
  std::shared_ptr<fake_device> const device = plug ("hid-a");
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));

  seam_->remove (device);
  poll (*watcher);
  seam_->plug (device, "hid-b");
  poll (*watcher);
  poll (*watcher);
  EXPECT_EQ (log_->all (),
             std::vector<std::string> ({ "+hid-a", "-hid-a", "+hid-b" }));
}

TEST_F (LumexHidWatcherTest,
        GivenASelectorByPath_WhenOtherDevicesComeAndGo_ThenItHearsOnlyItsOwn)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  hid::hid_selector_t selector;
  selector.path = "hid-5";
  watcher->watch (selector, recorder ());
  plug ("hid-3");
  poll (*watcher);
  std::shared_ptr<fake_device> const five = plug ("hid-5", KPRODUCT + 9);
  poll (*watcher);
  seam_->remove (five);
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-5", "-hid-5" }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenTwoInterfacesOfOneDevice_WhenOneIsSelected_ThenTheOtherIsNotReported)
{
  plug ("hid-if0", KPRODUCT, 0, 0x0001);
  plug ("hid-if1", KPRODUCT, 1, 0xFF00);
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  hid::hid_selector_t selector = lumex_device ();
  selector.interface_number = 1;
  watcher->watch (selector, recorder ());
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-if1" }));

  std::shared_ptr<event_log> const by_page = std::make_shared<event_log> ();
  hid::hid_selector_t page = lumex_device ();
  page.usage_page = 0x0001;
  watcher->watch (page, [by_page] (ss::hid_device_event_t const &event)
                    { by_page->add (event); });
  ASSERT_TRUE (eventually ([&by_page] { return by_page->count () == 1; }));
  EXPECT_EQ (by_page->all (), std::vector<std::string> ({ "+hid-if0" }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenAPresenceWatch_WhenMatchingDevicesComeAndGo_ThenOnlyTheChangesOfPresenceAreReported)
{
  std::shared_ptr<presence_log> const presence
      = std::make_shared<presence_log> ();
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch_presence (lumex_device (), [presence] (bool present)
                             { presence->add (present); });
  // The first evaluation reports absence too.
  ASSERT_TRUE (
      eventually ([&presence] { return presence->all ().size () == 1; }));
  std::shared_ptr<fake_device> const first = plug ("hid-a");
  poll (*watcher);
  std::shared_ptr<fake_device> const second = plug ("hid-b");
  poll (*watcher);
  seam_->remove (first);
  poll (*watcher);
  seam_->remove (second);
  poll (*watcher);
  EXPECT_EQ (presence->all (), std::vector<bool> ({ false, true, false }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenAShortPeriod_WhenTimePasses_ThenTheWatcherPollsTheHidDevicesByItself)
{
  ss::device_watcher watcher (runner_.get_executor (), ms (20),
                              serial_enumeration (), hid_enumeration ());
  watcher.watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return hid_calls () >= 3; }));
  std::shared_ptr<fake_device> const device = plug ("hid-a");
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  seam_->remove (device);
  ASSERT_TRUE (eventually ([this] { return log_->count () == 2; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a", "-hid-a" }));
}

TEST_F (LumexHidWatcherTest,
        GivenNoHidWatch_WhenThePollsRun_ThenTheHidEnumerationIsNotCalled)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  poll (*watcher);
  poll (*watcher);
  EXPECT_EQ (hid_calls (), 0u);
}

TEST_F (
    LumexHidWatcherTest,
    GivenAHidWatch_WhenItIsRemoved_ThenThePollsStopEnumeratingTheHidDevices)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  ss::device_watcher::watch_id_t const id
      = watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_GE (hid_calls (), 1u);
  watcher->unwatch (id);
  poll (*watcher);
  std::size_t const calls = hid_calls ();
  poll (*watcher);
  poll (*watcher);
  EXPECT_EQ (hid_calls (), calls);
}

TEST_F (
    LumexHidWatcherTest,
    GivenAStaleSnapshot_WhenAHidWatchIsAddedAgain_ThenItSeesTheDevicesOfNow)
{
  std::shared_ptr<fake_device> const device = plug ("hid-a");
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  ss::device_watcher::watch_id_t const id
      = watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_EQ (log_->count (), 1u);
  watcher->unwatch (id);
  poll (*watcher);
  // Unplugged while nobody watched: a new watch must not hear the old
  // snapshot's arrival.
  seam_->remove (device);
  std::shared_ptr<event_log> const second = std::make_shared<event_log> ();
  watcher->watch (lumex_device (), [second] (ss::hid_device_event_t const &e)
                    { second->add (e); });
  poll (*watcher);
  EXPECT_EQ (second->count (), 0u);
}

TEST_F (
    LumexHidWatcherTest,
    GivenTheHidEnumerationFails_WhenPolled_ThenNothingIsReportedGoneAndTheNextPollWorks)
{
  plug ("hid-a");
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_EQ (log_->count (), 1u);
  set_hid_fails (true);
  std::size_t const calls = hid_calls ();
  watcher->poll_now ();
  ASSERT_TRUE (eventually ([this, calls] { return hid_calls () > calls; }));
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (log_->count (), 1u);
  set_hid_fails (false);
  plug ("hid-b");
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a", "+hid-b" }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenTheHidEnumerationFails_WhenASerialWatchIsPolled_ThenTheSerialWatchStillWorks)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  std::shared_ptr<std::vector<std::string>> const serial_events
      = std::make_shared<std::vector<std::string>> ();
  std::mutex serial_mutex;
  ss::serial_selector_t port;
  port.path = "COM3";
  watcher->watch (
      port,
      [&serial_mutex, serial_events] (ss::device_event_t const &event)
        {
          std::lock_guard<std::mutex> const lock (serial_mutex);
          serial_events->push_back (event.port.path);
        });
  watcher->watch (lumex_device (), recorder ());
  set_hid_fails (true);
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    ss::serial_port_info_t info;
    info.path = "COM3";
    serial_ports_.push_back (info);
  }
  poll (*watcher);
  ASSERT_TRUE (eventually (
      [&serial_mutex, &serial_events]
        {
          std::lock_guard<std::mutex> const lock (serial_mutex);
          return serial_events->size () == 1;
        }));
  EXPECT_EQ (log_->count (), 0u);
}

TEST_F (LumexHidWatcherTest,
        GivenSerialAndHidWatches_WhenDevicesComeAndGo_ThenEachHearsItsOwnKind)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  std::mutex serial_mutex;
  std::shared_ptr<std::vector<std::string>> const serial_events
      = std::make_shared<std::vector<std::string>> ();
  ss::serial_selector_t port;
  port.path = "hid-a";
  watcher->watch (
      port,
      [&serial_mutex, serial_events] (ss::device_event_t const &event)
        {
          std::lock_guard<std::mutex> const lock (serial_mutex);
          serial_events->push_back (event.port.path);
        });
  watcher->watch (lumex_device (), recorder ());
  plug ("hid-a");
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a" }));
  std::lock_guard<std::mutex> const lock (serial_mutex);
  EXPECT_TRUE (serial_events->empty ());
}

TEST_F (LumexHidWatcherTest,
        GivenACallbackThatThrows_WhenAnEventComes_ThenTheWatcherGoesOn)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  std::shared_ptr<event_log> const log = log_;
  watcher->watch (lumex_device (),
                  [log] (ss::hid_device_event_t const &event)
                    {
                      log->add (event);
                      throw std::runtime_error ("callback failure");
                    });
  std::shared_ptr<fake_device> const device = plug ("hid-a");
  poll (*watcher);
  seam_->remove (device);
  poll (*watcher);
  EXPECT_EQ (log_->count (), 2u);
}

TEST_F (LumexHidWatcherTest,
        GivenAWatcher_WhenDestroyed_ThenItStopsPollingAndCallsNothing)
{
  std::unique_ptr<ss::device_watcher> watcher (
      new ss::device_watcher (runner_.get_executor (), ms (5),
                              serial_enumeration (), hid_enumeration ()));
  watcher->watch (lumex_device (), recorder ());
  ASSERT_TRUE (eventually ([this] { return hid_calls () >= 2; }));
  watcher.reset ();
  std::this_thread::sleep_for (ms (20));
  std::size_t const calls = hid_calls ();
  plug ("hid-a");
  std::this_thread::sleep_for (ms (60));
  EXPECT_EQ (hid_calls (), calls);
  EXPECT_EQ (log_->count (), 0u);
}

TEST_F (LumexHidWatcherTest,
        GivenTwoRecordsOnOnePath_WhenWatched_ThenThePathArrivesAndGoesOnce)
{
  // hidapi lists one record per top-level usage, all with the device's path.
  std::shared_ptr<fake_device> const first = plug ("hid-a");
  std::shared_ptr<fake_device> const second = plug ("hid-a");
  first->info.usage = 1;
  second->info.usage = 2;
  std::shared_ptr<presence_log> const presence
      = std::make_shared<presence_log> ();
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  watcher->watch_presence (lumex_device (), [presence] (bool present)
                             { presence->add (present); });
  poll (*watcher);
  ASSERT_TRUE (eventually ([this] { return log_->count () >= 1; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a" }));
  seam_->remove (first);
  poll (*watcher);
  // One record is left on the path: the device is still there.
  EXPECT_EQ (log_->count (), 1u);
  seam_->remove (second);
  poll (*watcher);
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a", "-hid-a" }));
  EXPECT_EQ (presence->all (), std::vector<bool> ({ true, false }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenTwoRecordsOnOnePath_WhenAUsageIsSelected_ThenThatRecordStandsForThePath)
{
  std::shared_ptr<fake_device> const first = plug ("hid-a");
  std::shared_ptr<fake_device> const second = plug ("hid-a");
  first->info.usage = 1;
  second->info.usage = 2;
  std::mutex mutex;
  std::vector<unsigned> usages;
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  hid::hid_selector_t selector = lumex_device ();
  selector.usage = 2;
  watcher->watch (selector,
                  [&mutex, &usages] (ss::hid_device_event_t const &e)
                    {
                      std::lock_guard<std::mutex> const lock (mutex);
                      usages.push_back (e.device.usage);
                    });
  poll (*watcher);
  std::lock_guard<std::mutex> const lock (mutex);
  EXPECT_EQ (usages, std::vector<unsigned> ({ 2u }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenOneDeviceGoingAndAnotherComingInOnePoll_WhenPolled_ThenTheRemovalComesBeforeTheArrival)
{
  std::shared_ptr<fake_device> const gone = plug ("hid-a");
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  ASSERT_EQ (log_->count (), 1u);
  seam_->remove (gone);
  plug ("hid-b");
  poll (*watcher);
  EXPECT_EQ (log_->all (),
             std::vector<std::string> ({ "+hid-a", "-hid-a", "+hid-b" }));
}

TEST_F (
    LumexHidWatcherTest,
    GivenTheSerialListingFails_WhenAHidDeviceComes_ThenItIsReportedAndThePollIsCounted)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  // The watch is in place (added on the strand) before the device comes.
  poll (*watcher);
  set_serial_fails (true);
  plug ("hid-a");
  // poll () waits for polls () to grow: a poll with one listing counts.
  poll (*watcher);
  ASSERT_TRUE (eventually ([this] { return log_->count () == 1; }));
  EXPECT_EQ (log_->all (), std::vector<std::string> ({ "+hid-a" }));
}

TEST_F (LumexHidWatcherTest,
        GivenEveryListingFails_WhenPolled_ThenThePollIsNotCounted)
{
  std::unique_ptr<ss::device_watcher> watcher = manual_watcher ();
  watcher->watch (lumex_device (), recorder ());
  poll (*watcher);
  set_serial_fails (true);
  set_hid_fails (true);
  std::uint64_t const polls = watcher->polls ();
  std::size_t const serial = serial_calls ();
  std::size_t const hid = hid_calls ();
  watcher->poll_now ();
  ASSERT_TRUE (eventually (
      [this, serial, hid]
        { return serial_calls () > serial && hid_calls () > hid; }));
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (watcher->polls (), polls);
}

// make_hid_factory: the device is found at each open (by VID/PID, serial
// number, interface and usage page; by path without an enumeration), on the
// open's worker thread and not when the factory runs; invalid_endpoint when
// no device matches; the errors of the enumeration; and a message_session
// that reconnects to a device that came back under another path or that
// arrived after the session started. The devices, descriptors and reports
// are synthetic.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFakeHidapi.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"
#include "lumex/applied/transport/session/LumexMessageSession.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace hid = tr::hid;
namespace ss = tr::session;
using lumex_fake_hidapi::fake_device;
using lumex_fake_hidapi::fake_seam;
using tr::deadline_t;
using tr::error::transport_errc;
using ms = std::chrono::milliseconds;

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

std::size_t
opens_of (std::shared_ptr<fake_device> const &device)
{
  std::lock_guard<std::mutex> const lock (device->mutex);
  return device->opens;
}

hid::hid_selector_t
lumex_device ()
{
  hid::hid_selector_t selector;
  selector.vendor_id = KVENDOR;
  selector.product_id = KPRODUCT;
  return selector;
}

class LumexHidFactoryTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    // Every test leaves the runtime as it found it.
    EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (5000)));
    EXPECT_TRUE (eventually (
        []
          {
            hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
            return stats.reader_threads == 0 && stats.pending_tasks == 0
                   && stats.open_devices == 0;
          }));
  }

  std::shared_ptr<fake_device>
  plug (std::string const &path, std::uint16_t product = KPRODUCT,
        int interface_number = 0, std::uint16_t usage_page = 0,
        std::uint16_t usage = 0)
  {
    return seam_->add (path, KVENDOR, product, "S1",
                       lumex_fake_hidapi::unnumbered_descriptor (8, 8),
                       interface_number, usage_page, usage);
  }

  // The enumeration over the fake seam, counting its calls and noting the
  // threads they ran on.
  hid::hid_enumeration_t
  enumeration ()
  {
    return [this] ()
      {
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          ++calls_;
          threads_.push_back (std::this_thread::get_id ());
        }
        return hid::detail::enumerate_hid_devices_with (*seam_,
                                                        hid::hid_filter_t ());
      };
  }

  std::size_t
  calls ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return calls_;
  }

  std::vector<std::thread::id>
  threads ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return threads_;
  }

  ss::message_session::factory_t
  factory (hid::hid_selector_t const &selector)
  {
    return hid::detail::make_hid_factory_with (seam_, selector,
                                               enumeration ());
  }

  ss::session_options_t
  fast_options ()
  {
    ss::session_options_t options;
    options.name = "hid-test";
    options.policy.initial_delay = ms (5);
    options.policy.max_delay = ms (20);
    options.policy.multiplier = 2.0;
    options.policy.jitter = 0.0;
    return options;
  }

  tr::stream::io_runner runner_{ 2 };
  std::shared_ptr<fake_seam> seam_ = std::make_shared<fake_seam> ();
  std::mutex mutex_;
  std::size_t calls_ = 0;
  std::vector<std::thread::id> threads_;
};
} // namespace

TEST_F (
    LumexHidFactoryTest,
    GivenSeveralDevices_WhenTheFactoryDeviceOpens_ThenTheSelectedOneIsOpened)
{
  std::shared_ptr<fake_device> const other = plug ("hid-other", KPRODUCT + 1);
  std::shared_ptr<fake_device> const wanted = plug ("hid-wanted");
  hid::hid_device device = factory (lumex_device ()) (runner_.get_executor ());
  ASSERT_TRUE (device.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (opens_of (wanted), 1u);
  EXPECT_EQ (opens_of (other), 0u);
}

TEST_F (
    LumexHidFactoryTest,
    GivenNoMatchingDevice_WhenTheFactoryDeviceOpens_ThenInvalidEndpointIsReported)
{
  plug ("hid-other", KPRODUCT + 1);
  hid::hid_device device = factory (lumex_device ()) (runner_.get_executor ());
  tr::status_result_t const opened = device.open (deadline_t (2000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error () == transport_errc::invalid_endpoint);
}

TEST_F (LumexHidFactoryTest,
        GivenASelectorByPath_WhenTheFactoryDeviceOpens_ThenNothingIsEnumerated)
{
  std::shared_ptr<fake_device> const wanted = plug ("hid-wanted");
  hid::hid_selector_t selector;
  selector.path = "hid-wanted";
  hid::hid_device device = factory (selector) (runner_.get_executor ());
  ASSERT_TRUE (device.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (opens_of (wanted), 1u);
  EXPECT_EQ (calls (), 0u);
}

TEST_F (
    LumexHidFactoryTest,
    GivenASelectorByPath_WhenThePathIsGone_ThenTheOpenFailsWithoutFallingBackToAnotherDevice)
{
  plug ("hid-other");
  hid::hid_selector_t selector;
  selector.path = "hid-gone";
  hid::hid_device device = factory (selector) (runner_.get_executor ());
  EXPECT_FALSE (device.open (deadline_t (2000)).has_value ());
}

TEST_F (
    LumexHidFactoryTest,
    GivenTheFactory_WhenItRunsAndWhenTheDeviceOpens_ThenOnlyTheOpenEnumeratesAndOffTheCallingThread)
{
  plug ("hid-wanted");
  ss::message_session::factory_t const make = factory (lumex_device ());
  hid::hid_device device = make (runner_.get_executor ());
  EXPECT_EQ (calls (), 0u);
  ASSERT_TRUE (device.open (deadline_t (2000)).has_value ());
  ASSERT_EQ (calls (), 1u);
  EXPECT_NE (threads ()[0], std::this_thread::get_id ());
}

TEST_F (LumexHidFactoryTest,
        GivenTheFactory_WhenItMakesTwoDevices_ThenEachOpenEnumeratesAgain)
{
  plug ("hid-wanted");
  ss::message_session::factory_t const make = factory (lumex_device ());
  hid::hid_device first = make (runner_.get_executor ());
  ASSERT_TRUE (first.open (deadline_t (2000)).has_value ());
  first.close ();
  hid::hid_device second = make (runner_.get_executor ());
  ASSERT_TRUE (second.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (calls (), 2u);
}

TEST_F (
    LumexHidFactoryTest,
    GivenTwoInterfacesOfOneDevice_WhenTheSelectorNamesOne_ThenThatInterfaceIsOpened)
{
  std::shared_ptr<fake_device> const keyboard
      = plug ("hid-if0", KPRODUCT, 0, 0x0001);
  std::shared_ptr<fake_device> const vendor
      = plug ("hid-if1", KPRODUCT, 1, 0xFF00);
  hid::hid_selector_t selector = lumex_device ();
  selector.usage_page = 0xFF00;
  hid::hid_device device = factory (selector) (runner_.get_executor ());
  ASSERT_TRUE (device.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (opens_of (vendor), 1u);
  EXPECT_EQ (opens_of (keyboard), 0u);
  device.close ();

  selector.usage_page = 0;
  selector.interface_number = 0;
  hid::hid_device second = factory (selector) (runner_.get_executor ());
  ASSERT_TRUE (second.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (opens_of (keyboard), 1u);
}

TEST_F (
    LumexHidFactoryTest,
    GivenAnEnumerationThatThrows_WhenTheDeviceOpens_ThenInvalidEndpointIsReported)
{
  hid::hid_enumeration_t const failing = []
    {
      throw std::runtime_error ("enumeration failed");
      return std::vector<hid::hid_device_info_t> ();
    };
  hid::hid_device device = hid::detail::make_hid_factory_with (
      seam_, lumex_device (), failing) (runner_.get_executor ());
  tr::status_result_t const opened = device.open (deadline_t (2000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error () == transport_errc::invalid_endpoint);
}

TEST_F (
    LumexHidFactoryTest,
    GivenAnEnumerationThatThrowsASystemError_WhenTheDeviceOpens_ThenItsCodeIsReported)
{
  hid::hid_enumeration_t const failing = []
    {
      throw std::system_error (std::make_error_code (std::errc::io_error));
      return std::vector<hid::hid_device_info_t> ();
    };
  hid::hid_device device = hid::detail::make_hid_factory_with (
      seam_, lumex_device (), failing) (runner_.get_executor ());
  tr::status_result_t const opened = device.open (deadline_t (2000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_EQ (opened.error (), std::make_error_code (std::errc::io_error));
}

TEST_F (
    LumexHidFactoryTest,
    GivenASessionOverASelector_WhenTheDeviceComesBackUnderAnotherPath_ThenTheSessionReconnectsToIt)
{
  std::shared_ptr<fake_device> const device = plug ("hid-before");
  ss::message_session session (runner_.get_executor (),
                               factory (lumex_device ()), fast_options ());
  ASSERT_TRUE (session.open (ms (3000)).has_value ());
  EXPECT_EQ (opens_of (device), 1u);

  // A receive is pending: it ends with the link.
  std::promise<std::error_code> ended;
  session.async_receive (
      [&ended] (std::error_code error, tr::datagram::message_t)
        { ended.set_value (error); });
  // Unplugged: the enumeration forgets it and its reads fail.
  seam_->remove (device);
  device->fail_read ();
  std::future<std::error_code> link = ended.get_future ();
  ASSERT_EQ (link.wait_for (ms (3000)), std::future_status::ready);
  EXPECT_TRUE (link.get () == transport_errc::disconnected);
  ASSERT_TRUE (eventually (
      [&session] { return session.state () != ss::channel_state::open; }));
  // Plugged in again, under another path: the selector finds it.
  seam_->plug (device, "hid-after");
  ASSERT_TRUE (eventually (
      [&session, &device]
        {
          return session.state () == ss::channel_state::open
                 && opens_of (device) == 2u;
        }));

  tr::datagram::message_t message;
  message.report_id = 0;
  message.payload = { 1, 2, 3 };
  ASSERT_NO_THROW (session.async_send (message, asio::use_future).get ());
  EXPECT_EQ (device->written ().size (), 1u);
  session.close ();
}

TEST_F (
    LumexHidFactoryTest,
    GivenASessionStartedWithoutTheDevice_WhenTheDeviceArrives_ThenTheSessionOpens)
{
  ss::message_session session (runner_.get_executor (),
                               factory (lumex_device ()), fast_options ());
  tr::status_result_t const early = session.open (ms (60));
  EXPECT_FALSE (early.has_value ());
  EXPECT_NE (session.state (), ss::channel_state::open);
  // A slow machine may not have finished the first attempt in 60 ms.
  EXPECT_TRUE (eventually (
      [&session]
        { return session.session_statistics ().failed_attempts >= 1u; }));

  std::shared_ptr<fake_device> const device = plug ("hid-late");
  ASSERT_TRUE (eventually (
      [&session] { return session.state () == ss::channel_state::open; }));
  EXPECT_EQ (opens_of (device), 1u);
  session.close ();
}

TEST_F (
    LumexHidFactoryTest,
    GivenASelector_WhenTheFactoryMakesADevice_ThenItIsNamedAfterTheSelector)
{
  hid::hid_selector_t selector = lumex_device ();
  hid::hid_device const named = factory (selector) (runner_.get_executor ());
  EXPECT_EQ (named.name (), "HID VID 1234 PID 5678");
  selector.options.name = "my device";
  hid::hid_device const given = factory (selector) (runner_.get_executor ());
  EXPECT_EQ (given.name (), "my device");
}

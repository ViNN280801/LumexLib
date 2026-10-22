// The selectors: the USB identity of a port (Linux fields, Windows hardware
// and instance IDs), matching by path and by identity, the factories they
// make (serial over a fake enumeration, TCP, UDP), and a session whose serial
// selector names no port.

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;

ss::serial_port_info_t
linux_port (std::string const &path, std::string const &vid,
            std::string const &pid, std::string const &serial)
{
  ss::serial_port_info_t port;
  port.path = path;
  port.vid = vid;
  port.pid = pid;
  port.serial_number = serial;
  return port;
}

ss::serial_port_info_t
windows_port (std::string const &path, std::string const &hardware_id,
              std::string const &instance_id)
{
  ss::serial_port_info_t port;
  port.path = path;
  port.hardware_id = hardware_id;
  port.instance_id = instance_id;
  return port;
}

ss::serial_selector_t
by_usb (std::string const &vid, std::string const &pid,
        std::string const &serial)
{
  ss::serial_selector_t selector;
  selector.vid = vid;
  selector.pid = pid;
  selector.serial_number = serial;
  return selector;
}

ss::serial_selector_t
by_path (std::string const &path)
{
  ss::serial_selector_t selector;
  selector.path = path;
  return selector;
}
} // namespace

TEST (LumexSessionSelectorsTest,
      GivenALinuxPort_WhenItsIdentityIsRead_ThenItsOwnFieldsAreUsed)
{
  ss::usb_identity_t const identity
      = ss::usb_identity (linux_port ("/dev/ttyACM0", "16e0", "1", "SN-7"));
  EXPECT_EQ (identity.vid, "16E0");
  EXPECT_EQ (identity.pid, "0001");
  EXPECT_EQ (identity.serial_number, "SN-7");
}

TEST (LumexSessionSelectorsTest,
      GivenAWindowsUsbPort_WhenItsIdentityIsRead_ThenTheIdsAreParsed)
{
  ss::usb_identity_t const identity = ss::usb_identity (
      windows_port ("\\\\.\\COM7", "USB\\VID_16e0&PID_0A2B&REV_0100",
                    "USB\\VID_16E0&PID_0A2B\\LX123456"));
  EXPECT_EQ (identity.vid, "16E0");
  EXPECT_EQ (identity.pid, "0A2B");
  EXPECT_EQ (identity.serial_number, "LX123456");
}

TEST (
    LumexSessionSelectorsTest,
    GivenACompositeOrUnnamedWindowsPort_WhenItsIdentityIsRead_ThenItHasNoSerial)
{
  ss::usb_identity_t const composite = ss::usb_identity (
      windows_port ("COM8", "USB\\VID_16E0&PID_0001&MI_00",
                    "USB\\VID_16E0&PID_0001&MI_00\\6&2A3B4C5D&0&0000"));
  EXPECT_EQ (composite.vid, "16E0");
  EXPECT_EQ (composite.pid, "0001");
  EXPECT_EQ (composite.serial_number, "");
  ss::usb_identity_t const generated = ss::usb_identity (
      windows_port ("COM9", "", "USB\\VID_16E0&PID_0001\\5&1A2B3C4D&0&2"));
  EXPECT_EQ (generated.vid, "16E0");
  EXPECT_EQ (generated.serial_number, "");
  ss::usb_identity_t const not_usb = ss::usb_identity (
      windows_port ("COM1", "ACPI\\PNP0501", "ACPI\\PNP0501\\0"));
  EXPECT_EQ (not_usb.vid, "");
  EXPECT_EQ (not_usb.pid, "");
  EXPECT_EQ (not_usb.serial_number, "");
}

TEST (LumexSessionSelectorsTest,
      GivenAnFtdiBusPort_WhenItsIdentityIsRead_ThenTheSerialIsTheThirdField)
{
  ss::usb_identity_t const identity = ss::usb_identity (
      windows_port ("COM4", "FTDIBUS\\COMPORT&VID_0403&PID_6001",
                    "FTDIBUS\\VID_0403+PID_6001+B71946KXA\\0000"));
  EXPECT_EQ (identity.vid, "0403");
  EXPECT_EQ (identity.pid, "6001");
  EXPECT_EQ (identity.serial_number, "B71946KXA");
}

TEST (LumexSessionSelectorsTest,
      GivenSelectors_WhenMatchedAgainstPorts_ThenEachGivenFieldMustMatch)
{
  ss::serial_port_info_t const port = windows_port (
      "\\\\.\\COM7", "USB\\VID_16E0&PID_0A2B", "USB\\VID_16E0&PID_0A2B\\LX1");
  EXPECT_TRUE (ss::matches (by_path ("com7"), port));
  EXPECT_TRUE (ss::matches (by_path ("\\\\.\\COM7"), port));
  EXPECT_FALSE (ss::matches (by_path ("COM70"), port));
  EXPECT_TRUE (ss::matches (by_usb ("16e0", "", ""), port));
  EXPECT_TRUE (ss::matches (by_usb ("0x16E0", "0a2b", ""), port));
  EXPECT_TRUE (ss::matches (by_usb ("16E0", "0A2B", "lx1"), port));
  EXPECT_FALSE (ss::matches (by_usb ("16E0", "0A2C", ""), port));
  EXPECT_FALSE (ss::matches (by_usb ("16E0", "", "LX2"), port));
  EXPECT_FALSE (ss::matches (by_usb ("", "", ""), port));
  // A path wins over the identity.
  ss::serial_selector_t both = by_usb ("16E0", "", "");
  both.path = "COM3";
  EXPECT_FALSE (ss::matches (both, port));
  EXPECT_TRUE (ss::matches (by_path ("/dev/ttyUSB0"),
                            linux_port ("/dev/ttyUSB0", "", "", "")));
  EXPECT_TRUE (ss::matches (by_path ("ttyUSB0"),
                            linux_port ("/dev/ttyUSB0", "", "", "")));
}

TEST (LumexSessionSelectorsTest,
      GivenPorts_WhenTheSelectorIsLookedUp_ThenTheFirstMatchIsFound)
{
  std::vector<ss::serial_port_info_t> const ports
      = { linux_port ("/dev/ttyS0", "", "", ""),
          linux_port ("/dev/ttyACM0", "16E0", "0001", "A"),
          linux_port ("/dev/ttyACM1", "16E0", "0001", "B") };
  EXPECT_EQ (ss::find_serial_port (by_usb ("16E0", "0001", ""), ports),
             "/dev/ttyACM0");
  EXPECT_EQ (ss::find_serial_port (by_usb ("16E0", "0001", "B"), ports),
             "/dev/ttyACM1");
  EXPECT_EQ (ss::find_serial_port (by_usb ("1234", "", ""), ports), "");
  EXPECT_EQ (ss::find_serial_port (by_path ("COM5"), ports), "COM5");
  EXPECT_EQ (ss::describe (by_usb ("16e0", "1", "SN")),
             "USB VID 16E0 PID 0001 SN SN");
  EXPECT_EQ (ss::describe (by_path ("COM5")), "COM5");
}

// REVIEW13 point 5: the factory by USB identity does not enumerate (a
// session runs its factory on its strand at every attempt); the stream it
// makes finds its port at each open, on the open's worker thread.
TEST (
    LumexSessionSelectorsTest,
    GivenASerialSelectorByIdentity_WhenTheFactoryRuns_ThenItDoesNotEnumerateAndTheOpenFindsThePort)
{
  tr::stream::io_runner runner;
  std::shared_ptr<std::mutex> const mutex = std::make_shared<std::mutex> ();
  std::shared_ptr<std::vector<ss::serial_port_info_t>> const ports
      = std::make_shared<std::vector<ss::serial_port_info_t>> ();
  std::shared_ptr<std::vector<std::thread::id>> const threads
      = std::make_shared<std::vector<std::thread::id>> ();
  ss::serial_enumeration_t const fake = [mutex, ports, threads] ()
    {
      std::lock_guard<std::mutex> const lock (*mutex);
      threads->push_back (std::this_thread::get_id ());
      return *ports;
    };
  ss::stream_session::factory_t const factory
      = ss::make_serial_factory (by_usb ("16E0", "", ""), fake);
  tr::stream::byte_stream none = factory (runner.get_executor ());
  {
    std::lock_guard<std::mutex> const lock (*mutex);
    EXPECT_TRUE (threads->empty ()) << "the factory enumerated";
  }
  EXPECT_EQ (none.name (), "USB VID 16E0");
  std::future<void> failed = none.async_open (asio::use_future);
  try
    {
      failed.get ();
      ADD_FAILURE () << "no port matches: the open must fail";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::invalid_endpoint);
    }
  {
    std::lock_guard<std::mutex> const lock (*mutex);
    ASSERT_EQ (threads->size (), 1u);
    EXPECT_NE ((*threads)[0], std::this_thread::get_id ());
#if defined(_WIN32)
    ports->push_back (
        windows_port ("\\\\.\\COM251", "USB\\VID_16E0&PID_0001", ""));
#else
    ports->push_back (linux_port ("/dev/ttyLUMEXABSENT0", "16E0", "0001", ""));
#endif
  }
  // A matching port that is not there: the open tries it and fails with the
  // system's error, not with invalid_endpoint.
  tr::stream::byte_stream absent = factory (runner.get_executor ());
  std::future<void> refused = absent.async_open (asio::use_future);
  try
    {
      refused.get ();
      ADD_FAILURE () << "the port is not there: the open must fail";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_FALSE (failure.code () == transport_errc::invalid_endpoint)
          << failure.code ().message ();
    }
  {
    std::lock_guard<std::mutex> const lock (*mutex);
    EXPECT_EQ (threads->size (), 2u);
  }

  // By path: no enumeration.
  ss::stream_session::factory_t const by_name
      = ss::make_serial_factory (by_path ("COM43"), fake);
  EXPECT_EQ (by_name (runner.get_executor ()).name (), "COM43");
  std::lock_guard<std::mutex> const lock (*mutex);
  EXPECT_EQ (threads->size (), 2u);
}

// REVIEW13 point 5: the default enumeration of the selectors and of the
// watcher lists the ports without opening them.
TEST (LumexSessionSelectorsTest,
      GivenTheDefaultEnumeration_WhenItRuns_ThenItListsThePortsWithoutProbing)
{
  std::vector<ss::serial_port_info_t> const ports
      = ss::default_serial_enumeration () ();
  std::vector<ss::serial_port_info_t> const listed = lumex::applied::serial::
      enumeration::enumerate_serial_ports_without_probe (false, true);
  ASSERT_EQ (ports.size (), listed.size ());
  for (std::size_t i = 0; i < ports.size (); ++i)
    {
      EXPECT_EQ (ports[i].path, listed[i].path);
      EXPECT_TRUE (ports[i].system_error.empty ()) << ports[i].path;
      EXPECT_TRUE (ports[i].holder_process_info.empty ()) << ports[i].path;
    }
}

TEST (
    LumexSessionSelectorsTest,
    GivenNetSelectors_WhenTheFactoriesRun_ThenTheyMakeTransportsToTheSelectedPeer)
{
  tr::stream::io_runner runner;
  ss::net_selector_t selector;
  selector.host = "127.0.0.1";
  selector.port = 5000;
  tr::net::tcp_options_t tcp;
  tcp.host = "ignored";
  tcp.port = 1;
  EXPECT_EQ (
      ss::make_tcp_factory (selector, tcp) (runner.get_executor ()).name (),
      "127.0.0.1:5000");
  EXPECT_EQ (ss::make_udp_factory (selector) (runner.get_executor ()).name (),
             "127.0.0.1:5000");
}

TEST (
    LumexSessionSelectorsTest,
    GivenASessionWhoseSelectorNamesNoPort_WhenItStarts_ThenEachAttemptFailsWithInvalidEndpoint)
{
  tr::stream::io_runner runner (2);
  ss::session_options_t options;
  options.policy.initial_delay = ms (5);
  options.policy.max_attempts = 2;
  ss::stream_session session (
      runner.get_executor (),
      ss::make_serial_factory (
          by_usb ("16E0", "", ""),
          [] () { return std::vector<ss::serial_port_info_t> (); }),
      options);
  tr::status_result_t const opened = session.open (ms (3000));
  ASSERT_FALSE (opened.has_value ());
  EXPECT_TRUE (opened.error () == transport_errc::invalid_endpoint);
  EXPECT_EQ (session.state (), ss::channel_state::faulted);
  EXPECT_EQ (session.session_statistics ().failed_attempts, 2u);
}

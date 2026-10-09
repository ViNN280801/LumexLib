#include <cerrno>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/serial/LumexSerialPort"
#include "lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::applied::serial::port;
using namespace lumex::applied::serial::enumeration;
using namespace lumex::applied::serial::probe::detail;
using namespace lumex::applied::serial::resolver;

TEST (LumexSerialPortEnumeration, StateToStringCoversEveryEnumerator)
{
  EXPECT_STREQ (serial_port_state_to_string (serial_port_state::available),
                "Available");
  EXPECT_STREQ (serial_port_state_to_string (serial_port_state::busy), "Busy");
  EXPECT_STREQ (serial_port_state_to_string (serial_port_state::free), "Free");
  EXPECT_STREQ (
      serial_port_state_to_string (static_cast<serial_port_state> (99)),
      "Unknown");
}

TEST (LumexSerialPortEnumeration, FormatSystemErrorZeroIsEmpty)
{
  EXPECT_TRUE (port_process_resolver::format_system_error (0).empty ());
}

TEST (LumexSerialPortEnumeration, FormatSystemErrorKnownCodeMentionsNumber)
{
  std::string const text = port_process_resolver::format_system_error (2);
  EXPECT_FALSE (text.empty ());
  EXPECT_NE (text.find ("2"), std::string::npos);
}

TEST (LumexSerialPortEnumeration, BluetoothPredicateRejectsEmptyAndUnknown)
{
  EXPECT_FALSE (is_bluetooth_enumerated_port (std::string ()));
  EXPECT_FALSE (is_bluetooth_enumerated_port ("not-a-port"));
  EXPECT_FALSE (is_bluetooth_enumerated_port ("COM"));
}

TEST (LumexSerialPortEnumeration, StateToString_WhenFound_ThenEnumeratorName)
{
  EXPECT_STREQ (serial_port_state_to_string (serial_port_state::available),
                "Available");
}

TEST (LumexSerialPortEnumeration, StateToString_WhenUnfound_ThenUnknown)
{
  EXPECT_STREQ (
      serial_port_state_to_string (static_cast<serial_port_state> (99)),
      "Unknown");
}

TEST (LumexSerialPortEnumeration, PrintGroupsSyntheticInfos)
{
  std::vector<serial_port_info_t> infos (3);
  infos[0].path = "COM1";
  infos[0].state = serial_port_state::available;
  infos[0].friendly_name = "USB Serial";
  infos[0].vid = "0403";
  infos[0].pid = "6001";
  infos[1].path = "COM2";
  infos[1].state = serial_port_state::busy;
  infos[1].holder_process_info = "PID=1";
  infos[2].path = "COM3";
  infos[2].state = serial_port_state::free;

  std::ostringstream out;
  print_serial_ports_info (out, infos);
  std::string const text = out.str ();
  EXPECT_NE (text.find ("COM1"), std::string::npos);
  EXPECT_NE (text.find ("USB Serial"), std::string::npos);
  EXPECT_NE (text.find ("VID:0403"), std::string::npos);
  EXPECT_NE (text.find ("COM2"), std::string::npos);
  EXPECT_NE (text.find ("PID=1"), std::string::npos);
  EXPECT_NE (text.find ("COM3"), std::string::npos);
  EXPECT_NE (text.find ("(total: 3)"), std::string::npos);
}

TEST (LumexSerialPortEnumeration, EnumerateBothBluetoothFlagValues)
{
  std::vector<serial_port_info_t> filtered;
  std::vector<serial_port_info_t> unfiltered;
  EXPECT_NO_THROW (filtered
                   = enumerate_serial_ports_detailed (std::string (), true));
  EXPECT_NO_THROW (unfiltered
                   = enumerate_serial_ports_detailed (std::string (), false));

  std::vector<std::string> filtered_names;
  std::vector<std::string> unfiltered_names;
  EXPECT_NO_THROW (filtered_names = enumerate_serial_port_names (true));
  EXPECT_NO_THROW (unfiltered_names = enumerate_serial_port_names (false));

  EXPECT_EQ (filtered_names.size (), filtered.size ());
  EXPECT_EQ (unfiltered_names.size (), unfiltered.size ());
  EXPECT_GE (unfiltered.size (), filtered.size ());
  for (std::size_t i = 0; i < filtered_names.size (); ++i)
    {
      bool found = false;
      for (std::size_t j = 0; j < unfiltered_names.size (); ++j)
        {
          if (unfiltered_names[j] == filtered_names[i])
            {
              found = true;
              break;
            }
        }
      EXPECT_TRUE (found) << filtered_names[i];
    }
  if (!filtered_names.empty ())
    EXPECT_FALSE (filtered_names.front ().empty ());
  if (!unfiltered_names.empty ())
    EXPECT_FALSE (unfiltered_names.front ().empty ());
}

TEST (LumexSerialPortEnumeration, FullPathFlagSelectsNameOrOpenablePath)
{
  std::vector<serial_port_info_t> named;
  std::vector<serial_port_info_t> pathed;
  EXPECT_NO_THROW (
      named = enumerate_serial_ports_detailed (std::string (), false, false));
  EXPECT_NO_THROW (
      pathed = enumerate_serial_ports_detailed (std::string (), false, true));

  ASSERT_EQ (named.size (), pathed.size ());
  for (std::size_t i = 0; i < named.size (); ++i)
    {
      // The two entries describe one and the same port: the resolver turns
      // the canonical name into the openable path.
      EXPECT_EQ (resolve_serial_port_path (
                     named[i].path, Constants::KSERIAL_PORT_CHANNEL_TYPE),
                 pathed[i].path)
          << named[i].path;
#if defined(_WIN32)
      if (named[i].path.compare (0, 3, "COM") == 0)
        EXPECT_EQ (pathed[i].path.compare (0, 4, R"(\\.\)"), 0)
            << pathed[i].path;
#else
      EXPECT_EQ (named[i].path.find ('/'), std::string::npos) << named[i].path;
      EXPECT_EQ (pathed[i].path.compare (0, 5, "/dev/"), 0) << pathed[i].path;
#endif
    }

  // The names-only entry point follows the same flag.
  std::vector<std::string> names;
  EXPECT_NO_THROW (names = enumerate_serial_port_names (false, false));
  ASSERT_EQ (names.size (), named.size ());
  for (std::size_t i = 0; i < names.size (); ++i)
    EXPECT_EQ (names[i], named[i].path);
}

// get_process_holding_port walks every process handle via
// NtQuerySystemInformation. That can block for a long time on a busy
// Windows host, so it is not called from this suite. Enumeration reports
// a busy-hint string instead of invoking the resolver.

TEST (LumexSerialPortEnumeration, StateFromOpenResultMapsEveryOutcome)
{
  open_result_t result;
  result.outcome = open_outcome::opened;
  EXPECT_EQ (state_from_open_result (result), serial_port_state::available);

  result = open_result_t ();
  result.outcome = open_outcome::timed_out;
  EXPECT_EQ (state_from_open_result (result), serial_port_state::unresponsive);

  result = open_result_t ();
  result.outcome = open_outcome::failed;
#if defined(_WIN32)
  result.error_code = 5; // ERROR_ACCESS_DENIED
  EXPECT_EQ (state_from_open_result (result), serial_port_state::busy);
  result.error_code = 32; // ERROR_SHARING_VIOLATION
  EXPECT_EQ (state_from_open_result (result), serial_port_state::busy);
  result.error_code = 2; // ERROR_FILE_NOT_FOUND
#else
  result.error_code = EBUSY;
  EXPECT_EQ (state_from_open_result (result), serial_port_state::busy);
  result.error_code = EACCES;
  EXPECT_EQ (state_from_open_result (result), serial_port_state::busy);
  result.error_code = ENOENT;
#endif
  EXPECT_EQ (state_from_open_result (result), serial_port_state::free);
}

TEST (LumexSerialPortEnumeration, StateToString_WhenUnresponsive_ThenToken)
{
  EXPECT_STREQ (serial_port_state_to_string (serial_port_state::unresponsive),
                "Unresponsive");
}

TEST (LumexSerialPortEnumeration, PrintGroupsUnresponsiveInfos)
{
  std::vector<serial_port_info_t> infos (2);
  infos[0].path = "COM1";
  infos[0].state = serial_port_state::available;
  infos[1].path = "COM9";
  infos[1].state = serial_port_state::unresponsive;
  infos[1].system_error = "open timed out after 500 ms";

  std::ostringstream out;
  print_serial_ports_info (out, infos);
  std::string const text = out.str ();

  std::size_t const section
      = text.find ("=== Ports: unresponsive (open timed out) ===");
  ASSERT_NE (section, std::string::npos);
  std::size_t const row = text.find ("COM9");
  ASSERT_NE (row, std::string::npos);
  EXPECT_LT (section, row);
  EXPECT_NE (text.find ("open timed out after 500 ms"), std::string::npos);
  EXPECT_LT (text.find ("COM9"), text.find ("=== Ports: free"));
  EXPECT_NE (text.find ("(total: 2)"), std::string::npos);
}

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/serial/probe/LumexSerialProber.hpp"

using namespace lumex::applied::serial::probe;

#if defined(__linux__)
namespace
{
// pty pair used as a fake serial device: the master side plays the device.
struct pty_pair_t
{
  int master = -1;
  std::string slave_path;

  bool
  open_pair ()
  {
    master = ::posix_openpt (O_RDWR | O_NOCTTY);
    if (master < 0)
      return false;
    if (::grantpt (master) != 0 || ::unlockpt (master) != 0)
      return false;
    char *name = ::ptsname (master);
    if (name == nullptr)
      return false;
    slave_path = name;
    return true;
  }

  ~pty_pair_t ()
  {
    if (master >= 0)
      ::close (master);
  }
};
} // namespace

TEST (LumexSerialProberPty, GivenPreWrittenResponse_WhenProbe_ThenResponded)
{
  pty_pair_t pair;
  if (!pair.open_pair ())
    GTEST_SKIP () << "the platform cannot allocate a pty pair";

  SerialProber prober (pair.slave_path, serial_port_settings_t ());
  serial_io_result_t const opened
      = prober.open (std::chrono::milliseconds (500));
  ASSERT_EQ (opened.status, serial_io_status::ok) << opened.system_error;
  ASSERT_TRUE (prober.is_open ());

  std::vector<std::uint8_t> const response = { 0x26U, 0x31U, 0x38U, 0x0DU };
  ssize_t const pre_written
      = ::write (pair.master, response.data (), response.size ());
  ASSERT_EQ (pre_written, static_cast<ssize_t> (response.size ()));

  serial_probe_result_t const result = prober.probe (
      std::vector<std::uint8_t> (1U, 0x41U),
      [] (std::vector<std::uint8_t> const &data)
        { return data.size () >= 4U; }, std::chrono::milliseconds (1000));

  EXPECT_EQ (result.status, serial_probe_status::responded);
  EXPECT_EQ (result.response, response);

  prober.close ();
  EXPECT_FALSE (prober.is_open ());
}

TEST (LumexSerialProberPty, GivenSilence_WhenProbe_ThenNoResponse)
{
  pty_pair_t pair;
  if (!pair.open_pair ())
    GTEST_SKIP () << "the platform cannot allocate a pty pair";

  SerialProber prober (pair.slave_path, serial_port_settings_t ());
  serial_probe_result_t const result = prober.probe (
      std::vector<std::uint8_t> (), [] (std::vector<std::uint8_t> const &)
        { return true; }, std::chrono::milliseconds (100));

  EXPECT_EQ (result.status, serial_probe_status::no_response);
  EXPECT_TRUE (result.response.empty ());
}

TEST (LumexSerialProberPty, WriteAndReadRoundTrip)
{
  pty_pair_t pair;
  if (!pair.open_pair ())
    GTEST_SKIP () << "the platform cannot allocate a pty pair";

  SerialProber prober (pair.slave_path, serial_port_settings_t ());
  serial_io_result_t const opened
      = prober.open (std::chrono::milliseconds (500));
  ASSERT_EQ (opened.status, serial_io_status::ok) << opened.system_error;

  std::vector<std::uint8_t> const request = { 0x41U, 0x42U, 0x43U };
  serial_io_result_t const written
      = prober.write (request, std::chrono::milliseconds (500));
  ASSERT_EQ (written.status, serial_io_status::ok);
  EXPECT_EQ (written.bytes_transferred, request.size ());

  std::vector<std::uint8_t> seen (request.size ());
  ssize_t const got = ::read (pair.master, seen.data (), seen.size ());
  ASSERT_EQ (got, static_cast<ssize_t> (request.size ()));
  EXPECT_EQ (seen, request);

  std::vector<std::uint8_t> const answer = { 0x4FU, 0x4BU };
  ASSERT_EQ (::write (pair.master, answer.data (), answer.size ()),
             static_cast<ssize_t> (answer.size ()));
  serial_read_result_t const read_result
      = prober.read (std::chrono::milliseconds (500));
  EXPECT_EQ (read_result.status, serial_io_status::ok);
  EXPECT_EQ (read_result.data, answer);
}

TEST (LumexSerialProberPty, OpenOnAnAbsentPathFails)
{
  SerialProber prober ("/dev/ttyLUMEXNOPE", serial_port_settings_t ());
  serial_io_result_t const result
      = prober.open (std::chrono::milliseconds (100));

  EXPECT_EQ (result.status, serial_io_status::failed);
  EXPECT_FALSE (result.system_error.empty ());
}
#endif

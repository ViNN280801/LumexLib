#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/serial/probe/LumexSerialProber.hpp"

using namespace lumex::applied::serial::probe;

namespace
{
// Path that no scan can create, so the failure paths are deterministic.
std::string
absent_port_path ()
{
#if defined(_WIN32)
  return "\\\\.\\LUMEXNOPE";
#else
  return "/dev/ttyLUMEXNOPE";
#endif
}
} // namespace

TEST (LumexSerialProber, ReadAndWriteAreNotOpenBeforeOpen)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());

  EXPECT_FALSE (prober.is_open ());

  serial_read_result_t const read_result
      = prober.read (std::chrono::milliseconds (10));
  EXPECT_EQ (read_result.status, serial_io_status::not_open);
  EXPECT_TRUE (read_result.data.empty ());

  serial_io_result_t const write_result = prober.write (
      std::vector<std::uint8_t> (1U, 0x01U), std::chrono::milliseconds (10));
  EXPECT_EQ (write_result.status, serial_io_status::not_open);
  EXPECT_EQ (write_result.bytes_transferred, 0U);
}

TEST (LumexSerialProber, OpenOnAnAbsentPortFailsFast)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());

  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  serial_io_result_t const result
      = prober.open (std::chrono::milliseconds (500));
  std::chrono::milliseconds const elapsed
      = std::chrono::duration_cast<std::chrono::milliseconds> (
          std::chrono::steady_clock::now () - start);

  EXPECT_EQ (result.status, serial_io_status::failed);
  EXPECT_FALSE (result.system_error.empty ());
  EXPECT_FALSE (prober.is_open ());
  EXPECT_LT (elapsed.count (), 500);
}

TEST (LumexSerialProber, CloseOnAClosedSessionIsANoOp)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());

  EXPECT_NO_THROW (prober.close ());
  EXPECT_FALSE (prober.is_open ());
  EXPECT_NO_THROW (prober.close ());
  EXPECT_FALSE (prober.is_open ());
}

TEST (LumexSerialProber, ProbeOnAnAbsentPortReportsOpenFailed)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());

  serial_probe_result_t const result = prober.probe (
      std::vector<std::uint8_t> (1U, 0x41U),
      [] (std::vector<std::uint8_t> const &) { return true; },
      std::chrono::milliseconds (500));

  EXPECT_EQ (result.status, serial_probe_status::open_failed);
  EXPECT_FALSE (result.system_error.empty ());
  EXPECT_TRUE (result.response.empty ());
}

TEST (LumexSerialProber, MoveConstructTransfersTheSession)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());
  SerialProber moved (std::move (prober));

  EXPECT_FALSE (moved.is_open ());
  // The moved-from session stays destructible and inert.
  EXPECT_NO_THROW (prober.close ());
  EXPECT_FALSE (prober.is_open ());
}

TEST (LumexSerialProber, MoveAssignTransfersTheSession)
{
  SerialProber prober (absent_port_path (), serial_port_settings_t ());
  SerialProber other (absent_port_path (), serial_port_settings_t ());

  other = std::move (prober);

  EXPECT_FALSE (other.is_open ());
}

TEST (LumexSerialProber, TypeTraitsLockTheMoveOnlyContract)
{
  EXPECT_TRUE (std::is_move_constructible<SerialProber>::value);
  EXPECT_TRUE (std::is_move_assignable<SerialProber>::value);
  EXPECT_FALSE (std::is_copy_constructible<SerialProber>::value);
  EXPECT_FALSE (std::is_copy_assignable<SerialProber>::value);
}

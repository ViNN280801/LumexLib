// LumexSerialBoundedOpen.cxx11.tests.cpp
//
// The bounded open takes a UTF-8 path. On Windows it used CreateFileA, which
// reads the path in the ANSI code page, so a name outside that page named
// another file or none. These tests open an ordinary file (CreateFile opens
// a file and a port alike), made with the wide API of the test support, and
// refuse a path that is not UTF-8; the Windows part is empty elsewhere.
#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp"
#include "lumex/tests/support/LumexTestCyrillicCopy.hpp"
#include "lumex/tests/support/LumexTestUtf8Files.hpp"

using namespace lumex::applied::serial::probe::detail;

#if defined(_WIN32)
namespace
{
std::chrono::milliseconds const kDeadline (5000);
} // namespace

TEST (LumexSerialBoundedOpenTest,
      GivenFileWithCyrillicName_WhenBoundedOpen_ThenItIsOpened)
{
  namespace files = lumex_test::utf8_files;
  std::string const path = lumex_test::cyrillic_copy::self_directory () + "\\"
                           + files::cyrillic ("_bounded_open.txt");
  ASSERT_TRUE (files::write_file (path, "x"));

  open_result_t const opened = bounded_open_serial_port (path, 0U, kDeadline);

  if (opened.outcome == open_outcome::opened)
    close_serial_handle (opened.handle);
  files::remove_file (path);
  EXPECT_TRUE (opened.outcome == open_outcome::opened)
      << "error " << opened.error_code << ": " << opened.system_error;
}

TEST (LumexSerialBoundedOpenTest,
      GivenPathThatIsNotUtf8_WhenBoundedOpen_ThenItIsRefusedAsInvalidName)
{
  namespace files = lumex_test::utf8_files;
  // The file "a" exists: a skipped byte would open it instead of refusing.
  std::string const base
      = lumex_test::cyrillic_copy::self_directory () + "\\LumexBoundedOpenBad";
  ASSERT_TRUE (files::write_file (base, "x"));

  open_result_t const opened
      = bounded_open_serial_port (base + "\xFF", 0U, kDeadline);

  if (opened.outcome == open_outcome::opened)
    close_serial_handle (opened.handle);
  files::remove_file (base);
  EXPECT_TRUE (opened.outcome == open_outcome::failed);
  EXPECT_EQ (opened.error_code, static_cast<int> (ERROR_INVALID_NAME));
}
#else
TEST (LumexSerialBoundedOpenTest, GivenMissingPath_WhenBoundedOpen_ThenFailed)
{
  open_result_t const opened = bounded_open_serial_port (
      "/nonexistent/lumex-bounded-open", 0U, std::chrono::milliseconds (1000));
  EXPECT_TRUE (opened.outcome == open_outcome::failed);
  EXPECT_NE (opened.error_code, 0);
}
#endif

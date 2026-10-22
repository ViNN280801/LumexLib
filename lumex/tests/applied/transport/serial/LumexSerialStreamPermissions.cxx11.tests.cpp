// The serial stream and permissions (G7): on POSIX a node the user may not
// open (a temporary file of mode 000) gives transport_errc::permission_denied
// and an error record of component transport.serial that names the node, its
// owner, group and mode (skipped for root, which ignores the mode); on
// Windows ERROR_ACCESS_DENIED from opening something that is not available
// (a directory stands in for a busy COM port) is NOT mapped and stays the
// system error.

#include <cstddef>
#include <cstdlib>
#include <future>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexPermissionDiagnostics.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/serial/LumexSerialStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/tests/applied/transport/LumexTempNode.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace lg = lumex::applied::transport::log;
namespace detail = lumex::applied::transport::detail;
using tr::deadline_t;
using tr::error::transport_errc;

struct captured_record_t
{
  lg::log_level level;
  std::string component;
  std::string message;
};

std::vector<captured_record_t> &
captured ()
{
  static std::vector<captured_record_t> records;
  return records;
}

void
capture_sink (lg::log_record_t const &record)
{
  captured_record_t entry;
  entry.level = record.level;
  entry.component = record.component;
  entry.message = record.message;
  captured ().push_back (entry);
}

class LumexSerialStreamPermissionsTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    captured ().clear ();
    lg::set_log_sink (&capture_sink);
    lg::set_log_level (lg::log_level::trace);
  }

  void
  TearDown () override
  {
    lg::set_log_sink (nullptr);
    lg::set_log_level (lg::log_level::warning);
  }
};

// The "open failed" record of the serial stream, or nullptr.
captured_record_t const *
open_failed_record ()
{
  for (std::size_t i = 0; i < captured ().size (); ++i)
    if (captured ()[i].component == "transport.serial"
        && captured ()[i].message.find ("open failed") != std::string::npos)
      return &captured ()[i];
  return nullptr;
}
} // namespace

#if defined(_WIN32)
TEST_F (
    LumexSerialStreamPermissionsTest,
    GivenWindowsAccessDenied_WhenOpened_ThenItStaysASystemErrorAndNotPermissionDenied)
{
  // CreateFile on a directory without FILE_FLAG_BACKUP_SEMANTICS fails with
  // ERROR_ACCESS_DENIED (5), the code of a busy COM port.
  char const *const root = std::getenv ("SystemRoot");
  if (root == nullptr)
    GTEST_SKIP () << "SystemRoot is not set";
  tr::stream::io_runner runner;
  tr::serial::serial_stream_options_t options;
  options.path = root;
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_FALSE (result.error () == transport_errc::permission_denied);
  EXPECT_TRUE (result.error ().category () == std::system_category ());
  EXPECT_EQ (result.error ().value (), 5);
  captured_record_t const *const record = open_failed_record ();
  ASSERT_NE (record, nullptr);
  EXPECT_EQ (record->message.find ("udev"), std::string::npos)
      << record->message;
}
#else
TEST_F (
    LumexSerialStreamPermissionsTest,
    GivenANodeWithMode000_WhenOpened_ThenPermissionDeniedAndTheLogNamesNodeOwnerGroupAndMode)
{
  if (::geteuid () == 0)
    GTEST_SKIP () << "root ignores the mode of a file";
  // Removed by the destructor, also when an ASSERT_* leaves the test.
  lumex_transport_tests::temp_node const node (0);
  ASSERT_FALSE (node.path ().empty ());
  std::string const &path = node.path ();

  tr::stream::io_runner runner;
  tr::serial::serial_stream_options_t options;
  options.path = path;
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (5000));

  struct ::stat info;
  ASSERT_EQ (::stat (path.c_str (), &info), 0);

  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::permission_denied)
      << result.error ().message ();
  EXPECT_FALSE (stream.is_open ());

  captured_record_t const *const record = open_failed_record ();
  ASSERT_NE (record, nullptr);
  EXPECT_EQ (record->level, lg::log_level::error);
  std::string const &text = record->message;
  EXPECT_NE (text.find (path), std::string::npos) << text;
  EXPECT_NE (text.find ("mode 0000"), std::string::npos) << text;
  ::passwd const *const user = ::getpwuid (info.st_uid);
  std::string const owner = user != nullptr
                                ? std::string (user->pw_name)
                                : "uid " + std::to_string (info.st_uid);
  EXPECT_NE (text.find ("owner " + owner), std::string::npos) << text;
  ::group const *const group = ::getgrgid (info.st_gid);
  std::string const group_text = group != nullptr
                                     ? std::string (group->gr_name)
                                     : "gid " + std::to_string (info.st_gid);
  EXPECT_NE (text.find ("group " + group_text), std::string::npos) << text;
  EXPECT_NE (text.find ("udev"), std::string::npos) << text;
}

TEST_F (
    LumexSerialStreamPermissionsTest,
    GivenTheErrorLogIsOff_WhenAPermissionDeniedOpenFails_ThenNoNodeLookupRuns)
{
  if (::geteuid () == 0)
    GTEST_SKIP () << "root ignores the mode of a file";
  lumex_transport_tests::temp_node const node (0);
  ASSERT_FALSE (node.path ().empty ());
  lg::set_log_level (lg::log_level::off);
  ASSERT_FALSE (lg::log_enabled (lg::log_level::error, "transport.serial"));

  tr::stream::io_runner runner;
  tr::serial::serial_stream_options_t options;
  options.path = node.path ();
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  std::size_t const before = detail::node_query_stats ().count;
  tr::status_result_t const result = stream.open (deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  // The code is still permission_denied; only the diagnostic is skipped.
  EXPECT_TRUE (result.error () == transport_errc::permission_denied)
      << result.error ().message ();
  EXPECT_EQ (detail::node_query_stats ().count, before);
  EXPECT_TRUE (captured ().empty ());
}

TEST_F (
    LumexSerialStreamPermissionsTest,
    GivenTheErrorLogIsOn_WhenAPermissionDeniedOpenFails_ThenTheLookupRunsOnceAndNotOnTheIoThread)
{
  if (::geteuid () == 0)
    GTEST_SKIP () << "root ignores the mode of a file";
  lumex_transport_tests::temp_node const node (0);
  ASSERT_FALSE (node.path ().empty ());

  tr::stream::io_runner runner;
  std::promise<std::thread::id> io_thread;
  asio::post (runner.context (), [&io_thread] ()
                { io_thread.set_value (std::this_thread::get_id ()); });
  std::thread::id const io_thread_id = io_thread.get_future ().get ();

  tr::serial::serial_stream_options_t options;
  options.path = node.path ();
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  std::size_t const before = detail::node_query_stats ().count;
  tr::status_result_t const result = stream.open (deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  detail::node_query_stats_t const after = detail::node_query_stats ();
  EXPECT_EQ (after.count, before + 1);
  // The lookups (stat, getpwuid_r, getgrgid_r) may block: not on the io
  // thread, not on the caller's thread either (the worker thread).
  EXPECT_FALSE (after.last_thread == io_thread_id);
  EXPECT_FALSE (after.last_thread == std::this_thread::get_id ());
  captured_record_t const *const record = open_failed_record ();
  ASSERT_NE (record, nullptr);
  EXPECT_NE (record->message.find ("mode 0000"), std::string::npos)
      << record->message;
}

TEST_F (
    LumexSerialStreamPermissionsTest,
    GivenAMissingNode_WhenOpened_ThenTheSystemErrorIsKeptAndNoPermissionText)
{
  tr::stream::io_runner runner;
  tr::serial::serial_stream_options_t options;
  options.path = "/dev/ttyLUMEX_NO_SUCH_PORT";
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_FALSE (result.error () == transport_errc::permission_denied);
  EXPECT_TRUE (result.error ().category () == std::system_category ());
  captured_record_t const *const record = open_failed_record ();
  ASSERT_NE (record, nullptr);
  EXPECT_EQ (record->message.find ("udev"), std::string::npos)
      << record->message;
}
#endif

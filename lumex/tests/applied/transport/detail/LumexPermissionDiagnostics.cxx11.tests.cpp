// The permission diagnostics of the serial open
// (lumex/applied/transport/detail/LumexPermissionDiagnostics): the message
// from fixed inputs on every platform (names, numeric fallback, odd modes, a
// node that cannot be examined), the decision which errno is a permission
// problem (EACCES and EPERM on POSIX; on Windows nothing, since
// ERROR_ACCESS_DENIED on a COM port means "busy"), the code a failed open
// gets, and on POSIX the real lookup of a temporary file.

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <system_error>
#include <thread>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexPermissionDiagnostics.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/tests/applied/transport/LumexTempNode.hpp"

namespace
{
namespace dt = lumex::applied::transport::detail;
using lumex::applied::transport::error::transport_errc;

#if defined(_WIN32)
// ERROR_ACCESS_DENIED of winerror.h.
int const kWindowsAccessDenied = 5;
#endif

dt::node_access_t
node_of (std::uint32_t uid, std::uint32_t gid, std::uint32_t mode,
         std::string const &owner, std::string const &group)
{
  dt::node_access_t node;
  node.found = true;
  node.uid = uid;
  node.gid = gid;
  node.mode = mode;
  node.owner = owner;
  node.group = group;
  return node;
}

bool
contains (std::string const &text, std::string const &part)
{
  return text.find (part) != std::string::npos;
}
} // namespace

TEST (LumexPermissionDiagnosticsTest,
      GivenKnownNames_WhenTheMessageIsBuilt_ThenNodeOwnerGroupModeAndFix)
{
  std::string const text = dt::permission_denied_message (
      "/dev/ttyUSB0", node_of (0, 20, 0660, "root", "dialout"));
  EXPECT_TRUE (contains (text, "permission denied on /dev/ttyUSB0")) << text;
  EXPECT_TRUE (contains (text, "owner root,")) << text;
  EXPECT_TRUE (contains (text, "group dialout,")) << text;
  EXPECT_TRUE (contains (text, "mode 0660;")) << text;
#if defined(__linux__)
  EXPECT_TRUE (contains (text, "dialout")) << text;
  EXPECT_TRUE (contains (text, "plugdev")) << text;
  EXPECT_TRUE (contains (text, "the LumexLib udev rule (share/LumexLib/udev)"))
      << text;
  EXPECT_FALSE (contains (text, "examples/transport")) << text;
#else
  EXPECT_TRUE (contains (text, "grant the user read and write access to the "
                               "node"))
      << text;
#endif
}

TEST (LumexPermissionDiagnosticsTest,
      GivenASystemThatIsNotLinux_WhenTheMessageIsBuilt_ThenNoLinuxHint)
{
  std::string const text = dt::permission_denied_message (
      "/dev/cu.usbserial", node_of (0, 20, 0660, "root", "wheel"));
#if defined(__linux__)
  // The Linux hint is the right one here; the generic text is for the rest.
  EXPECT_FALSE (contains (text, "grant the user")) << text;
#else
  EXPECT_FALSE (contains (text, "dialout")) << text;
  EXPECT_FALSE (contains (text, "plugdev")) << text;
  EXPECT_FALSE (contains (text, "udev")) << text;
  EXPECT_TRUE (contains (text, "grant the user read and write access to the "
                               "node"))
      << text;
#endif
}

TEST (
    LumexPermissionDiagnosticsTest,
    GivenAGroupWithReadAndWrite_WhenTheMessageIsBuilt_ThenGroupAndRuleAreBothOffered)
{
  std::string const text = dt::permission_denied_message (
      "/dev/ttyUSB0", node_of (0, 20, 0660, "root", "dialout"));
#if defined(__linux__)
  EXPECT_TRUE (contains (text, "add the user to the group")) << text;
  EXPECT_TRUE (contains (text, "udev rule")) << text;
#else
  static_cast<void> (text);
#endif
}

TEST (
    LumexPermissionDiagnosticsTest,
    GivenAGroupWithoutReadAndWrite_WhenTheMessageIsBuilt_ThenOnlyTheRuleIsOffered)
{
  // The group lacks read, write or both (whatever the owner and the others
  // have), against modes where the group has both.
  std::uint32_t const without[] = { 0000U, 0600U, 0640U, 0620U, 0604U };
  for (std::uint32_t mode : without)
    {
      std::string const text = dt::permission_denied_message (
          "/dev/ttyUSB0", node_of (0, 20, mode, "root", "dialout"));
#if defined(__linux__)
      EXPECT_FALSE (contains (text, "add the user to the group")) << text;
      EXPECT_TRUE (contains (text, "udev rule (share/LumexLib/udev)")) << text;
      EXPECT_TRUE (contains (text, "does not help")) << text;
#else
      EXPECT_TRUE (contains (text, "grant the user")) << text;
#endif
    }
  std::uint32_t const with[] = { 0060U, 0460U, 0664U, 0770U, 0777U, 02660U };
  for (std::uint32_t mode : with)
    {
      std::string const text = dt::permission_denied_message (
          "/dev/ttyUSB0", node_of (0, 20, mode, "root", "dialout"));
#if defined(__linux__)
      EXPECT_TRUE (contains (text, "add the user to the group")) << text;
      EXPECT_FALSE (contains (text, "does not help")) << text;
#else
      EXPECT_TRUE (contains (text, "grant the user")) << text;
#endif
    }
}

TEST (LumexPermissionDiagnosticsTest,
      GivenIdsWithoutNames_WhenTheMessageIsBuilt_ThenTheNumbersStandIn)
{
  std::string const text = dt::permission_denied_message (
      "/dev/hidraw3", node_of (1234, 77, 0600, "", ""));
  EXPECT_TRUE (contains (text, "permission denied on /dev/hidraw3")) << text;
  EXPECT_TRUE (contains (text, "owner uid 1234,")) << text;
  EXPECT_TRUE (contains (text, "group gid 77,")) << text;
  EXPECT_TRUE (contains (text, "mode 0600;")) << text;
}

TEST (LumexPermissionDiagnosticsTest,
      GivenOneNameMissing_WhenTheMessageIsBuilt_ThenOnlyThatIdIsNumeric)
{
  std::string const text = dt::permission_denied_message (
      "/dev/ttyACM1", node_of (0, 999, 0664, "root", ""));
  EXPECT_TRUE (contains (text, "owner root,")) << text;
  EXPECT_TRUE (contains (text, "group gid 999,")) << text;
  EXPECT_TRUE (contains (text, "mode 0664;")) << text;
}

TEST (LumexPermissionDiagnosticsTest,
      GivenSpecialModes_WhenTheMessageIsBuilt_ThenFourOctalDigits)
{
  EXPECT_TRUE (contains (
      dt::permission_denied_message ("n", node_of (0, 0, 0, "root", "root")),
      "mode 0000;"));
  EXPECT_TRUE (contains (dt::permission_denied_message (
                             "n", node_of (0, 0, 04755, "root", "root")),
                         "mode 4755;"));
  EXPECT_TRUE (contains (dt::permission_denied_message (
                             "n", node_of (0, 0, 0777, "root", "root")),
                         "mode 0777;"));
  EXPECT_TRUE (contains (dt::permission_denied_message (
                             "n", node_of (0, 0, 01777, "root", "root")),
                         "mode 1777;"));
}

TEST (
    LumexPermissionDiagnosticsTest,
    GivenANodeThatCannotBeExamined_WhenTheMessageIsBuilt_ThenItSaysSoAndKeepsTheFix)
{
  std::string const text
      = dt::permission_denied_message ("/dev/ttyUSB9", dt::node_access_t ());
  EXPECT_TRUE (contains (text, "permission denied on /dev/ttyUSB9")) << text;
  EXPECT_TRUE (contains (text, "unknown")) << text;
  EXPECT_FALSE (contains (text, "mode 0")) << text;
#if defined(__linux__)
  // The mode is unknown: both fixes are offered.
  EXPECT_TRUE (contains (text, "add the user to the group")) << text;
  EXPECT_TRUE (contains (text, "udev rule")) << text;
#else
  EXPECT_TRUE (contains (text, "grant the user")) << text;
#endif
}

TEST (
    LumexPermissionDiagnosticsTest,
    GivenErrnoValues_WhenClassified_ThenPermissionOnlyForEaccesAndEpermOnPosix)
{
#if defined(_WIN32)
  EXPECT_FALSE (dt::is_permission_error (kWindowsAccessDenied));
  EXPECT_FALSE (dt::is_permission_error (EACCES));
  EXPECT_FALSE (dt::is_permission_error (EPERM));
  EXPECT_FALSE (dt::is_permission_error (0));
#else
  EXPECT_TRUE (dt::is_permission_error (EACCES));
  EXPECT_TRUE (dt::is_permission_error (EPERM));
  EXPECT_FALSE (dt::is_permission_error (ENOENT));
  EXPECT_FALSE (dt::is_permission_error (EBUSY));
  EXPECT_FALSE (dt::is_permission_error (EIO));
  EXPECT_FALSE (dt::is_permission_error (0));
#endif
}

TEST (LumexPermissionDiagnosticsTest,
      GivenAFailedOpen_WhenTheCodeIsMade_ThenPermissionDeniedOnlyWhereItIsOne)
{
  std::error_code const missing = dt::open_failure_code (ENOENT);
  EXPECT_TRUE (missing.category () == std::system_category ());
  EXPECT_EQ (missing.value (), ENOENT);
  EXPECT_FALSE (missing == transport_errc::permission_denied);

#if defined(_WIN32)
  // A busy COM port: ERROR_ACCESS_DENIED stays the system error.
  std::error_code const denied = dt::open_failure_code (kWindowsAccessDenied);
  EXPECT_TRUE (denied.category () == std::system_category ());
  EXPECT_EQ (denied.value (), kWindowsAccessDenied);
  EXPECT_FALSE (denied == transport_errc::permission_denied);
#else
  EXPECT_TRUE (dt::open_failure_code (EACCES)
               == transport_errc::permission_denied);
  EXPECT_TRUE (dt::open_failure_code (EPERM)
               == transport_errc::permission_denied);
#endif
}

#if defined(_WIN32)
TEST (LumexPermissionDiagnosticsTest,
      GivenWindows_WhenTheNodeIsQueried_ThenNothingIsFound)
{
  EXPECT_FALSE (dt::query_node_access ("COM3").found);
  EXPECT_TRUE (contains (dt::describe_permission_denied ("COM3"),
                         "permission denied on COM3"));
}
#else
using lumex_transport_tests::temp_node;

TEST (LumexPermissionDiagnosticsTest,
      GivenAFileWithMode0640_WhenTheNodeIsQueried_ThenOwnerGroupAndModeAreRead)
{
  temp_node const node (0640);
  ASSERT_FALSE (node.path ().empty ());
  dt::node_access_t const access = dt::query_node_access (node.path ());
  ASSERT_TRUE (access.found);
  EXPECT_EQ (access.mode, 0640U);
  EXPECT_EQ (access.uid, static_cast<std::uint32_t> (::geteuid ()));
  EXPECT_FALSE (access.owner.empty ());
}

TEST (LumexPermissionDiagnosticsTest,
      GivenAFileWithStickyBit_WhenTheNodeIsQueried_ThenTheSpecialBitsAreKept)
{
  // The mask is 07777: the sticky bit (01000) of 01640 must survive.
  temp_node const node (01640);
  ASSERT_FALSE (node.path ().empty ());
  dt::node_access_t const access = dt::query_node_access (node.path ());
  ASSERT_TRUE (access.found);
  EXPECT_EQ (access.mode, 01640U);
  EXPECT_TRUE (contains (dt::permission_denied_message (node.path (), access),
                         "mode 1640;"));
}

TEST (LumexPermissionDiagnosticsTest,
      GivenAQuery_WhenItRuns_ThenTheCountGrowsAndTheThreadIsTheCaller)
{
  temp_node const node (0640);
  ASSERT_FALSE (node.path ().empty ());
  dt::node_query_stats_t const before = dt::node_query_stats ();
  static_cast<void> (dt::query_node_access (node.path ()));
  dt::node_query_stats_t const after = dt::node_query_stats ();
  EXPECT_EQ (after.count, before.count + 1);
  EXPECT_TRUE (after.last_thread == std::this_thread::get_id ());
}

TEST (LumexPermissionDiagnosticsTest,
      GivenAMissingNode_WhenTheNodeIsQueried_ThenNotFound)
{
  EXPECT_FALSE (dt::query_node_access ("/dev/ttyLUMEX_NO_SUCH_PORT").found);
}

TEST (LumexPermissionDiagnosticsTest,
      GivenAFileWithMode000_WhenDescribed_ThenThePathOwnerGroupAndModeAreNamed)
{
  temp_node const node (0);
  ASSERT_FALSE (node.path ().empty ());
  dt::node_access_t const access = dt::query_node_access (node.path ());
  ASSERT_TRUE (access.found);
  std::string const text = dt::describe_permission_denied (node.path ());
  EXPECT_TRUE (contains (text, node.path ())) << text;
  EXPECT_TRUE (contains (text, "mode 0000;")) << text;
  EXPECT_TRUE (contains (
      text, "owner "
                + (access.owner.empty () ? "uid " + std::to_string (access.uid)
                                         : access.owner)
                + ","))
      << text;
  EXPECT_TRUE (contains (
      text, "group "
                + (access.group.empty () ? "gid " + std::to_string (access.gid)
                                         : access.group)
                + ","))
      << text;
}
#endif

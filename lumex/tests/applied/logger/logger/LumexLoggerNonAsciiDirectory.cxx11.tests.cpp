// LumexLoggerNonAsciiDirectory.cxx11.tests.cpp
//
// The logger of a program that lives in a directory with a non-ASCII name
// (Windows). The directory of the logger (the trigger file is read from it and
// the log is written to it) came from GetModuleFileNameA, in the ANSI code
// page of the process: a Cyrillic directory was a string of '?', the trigger
// file was never found and no log was written. The test helpers of
// LumexLoggerTriggerFile.hpp read and write by UTF-8 path, so they have to
// work from such a directory too. The child is a copy of this binary and of
// its DLLs started from a Cyrillic folder (LumexTestNonAsciiDirectory.hpp);
// on other systems the file is empty.
#include <string>

#include <gtest/gtest.h>

#include "LumexLoggerTriggerFile.hpp"
#include "lumex/applied/logger/LumexLogger"
#include "lumex/tests/support/LumexTestNonAsciiDirectory.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if defined(_WIN32)
using namespace lumex::applied::logger::logger;
namespace trigger = lumex_test::logger_trigger;
namespace na = lumex_test::non_ascii;

namespace
{
// --- Access to the private directory function of the logger ---

template <typename Tag> struct slot
{
  static typename Tag::type value;
};

template <typename Tag>
typename Tag::type slot<Tag>::value = typename Tag::type ();

/// Stores `Member` in the slot of `Tag` when the program starts.
template <typename Tag, typename Tag::type Member> struct reveal
{
  reveal () { slot<Tag>::value = Member; }

  static reveal const instance;
};

template <typename Tag, typename Tag::type Member>
reveal<Tag, Member> const reveal<Tag, Member>::instance;

struct executable_directory_tag
{
  using type = std::string (LumexLogger::*) () const;
};

template struct reveal<executable_directory_tag,
                       &LumexLogger::_get_executable_directory>;
} // namespace

// Child: reports the directory of the logger and the directory of the test
// helper, one per line.
// Never runs by itself (DISABLED_).
TEST (LumexLoggerNonAsciiTest, DISABLED_ChildReportsItsDirectories)
{
  if (na::child_result_path ().empty ())
    GTEST_SKIP () << "started without a result file";
  LumexLogger &logger = LumexLogger::get_instance ();
  std::string const report = (logger.*slot<executable_directory_tag>::value) ()
                             + "\n" + trigger::logger_directory ();
  EXPECT_TRUE (na::write_child_result (report.data (), report.size ()));
}

// The logger reads its trigger file from and writes its log to this
// directory (the directory of its DLL, or of the program when it is linked
// statically); the helper has to name the same one. The end to end case (the
// trigger file in a Cyrillic directory enables the log) needs the narrow file
// calls of the logger moved to the wide API as well (branch narrow-paths).
TEST (
    LumexLoggerNonAsciiTest,
    GivenBinaryInCyrillicDirectory_WhenLoggerAsksItsDirectory_ThenItIsTheRealOne)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::wstring const binary = space.copy_binary ();
  ASSERT_FALSE (binary.empty ());

  DWORD const code
      = space.run (binary, L"--gtest_filter=LumexLoggerNonAsciiTest."
                           L"DISABLED_ChildReportsItsDirectories "
                           L"--gtest_also_run_disabled_tests");

  ASSERT_EQ (code, 0u) << space.child_log ();
  std::string report;
  ASSERT_TRUE (na::read_bytes (space.result_file (), report));
  std::string::size_type const newline = report.find ('\n');
  ASSERT_NE (newline, std::string::npos) << report;
  std::string const expected = na::utf8_of (space.directory ());
  EXPECT_EQ (report.substr (0, newline), expected)
      << "the directory of the logger";
  EXPECT_EQ (report.substr (newline + 1), expected)
      << "the directory of the test helper";
}

// The helpers themselves, in a directory that only the wide API can name.
TEST (LumexLoggerNonAsciiTest,
      GivenCyrillicDirectory_WhenHelpersWriteReadAndRemove_ThenTheyWork)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::string const directory = na::utf8_of (space.directory ()) + "\\"
                                + na::utf8_of (L"\u041F\u0430");
  std::string const file
      = directory + "\\" + na::utf8_of (L"\u0424\u0430\u0439\u043B.cfg");

  EXPECT_TRUE (trigger::make_directory (directory));
  EXPECT_TRUE (trigger::write_file (file, "LEVEL=debug\n"));
  EXPECT_TRUE (trigger::file_exists (file));
  EXPECT_EQ (trigger::read_file (file), "LEVEL=debug\n");
  EXPECT_TRUE (trigger::remove_file (file));
  EXPECT_FALSE (trigger::file_exists (file));
  EXPECT_TRUE (trigger::remove_directory (directory));
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

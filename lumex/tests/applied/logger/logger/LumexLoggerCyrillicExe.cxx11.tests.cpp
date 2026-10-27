// LumexLoggerCyrillicExe.cxx11.tests.cpp
//
// The logger finds its own directory (the DLL or the executable that holds it)
// and keeps its trigger file, its logs and the `Logger_logs` directory there.
// The paths are UTF-8, and a narrow Win32 call or stream read them in the ANSI
// code page, so a program under a Cyrillic directory (a user profile) lost
// the directory creation, the free-space and directory-size queries and the
// removal of old logs. The child is this test executable, copied with its DLLs
// into such a directory (Windows only).
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "LumexLoggerTriggerFile.hpp"
#include "lumex/applied/logger/LumexLogger"
#include "lumex/tests/support/LumexTestCyrillicCopy.hpp"
#include "lumex/tests/support/LumexTestNarrowSkip.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if defined(_WIN32)
using namespace lumex::applied::logger::logger;
namespace copy = lumex_test::cyrillic_copy;
namespace files = lumex_test::utf8_files;
namespace trigger = lumex_test::logger_trigger;

// The child: the logger is enabled by the trigger file the parent left
// (timestamped logs), logs, gets a log file of 3 GB (sparse) beside its own
// log, and logs again: the size check finds the directory over its limit and
// removes the oldest files.
TEST (LumexLoggerCyrillicExeTest, ChildLogsFromACyrillicDirectory)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  if (!copy::is_child ())
    GTEST_SKIP () << "runs only as the child of the test below";
  LumexLogger &logger = LumexLogger::get_instance ();
  ASSERT_TRUE (logger.is_logging_enabled ());
  // The test helper names the logger's directory with the wide API too; it
  // has to be the directory the child runs from.
  EXPECT_EQ (trigger::logger_directory (), copy::self_directory ());
  logger.log (LogLevel::LEVEL_ERROR, "cyrillic-exe-first-marker");
  ASSERT_TRUE (copy::make_sparse_file (
      copy::self_directory () + "/Logger_logs/" + files::cyrillic ("_big.log"),
      3LL * 1024 * 1024 * 1024));
  logger.log (LogLevel::LEVEL_ERROR, "cyrillic-exe-second-marker");
  logger.flush ();
}

TEST (LumexLoggerCyrillicExeTest,
      GivenExecutableInACyrillicDirectory_WhenLogging_ThenTheLogsAreKeptThere)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  if (copy::is_child ())
    GTEST_SKIP () << "the child runs the test above";
  if (!trigger::format_supported ())
    GTEST_SKIP () << "YAML trigger files are not readable yet";
  std::string directory;
  ASSERT_TRUE (copy::make_copy ("_logger", directory));
  ASSERT_TRUE (
      files::write_file (directory + "/enable_logs",
                         trigger::config_text ("debug", "TIMESTAMPED=true")));

  int const code = copy::run (
      directory, "LumexLoggerCyrillicExeTest.ChildLogsFromACyrillicDirectory");

  std::string const logs = directory + "/Logger_logs";
  std::vector<std::string> const names = copy::entries (logs);
  std::string text;
  for (std::size_t i = 0; i < names.size (); ++i)
    {
      std::string content;
      if (files::read_file (logs + "/" + names[i], content))
        text += content;
    }
  bool const big_left
      = files::exists (logs + "/" + files::cyrillic ("_big.log"));
  copy::remove_tree (directory);

  EXPECT_EQ (code, 0);
  // The directory is made by CreateDirectoryW and the log is opened in it.
  EXPECT_FALSE (names.empty ()) << "no Logger_logs directory or no log in it";
  EXPECT_NE (text.find ("cyrillic-exe-first-marker"), std::string::npos);
  // The old file is found (directory size, free space), counted and removed.
  EXPECT_FALSE (big_left) << "the old log was not purged";
}

// One file in the log directory has a name that is not valid UTF-16 (a lone
// surrogate, which NTFS accepts). The size check must still count it, by the
// size of the listing and without its name: when it returned "unknown" the
// limit and the purge were off for good. The child makes the 3 GB (sparse)
// odd file and a small old file, and logs again; the old file is purged only
// when the odd one was counted. (The odd file itself cannot be named, so it is
// never removed, and the purge may take other files with it.)
TEST (LumexLoggerCyrillicExeTest, ChildLogsBesideAnUnnameableFile)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  if (!copy::is_child ())
    GTEST_SKIP () << "runs only as the child of the test below";
  LumexLogger &logger = LumexLogger::get_instance ();
  ASSERT_TRUE (logger.is_logging_enabled ());
  logger.log (LogLevel::LEVEL_ERROR, "unnameable-first-marker");
  std::wstring odd
      = files::wide (copy::self_directory ()) + L"\\Logger_logs\\odd";
  odd.push_back (static_cast<wchar_t> (0xD800));
  ASSERT_TRUE (copy::make_sparse_file_wide (odd, 3LL * 1024 * 1024 * 1024));
  ASSERT_TRUE (copy::make_sparse_file (
      copy::self_directory () + "/Logger_logs/" + files::cyrillic ("_old.log"),
      1024));
  logger.log (LogLevel::LEVEL_ERROR, "unnameable-second-marker");
  logger.flush ();
}

TEST (
    LumexLoggerCyrillicExeTest,
    GivenFileWithUnnameableNameInTheLogDirectory_WhenLogging_ThenItIsCountedAndTheOldLogPurged)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  if (copy::is_child ())
    GTEST_SKIP () << "the child runs the test above";
  if (!trigger::format_supported ())
    GTEST_SKIP () << "YAML trigger files are not readable yet";
  std::string directory;
  ASSERT_TRUE (copy::make_copy ("_unnameable", directory));
  ASSERT_TRUE (
      files::write_file (directory + "/enable_logs",
                         trigger::config_text ("debug", "TIMESTAMPED=true")));

  int const code = copy::run (
      directory, "LumexLoggerCyrillicExeTest.ChildLogsBesideAnUnnameableFile");

  bool const old_left = files::exists (directory + "/Logger_logs/"
                                       + files::cyrillic ("_old.log"));
  copy::remove_tree (directory);

  EXPECT_EQ (code, 0);
  EXPECT_FALSE (old_left)
      << "the odd file was not counted: the size limit and the purge are off";
}

// The file helpers of LumexLoggerTriggerFile.hpp take UTF-8 paths: from a
// Cyrillic directory they write, read, find and remove the files. The
// directory is made with the wide API of the test support, not with the
// helper under test.
TEST (LumexLoggerCyrillicExeTest,
      GivenCyrillicDirectory_WhenTriggerHelpersWriteReadAndRemove_ThenTheyWork)
{
  std::string const directory
      = copy::self_directory () + "/" + files::cyrillic ("_helpers");
  copy::remove_tree (directory);
  ASSERT_TRUE (files::make_directory (directory));
  std::string const nested = directory + "/" + files::cyrillic ("_nested");
  std::string const file = nested + "/" + files::cyrillic (".cfg");

  bool const made = trigger::make_directory (nested);
  bool const written = trigger::write_file (file, "LEVEL=debug\n");
  bool const exists = trigger::file_exists (file);
  bool const system_sees_it = files::exists (file);
  std::string const content = trigger::read_file (file);
  bool const removed = trigger::remove_file (file);
  bool const gone = !files::exists (file);
  bool const directory_removed = trigger::remove_directory (nested);
  copy::remove_tree (directory);

  EXPECT_TRUE (made);
  EXPECT_TRUE (written);
  EXPECT_TRUE (exists);
  EXPECT_TRUE (system_sees_it) << "the file is not under the name asked for";
  EXPECT_EQ (content, "LEVEL=debug\n");
  EXPECT_TRUE (removed);
  EXPECT_TRUE (gone);
  EXPECT_TRUE (directory_removed);
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

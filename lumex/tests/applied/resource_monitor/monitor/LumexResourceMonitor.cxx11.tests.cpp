#include <chrono>
#include <csignal>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"
#include "lumex/core/filesystem/LumexFilesystem"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::applied::resource_monitor::monitor;

using lumex::applied::resource_monitor::monitor::LumexResourceMonitor;

namespace
{
// One unique scratch directory per test, so tests never step on each other's
// log files.
lumex::path
make_scratch_dir (std::string const &testName)
{
  lumex::path const dir
      = lumex::filesystem::temp_directory_path ().value ()
        / ("LumexResourceMonitorTests_" + testName + "_"
           + std::to_string (std::chrono::steady_clock::now ()
                                 .time_since_epoch ()
                                 .count ()));
  lumex::filesystem::create_directories (dir);
  return dir;
}

// The text of every file of a directory.
std::string
read_all_logs (lumex::path const &dir)
{
  std::string text;
  if (!lumex::filesystem::exists (dir))
    return text;
  for (lumex::directory_iterator it (dir), end; it != end; ++it)
    {
      if (!it->is_regular_file ())
        continue;
      std::ifstream in (it->path ().string ());
      std::ostringstream content;
      content << in.rdbuf ();
      text += content.str ();
    }
  return text;
}

// Waits until the logs have a complete line with `needle`, at most `limit`
// (the sampler first waits out its 2 s startup pause).
std::string
wait_for_log (lumex::path const &dir, std::string const &needle,
              std::chrono::milliseconds limit)
{
  auto const until = std::chrono::steady_clock::now () + limit;
  std::string text;
  do
    {
      std::this_thread::sleep_for (std::chrono::milliseconds (100));
      text = read_all_logs (dir);
    }
  while (text.find (needle) == std::string::npos
         && std::chrono::steady_clock::now () < until);
  return text;
}

bool
directory_has_any_file (lumex::path const &dir)
{
  if (!lumex::filesystem::exists (dir))
    return false;
  for (lumex::directory_iterator it (dir), end; it != end; ++it)
    if (it->is_regular_file ())
      return true;
  return false;
}
} // namespace

class LumexResourceMonitorTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    // Always leave the singleton sampler stopped, regardless of what the test
    // did with it.
    LumexResourceMonitor::stop ();
    if (!scratchDir.empty ())
      lumex::filesystem::remove_all (scratchDir);
  }

  lumex::path scratchDir;
};

TEST_F (LumexResourceMonitorTest, StopWithoutStartIsNoOp)
{
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
  // Calling it twice must also be safe.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
}

TEST_F (LumexResourceMonitorTest, StartIfEnabledCreatesTheLogDirectory)
{
  scratchDir = make_scratch_dir ("CreatesDir");
  auto const nestedDir = scratchDir / "nested" / "logs";
  ASSERT_FALSE (lumex::filesystem::exists (nestedDir));

  LumexResourceMonitor::start_if_enabled (nestedDir.string (),
                                          std::chrono::milliseconds (50));

  EXPECT_TRUE (lumex::filesystem::exists (nestedDir));
}

TEST_F (LumexResourceMonitorTest, SecondStartWhileRunningIsIgnored)
{
  scratchDir = make_scratch_dir ("DoubleStart");

  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::milliseconds (50));
  // Must not crash, deadlock, or replace the already-running sampler.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::start_if_enabled (
      scratchDir.string (), std::chrono::milliseconds (50)));
}

TEST_F (LumexResourceMonitorTest, NonPositivePollIntervalDoesNotCrash)
{
  scratchDir = make_scratch_dir ("NonPositiveInterval");
  // Zero/negative intervals are documented as falling back to the default
  // rather than busy-looping or misbehaving.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::start_if_enabled (
      scratchDir.string (), std::chrono::milliseconds (0)));
}

TEST_F (LumexResourceMonitorTest, DefaultPollIntervalArgumentCompilesAndRuns)
{
  scratchDir = make_scratch_dir ("DefaultInterval");
  // No explicit interval - exercises Constants::KDEFAULT_POLL_INTERVAL_MS.
  EXPECT_NO_FATAL_FAILURE (
      LumexResourceMonitor::start_if_enabled (scratchDir.string ()));
}

TEST_F (LumexResourceMonitorTest, StopIsSafeToCallRepeatedly)
{
  scratchDir = make_scratch_dir ("RepeatedStop");
  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::milliseconds (50));
  LumexResourceMonitor::stop ();
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
}

TEST_F (LumexResourceMonitorTest, RestartAfterStopStartsANewSamplerInstance)
{
  scratchDir = make_scratch_dir ("Restart");
  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::milliseconds (50));
  LumexResourceMonitor::stop ();

  // A second logDirectory - starting again after a clean stop must work, not
  // be silently ignored.
  auto const secondDir = scratchDir / "second";
  LumexResourceMonitor::start_if_enabled (secondDir.string (),
                                          std::chrono::milliseconds (50));
  EXPECT_TRUE (lumex::filesystem::exists (secondDir));
}

// stop () wakes the sampler: it neither sleeps out the startup pause (2 s) nor
// the rest of a poll interval. The budget is far below both.
TEST_F (LumexResourceMonitorTest, StopDuringTheStartupPauseReturnsAtOnce)
{
  scratchDir = make_scratch_dir ("StopDuringPause");
  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::seconds (60));

  auto const begin = std::chrono::steady_clock::now ();
  LumexResourceMonitor::stop ();
  auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::steady_clock::now () - begin);
  EXPECT_LT (elapsed.count (), 1000);
}

TEST_F (LumexResourceMonitorTest, StopBetweenSamplesReturnsAtOnce)
{
  scratchDir = make_scratch_dir ("StopBetweenSamples");
  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::seconds (60));
  // Past the startup pause: the sampler now waits for its first interval.
  std::this_thread::sleep_for (
      std::chrono::milliseconds (Constants::KSTARTUP_GRACE_PERIOD_MS + 500));

  auto const begin = std::chrono::steady_clock::now ();
  LumexResourceMonitor::stop ();
  auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::steady_clock::now () - begin);
  EXPECT_LT (elapsed.count (), 1000);
}

// Slower, end-to-end test: waits out the sampler's fixed startup grace period
// plus one poll interval and checks that a real sample line landed in the log
// file. Mirrors the style of the hardware module's own opt-in timing test (a
// few seconds is an acceptable budget for a background sampler integration
// test).
TEST_F (LumexResourceMonitorTest, ProducesAtLeastOneSampleLineAfterGracePeriod)
{
  scratchDir = make_scratch_dir ("ProducesSample");

  LumexResourceMonitor::start_if_enabled (scratchDir.string (),
                                          std::chrono::milliseconds (200));

  // Fixed ~2s startup grace period (see LumexResourceMonitor.cpp) + one poll
  // interval + slack.
  std::this_thread::sleep_for (std::chrono::milliseconds (3000));

  ASSERT_TRUE (directory_has_any_file (scratchDir));

  bool foundNonEmptyLine = false;
  for (lumex::directory_iterator it (scratchDir), end; it != end; ++it)
    {
      if (!it->is_regular_file ())
        continue;
      std::ifstream in (it->path ().string ());
      std::string line;
      while (std::getline (in, line))
        {
          if (!line.empty ())
            {
              foundNonEmptyLine = true;
              // Sanity-check the line looks like "<cpu>%/100%,
              // <used>Gb/<total>Gb".
              EXPECT_NE (line.find ("%/100%"), std::string::npos);
              EXPECT_NE (line.find ("Gb/"), std::string::npos);
            }
        }
    }
  EXPECT_TRUE (foundNonEmptyLine);
}

namespace
{
std::string
own_name_for_watching ()
{
#if defined(_WIN32)
  return std::string ();
#else
  return lumex::filesystem::read_symlink (lumex::path ("/proc/self/exe"))
      .value ()
      .filename ()
      .string ();
#endif
}
} // namespace

// A watch list adds one entry per item to the sample line: the process by its
// ID (with the usage), a process ID nothing has, and a name nothing has.
TEST_F (LumexResourceMonitorTest, WatchListEntriesFollowTheSystemPartOfTheLine)
{
  scratchDir = make_scratch_dir ("WatchEntries");
  using lumex::applied::resource_monitor::process::LumexProcessMonitor;
  using lumex::applied::resource_monitor::process::watch_list_t;
  LumexProcessMonitor::current_process_id ();

  watch_list_t list;
  list.pids.push_back (LumexProcessMonitor::current_process_id ());
  list.pids.push_back (4194303);
  list.names.push_back ("LumexNoProcessHasThisName12345");

  LumexResourceMonitor::start_with_watch_list (
      scratchDir.string (), list, std::chrono::milliseconds (200));
  std::string const text
      = wait_for_log (scratchDir, "not running", std::chrono::seconds (8));
  LumexResourceMonitor::stop ();

  std::string const self
      = "[" + std::to_string (LumexProcessMonitor::current_process_id ())
        + "] ";
  ASSERT_NE (text.find ("not running"), std::string::npos) << text;
  std::istringstream lines (text);
  std::string line;
  bool found = false;
  while (std::getline (lines, line))
    {
      if (line.find ("not running") == std::string::npos)
        continue;
      found = true;
      // "<time> <cpu>%/100%, <used>Gb/<total>Gb | name[pid] ... | 4194303
      // exited | LumexNoProcessHasThisName12345[x0] not running"
      EXPECT_NE (line.find ("%/100%, "), std::string::npos) << line;
      EXPECT_NE (line.find ("Gb/"), std::string::npos) << line;
      EXPECT_NE (line.find ("Gb | "), std::string::npos) << line;
      EXPECT_NE (line.find (self), std::string::npos) << line;
      EXPECT_NE (line.find (" | 4194303 exited | "), std::string::npos)
          << line;
      EXPECT_NE (line.find ("LumexNoProcessHasThisName12345[x0] not running"),
                 std::string::npos)
          << line;
      // The baseline was taken at the start: the first line has the share.
      EXPECT_EQ (line.find ("n/a"), std::string::npos) << line;
      break;
    }
  EXPECT_TRUE (found);
}

// An empty watch list is the plain sampler: the same line, no entries.
TEST_F (LumexResourceMonitorTest, AnEmptyWatchListLogsThePlainLine)
{
  scratchDir = make_scratch_dir ("EmptyWatchList");
  LumexResourceMonitor::start_with_watch_list (
      scratchDir.string (),
      lumex::applied::resource_monitor::process::watch_list_t (),
      std::chrono::milliseconds (200));
  std::string const text
      = wait_for_log (scratchDir, "Gb\n", std::chrono::seconds (8));
  LumexResourceMonitor::stop ();

  ASSERT_NE (text.find ("%/100%, "), std::string::npos) << text;
  EXPECT_EQ (text.find (" | "), std::string::npos) << text;
}

#if !defined(_WIN32)
// The process below a watched one is added to its entry (`[pid +1]`), and with
// the breakdown on, its own line comes under the sample line, indented.
TEST_F (LumexResourceMonitorTest,
        TheBreakdownListsTheProcessesBelowAWatchedOne)
{
  scratchDir = make_scratch_dir ("WatchBreakdown");
  using lumex::applied::resource_monitor::process::LumexProcessMonitor;
  using lumex::applied::resource_monitor::process::watch_list_t;

  pid_t const child = ::fork ();
  ASSERT_GE (child, 0);
  if (child == 0)
    {
      ::pause ();
      ::_exit (0);
    }
  watch_list_t list;
  list.pids.push_back (LumexProcessMonitor::current_process_id ());
  list.breakdown = true;

  LumexResourceMonitor::start_with_watch_list (
      scratchDir.string (), list, std::chrono::milliseconds (200));
  std::string const self
      = std::to_string (LumexProcessMonitor::current_process_id ());
  std::string const text
      = wait_for_log (scratchDir, "    " + std::to_string (child) + " ",
                      std::chrono::seconds (8));
  LumexResourceMonitor::stop ();
  ::kill (child, SIGKILL);
  int status = 0;
  ::waitpid (child, &status, 0);

  EXPECT_NE (text.find ("[" + self + " +"), std::string::npos) << text;
  // The line of the child starts with four spaces and its ID.
  EXPECT_NE (text.find ("\n    " + std::to_string (child) + " "),
             std::string::npos)
      << text;
  EXPECT_NE (
      text.find ("\n    " + self + " " + own_name_for_watching () + " "),
      std::string::npos)
      << text;
}

// The same list without the processes below: one process, no breakdown line.
TEST_F (LumexResourceMonitorTest, WithoutChildrenTheEntryIsOneProcess)
{
  scratchDir = make_scratch_dir ("WatchNoChildren");
  using lumex::applied::resource_monitor::process::LumexProcessMonitor;
  using lumex::applied::resource_monitor::process::watch_list_t;

  pid_t const child = ::fork ();
  ASSERT_GE (child, 0);
  if (child == 0)
    {
      ::pause ();
      ::_exit (0);
    }
  watch_list_t list;
  list.pids.push_back (LumexProcessMonitor::current_process_id ());
  list.include_children = false;
  list.breakdown = true;

  LumexResourceMonitor::start_with_watch_list (
      scratchDir.string (), list, std::chrono::milliseconds (200));
  std::string const self
      = std::to_string (LumexProcessMonitor::current_process_id ());
  std::string const text
      = wait_for_log (scratchDir, "[" + self + "] ", std::chrono::seconds (8));
  LumexResourceMonitor::stop ();
  ::kill (child, SIGKILL);
  int status = 0;
  ::waitpid (child, &status, 0);

  EXPECT_NE (text.find ("[" + self + "] "), std::string::npos) << text;
  EXPECT_EQ (text.find ("[" + self + " +"), std::string::npos) << text;
  EXPECT_EQ (text.find ("\n    "), std::string::npos) << text;
}
#endif

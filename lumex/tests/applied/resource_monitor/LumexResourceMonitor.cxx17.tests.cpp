#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"
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
std::filesystem::path
makeScratchDir (std::string const &testName)
{
  auto const dir = std::filesystem::temp_directory_path ()
                   / ("LumexResourceMonitorTests_" + testName + "_"
                      + std::to_string (std::chrono::steady_clock::now ()
                                            .time_since_epoch ()
                                            .count ()));
  std::filesystem::create_directories (dir);
  return dir;
}

bool
directoryHasAnyFile (std::filesystem::path const &dir)
{
  std::error_code ec;
  if (!std::filesystem::exists (dir, ec))
    return false;
  for (auto const &entry : std::filesystem::directory_iterator (dir, ec))
    if (entry.is_regular_file ())
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
    std::error_code ec;
    if (!scratchDir.empty ())
      std::filesystem::remove_all (scratchDir, ec);
  }

  std::filesystem::path scratchDir;
};

TEST_F (LumexResourceMonitorTest, StopWithoutStartIsNoOp)
{
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
  // Calling it twice must also be safe.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
}

TEST_F (LumexResourceMonitorTest, StartIfEnabledCreatesTheLogDirectory)
{
  scratchDir = makeScratchDir ("CreatesDir");
  auto const nestedDir = scratchDir / "nested" / "logs";
  ASSERT_FALSE (std::filesystem::exists (nestedDir));

  LumexResourceMonitor::startIfEnabled (nestedDir.string (),
                                        std::chrono::milliseconds (50));

  EXPECT_TRUE (std::filesystem::exists (nestedDir));
}

TEST_F (LumexResourceMonitorTest, SecondStartWhileRunningIsIgnored)
{
  scratchDir = makeScratchDir ("DoubleStart");

  LumexResourceMonitor::startIfEnabled (scratchDir.string (),
                                        std::chrono::milliseconds (50));
  // Must not crash, deadlock, or replace the already-running sampler.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::startIfEnabled (
      scratchDir.string (), std::chrono::milliseconds (50)));
}

TEST_F (LumexResourceMonitorTest, NonPositivePollIntervalDoesNotCrash)
{
  scratchDir = makeScratchDir ("NonPositiveInterval");
  // Zero/negative intervals are documented as falling back to the default
  // rather than busy-looping or misbehaving.
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::startIfEnabled (
      scratchDir.string (), std::chrono::milliseconds (0)));
}

TEST_F (LumexResourceMonitorTest, DefaultPollIntervalArgumentCompilesAndRuns)
{
  scratchDir = makeScratchDir ("DefaultInterval");
  // No explicit interval - exercises Constants::KDEFAULT_POLL_INTERVAL_MS.
  EXPECT_NO_FATAL_FAILURE (
      LumexResourceMonitor::startIfEnabled (scratchDir.string ()));
}

TEST_F (LumexResourceMonitorTest, StopIsSafeToCallRepeatedly)
{
  scratchDir = makeScratchDir ("RepeatedStop");
  LumexResourceMonitor::startIfEnabled (scratchDir.string (),
                                        std::chrono::milliseconds (50));
  LumexResourceMonitor::stop ();
  EXPECT_NO_FATAL_FAILURE (LumexResourceMonitor::stop ());
}

TEST_F (LumexResourceMonitorTest, RestartAfterStopStartsANewSamplerInstance)
{
  scratchDir = makeScratchDir ("Restart");
  LumexResourceMonitor::startIfEnabled (scratchDir.string (),
                                        std::chrono::milliseconds (50));
  LumexResourceMonitor::stop ();

  // A second logDirectory - starting again after a clean stop must work, not
  // be silently ignored.
  auto const secondDir = scratchDir / "second";
  LumexResourceMonitor::startIfEnabled (secondDir.string (),
                                        std::chrono::milliseconds (50));
  EXPECT_TRUE (std::filesystem::exists (secondDir));
}

// Slower, end-to-end test: waits out the sampler's fixed startup grace period
// plus one poll interval and checks that a real sample line landed in the log
// file. Mirrors the style of the hardware module's own opt-in timing test (a
// few seconds is an acceptable budget for a background sampler integration
// test).
TEST_F (LumexResourceMonitorTest, ProducesAtLeastOneSampleLineAfterGracePeriod)
{
  scratchDir = makeScratchDir ("ProducesSample");

  LumexResourceMonitor::startIfEnabled (scratchDir.string (),
                                        std::chrono::milliseconds (200));

  // Fixed ~2s startup grace period (see LumexResourceMonitor.cpp) + one poll
  // interval + slack.
  std::this_thread::sleep_for (std::chrono::milliseconds (3000));

  ASSERT_TRUE (directoryHasAnyFile (scratchDir));

  bool foundNonEmptyLine = false;
  for (auto const &entry : std::filesystem::directory_iterator (scratchDir))
    {
      if (!entry.is_regular_file ())
        continue;
      std::ifstream in (entry.path ());
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

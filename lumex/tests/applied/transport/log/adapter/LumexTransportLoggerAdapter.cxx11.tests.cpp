// LumexTransportLoggerAdapter.cxx11.tests.cpp: install_lumex_logger_sink
// (records of the transport log reach the LumexLogger file at the mapped
// level; the transport threshold follows the logger's level through every
// re-read of the trigger file) and lumex_logger_watch_timer (the re-read on a
// timer of an io_runner).
//
// Every test runs in its own process, enables the logger through a trigger
// file of its own name and holds the CTest RESOURCE_LOCK
// lumex_logger_trigger_file (see CMakeLists.txt) while it writes the log file
// next to the logger.

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/log/adapter/LumexTransportLoggerAdapter.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/tests/applied/logger/logger/LumexLoggerTriggerFile.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace tlog = lumex::applied::transport::log;
namespace trigger = lumex_test::logger_trigger;
using lumex::applied::logger::logger::LogLevel;

namespace
{
char const *const kTriggerName = "lumex_transport_adapter_test_trigger";
char const *const kComponent = "transport.adapter_test";

/// The level other_callback heard last.
int g_other_level = -1;

void
other_callback (bool, LogLevel level)
{
  g_other_level = static_cast<int> (level) + 100;
}

/// Two callbacks of other binaries that disagree on the level (review N8).
void
wants_debug (bool, LogLevel level)
{
  if (level != LogLevel::LEVEL_DEBUG)
    LumexLogger::get_instance ().set_log_level (LogLevel::LEVEL_DEBUG);
}

void
wants_info (bool, LogLevel level)
{
  if (level != LogLevel::LEVEL_INFO)
    LumexLogger::get_instance ().set_log_level (LogLevel::LEVEL_INFO);
}

/// The level each filler callback heard last (a body of its own: identical
/// empty functions may be folded into one address by the linker).
std::vector<int> &
filler_levels ()
{
  static std::vector<int> levels (8, -1);
  return levels;
}

/// Seven callbacks of other binaries, with other_callback eight.
template <int Index>
void
filler_callback (bool, LogLevel level)
{
  filler_levels ()[Index] = static_cast<int> (level);
}

std::vector<LumexLogger::level_change_callback_t> const &
filler_callbacks ()
{
  static std::vector<LumexLogger::level_change_callback_t> const callbacks
      = { &filler_callback<0>, &filler_callback<1>, &filler_callback<2>,
          &filler_callback<3>, &filler_callback<4>, &filler_callback<5>,
          &filler_callback<6> };
  return callbacks;
}

/// Polls `condition` until it holds or `timeout` passes.
template <typename Condition>
bool
eventually (Condition condition, std::chrono::milliseconds timeout)
{
  std::chrono::steady_clock::time_point const deadline
      = std::chrono::steady_clock::now () + timeout;
  while (!condition ())
    {
      if (std::chrono::steady_clock::now () >= deadline)
        return false;
      std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
  return true;
}

bool
contains (std::string const &text, std::string const &part)
{
  return text.find (part) != std::string::npos;
}
} // namespace

class LumexTransportLoggerAdapterTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    if (!trigger::format_supported ())
      GTEST_SKIP () << "YAML trigger files are not readable yet";
    logger_ = &LumexLogger::get_instance ();
    trigger_path_ = trigger::path_in_logger_directory (kTriggerName);
    ASSERT_TRUE (trigger::remove_file (trigger_path_));
    logger_->set_trigger_file_name (kTriggerName);
    logger_->force_reread ();
    ASSERT_FALSE (logger_->is_logging_enabled ());
    tlog::set_log_sink (nullptr);
    tlog::set_log_level (tlog::log_level::warning);
  }

  void
  TearDown () override
  {
    if (logger_ == nullptr)
      return;
    tlog::uninstall_lumex_logger_sink ();
    logger_->remove_level_change_callback (&other_callback);
    logger_->remove_level_change_callback (&wants_debug);
    logger_->remove_level_change_callback (&wants_info);
    for (LumexLogger::level_change_callback_t const callback :
         filler_callbacks ())
      logger_->remove_level_change_callback (callback);
    trigger::remove_file (trigger_path_);
    logger_->force_reread ();
    logger_->set_trigger_file_name ("enable_logs");
    tlog::set_log_level (tlog::log_level::warning);
  }

  /// Writes the trigger file and re-reads it. Two writes in a row differ in
  /// size (the check compares modification time and size, and a time may
  /// repeat within one clock tick).
  void
  reread_with (std::string const &level)
  {
    ASSERT_TRUE (
        trigger::write_file (trigger_path_, trigger::config_text (level)));
    logger_->force_reread ();
  }

  LumexLogger *logger_ = nullptr;
  std::string trigger_path_;
};

// --- Records ----------------------------------------------------------------

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTheLoggerSink_WhenTheTransportLogs_ThenEachRecordReachesTheFileAtItsLevel)
{
  reread_with ("trace");
  tlog::install_lumex_logger_sink ();
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::trace);

  std::vector<std::uint8_t> const bytes = { 0x41, 0x0D, 0x0A };
  LUMEX_TRANSPORT_LOG (trace, kComponent, "COM9",
                       std::string ("adapter-trace-marker"),
                       tlog::log_bytes_t (bytes));
  LUMEX_TRANSPORT_LOG (debug, kComponent, "COM9",
                       std::string ("adapter-debug-marker"),
                       tlog::log_bytes_t ());
  LUMEX_TRANSPORT_LOG (info, kComponent, "",
                       std::string ("adapter-info-marker"),
                       tlog::log_bytes_t ());
  LUMEX_TRANSPORT_LOG (warning, kComponent, "COM9",
                       std::string ("adapter-warning-marker"),
                       tlog::log_bytes_t ());
  LUMEX_TRANSPORT_LOG (error, kComponent, "COM9",
                       std::string ("adapter-error-marker"),
                       tlog::log_bytes_t ());
  logger_->flush ();

  std::string const text = trigger::read_file (logger_->get_log_file_path ());
  std::string const prefix = std::string ("[") + kComponent + " COM9] ";
  EXPECT_TRUE (contains (text, "[TRACE] " + prefix
                                   + "adapter-trace-marker | 41 0D 0A "
                                     "|A<CR><LF>|"))
      << text;
  EXPECT_TRUE (contains (text, "[DEBUG] " + prefix + "adapter-debug-marker"));
  EXPECT_TRUE (contains (text, std::string ("[INFO ] [") + kComponent
                                   + "] adapter-info-marker"));
  EXPECT_TRUE (
      contains (text, "[WARNING] " + prefix + "adapter-warning-marker"));
  EXPECT_TRUE (contains (text, "[ERROR] " + prefix + "adapter-error-marker"));
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTheLoggerSink_WhenTheLoggerFiltersALevel_ThenTheRecordIsNotWritten)
{
  reread_with ("warning");
  tlog::install_lumex_logger_sink ();
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::warning);
  EXPECT_FALSE (tlog::log_enabled (tlog::log_level::info, kComponent));

  LUMEX_TRANSPORT_LOG (info, kComponent, "COM9",
                       std::string ("adapter-filtered-marker"),
                       tlog::log_bytes_t ());
  // Even past the transport threshold, the logger keeps its own filter.
  tlog::log_write (tlog::log_level::debug, kComponent, "COM9",
                   "adapter-below-logger-marker");
  logger_->flush ();

  std::string const text = trigger::read_file (logger_->get_log_file_path ());
  EXPECT_FALSE (contains (text, "adapter-filtered-marker"));
  EXPECT_FALSE (contains (text, "adapter-below-logger-marker"));
}

// --- The threshold follows the logger ---------------------------------------

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTheLoggerSink_WhenReReadsChangeTheLoggerLevel_ThenTheThresholdFollows)
{
  reread_with ("warning");
  tlog::install_lumex_logger_sink ();
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::warning);

  reread_with ("debug"); // lowers the logger level
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::debug);
  EXPECT_TRUE (tlog::log_enabled (tlog::log_level::debug, kComponent));

  reread_with ("success"); // no transport level: info is below it
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::warning);

  reread_with ("fatal"); // above every transport level
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::off);

  ASSERT_TRUE (trigger::remove_file (trigger_path_));
  logger_->force_reread (); // logger disabled
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::off);

  reread_with ("info"); // the file is back
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::info);
}

TEST_F (LumexTransportLoggerAdapterTest,
        GivenADisabledLogger_WhenTheSinkIsInstalled_ThenTheThresholdIsOff)
{
  tlog::set_log_level (tlog::log_level::trace);
  tlog::install_lumex_logger_sink ();
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::off);
  EXPECT_FALSE (tlog::log_enabled (tlog::log_level::error, kComponent));

  tlog::install_lumex_logger_sink (); // again: nothing changes
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::off);
  EXPECT_NE (tlog::get_log_sink (), nullptr);
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTheLoggerSink_WhenTheLevelIsSetThroughTheApi_ThenTheThresholdFollows)
{
  reread_with ("info");
  tlog::install_lumex_logger_sink ();
  ASSERT_EQ (tlog::get_log_level (), tlog::log_level::info);

  // Review R1: set_log_level, not only a re-read, moves the threshold.
  logger_->set_log_level (LogLevel::LEVEL_TRACE);
  LUMEX_TRANSPORT_LOG (trace, kComponent, "COM9",
                       std::string ("adapter-api-trace-marker"),
                       tlog::log_bytes_t ());
  logger_->flush ();

  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::trace);
  EXPECT_TRUE (contains (trigger::read_file (logger_->get_log_file_path ()),
                         "adapter-api-trace-marker"));

  logger_->set_log_level (LogLevel::LEVEL_FATAL);
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::off);
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTwoBinariesWithTheSink_WhenTheFileAppears_ThenBothThresholdsFollow)
{
  tlog::install_lumex_logger_sink (); // this binary, logger disabled: off
  ASSERT_EQ (tlog::get_log_level (), tlog::log_level::off);
  // Review R2: a DLL that links the static transport installs its own
  // adapter, whose callback is another function.
  ASSERT_TRUE (logger_->add_level_change_callback (&other_callback));

  reread_with ("trace");
  ASSERT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::trace)
      << "the callback of the other binary replaced this one's";
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenAFullCallbackTable_WhenTheSinkIsInstalled_ThenItSaysSoAndSetsTheLevelOnce)
{
  reread_with ("info");
  ASSERT_TRUE (logger_->add_level_change_callback (&other_callback));
  for (LumexLogger::level_change_callback_t const callback :
       filler_callbacks ())
    ASSERT_TRUE (logger_->add_level_change_callback (callback));

  EXPECT_FALSE (tlog::install_lumex_logger_sink ());
  EXPECT_NE (tlog::get_log_sink (), nullptr);
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::info);
  reread_with ("debug"); // not followed: the table is full
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::info);

  EXPECT_TRUE (logger_->remove_level_change_callback (&other_callback));
  EXPECT_TRUE (tlog::install_lumex_logger_sink ());
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::debug);
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenATransportOnlyProgramWithAWatchTimer_WhenTheFileAppears_ThenItLogs)
{
  // Review R3: the threshold is off, so no transport record reaches the
  // logger and its lazy check; the timer notices the file.
  ASSERT_TRUE (tlog::install_lumex_logger_sink ());
  ASSERT_EQ (tlog::get_log_level (), tlog::log_level::off);
  lumex::applied::transport::stream::io_runner runner (1);
  tlog::lumex_logger_watch_timer const timer (runner,
                                              std::chrono::milliseconds (20));

  ASSERT_TRUE (
      trigger::write_file (trigger_path_, trigger::config_text ("info")));
  EXPECT_TRUE (eventually (
      [] () { return tlog::get_log_level () == tlog::log_level::info; },
      std::chrono::milliseconds (5000)));
  LUMEX_TRANSPORT_LOG (info, kComponent, "COM9",
                       std::string ("adapter-transport-only-marker"),
                       tlog::log_bytes_t ());
  logger_->flush ();
  EXPECT_TRUE (contains (trigger::read_file (logger_->get_log_file_path ()),
                         "adapter-transport-only-marker"));
}

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTwoCallbacksThatDisagree_WhenTheLevelIsSet_ThenTheThresholdIsTheFinalLevel)
{
  reread_with ("info");
  ASSERT_TRUE (tlog::install_lumex_logger_sink ());
  ASSERT_TRUE (logger_->add_level_change_callback (&wants_debug));
  ASSERT_TRUE (logger_->add_level_change_callback (&wants_info));

  // Review N8: the rounds end, and the adapter heard the final level last.
  logger_->set_log_level (LogLevel::LEVEL_ERROR);
  LogLevel const level = logger_->get_log_level ();
  EXPECT_TRUE (level == LogLevel::LEVEL_DEBUG || level == LogLevel::LEVEL_INFO)
      << log_level_to_string (level);
  EXPECT_EQ (tlog::get_log_level (), level == LogLevel::LEVEL_DEBUG
                                         ? tlog::log_level::debug
                                         : tlog::log_level::info);
}

// --- Uninstall --------------------------------------------------------------

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenTheLoggerSink_WhenUninstalled_ThenTheDefaultSinkIsBackAndTheLevelStays)
{
  reread_with ("warning");
  tlog::install_lumex_logger_sink ();
  ASSERT_NE (tlog::get_log_sink (), nullptr);

  tlog::uninstall_lumex_logger_sink ();
  EXPECT_EQ (tlog::get_log_sink (), nullptr);
  reread_with ("debug");
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::warning);
}

TEST_F (LumexTransportLoggerAdapterTest,
        GivenAnotherSinkAndCallback_WhenUninstalled_ThenTheyStay)
{
  tlog::install_lumex_logger_sink ();
  ASSERT_TRUE (logger_->add_level_change_callback (&other_callback));
  tlog::set_log_sink ([] (tlog::log_record_t const &) {});

  tlog::uninstall_lumex_logger_sink ();
  EXPECT_NE (tlog::get_log_sink (), nullptr);
  EXPECT_TRUE (logger_->remove_level_change_callback (&other_callback));
}

// --- lumex_logger_watch_timer -----------------------------------------------

TEST_F (
    LumexTransportLoggerAdapterTest,
    GivenAWatchTimer_WhenTheFileChanges_ThenTheRunnerAppliesItUntilTheTimerEnds)
{
  reread_with ("warning");
  tlog::install_lumex_logger_sink ();
  lumex::applied::transport::stream::io_runner runner (1);
  {
    tlog::lumex_logger_watch_timer timer (runner,
                                          std::chrono::milliseconds (20));
    EXPECT_TRUE (timer.is_running ());
    EXPECT_EQ (timer.period (), std::chrono::milliseconds (20));

    // Nothing in this thread calls the logger: only the timer re-reads.
    ASSERT_TRUE (
        trigger::write_file (trigger_path_, trigger::config_text ("debug")));
    EXPECT_TRUE (eventually (
        [] () { return tlog::get_log_level () == tlog::log_level::debug; },
        std::chrono::milliseconds (5000)));
  }

  // After the destructor no check of the timer runs.
  ASSERT_TRUE (
      trigger::write_file (trigger_path_, trigger::config_text ("warning")));
  std::this_thread::sleep_for (std::chrono::milliseconds (200));
  EXPECT_EQ (tlog::get_log_level (), tlog::log_level::debug);
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
}

TEST_F (LumexTransportLoggerAdapterTest,
        GivenAWatchTimer_WhenMoved_ThenTheNewOwnerRunsIt)
{
  lumex::applied::transport::stream::io_runner runner (2);
  tlog::lumex_logger_watch_timer first (runner, std::chrono::milliseconds (0));
  EXPECT_EQ (first.period (), std::chrono::milliseconds (1));

  tlog::lumex_logger_watch_timer second (std::move (first));
  EXPECT_FALSE (first.is_running ()); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ (first.period (), std::chrono::milliseconds (0));
  EXPECT_TRUE (second.is_running ());

  tlog::lumex_logger_watch_timer third (runner);
  EXPECT_EQ (third.period (), std::chrono::milliseconds (1000));
  third = std::move (second);
  EXPECT_TRUE (third.is_running ());
  EXPECT_EQ (third.period (), std::chrono::milliseconds (1));

  // Checks run on two threads every millisecond while the file comes.
  ASSERT_TRUE (
      trigger::write_file (trigger_path_, trigger::config_text ("info")));
  EXPECT_TRUE (eventually ([this] ()
                             { return logger_->is_logging_enabled (); },
                           std::chrono::milliseconds (5000)));
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

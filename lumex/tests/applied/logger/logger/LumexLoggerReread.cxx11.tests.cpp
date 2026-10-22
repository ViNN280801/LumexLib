// LumexLoggerReread.cxx11.tests.cpp: the run-time re-read of the trigger
// file (spec "Logging", decision B): the lazy check of log () and
// is_level_enabled () (at most once per 2 s, before the level filter),
// force_reread (), the level change callback and trigger_file_watcher.
//
// Every test runs in its own process (gtest_discover_tests), so the singleton
// starts fresh; the fixture still brings it to "disabled" first, whatever an
// earlier test or the machine left. The trigger file has a name of its own
// (never `enable_logs`), and the tests that write the logger's log file next
// to the logger hold the CTest RESOURCE_LOCK lumex_logger_trigger_file (see
// CMakeLists.txt), so parallel ctest runs do not share it.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// clang-format off
#include <tlhelp32.h>
// clang-format on
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "LumexLoggerTriggerFile.hpp"
#include "lumex/applied/logger/LumexLogger"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::applied::logger::logger;
namespace trigger = lumex_test::logger_trigger;

namespace
{
char const *const kTriggerName = "lumex_logger_reread_test_trigger";

// --- What the level change callback saw ------------------------------------

struct callback_call_t
{
  bool enabled;
  LogLevel level;
  std::thread::id thread;
  unsigned long os_thread;
};

struct callback_log_t
{
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<callback_call_t> calls;
};

callback_log_t &
callback_log ()
{
  static callback_log_t log;
  return log;
}

unsigned long
current_os_thread_id ()
{
#if defined(_WIN32)
  return static_cast<unsigned long> (GetCurrentThreadId ());
#elif defined(__linux__)
  return static_cast<unsigned long> (syscall (SYS_gettid));
#else
  return 0;
#endif
}

void
record_level_change (bool enabled, LogLevel level)
{
  callback_log_t &log = callback_log ();
  callback_call_t const call = { enabled, level, std::this_thread::get_id (),
                                 current_os_thread_id () };
  {
    std::lock_guard<std::mutex> const lock (log.mutex);
    log.calls.push_back (call);
  }
  log.changed.notify_all ();
}

std::vector<callback_call_t>
recorded_calls ()
{
  callback_log_t &log = callback_log ();
  std::lock_guard<std::mutex> const lock (log.mutex);
  return log.calls;
}

void
clear_recorded_calls ()
{
  callback_log_t &log = callback_log ();
  std::lock_guard<std::mutex> const lock (log.mutex);
  log.calls.clear ();
}

void
other_callback (bool, LogLevel)
{
}

/// A callback that raises a TRACE level to ERROR through the API.
void
raising_callback (bool enabled, LogLevel level)
{
  record_level_change (enabled, level);
  if (enabled && level == LogLevel::LEVEL_TRACE)
    LumexLogger::get_instance ().set_log_level (LogLevel::LEVEL_ERROR);
}

/// The last level `last_level_callback` heard.
std::atomic<int> &
last_heard_level ()
{
  static std::atomic<int> level (-1);
  return level;
}

void
last_level_callback (bool, LogLevel level)
{
  last_heard_level ().store (static_cast<int> (level));
}

/// Set while slow_callback runs; slow_callback calls counted.
std::atomic<int> &
slow_inside ()
{
  static std::atomic<int> inside (0);
  return inside;
}

std::atomic<int> &
slow_calls ()
{
  static std::atomic<int> calls (0);
  return calls;
}

/// A callback that takes 300 ms (review N5).
void
slow_callback (bool, LogLevel)
{
  slow_inside ().store (1);
  slow_calls ().fetch_add (1);
  std::this_thread::sleep_for (std::chrono::milliseconds (300));
  slow_inside ().store (0);
}

std::atomic<int> &
reentrant_calls ()
{
  static std::atomic<int> calls (0);
  return calls;
}

/// A callback that forces a re-read (review N9): returns at once there.
void
reentrant_callback (bool, LogLevel)
{
  reentrant_calls ().fetch_add (1);
  LumexLogger::get_instance ().force_reread ();
}

/// How often each of the 8 callbacks of the table test was called.
std::vector<int> &
table_calls ()
{
  static std::vector<int> calls (8, 0);
  return calls;
}

/// One of 8 distinct callbacks for the table test.
template <int Index>
void
table_callback (bool, LogLevel)
{
  ++table_calls ()[Index];
}

/// A callback that logs, which the logger allows.
void
logging_callback (bool enabled, LogLevel level)
{
  record_level_change (enabled, level);
  LumexLogger::get_instance ().log (LogLevel::LEVEL_WARNING,
                                    "reread-callback-log-marker");
}

/// Waits until the callback was called from a thread other than this one.
bool
wait_for_call_from_another_thread (callback_call_t &found,
                                   std::chrono::milliseconds timeout)
{
  callback_log_t &log = callback_log ();
  std::thread::id const self = std::this_thread::get_id ();
  std::unique_lock<std::mutex> lock (log.mutex);
  return log.changed.wait_for (lock, timeout,
                               [&log, &found, self] ()
                                 {
                                   for (callback_call_t const &call :
                                        log.calls)
                                     if (call.thread != self)
                                       {
                                         found = call;
                                         return true;
                                       }
                                   return false;
                                 });
}

/// True while the operating system lists the thread in this process.
bool
os_thread_alive (unsigned long id)
{
#if defined(_WIN32)
  HANDLE const snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return false;
  THREADENTRY32 entry;
  entry.dwSize = sizeof (entry);
  bool alive = false;
  DWORD const process = GetCurrentProcessId ();
  for (BOOL more = Thread32First (snapshot, &entry); more && !alive;
       more = Thread32Next (snapshot, &entry))
    alive = entry.th32OwnerProcessID == process
            && static_cast<unsigned long> (entry.th32ThreadID) == id;
  CloseHandle (snapshot);
  return alive;
#elif defined(__linux__)
  return trigger::file_exists ("/proc/self/task/" + std::to_string (id)
                               + "/stat");
#else
  (void)id;
  return false;
#endif
}

bool
os_thread_check_supported ()
{
#if defined(_WIN32) || defined(__linux__)
  return true;
#else
  return false;
#endif
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

// --- Fixture ----------------------------------------------------------------

class LumexLoggerRereadTest : public ::testing::Test
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
    clear_recorded_calls ();
  }

  void
  TearDown () override
  {
    if (logger_ == nullptr)
      return;
    logger_->remove_level_change_callback (&record_level_change);
    logger_->remove_level_change_callback (&logging_callback);
    logger_->remove_level_change_callback (&other_callback);
    logger_->remove_level_change_callback (&raising_callback);
    logger_->remove_level_change_callback (&last_level_callback);
    logger_->remove_level_change_callback (&slow_callback);
    logger_->remove_level_change_callback (&reentrant_callback);
    trigger::remove_file (trigger_path_);
    logger_->force_reread ();
    logger_->set_trigger_file_name ("enable_logs");
  }

  /// Writes the trigger file with `level` (and `extra` lines) and checks it.
  /// Two writes in a row differ in size: a file system may keep the same
  /// modification time for writes a few milliseconds apart (NTFS updates it
  /// with the system clock tick), and the check compares time and size.
  void
  write_trigger (std::string const &level, std::string const &extra = "")
  {
    ASSERT_TRUE (trigger::write_file (trigger_path_,
                                      trigger::config_text (level, extra)));
  }

  /// Enables the logger through the trigger file at `level`.
  void
  enable_at (std::string const &level, std::string const &extra = "")
  {
    write_trigger (level, extra);
    logger_->force_reread ();
    ASSERT_TRUE (logger_->is_logging_enabled ());
  }

  LumexLogger *logger_ = nullptr;
  std::string trigger_path_;
};

// --- Appear / disappear -----------------------------------------------------

TEST_F (
    LumexLoggerRereadTest,
    GivenNoTriggerFile_WhenTheFileAppearsAndIsReread_ThenTheLogIsOpenedAsAtStart)
{
  write_trigger ("debug");
  logger_->force_reread ();

  ASSERT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  EXPECT_TRUE (logger_->is_level_enabled (LogLevel::LEVEL_DEBUG));
  EXPECT_FALSE (logger_->is_level_enabled (LogLevel::LEVEL_TRACE));
  std::string const path = logger_->get_log_file_path ();
  ASSERT_FALSE (path.empty ());

  logger_->log (LogLevel::LEVEL_DEBUG, "reread-appear-marker");
  logger_->flush ();
  std::string const text = trigger::read_file (path);
  EXPECT_TRUE (contains (text, "=== Log start of the program ==="));
  EXPECT_TRUE (contains (text, std::string ("Logging enabled by file: ")
                                   + kTriggerName));
  EXPECT_TRUE (contains (text, "Log level from trigger-file: LEVEL_DEBUG"));
  EXPECT_TRUE (contains (text, "reread-appear-marker"));
}

TEST_F (
    LumexLoggerRereadTest,
    GivenAnEnabledLogger_WhenTheTriggerFileDisappears_ThenTheLogIsClosedAndKept)
{
  enable_at ("info");
  std::string const path = logger_->get_log_file_path ();
  logger_->log (LogLevel::LEVEL_INFO, "reread-before-close-marker");

  ASSERT_TRUE (trigger::remove_file (trigger_path_));
  logger_->force_reread ();

  EXPECT_FALSE (logger_->is_logging_enabled ());
  EXPECT_FALSE (logger_->is_level_enabled (LogLevel::LEVEL_FATAL));
  EXPECT_TRUE (logger_->get_log_file_path ().empty ());
  logger_->log (LogLevel::LEVEL_ERROR, "reread-after-close-marker");

  ASSERT_TRUE (trigger::file_exists (path))
      << "the log is closed, not deleted";
  std::string const text = trigger::read_file (path);
  EXPECT_TRUE (contains (text, "reread-before-close-marker"));
  EXPECT_TRUE (contains (
      text, "=== Logging disabled: the trigger file was removed ==="));
  EXPECT_TRUE (contains (text, "Active time: "));
  EXPECT_FALSE (contains (text, "reread-after-close-marker"));
  // Windows refuses to delete a file a stream still holds open: the log is
  // closed. Elsewhere the removal only cleans up.
  EXPECT_TRUE (trigger::remove_file (path));
}

TEST_F (
    LumexLoggerRereadTest,
    GivenADisabledLogger_WhenTheTriggerFileAppearsAgain_ThenANewLogIsStarted)
{
  enable_at ("info");
  std::string const path = logger_->get_log_file_path ();
  logger_->log (LogLevel::LEVEL_INFO, "reread-first-run-marker");
  ASSERT_TRUE (trigger::remove_file (trigger_path_));
  logger_->force_reread ();
  ASSERT_FALSE (logger_->is_logging_enabled ());

  enable_at ("warning");

  // Plain mode: the same name, truncated as at start.
  EXPECT_EQ (logger_->get_log_file_path (), path);
  logger_->flush ();
  std::string const text = trigger::read_file (path);
  EXPECT_TRUE (contains (text, "Log level from trigger-file: LEVEL_WARNING"));
  EXPECT_FALSE (contains (text, "reread-first-run-marker"));
}

// --- A change of the file ---------------------------------------------------

TEST_F (
    LumexLoggerRereadTest,
    GivenAnEnabledLogger_WhenTheTriggerFileChangesItsLevel_ThenOneCheckAppliesIt)
{
  enable_at ("info");
  EXPECT_FALSE (logger_->is_level_enabled (LogLevel::LEVEL_DEBUG));

  // Another size as well as another time: the check sees it also where the
  // file system keeps coarse times.
  write_trigger ("trace");
  logger_->force_reread ();

  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_TRACE);
  EXPECT_TRUE (logger_->is_level_enabled (LogLevel::LEVEL_TRACE));
  logger_->flush ();
  EXPECT_TRUE (
      contains (trigger::read_file (logger_->get_log_file_path ()),
                "=== Trigger file re-read, log level: LEVEL_TRACE ==="));
}

TEST_F (LumexLoggerRereadTest,
        GivenAChangedTriggerFile_WhenReread_ThenEverySettingOfTheFileIsApplied)
{
  enable_at ("info");
  ASSERT_FALSE (logger_->is_buffering_enabled ());

  write_trigger ("debug", "FUNCNAME=NORMAL\n"
                          "STACKTRACE=true\n"
                          "STACKTRACE_FRAMES=12\n"
                          "PRESET=NetworkManager,DatabaseConnection\n"
                          "BUFFERING=true\n"
                          "BUFFER_SIZE=40\n"
                          "BUFFERING_TRIGGER=ERROR\n"
                          "DESCRIBE_FRAME=true\n");
  logger_->force_reread ();

  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  EXPECT_EQ (logger_->get_function_name_mode (), FunctionNameMode::NORMAL);
  EXPECT_TRUE (logger_->is_preset_enabled ());
  EXPECT_EQ (logger_->get_preset_components ().size (), 2U);
  EXPECT_TRUE (logger_->is_buffering_enabled ());
  EXPECT_EQ (logger_->get_buffer_size (), 40U);
  EXPECT_TRUE (logger_->is_describe_frame_enabled ());
  logger_applied_config_view_t const view
      = logger_->get_applied_config_view ();
  EXPECT_TRUE (view.show_stack_trace_in_messages);
  EXPECT_EQ (view.stack_trace_max_frames, 12);
  EXPECT_TRUE (view.log_buffer_flush_level_configured);
  EXPECT_EQ (view.log_buffer_flush_trigger_level, LogLevel::LEVEL_ERROR);

  // A buffered record, then a file without those settings: buffering off
  // writes the buffer, the preset and the other flags go back to defaults.
  logger_->log (LogLevel::LEVEL_INFO,
                "[NetworkManager] reread-buffered-marker");
  std::string const path = logger_->get_log_file_path ();
  EXPECT_FALSE (
      contains (trigger::read_file (path), "reread-buffered-marker"));

  write_trigger ("info");
  logger_->force_reread ();

  EXPECT_FALSE (logger_->is_buffering_enabled ());
  EXPECT_FALSE (logger_->is_preset_enabled ());
  EXPECT_FALSE (logger_->is_describe_frame_enabled ());
  EXPECT_EQ (logger_->get_function_name_mode (), FunctionNameMode::FULL);
  EXPECT_FALSE (
      logger_->get_applied_config_view ().show_stack_trace_in_messages);
  logger_->flush ();
  EXPECT_TRUE (contains (trigger::read_file (path), "reread-buffered-marker"));
}

TEST_F (LumexLoggerRereadTest,
        GivenAnUnchangedTriggerFile_WhenRereadAgain_ThenNothingChanges)
{
  enable_at ("warning");
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  ASSERT_EQ (recorded_calls ().size (), 1U);
  std::string const path = logger_->get_log_file_path ();

  // A level set through the API stays while the file does not change; the
  // callbacks hear it at once.
  logger_->set_log_level (LogLevel::LEVEL_DEBUG);
  ASSERT_EQ (recorded_calls ().size (), 2U);
  EXPECT_EQ (recorded_calls ()[1].level, LogLevel::LEVEL_DEBUG);
  logger_->force_reread ();
  logger_->force_reread ();

  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  EXPECT_EQ (logger_->get_log_file_path (), path);
  EXPECT_EQ (recorded_calls ().size (), 2U);
  logger_->flush ();
  EXPECT_FALSE (contains (trigger::read_file (path), "Trigger file re-read"));
}

TEST_F (LumexLoggerRereadTest,
        GivenATouchedTriggerFileWithTheSameText_WhenReread_ThenApiValuesStay)
{
  enable_at ("warning", "PRESET=NetworkManager\n");
  std::string const path = logger_->get_log_file_path ();
  logger_->set_log_level (LogLevel::LEVEL_DEBUG);
  logger_->disable_preset ();

  // A new modification time (past one clock tick), the same text.
  std::this_thread::sleep_for (std::chrono::milliseconds (50));
  write_trigger ("warning", "PRESET=NetworkManager\n");
  logger_->force_reread ();

  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  EXPECT_FALSE (logger_->is_preset_enabled ());
  logger_->flush ();
  EXPECT_FALSE (contains (trigger::read_file (path), "Trigger file re-read"));

  // A real change applies only what changed: the preset stays off.
  write_trigger ("error", "PRESET=NetworkManager\n");
  logger_->force_reread ();
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_ERROR);
  EXPECT_FALSE (logger_->is_preset_enabled ());
}

TEST_F (
    LumexLoggerRereadTest,
    GivenADisabledLoggerWithoutTriggerFile_WhenRereadAgain_ThenItStaysDisabled)
{
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  logger_->force_reread ();
  logger_->log (LogLevel::LEVEL_FATAL, "reread-nothing-marker");

  EXPECT_FALSE (logger_->is_logging_enabled ());
  EXPECT_TRUE (logger_->get_log_file_path ().empty ());
  EXPECT_EQ (recorded_calls ().size (), 1U);
}

// --- The level change callback ----------------------------------------------

TEST_F (LumexLoggerRereadTest,
        GivenALevelChangeCallback_WhenSetAndAfterEachChange_ThenItGetsTheState)
{
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  enable_at ("warning");
  write_trigger ("debug");
  logger_->force_reread ();
  ASSERT_TRUE (trigger::remove_file (trigger_path_));
  logger_->force_reread ();

  std::vector<callback_call_t> const calls = recorded_calls ();
  ASSERT_EQ (calls.size (), 4U);
  EXPECT_FALSE (calls[0].enabled); // the call of add_level_change_callback
  EXPECT_TRUE (calls[1].enabled);
  EXPECT_EQ (calls[1].level, LogLevel::LEVEL_WARNING);
  EXPECT_TRUE (calls[2].enabled);
  EXPECT_EQ (calls[2].level, LogLevel::LEVEL_DEBUG);
  EXPECT_FALSE (calls[3].enabled);
}

TEST_F (LumexLoggerRereadTest,
        GivenLevelChangeCallbacks_WhenAddedAndRemoved_ThenEachIsCalledOnce)
{
  EXPECT_FALSE (logger_->add_level_change_callback (nullptr));
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  // The same pointer again: one entry, told the state once more.
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  ASSERT_TRUE (logger_->add_level_change_callback (&other_callback));
  EXPECT_EQ (recorded_calls ().size (), 2U);

  enable_at ("info");
  EXPECT_EQ (recorded_calls ().size (), 3U) << "one entry, one call";

  EXPECT_FALSE (logger_->remove_level_change_callback (nullptr));
  EXPECT_TRUE (logger_->remove_level_change_callback (&record_level_change));
  EXPECT_FALSE (logger_->remove_level_change_callback (&record_level_change));
  write_trigger ("debug");
  logger_->force_reread ();
  EXPECT_EQ (recorded_calls ().size (), 3U);
  EXPECT_TRUE (logger_->remove_level_change_callback (&other_callback));
}

TEST_F (
    LumexLoggerRereadTest,
    GivenACallbackThatSetsTheLevel_WhenAChangeIsTold_ThenAllHearTheFinalLevel)
{
  enable_at ("info");
  ASSERT_TRUE (logger_->add_level_change_callback (&raising_callback));
  ASSERT_TRUE (logger_->add_level_change_callback (&last_level_callback));

  write_trigger ("trace");
  logger_->force_reread ();

  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_ERROR);
  EXPECT_EQ (last_heard_level ().load (),
             static_cast<int> (LogLevel::LEVEL_ERROR));
  std::vector<callback_call_t> const calls = recorded_calls ();
  ASSERT_EQ (calls.size (), 3U); // the add, TRACE, then ERROR
  EXPECT_EQ (calls[2].level, LogLevel::LEVEL_ERROR);
}

TEST_F (LumexLoggerRereadTest,
        GivenEightLevelChangeCallbacks_WhenANinthIsAdded_ThenItIsRefused)
{
  std::vector<LumexLogger::level_change_callback_t> const table
      = { &table_callback<0>, &table_callback<1>, &table_callback<2>,
          &table_callback<3>, &table_callback<4>, &table_callback<5>,
          &table_callback<6>, &table_callback<7> };
  for (LumexLogger::level_change_callback_t const callback : table)
    ASSERT_TRUE (logger_->add_level_change_callback (callback));
  EXPECT_FALSE (logger_->add_level_change_callback (&record_level_change));
  EXPECT_TRUE (logger_->add_level_change_callback (table[3])) << "present";

  // Every registered callback hears a change; a freed entry takes another.
  enable_at ("debug");
  for (int index = 0; index < 8; ++index)
    EXPECT_EQ (table_calls ()[static_cast<std::size_t> (index)],
               index == 3 ? 3 : 2)
        << "callback " << index;
  EXPECT_TRUE (logger_->remove_level_change_callback (table[5]));
  EXPECT_TRUE (logger_->add_level_change_callback (&record_level_change));
  for (LumexLogger::level_change_callback_t const callback : table)
    logger_->remove_level_change_callback (callback);
}

TEST_F (
    LumexLoggerRereadTest,
    GivenACallbackThatLogs_WhenACheckRunsWhileTheLazyCheckIsDue_ThenItsRecordIsWritten)
{
  enable_at ("info");
  ASSERT_TRUE (logger_->add_level_change_callback (&logging_callback));
  // Past the lazy period: the log () of the callback would start a lazy
  // check in the thread that already holds the check.
  std::this_thread::sleep_for (std::chrono::milliseconds (2200));
  write_trigger ("debug");
  logger_->force_reread ();

  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  std::vector<callback_call_t> const calls = recorded_calls ();
  ASSERT_EQ (calls.size (), 2U);
  EXPECT_EQ (calls[1].level, LogLevel::LEVEL_DEBUG);
  logger_->flush ();
  std::string const text = trigger::read_file (logger_->get_log_file_path ());
  std::string::size_type const reread
      = text.find ("=== Trigger file re-read, log level: LEVEL_DEBUG ===");
  ASSERT_NE (reread, std::string::npos);
  EXPECT_NE (text.find ("reread-callback-log-marker", reread),
             std::string::npos);

  // The log () of the callback did not take the lazy period (review M8):
  // the very next filtered call checks and applies a new change.
  write_trigger ("warning");
  EXPECT_TRUE (logger_->is_level_enabled (LogLevel::LEVEL_ERROR));
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_WARNING)
      << "the callback's log () consumed the lazy period of this thread";
}

// --- Failures and retries ---------------------------------------------------

TEST_F (LumexLoggerRereadTest,
        GivenAnEmptyTriggerFile_WhenItAppears_ThenLoggingStartsWithTheDefaults)
{
  // Review N1: the hint says "create enable_logs"; an empty file is the
  // defaults in every format, also when it appears at run time.
  ASSERT_TRUE (trigger::write_file (trigger_path_, ""));
  logger_->force_reread ();

  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_INFO);
  std::ostringstream captured;
  std::streambuf *const previous = std::cerr.rdbuf (captured.rdbuf ());
  logger_->force_reread ();
  std::cerr.rdbuf (previous);
  EXPECT_TRUE (captured.str ().empty ()) << captured.str ();
}

TEST_F (
    LumexLoggerRereadTest,
    GivenAnEnabledLogger_WhenTheFileIsCaughtEmptyDuringARewrite_ThenNothingChanges)
{
  // Review L1: an editor truncates, then writes the text again in place.
  enable_at ("debug");
  logger_->set_log_level (LogLevel::LEVEL_TRACE);
  ASSERT_TRUE (trigger::write_file (trigger_path_, ""));
  logger_->force_reread ();
  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_TRACE)
      << "the empty file applied the defaults";

  // The same text again: nothing differs from the last successful read
  write_trigger ("debug");
  logger_->force_reread ();
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_TRACE);

  // A real change still applies
  write_trigger ("warning");
  logger_->force_reread ();
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_WARNING);
}

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)                                  \
    || defined(LUMEX_LOGGER_CONFIG_FORMAT_XML)
TEST_F (LumexLoggerRereadTest,
        GivenAMalformedTriggerFile_WhenItAppears_ThenTheDefaultsUntilItParses)
{
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
  std::string const malformed = "{ \"logger\": { \"LEVEL\": \"tr";
  std::string const longerMalformed
      = "{ \"logger\": { \"LEVEL\": \"trace\", \"FUNCNAME\": \"no";
#else
  std::string const malformed = "<?xml version=\"1.0\"?><logger><LEVEL>tr";
  std::string const longerMalformed
      = "<?xml version=\"1.0\"?><logger><LEVEL>trace</LEVEL><FUNC";
#endif
  // Appears unparsable (review N1): enabled with the defaults, as at start
  ASSERT_TRUE (trigger::write_file (trigger_path_, malformed));
  logger_->force_reread ();
  ASSERT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_INFO);

  // Written completely: the next check reads it
  write_trigger ("debug");
  logger_->force_reread ();
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);

  // Unparsable while enabled (half written): nothing changes, read again
  ASSERT_TRUE (trigger::write_file (trigger_path_, longerMalformed));
  logger_->force_reread ();
  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  write_trigger ("trace");
  logger_->force_reread ();
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_TRACE);
}
#endif

TEST_F (
    LumexLoggerRereadTest,
    GivenARoundInAnotherThread_WhenItsCallbackIsRemoved_ThenRemoveWaitsForIt)
{
  // Review N5: after remove returns, the callback (in a module that is about
  // to be unloaded) is neither running nor called again.
  enable_at ("info");
  ASSERT_TRUE (logger_->add_level_change_callback (&slow_callback));
  ASSERT_EQ (slow_inside ().load (), 0);
  std::thread other ([this] ()
                       { logger_->set_log_level (LogLevel::LEVEL_DEBUG); });
  while (slow_inside ().load () == 0)
    std::this_thread::yield ();
  bool const removed = logger_->remove_level_change_callback (&slow_callback);
  int const insideAfterRemove = slow_inside ().load ();
  other.join ();

  EXPECT_TRUE (removed);
  EXPECT_EQ (insideAfterRemove, 0) << "remove returned during the call";
  int const calls = slow_calls ().load ();
  logger_->set_log_level (LogLevel::LEVEL_INFO);
  EXPECT_EQ (slow_calls ().load (), calls);
}

TEST_F (LumexLoggerRereadTest,
        GivenACallbackThatForcesAReread_WhenTheStateChanges_ThenNoDeadlock)
{
  // Review N9: force_reread from a callback returns at once. MSVC's mutex
  // reports the second lock by its owner; glibc's would hang (todo 158).
  enable_at ("info");
  ASSERT_TRUE (logger_->add_level_change_callback (&reentrant_callback));
  std::atomic<bool> done (false);
  std::thread worker (
      [this, &done] ()
        {
          write_trigger ("debug");
          logger_->force_reread ();
          logger_->set_log_level (LogLevel::LEVEL_WARNING);
          done.store (true);
        });
  for (int index = 0; index < 300 && !done.load (); ++index)
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  ASSERT_TRUE (done.load ()) << "deadlock: a callback's force_reread blocked";
  worker.join ();
  EXPECT_GE (reentrant_calls ().load (), 3);
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_WARNING);
}

#if defined(_WIN32)
TEST_F (
    LumexLoggerRereadTest,
    GivenAnUnchangedTriggerFileThatIsLocked_WhenRereadAgain_ThenItIsNotRead)
{
  enable_at ("info");
  HANDLE const locked
      = CreateFileA (trigger_path_.c_str (), GENERIC_READ, 0, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE (locked, INVALID_HANDLE_VALUE);
  std::ostringstream captured;
  std::streambuf *const previous = std::cerr.rdbuf (captured.rdbuf ());
  logger_->force_reread ();
  std::cerr.rdbuf (previous);
  CloseHandle (locked);

  // Nothing changed, so the check stops at the stat: a read of the locked
  // file would have failed and said so on std::cerr.
  EXPECT_TRUE (captured.str ().empty ()) << captured.str ();
  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_INFO);
}

TEST_F (LumexLoggerRereadTest,
        GivenATriggerFileThatCannotBeRead_WhenItCanAgain_ThenItsSettingsApply)
{
  enable_at ("trace");
  write_trigger ("warning");
  // An editor or a virus scanner holds the file without sharing (review R4).
  HANDLE const locked
      = CreateFileA (trigger_path_.c_str (), GENERIC_READ, 0, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE (locked, INVALID_HANDLE_VALUE);
  logger_->force_reread ();
  LogLevel const whileLocked = logger_->get_log_level ();
  CloseHandle (locked);
  logger_->force_reread ();

  EXPECT_EQ (whileLocked, LogLevel::LEVEL_TRACE)
      << "an unreadable file applied " << log_level_to_string (whileLocked);
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_WARNING)
      << "the next check did not retry the read";
}
#endif

TEST_F (LumexLoggerRereadTest,
        GivenALogThatCannotBeOpened_WhenItCanLater_ThenTheNextCheckEnables)
{
  // A directory of the log's name: the log cannot be opened (review R5).
  std::string const logPath = trigger::path_in_logger_directory ("logger.log");
  trigger::remove_file (logPath);
  ASSERT_TRUE (trigger::make_directory (logPath));
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  write_trigger ("debug");
  logger_->force_reread ();
  bool const enabledWhileBlocked = logger_->is_logging_enabled ();
  LogLevel const levelWhileBlocked = logger_->get_log_level ();
  std::size_t const callsWhileBlocked = recorded_calls ().size ();
  ASSERT_TRUE (trigger::remove_directory (logPath));
  logger_->force_reread ();

  EXPECT_FALSE (enabledWhileBlocked);
  EXPECT_NE (levelWhileBlocked, LogLevel::LEVEL_DEBUG)
      << "the failed open applied the settings of the file";
  EXPECT_EQ (callsWhileBlocked, 1U) << "a failed open told the callbacks";
  EXPECT_TRUE (logger_->is_logging_enabled ())
      << "the next check did not retry the open";
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
  std::vector<callback_call_t> const calls = recorded_calls ();
  ASSERT_EQ (calls.size (), 2U);
  EXPECT_TRUE (calls[1].enabled);
  EXPECT_EQ (calls[1].level, LogLevel::LEVEL_DEBUG);
}

TEST_F (LumexLoggerRereadTest,
        GivenTimestampedLogs_WhenTheFileComesBackWithinASecond_ThenANewLog)
{
  // Right after a second boundary: both opens fall in the same second.
  for (;;)
    {
      long long const ms
          = static_cast<long long> (
                std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::system_clock::now ().time_since_epoch ())
                    .count ())
            % 1000;
      if (ms < 100)
        break;
      std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
  enable_at ("info", "TIMESTAMPED=true\n");
  std::string const first = logger_->get_log_file_path ();
  logger_->log (LogLevel::LEVEL_INFO, "reread-first-timestamped-marker");
  ASSERT_TRUE (trigger::remove_file (trigger_path_));
  logger_->force_reread ();
  enable_at ("debug", "TIMESTAMPED=true\n");
  std::string const second = logger_->get_log_file_path ();
  logger_->flush ();

  EXPECT_NE (first, second) << "the second log took the name of the first";
  EXPECT_TRUE (contains (trigger::read_file (first),
                         "reread-first-timestamped-marker"));
  trigger::remove_file (trigger_path_);
  logger_->force_reread ();
  trigger::remove_file (first);
  trigger::remove_file (second);
}

// --- The lazy check ---------------------------------------------------------

TEST_F (
    LumexLoggerRereadTest,
    GivenTheLazyCheck_WhenTwoSecondsPassed_ThenAFilteredCallAppliesTheChange)
{
  enable_at ("warning");
  write_trigger ("debug");

  // The next lazy check is due at most 2 s after the last one claimed, which
  // was before the write: after 2.2 s the first call checks.
  std::this_thread::sleep_for (std::chrono::milliseconds (2200));
  EXPECT_TRUE (logger_->is_level_enabled (LogLevel::LEVEL_DEBUG));
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);

  // That call took the period: a change right after it waits for the next.
  write_trigger ("warning");
  EXPECT_TRUE (logger_->is_level_enabled (LogLevel::LEVEL_DEBUG));
  logger_->log (LogLevel::LEVEL_TRACE, "reread-filtered-marker");
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_DEBUG);
}

TEST_F (
    LumexLoggerRereadTest,
    GivenTheLazyCheck_WhenTheFileAppearsAndTwoSecondsPassed_ThenLogEnablesIt)
{
  write_trigger ("info");
  std::this_thread::sleep_for (std::chrono::milliseconds (2200));
  logger_->log (LogLevel::LEVEL_INFO, "reread-lazy-appear-marker");

  ASSERT_TRUE (logger_->is_logging_enabled ());
  logger_->flush ();
  EXPECT_TRUE (contains (trigger::read_file (logger_->get_log_file_path ()),
                         "reread-lazy-appear-marker"));
}

// --- trigger_file_watcher ---------------------------------------------------

TEST_F (
    LumexLoggerRereadTest,
    GivenAWatcher_WhenTheFileChanges_ThenItsThreadAppliesItAndLeavesNoThread)
{
  std::size_t const before = trigger_file_watcher::running_threads ();
  ASSERT_TRUE (logger_->add_level_change_callback (&record_level_change));
  clear_recorded_calls ();

  callback_call_t watcher_call
      = { false, LogLevel::LEVEL_INFO, std::thread::id (), 0 };
  {
    trigger_file_watcher watcher (std::chrono::milliseconds (20));
    EXPECT_TRUE (watcher.is_running ());
    EXPECT_EQ (watcher.period (), std::chrono::milliseconds (20));
    EXPECT_EQ (trigger_file_watcher::running_threads (), before + 1);

    write_trigger ("debug");
    ASSERT_TRUE (wait_for_call_from_another_thread (
        watcher_call, std::chrono::milliseconds (5000)));
    EXPECT_TRUE (watcher_call.enabled);
    EXPECT_EQ (watcher_call.level, LogLevel::LEVEL_DEBUG);
    EXPECT_TRUE (logger_->is_logging_enabled ());
    if (os_thread_check_supported ())
      EXPECT_TRUE (os_thread_alive (watcher_call.os_thread));
  }

  EXPECT_EQ (trigger_file_watcher::running_threads (), before);
  if (os_thread_check_supported ())
    EXPECT_TRUE (
        eventually ([&watcher_call] ()
                      { return !os_thread_alive (watcher_call.os_thread); },
                    std::chrono::milliseconds (2000)))
        << "the watcher thread is still listed after the destructor";
}

TEST_F (
    LumexLoggerRereadTest,
    GivenAWatcher_WhenMovedAndDestroyed_ThenOneThreadRunsAndEndsWithTheOwner)
{
  std::size_t const before = trigger_file_watcher::running_threads ();
  {
    trigger_file_watcher first (std::chrono::milliseconds (0));
    EXPECT_EQ (first.period (), std::chrono::milliseconds (1));
    trigger_file_watcher second (std::move (first));
    EXPECT_FALSE (first.is_running ()); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ (first.period (), std::chrono::milliseconds (0));
    EXPECT_TRUE (second.is_running ());
    EXPECT_EQ (trigger_file_watcher::running_threads (), before + 1);

    trigger_file_watcher third;
    EXPECT_EQ (third.period (), std::chrono::milliseconds (1000));
    EXPECT_EQ (trigger_file_watcher::running_threads (), before + 2);
    third = std::move (second); // stops the thread of third, takes second's
    EXPECT_EQ (trigger_file_watcher::running_threads (), before + 1);
    EXPECT_TRUE (third.is_running ());
    EXPECT_EQ (third.period (), std::chrono::milliseconds (1));
  }
  EXPECT_EQ (trigger_file_watcher::running_threads (), before);
}

// --- Concurrency ------------------------------------------------------------

TEST_F (
    LumexLoggerRereadTest,
    GivenThreadsThatLog_WhenTheTriggerFileComesAndGoes_ThenTheLoggerFollows)
{
  trigger_file_watcher watcher (std::chrono::milliseconds (1));
  std::mutex stop_mutex;
  bool stop = false;
  std::vector<std::thread> writers;
  for (int index = 0; index < 4; ++index)
    writers.emplace_back (
        [this, index, &stop_mutex, &stop] ()
          {
            for (unsigned count = 0;; ++count)
              {
                {
                  std::lock_guard<std::mutex> const lock (stop_mutex);
                  if (stop)
                    return;
                }
                if (logger_->is_level_enabled (LogLevel::LEVEL_INFO))
                  logger_->log (LogLevel::LEVEL_INFO,
                                "reread-writer-" + std::to_string (index) + "-"
                                    + std::to_string (count));
                (void)logger_->get_log_file_path ();
                (void)logger_->is_buffering_enabled ();
              }
          });

  for (int round = 0; round < 10; ++round)
    {
      write_trigger (round % 2 == 0 ? "debug" : "info");
      logger_->force_reread ();
      std::this_thread::sleep_for (std::chrono::milliseconds (5));
      trigger::remove_file (trigger_path_);
      logger_->force_reread ();
      std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
  enable_at ("warning");

  {
    std::lock_guard<std::mutex> const lock (stop_mutex);
    stop = true;
  }
  for (std::thread &writer : writers)
    writer.join ();

  EXPECT_TRUE (logger_->is_logging_enabled ());
  EXPECT_EQ (logger_->get_log_level (), LogLevel::LEVEL_WARNING);
  logger_->flush ();
  // The watcher may have caught the file between its truncation and its
  // text (defaults, then a re-read): either the header or a re-read line
  // names the final level.
  std::string const text = trigger::read_file (logger_->get_log_file_path ());
  EXPECT_TRUE (contains (text, "Log level from trigger-file: LEVEL_WARNING")
               || contains (text, "log level: LEVEL_WARNING ==="));
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

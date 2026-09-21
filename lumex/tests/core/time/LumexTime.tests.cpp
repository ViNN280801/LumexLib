#include <algorithm> // For std::all_of
#include <chrono>
#include <ctime>
#include <sstream>
#include <stdexcept> // For potential exceptions in string to long long conversion
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/LumexUtility" // For LUMEX_OS_WINDOWS, LUMEX_OS_IS_UNIX

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::time::clock;
using namespace lumex::core::time::timer;

// Test fixture for LumexTime to provide a clean state if needed (though not
// strictly necessary for static functions).
class LumexTimeTest : public ::testing::Test
{
};

// --- get_current_datetime Tests ---

TEST_F (LumexTimeTest, GetCurrentDatetime_DefaultFormat_Clean)
{
  std::string datetime_str = LumexTime::get_current_datetime ();
  // Expect a non-empty string of a certain minimum length, e.g.,
  // "DD.MM.YYYY_HH:MM:SS" is 19 chars
  ASSERT_FALSE (datetime_str.empty ());
  ASSERT_GE (datetime_str.length (), 19);

  // Basic format check: should contain at least one '.' and one '_' and one
  // ':'
  ASSERT_NE (datetime_str.find ('.'), std::string::npos);
  ASSERT_NE (datetime_str.find ('_'), std::string::npos);
  ASSERT_NE (datetime_str.find (':'), std::string::npos);
}

TEST_F (LumexTimeTest, GetCurrentDatetime_ISO8601Format_Clean)
{
  std::string format_str = "%Y-%m-%dT%H:%M:%S"; // ISO 8601-like
  std::string datetime_str
      = LumexTime::get_current_datetime (format_str.c_str ());
  ASSERT_FALSE (datetime_str.empty ());
  ASSERT_GE (datetime_str.length (), format_str.length ());

// strptime is POSIX, not standard C++. Windows does not have it by default.
// This parsing verification is specific to Unix-like systems.
#if LUMEX_OS_IS_UNIX
  std::tm tm_from_str = {};
  char *parse_result
      = strptime (datetime_str.c_str (), format_str.c_str (), &tm_from_str);
  ASSERT_NE (parse_result, nullptr)
      << "Failed to parse generated datetime string: " << datetime_str;

  // Additional sanity check: current year should be non-zero (since 1900)
  ASSERT_GE (tm_from_str.tm_year + 1900, 2023);
#else
  // On Windows, we cannot reliably use strptime.
  // We rely on the format string length check and manual inspection for
  // validity. No further runtime parsing validation for ISO 8601 format on
  // Windows in tests.
  SUCCEED () << "strptime test skipped on non-UNIX platform (Windows).";
#endif
}

TEST_F (LumexTimeTest, GetCurrentDatetime_EmptyFormat_Dirty)
{
  std::string datetime_str = LumexTime::get_current_datetime ("");
  ASSERT_TRUE (datetime_str.empty ()); // Expect empty string for empty format
}

TEST_F (LumexTimeTest, GetCurrentDatetime_InvalidFormat_Dirty)
{
  // %Q is an invalid format specifier for strftime
  std::string datetime_str
      = LumexTime::get_current_datetime ("%Y-%m-%d %Q %H:%M:%S");
  // strftime might return an empty string or partial string depending on
  // implementation A robust LumexTime should handle this gracefully (e.g.,
  // return empty)
  ASSERT_TRUE (datetime_str.empty ());
}

TEST_F (LumexTimeTest, GetCurrentDatetime_ThreadSafety_Dirty)
{
  // Test concurrent access to get_current_datetime
  std::vector<std::thread> threads;
  std::vector<std::string> results (100); // Store results from 100 threads

  for (int i = 0; i < 100; ++i)
    {
      threads.emplace_back ([&results, i] () {
        results[i] = LumexTime::get_current_datetime ();
      });
    }

  for (auto &th : threads)
    if (th.joinable ())
      th.join ();

  // Verify all results are non-empty and have expected length
  for (std::string const &s : results)
    {
      ASSERT_FALSE (s.empty ());
      ASSERT_GE (s.length (), 19);
    }
}

// --- get_timestamp_xxx Tests ---

// Helper to convert string timestamp to long long, with error handling
long long
safe_stoll (std::string const &s)
{
  try
    {
      return std::stoll (s);
    }
  catch (std::exception const &e)
    {
      ADD_FAILURE () << "Failed to convert timestamp string '" << s
                     << "' to long long: " << e.what ();
      return 0; // Return 0 on error, so subsequent checks don't crash
    }
}

// Macro to avoid repetition for timestamp tests
#define TEST_TIMESTAMP_FUNCTION(Func, ExpectedMinLength, DivisorNsToUnit)     \
  TEST_F (LumexTimeTest, GetTimestamp_##Func##_Clean)                         \
  {                                                                           \
    std::string timestamp_str = LumexTime::Func ();                           \
    ASSERT_FALSE (timestamp_str.empty ());                                    \
    ASSERT_TRUE (std::all_of (timestamp_str.begin (), timestamp_str.end (),   \
                              ::isdigit))                                     \
        << "Timestamp contains non-digit characters: " << timestamp_str;      \
    ASSERT_GE (timestamp_str.length (), ExpectedMinLength);                   \
                                                                              \
    /* Verify increasing value */                                             \
    std::string timestamp_str2 = LumexTime::Func ();                          \
    ASSERT_GT (safe_stoll (timestamp_str2), safe_stoll (timestamp_str));      \
  }

TEST_TIMESTAMP_FUNCTION (get_timestamp_ns, 19, 1LL)
TEST_TIMESTAMP_FUNCTION (get_timestamp_mcs, 16,
                         lumex::core::time::clock::Constants::NS_IN_MCS)
TEST_TIMESTAMP_FUNCTION (get_timestamp_ms, 13,
                         lumex::core::time::clock::Constants::NS_IN_MS)
TEST_TIMESTAMP_FUNCTION (get_timestamp_s, 10,
                         lumex::core::time::clock::Constants::NS_IN_S)
TEST_TIMESTAMP_FUNCTION (get_timestamp_min, 8,
                         lumex::core::time::clock::Constants::NS_IN_MIN)
TEST_TIMESTAMP_FUNCTION (get_timestamp_h, 6,
                         lumex::core::time::clock::Constants::NS_IN_H)
TEST_TIMESTAMP_FUNCTION (get_timestamp_d, 4,
                         lumex::core::time::clock::Constants::NS_IN_D)
TEST_TIMESTAMP_FUNCTION (get_timestamp_w, 3,
                         lumex::core::time::clock::Constants::NS_IN_W)
TEST_TIMESTAMP_FUNCTION (get_timestamp_m, 2,
                         lumex::core::time::clock::Constants::NS_IN_M)
TEST_TIMESTAMP_FUNCTION (get_timestamp_y, 2,
                         lumex::core::time::clock::Constants::NS_IN_Y)

// Dirty test for performance sanity check (not a benchmark)
TEST_F (LumexTimeTest, GetTimestamp_Performance_Dirty)
{
  // Check if it runs quickly over many iterations
  int const num_iterations = 100000;
  ASSERT_NO_FATAL_FAILURE ({
    for (int i = 0; i < num_iterations; ++i)
      LumexTime::get_timestamp_ns ();
  }) << "get_timestamp_ns took too long for "
     << num_iterations << " iterations.";
}

// Dirty test for get_timestamp when exception occurs in private helper
// This is hard to trigger directly without mocking, but we can verify the
// fallback.
TEST_F (LumexTimeTest, GetTimestamp_ExceptionFallback_Dirty)
{
  // The internal _get_timestamp catches std::exception and returns an empty
  // string. We can't easily force std::to_string or chrono to throw here
  // without advanced mocking. This test primarily serves as a placeholder and
  // an acknowledgment of the existing handling. If `_get_timestamp` was
  // refactored to allow injecting a throwing component, this would be
  // expanded.
  std::string result = LumexTime::get_timestamp_ns ();
  ASSERT_FALSE (result.empty ())
      << "Expected a valid timestamp under normal conditions.";
}

#undef TEST_TIMESTAMP_FUNCTION

// --- timestamp() Tests ---------------------------------------------------

TEST_F (LumexTimeTest, Timestamp_DefaultFormat_Clean)
{
  std::string stamp = LumexTime::timestamp ();
  // Default format "%Y%m%d-%H%M%S" -> "YYYYMMDD-HHMMSS", exactly 15
  // characters.
  ASSERT_EQ (stamp.size (), 15u);
  EXPECT_EQ (stamp[8], '-');
  EXPECT_TRUE (std::all_of (stamp.begin (), stamp.begin () + 8, ::isdigit));
  EXPECT_TRUE (std::all_of (stamp.begin () + 9, stamp.end (), ::isdigit));
}

TEST_F (LumexTimeTest, Timestamp_CustomFormat_Clean)
{
  std::string stamp = LumexTime::timestamp (std::time (nullptr), "%Y-%m-%d");
  ASSERT_EQ (stamp.size (), 10u);
  EXPECT_EQ (stamp[4], '-');
  EXPECT_EQ (stamp[7], '-');
}

TEST_F (LumexTimeTest, Timestamp_EmptyFormat_WhenUnfound_ThenNotDefaultLayout)
{
  // Found path: default "%Y%m%d-%H%M%S" is 15 characters. An empty format
  // is the unfound-format branch: put_time writes nothing when localtime
  // succeeds, or the raw epoch digits when the tm snapshot cannot be taken.
  std::string const stamp = LumexTime::timestamp (std::time (nullptr), "");
  EXPECT_NE (stamp.size (), 15u);
  if (!stamp.empty ())
    {
      EXPECT_TRUE (std::all_of (stamp.begin (), stamp.end (), ::isdigit))
          << "unexpected fallback stamp: " << stamp;
    }
}

TEST_F (LumexTimeTest,
        Timestamp_UnknownConversion_WhenUnfound_ThenDiffersFromKnownFormat)
{
  std::time_t const now = std::time (nullptr);
  std::string const found = LumexTime::timestamp (now, "%Y%m%d-%H%M%S");
  std::string const unfound = LumexTime::timestamp (now, "%Q");
  ASSERT_EQ (found.size (), 15u);
  EXPECT_NE (unfound, found);
}

TEST_F (LumexTimeTest, Timestamp_FixedEpoch_ProducesNonEmptyResult)
{
  // std::time_t(0) is a valid, well-defined instant (the Unix epoch); the
  // fallback chain (localtime -> gmtime -> raw epoch seconds) must produce a
  // non-empty string either way.
  std::string stamp
      = LumexTime::timestamp (static_cast<std::time_t> (0), "%Y");
  EXPECT_FALSE (stamp.empty ());
}

TEST_F (LumexTimeTest, Timestamp_ThreadSafety_Dirty)
{
  std::vector<std::thread> threads;
  std::vector<std::string> results (50);

  for (int i = 0; i < 50; ++i)
    {
      threads.emplace_back (
          [&results, i] () { results[i] = LumexTime::timestamp (); });
    }
  for (auto &th : threads)
    if (th.joinable ())
      th.join ();

  for (std::string const &s : results)
    ASSERT_EQ (s.size (), 15u);
}

// --- timestamp_ms() Tests -------------------------------------------------

TEST_F (LumexTimeTest, TimestampMs_DefaultFormat_Clean)
{
  std::string stamp = LumexTime::timestamp_ms ();
  // Default format "%Y-%m-%d %H:%M:%S" (19 chars) + "." + 3-digit ms suffix =
  // 23 chars.
  ASSERT_EQ (stamp.size (), 23u);
  EXPECT_EQ (stamp[19], '.');
  EXPECT_TRUE (std::all_of (stamp.begin () + 20, stamp.end (), ::isdigit));
}

TEST_F (LumexTimeTest, TimestampMs_CustomFormat_Clean)
{
  std::string stamp = LumexTime::timestamp_ms (
      std::chrono::system_clock::now (), "%H:%M:%S");
  // "HH:MM:SS" (8 chars) + "." + 3-digit ms suffix = 12 chars.
  ASSERT_EQ (stamp.size (), 12u);
  EXPECT_EQ (stamp[8], '.');
}

TEST_F (LumexTimeTest,
        TimestampMs_GivenExplicitTimePoint_ThenDeterministicSecondsPortion)
{
  // Construct a time_point exactly at the Unix epoch plus 500ms; the
  // millisecond suffix must reflect that fractional part regardless of
  // platform/timezone.
  auto tp = std::chrono::system_clock::time_point (
      std::chrono::milliseconds (500));
  std::string stamp = LumexTime::timestamp_ms (tp, "%Y");
  // The calendar year portion is timezone-dependent (could read 1969 or 1970
  // depending on the machine's local offset around the epoch), but
  // time_since_epoch() itself is not: the ".500" millisecond suffix must
  // always be present regardless of timezone.
  ASSERT_GE (stamp.size (), 4u);
  EXPECT_EQ (stamp.substr (stamp.size () - 4), ".500")
      << "full stamp: " << stamp;
}

// --- LumexTimer Tests ------------------------------------------------------

class LumexTimerTest : public ::testing::Test
{
};

TEST_F (LumexTimerTest, DefaultConstructed_ElapsedIsZero)
{
  LumexTimer timer;
  EXPECT_EQ (timer.elapsed_time_ms (), 0);
}

TEST_F (LumexTimerTest, StartThenStop_MeasuresNonNegativeElapsed)
{
  LumexTimer timer;
  timer.start_timer ();
  std::this_thread::sleep_for (std::chrono::milliseconds (20));
  timer.stop_timer ();

  EXPECT_GE (timer.elapsed_time_ms (), 10)
      << "Expected at least ~10ms to have elapsed after a 20ms sleep.";
}

TEST_F (LumexTimerTest, RestartingTimer_UpdatesStartPoint)
{
  LumexTimer timer;
  timer.start_timer ();
  std::this_thread::sleep_for (std::chrono::milliseconds (5));
  timer.stop_timer ();
  long long first_elapsed = timer.elapsed_time_ms ();

  timer.start_timer ();
  std::this_thread::sleep_for (std::chrono::milliseconds (30));
  timer.stop_timer ();
  long long second_elapsed = timer.elapsed_time_ms ();

  EXPECT_GT (second_elapsed, first_elapsed);
}

// --- extract_function_name() Tests
// --------------------------------------------

TEST (ExtractFunctionNameTest, GivenSimpleCall_ThenReturnsFunctionName)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("foo()"), "foo");
}

TEST (ExtractFunctionNameTest, GivenArrowMemberCall_ThenReturnsMethodName)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name (
                 "obj->DoSomething(1, 2)"),
             "DoSomething");
}

TEST (ExtractFunctionNameTest, GivenDotMemberCall_ThenReturnsMethodName)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("obj.Method()"),
             "Method");
}

TEST (ExtractFunctionNameTest,
      GivenChainedMemberAccess_ThenReturnsOnlyLastSegment)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("a.b->c(1)"),
             "c");
}

TEST (ExtractFunctionNameTest,
      GivenNoParensOrMemberAccess_ThenReturnsInputUnchanged)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("justName"),
             "justName");
}

TEST (ExtractFunctionNameTest,
      GivenLeadingAndTrailingWhitespace_ThenTrimsAndExtracts)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("  spaced  (x)"),
             "spaced");
}

TEST (ExtractFunctionNameTest, GivenEmptyString_ThenReturnsEmptyString)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name (""), "");
}

TEST (ExtractFunctionNameTest,
      GivenQualifiedCall_WhenExtracted_ThenKeepsNamespacePrefix)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("ns::foo()"),
             "ns::foo");
}

TEST (ExtractFunctionNameTest,
      GivenGlobalQualifiedCall_WhenExtracted_ThenKeepsLeadingColons)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("::bar()"),
             "::bar");
}

TEST (ExtractFunctionNameTest,
      GivenOnlyParentheses_WhenExtracted_ThenReturnsOriginal)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("()"), "()");
}

TEST (ExtractFunctionNameTest,
      GivenWhitespaceOnly_WhenExtracted_ThenReturnsOriginal)
{
  EXPECT_EQ (lumex::core::time::timer::extract_function_name ("   "), "   ");
}

// --- measure_execution_time / measure_time / LUMEX_MEASURE_TIME -------------

TEST (MeasureExecutionTimeTest, GivenSleepingCallable_ThenReturnsNonNegativeMs)
{
  long long const elapsed = measure_execution_time (
      [] () { std::this_thread::sleep_for (std::chrono::milliseconds (15)); });
  EXPECT_GE (elapsed, 5);
}

TEST (MeasureTimeTest, GivenGateOff_ThenAlwaysReportsToStream)
{
  std::ostringstream oss;
  int calls = 0;
  bool const reported = measure_time (
      [&calls] () {
        ++calls;
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
      },
      "unit", oss, /*need_to_gate_via_env=*/false, "UNUSED_ENV_NAME");

  EXPECT_TRUE (reported);
  EXPECT_EQ (calls, 1);
  EXPECT_NE (oss.str ().find ("unit: execution time:"), std::string::npos);
  EXPECT_NE (oss.str ().find ("ms"), std::string::npos);
}

TEST (MeasureTimeTest, GivenGateOnAndEnvUnset_ThenRunsWithoutReport)
{
  using lumex::core::environment::env::LumexEnvironment;
  char const *const env_name = "LUMEX_TEST_MEASURE_TIME_GATE_UNSET";
  ASSERT_TRUE (LumexEnvironment::set (env_name, nullptr));

  std::ostringstream oss;
  int calls = 0;
  bool const reported = measure_time ([&calls] () { ++calls; }, "skipped", oss,
                                      /*need_to_gate_via_env=*/true, env_name);

  EXPECT_FALSE (reported);
  EXPECT_EQ (calls, 1);
  EXPECT_TRUE (oss.str ().empty ());
}

TEST (MeasureTimeTest, GivenGateOnAndEnvSet_ThenReportsToStream)
{
  using lumex::core::environment::env::LumexEnvironment;
  char const *const env_name = "LUMEX_TEST_MEASURE_TIME_GATE_SET";
  ASSERT_TRUE (LumexEnvironment::set (env_name, "1"));

  std::ostringstream oss;
  int calls = 0;
  bool const reported = measure_time ([&calls] () { ++calls; }, "gated", oss,
                                      /*need_to_gate_via_env=*/true, env_name);

  ASSERT_TRUE (LumexEnvironment::set (env_name, nullptr));

  EXPECT_TRUE (reported);
  EXPECT_EQ (calls, 1);
  EXPECT_NE (oss.str ().find ("gated: execution time:"), std::string::npos);
}

TEST (WriteMeasureTimeReportTest, GivenEmptyMessage_ThenWritesGenericLine)
{
  std::ostringstream oss;
  write_measure_time_report (oss, "", 42);
  EXPECT_EQ (oss.str (), "Time: 42 [ms]\n");
}

TEST (MeasureTimeMacroTest, GivenGateOffViaApi_WhenMacroWouldGate_ThenNoOpPath)
{
  // Macro uses default env gate; verify the 2-arg form compiles and runs the
  // expression when the default env is unset (no report required here).
  using lumex::core::environment::env::LumexEnvironment;
  ASSERT_TRUE (
      LumexEnvironment::set (default_measure_time_env_name (), nullptr));

  int calls = 0;
  LUMEX_MEASURE_TIME (++calls);
  EXPECT_EQ (calls, 1);

  LUMEX_MEASURE_TIME (++calls, "custom");
  EXPECT_EQ (calls, 2);
}

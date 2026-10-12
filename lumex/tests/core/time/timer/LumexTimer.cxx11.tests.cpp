// Tests of lumex_timer and the time measurement helpers (lumex/core/time/
// timer): the timer, measure_time, the report writer and the function name
// extraction. Every suite of this directory compiles this file.

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/time/LumexTime"

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

// --- lumex_timer Tests ------------------------------------------------------

class LumexTimerTest : public ::testing::Test
{
};

TEST_F (LumexTimerTest, DefaultConstructed_ElapsedIsZero)
{
  lumex_timer timer;
  EXPECT_EQ (timer.elapsed_time_ms (), 0);
}

TEST_F (LumexTimerTest, StartThenStop_MeasuresNonNegativeElapsed)
{
  lumex_timer timer;
  timer.start_timer ();
  std::this_thread::sleep_for (std::chrono::milliseconds (20));
  timer.stop_timer ();

  EXPECT_GE (timer.elapsed_time_ms (), 10)
      << "Expected at least ~10ms to have elapsed after a 20ms sleep.";
}

TEST_F (LumexTimerTest, RestartingTimer_UpdatesStartPoint)
{
  lumex_timer timer;
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
      [&calls] ()
        {
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
  using lumex::core::environment::env::lumex_environment;
  char const *const env_name = "LUMEX_TEST_MEASURE_TIME_GATE_UNSET";
  ASSERT_TRUE (lumex_environment::set (env_name, nullptr));

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
  using lumex::core::environment::env::lumex_environment;
  char const *const env_name = "LUMEX_TEST_MEASURE_TIME_GATE_SET";
  ASSERT_TRUE (lumex_environment::set (env_name, "1"));

  std::ostringstream oss;
  int calls = 0;
  bool const reported = measure_time ([&calls] () { ++calls; }, "gated", oss,
                                      /*need_to_gate_via_env=*/true, env_name);

  ASSERT_TRUE (lumex_environment::set (env_name, nullptr));

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
  using lumex::core::environment::env::lumex_environment;
  ASSERT_TRUE (
      lumex_environment::set (default_measure_time_env_name (), nullptr));

  int calls = 0;
  LUMEX_MEASURE_TIME (++calls);
  EXPECT_EQ (calls, 1);

  LUMEX_MEASURE_TIME (++calls, "custom");
  EXPECT_EQ (calls, 2);
}

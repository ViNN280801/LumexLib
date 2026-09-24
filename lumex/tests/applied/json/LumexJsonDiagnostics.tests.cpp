// LumexJsonDiagnostics.tests.cpp
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/applied/json/diagnostics/LumexJsonDiagnostics.hpp"

using lumex::applied::json::diagnostics::get_diagnostic_reporter;
using lumex::applied::json::diagnostics::json_diagnostic_fn;
using lumex::applied::json::diagnostics::LumexJsonDiagnosticLevel;
using lumex::applied::json::diagnostics::LumexJsonDiagnosticSlot;
using lumex::applied::json::diagnostics::set_diagnostic_reporter;
namespace Detail = lumex::applied::json::diagnostics::Detail;

namespace
{
LumexJsonDiagnosticLevel g_level = LumexJsonDiagnosticLevel::debug;
std::string g_message;
int g_calls = 0;

void
record (LumexJsonDiagnosticLevel level, char const *message)
{
  ++g_calls;
  g_level = level;
  g_message = message;
}

void
throwing_reporter (LumexJsonDiagnosticLevel, char const *)
{
  throw std::runtime_error ("reporter failed");
}

struct unstreamable_friendly_t
{
  int value;
};

std::ostream &
operator<< (std::ostream &stream, unstreamable_friendly_t const &item)
{
  return stream << "item#" << item.value;
}

class LumexJsonDiagnosticsTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    set_diagnostic_reporter (nullptr);
    g_calls = 0;
    g_message.clear ();
  }

  void
  TearDown () override
  {
    set_diagnostic_reporter (nullptr);
  }
};
} // namespace

TEST_F (LumexJsonDiagnosticsTest, GivenFreshState_WhenGet_ThenNoReporter)
{
  EXPECT_EQ (get_diagnostic_reporter (), nullptr);
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenReporterSet_WhenGet_ThenSameAsSlotAndFunction)
{
  set_diagnostic_reporter (&record);
  EXPECT_EQ (get_diagnostic_reporter (), &record);
  EXPECT_EQ (LumexJsonDiagnosticSlot::get (), &record);
  set_diagnostic_reporter (nullptr);
  EXPECT_FALSE (LumexJsonDiagnosticSlot::is_set ());
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenReporter_WhenReportParts_ThenConcatenatedTextAndLevel)
{
  set_diagnostic_reporter (&record);
  Detail::report (LumexJsonDiagnosticLevel::warning, "key '",
                  std::string ("k"), "' = ", 42, ", ",
                  unstreamable_friendly_t{ 7 });
  EXPECT_EQ (g_calls, 1);
  EXPECT_EQ (g_level, LumexJsonDiagnosticLevel::warning);
  EXPECT_EQ (g_message, "key 'k' = 42, item#7");
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenReporter_WhenDebugOrInfo_ThenStillDelivered)
{
  set_diagnostic_reporter (&record);
  Detail::report (LumexJsonDiagnosticLevel::debug, "d");
  EXPECT_EQ (g_level, LumexJsonDiagnosticLevel::debug);
  Detail::report (LumexJsonDiagnosticLevel::info, "i");
  EXPECT_EQ (g_level, LumexJsonDiagnosticLevel::info);
  EXPECT_EQ (g_calls, 2);
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenNoReporter_WhenWarningOrError_ThenWrittenToStderrWithLevel)
{
  ::testing::internal::CaptureStderr ();
  Detail::report (LumexJsonDiagnosticLevel::warning, "careful");
  Detail::report (LumexJsonDiagnosticLevel::error, "broken");
  std::string const text = ::testing::internal::GetCapturedStderr ();
  EXPECT_NE (text.find ("[warning] careful"), std::string::npos);
  EXPECT_NE (text.find ("[error] broken"), std::string::npos);
}

TEST_F (LumexJsonDiagnosticsTest, GivenNoReporter_WhenDebugOrInfo_ThenDropped)
{
  ::testing::internal::CaptureStderr ();
  Detail::report (LumexJsonDiagnosticLevel::debug, "noise");
  Detail::report (LumexJsonDiagnosticLevel::info, "noise");
  EXPECT_TRUE (::testing::internal::GetCapturedStderr ().empty ());
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenThrowingReporter_WhenError_ThenFallsBackToStderrWithoutThrow)
{
  set_diagnostic_reporter (&throwing_reporter);
  ::testing::internal::CaptureStderr ();
  EXPECT_NO_THROW (
      Detail::report (LumexJsonDiagnosticLevel::error, "rescued"));
  EXPECT_NE (
      ::testing::internal::GetCapturedStderr ().find ("[error] rescued"),
      std::string::npos);
}

TEST_F (LumexJsonDiagnosticsTest,
        GivenThrowingReporter_WhenInfo_ThenDroppedWithoutThrow)
{
  set_diagnostic_reporter (&throwing_reporter);
  ::testing::internal::CaptureStderr ();
  EXPECT_NO_THROW (Detail::report (LumexJsonDiagnosticLevel::info, "quiet"));
  std::string const text = ::testing::internal::GetCapturedStderr ();
  // The slot itself may note the failing callback; the json default sink
  // must not print the info message.
  EXPECT_EQ (text.find ("[info]"), std::string::npos) << text;
  EXPECT_EQ (text.find ("quiet"), std::string::npos) << text;
}

TEST_F (LumexJsonDiagnosticsTest, GivenLevels_WhenToString_ThenNames)
{
  EXPECT_STREQ (toString (LumexJsonDiagnosticLevel::debug), "debug");
  EXPECT_STREQ (toString (LumexJsonDiagnosticLevel::info), "info");
  EXPECT_STREQ (toString (LumexJsonDiagnosticLevel::warning), "warning");
  EXPECT_STREQ (toString (LumexJsonDiagnosticLevel::error), "error");
}

TEST_F (LumexJsonDiagnosticsTest, GivenApi_WhenInspected_ThenNoexcept)
{
  static_assert (noexcept (set_diagnostic_reporter (nullptr)), "noexcept");
  static_assert (noexcept (get_diagnostic_reporter ()), "noexcept");
  static_assert (
      noexcept (Detail::report (LumexJsonDiagnosticLevel::error, "x", 1)),
      "noexcept");
  static_assert (std::is_same<json_diagnostic_fn,
                              LumexJsonDiagnosticSlot::function_type>::value,
                 "the public pointer type is the slot's function type");
  SUCCEED ();
}

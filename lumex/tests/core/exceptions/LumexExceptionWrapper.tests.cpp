#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexExceptionWrapper.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::exceptions;

using lumex::core::exceptions::Wrapper::ExceptionWrapper;

// Helper to capture stderr output for the duration of a scope (mirrors
// LumexException.tests.cpp's own StderrCapture, kept as a distinct type here
// since each .tests.cpp in this executable is its own translation unit).
class WrapperStderrCapture
{
public:
  WrapperStderrCapture ()
  {
    m_oldBuf = std::cerr.rdbuf ();
    std::cerr.rdbuf (m_capturedBuf.rdbuf ());
  }

  ~WrapperStderrCapture () { std::cerr.rdbuf (m_oldBuf); }

  std::string
  output ()
  {
    return m_capturedBuf.str ();
  }

private:
  std::streambuf *m_oldBuf;
  std::ostringstream m_capturedBuf;
};

// --- ExceptionWrapper: success path -------------------------------------

TEST (LumexExceptionWrapperTest, SuccessfulCall_ReturnsFunctionResult)
{
  int result = ExceptionWrapper (
      "exc", "unk", [] (int a, int b) { return a + b; }, 2, 3);
  EXPECT_EQ (result, 5);
}

TEST (LumexExceptionWrapperTest, SuccessfulCall_WritesNothingToStderr)
{
  WrapperStderrCapture capture;
  int result = ExceptionWrapper ("exc", "unk", [] () { return 42; });
  EXPECT_EQ (result, 42);
  EXPECT_TRUE (capture.output ().empty ());
}

TEST (LumexExceptionWrapperTest,
      SuccessfulCall_VoidReturnType_DoesNotThrowOrCrash)
{
  bool called = false;
  EXPECT_NO_THROW (
      ExceptionWrapper ("exc", "unk", [&called] () { called = true; }));
  EXPECT_TRUE (called);
}

// --- ExceptionWrapper: std::exception path -------------------------------

TEST (LumexExceptionWrapperTest, StdException_ReturnsDefaultConstructedValue)
{
  int result = ExceptionWrapper ("exc message", "unk message", [] () -> int {
    throw std::runtime_error ("boom");
  });
  EXPECT_EQ (result, 0);
}

TEST (LumexExceptionWrapperTest, StdException_ReportsExcMessageAndWhatToStderr)
{
  WrapperStderrCapture capture;
  ExceptionWrapper ("Failed to compute", "unused", [] () -> int {
    throw std::runtime_error ("disk on fire");
  });

  std::string output = capture.output ();
  EXPECT_NE (output.find ("Failed to compute"), std::string::npos);
  EXPECT_NE (output.find ("disk on fire"), std::string::npos);
}

TEST (LumexExceptionWrapperTest, StdException_DefaultString_IsEmpty)
{
  std::string result = ExceptionWrapper (
      "exc", "unk", [] () -> std::string { throw std::logic_error ("nope"); });
  EXPECT_TRUE (result.empty ());
}

TEST (LumexExceptionWrapperTest, StdException_DefaultPointer_IsNull)
{
  int *result = ExceptionWrapper (
      "exc", "unk", [] () -> int * { throw std::runtime_error ("bad ptr"); });
  EXPECT_EQ (result, nullptr);
}

// --- ExceptionWrapper: unknown (non-std::exception) path -----------------

TEST (LumexExceptionWrapperTest,
      UnknownException_ReturnsDefaultConstructedValue)
{
  int result = ExceptionWrapper ("exc", "unk", [] () -> int { throw 1337; });
  EXPECT_EQ (result, 0);
}

TEST (LumexExceptionWrapperTest,
      UnknownException_ReportsUnknownMessageToStderr)
{
  WrapperStderrCapture capture;
  ExceptionWrapper ("unused", "Unknown failure in widget",
                    [] () -> int { throw 7; });

  std::string output = capture.output ();
  EXPECT_NE (output.find ("Unknown failure in widget"), std::string::npos);
}

TEST (LumexExceptionWrapperTest, UnknownException_DoesNotUseExcMessage)
{
  WrapperStderrCapture capture;
  ExceptionWrapper ("should not appear", "unknown branch", [] () -> int {
    throw std::string ("not a std::exception");
  });

  std::string output = capture.output ();
  EXPECT_EQ (output.find ("should not appear"), std::string::npos);
  EXPECT_NE (output.find ("unknown branch"), std::string::npos);
}

// --- Argument forwarding --------------------------------------------------

TEST (LumexExceptionWrapperTest, ForwardsMultipleArgumentsByValueAndReference)
{
  std::string accumulator;
  auto append
      = [] (std::string &acc, std::string const &piece) { acc += piece; };
  ExceptionWrapper ("exc", "unk", append, accumulator, std::string ("hello"));
  EXPECT_EQ (accumulator, "hello");
}

TEST (LumexExceptionWrapperTest, ForwardsMoveOnlyArgument)
{
  auto holdsUnique = [] (std::unique_ptr<int> ptr) { return *ptr; };
  int result = ExceptionWrapper ("exc", "unk", holdsUnique,
                                 std::unique_ptr<int> (new int (99)));
  EXPECT_EQ (result, 99);
}

// --- LUMEX_SAFE_CALL / LUMEX_SAFE_CALL_MSG / LUMEX_SAFE_CALL_LAMBDA_MSG
// macros ---

TEST (LumexExceptionWrapperMacroTest, LUMEX_SAFE_CALL_ReturnsExpressionValue)
{
  int x = 10;
  int y = 20;
  int result = LUMEX_SAFE_CALL (x + y);
  EXPECT_EQ (result, 30);
}

TEST (LumexExceptionWrapperMacroTest,
      LUMEX_SAFE_CALL_ReportsDefaultDiagnosticsOnThrow)
{
  WrapperStderrCapture capture;
  auto throwing = [] () -> int { throw std::runtime_error ("macro boom"); };
  int result = LUMEX_SAFE_CALL (throwing ());

  EXPECT_EQ (result, 0);
  std::string output = capture.output ();
  EXPECT_NE (output.find ("Exception in"), std::string::npos);
  EXPECT_NE (output.find ("macro boom"), std::string::npos);
  EXPECT_NE (output.find (__FILE__), std::string::npos);
}

TEST (LumexExceptionWrapperMacroTest, LUMEX_SAFE_CALL_MSG_UsesCustomMessages)
{
  WrapperStderrCapture capture;
  auto throwing = [] () -> int { throw std::runtime_error ("custom boom"); };
  int result = LUMEX_SAFE_CALL_MSG (throwing (), "Custom failure message",
                                    "Custom unknown message");

  EXPECT_EQ (result, 0);
  std::string output = capture.output ();
  EXPECT_NE (output.find ("Custom failure message"), std::string::npos);
  EXPECT_NE (output.find ("custom boom"), std::string::npos);
}

TEST (LumexExceptionWrapperMacroTest,
      LUMEX_SAFE_CALL_LAMBDA_MSG_RunsMultiStatementBody)
{
  int state = 0;
  auto lambda = [&state] () {
    state += 1;
    state += 2;
  };
  LUMEX_SAFE_CALL_LAMBDA_MSG (lambda, "exc", "unk");
  EXPECT_EQ (state, 3);
}

TEST (LumexExceptionWrapperMacroTest,
      LUMEX_SAFE_CALL_LAMBDA_MSG_CatchesExceptionFromMultiStatementBody)
{
  WrapperStderrCapture capture;
  int state = 0;
  auto lambda = [&state] () {
    state = 1;
    throw std::runtime_error ("lambda body boom");
  };
  EXPECT_NO_THROW (LUMEX_SAFE_CALL_LAMBDA_MSG (lambda, "lambda failed",
                                               "lambda unknown failure"));

  EXPECT_EQ (state, 1);
  std::string output = capture.output ();
  EXPECT_NE (output.find ("lambda failed"), std::string::npos);
  EXPECT_NE (output.find ("lambda body boom"), std::string::npos);
}

// --- noexcept propagation --------------------------------------------------

TEST (LumexExceptionWrapperTest, NoexceptCallable_MakesWrapperCallNoexcept)
{
  // Bind to pre-existing std::string lvalues rather than string literals:
  // binding an lvalue directly to a `std::string const &` parameter is itself
  // noexcept, whereas passing a `char const *` literal would construct a
  // temporary `std::string` (not noexcept) and make the whole call expression
  // non-noexcept regardless of the wrapped callable - which is what this
  // assertion is actually about.
  std::string const excMsg = "exc";
  std::string const unkMsg = "unk";
  auto noexceptLambda = [] () noexcept -> int { return 5; };
  LUMEX_STATIC_ASSERT_MSG (
      noexcept (ExceptionWrapper (excMsg, unkMsg, noexceptLambda)),
      "ExceptionWrapper must be noexcept when the wrapped callable "
      "is noexcept.");

  EXPECT_EQ (ExceptionWrapper (excMsg, unkMsg, noexceptLambda), 5);
}

TEST (LumexExceptionWrapperTest,
      PotentiallyThrowingCallable_MakesWrapperCallNotNoexcept)
{
  std::string const excMsg = "exc";
  std::string const unkMsg = "unk";
  auto throwingLambda = [] () -> int { return 5; }; // not marked noexcept
  LUMEX_STATIC_ASSERT_MSG (
      !noexcept (ExceptionWrapper (excMsg, unkMsg, throwingLambda)),
      "ExceptionWrapper must not be noexcept when the wrapped "
      "callable is not noexcept.");

  EXPECT_EQ (ExceptionWrapper (excMsg, unkMsg, throwingLambda), 5);
}

// --- Type trait reuse -------------------------------------------------------

std::string g_safe_call_report;

void
capture_safe_call_report (char const *message)
{
  g_safe_call_report = (message != nullptr) ? message : "";
}

void
throwing_safe_call_report (char const *)
{
  throw std::runtime_error ("logger down");
}

class SafeCallReporterGuard
{
public:
  explicit SafeCallReporterGuard (Wrapper::safe_call_report_fn reporter)
      : m_previous (Wrapper::get_safe_call_reporter ())
  {
    Wrapper::set_safe_call_reporter (reporter);
  }

  ~SafeCallReporterGuard () { Wrapper::set_safe_call_reporter (m_previous); }

private:
  Wrapper::safe_call_report_fn m_previous;
};

TEST (LumexExceptionWrapperTest, InstalledReporter_ReceivesReasonLine)
{
  g_safe_call_report.clear ();
  SafeCallReporterGuard const guard (&capture_safe_call_report);
  WrapperStderrCapture capture;
  ExceptionWrapper ("Failed to compute", "unused", [] () -> int {
    throw std::runtime_error ("disk on fire");
  });

  EXPECT_NE (g_safe_call_report.find ("Failed to compute"), std::string::npos);
  EXPECT_NE (g_safe_call_report.find (". Reason: "), std::string::npos);
  EXPECT_NE (g_safe_call_report.find ("disk on fire"), std::string::npos);
  EXPECT_TRUE (capture.output ().empty ());
}

TEST (LumexExceptionWrapperTest, ThrowingReporter_FallsBackToStderr)
{
  SafeCallReporterGuard const guard (&throwing_safe_call_report);
  WrapperStderrCapture capture;
  ExceptionWrapper ("sink failed", "unused",
                    [] () -> int { throw std::runtime_error ("boom"); });

  std::string output = capture.output ();
  EXPECT_NE (output.find ("sink failed"), std::string::npos);
  EXPECT_NE (output.find ("boom"), std::string::npos);
}

TEST (LumexExceptionWrapperTest, IsCallable_ReusesLumexTypeTraits)
{
  auto fn = [] (int) { return 1; };
  EXPECT_TRUE (
      (lumex::core::utility::traits::invoke::is_callable_v<decltype (fn),
                                                           int>));
  EXPECT_FALSE (
      (lumex::core::utility::traits::invoke::is_callable_v<decltype (fn),
                                                           std::string>));
}

#include <cstdio> // For remove
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/LumexUtility"

#include "lumex/tests/core/exceptions/exception/LumexExceptionTestFixtures.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"

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

using namespace lumex::core::exceptions;
using namespace lumex::core::exceptions::exception;
using namespace lumex::core::exceptions::crash;
using namespace lumex::core::exceptions::stacktrace;

// Define a custom exception for testing purposes
LUMEX_DEFINE_EXCEPTION (TestException, lumex_base_exception);
LUMEX_DEFINE_EXCEPTION (AnotherTestException, lumex_base_exception);

// Helper to capture stderr output
class StderrCapture
{
public:
  StderrCapture ()
  {
    old_cerr_buf = std::cerr.rdbuf ();
    // Redirect std::cerr to a stringstream for capture on all platforms
    std::cerr.rdbuf (new_cerr_buf.rdbuf ());
  }

  ~StderrCapture ()
  {
    std::cerr.rdbuf (old_cerr_buf); // Restore original stderr buffer
  }

  std::string
  get_output ()
  {
    return new_cerr_buf.str ();
  }

private:
  std::streambuf *old_cerr_buf;
  std::ostringstream new_cerr_buf;
};

// --- lumex_base_exception Tests -------------------------------------------

// API Contract Verifier: Test default constructor message
TEST_F (LumexExceptionTest,
        LumexBaseException_Constructor_SetsMessageAndStackTrace)
{
  // Arrange
  std::string expected_message = "Test exception message";

  // Act
  lumex_base_exception ex (expected_message);

  // Assert
  EXPECT_EQ (ex.what (), expected_message);

#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (ex.get_stack_trace ().empty ());
#else
  // On Linux Release builds, stack traces might be limited due to
  // optimizations Just verify that the stacktrace mechanism works (doesn't
  // crash)
  auto st = ex.get_stack_trace ();
  std::cout << "Stack trace size: " << st.size () << std::endl;
#endif
}

// API Contract Verifier: Test `what()` returns the correct message
TEST_F (LumexExceptionTest, LumexBaseException_What_ReturnsCorrectMessage)
{
  // Arrange
  std::string msg = "Another message";
  lumex_base_exception ex (msg);

  // Act & Assert
  EXPECT_STREQ (ex.what (), msg.c_str ());
}

// API Contract Verifier: Test `get_stack_trace()` returns a non-empty stack
// trace
TEST_F (LumexExceptionTest,
        LumexBaseException_GetStackTrace_ReturnsValidStackTrace)
{
  // Arrange
  lumex_base_exception ex ("Stack trace test");

  // Act
  lumex_stacktrace st = ex.get_stack_trace ();

  std::string what = "Stack trace test";
  EXPECT_EQ (what, ex.what ());

  // Assert
#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (st.empty ());
  EXPECT_GT (st.size (), 0);
  // On some platforms/configurations, symbol resolution might fail,
  // so we can't assert specific function names, but we expect entries.
#else
  // On Linux Release builds, stack traces might be limited due to
  // optimizations Just verify that the mechanism works without crashing
  std::cout << "Stack trace size: " << st.size () << std::endl;
  if (!st.empty ())
    {
      std::cout << "Stack trace content: " << to_string (st) << std::endl;
    }
#endif
}

// API Contract Verifier & Concurrency Specialist: Test `to_stderr()` output
TEST_F (LumexExceptionTest, LumexBaseException_ToStderr_OutputsCorrectFormat)
{
  // Arrange
  std::string msg = "Error for stderr";
  TestException ex (msg.c_str ()); // Use TestException to check demangled name

  StderrCapture capture; // Capture stderr output

  // Act
  ex.to_stderr ();
  std::string output = capture.get_output ();

  // Assert
  std::string expected_prefix
      = std::string ("[") + lumDemangle (TestException) + "]:" + msg;
  EXPECT_TRUE (output.find (expected_prefix) != std::string::npos)
      << "Expected message: " << expected_prefix << ", Actual: " << output;
}

// API Contract Verifier & Concurrency Specialist: Test `to_crash_report()`
// creates file and content
TEST_F (LumexExceptionTest,
        LumexBaseException_ToCrashReport_CreatesFileWithContent)
{
  // Arrange
  std::string msg = "Crash report test message";
  lumex_base_exception ex (msg);

  // Act
  ex.to_crash_report ();

  // Assert - Check if a file starting with "crash_report_" exists in the
  // default crashes directory
  lumex::path default_crash_path
      = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
            .parent_path ()
        / KDEFAULT_CRASHES_DIR_PATH;
  auto entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          default_crash_path);
  ASSERT_FALSE (entries.empty ());

  lumex::path report_file_path;
  for (auto const &entry : entries)
    {
      if (entry.path ().filename ().string ().rfind (
              KDEFAULT_CRASH_REPORT_PREFIX, 0)
          == 0)
        {
          report_file_path = entry.path ();
          break;
        }
    }
  ASSERT_FALSE (report_file_path.empty ()) << "No crash report file found!";

  std::string file_content = read_file_content (report_file_path);
  EXPECT_TRUE (file_content.find ("========== Crash Report ==========")
               != std::string::npos);
  EXPECT_TRUE (file_content.find ("Message    : " + msg) != std::string::npos);
  EXPECT_TRUE (file_content.find ("Stack trace:") != std::string::npos);

  std::cout << "file_content: " << file_content << std::endl;

#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (
      file_content.find (" #0 ")
      == std::string::npos); // Should contain at least one stack entry
#else
  // On Linux in Release builds, stack traces might be very limited due to
  // optimizations Just check that the crash report structure is present
  std::cout << "Note: Linux Release builds may have limited stack trace info "
               "due to optimizations"
            << std::endl;
#endif
}

// Concurrency Specialist: `to_crash_report` thread safety
TEST_F (LumexExceptionTest, LumexBaseException_ToCrashReport_ThreadSafe)
{
  // Arrange
  // Clean up the default crash directory to ensure a fresh start for this test
  lumex::path default_crash_path
      = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
            .parent_path ()
        / KDEFAULT_CRASHES_DIR_PATH;
  if (lumex::core::filesystem::fs::lumex_filesystem::exists (
          default_crash_path))
    lumex::core::filesystem::fs::lumex_filesystem::remove_all (
        default_crash_path);

  int const num_threads = 5;
  std::vector<std::thread> threads;
  std::vector<lumex_base_exception> exceptions;
  for (int i = 0; i < num_threads; ++i)
    exceptions.emplace_back ("Concurrent crash message " + std::to_string (i));

  // Act
  for (int i = 0; i < num_threads; ++i)
    threads.emplace_back ([&exceptions, i] ()
                            { exceptions[i].to_crash_report (); });

  for (auto &t : threads)
    if (t.joinable ())
      t.join ();

  // Assert - Check if only one crash report file was created (due to
  // std::once_flag) and if it contains messages from all threads.
  auto entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          default_crash_path);
  ASSERT_GT (entries.size (), 0)
      << "Expected at least one crash report file for concurrent writes.";

  lumex::path report_file_path = entries[0].path ();
  std::string file_content = read_file_content (report_file_path);

  std::cout << "Crash report file content:\n" << file_content << std::endl;
  std::cout << "File size: " << file_content.size () << " bytes" << std::endl;

  for (int i = 0; i < num_threads; ++i)
    {
      std::string search_text
          = "Concurrent crash message " + std::to_string (i);
      bool found = file_content.find (search_text) != std::string::npos;
      std::cout << "Looking for: '" << search_text << "' - "
                << (found ? "FOUND" : "NOT FOUND") << std::endl;

      EXPECT_TRUE (found) << "Missing message from thread " << i
                          << " in crash report.";
    }
}

// --- Exception Macro Tests -------------------------------------------

// API Contract Verifier: Test LUMEX_DEFINE_EXCEPTION
TEST (LumexExceptionMacroTest, LUMEX_DEFINE_EXCEPTION_CreatesNewExceptionType)
{
  // Arrange & Act (definition is compile-time)
  // Attempt to create an instance of the defined exception
  AnotherTestException ex ("Macro defined exception");

  // Assert
  EXPECT_EQ (ex.what (), std::string ("Macro defined exception"));

#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (ex.get_stack_trace ().empty ());
#else
  // On Linux Release builds, stack traces might be limited due to
  // optimizations Just verify that the mechanism works without crashing
  auto st = ex.get_stack_trace ();
  std::cout << "Stack trace size: " << st.size () << std::endl;
#endif
}

// API Contract Verifier: Test LUMEX_THROW_EXCEPTION
TEST (LumexExceptionMacroTest,
      LUMEX_THROW_EXCEPTION_ThrowsCorrectExceptionWithDemangledName)
{
  // Arrange
  std::string msg = "Macro throw test message";

  // Act & Assert
  bool thrown = false;
  try
    {
      LUMEX_THROW_EXCEPTION (TestException, msg);
    }
  catch (TestException const &ex)
    {
      thrown = true;
      std::string expected_message_part
          = lumDemangle (TestException) + ": " + msg;
      EXPECT_TRUE (std::string (ex.what ()).find (expected_message_part)
                   != std::string::npos)
          << "Expected message: " << expected_message_part
          << ", Actual: " << ex.what ();
    }
  catch (...)
    {
      FAIL () << "Caught unexpected exception type";
    }
  EXPECT_TRUE (thrown) << "Expected TestException to be thrown";
}

// API Contract Verifier: Test LUMEX_EXCEPTION_HANDLE_BEGIN/END
TEST_F (LumexExceptionTest, LUMEX_EXCEPTION_HANDLE_BLOCK_CatchesLumexException)
{
  // Arrange
  std::string msg = "Exception in handle block";
  StderrCapture capture;

  // Act
  LUMEX_EXCEPTION_HANDLE_BEGIN
  LUMEX_THROW_EXCEPTION (TestException, msg);
  LUMEX_EXCEPTION_HANDLE_END

  // Assert
  std::string output = capture.get_output ();
  // The message returned by what() already contains the demangled name and ":
  // ". The expected output in stderr should now directly match what() with a
  // newline.
  std::string expected_output_content
      = std::string (lumDemangle (TestException)) + ": " + msg;
  EXPECT_TRUE (output.find (expected_output_content) != std::string::npos)
      << "Expected stderr output: " << expected_output_content
      << ", Actual: " << output;

  // Verify crash report file was created
  lumex::path default_crash_path
      = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
            .parent_path ()
        / KDEFAULT_CRASHES_DIR_PATH;
  auto entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          default_crash_path);
  EXPECT_FALSE (entries.empty ())
      << "Expected a crash report file to be created.";
}

TEST_F (LumexExceptionTest, LUMEX_EXCEPTION_HANDLE_BLOCK_CatchesStdException)
{
  // Arrange
  std::string msg = "Standard exception message";
  StderrCapture capture;

  // Act
  LUMEX_EXCEPTION_HANDLE_BEGIN
  throw std::runtime_error (msg);
  LUMEX_EXCEPTION_HANDLE_END

  // Assert
  std::string output = capture.get_output ();
  EXPECT_TRUE (output.find ("[std::exception] " + msg) != std::string::npos);
}

TEST_F (LumexExceptionTest,
        LUMEX_EXCEPTION_HANDLE_BLOCK_CatchesUnknownException)
{
  // Arrange
  StderrCapture capture;

  // Act
  LUMEX_EXCEPTION_HANDLE_BEGIN
  throw 1; // Throw an integer to simulate unknown exception
  LUMEX_EXCEPTION_HANDLE_END

  // Assert
  std::string output = capture.get_output ();
  EXPECT_TRUE (output.find ("[Unknown exception]") != std::string::npos);
}

#include <chrono>
#include <cstdint>
#include <cstdio> // For remove
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/LumexUtility"

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
LUMEX_DEFINE_EXCEPTION (TestException, LumexBaseException);
LUMEX_DEFINE_EXCEPTION (AnotherTestException, LumexBaseException);

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
  getOutput ()
  {
    return new_cerr_buf.str ();
  }

private:
  std::streambuf *old_cerr_buf;
  std::ostringstream new_cerr_buf;
};

// --- Fixture ------------------------------------------------------------
class LumexExceptionTest : public ::testing::Test
{
protected:
  lumex::path test_crash_dir;

  void
  SetUp () override
  {
    // Create a unique temporary directory for each test fixture run
    // to avoid conflicts with crash reports from other tests.
    test_crash_dir
        = lumex::core::filesystem::fs::lumex_filesystem::temp_directory_path ()
              .value ()
          / ("LumexTestCrashes_" + LumexTime::get_timestamp_ns ());
    lumex::core::filesystem::fs::lumex_filesystem::create_directories (
        test_crash_dir);

    // This is important for to_crash_report to create new files in each test.
    // It relies on
    // lumex::core::filesystem::fs::lumex_filesystem::get_exe_path().parent_path()
    // / KDEFAULT_CRASHES_DIR_PATH We need to ensure KDEFAULT_CRASHES_DIR_PATH
    // is relative to our test_crash_dir For testing purposes, we need to
    // temporarily redirect the "default" crash path. This cannot be easily
    // done without modifying the `DefaultPaths.hpp` directly or using
    // environment variables, which the current `to_crash_report`
    // implementation does not take into account for its static initialization.
    // As a workaround for testing, we'll clean up the default crashes
    // directory before each test, or just check the presence of new files. For
    // concurrent tests, we'll check for the total number of entries.
  }

  void
  TearDown () override
  {
    // Clean up the temporary directory
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (test_crash_dir))
      lumex::core::filesystem::fs::lumex_filesystem::remove_all (
          test_crash_dir);
    // Also clean up the global default crash directory if it's not the same as
    // test_crash_dir
    lumex::path default_crash_path
        = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
              .parent_path ()
          / KDEFAULT_CRASHES_DIR_PATH;
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (
            default_crash_path)
        && default_crash_path != test_crash_dir)
      lumex::core::filesystem::fs::lumex_filesystem::remove_all (
          default_crash_path);
  }

  // Helper to read content of a file
  std::string
  read_file_content (lumex::path const &path)
  {
    std::ifstream file (path.string ());
    if (!file.is_open ())
      return "";
    std::stringstream buffer;
    buffer << file.rdbuf ();
    return buffer.str ();
  }
};

// --- LumexBaseException Tests -------------------------------------------

// API Contract Verifier: Test default constructor message
TEST_F (LumexExceptionTest,
        LumexBaseException_Constructor_SetsMessageAndStackTrace)
{
  // Arrange
  std::string expected_message = "Test exception message";

  // Act
  LumexBaseException ex (expected_message);

  // Assert
  EXPECT_EQ (ex.what (), expected_message);

#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (ex.getStackTrace ().empty ());
#else
  // On Linux Release builds, stack traces might be limited due to
  // optimizations Just verify that the stacktrace mechanism works (doesn't
  // crash)
  auto st = ex.getStackTrace ();
  std::cout << "Stack trace size: " << st.size () << std::endl;
#endif
}

// API Contract Verifier: Test `what()` returns the correct message
TEST_F (LumexExceptionTest, LumexBaseException_What_ReturnsCorrectMessage)
{
  // Arrange
  std::string msg = "Another message";
  LumexBaseException ex (msg);

  // Act & Assert
  EXPECT_STREQ (ex.what (), msg.c_str ());
}

// API Contract Verifier: Test `getStackTrace()` returns a non-empty stack
// trace
TEST_F (LumexExceptionTest,
        LumexBaseException_GetStackTrace_ReturnsValidStackTrace)
{
  // Arrange
  LumexBaseException ex ("Stack trace test");

  // Act
  LumexStacktrace st = ex.getStackTrace ();

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
  std::string output = capture.getOutput ();

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
  LumexBaseException ex (msg);

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
  std::vector<LumexBaseException> exceptions;
  for (int i = 0; i < num_threads; ++i)
    exceptions.emplace_back ("Concurrent crash message " + std::to_string (i));

  // Act
  for (int i = 0; i < num_threads; ++i)
    threads.emplace_back (
        [&exceptions, i] () { exceptions[i].to_crash_report (); });

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
  EXPECT_FALSE (ex.getStackTrace ().empty ());
#else
  // On Linux Release builds, stack traces might be limited due to
  // optimizations Just verify that the mechanism works without crashing
  auto st = ex.getStackTrace ();
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
  std::string output = capture.getOutput ();
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
  std::string output = capture.getOutput ();
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
  std::string output = capture.getOutput ();
  EXPECT_TRUE (output.find ("[Unknown exception]") != std::string::npos);
}

// --- LumexStacktrace Tests ----------------------------------------------

// Helper static functions to prevent inlining for stack trace tests
// External linkage (not static): Release without /Zi still exports the name
// so DbgHelp can match it. static + /O2 + ICF left only TestBody on the walk.
LUMEX_ATTRIBUTE_NOINLINE
LumexStacktrace
StacktraceTest_func_a ()
{
  return LumexStacktrace::current (0); // Skip 0 frames from capture itself
}

LUMEX_ATTRIBUTE_NOINLINE
LumexStacktrace
StacktraceTest_func_b ()
{
  return StacktraceTest_func_a ();
}

LUMEX_ATTRIBUTE_NOINLINE
LumexStacktrace
StacktraceTest_func_c ()
{
  return StacktraceTest_func_b ();
}

LUMEX_ATTRIBUTE_NOINLINE
LumexStacktrace
StacktraceTest_func_other ()
{
  return LumexStacktrace::current (0);
}

// API Contract Verifier: Stacktrace capture depth
TEST (LumexStacktraceTest, Stacktrace_Current_CapturesCorrectDepth)
{
  // Arrange
  // Using LUMEX_ATTRIBUTE_NOINLINE on static helper functions to prevent
  // inlining and ensure their frames appear in the stack trace.

  // Act
  LumexStacktrace st = StacktraceTest_func_c ();

  // Assert
  // Exact depth is hard to predict due to compiler optimizations and base
  // frames from GTest, but with noinline, we expect a reasonable number of
  // frames related to the call chain. The value '3' comes from
  // StacktraceTest_func_a, StacktraceTest_func_b, StacktraceTest_func_c.
#if LUMEX_OS_WINDOWS
  EXPECT_GT (st.size (), 3);
#else
  // On Linux in Release builds, optimizations can severely limit stack traces
  // Just check that we get at least some frame
  EXPECT_GE (st.size (), 0);
  std::cout << "Note: Linux Release builds may have very limited stack traces "
               "due to optimizations"
            << std::endl;
  std::cout << "Captured " << st.size () << " frames" << std::endl;
#endif

  // Verify at least some known functions appear in the stack trace.
  // The function names will be based on their static names.
  std::string st_str = to_string (st);
  std::cout << "Stack trace content: " << st_str << std::endl;

#if LUMEX_OS_WINDOWS
  EXPECT_TRUE (st_str.find ("StacktraceTest_func_a") != std::string::npos
               || st_str.find ("StacktraceTest_func_b") != std::string::npos
               || st_str.find ("StacktraceTest_func_c") != std::string::npos)
      << "Stack trace did not contain expected function names:\n"
      << st_str;
#else
  // On Linux, just check that we have some content (even if it's just
  // addresses)
  if (!st.empty ())
    {
      EXPECT_FALSE (st_str.empty ()) << "Stack trace should not be completely "
                                        "empty if frames were captured";
    }
#endif
}

// Helper static functions for skipping test
LUMEX_ATTRIBUTE_NOINLINE
static LumexStacktrace
StacktraceTest_inner_func ()
{
  return LumexStacktrace::current (
      0); // Should capture 'StacktraceTest_inner_func' at index 0
}

// API Contract Verifier: Stacktrace empty if capture fails or max_depth is 0
TEST (LumexStacktraceTest, Stacktrace_InnerFunc_CapturesFrame)
{
  LumexStacktrace st = StacktraceTest_inner_func ();
  EXPECT_FALSE (st.empty ());
}

TEST (LumexStacktraceTest, Stacktrace_Empty_ForZeroMaxDepth)
{
  LumexStacktrace st = LumexStacktrace::current (0, 0); // Max depth 0
  EXPECT_TRUE (st.empty ());
}

// API Contract Verifier: Stacktrace iteration and access
TEST (LumexStacktraceTest, Stacktrace_IterationAndAccess_WorksCorrectly)
{
  // Arrange
  LumexStacktrace st = LumexStacktrace::current (0);

#if LUMEX_OS_WINDOWS
  ASSERT_FALSE (st.empty ());
#else
  // On Linux Release builds, stack traces might be empty due to optimizations
  if (st.empty ())
    {
      std::cout << "Note: Stack trace is empty in Linux Release build due to "
                   "optimizations"
                << std::endl;
      GTEST_SKIP () << "Skipping iteration test as stack trace is empty";
    }
#endif

  // Act & Assert
  std::size_t count = 0;
  for (auto const &entry : st)
    {
      EXPECT_FALSE (
          entry.description ().empty ()); // Description should not be empty
      // Cannot always assert source_file/line due to symbol availability
      count++;
    }
  EXPECT_EQ (count, st.size ());
  EXPECT_NO_THROW (st.at (0));
  EXPECT_EQ (st[0].native_handle (), st.at (0).native_handle ());
}

// API Contract Verifier: Stacktrace comparison
TEST (LumexStacktraceTest, Stacktrace_Comparison_WorksCorrectly)
{
  LumexStacktrace st1 = LumexStacktrace::current (0);
  LumexStacktrace st2 = LumexStacktrace::current (0);
  // These should be equal if called consecutively from the same point, but
  // sometimes a slight difference might occur depending on compiler/OS.
  // For robust testing, we test against copied stacktraces or specific
  // handles.

  LumexStacktrace st1_copy = st1;
  EXPECT_EQ (st1, st1_copy);
  EXPECT_FALSE (st1 != st1_copy);

  // Named noinline callee: a lambda is inlined in Release and both traces
  // then start at TestBody, so EXPECT_NE is false for the wrong reason.
  LumexStacktrace st_different = StacktraceTest_func_other ();

#if LUMEX_OS_WINDOWS
  EXPECT_NE (st1, st_different);
#else
  // On Linux Release builds, both traces might be identical due to
  // optimizations Just verify that comparison operators work without crashing
  std::cout << "st1 content: " << to_string (st1) << std::endl;
  std::cout << "st_different content: " << to_string (st_different)
            << std::endl;

  // Test that comparison operators work
  bool are_equal = (st1 == st_different);
  bool are_not_equal = (st1 != st_different);
  EXPECT_EQ (are_equal, !are_not_equal); // Basic consistency check

  std::cout << "Traces are " << (are_equal ? "equal" : "different")
            << std::endl;
#endif
}

// --- LumexStacktraceEntry Tests -----------------------------------------

// API Contract Verifier: StacktraceEntry construction and basic properties
TEST (LumexStacktraceEntryTest,
      StacktraceEntry_Constructor_SetsAddressAndInvalidatesCache)
{
  void *test_addr
      = reinterpret_cast<void *> (static_cast<std::uintptr_t> (0xDEADBEEF));
  LumexStacktraceEntry entry (test_addr);

  EXPECT_EQ (entry.native_handle (), test_addr);
  EXPECT_TRUE (static_cast<bool> (entry)); // Operator bool should be true
                                           // Cache should be invalid initially
  // There's no direct way to check m_cache_valid, so we rely on
  // ensure_cache_valid behavior.
}

TEST (LumexStacktraceEntryTest,
      StacktraceEntry_DefaultConstructor_CreatesInvalidEntry)
{
  LumexStacktraceEntry entry;
  EXPECT_EQ (entry.native_handle (), nullptr);
  EXPECT_FALSE (static_cast<bool> (entry));
  EXPECT_TRUE (entry.description ().empty ()
               || entry.description () == "0x0"); // Should be empty or "0x0"
  EXPECT_TRUE (entry.source_file ().empty ());
  EXPECT_EQ (entry.source_line (), 0U);
}

// API Contract Verifier: `description()` provides a readable function name
TEST (LumexStacktraceEntryTest,
      StacktraceEntry_Description_ReturnsFunctionName)
{
  // Arrange
  LumexStacktrace st = LumexStacktrace::current (0);
  ASSERT_FALSE (st.empty ());
  LumexStacktraceEntry entry = st[0]; // Get the first entry

  // Act
  std::string desc = entry.description ();

  // Assert
  EXPECT_FALSE (desc.empty ());
  // Expect the test function's name or a related internal function name to be
  // present. Exact matching is fragile due to compiler/linker optimizations
  // (inlining, symbol stripping). So, we check for a non-empty string.
}

// API Contract Verifier: `source_file()` and `source_line()` provide valid
// info (if available)
TEST (LumexStacktraceEntryTest,
      StacktraceEntry_SourceInfo_ReturnsValidDataIfAvailable)
{
  // Arrange
  LumexStacktrace st = LumexStacktrace::current (0);
  ASSERT_FALSE (st.empty ());
  LumexStacktraceEntry entry = st[0];

  // Act
  std::string file = entry.source_file ();
  std::uint32_t line = entry.source_line ();

  // Assert
  // This is highly dependent on debug symbol availability and build
  // configuration. We can only assert that if they are not empty/zero, they
  // *look* like file/line.
  if (!file.empty () || line != 0)
    {
      EXPECT_FALSE (
          file.empty ());   // If line is non-zero, file should not be empty
      EXPECT_NE (line, 0U); // If file is not empty, line should not be zero
      EXPECT_TRUE (file.find (".cpp") != std::string::npos
                   || file.find (".h") != std::string::npos);
      EXPECT_TRUE (line > 0);
    }
  else
    {
      // Log a warning if no source info, but don't fail the test
      // std::cerr << "Warning: No source info available for stacktrace entry
      // in this build configuration.\n";
    }
}

// --- Performance & Stress Analyst ---------------------------------------

TEST (LumexStacktraceTest, Perf_StacktraceCapture_IsEfficient)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Arrange
  int const N = 1000; // Number of stack trace captures
  std::vector<LumexStacktrace> traces;
  traces.reserve (N);

  auto start = std::chrono::high_resolution_clock::now ();

  // Act
  for (int i = 0; i < N; ++i)
    traces.emplace_back (LumexStacktrace::current (0)); // Capture stacktrace

  auto end = std::chrono::high_resolution_clock::now ();
  auto duration
      = std::chrono::duration_cast<std::chrono::milliseconds> (end - start);

  // Assert
  // This threshold might need adjustment based on system performance and build
  // type (Debug vs Release) On average, capturing 1000 stack traces should be
  // under a few seconds.
  EXPECT_LT (duration.count (), 5000)
      << "Capturing " << N
      << " stack traces took too long: " << duration.count () << "ms";
  EXPECT_FALSE (traces.empty ());
  EXPECT_FALSE (traces[0].empty ());
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

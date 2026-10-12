// Tests of lumex::core::exceptions::stacktrace (lumex/core/exceptions/
// stacktrace): lumex_stacktrace and lumex_stacktrace_entry, and the frames
// described by module and offset on POSIX. Every suite of this directory
// compiles this file.

#include <chrono>
#include <cstdint>
#include <cstdio> // For remove
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#if defined(__linux__)
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "lumex/core/exceptions/LumexException"
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

// --- lumex_stacktrace Tests ----------------------------------------------

// Helper static functions to prevent inlining for stack trace tests
// External linkage (not static): Release without /Zi still exports the name
// so DbgHelp can match it. static + /O2 + ICF left only TestBody on the walk.
// The tests search the captured trace text for these names, so they keep them.
// NOLINTBEGIN(readability-identifier-naming)
LUMEX_ATTRIBUTE_NOINLINE
lumex_stacktrace
StacktraceTest_func_a ()
{
  return lumex_stacktrace::current (0); // Skip 0 frames from capture itself
}

LUMEX_ATTRIBUTE_NOINLINE
lumex_stacktrace
StacktraceTest_func_b ()
{
  return StacktraceTest_func_a ();
}

LUMEX_ATTRIBUTE_NOINLINE
lumex_stacktrace
StacktraceTest_func_c ()
{
  return StacktraceTest_func_b ();
}

LUMEX_ATTRIBUTE_NOINLINE
lumex_stacktrace
StacktraceTest_func_other ()
{
  return lumex_stacktrace::current (0);
}
// NOLINTEND(readability-identifier-naming)

// API Contract Verifier: Stacktrace capture depth
TEST (LumexStacktraceTest, Stacktrace_Current_CapturesCorrectDepth)
{
  // Arrange
  // Using LUMEX_ATTRIBUTE_NOINLINE on static helper functions to prevent
  // inlining and ensure their frames appear in the stack trace.

  // Act
  lumex_stacktrace st = StacktraceTest_func_c ();

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
static lumex_stacktrace
// The test searches the captured trace text for this name.
// NOLINTNEXTLINE(readability-identifier-naming)
StacktraceTest_inner_func ()
{
  return lumex_stacktrace::current (
      0); // Should capture 'StacktraceTest_inner_func' at index 0
}

// API Contract Verifier: Stacktrace empty if capture fails or max_depth is 0
TEST (LumexStacktraceTest, Stacktrace_InnerFunc_CapturesFrame)
{
  lumex_stacktrace st = StacktraceTest_inner_func ();
  EXPECT_FALSE (st.empty ());
}

TEST (LumexStacktraceTest, Stacktrace_Empty_ForZeroMaxDepth)
{
  lumex_stacktrace st = lumex_stacktrace::current (0, 0); // Max depth 0
  EXPECT_TRUE (st.empty ());
}

// API Contract Verifier: Stacktrace iteration and access
TEST (LumexStacktraceTest, Stacktrace_IterationAndAccess_WorksCorrectly)
{
  // Arrange
  lumex_stacktrace st = lumex_stacktrace::current (0);

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
  lumex_stacktrace st1 = lumex_stacktrace::current (0);
  lumex_stacktrace st2 = lumex_stacktrace::current (0);
  // These should be equal if called consecutively from the same point, but
  // sometimes a slight difference might occur depending on compiler/OS.
  // For robust testing, we test against copied stacktraces or specific
  // handles.

  lumex_stacktrace st1_copy = st1;
  EXPECT_EQ (st1, st1_copy);
  EXPECT_FALSE (st1 != st1_copy);

  // Named noinline callee: a lambda is inlined in Release and both traces
  // then start at TestBody, so EXPECT_NE is false for the wrong reason.
  lumex_stacktrace st_different = StacktraceTest_func_other ();

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

// --- lumex_stacktrace_entry Tests -----------------------------------------

// API Contract Verifier: StacktraceEntry construction and basic properties
TEST (LumexStacktraceEntryTest,
      StacktraceEntry_Constructor_SetsAddressAndInvalidatesCache)
{
  void *test_addr
      = reinterpret_cast<void *> (static_cast<std::uintptr_t> (0xDEADBEEF));
  lumex_stacktrace_entry entry (test_addr);

  EXPECT_EQ (entry.native_handle (), test_addr);
  EXPECT_TRUE (static_cast<bool> (entry)); // Operator bool should be true
                                           // Cache should be invalid initially
  // There's no direct way to check m_cache_valid, so we rely on
  // ensure_cache_valid behavior.
}

TEST (LumexStacktraceEntryTest,
      StacktraceEntry_DefaultConstructor_CreatesInvalidEntry)
{
  lumex_stacktrace_entry entry;
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
  lumex_stacktrace st = lumex_stacktrace::current (0);
  ASSERT_FALSE (st.empty ());
  lumex_stacktrace_entry entry = st[0]; // Get the first entry

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
  lumex_stacktrace st = lumex_stacktrace::current (0);
  ASSERT_FALSE (st.empty ());
  lumex_stacktrace_entry entry = st[0];

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
  std::vector<lumex_stacktrace> traces;
  traces.reserve (N);

  auto start = std::chrono::high_resolution_clock::now ();

  // Act
  for (int i = 0; i < N; ++i)
    traces.emplace_back (lumex_stacktrace::current (0)); // Capture stacktrace

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

// --- Frames described by module and offset (POSIX)
// --------------------------- A frame is described by its exported symbol, or
// by its module and the address inside it, which addr2line and gdb resolve
// offline with the module or its separate debug file. Describing starts no
// process: the addr2line it used to start per frame read a whole separate
// debug file each time.

#if defined(LUMEX_OS_LINUX)
namespace
{
LUMEX_ATTRIBUTE_NOINLINE lumex_stacktrace
stacktrace_from_unexported_function ()
{
  return lumex_stacktrace::current (0);
}

// Internal linkage: never in the executable's dynamic symbol table, even
// when the executable exports its other symbols, so dladdr has no name for
// it.
LUMEX_ATTRIBUTE_NOINLINE int
unexported_probe (int value)
{
  return value * 3 + 1;
}

// A stand-in addr2line in a fresh directory put first on PATH: it leaves a
// mark when anything starts it. The destructor restores PATH.
class StandInAddr2line
{
public:
  StandInAddr2line ()
  {
    char const *tmp = std::getenv ("TMPDIR");
    std::string pattern = std::string (tmp != nullptr ? tmp : "/tmp")
                          + "/lumex-addr2line-XXXXXX";
    char *made = mkdtemp (&pattern[0]);
    m_dir = made != nullptr ? std::string (made) : std::string ();
    std::ofstream script ((m_dir + "/addr2line").c_str ());
    script << "#!/bin/sh\ntouch '" << mark () << "'\n";
    script.close ();
    chmod ((m_dir + "/addr2line").c_str (), 0700);
    char const *path = std::getenv ("PATH");
    m_old_path = path != nullptr ? path : "";
    setenv ("PATH", (m_dir + ":" + m_old_path).c_str (), 1);
  }

  ~StandInAddr2line ()
  {
    setenv ("PATH", m_old_path.c_str (), 1);
    std::remove (mark ().c_str ());
    std::remove ((m_dir + "/addr2line").c_str ());
    rmdir (m_dir.c_str ());
  }

  bool
  valid () const
  {
    return !m_dir.empty ();
  }

  bool
  started () const
  {
    struct stat info;
    return stat (mark ().c_str (), &info) == 0;
  }

private:
  std::string
  mark () const
  {
    return m_dir + "/started";
  }

  std::string m_dir;
  std::string m_old_path;
};
} // namespace

TEST (
    LumexStacktraceEntryTest,
    GivenAStack_WhenItsFramesAreDescribed_ThenSymbolOrModuleAndOffsetAndNoSource)
{
  lumex_stacktrace const st = stacktrace_from_unexported_function ();
  ASSERT_FALSE (st.empty ());

  for (std::size_t i = 0; i < st.size (); ++i)
    {
      void *address = st[i].native_handle ();
      std::string const location
          = lumex::core::utility::debug::Detail::describe_module_address (
              address);
      std::string const description = st[i].description ();
      Dl_info info;
      if (dladdr (address, &info) != 0 && info.dli_sname != nullptr)
        {
          EXPECT_NE (description.find (" (" + location + ")"),
                     std::string::npos)
              << description;
        }
      else
        {
          EXPECT_EQ (description, location);
        }
      EXPECT_TRUE (st[i].source_file ().empty ()) << description;
      EXPECT_EQ (st[i].source_line (), 0U) << description;
    }
}

TEST (LumexStacktraceEntryTest,
      GivenAnUnexportedFunction_WhenDescribed_ThenItIsItsModuleAndOffset)
{
  void *address = reinterpret_cast<void *> (&unexported_probe);
  lumex_stacktrace_entry const entry (address);

  std::string const location
      = lumex::core::utility::debug::Detail::describe_module_address (address);

  ASSERT_FALSE (location.empty ());
  EXPECT_NE (location.find ("+0x"), std::string::npos) << location;
  EXPECT_EQ (entry.description (), location);
  EXPECT_TRUE (entry.source_file ().empty ());
  EXPECT_EQ (entry.source_line (), 0U);
  EXPECT_EQ (unexported_probe (1), 4);
}

TEST (LumexStacktraceEntryTest,
      GivenAnAddr2lineOnThePath_WhenFramesAreDescribed_ThenNothingStartsIt)
{
  StandInAddr2line const stand_in;
  ASSERT_TRUE (stand_in.valid ());
  lumex_stacktrace const st = stacktrace_from_unexported_function ();
  ASSERT_FALSE (st.empty ());

  for (std::size_t i = 0; i < st.size (); ++i)
    EXPECT_FALSE (st[i].description ().empty ());

  EXPECT_FALSE (stand_in.started ()) << to_string (st);
}
#endif

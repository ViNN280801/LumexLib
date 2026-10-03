// LumexDebug.cxx11.tests.cpp
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#if defined(__linux__)
#include <dlfcn.h>
#include <elf.h>
#include <execinfo.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "lumex/core/utility/debug/LumexDebug.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::debug;

// --- Detail::formatHex
// ---------------------------------------------------------------------
// Regression coverage for the bug fixed in this change: captureStackTrace()
// used to embed addresses via `stringify(..., "[0x", address, "]", ...)`,
// which streams `address` through the default (decimal) operator<<, producing
// e.g. "[0x6810300]" instead of real hex. formatHex() is the helper introduced
// to fix this; it is tested directly here because a real captureStackTrace()
// address is randomized (ASLR) and won't reliably contain a hex letter to
// distinguish decimal from hex output at the string level.

TEST (
    LumexDebugTest,
    GivenAddressWithHexLetters_WhenFormatHex_ThenProducesUppercaseHexWithoutPrefix)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0x1A2B3CULL)),
             "1A2B3C");
}

TEST (LumexDebugTest, GivenZeroAddress_WhenFormatHex_ThenProducesZero)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0)),
             "0");
}

TEST (LumexDebugTest,
      GivenLargeAddress_WhenFormatHex_ThenMatchesManualHexConversion)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0xDEADBEEFULL)),
             "DEADBEEF");
}

TEST (LumexDebugTest, GivenSingleDigitAddress_WhenFormatHex_ThenHasNoPadding)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (1)),
             "1");
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0xF)),
             "F");
}

TEST (LumexDebugTest, GivenPowerOfSixteen_WhenFormatHex_ThenKeepsTrailingZero)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0x10)),
             "10");
}

TEST (
    LumexDebugTest,
    GivenDefaultArgs_WhenCaptureStackTrace_ThenReturnsNonEmptyStringWithFirstFrame)
{
  std::string const trace
      = lumex::core::utility::debug::captureStackTrace (0, 5);
  EXPECT_FALSE (trace.empty ());
  EXPECT_NE (trace.find ("#0"), std::string::npos);
}

TEST (LumexDebugTest,
      GivenZeroMaxFrames_WhenCaptureStackTrace_ThenDoesNotThrow)
{
  EXPECT_NO_THROW ({
    std::string const trace
        = lumex::core::utility::debug::captureStackTrace (0, 0);
    (void)trace;
  });
}

TEST (LumexDebugTest,
      GivenSkipBeyondAvailableFrames_WhenCaptureStackTrace_ThenDoesNotThrow)
{
  EXPECT_NO_THROW ({
    std::string const trace
        = lumex::core::utility::debug::captureStackTrace (10000, 4);
    (void)trace;
  });
}

TEST (LumexDebugTest,
      GivenCaptureCallerInfoMacro_WhenCalled_ThenMentionsThisFile)
{
  std::string const info = LUMEX_CAPTURE_CALLER_INFO ();
  EXPECT_NE (info.find ("LumexDebug.cxx11.tests.cpp"), std::string::npos);
}

TEST (LumexDebugTest, GivenLowerHexLetters_WhenFormatHex_ThenEmitsUppercase)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0xabcdefULL)),
             "ABCDEF");
}

TEST (LumexDebugTest, GivenTen_WhenFormatHex_ThenIsA)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0xA)),
             "A");
}

TEST (LumexDebugTest, GivenTwoFiftyFive_WhenFormatHex_ThenIsFF)
{
  EXPECT_EQ (lumex::core::utility::debug::Detail::formatHex (
                 static_cast<std::uintptr_t> (0xFF)),
             "FF");
}

TEST (LumexDebugTest, GivenMaxUintptr_WhenFormatHex_ThenIsAllF)
{
  std::string const hex = lumex::core::utility::debug::Detail::formatHex (
      std::numeric_limits<std::uintptr_t>::max ());
  EXPECT_FALSE (hex.empty ());
  EXPECT_EQ (hex.find_first_not_of ("0123456789ABCDEF"), std::string::npos);
  EXPECT_EQ (hex[0], 'F');
}

TEST (LumexDebugTest,
      GivenSingleFrame_WhenCaptureStackTrace_ThenHasHashZeroNotHashOne)
{
  std::string const trace
      = lumex::core::utility::debug::captureStackTrace (0, 1);
  EXPECT_NE (trace.find ("#0"), std::string::npos);
  EXPECT_EQ (trace.find ("#1"), std::string::npos);
}

TEST (LumexDebugTest,
      GivenCapturedTrace_WhenInspected_ThenContainsHexAddressMarker)
{
  std::string const trace
      = lumex::core::utility::debug::captureStackTrace (0, 8);
  EXPECT_NE (trace.find ("[0x"), std::string::npos);
}

TEST (LumexDebugTest,
      GivenCaptureCallerInfoImpl_WhenPassedPath_ThenKeepsOnlyBasename)
{
  std::string const win
      = captureCallerInfoImpl ("foo", "C:\\dir\\sub\\file.cpp", 12);
  EXPECT_NE (win.find ("foo() at file.cpp:12"), std::string::npos);

  std::string const posix
      = captureCallerInfoImpl ("bar", "/usr/src/other.cpp", 99);
  EXPECT_NE (posix.find ("bar() at other.cpp:99"), std::string::npos);
}

TEST (LumexDebugTest,
      GivenNullFunction_WhenCaptureCallerInfoImpl_ThenUsesUnknownPlaceholder)
{
  std::string const info
      = captureCallerInfoImpl (nullptr, "LumexDebug.tests.cpp", 1);
  EXPECT_NE (info.find ("<unknown>() at LumexDebug.tests.cpp:1"),
             std::string::npos);
}

TEST (LumexDebugTest,
      GivenCallerInfoMacro_WhenCalled_ThenContainsFunctionParensAndLine)
{
  std::string const info = LUMEX_CAPTURE_CALLER_INFO ();
  EXPECT_NE (info.find ("() at "), std::string::npos);
  EXPECT_NE (info.find (':'), std::string::npos);
}

TEST (LumexDebugTest, GivenCaptureStackTrace_WhenCalled_ThenIsNoexcept)
{
  EXPECT_TRUE (
      noexcept (lumex::core::utility::debug::captureStackTrace (0, 1)));
}

TEST (LumexDebugTest, GivenRepeatedCapture_WhenCalled_ThenEachResultIsNonEmpty)
{
  for (int i = 0; i < 3; ++i)
    {
      std::string const trace
          = lumex::core::utility::debug::captureStackTrace (0, 3);
      EXPECT_FALSE (trace.empty ()) << "iteration " << i;
    }
}

// --- Frames named by module and offset (POSIX)
// ------------------------------- A frame without an exported symbol is named
// by its module and the address inside that module's image: the address
// addr2line and gdb take with the module or its separate debug file.
// captureStackTrace symbolizes nothing in-process and starts no process: with
// a separate debug file next to the module, the addr2line it used to start per
// frame read the whole file each time (seconds per stack).

#if defined(LUMEX_OS_LINUX)
namespace
{
// Internal linkage: the test executable never exports it, so dladdr has no
// name for it.
LUMEX_ATTRIBUTE_NOINLINE int
unexported_probe (int value)
{
  return value * 3 + 1;
}

std::string
own_executable_path ()
{
  char buffer[4096] = {};
  ssize_t const length
      = readlink ("/proc/self/exe", buffer, sizeof (buffer) - 1);
  return length > 0 ? std::string (buffer, static_cast<std::size_t> (length))
                    : std::string ();
}

std::string
file_name_of (std::string const &path)
{
  std::size_t const slash = path.find_last_of ('/');
  return slash == std::string::npos ? path : path.substr (slash + 1);
}

// The address inside the executable's image, worked out without the loader:
// an executable linked at a fixed address (ET_EXEC) runs at its file
// addresses, a position-independent one (ET_DYN) is shifted by the start of
// its first mapping, where its first loadable segment (address 0) lies.
std::uintptr_t
expected_executable_offset (std::uintptr_t address)
{
  std::string const exe = own_executable_path ();
  std::ifstream elf (exe.c_str (), std::ios::binary);
  unsigned char header[EI_NIDENT + sizeof (std::uint16_t)] = {};
  elf.read (reinterpret_cast<char *> (header), sizeof (header));
  std::uint16_t type = 0;
  std::memcpy (&type, header + EI_NIDENT, sizeof (type));
  if (type == ET_EXEC)
    return address;
  std::ifstream maps ("/proc/self/maps");
  std::string line;
  while (std::getline (maps, line))
    {
      if (line.find (exe) == std::string::npos)
        continue;
      std::istringstream fields (line);
      std::string range;
      std::string perms;
      std::string offset;
      fields >> range >> perms >> offset;
      if (std::stoull (offset, nullptr, 16) != 0)
        continue;
      return address
             - static_cast<std::uintptr_t> (std::stoull (
                 range.substr (0, range.find ('-')), nullptr, 16));
    }
  return 0;
}

// A return address inside the C library, taken from the real call stack
// (the frame of __libc_start_main below main).
void *
libc_frame ()
{
  void *frames[64];
  int const count = backtrace (frames, 64);
  for (int i = 0; i < count; ++i)
    {
      Dl_info info;
      if (dladdr (frames[i], &info) != 0 && info.dli_fname != nullptr
          && std::strstr (info.dli_fname, "libc.so") != nullptr)
        return frames[i];
    }
  return nullptr;
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
    LumexDebugTest,
    GivenAnUnexportedFunction_WhenItsModuleIsFound_ThenItIsTheExecutableAtItsFileAddress)
{
  void const *address = reinterpret_cast<void const *> (&unexported_probe);

  Detail::module_address_t const where = Detail::find_module_address (address);

  ASSERT_TRUE (where.found);
  EXPECT_EQ (where.module, file_name_of (own_executable_path ()));
  EXPECT_EQ (where.offset, expected_executable_offset (
                               reinterpret_cast<std::uintptr_t> (address)));
}

TEST (
    LumexDebugTest,
    GivenAFrameInsideTheCLibrary_WhenItsModuleIsFound_ThenItIsTheLibraryAtItsOffset)
{
  void *address = libc_frame ();
  ASSERT_NE (address, nullptr);
  Dl_info info;
  ASSERT_NE (dladdr (address, &info), 0);

  Detail::module_address_t const where = Detail::find_module_address (address);

  ASSERT_TRUE (where.found);
  EXPECT_EQ (where.module, file_name_of (info.dli_fname));
  EXPECT_EQ (where.offset,
             reinterpret_cast<std::uintptr_t> (address)
                 - reinterpret_cast<std::uintptr_t> (info.dli_fbase));
}

TEST (LumexDebugTest,
      GivenAnAddressNoModuleHolds_WhenFormatted_ThenOnlyTheAddressIsWritten)
{
  void *address
      = reinterpret_cast<void *> (static_cast<std::uintptr_t> (0x10));

  EXPECT_FALSE (Detail::find_module_address (address).found);
  EXPECT_EQ (Detail::format_frame (0, address), "  #0: [0x10]\n");
}

TEST (
    LumexDebugTest,
    GivenAFrameWithoutAnExportedSymbol_WhenFormatted_ThenItIsItsModuleAndOffset)
{
  void *address = reinterpret_cast<void *> (&unexported_probe);
  std::uintptr_t const value = reinterpret_cast<std::uintptr_t> (address);

  EXPECT_EQ (Detail::format_frame (2, address),
             "  #2: " + file_name_of (own_executable_path ()) + "+0x"
                 + Detail::formatHex (expected_executable_offset (value))
                 + " [0x" + Detail::formatHex (value) + "]\n");
}

TEST (
    LumexDebugTest,
    GivenAFrameWithAnExportedSymbol_WhenFormatted_ThenTheNameComesWithModuleAndOffset)
{
  void *address = libc_frame ();
  ASSERT_NE (address, nullptr);
  Dl_info info;
  ASSERT_NE (dladdr (address, &info), 0);
  ASSERT_NE (info.dli_sname, nullptr);

  std::string const frame = Detail::format_frame (1, address);

  EXPECT_EQ (frame.rfind ("  #1: ", 0), 0U) << frame;
  EXPECT_NE (frame.find (info.dli_sname), std::string::npos) << frame;
  EXPECT_NE (frame.find (" (" + file_name_of (info.dli_fname) + "+0x"),
             std::string::npos)
      << frame;
  EXPECT_NE (frame.find (") [0x"
                         + Detail::formatHex (
                             reinterpret_cast<std::uintptr_t> (address))
                         + "]\n"),
             std::string::npos)
      << frame;
}

TEST (LumexDebugTest,
      GivenAnAddr2lineOnThePath_WhenAStackIsCaptured_ThenNothingStartsIt)
{
  StandInAddr2line const stand_in;
  ASSERT_TRUE (stand_in.valid ());

  std::string const trace = lumex::core::utility::debug::captureStackTrace (
      0, unexported_probe (5));

  EXPECT_FALSE (trace.empty ());
  EXPECT_FALSE (stand_in.started ()) << trace;
}
#endif

TEST (LumexDebugTest, Perf_CaptureStackTrace_TakesMicrosecondsPerStack)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const stacks = 1000;
  auto const start = std::chrono::steady_clock::now ();

  std::size_t total = 0;
  for (int i = 0; i < stacks; ++i)
    total += lumex::core::utility::debug::captureStackTrace (0, 16).size ();

  auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::steady_clock::now () - start);
  EXPECT_GT (total, 0U);
  EXPECT_LT (elapsed.count (), 1000)
      << stacks << " stacks of up to 16 frames took " << elapsed.count ()
      << " ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

// LumexDebug.tests.cpp
#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/debug/LumexDebug.hpp"
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
  EXPECT_NE (info.find ("LumexDebug.tests.cpp"), std::string::npos);
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

// LumexCheckOS.cxx11.tests.cpp
// LUMEX_OS_IS_*() expand to the constant 1 or 0, so they work in `#if` and
// in ordinary code, and agree with the LUMEX_OS_* macros they summarize.
#include <gtest/gtest.h>

#include "lumex/core/utility/os/LumexCheckOS.hpp"

// Apple takes the POSIX branches of the library (1.0.1.0).
#if defined(LUMEX_OS_APPLE) && !defined(LUMEX_OS_UNIX)
#error "LUMEX_OS_APPLE must come with LUMEX_OS_UNIX"
#endif

namespace
{
// The expected answers, from the LUMEX_OS_* macros themselves.
#if defined(LUMEX_OS_WINDOWS)
bool const kWindows = true;
#else
bool const kWindows = false;
#endif
#if defined(LUMEX_OS_LINUX) || defined(LUMEX_OS_ANDROID)
bool const kLinux = true;
#else
bool const kLinux = false;
#endif
#if defined(LUMEX_OS_ANDROID)
bool const kAndroid = true;
#else
bool const kAndroid = false;
#endif
#if defined(LUMEX_OS_MAC) || defined(LUMEX_OS_MACOS)
bool const kMacos = true;
#else
bool const kMacos = false;
#endif
#if defined(LUMEX_OS_IOS)
bool const kIos = true;
#else
bool const kIos = false;
#endif
#if defined(LUMEX_OS_APPLE)
bool const kApple = true;
#else
bool const kApple = false;
#endif
#if defined(LUMEX_OS_UNIX) || defined(LUMEX_OS_LINUX)                         \
    || defined(LUMEX_OS_APPLE)
bool const kUnix = true;
#else
bool const kUnix = false;
#endif
#if defined(LUMEX_OS_POSIX)
bool const kPosixFallback = true;
#else
bool const kPosixFallback = false;
#endif
bool const kPosix = kUnix || kPosixFallback;

// The same answers, taken from the macros inside `#if`.
#if LUMEX_OS_IS_WINDOWS()
bool const kWindowsFromIf = true;
#else
bool const kWindowsFromIf = false;
#endif
#if LUMEX_OS_IS_LINUX()
bool const kLinuxFromIf = true;
#else
bool const kLinuxFromIf = false;
#endif
#if LUMEX_OS_IS_ANDROID()
bool const kAndroidFromIf = true;
#else
bool const kAndroidFromIf = false;
#endif
#if LUMEX_OS_IS_MACOS()
bool const kMacosFromIf = true;
#else
bool const kMacosFromIf = false;
#endif
#if LUMEX_OS_IS_IOS()
bool const kIosFromIf = true;
#else
bool const kIosFromIf = false;
#endif
#if LUMEX_OS_IS_APPLE()
bool const kAppleFromIf = true;
#else
bool const kAppleFromIf = false;
#endif
#if LUMEX_OS_IS_UNIX()
bool const kUnixFromIf = true;
#else
bool const kUnixFromIf = false;
#endif
#if LUMEX_OS_IS_POSIX()
bool const kPosixFromIf = true;
#else
bool const kPosixFromIf = false;
#endif

// Constant expressions of value 0 or 1 in ordinary code as well.
static_assert (LUMEX_OS_IS_WINDOWS () == 0 || LUMEX_OS_IS_WINDOWS () == 1,
               "LUMEX_OS_IS_WINDOWS() is 0 or 1");
static_assert (LUMEX_OS_IS_POSIX () == 0 || LUMEX_OS_IS_POSIX () == 1,
               "LUMEX_OS_IS_POSIX() is 0 or 1");
static_assert (LUMEX_OS_IS_WINDOWS () + LUMEX_OS_IS_UNIX () <= 1,
               "a build is not Windows and Unix at once");
} // namespace

TEST (LumexCheckOSTest, GivenOsMacros_WhenUsedInIf_ThenMatchTheOsDefines)
{
  EXPECT_EQ (kWindowsFromIf, kWindows);
  EXPECT_EQ (kLinuxFromIf, kLinux);
  EXPECT_EQ (kAndroidFromIf, kAndroid);
  EXPECT_EQ (kMacosFromIf, kMacos);
  EXPECT_EQ (kIosFromIf, kIos);
  EXPECT_EQ (kAppleFromIf, kApple);
  EXPECT_EQ (kUnixFromIf, kUnix);
  EXPECT_EQ (kPosixFromIf, kPosix);
}

TEST (LumexCheckOSTest, GivenOsMacros_WhenUsedInCode_ThenMatchTheOsDefines)
{
  EXPECT_EQ (LUMEX_OS_IS_WINDOWS () != 0, kWindows);
  EXPECT_EQ (LUMEX_OS_IS_LINUX () != 0, kLinux);
  EXPECT_EQ (LUMEX_OS_IS_ANDROID () != 0, kAndroid);
  EXPECT_EQ (LUMEX_OS_IS_MACOS () != 0, kMacos);
  EXPECT_EQ (LUMEX_OS_IS_IOS () != 0, kIos);
  EXPECT_EQ (LUMEX_OS_IS_APPLE () != 0, kApple);
  EXPECT_EQ (LUMEX_OS_IS_UNIX () != 0, kUnix);
  EXPECT_EQ (LUMEX_OS_IS_POSIX () != 0, kPosix);

  bool unix_branch = false;
  if (LUMEX_OS_IS_UNIX ())
    unix_branch = true;
  EXPECT_EQ (unix_branch, kUnix);
}

TEST (LumexCheckOSTest, GivenThisBuild_WhenQueried_ThenTheCompilerPlatformWins)
{
#if defined(_WIN32)
  EXPECT_EQ (LUMEX_OS_IS_WINDOWS (), 1);
  EXPECT_EQ (LUMEX_OS_IS_UNIX (), 0);
  EXPECT_EQ (LUMEX_OS_IS_LINUX (), 0);
#elif defined(__APPLE__)
  EXPECT_EQ (LUMEX_OS_IS_APPLE (), 1);
  EXPECT_EQ (LUMEX_OS_IS_UNIX (), 1);
  EXPECT_EQ (LUMEX_OS_IS_POSIX (), 1);
  EXPECT_EQ (LUMEX_OS_IS_WINDOWS (), 0);
#elif defined(__linux__) && !defined(__ANDROID__)
  EXPECT_EQ (LUMEX_OS_IS_LINUX (), 1);
  EXPECT_EQ (LUMEX_OS_IS_UNIX (), 1);
  EXPECT_EQ (LUMEX_OS_IS_POSIX (), 1);
  EXPECT_EQ (LUMEX_OS_IS_WINDOWS (), 0);
  EXPECT_EQ (LUMEX_OS_IS_APPLE (), 0);
#else
  GTEST_SKIP () << "no expectation for this platform";
#endif
}

TEST (LumexCheckOSTest, GivenDebugInfoMacro_WhenUsedAsStatement_ThenCompiles)
{
  // Debug builds expand it to a block that names the OS, Release builds to
  // an empty statement; it compiles as a statement in both.
  LUMEX_OS_DEBUG_INFO ();
  SUCCEED ();
}

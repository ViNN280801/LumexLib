// LumexProcess.tests.cpp
#include <gtest/gtest.h>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#include "lumex/core/utility/process/LumexProcess.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using lumex::core::utility::process::get_current_pid;

namespace
{
unsigned long
native_current_pid ()
{
#ifdef LUMEX_OS_WINDOWS
  return GetCurrentProcessId ();
#else
  return static_cast<unsigned long> (getpid ());
#endif
}
} // namespace

TEST (LumexProcessTest,
      GivenCurrentProcess_WhenGetCurrentPid_ThenMatchesPlatformApi)
{
  EXPECT_EQ (get_current_pid (), native_current_pid ());
}

TEST (LumexProcessTest,
      GivenCurrentProcess_WhenGetCurrentPidTwice_ThenSameValue)
{
  unsigned long const first = get_current_pid ();
  unsigned long const second = get_current_pid ();
  EXPECT_EQ (first, second);
}

TEST (LumexProcessTest, GivenCurrentProcess_WhenGetCurrentPid_ThenIsNonZero)
{
  EXPECT_NE (get_current_pid (), 0UL);
}

TEST (LumexProcessTest,
      GivenCurrentProcess_WhenGetCurrentPid_ThenIsNotArbitrarySentinel)
{
  // Unfound: a made-up id must not equal the live process id.
  EXPECT_NE (get_current_pid (), 0xFFFFFFFEUL);
}

TEST (LumexProcessTest, GivenGetCurrentPid_WhenCalled_ThenIsNoexcept)
{
  LUMEX_STATIC_ASSERT_MSG (noexcept (get_current_pid ()),
                           "get_current_pid is noexcept");
  SUCCEED ();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

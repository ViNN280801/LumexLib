// LumexUtilities.tests.cpp
#include <climits>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

TEST (LumexUtilitiesTest, GivenNonNegativeFd_WhenFdToPtrAndBack_ThenRestoresFd)
{
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (0)), 0);
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (1)), 1);
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (5)), 5);
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (42)), 42);
}

TEST (LumexUtilitiesTest, GivenNullPointer_WhenPtrToFd_ThenReturnsZero)
{
  EXPECT_EQ (ptr_to_fd (nullptr), 0);
}

TEST (LumexUtilitiesTest,
      GivenNegativeSentinel_WhenFdToPtrAndBack_ThenPreservesBitPattern)
{
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (-1)), -1);
}

TEST (LumexUtilitiesTest, GivenStdinFdZero_WhenFdToPtr_ThenYieldsNullptr)
{
  EXPECT_EQ (fd_to_ptr (0), nullptr);
}

TEST (LumexUtilitiesTest,
      GivenDistinctPositiveFds_WhenFdToPtr_ThenProducesDistinctPointers)
{
  EXPECT_NE (fd_to_ptr (1), fd_to_ptr (2));
  EXPECT_NE (fd_to_ptr (3), fd_to_ptr (4));
  EXPECT_NE (fd_to_ptr (1), nullptr);
}

TEST (LumexUtilitiesTest, GivenIntMax_WhenFdToPtrAndBack_ThenRestoresFd)
{
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (INT_MAX)), INT_MAX);
}

TEST (LumexUtilitiesTest, GivenIntMin_WhenFdToPtrAndBack_ThenRestoresFd)
{
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (INT_MIN)), INT_MIN);
}

TEST (LumexUtilitiesTest,
      GivenTypicalPosixDescriptors_WhenRoundTripped_ThenMatchOriginal)
{
  int const fds[] = { 0, 1, 2, 3, 255, 1023, 4096, 65535 };
  for (int const fd : fds)
    EXPECT_EQ (ptr_to_fd (fd_to_ptr (fd)), fd) << "fd=" << fd;
}

TEST (LumexUtilitiesTest,
      GivenNegativeErrorCodes_WhenFdToPtrAndBack_ThenPreservesValue)
{
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (-2)), -2);
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (-100)), -100);
}

TEST (LumexUtilitiesTest, GivenFdToPtr_WhenCalled_ThenIsNoexcept)
{
  LUMEX_STATIC_ASSERT_MSG (noexcept (fd_to_ptr (0)), "fd_to_ptr is noexcept");
  LUMEX_STATIC_ASSERT_MSG (noexcept (ptr_to_fd (nullptr)),
                           "ptr_to_fd is noexcept");
  SUCCEED ();
}

TEST (LumexUtilitiesTest,
      GivenSameFdTwice_WhenFdToPtr_ThenReturnsIdenticalPointer)
{
  EXPECT_EQ (fd_to_ptr (7), fd_to_ptr (7));
}

TEST (LumexUtilitiesTest,
      GivenPointerFromFd_WhenTruncationWouldMatter_ThenLowBitsSurvive)
{
  int const fd = 0x7F123456;
  void *const stored = fd_to_ptr (fd);
  EXPECT_EQ (ptr_to_fd (stored), fd);
  EXPECT_EQ (reinterpret_cast<std::intptr_t> (stored),
             static_cast<std::intptr_t> (fd));
}

class LumexUtilitiesFdRoundtripTest : public ::testing::TestWithParam<int>
{
};

TEST_P (LumexUtilitiesFdRoundtripTest,
        GivenParameterizedFd_WhenFdToPtrAndBack_ThenRestoresFd)
{
  int const fd = GetParam ();
  EXPECT_EQ (ptr_to_fd (fd_to_ptr (fd)), fd);
}

INSTANTIATE_TEST_SUITE_P (BoundaryFds, LumexUtilitiesFdRoundtripTest,
                          ::testing::Values (0, 1, -1, 2, 16, 256, 1024,
                                             INT_MAX, INT_MIN,
                                             std::numeric_limits<int>::min ()
                                                 + 1));

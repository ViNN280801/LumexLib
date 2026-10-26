// lumex/tests/core/interprocess/LumexInterprocess.cxx11.tests.cpp
//
// The umbrella header alone is enough for the whole module, and the error
// kind is a reflected enum: its names, its companions and the fallback of a
// value outside the enumeration.
#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/interprocess/LumexInterprocess"

namespace lk = lumex::core::interprocess::lock;

TEST (LumexInterprocessTest,
      GivenUmbrella_WhenIncluded_ThenTheLockAndItsErrors)
{
  EXPECT_FALSE (std::is_copy_constructible<lk::interprocess_lock>::value);
  EXPECT_FALSE (std::is_copy_assignable<lk::interprocess_lock>::value);
  EXPECT_TRUE (
      std::is_nothrow_move_constructible<lk::interprocess_lock>::value);
  EXPECT_TRUE (std::is_nothrow_move_assignable<lk::interprocess_lock>::value);
  EXPECT_FALSE (std::is_default_constructible<lk::interprocess_lock>::value);
}

TEST (LumexInterprocessTest, GivenLockError_WhenToString_ThenTheEnumeratorName)
{
  EXPECT_EQ (std::string (to_string (lk::lock_error::busy)), "busy");
  EXPECT_EQ (std::string (to_string (lk::lock_error::access_denied)),
             "access_denied");
  EXPECT_EQ (std::string (to_string (lk::lock_error::io_error)), "io_error");
}

TEST (LumexInterprocessTest,
      GivenLockError_WhenReflected_ThenThreeKindsInOrder)
{
  EXPECT_EQ (lk::lock_errorSize, 3U);
  EXPECT_EQ (lk::lock_errorFirst, lk::lock_error::busy);
  EXPECT_EQ (lk::lock_errorLast, lk::lock_error::io_error);
  EXPECT_EQ (lk::lock_errorValues[1], lk::lock_error::access_denied);
}

TEST (LumexInterprocessTest, GivenValueOutsideTheEnum_WhenToString_ThenUnknown)
{
  EXPECT_EQ (std::string (to_string (static_cast<lk::lock_error> (7))),
             "<Unknown>");
}

TEST (LumexInterprocessTest,
      GivenDefaultFailure_WhenRead_ThenIoErrorWithoutCode)
{
  lk::lock_failure_t const failure;
  EXPECT_EQ (failure.kind, lk::lock_error::io_error);
  EXPECT_EQ (failure.system_code, 0);
}

// The branch of the subprocess runner for platforms without fork (Windows,
// MinGW), built on every platform with LUMEX_TEST_FORCE_NO_FORK: the runner
// must report that it cannot run a child, not run the body in this process,
// and the tests that need a child skip on that answer.

#include <gtest/gtest.h>

#include "lumex/tests/support/LumexTestSubprocess.hpp"

TEST (LumexTestNoForkTest,
      GivenAPlatformWithoutFork_WhenAskedForAChild_ThenItIsUnavailable)
{
  EXPECT_FALSE (lumex_test::subprocess_available ());
  bool ran = false;
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      [&]
        {
          ran = true;
          return 0;
        });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::unavailable);
  EXPECT_FALSE (ran) << "the body must not run in the calling process";
  EXPECT_FALSE (lumex_test::child_was_caught (result))
      << "an unavailable child is not a caught defect";
  EXPECT_NE (lumex_test::describe_child (result).find ("no subprocess"),
             std::string::npos);
}

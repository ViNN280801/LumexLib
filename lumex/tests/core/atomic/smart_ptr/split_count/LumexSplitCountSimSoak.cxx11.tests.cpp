/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// The soak tests of the simulated-scheduler explorer
// (LumexSplitCountSim.hpp): every scenario of the table at the soak budget
// (the preemption bound one above the quick one, the cap of 600000 schedules
// and 5000 random schedules for five and six threads), with COVERAGE asserted
// for the scenarios marked so. The suite name contains `Soak`, so
// `SOAK_FILTER *Soak*` keeps these tests out of the default run; they are
// registered with -DLUMEX_BUILD_SOAK_TESTS=ON under the label soak. They run
// in the C++11 suite with the default backend only.
#include <string>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountSim.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_sim;

#if __cplusplus == 201103L && !defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)         \
    && LUMEX_DWCAS_BACKEND != LUMEX_DWCAS_BACKEND_BUILTIN
bool const soak_sim_suite = true;
#else
bool const soak_sim_suite = false;
#endif

#define LUMEX_SIM_SOAK_TEST(Id, Policy)                                                                    \
  TEST (                                                                                                   \
      LumexSplitCountSimSoakTest,                                                                          \
      GivenScenario##Id##Policy##_WhenExploredAtTheSoakBound_ThenEveryOracleHoldsAndTheOutcomesAreCovered) \
  {                                                                                                        \
    if (char const *reason = skip_reason (sc_##Id, pol_##Policy))                                          \
      GTEST_SKIP () << reason;                                                                             \
    if (!soak_sim_suite)                                                                                   \
      GTEST_SKIP () << "the soak set runs in the C++11 suite";                                             \
    if (!lumex_test::soak_requested ()                                                                     \
        && std::getenv ("LUMEX_SIM_ONLY") == nullptr)                                                      \
      GTEST_SKIP () << "set LUMEX_TEST_SOAK to run the soak tests";                                        \
    scenario_def_t const &def = scenario_of (sc_##Id);                                                     \
    run_result_t const result                                                                              \
        = run_scenario (sc_##Id, pol_##Policy, soak_budget (def.nthreads));                                \
    EXPECT_GT (result.dfs.schedules, 0);                                                                   \
    if (def.cov && !result.dfs.capped)                                                                     \
      EXPECT_TRUE (result.covered)                                                                         \
          << "SIM-FAIL oracles=COVERAGE: " << result.seen << " of "                                        \
          << result.expected << " expected outcomes were seen";                                            \
  }

LUMEX_SIM_ALL_RUNS (LUMEX_SIM_SOAK_TEST)

#undef LUMEX_SIM_SOAK_TEST
} // namespace

#else

TEST (LumexSplitCountSimSoakTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

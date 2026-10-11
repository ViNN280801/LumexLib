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

// The quick tests of the simulated-scheduler explorer
// (LumexSplitCountSim.hpp): the self-test of the explorer on toy threads, the
// determinism of a search, the outcome table, and every scenario of the
// table at the quick budget (the default run). The long runs are the tests of
// LumexSplitCountSimSoak.cxx11.tests.cpp (the suite name contains `Soak`).
//
// The full quick set runs in the C++11 suites with the default and the
// built-in backend; the other suites run the self-tests and S01, H1 and LK at
// bound 1.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountSim.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_sim;

#if __cplusplus == 201103L && !defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)
bool const full_sim_set = true;
#else
bool const full_sim_set = false;
#endif

/// The scenarios the suites without the full set still run (at bound 1).
bool
in_small_set (scenario_id_t id, policy_id_t policy)
{
  return policy == pol_R8 && (id == sc_S01 || id == sc_H1 || id == sc_LK);
}

/// The number of schedules of the model for the toy threads (each thread
/// with the given number of yields) at the bounds 0 to 3.
struct toy_row_t
{
  std::vector<int> yields;
  long schedules[4];
};

TEST (
    LumexSplitCountSimTest,
    GivenToyThreads_WhenExploredToBoundsZeroToThree_ThenTheScheduleCountsMatchTheModel)
{
  std::vector<toy_row_t> rows;
  toy_row_t row;
  row.yields = { 2, 2 };
  row.schedules[0] = 2;
  row.schedules[1] = 6;
  row.schedules[2] = 14;
  row.schedules[3] = 18;
  rows.push_back (row);
  row.yields = { 1, 1, 1 };
  row.schedules[0] = 6;
  row.schedules[1] = 24;
  row.schedules[2] = 60;
  row.schedules[3] = 90;
  rows.push_back (row);
  row.yields = { 2, 2, 2 };
  row.schedules[0] = 6;
  row.schedules[1] = 42;
  row.schedules[2] = 192;
  row.schedules[3] = 552;
  rows.push_back (row);
  for (std::size_t r = 0; r < rows.size (); ++r)
    for (int bound = 0; bound <= 3; ++bound)
      {
        toy_world_t world (rows[r].yields);
        explorer_t explorer (world, "toy");
        explorer.run_dfs (bound, 0);
        EXPECT_EQ (explorer.stats ().schedules, rows[r].schedules[bound])
            << "toy " << r << " at bound " << bound;
      }
}

TEST (
    LumexSplitCountSimTest,
    GivenAToyLostUpdate_WhenExplored_ThenTheFailureIsFoundAndItsDecisionsReplayIt)
{
  std::vector<int> const yields (2, 1);
  {
    toy_world_t world (yields, true);
    explorer_t explorer (world, "toy");
    explorer.set_record_mode (true);
    explorer.run_dfs (0, 0);
    EXPECT_FALSE (explorer.failed ())
        << "without a preemption the two updates are serialized";
  }
  std::string decisions;
  {
    toy_world_t world (yields, true);
    explorer_t explorer (world, "toy");
    explorer.set_record_mode (true);
    explorer.run_dfs (1, 0);
    ASSERT_TRUE (explorer.failed ()) << "one preemption loses an update";
    decisions = explorer.failure_decisions ();
    EXPECT_FALSE (decisions.empty ());
  }
  {
    toy_world_t world (yields, true);
    explorer_t explorer (world, "toy");
    explorer.set_record_mode (true);
    explorer.run_replay (decisions);
    EXPECT_TRUE (explorer.failed ())
        << "the printed decisions " << decisions << " replay the failure";
  }
}

TEST (
    LumexSplitCountSimTest,
    GivenTheScenarioTable_WhenTheOutcomesAreExpanded_ThenTheirNumberIsTheOneOfTheTable)
{
  std::vector<scenario_def_t> const &table = scenario_table ();
  EXPECT_EQ (table.size (), static_cast<std::size_t> (sc_count));
  for (std::size_t i = 0; i < table.size (); ++i)
    {
      EXPECT_EQ (static_cast<int> (table[i].expected.size ()),
                 table[i].outcomes)
          << table[i].name;
      EXPECT_GE (table[i].nthreads, 1) << table[i].name;
    }
}

TEST (
    LumexSplitCountSimTest,
    GivenScenarioS02_WhenExploredTwice_ThenTheCountsAndTheTraceHashesAreEqual)
{
  if (char const *reason = skip_reason (sc_S02, pol_R8))
    GTEST_SKIP () << reason;
  budget_t budget = quick_budget (scenario_of (sc_S02).nthreads);
  budget.bound = 1;
  run_result_t const first = run_scenario (sc_S02, pol_R8, budget);
  run_result_t const second = run_scenario (sc_S02, pol_R8, budget);
  EXPECT_GT (first.dfs.schedules, 0);
  EXPECT_EQ (first.dfs.schedules, second.dfs.schedules);
  EXPECT_EQ (first.dfs.hash, second.dfs.hash);
  EXPECT_EQ (first.dfs.steps, second.dfs.steps);
}

#define LUMEX_SIM_QUICK_TEST(Id, Policy)                                      \
  TEST (LumexSplitCountSimTest,                                               \
        GivenScenario##Id##Policy##_WhenExplored_ThenEveryOracleHolds)        \
  {                                                                           \
    if (char const *reason = skip_reason (sc_##Id, pol_##Policy))             \
      GTEST_SKIP () << reason;                                                \
    bool const small = in_small_set (sc_##Id, pol_##Policy);                  \
    if (!full_sim_set && !small)                                              \
      GTEST_SKIP () << "full sim set runs in the C++11 suites";               \
    budget_t budget = quick_budget (scenario_of (sc_##Id).nthreads);          \
    if (!full_sim_set)                                                        \
      budget.bound = 1;                                                       \
    run_result_t const result = run_scenario (sc_##Id, pol_##Policy, budget); \
    EXPECT_GT (result.dfs.schedules, 0);                                      \
  }

LUMEX_SIM_ALL_RUNS (LUMEX_SIM_QUICK_TEST)

#undef LUMEX_SIM_QUICK_TEST
} // namespace

#else

TEST (LumexSplitCountSimTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

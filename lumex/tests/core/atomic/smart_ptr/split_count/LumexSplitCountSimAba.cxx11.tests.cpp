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

// The quick tests of package V2 (the ABA scenarios of the explorer): the
// negative control (a deliberately ABA-vulnerable toy stack that the explorer
// must catch, and its two repaired forms that it must not), and every scenario
// of the package at the quick budget (the default run). The long runs are the
// tests of LumexSplitCountSimAbaSoak.cxx11.tests.cpp (the suite name contains
// `Soak`).
//
// The full quick set runs in the C++11 suite with the default backend; the
// other suites (built-in backend, C++17, C++20, wait table) run the toy
// control and the two scenarios AR1 and HA3 at bound 1.
#include <string>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountSimAba.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_sim;

#if __cplusplus == 201103L && !defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)         \
    && LUMEX_DWCAS_BACKEND != LUMEX_DWCAS_BACKEND_BUILTIN
bool const full_aba_set = true;
#else
bool const full_aba_set = false;
#endif

/// The scenarios the suites without the full set still run (at bound 1).
bool
in_small_aba_set (scenario_id_t id, policy_id_t policy)
{
  return policy == pol_R8 && (id == sc_AR1 || id == sc_HA3);
}

/// Explores the toy stack to @p bound; true when it failed. The decisions of
/// the failure are returned through @p decisions, the number of schedules
/// that were run through @p schedules.
bool
explore_toy (toy_aba_mode_t mode, int bound, std::string &decisions,
             long &schedules)
{
  toy_aba_world_t world (mode);
  explorer_t explorer (world, "toy-aba");
  explorer.set_record_mode (true);
  explorer.run_dfs (bound, 0);
  decisions = explorer.failure_decisions ();
  schedules = explorer.stats ().schedules;
  return explorer.failed ();
}

bool
explore_toy (toy_aba_mode_t mode, int bound, std::string &decisions)
{
  long schedules = 0;
  return explore_toy (mode, bound, decisions, schedules);
}

TEST (
    LumexSplitCountSimAbaTest,
    GivenAToyAbaStackWithImmediateReuse_WhenExplored_ThenOnlyOnePreemptionFindsTheSchedule)
{
  std::string decisions;
  EXPECT_FALSE (explore_toy (toy_aba_reuse, 0, decisions))
      << "without a preemption the pops are serialized";
  ASSERT_TRUE (explore_toy (toy_aba_reuse, 1, decisions))
      << "the ABA schedule needs one preemption and the explorer must find it";
  EXPECT_FALSE (decisions.empty ());
  std::printf ("SIM-ABA-CONTROL toy=reuse bound=1 decisions=%s\n",
               decisions.c_str ());
  std::fflush (stdout);
  // The printed decisions replay the failure.
  toy_aba_world_t world (toy_aba_reuse);
  explorer_t explorer (world, "toy-aba");
  explorer.set_record_mode (true);
  explorer.run_replay (decisions);
  EXPECT_TRUE (explorer.failed ())
      << "the decisions " << decisions << " replay the ABA failure";
}

// A regression test of the control, not a search: the state space of the toy
// is small, bound one already exhausts it (the schedule counts of the bounds
// one and three are equal), so exploring to bound three adds nothing. The
// test shows that the oracle does not fire on a stack that has no ABA, so
// that the failure of the vulnerable stack comes from the ABA and not from the
// oracle or the explorer.
TEST (LumexSplitCountSimAbaTest,
      GivenTheRepairedToyStacks_WhenExplored_ThenNothingFails)
{
  std::string decisions;
  long at_bound_one = 0;
  long at_bound_three = 0;
  EXPECT_FALSE (explore_toy (toy_aba_versioned, 1, decisions, at_bound_one));
  EXPECT_FALSE (explore_toy (toy_aba_versioned, 3, decisions, at_bound_three))
      << "a version in the top word defeats the reuse: " << decisions;
  EXPECT_EQ (at_bound_one, at_bound_three)
      << "bound one exhausts the state space of the toy";
  EXPECT_FALSE (explore_toy (toy_aba_no_reuse, 1, decisions, at_bound_one));
  EXPECT_FALSE (explore_toy (toy_aba_no_reuse, 3, decisions, at_bound_three))
      << "without the reuse the old top never comes back: " << decisions;
  EXPECT_EQ (at_bound_one, at_bound_three)
      << "bound one exhausts the state space of the toy";
}

TEST (
    LumexSplitCountSimAbaTest,
    GivenTheAbaScenarios_WhenTheOutcomesAreExpanded_ThenTheirNumberIsTheOneOfTheTable)
{
  scenario_id_t const ids[]
      = { sc_AR1, sc_AR2, sc_AR3, sc_AR4, sc_DA,  sc_BA4, sc_CD1, sc_CD2,
          sc_CD3, sc_LKX, sc_LKC, sc_EXX, sc_EXC, sc_HA1, sc_HA2, sc_HA3,
          sc_WH1, sc_WH2, sc_CH1, sc_CH2, sc_RW,  sc_RX,  sc_DAC, sc_HA4,
          sc_CE1, sc_CE2, sc_CE3, sc_WR,  sc_DA2 };
  for (std::size_t i = 0; i < sizeof (ids) / sizeof (ids[0]); ++i)
    {
      scenario_def_t const &def = scenario_of (ids[i]);
      EXPECT_EQ (static_cast<int> (def.expected.size ()), def.outcomes)
          << def.name;
      EXPECT_GE (def.nthreads, 2) << def.name;
    }
}

#define LUMEX_SIM_ABA_QUICK_TEST(Id, Policy)                                  \
  TEST (LumexSplitCountSimAbaTest,                                            \
        GivenScenario##Id##Policy##_WhenExplored_ThenEveryOracleHolds)        \
  {                                                                           \
    if (char const *reason = skip_reason (sc_##Id, pol_##Policy))             \
      GTEST_SKIP () << reason;                                                \
    bool const small = in_small_aba_set (sc_##Id, pol_##Policy);              \
    if (!full_aba_set && !small)                                              \
      GTEST_SKIP () << "full sim set runs in the C++11 suites";               \
    budget_t budget = quick_budget (scenario_of (sc_##Id));                   \
    if (!full_aba_set)                                                        \
      {                                                                       \
        budget.bound = 1;                                                     \
        budget.dfs = true;                                                    \
      }                                                                       \
    run_result_t const result = run_scenario (sc_##Id, pol_##Policy, budget); \
    EXPECT_GT (result.dfs.schedules + result.random.schedules, 0);            \
  }

LUMEX_SIM_ABA_RUNS (LUMEX_SIM_ABA_QUICK_TEST)

#undef LUMEX_SIM_ABA_QUICK_TEST
} // namespace

#else

TEST (LumexSplitCountSimAbaTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif

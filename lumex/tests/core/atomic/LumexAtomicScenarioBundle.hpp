/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexAtomicScenarioBundle.hpp
 * @brief The set of checkers that every correct atomic shared pointer must
 * pass, as one call.
 * @details `run_bundle<Engine>` runs the register, identity, ABA,
 * linearizability, structure and lifecycle checkers at small sizes;
 * `run_weak_bundle<Engine>` the ones of `atomic_weak_ptr`. The falsification
 * suite runs the bundles on the correct test-side engines (the positive
 * control: the checkers must not report a correct engine) and the C++20 suite
 * on `std::atomic<std::shared_ptr>`, an independent implementation.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIO_BUNDLE_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIO_BUNDLE_HPP

#include <algorithm>
#include <vector>

#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosGap.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosLifecycle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosStructures.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosWeak.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
/// Runs a checker on every thread count of @p counts and schedule kind.
inline void
matrix (std::vector<int> const &counts, Checker checker, int iterations,
        lumex_test::Verdict &verdict)
{
  run_matrix_for (counts, checker, iterations, verdict);
}

inline std::vector<int>
two_and_four ()
{
  std::vector<int> counts;
  counts.push_back (2);
  counts.push_back (4);
  return counts;
}

inline std::vector<int>
four_and_eight ()
{
  std::vector<int> counts;
  counts.push_back (4);
  counts.push_back (8);
  return counts;
}

/// The checkers that every correct engine must pass; small sizes.
template <typename Engine>
void
run_bundle (lumex_test::Verdict &verdict, int iterations)
{
  identity_rule<Engine> (verdict);
  aba_same_owner<Engine> (verdict);
  reuse_equivalence<Engine> (verdict);
  replaced_value_released<Engine> (verdict);
  gap_aba<Engine> (verdict);
  std::vector<int> const counts = two_and_four ();
  matrix (counts, &cas_counter<Engine>, iterations, verdict);
  matrix (counts, &exchange_permutation<Engine>, iterations, verdict);
  matrix (counts, &monotone_loads<Engine>, iterations, verdict);
  matrix (counts, &rmw_walk<Engine>, iterations * 3, verdict);
  matrix (counts, &strong_cas_precision<Engine>, iterations, verdict);
  matrix (counts, &reuse_stress<Engine>, iterations, verdict);
  matrix (counts, &owner_balance<Engine>, iterations * 2, verdict);
  matrix (counts, &shared_value_fanout<Engine>, iterations * 2, verdict);
  matrix (counts, &thread_churn<Engine>, std::max (8, iterations / 8),
          verdict);
  matrix (counts, &treiber_stress<Engine>, iterations, verdict);
  matrix (counts, &queue_stress<Engine>, iterations, verdict);
  matrix (counts, &list_stress<Engine>, iterations, verdict);
  matrix (counts, &cas_double_winner<Engine>, 20, verdict);
  matrix (counts, &exchange_double_return<Engine>, 20, verdict);
  run_matrix_for (history_thread_counts (), &linearizable_register<Engine>,
                  std::max (20, iterations / 2), verdict);
  run_matrix_for (history_thread_counts (),
                  &structure_linearizable<Engine, false>,
                  std::max (20, iterations / 2), verdict);
  run_matrix_for (history_thread_counts (),
                  &structure_linearizable<Engine, true>,
                  std::max (20, iterations / 2), verdict);
  run_matrix_for (history_thread_counts (), &list_linearizable<Engine>,
                  std::max (20, iterations / 2), verdict);
  {
    lumex_test::ObjectLedger ledger (64);
    lumex_test::ReusePool pool;
    {
      SharedStack<Engine> stack (ledger, pool);
      treiber_aba_schedule (stack, verdict, "shared stack");
    }
    Engine::quiesce ();
    check_balance (ledger, pool, verdict, "shared stack");
  }
}

/// The checkers of the weak pointer; small sizes.
template <typename Engine>
void
run_weak_bundle (lumex_test::Verdict &verdict, int iterations)
{
  weak_owner_identity<Engine> (verdict);
  weak_split_allocation_identity<Engine> (verdict);
  std::vector<int> const counts = two_and_four ();
  matrix (counts, &promotion_race<Engine, true>, iterations, verdict);
  matrix (counts, &promotion_race<Engine, false>, iterations, verdict);
  matrix (counts, &expired_race<Engine>, iterations, verdict);
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIO_BUNDLE_HPP

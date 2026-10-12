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
 * @file LumexAtomicScenariosGap.hpp
 * @brief Deterministic interleavings through the hook points of the
 * test-side engines.
 * @details The engines of `LumexAtomicTestEngines.hpp` call
 * `EngineHooks::run_cas_gap ()` at the point where an implementation is open
 * to being overtaken (between the comparison and the act of a
 * compare-exchange, between the load and the store of a split exchange).
 * These checkers install a hook there that parks or meets the calling
 * thread, run the interleaving that exposes the defect and read the outcome.
 * An engine that never calls the hook (every real engine) is not stopped:
 * the checkers notice that the hook was not reached and report nothing, so
 * they may run on any engine; they only prove something for engines with
 * the hook, which is what the falsification suite uses them on.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_GAP_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_GAP_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
/// Clears the hook when the scope ends, whatever happens.
class GapHookScope
{
public:
  explicit GapHookScope (std::function<void ()> hook)
  {
    EngineHooks::cas_gap () = std::move (hook);
  }

  ~GapHookScope () { EngineHooks::cas_gap () = std::function<void ()> (); }

  GapHookScope (GapHookScope const &) = delete;
  GapHookScope &operator= (GapHookScope const &) = delete;
};

/**
 * @brief Two threads compare-exchange from the same expected value at the
 * same moment: at most one may win.
 * @details The hook makes both threads meet between the comparison and the
 * store of an engine that has such a gap, so an engine that locks the two
 * halves separately lets both win.
 */
template <typename Engine>
void
cas_double_winner (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (p.iterations) * 4
                                   + 64);
  lumex_test::ReusePool pool;
  lumex_test::Rendezvous meeting (2);
  GapHookScope const hook ([&] { meeting.meet (20); });
  int doubled = 0;
  for (int round = 0; round < p.iterations; ++round)
    {
      meeting.reset ();
      VNodePtr const first = lumex_test::make_pooled<VNode> (pool, ledger, 0);
      atom_type a (first);
      std::atomic<int> winners (0);
      std::string const error = lumex_test::run_threads (
          2,
          [&] (int t)
            {
              VNodePtr expected = first;
              if (a.compare_exchange_strong (
                      expected,
                      lumex_test::make_pooled<VNode> (pool, ledger, t + 1)))
                winners.fetch_add (1);
            });
      verdict.require (error.empty (), error);
      if (winners.load () > 1)
        ++doubled;
    }
  Engine::quiesce ();
  verdict.require (doubled == 0,
                   "cas_double_winner: in " + std::to_string (doubled)
                       + " rounds two compare_exchange calls from the same "
                         "expected value both succeeded (a lost update)");
  check_balance (ledger, pool, verdict, "cas_double_winner");
}

/**
 * @brief Two threads exchange at the same moment: they must not return the
 * same previous value.
 */
template <typename Engine>
void
exchange_double_return (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (p.iterations) * 4
                                   + 64);
  lumex_test::ReusePool pool;
  lumex_test::Rendezvous meeting (2);
  GapHookScope const hook ([&] { meeting.meet (20); });
  int duplicated = 0;
  for (int round = 0; round < p.iterations; ++round)
    {
      meeting.reset ();
      atom_type a (lumex_test::make_pooled<VNode> (pool, ledger, 0));
      VNodePtr returned[2];
      std::string const error = lumex_test::run_threads (
          2,
          [&] (int t)
            {
              returned[t] = a.exchange (
                  lumex_test::make_pooled<VNode> (pool, ledger, t + 1));
            });
      verdict.require (error.empty (), error);
      if (returned[0] && returned[1]
          && equivalent_values (returned[0], returned[1]))
        ++duplicated;
    }
  Engine::quiesce ();
  verdict.require (duplicated == 0,
                   "exchange_double_return: in " + std::to_string (duplicated)
                       + " rounds two exchanges returned the same previous "
                         "value");
  check_balance (ledger, pool, verdict, "exchange_double_return");
}

/**
 * @brief The ABA of a pointer word: a compare-exchange is stopped after it
 * decided the value matches, the value is replaced twice, and the second
 * replacement is built where the first one stood.
 * @details Deterministic. The expected value is held by the stopped thread,
 * so its node stays alive; it is the engine's own storage (a box behind a
 * pointer word) that is freed and reused. The compare-exchange must fail:
 * the stored value is no longer the expected one. An engine that re-reads
 * only the address of its box accepts it.
 */
template <typename Engine>
void
gap_aba (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  lumex_test::StallPoint stall;
  stall.arm (true);
  GapHookScope const hook ([&] { stall.park (5000); });
  {
    VNodePtr const first = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    atom_type a (first);
    std::atomic<bool> finished (false);
    bool won = false;
    std::thread stopped (
        [&]
          {
            VNodePtr expected = first;
            won = a.compare_exchange_strong (
                expected, lumex_test::make_pooled<VNode> (pool, ledger, 99));
            finished.store (true);
          });
    bool parked = false;
    for (int waited = 0; waited < 300 && !finished.load (); ++waited)
      if (stall.wait_until_parked (1, 1))
        {
          parked = true;
          break;
        }
    if (parked)
      {
        a.store (lumex_test::make_pooled<VNode> (pool, ledger, 2));
        a.store (lumex_test::make_pooled<VNode> (pool, ledger, 3));
      }
    stall.arm (false);
    stall.release ();
    stopped.join ();
    if (parked)
      {
        verdict.require (!won,
                         "gap_aba: a compare_exchange succeeded against a "
                         "value that had been replaced twice (ABA on the "
                         "pointer word: the new box took the freed one's "
                         "address)");
        VNodePtr const now = a.load ();
        verdict.require (won || (now && now->version == 3),
                         "gap_aba: the value stored behind the stopped "
                         "compare_exchange was overwritten");
        verdict.require (now && now->intact (),
                         "gap_aba: a corrupt node was stored");
      }
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "gap_aba");
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_GAP_HPP

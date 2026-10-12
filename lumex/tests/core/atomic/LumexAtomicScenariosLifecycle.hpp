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
 * @file LumexAtomicScenariosLifecycle.hpp
 * @brief Engine-parametrized checkers of ownership: when a replaced value is
 * released, reference-count balance under every operation and under
 * self-assignment, one value shared by many atomics, threads that exit while
 * holding.
 * @details Same shape as `LumexAtomicScenarios.hpp`. The ownership checks
 * that depend on promptness ask `Engine::replaced_value_dies_in_call ()`: an
 * engine that defers the destruction of a replaced value (a hazard-protected
 * box) is checked after `Engine::quiesce ()` and after the atomic is gone,
 * and one that destroys it in the call is checked right after the call.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_LIFECYCLE_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_LIFECYCLE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
/**
 * @brief The value that store, exchange and compare-exchange replace is
 * released by the call (for an engine that promises it), and every object
 * dies exactly once.
 * @details Single threaded and exact: after each replacing operation the
 * only live node is the stored one plus the ones the test still holds. An
 * implementation that forgets to release the replaced value (a store that
 * keeps a copy) leaves a node alive that the ledger names.
 */
template <typename Engine>
void
replaced_value_released (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  bool const prompt = Engine::replaced_value_dies_in_call ();
  lumex_test::ObjectLedger ledger (32);
  lumex_test::ReusePool pool;
  {
    atom_type a;
    VNodePtr x = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    a.store (x);
    VNodePtr y = lumex_test::make_pooled<VNode> (pool, ledger, 2);
    a.store (y); // x is replaced
    Engine::quiesce ();
    if (prompt)
      verdict.require (x.use_count () == 1,
                       "replaced_value_released: after store the replaced "
                       "value still has "
                           + std::to_string (x.use_count ())
                           + " owners instead of 1");
    x.reset ();
    if (prompt)
      verdict.require (ledger.alive () == 1,
                       "replaced_value_released: a replaced value outlives "
                       "store () (alive "
                           + std::to_string (ledger.alive ()) + ")");

    VNodePtr z = lumex_test::make_pooled<VNode> (pool, ledger, 3);
    {
      VNodePtr const old = a.exchange (z); // y is replaced
      verdict.require (
          equivalent_values (old, y),
          "replaced_value_released: exchange returned the wrong value");
    }
    Engine::quiesce ();
    if (prompt)
      verdict.require (y.use_count () == 1,
                       "replaced_value_released: after exchange the replaced "
                       "value still has "
                           + std::to_string (y.use_count ())
                           + " owners instead of 1");
    y.reset ();
    if (prompt)
      verdict.require (ledger.alive () == 1,
                       "replaced_value_released: a value replaced by "
                       "exchange () outlives the call (alive "
                           + std::to_string (ledger.alive ()) + ")");

    VNodePtr w = lumex_test::make_pooled<VNode> (pool, ledger, 4);
    VNodePtr expected = z;
    verdict.require (a.compare_exchange_strong (expected, w),
                     "replaced_value_released: compare_exchange failed");
    expected.reset ();
    Engine::quiesce ();
    if (prompt)
      verdict.require (z.use_count () == 1,
                       "replaced_value_released: after compare_exchange the "
                       "replaced value still has "
                           + std::to_string (z.use_count ())
                           + " owners instead of 1");
    z.reset ();
    if (prompt)
      verdict.require (ledger.alive () == 1,
                       "replaced_value_released: a value replaced by "
                       "compare_exchange () outlives the call (alive "
                           + std::to_string (ledger.alive ()) + ")");

    a.store (VNodePtr ());
    Engine::quiesce ();
    w.reset ();
    if (prompt)
      verdict.require (ledger.alive () == 0,
                       "replaced_value_released: storing an empty pointer "
                       "did not release the last value (alive "
                           + std::to_string (ledger.alive ()) + ")");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "replaced_value_released");
}

/**
 * @brief Reference-count balance under every operation and under
 * self-assignment: a few owners circulate through one atomic; afterwards
 * every owner is back at one reference.
 * @details The mix includes `store (load ())`, `exchange (load ())` and a
 * compare-exchange whose desired value is a fresh load of the same atomic
 * (the self-assignment races), loaded copies held for varying times, and
 * the replaced values of every kind.
 */
template <typename Engine>
void
owner_balance (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  int const owner_count = 4;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  {
    std::vector<VNodePtr> owners;
    for (int i = 0; i < owner_count; ++i)
      owners.push_back (lumex_test::make_pooled<VNode> (pool, ledger, i));
    {
      atom_type a (owners[0]);
      std::string const error = lumex_test::run_threads (
          p.threads,
          [&] (int t)
            {
              lumex_test::Schedule schedule (
                  p.schedule, lumex_test::derive_seed (
                                  p.seed, static_cast<std::uint64_t> (t), 14));
              VNodePtr held;
              for (int i = 0; i < p.iterations; ++i)
                {
                  VNodePtr const &target = owners[static_cast<std::size_t> (
                      schedule.random ().below (
                          static_cast<std::uint32_t> (owner_count)))];
                  switch (schedule.random ().below (8u))
                    {
                    case 0:
                      a.store (target);
                      break;
                    case 1:
                      {
                        VNodePtr const old = a.exchange (target);
                        check_node (old, verdict, "owner_balance exchange");
                        break;
                      }
                    case 2:
                      {
                        VNodePtr expected = a.load ();
                        a.compare_exchange_strong (expected, target);
                        break;
                      }
                    case 3:
                      a.store (a.load ());
                      break;
                    case 4:
                      {
                        VNodePtr const old = a.exchange (a.load ());
                        check_node (old, verdict,
                                    "owner_balance self exchange");
                        break;
                      }
                    case 5:
                      {
                        VNodePtr expected = a.load ();
                        a.compare_exchange_weak (expected, a.load ());
                        break;
                      }
                    case 6:
                      held = a.load (); // kept until a later iteration
                      check_node (held, verdict, "owner_balance load");
                      break;
                    default:
                      held.reset ();
                      break;
                    }
                  schedule.point ();
                }
            });
      verdict.require (error.empty (), error);
    }
    Engine::quiesce ();
    for (int i = 0; i < owner_count; ++i)
      verdict.require (
          owners[static_cast<std::size_t> (i)].use_count () == 1,
          "owner_balance: owner " + std::to_string (i) + " has "
              + std::to_string (
                  owners[static_cast<std::size_t> (i)].use_count ())
              + " references after the atomic is gone, 1 expected");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "owner_balance");
}

/**
 * @brief Many atomics hold one value; threads copy it between them and
 * replace it; afterwards every owner is back at one reference.
 */
template <typename Engine>
void
shared_value_fanout (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  int const owner_count = 3;
  int const atom_count = 7;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  {
    std::vector<VNodePtr> owners;
    for (int i = 0; i < owner_count; ++i)
      owners.push_back (lumex_test::make_pooled<VNode> (pool, ledger, i));
    {
      std::vector<std::unique_ptr<atom_type>> atoms;
      for (int i = 0; i < atom_count; ++i)
        atoms.push_back (
            std::unique_ptr<atom_type> (new atom_type (owners[0])));
      std::string const error = lumex_test::run_threads (
          p.threads,
          [&] (int t)
            {
              lumex_test::Schedule schedule (
                  p.schedule, lumex_test::derive_seed (
                                  p.seed, static_cast<std::uint64_t> (t), 15));
              for (int i = 0; i < p.iterations; ++i)
                {
                  atom_type &from = *atoms[schedule.random ().below (
                      static_cast<std::uint32_t> (atom_count))];
                  atom_type &to = *atoms[schedule.random ().below (
                      static_cast<std::uint32_t> (atom_count))];
                  switch (schedule.random ().below (4u))
                    {
                    case 0:
                      to.store (from.load ());
                      break;
                    case 1:
                      {
                        VNodePtr const old = to.exchange (from.load ());
                        check_node (old, verdict,
                                    "shared_value_fanout exchange");
                        break;
                      }
                    case 2:
                      {
                        VNodePtr expected = to.load ();
                        to.compare_exchange_strong (
                            expected,
                            owners[schedule.random ().below (
                                static_cast<std::uint32_t> (owner_count))]);
                        break;
                      }
                    default:
                      {
                        VNodePtr const seen = from.load ();
                        check_node (seen, verdict, "shared_value_fanout load");
                        break;
                      }
                    }
                  schedule.point ();
                }
            });
      verdict.require (error.empty (), error);
    }
    Engine::quiesce ();
    for (int i = 0; i < owner_count; ++i)
      verdict.require (
          owners[static_cast<std::size_t> (i)].use_count () == 1,
          "shared_value_fanout: owner " + std::to_string (i) + " has "
              + std::to_string (
                  owners[static_cast<std::size_t> (i)].use_count ())
              + " references after the atomics are gone, 1 expected");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "shared_value_fanout");
}

/// Holds a node until the thread that owns it exits.
struct ThreadExitHolder
{
  VNodePtr held;
};

/**
 * @brief Threads that exit while holding references: waves of short-lived
 * threads load, store, exchange and then end with a loaded value still held
 * in thread-local storage, so the last release of a node happens in the
 * destructor of a thread that is exiting, while others keep running.
 */
template <typename Engine>
void
thread_churn (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  int const waves = std::max (1, p.iterations / 4);
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (waves)
                                       * static_cast<std::size_t> (p.threads)
                                       * 4
                                   + 64);
  lumex_test::ReusePool pool;
  std::atomic<int> serial (0);
  {
    atom_type a (
        lumex_test::make_pooled<VNode> (pool, ledger, serial.fetch_add (1)));
    for (int wave = 0; wave < waves; ++wave)
      {
        std::vector<std::thread> threads;
        for (int t = 0; t < p.threads; ++t)
          threads.push_back (std::thread (
              [&, t, wave]
                {
                  static thread_local ThreadExitHolder holder;
                  lumex_test::SeededRandom random (lumex_test::derive_seed (
                      p.seed, static_cast<std::uint64_t> (wave),
                      static_cast<std::uint64_t> (t)));
                  holder.held = a.load ();
                  check_node (holder.held, verdict, "thread_churn load");
                  switch (random.below (3u))
                    {
                    case 0:
                      a.store (lumex_test::make_pooled<VNode> (
                          pool, ledger, serial.fetch_add (1)));
                      break;
                    case 1:
                      {
                        VNodePtr const old
                            = a.exchange (lumex_test::make_pooled<VNode> (
                                pool, ledger, serial.fetch_add (1)));
                        check_node (old, verdict, "thread_churn exchange");
                        break;
                      }
                    default:
                      {
                        VNodePtr expected = holder.held;
                        a.compare_exchange_strong (
                            expected, lumex_test::make_pooled<VNode> (
                                          pool, ledger, serial.fetch_add (1)));
                        break;
                      }
                    }
                  // The thread ends here, still holding a reference in
                  // `holder`.
                }));
        for (std::size_t i = 0; i < threads.size (); ++i)
          threads[i].join ();
      }
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "thread_churn");
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_LIFECYCLE_HPP

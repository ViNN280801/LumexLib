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
 * @file LumexAtomicScenarios.hpp
 * @brief Engine-parametrized checkers for atomic shared pointers: register
 * semantics, ABA through address reuse, linearizability, progress.
 * @details Every function here is a template over an `Engine` (a descriptor
 * with a nested `shared<T>` alias template, see `EngineUnderTest` in
 * `LumexAtomicTestSupport.hpp` and the descriptors of the test-side engines in
 * `LumexAtomicTestEngines.hpp`) and reports into a
 * `lumex_test::Verdict`. A suite runs a checker on the engine under test and
 * expects a clean verdict; the falsification suite runs the same checker on a
 * deliberately broken engine and expects a violation, which proves that the
 * checker can see the bug. The checkers never name an engine, so the
 * suites run unchanged on `atomic_shared_ptr_lock_based`, the lock-free
 * engine and the std-backed one.
 *
 * Objects that the checkers share carry a `lumex_test::LedgerEntry` and a
 * canary, and come from a `lumex_test::ReusePool`: the address of a destroyed
 * node is the address of the next node, the schedule that every ABA bug
 * needs.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/tests/core/atomic/LumexAtomicTestEngines.hpp"
#include "lumex/tests/support/LumexTestConfig.hpp"
#include "lumex/tests/support/LumexTestHistory.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
/// What a checker runs with.
struct Params
{
  Params ()
      : threads (4), iterations (500), seed (lumex_test::base_seed ()),
        schedule (lumex_test::ScheduleKind::tight)
  {
  }

  int threads;
  int iterations;
  std::uint64_t seed;
  lumex_test::ScheduleKind schedule;
};

/// Params for a thread count and a schedule, with the iterations of a test.
inline Params
make_params (int threads, int iterations, lumex_test::ScheduleKind schedule,
             std::uint64_t seed)
{
  Params p;
  p.threads = threads;
  p.iterations = iterations;
  p.schedule = schedule;
  p.seed = seed;
  return p;
}

/// A checker: runs the scenario for the given parameters.
typedef void (*Checker) (Params const &, lumex_test::Verdict &);

/**
 * @brief Runs @p checker for every thread count of the run
 * (`LUMEX_TEST_THREADS`, default 1, 2, 4, 8 and above the cores) under every
 * schedule kind (tight, yielding, jittered, cold start), collecting the
 * violations of all runs in @p verdict.
 * @details The jittered and cold kinds are slow per operation (sleeps, a
 * cache flush), so they run a fraction of the iterations. Each failure names
 * the thread count, the schedule and the seed; a rerun with
 * `LUMEX_TEST_SEED` and `LUMEX_TEST_THREADS` reproduces the same
 * random streams.
 */
inline void
run_matrix_for (std::vector<int> const &counts, Checker checker,
                int base_iterations, lumex_test::Verdict &verdict)
{
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  std::uint64_t entry = 0;
  for (std::size_t c = 0; c < counts.size (); ++c)
    for (std::size_t k = 0; k < kinds.size (); ++k)
      {
        int iterations = base_iterations;
        if (kinds[k] == lumex_test::ScheduleKind::jittered)
          iterations = std::max (1, base_iterations / 6);
        else if (kinds[k] == lumex_test::ScheduleKind::cold)
          iterations = std::max (1, base_iterations / 8);
        Params p;
        p.threads = counts[c];
        p.iterations = iterations;
        p.schedule = kinds[k];
        p.seed = lumex_test::derive_seed (lumex_test::base_seed (), entry++);
        lumex_test::Verdict run;
        checker (p, run);
        if (!run.ok ())
          verdict.fail (
              "[threads=" + std::to_string (p.threads)
              + " schedule=" + lumex_test::schedule_name (p.schedule) + " "
              + lumex_test::replay_text (lumex_test::base_seed (), p.threads)
              + "] " + run.text ());
      }
}

/// `run_matrix_for` over the thread counts of the run.
inline void
run_matrix (Checker checker, int base_iterations, lumex_test::Verdict &verdict)
{
  run_matrix_for (lumex_test::thread_counts (), checker, base_iterations,
                  verdict);
}

/// The thread counts of histories that a checker can search: 2 and 3.
inline std::vector<int>
history_thread_counts ()
{
  std::vector<int> counts;
  counts.push_back (2);
  counts.push_back (3);
  return counts;
}

/// A numbered node: ledger entry, version and a check word.
struct VNode
{
  VNode (lumex_test::ObjectLedger &ledger, int v)
      : entry (ledger), version (v), check (~v)
  {
  }

  /// True while the node is alive and not overwritten.
  bool
  intact () const
  {
    return entry.intact () && check == ~version;
  }

  lumex_test::LedgerEntry entry;
  int version;
  int check;
};

typedef std::shared_ptr<VNode> VNodePtr;
} // namespace scenario

/// Content equality of nodes, for the engine that wrongly compares by it.
template <> struct ContentEqual<scenario::VNode>
{
  static bool
  equal (scenario::VNode const &lhs, scenario::VNode const &rhs)
  {
    return lhs.version == rhs.version;
  }
};

namespace scenario
{

/// Reports a problem of a loaded node and returns false when it is bad.
inline bool
check_node (VNodePtr const &node, lumex_test::Verdict &verdict,
            char const *where)
{
  if (!node)
    {
      verdict.fail (std::string (where) + ": a null node was loaded");
      return false;
    }
  if (!node->intact ())
    {
      verdict.fail (std::string (where)
                    + ": a node that is destroyed or overwritten was loaded");
      return false;
    }
  return true;
}

/// Ledger capacity for a scenario: generous, in nodes.
inline std::size_t
node_capacity (Params const &p, int per_attempt)
{
  return static_cast<std::size_t> (p.threads)
             * static_cast<std::size_t> (p.iterations)
             * static_cast<std::size_t> (per_attempt)
         + 64;
}

/// Records the end state of the ledger and the pool in the verdict.
inline void
check_balance (lumex_test::ObjectLedger const &ledger,
               lumex_test::ReusePool const &pool, lumex_test::Verdict &verdict,
               char const *where)
{
  verdict.require (ledger.balanced (),
                   std::string (where) + ": ledger " + ledger.report ());
  verdict.require (pool.live () == 0,
                   std::string (where) + ": control blocks still allocated: "
                       + std::to_string (pool.live ()));
}

/**
 * @brief While the atomic still exists and no thread holds anything, only the
 * stored value may be alive (for an engine that destroys a replaced value
 * in the call): a replaced value that is still alive is a leak that the
 * balance after the atomic's destruction would hide.
 */
inline void
check_only_stored_alive (lumex_test::ObjectLedger const &ledger,
                         std::size_t expected_alive, bool prompt,
                         lumex_test::Verdict &verdict, char const *where)
{
  if (prompt)
    verdict.require (ledger.alive () == expected_alive,
                     std::string (where) + ": "
                         + std::to_string (ledger.alive ())
                         + " objects are alive while only the stored value "
                           "should be (a replaced value was not released)");
}

// --- counter built from CAS loops -------------------------------------------

/**
 * @brief N threads increment a version by compare-exchange loops over fresh
 * nodes: the final version must be N times the increments (no lost update),
 * every loaded node must be intact, every node destroyed once.
 * @details A compare-exchange that succeeds against a value that is no longer
 * current loses another thread's update; one that treats a recycled address
 * as the expected object lets a stale increment through. Either shows as a
 * wrong final version.
 */
template <typename Engine>
void
cas_counter (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (node_capacity (p, p.threads + 2));
  lumex_test::ReusePool pool;
  std::atomic<long> wins (0);
  long final_version = -1;
  {
    atom_type a (lumex_test::make_pooled<VNode> (pool, ledger, 0));
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 1));
            for (int i = 0; i < p.iterations; ++i)
              {
                VNodePtr expected = a.load ();
                for (;;)
                  {
                    if (!check_node (expected, verdict, "cas_counter"))
                      return;
                    VNodePtr desired = lumex_test::make_pooled<VNode> (
                        pool, ledger, expected->version + 1);
                    if (a.compare_exchange_weak (expected, desired))
                      {
                        wins.fetch_add (1);
                        break;
                      }
                    schedule.point ();
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    VNodePtr const last = a.load ();
    if (check_node (last, verdict, "cas_counter final"))
      final_version = last->version;
    Engine::quiesce ();
    check_only_stored_alive (ledger, 1, Engine::replaced_value_dies_in_call (),
                             verdict, "cas_counter");
  }
  Engine::quiesce ();
  long const expected_total = static_cast<long> (p.threads) * p.iterations;
  verdict.require (wins.load () == expected_total,
                   "cas_counter: successes " + std::to_string (wins.load ())
                       + " != " + std::to_string (expected_total));
  verdict.require (final_version == expected_total,
                   "cas_counter: final version "
                       + std::to_string (final_version)
                       + " != " + std::to_string (expected_total)
                       + " (an update was lost or applied twice)");
  check_balance (ledger, pool, verdict, "cas_counter");
}

// --- exchange permutation
// ----------------------------------------------------

/**
 * @brief Every thread exchanges unique tokens in; the tokens that come out,
 * plus the one left, must be exactly the tokens that went in plus the
 * initial one, each once.
 */
template <typename Engine>
void
exchange_permutation (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (node_capacity (p, 1));
  lumex_test::ReusePool pool;
  std::vector<std::vector<int>> returned (
      static_cast<std::size_t> (p.threads));
  int last_token = -1;
  {
    atom_type a (lumex_test::make_pooled<VNode> (pool, ledger, 0));
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 2));
            std::vector<int> &mine = returned[static_cast<std::size_t> (t)];
            for (int i = 0; i < p.iterations; ++i)
              {
                int const token = t * p.iterations + i + 1;
                VNodePtr old = a.exchange (
                    lumex_test::make_pooled<VNode> (pool, ledger, token));
                if (!check_node (old, verdict, "exchange_permutation"))
                  return;
                mine.push_back (old->version);
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    VNodePtr const last = a.load ();
    if (check_node (last, verdict, "exchange_permutation final"))
      last_token = last->version;
    Engine::quiesce ();
    check_only_stored_alive (ledger, 1, Engine::replaced_value_dies_in_call (),
                             verdict, "exchange_permutation");
  }
  Engine::quiesce ();
  std::vector<int> all;
  for (std::size_t t = 0; t < returned.size (); ++t)
    all.insert (all.end (), returned[t].begin (), returned[t].end ());
  all.push_back (last_token);
  std::sort (all.begin (), all.end ());
  int const total = p.threads * p.iterations + 1;
  bool permutation = static_cast<int> (all.size ()) == total;
  for (int i = 0; permutation && i < total; ++i)
    permutation = all[static_cast<std::size_t> (i)] == i;
  verdict.require (permutation,
                   "exchange_permutation: the tokens that came out are not "
                   "the tokens that went in exactly once (a value was "
                   "returned twice or lost)");
  check_balance (ledger, pool, verdict, "exchange_permutation");
}

// --- monotone loads
// ------------------------------------------------------------

/**
 * @brief One writer stores increasing versions; no reader may see a version
 * go backwards, see less than was published before the load started, or
 * read an older value than the writer itself stored.
 */
template <typename Engine>
void
monotone_loads (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  int const readers = std::max (1, p.threads - 1);
  lumex_test::ObjectLedger ledger (node_capacity (p, 1) + 8);
  lumex_test::ReusePool pool;
  std::atomic<int> published (0);
  std::atomic<bool> done (false);
  {
    atom_type a (lumex_test::make_pooled<VNode> (pool, ledger, 0));
    std::string const error = lumex_test::run_threads (
        readers + 1,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 3));
            if (t == 0)
              {
                for (int version = 1; version <= p.iterations; ++version)
                  {
                    a.store (lumex_test::make_pooled<VNode> (pool, ledger,
                                                             version));
                    VNodePtr const mine = a.load ();
                    if (check_node (mine, verdict, "monotone_loads writer")
                        && mine->version < version)
                      verdict.fail (
                          "monotone_loads: the writer loaded version "
                          + std::to_string (mine->version) + " after storing "
                          + std::to_string (version));
                    published.store (version);
                    schedule.point ();
                  }
                done.store (true);
                return;
              }
            int last = -1;
            while (!done.load ())
              {
                int const floor = published.load ();
                VNodePtr const seen = a.load ();
                if (!check_node (seen, verdict, "monotone_loads reader"))
                  return;
                if (seen->version < floor)
                  verdict.fail ("monotone_loads: a load returned version "
                                + std::to_string (seen->version) + " although "
                                + std::to_string (floor)
                                + " was published before it started");
                if (seen->version < last)
                  verdict.fail ("monotone_loads: versions went backwards, "
                                + std::to_string (last) + " then "
                                + std::to_string (seen->version));
                last = seen->version;
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    VNodePtr const last = a.load ();
    if (check_node (last, verdict, "monotone_loads final"))
      verdict.require (last->version == p.iterations,
                       "monotone_loads: the final version is "
                           + std::to_string (last->version));
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "monotone_loads");
}

// --- strong compare-exchange precision
// ------------------------------------------

/**
 * @brief A failed strong compare-exchange must have found a value that is not
 * equivalent to the expected one; the number of failures of a loop is bounded
 * by the modifications of the others.
 * @details This is also the "bounded retries" check of the livelock
 * regression: a loop that fails more often than there were foreign successes
 * waits for something other than the others' progress.
 */
template <typename Engine>
void
strong_cas_precision (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (node_capacity (p, p.threads + 2));
  lumex_test::ReusePool pool;
  std::atomic<long> wins (0);
  std::atomic<long> excess (0);
  {
    atom_type a (lumex_test::make_pooled<VNode> (pool, ledger, 0));
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 4));
            for (int i = 0; i < p.iterations; ++i)
              {
                long const wins_before = wins.load ();
                long retries = 0;
                VNodePtr expected = a.load ();
                for (;;)
                  {
                    if (!check_node (expected, verdict,
                                     "strong_cas_precision"))
                      return;
                    VNodePtr const old_expected = expected;
                    VNodePtr desired = lumex_test::make_pooled<VNode> (
                        pool, ledger, expected->version + 1);
                    if (a.compare_exchange_strong (expected, desired))
                      {
                        wins.fetch_add (1);
                        break;
                      }
                    ++retries;
                    if (equivalent_values (expected, old_expected))
                      verdict.fail (
                          "strong_cas_precision: compare_exchange_strong "
                          "failed although the stored value was "
                          "equivalent to the expected one");
                    schedule.point ();
                  }
                long const foreign = wins.load () - wins_before + p.threads;
                if (retries > foreign)
                  excess.fetch_add (1);
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
  }
  Engine::quiesce ();
  verdict.require (
      excess.load () == 0,
      "strong_cas_precision: " + std::to_string (excess.load ())
          + " loops retried more often than the others succeeded");
  verdict.require (wins.load ()
                       == static_cast<long> (p.threads) * p.iterations,
                   "strong_cas_precision: wrong number of successes");
  check_balance (ledger, pool, verdict, "strong_cas_precision");
}

// --- the value universe of the identity checks
// -----------------------------------

/// Two ints, so aliasing pointers can name different members.
struct Pair
{
  int first;
  int second;
};

/**
 * @brief Six values of `std::shared_ptr<int>` with every identity relation of
 * the equivalence rule: empty; two pointers of one owner; two owners; the
 * same stored pointer under a different owner; a plain object.
 * @details Ids: 0 empty; 1 and 2 share an owner and differ in the stored
 * pointer; 3 is a second owner; 4 stores the pointer of 1 under the owner of
 * 3 (same pointer, different owner); 5 is a plain object. Two values are
 * equivalent exactly when their ids are equal.
 */
class Universe
{
public:
  Universe ()
  {
    std::shared_ptr<Pair> x (new Pair ());
    std::shared_ptr<Pair> y (new Pair ());
    values_.push_back (std::shared_ptr<int> ());
    values_.push_back (std::shared_ptr<int> (x, &x->first));
    values_.push_back (std::shared_ptr<int> (x, &x->second));
    values_.push_back (std::shared_ptr<int> (y, &y->first));
    values_.push_back (std::shared_ptr<int> (y, &x->first));
    values_.push_back (std::make_shared<int> (5));
  }

  /// Number of values.
  int
  size () const
  {
    return static_cast<int> (values_.size ());
  }

  /// The value with id @p id.
  std::shared_ptr<int> const &
  value (int id) const
  {
    return values_[static_cast<std::size_t> (id)];
  }

  /// The id of the value equivalent to @p p, or -1.
  int
  id_of (std::shared_ptr<int> const &p) const
  {
    for (std::size_t i = 0; i < values_.size (); ++i)
      if (equivalent_values (values_[i], p))
        return static_cast<int> (i);
    return -1;
  }

private:
  std::vector<std::shared_ptr<int>> values_;
};

// --- identity of the equivalence rule
// ---------------------------------------------

/**
 * @brief Every (current, expected, desired) triple of @p values: the
 * compare-exchange succeeds exactly when current and expected are
 * equivalent (same pointer AND same ownership), stores desired then, and
 * leaves the current value in `expected` otherwise.
 * @details @p values must be pairwise non-equivalent (index = identity).
 * Covers aliasing with the same stored pointer and different owners, with
 * the same owner and different pointers, and the empty value, for the
 * strong form, the weak form (a failure of equal values is retried a
 * bounded number of times) and `exchange`.
 * @tparam Atom The atomic shared pointer type for `T`.
 */
template <typename Atom, typename T>
void
identity_rule_over (std::vector<std::shared_ptr<T>> const &values,
                    lumex_test::Verdict &verdict)
{
  int const n = static_cast<int> (values.size ());
  auto id_of = [&] (std::shared_ptr<T> const &p) -> int
    {
      for (int i = 0; i < n; ++i)
        if (equivalent_values (values[static_cast<std::size_t> (i)], p))
          return i;
      return -1;
    };
  for (int cur = 0; cur < n; ++cur)
    for (int exp = 0; exp < n; ++exp)
      for (int des = 0; des < n; ++des)
        for (int form = 0; form < 2; ++form)
          {
            Atom a (values[static_cast<std::size_t> (cur)]);
            std::shared_ptr<T> expected
                = values[static_cast<std::size_t> (exp)];
            bool ok = false;
            if (form == 0)
              ok = a.compare_exchange_strong (
                  expected, values[static_cast<std::size_t> (des)]);
            else
              {
                // The weak form may fail spuriously only for equal values.
                for (int attempt = 0; attempt < 1000 && !ok; ++attempt)
                  {
                    ok = a.compare_exchange_weak (
                        expected, values[static_cast<std::size_t> (des)]);
                    if (ok || cur != exp)
                      break;
                    expected = values[static_cast<std::size_t> (exp)];
                  }
              }
            std::string const where
                = std::string (form == 0 ? "strong" : "weak") + " current="
                  + std::to_string (cur) + " expected=" + std::to_string (exp)
                  + " desired=" + std::to_string (des);
            if (cur == exp)
              {
                verdict.require (ok,
                                 "identity_rule: equivalent values did not "
                                 "exchange: "
                                     + where);
                verdict.require (id_of (a.load ()) == des,
                                 "identity_rule: desired was not stored: "
                                     + where);
              }
            else
              {
                verdict.require (!ok, "identity_rule: a different value was "
                                      "treated as equivalent (same pointer or "
                                      "same owner is not enough): "
                                          + where);
                verdict.require (id_of (expected) == cur,
                                 "identity_rule: expected was not refreshed "
                                 "with the current value: "
                                     + where);
                verdict.require (
                    id_of (a.load ()) == cur,
                    "identity_rule: the value changed on a failed "
                    "compare-exchange: "
                        + where);
              }
          }
  for (int cur = 0; cur < n; ++cur)
    for (int des = 0; des < n; ++des)
      {
        Atom a (values[static_cast<std::size_t> (cur)]);
        std::shared_ptr<T> const old
            = a.exchange (values[static_cast<std::size_t> (des)]);
        verdict.require (id_of (old) == cur,
                         "identity_rule: exchange returned the wrong value");
        verdict.require (id_of (a.load ()) == des,
                         "identity_rule: exchange stored the wrong value");
      }
}

/// `identity_rule_over` for the six values of the `Universe`.
template <typename Engine>
void
identity_rule (lumex_test::Verdict &verdict)
{
  Universe const universe;
  std::vector<std::shared_ptr<int>> values;
  for (int i = 0; i < universe.size (); ++i)
    values.push_back (universe.value (i));
  identity_rule_over<typename Engine::template shared<int>> (values, verdict);
}

// --- the walk of the read-modify-write operations
// ---------------------------------

/**
 * @brief Threads exchange and compare-exchange among the values of the
 * universe; the successful operations must form one walk through the values.
 * @details Every successful read-modify-write operation is an edge from the
 * value it replaced to the value it stored (`exchange`: the returned value;
 * `compare_exchange`: the expected value). The values of the atomic follow
 * one path from the initial value to the final one, so for every value the
 * edges that leave it minus the edges that enter it are 1 for the initial
 * value, -1 for the final one (0 if both are the same value, or neither) and
 * 0 for the others. A compare-exchange that wins against a value it does not
 * match (same pointer under another owner, the same owner under another
 * pointer), two winners from one value, an exchange that returns a value
 * twice or loses one, all break the balance, whatever the interleaving. The
 * check needs no history, so it runs for thousands of operations. Reference
 * counts of every owner must be back at their start values afterwards.
 */
template <typename Engine>
void
rmw_walk (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<int> atom_type;
  Universe const universe;
  std::size_t const n = static_cast<std::size_t> (universe.size ());
  std::vector<long> base_counts;
  for (std::size_t i = 0; i < n; ++i)
    base_counts.push_back (universe.value (static_cast<int> (i)).use_count ());
  std::vector<std::vector<long>> edges (static_cast<std::size_t> (p.threads),
                                        std::vector<long> (n * n, 0));
  int const initial = 1;
  int final_id = -2;
  {
    atom_type a (universe.value (initial));
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 10));
            std::vector<long> &mine = edges[static_cast<std::size_t> (t)];
            for (int i = 0; i < p.iterations; ++i)
              {
                int const want = static_cast<int> (
                    schedule.random ().below (static_cast<std::uint32_t> (n)));
                int const next = static_cast<int> (
                    schedule.random ().below (static_cast<std::uint32_t> (n)));
                switch (schedule.random ().below (4u))
                  {
                  case 0:
                    {
                      std::shared_ptr<int> const old
                          = a.exchange (universe.value (next));
                      int const old_id = universe.id_of (old);
                      if (old_id < 0)
                        verdict.fail ("rmw_walk: exchange returned a value "
                                      "that was never stored");
                      else
                        ++mine[static_cast<std::size_t> (old_id) * n
                               + static_cast<std::size_t> (next)];
                      break;
                    }
                  case 1:
                    {
                      std::shared_ptr<int> expected = universe.value (want);
                      if (a.compare_exchange_strong (expected,
                                                     universe.value (next)))
                        ++mine[static_cast<std::size_t> (want) * n
                               + static_cast<std::size_t> (next)];
                      else if (universe.id_of (expected) < 0)
                        verdict.fail (
                            "rmw_walk: a failed compare-exchange returned "
                            "a value that was never stored");
                      break;
                    }
                  case 2:
                    {
                      std::shared_ptr<int> expected = universe.value (want);
                      if (a.compare_exchange_weak (expected,
                                                   universe.value (next)))
                        ++mine[static_cast<std::size_t> (want) * n
                               + static_cast<std::size_t> (next)];
                      else if (universe.id_of (expected) < 0)
                        verdict.fail (
                            "rmw_walk: a failed compare-exchange returned "
                            "a value that was never stored");
                      break;
                    }
                  default:
                    {
                      // A compare-exchange from the value just loaded: the
                      // likeliest to win, so the walk really moves.
                      std::shared_ptr<int> expected = a.load ();
                      int const seen = universe.id_of (expected);
                      if (seen < 0)
                        {
                          verdict.fail ("rmw_walk: load returned a value that "
                                        "was never stored");
                          break;
                        }
                      if (a.compare_exchange_strong (expected,
                                                     universe.value (next)))
                        ++mine[static_cast<std::size_t> (seen) * n
                               + static_cast<std::size_t> (next)];
                      break;
                    }
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    final_id = universe.id_of (a.load ());
  }
  Engine::quiesce ();
  verdict.require (final_id >= 0,
                   "rmw_walk: the final value was never stored");
  for (std::size_t v = 0; v < n; ++v)
    {
      long out = 0;
      long in = 0;
      for (std::size_t t = 0; t < edges.size (); ++t)
        for (std::size_t w = 0; w < n; ++w)
          {
            out += edges[t][v * n + w];
            in += edges[t][w * n + v];
          }
      long const want = (static_cast<int> (v) == initial ? 1 : 0)
                        - (static_cast<int> (v) == final_id ? 1 : 0);
      verdict.require (out - in == want,
                       "rmw_walk: value " + std::to_string (v) + " has "
                           + std::to_string (out) + " outgoing and "
                           + std::to_string (in)
                           + " incoming edges; the walk of the stored values "
                             "is broken (a lost update, a double winner or a "
                             "match on the wrong identity)");
    }
  for (std::size_t i = 0; i < n; ++i)
    verdict.require (
        universe.value (static_cast<int> (i)).use_count () == base_counts[i],
        "rmw_walk: reference count of value " + std::to_string (i) + " is "
            + std::to_string (
                universe.value (static_cast<int> (i)).use_count ())
            + ", it was " + std::to_string (base_counts[i]));
}

// --- ABA through address reuse
// -----------------------------------------------------

/**
 * @brief A node dies and a new node takes its address; a handle that names
 * only the old address (stored pointer without ownership) must not compare
 * equal to the new node.
 * @details Deterministic. `std::allocate_shared` over a `ReusePool` returns
 * the address of the destroyed node (object and control block) for the next
 * node, which the check asserts first (otherwise it proves nothing). The
 * stale handle is an aliasing `std::shared_ptr` of that raw address with an
 * empty owner. The compare-exchange from it, with the new node stored, must
 * fail, report the new node and change nothing; so must `exchange`'s
 * result differ and a weak form; and after the old A is stored again A -> B ->
 * A with the very same owner the compare-exchange must succeed.
 */
template <typename Engine>
void
reuse_equivalence (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  if (!Engine::replaced_value_dies_in_call ())
    return; // a deferred engine frees the node later: no address to reuse
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  for (int form = 0; form < 2; ++form)
    {
      VNodePtr stale;
      {
        atom_type a;
        VNodePtr first = lumex_test::make_pooled<VNode> (pool, ledger, 1);
        VNode *const raw = first.get ();
        stale = VNodePtr (std::shared_ptr<void> (), raw);
        a.store (first);
        a.store (lumex_test::make_pooled<VNode> (pool, ledger, 2));
        first.reset ();
        Engine::quiesce ();
        VNodePtr second = lumex_test::make_pooled<VNode> (pool, ledger, 3);
        if (second.get () != raw)
          {
            verdict.fail ("reuse_equivalence: the allocator did not hand the "
                          "address back; the check proves nothing");
            return;
          }
        a.store (second);
        // Same stored pointer, different ownership.
        bool ok = false;
        VNodePtr desired = lumex_test::make_pooled<VNode> (pool, ledger, 4);
        if (form == 0)
          ok = a.compare_exchange_strong (stale, desired);
        else
          ok = a.compare_exchange_weak (stale, desired);
        verdict.require (!ok, "reuse_equivalence: a new object at the address "
                              "of a dead one compared equal to a handle that "
                              "only names that address");
        verdict.require (equivalent_values (stale, second),
                         "reuse_equivalence: expected was not refreshed with "
                         "the stored node");
        verdict.require (equivalent_values (a.load (), second),
                         "reuse_equivalence: a failed compare-exchange "
                         "changed the stored value");
        verdict.require (a.load ()->version == 3,
                         "reuse_equivalence: wrong node stored");
      }
      Engine::quiesce ();
    }
  check_balance (ledger, pool, verdict, "reuse_equivalence");
}

/**
 * @brief A -> B -> A with the same owner: a compare-exchange that holds the
 * first A succeeds; A -> B -> A' (a new object, even an equal one) fails.
 */
template <typename Engine>
void
aba_same_owner (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  {
    VNodePtr const a_owner = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    VNodePtr const b_owner = lumex_test::make_pooled<VNode> (pool, ledger, 2);
    atom_type a (a_owner);
    VNodePtr held = a.load ();
    a.store (b_owner);
    a.store (a_owner);
    VNodePtr desired = lumex_test::make_pooled<VNode> (pool, ledger, 9);
    verdict.require (a.compare_exchange_strong (held, desired),
                     "aba_same_owner: A -> B -> A (the same object) must "
                     "compare equal to the A that was loaded first");
    verdict.require (a.load ()->version == 9, "aba_same_owner: wrong value");

    VNodePtr held_again = a.load ();
    a.store (b_owner);
    a.store (
        lumex_test::make_pooled<VNode> (pool, ledger, 9)); // equal content
    VNodePtr other = lumex_test::make_pooled<VNode> (pool, ledger, 10);
    verdict.require (!a.compare_exchange_strong (held_again, other),
                     "aba_same_owner: A -> B -> A' (a new object with equal "
                     "content) must not compare equal");
    verdict.require (held_again->version == 9
                         && !equivalent_values (held_again, other),
                     "aba_same_owner: expected not refreshed");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "aba_same_owner");
}

/**
 * @brief Threads replace the value with fresh nodes from the reuse pool and
 * compare-exchange from loaded values, while others only read: every loaded
 * node must be intact (a recycled or freed node is the use-after-free of an
 * unprotected engine), and every node is destroyed exactly once.
 * @details The address of each destroyed node is the address of the next
 * one, so a reader that was handed a pointer to a node that was freed in
 * the meantime sees a poisoned or foreign node, and under
 * AddressSanitizer a use-after-poison report.
 */
template <typename Engine>
void
reuse_stress (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (node_capacity (p, 3));
  lumex_test::ReusePool pool;
  std::atomic<int> serial (0);
  {
    atom_type a (
        lumex_test::make_pooled<VNode> (pool, ledger, serial.fetch_add (1)));
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 5));
            for (int i = 0; i < p.iterations; ++i)
              {
                switch (schedule.random ().below (5u))
                  {
                  case 0:
                    a.store (lumex_test::make_pooled<VNode> (
                        pool, ledger, serial.fetch_add (1)));
                    break;
                  case 1:
                    {
                      VNodePtr old
                          = a.exchange (lumex_test::make_pooled<VNode> (
                              pool, ledger, serial.fetch_add (1)));
                      check_node (old, verdict, "reuse_stress exchange");
                      break;
                    }
                  case 2:
                    {
                      VNodePtr expected = a.load ();
                      if (check_node (expected, verdict,
                                      "reuse_stress cas load"))
                        a.compare_exchange_strong (
                            expected, lumex_test::make_pooled<VNode> (
                                          pool, ledger, serial.fetch_add (1)));
                      break;
                    }
                  default:
                    {
                      VNodePtr seen = a.load ();
                      check_node (seen, verdict, "reuse_stress load");
                      break;
                    }
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    Engine::quiesce ();
    check_only_stored_alive (ledger, 1, Engine::replaced_value_dies_in_call (),
                             verdict, "reuse_stress");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "reuse_stress");
  if (Engine::replaced_value_dies_in_call () && p.threads * p.iterations > 200)
    verdict.require (pool.reuses () > 0,
                     "reuse_stress: no address was ever reused; the run "
                     "proves nothing about ABA");
}

// --- linearizability of the register
// --------------------------------------------------

/**
 * @brief Records short concurrent histories on one atomic and checks each
 * with the Wing and Gong search against a register with exchange and
 * compare-exchange.
 * @details Values come from the `Universe` (empty, aliasing and plain
 * pointers), so the model's "equal id" is exactly the standard's equivalence.
 * Each round starts @p p.threads threads (2 or 3) with 3 to 5 operations
 * each; the iterations are rounds. The first non-linearizable history is
 * reported with its operations.
 */
template <typename Engine>
void
linearizable_register (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<int> atom_type;
  Universe const universe;
  lumex_test::SeededRandom plan (lumex_test::derive_seed (p.seed, 77));
  bool reported = false;
  for (int round = 0; round < p.iterations && !reported; ++round)
    {
      int const initial = static_cast<int> (
          plan.below (static_cast<std::uint32_t> (universe.size ())));
      atom_type a (universe.value (initial));
      lumex_test::HistoryRecorder recorder (p.threads);
      std::uint64_t const round_seed = lumex_test::derive_seed (
          p.seed, static_cast<std::uint64_t> (round), 6);
      std::string const error = lumex_test::run_threads (
          p.threads,
          [&] (int t)
            {
              lumex_test::Schedule schedule (
                  p.schedule, lumex_test::derive_seed (
                                  round_seed, static_cast<std::uint64_t> (t)));
              int const ops
                  = 3 + static_cast<int> (schedule.random ().below (3u));
              for (int i = 0; i < ops; ++i)
                {
                  int const arg0 = static_cast<int> (schedule.random ().below (
                      static_cast<std::uint32_t> (universe.size ())));
                  int const arg1 = static_cast<int> (schedule.random ().below (
                      static_cast<std::uint32_t> (universe.size ())));
                  std::uint64_t const invoke = recorder.begin ();
                  switch (schedule.random ().below (5u))
                    {
                    case 0:
                      {
                        std::shared_ptr<int> seen = a.load ();
                        recorder.end (t, lumex_test::register_load, invoke, 0,
                                      0, universe.id_of (seen), 0);
                        break;
                      }
                    case 1:
                      a.store (universe.value (arg0));
                      recorder.end (t, lumex_test::register_store, invoke,
                                    arg0, 0, 0, 0);
                      break;
                    case 2:
                      {
                        std::shared_ptr<int> old
                            = a.exchange (universe.value (arg0));
                        recorder.end (t, lumex_test::register_exchange, invoke,
                                      arg0, 0, universe.id_of (old), 0);
                        break;
                      }
                    case 3:
                      {
                        std::shared_ptr<int> expected = universe.value (arg0);
                        bool const ok = a.compare_exchange_strong (
                            expected, universe.value (arg1));
                        recorder.end (t, lumex_test::register_cas_strong,
                                      invoke, arg0, arg1, ok ? 1 : 0,
                                      ok ? 0 : universe.id_of (expected));
                        break;
                      }
                    default:
                      {
                        std::shared_ptr<int> expected = universe.value (arg0);
                        bool const ok = a.compare_exchange_weak (
                            expected, universe.value (arg1));
                        recorder.end (t, lumex_test::register_cas_weak, invoke,
                                      arg0, arg1, ok ? 1 : 0,
                                      ok ? 0 : universe.id_of (expected));
                        break;
                      }
                    }
                  schedule.point ();
                }
            });
      verdict.require (error.empty (), error);
      std::vector<lumex_test::HistoryOp> const history = recorder.history ();
      if (lumex_test::is_linearizable<lumex_test::RegisterModel> (history,
                                                                  initial)
          == lumex_test::Linearizability::not_linearizable)
        {
          verdict.fail (
              "linearizable_register: round " + std::to_string (round)
              + " (initial value " + std::to_string (initial)
              + ") is not linearizable:"
              + lumex_test::describe_history<lumex_test::RegisterModel> (
                  history));
          reported = true;
        }
    }
}

// --- progress
// ---------------------------------------------------------------------------

/**
 * @brief Loaders and compare-exchange threads for a fixed time; every
 * compare-exchange thread must win at least @p minimum_each times and the
 * group at least @p minimum_total times.
 * @details The regression of the livelock of the lock-free libc++ attempt:
 * a compare-exchange that waited for in-flight loads starved when loads kept
 * coming. The pattern is the benchmark one, `load (relaxed)` followed by a
 * strong compare-exchange between two owners, with extra loader threads
 * that never stop reading.
 */
template <typename Engine>
void
progress_under_loaders (int loaders, int swappers, int milliseconds,
                        long minimum_each, long minimum_total,
                        std::uint64_t seed, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> atom_type;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  std::vector<long> wins (static_cast<std::size_t> (swappers), 0);
  {
    VNodePtr const left = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    VNodePtr const right = lumex_test::make_pooled<VNode> (pool, ledger, 2);
    atom_type a (left);
    std::atomic<bool> stop (false);
    std::atomic<int> started (0);
    std::string const error = lumex_test::run_threads (
        loaders + swappers + 1,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                lumex_test::ScheduleKind::yielding,
                lumex_test::derive_seed (seed, static_cast<std::uint64_t> (t),
                                         7));
            if (t == 0)
              {
                while (started.load () < loaders + swappers)
                  std::this_thread::yield ();
                std::this_thread::sleep_for (
                    std::chrono::milliseconds (milliseconds));
                stop.store (true);
                return;
              }
            started.fetch_add (1);
            if (t <= loaders)
              {
                while (!stop.load ())
                  {
                    VNodePtr const seen = a.load (std::memory_order_relaxed);
                    check_node (seen, verdict, "progress loader");
                    schedule.point ();
                  }
                return;
              }
            long &mine = wins[static_cast<std::size_t> (t - loaders - 1)];
            while (!stop.load ())
              {
                VNodePtr expected = a.load (std::memory_order_relaxed);
                VNodePtr const desired
                    = equivalent_values (expected, left) ? right : left;
                if (a.compare_exchange_strong (expected, desired))
                  ++mine;
              }
          });
    verdict.require (error.empty (), error);
  }
  Engine::quiesce ();
  long total = 0;
  for (std::size_t i = 0; i < wins.size (); ++i)
    {
      total += wins[i];
      verdict.require (wins[i] >= minimum_each,
                       "progress: swapper " + std::to_string (i) + " won only "
                           + std::to_string (wins[i]) + " times in "
                           + std::to_string (milliseconds) + " ms ("
                           + std::to_string (loaders) + " loaders, "
                           + std::to_string (swappers) + " swappers)");
    }
  verdict.require (total >= minimum_total,
                   "progress: the group won only " + std::to_string (total)
                       + " compare-exchanges in "
                       + std::to_string (milliseconds) + " ms");
  check_balance (ledger, pool, verdict, "progress");
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_HPP

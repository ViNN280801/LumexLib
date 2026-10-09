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
 * @file LumexAtomicScenariosWeak.hpp
 * @brief Engine-parametrized checkers for `atomic_weak_ptr`: identity under
 * address reuse, promotion racing the last release, `expired ()` races.
 * @details Same shape as `LumexAtomicScenarios.hpp`: templates over an
 * `Engine` with nested `shared<T>` and `weak<T>` alias templates, reporting
 * into a `lumex_test::Verdict`.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_WEAK_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_WEAK_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
typedef std::weak_ptr<VNode> VNodeWeak;

/**
 * @brief Weak pointers compare by owner: A -> B -> A with the same owner
 * (even after A expired) matches, a different owner does not, also when
 * both are expired.
 * @details The control block of a node that a weak pointer pins is not
 * released, so its address cannot be reused while the weak pointer lives;
 * the test asserts that, and then checks that the owner, not the (now
 * unobservable) object, decides.
 */
template <typename Engine>
void
weak_owner_identity (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template weak<VNode> weak_atom;
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  {
    weak_atom a;
    VNodePtr first = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    VNode *const first_address = first.get ();
    VNodeWeak const pinned (first);
    a.store (pinned);
    first.reset (); // the object dies, its control block is pinned
    VNodePtr second = lumex_test::make_pooled<VNode> (pool, ledger, 2);
    verdict.require (second.get () != first_address,
                     "weak_owner_identity: a pinned control block was reused");
    VNodeWeak const other (second);

    // A -> B -> A, the same owner: equivalent although A is expired.
    a.store (other);
    a.store (pinned);
    VNodeWeak expected = pinned;
    verdict.require (a.compare_exchange_strong (expected, other),
                     "weak_owner_identity: the same expired owner must match");
    VNodePtr const now = a.load ().lock ();
    verdict.require (now && now->version == 2,
                     "weak_owner_identity: wrong value stored");

    // A -> B -> A' where A' is another expired owner: not equivalent.
    VNodePtr third = lumex_test::make_pooled<VNode> (pool, ledger, 3);
    VNodeWeak const expired_other (third);
    third.reset ();
    a.store (pinned);
    a.store (expired_other);
    VNodeWeak stale = pinned;
    verdict.require (!a.compare_exchange_strong (stale, other),
                     "weak_owner_identity: two different expired owners "
                     "compared equal");
    verdict.require (!stale.owner_before (expired_other)
                         && !expired_other.owner_before (stale),
                     "weak_owner_identity: expected not refreshed");
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "weak_owner_identity");
}

/// Returns the memory of a node to a pool after destroying it.
struct PoolObjectDeleter
{
  lumex_test::ReusePool *pool;

  void
  operator() (VNode *node) const
  {
    node->~VNode ();
    pool->release (node, sizeof (VNode));
  }
};

/**
 * @brief The object of a weakly held node dies and a new object takes its
 * ADDRESS, while the control block of the old one stays pinned: the two
 * weak pointers share the object address and not the owner, so they are not
 * equivalent.
 * @details This is the address reuse that a weak pointer cannot prevent by
 * itself: object and control block are separate allocations, the weak
 * reference pins only the control block. The stale weak pointer is expired
 * (its stored pointer is unobservable), the new one is live at the very same
 * address. A compare-exchange from the stale one against the new one must
 * fail and refresh `expected`; against itself (the old owner stored again)
 * it must succeed.
 */
template <typename Engine>
void
weak_split_allocation_identity (lumex_test::Verdict &verdict)
{
  typedef typename Engine::template weak<VNode> weak_atom;
  lumex_test::ObjectLedger ledger (32);
  lumex_test::ReusePool object_pool;
  lumex_test::ReusePool control_pool;
  {
    PoolObjectDeleter const deleter = { &object_pool };
    VNode *const first_object
        = new (object_pool.allocate (sizeof (VNode))) VNode (ledger, 1);
    VNodePtr first (first_object, deleter,
                    lumex_test::ReuseAllocator<char> (control_pool));
    VNodeWeak const stale (first);
    weak_atom a (stale);
    first.reset (); // the object is destroyed, its control block is pinned
    VNode *const second_object
        = new (object_pool.allocate (sizeof (VNode))) VNode (ledger, 2);
    verdict.require (second_object == first_object,
                     "weak_split_allocation_identity: the object address was "
                     "not reused; the check proves nothing");
    VNodePtr second (second_object, deleter,
                     lumex_test::ReuseAllocator<char> (control_pool));
    VNodeWeak const fresh (second);
    a.store (fresh);
    VNodeWeak expected = stale;
    verdict.require (!a.compare_exchange_strong (expected, stale),
                     "weak_split_allocation_identity: a weak pointer to a "
                     "dead object matched a new object at the same address");
    verdict.require (!expected.owner_before (fresh)
                         && !fresh.owner_before (expected),
                     "weak_split_allocation_identity: expected was not "
                     "refreshed with the stored weak pointer");
    VNodePtr const live = expected.lock ();
    verdict.require (live && live->version == 2,
                     "weak_split_allocation_identity: the refreshed pointer "
                     "does not lock to the new object");
    a.store (stale);
    VNodeWeak again = stale;
    verdict.require (a.compare_exchange_strong (again, fresh),
                     "weak_split_allocation_identity: the same expired owner "
                     "stored again must match");
  }
  Engine::quiesce ();
  verdict.require (ledger.balanced (),
                   "weak_split_allocation_identity: " + ledger.report ());
  verdict.require (object_pool.live () == 0 && control_pool.live () == 0,
                   "weak_split_allocation_identity: blocks leaked");
}

/**
 * @brief `weak.lock ()` races the last strong release.
 * @tparam WeakFirst The owner thread publishes the weak pointer before the
 * strong one (true) or after, from a loaded copy (false).
 * @details One thread creates nodes, publishes them through an atomic shared
 * and an atomic weak pointer, then drops the last strong reference. The
 * others `load ().lock ()` all the time: a promotion must give a live,
 * intact node that stays intact while held, or null; a pointer seen
 * expired must never promote again; a null promotion means expired. The
 * ledger proves every node died once.
 */
template <typename Engine, bool WeakFirst>
void
promotion_race (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template shared<VNode> shared_atom;
  typedef typename Engine::template weak<VNode> weak_atom;
  lumex_test::ObjectLedger ledger (node_capacity (p, 1) + 8);
  lumex_test::ReusePool pool;
  std::atomic<bool> done (false);
  std::atomic<int> published (0);
  {
    shared_atom strong;
    weak_atom weak;
    std::string const error = lumex_test::run_threads (
        p.threads + 1,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 8));
            if (t == 0)
              {
                for (int i = 1; i <= p.iterations; ++i)
                  {
                    VNodePtr node
                        = lumex_test::make_pooled<VNode> (pool, ledger, i);
                    if (WeakFirst)
                      {
                        weak.store (VNodeWeak (node));
                        strong.store (node);
                      }
                    else
                      {
                        strong.store (node);
                        VNodePtr const copy = strong.load ();
                        weak.store (VNodeWeak (copy));
                      }
                    node.reset ();
                    published.store (i);
                    schedule.point ();
                    strong.store (VNodePtr ()); // the last strong release
                  }
                done.store (true);
                return;
              }
            while (!done.load ())
              {
                VNodeWeak const w = weak.load ();
                bool const was_expired = w.expired ();
                VNodePtr const held = w.lock ();
                if (held)
                  {
                    if (!check_node (held, verdict, "promotion_race"))
                      return;
                    if (held->version < 1 || held->version > p.iterations)
                      verdict.fail ("promotion_race: a promoted node has a "
                                    "version that was never published");
                    schedule.point ();
                    if (!held->intact ())
                      verdict.fail ("promotion_race: a promoted node was "
                                    "destroyed while it was held");
                  }
                else if (!w.expired ())
                  verdict.fail (
                      "promotion_race: lock () returned null but the "
                      "pointer is not expired");
                if (was_expired && w.lock ())
                  verdict.fail ("promotion_race: an expired weak pointer "
                                "promoted later");
                if (schedule.random ().below (4u) == 0u)
                  {
                    // Hold a strong copy from the atomic to lengthen the
                    // last release race.
                    VNodePtr const copy = strong.load ();
                    if (copy && !copy->intact ())
                      verdict.fail ("promotion_race: strong load returned a "
                                    "destroyed node");
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "promotion_race");
}

/**
 * @brief `expired ()` and `use_count ()` are monotone: once a weak pointer is
 * seen expired it stays expired and never promotes.
 * @details Observers promote the pointer while the owner holds the object (the
 * race with the owner letting go is the point); once the owner has let go
 * they only poll `expired ()`, because observers that kept re-promoting would
 * keep the object alive for as long as they overlap and the expiry would
 * never come. After it came, every observer checks that no promotion works
 * any more.
 */
template <typename Engine>
void
expired_race (Params const &p, lumex_test::Verdict &verdict)
{
  typedef typename Engine::template weak<VNode> weak_atom;
  int const rounds = std::max (1, p.iterations / 10);
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (rounds) + 8);
  lumex_test::ReusePool pool;
  for (int round = 0; round < rounds; ++round)
    {
      VNodePtr owner = lumex_test::make_pooled<VNode> (pool, ledger, round);
      weak_atom w ((VNodeWeak (owner)));
      std::atomic<bool> released (false);
      std::atomic<int> expired_seen (0);
      std::string const error = lumex_test::run_threads (
          p.threads + 1,
          [&] (int t)
            {
              lumex_test::Schedule schedule (
                  p.schedule,
                  lumex_test::derive_seed (
                      p.seed, static_cast<std::uint64_t> (round * 64 + t), 9));
              if (t == 0)
                {
                  schedule.point ();
                  owner.reset ();
                  released.store (true);
                  return;
                }
              while (!released.load ())
                {
                  VNodeWeak const copy = w.load ();
                  VNodePtr const held = copy.lock ();
                  if (held && !held->intact ())
                    verdict.fail ("expired_race: promoted a dead node");
                  if (!held && !copy.expired ())
                    verdict.fail ("expired_race: expired () disagrees with a "
                                  "null lock ()");
                  schedule.point ();
                }
              for (;;)
                {
                  VNodeWeak const copy = w.load ();
                  if (copy.expired ())
                    break;
                  std::this_thread::yield ();
                }
              expired_seen.fetch_add (1);
              for (int again = 0; again < 3; ++again)
                {
                  VNodeWeak const copy = w.load ();
                  if (copy.lock () || !copy.expired ()
                      || copy.use_count () != 0)
                    verdict.fail ("expired_race: an expired pointer came back "
                                  "to life");
                }
            });
      verdict.require (error.empty (), error);
      verdict.require (expired_seen.load () == p.threads,
                       "expired_race: not every observer saw the expiry");
    }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "expired_race");
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_WEAK_HPP

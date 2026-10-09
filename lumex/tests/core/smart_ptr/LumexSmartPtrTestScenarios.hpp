/**
 * @file LumexSmartPtrTestScenarios.hpp
 * @brief The concurrent scenarios of the smart pointer tests, written once
 * as checkers that the stress tests, the ABA tests and the soak run share.
 * @details A checker takes the thread count, the schedule kind, the number
 * of iterations and a seed (`Params`) and writes every broken invariant to a
 * `lumex_test::Verdict`; it never asserts inside a worker thread. The
 * scenarios:
 *
 * - `copy_release`: every thread copies and drops a shared object in a loop
 *   and reads it under the copy, so a count lost or double dropped destroys
 *   the object early (the canary of the ledger entry shows it) or leaks it.
 * - `last_release_race`: all threads drop their copy of a fresh object at
 *   the same moment; the object is destroyed exactly once.
 * - `weak_lock_vs_release`: weak pointers are promoted while the last owners
 *   drop; a promoted pointer always sees an intact object, and once
 *   `expired ()` was true no later `lock ()` succeeds.
 * - `block_lifetime`: weak pointers are copied across threads against a
 *   counting allocator; the single allocation is freed exactly once, after
 *   the last weak pointer.
 * - `esft_concurrent`: `shared_from_this ()` from many threads.
 * - `handoff`: producers create objects and consumers on other threads drop
 *   the last reference.
 * - `mailbox_churn`: threads swap fresh objects into shared mailboxes and
 *   promote weak pointers to the replaced ones.
 * - `alias_and_convert`: conversions, aliases, casts and weak pointers of
 *   one object from many threads.
 * - `address_reuse_generations`: control blocks and objects come from a pool
 *   that hands the most recently freed address back first; weak pointers
 *   remember the generation they were made for, and a successful `lock ()`
 *   must return that generation. The block cannot be reused while a weak
 *   pointer holds it, so a stale weak pointer never sees a new object.
 */
#ifndef LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SCENARIOS_HPP
#define LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SCENARIOS_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"
#include "lumex/tests/support/LumexTestConfig.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

namespace smart_ptr_test
{
namespace scenario
{
/// What a checker runs with.
struct Params
{
  int threads;
  lumex_test::ScheduleKind schedule;
  int iterations;
  std::uint64_t seed;
};

typedef void (*Checker) (Params const &, lumex_test::Verdict &);

/// The probe of the address-reuse scenarios: the generation it was made for.
class Generation
{
public:
  explicit Generation (std::uint64_t generation)
      : id (generation), canary_ (lumex_test::alive_canary)
  {
  }
  ~Generation () { canary_ = lumex_test::dead_canary; }
  bool
  intact () const
  {
    return canary_ == lumex_test::alive_canary;
  }
  std::uint64_t const id;

private:
  volatile std::uint32_t canary_;
};

inline void
join_or_fail (std::string const &error, lumex_test::Verdict &verdict)
{
  if (!error.empty ())
    verdict.fail (error);
}

inline lumex_test::Schedule
schedule_of (Params const &p, int index)
{
  return lumex_test::Schedule (
      p.schedule,
      lumex_test::derive_seed (p.seed, static_cast<std::uint64_t> (index)));
}

// --- copy_release
// --------------------------------------------------------------

inline void
copy_release (Params const &p, lumex_test::Verdict &verdict)
{
  lumex_test::ObjectLedger ledger (1);
  {
    sp::shared_ptr<Probe> origin = sp::make_shared<Probe> (ledger, 7);
    join_or_fail (lumex_test::run_threads (
                      p.threads,
                      [&] (int index)
                        {
                          lumex_test::Schedule schedule
                              = schedule_of (p, index);
                          sp::shared_ptr<Probe> mine
                              = origin; // concurrent reads of origin
                          for (int i = 0; i < p.iterations; ++i)
                            {
                              sp::shared_ptr<Probe> a = mine;
                              sp::shared_ptr<Probe> b (a);
                              verdict.require (a->intact () && b->value == 7,
                                               "object damaged under a copy");
                              schedule.point ();
                              a.reset ();
                              sp::shared_ptr<Probe> c = std::move (b);
                              verdict.require (c->intact (),
                                               "object damaged after a move");
                              if (schedule.random ().chance (10))
                                {
                                  mine.reset ();
                                  mine = c;
                                }
                            }
                        }),
                  verdict);
    verdict.require (origin.use_count () == 1,
                     "use_count after the join is "
                         + std::to_string (origin.use_count ()));
  }
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- last_release_race
// ----------------------------------------------------------

inline void
last_release_race (Params const &p, lumex_test::Verdict &verdict)
{
  int const rounds = std::max (1, p.iterations / 20);
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (rounds));
  lumex_test::StartBarrier phase (p.threads);
  std::vector<sp::shared_ptr<Probe>> copies (
      static_cast<std::size_t> (p.threads));
  std::atomic<int> alive_after (0);
  join_or_fail (
      lumex_test::run_threads (
          p.threads,
          [&] (int index)
            {
              lumex_test::Schedule schedule = schedule_of (p, index);
              for (int round = 0; round < rounds; ++round)
                {
                  if (index == 0)
                    {
                      sp::shared_ptr<Probe> object
                          = sp::make_shared<Probe> (ledger, round);
                      for (std::size_t i = 0; i < copies.size (); ++i)
                        copies[i] = object;
                    }
                  phase.arrive_and_wait ();
                  schedule.point ();
                  verdict.require (
                      copies[static_cast<std::size_t> (index)]->intact (),
                      "damaged before the drop");
                  copies[static_cast<std::size_t> (index)].reset ();
                  phase.arrive_and_wait ();
                  if (index == 0)
                    alive_after.store (static_cast<int> (ledger.alive ()));
                  phase.arrive_and_wait ();
                  verdict.require (alive_after.load () == 0,
                                   "an object survived the last release");
                }
            }),
      verdict);
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- weak_lock_vs_release
// ---------------------------------------------------------

inline void
weak_lock_vs_release (Params const &p, lumex_test::Verdict &verdict)
{
  int const rounds = std::max (1, p.iterations / 20);
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (rounds));
  lumex_test::StartBarrier phase (p.threads);
  sp::shared_ptr<Probe> owner;
  sp::shared_ptr<Probe> second_owner;
  sp::weak_ptr<Probe> weak;
  std::atomic<long> promotions (0);
  join_or_fail (
      lumex_test::run_threads (
          p.threads,
          [&] (int index)
            {
              lumex_test::Schedule schedule = schedule_of (p, index);
              for (int round = 0; round < rounds; ++round)
                {
                  if (index == 0)
                    {
                      owner = sp::make_shared<Probe> (ledger, round);
                      second_owner = owner;
                      weak = owner;
                    }
                  phase.arrive_and_wait ();
                  if (index == 0)
                    {
                      schedule.point ();
                      owner.reset ();
                    }
                  else if (index == 1 && p.threads > 2)
                    {
                      schedule.point ();
                      second_owner.reset ();
                    }
                  else
                    {
                      bool saw_expired = false;
                      for (int i = 0; i < 64; ++i)
                        {
                          sp::weak_ptr<Probe> local (weak);
                          sp::shared_ptr<Probe> locked = local.lock ();
                          if (locked)
                            {
                              verdict.require (locked->intact (),
                                               "promoted a destroyed object");
                              verdict.require (!saw_expired,
                                               "lock () succeeded after "
                                               "expired () was true");
                              promotions.fetch_add (1,
                                                    std::memory_order_relaxed);
                            }
                          else
                            verdict.require (
                                local.expired () || local.use_count () == 0,
                                "lock () failed on a live object");
                          if (local.expired ())
                            saw_expired = true;
                          schedule.point ();
                        }
                    }
                  phase.arrive_and_wait ();
                  if (index == 0)
                    {
                      second_owner.reset ();
                      verdict.require (weak.expired (),
                                       "the object outlived every owner");
                      weak.reset ();
                    }
                  phase.arrive_and_wait ();
                }
            }),
      verdict);
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- block_lifetime
// ------------------------------------------------------------------

inline void
block_lifetime (Params const &p, lumex_test::Verdict &verdict)
{
  int const rounds = std::max (1, p.iterations / 20);
  for (int round = 0; round < rounds; ++round)
    {
      AllocStats stats;
      lumex_test::ObjectLedger ledger (1);
      {
        sp::shared_ptr<Probe> owner = sp::allocate_shared<Probe> (
            CountingAllocator<Probe> (&stats), std::ref (ledger), round);
        std::vector<sp::weak_ptr<Probe>> weaks (
            static_cast<std::size_t> (p.threads), sp::weak_ptr<Probe> (owner));
        lumex_test::StartBarrier start (p.threads + 1);
        std::thread dropper (
            [&]
              {
                start.arrive_and_wait ();
                owner.reset ();
              });
        join_or_fail (
            lumex_test::run_threads (
                p.threads,
                [&] (int index)
                  {
                    lumex_test::Schedule schedule
                        = schedule_of (p, index + round * 100);
                    start.arrive_and_wait ();
                    sp::weak_ptr<Probe> mine (
                        weaks[static_cast<std::size_t> (index)]);
                    weaks[static_cast<std::size_t> (index)].reset ();
                    for (int i = 0; i < 16; ++i)
                      {
                        sp::weak_ptr<Probe> copy (mine);
                        if (sp::shared_ptr<Probe> locked = copy.lock ())
                          verdict.require (locked->intact (),
                                           "damaged under a promotion");
                        schedule.point ();
                      }
                  }),
            verdict);
        dropper.join ();
        verdict.require (ledger.balanced (), ledger.report ());
        verdict.require (stats.destroys.load () == 1,
                         "the object was destroyed "
                             + std::to_string (stats.destroys.load ())
                             + " times");
      }
      verdict.require (
          stats.allocations.load () == 1 && stats.deallocations.load () == 1,
          "the block was freed " + std::to_string (stats.deallocations.load ())
              + " times of " + std::to_string (stats.allocations.load ()));
    }
}

// --- esft_concurrent
// ------------------------------------------------------------------

class SharedNode : public sp::enable_shared_from_this<SharedNode>
{
public:
  explicit SharedNode (lumex_test::ObjectLedger &ledger) : entry_ (ledger) {}
  bool
  intact () const
  {
    return entry_.intact ();
  }

private:
  lumex_test::LedgerEntry entry_;
};

inline void
esft_concurrent (Params const &p, lumex_test::Verdict &verdict)
{
  lumex_test::ObjectLedger ledger (1);
  {
    sp::shared_ptr<SharedNode> node = sp::make_shared<SharedNode> (ledger);
    join_or_fail (
        lumex_test::run_threads (
            p.threads,
            [&] (int index)
              {
                lumex_test::Schedule schedule = schedule_of (p, index);
                for (int i = 0; i < p.iterations; ++i)
                  {
                    sp::shared_ptr<SharedNode> self
                        = node->shared_from_this ();
                    verdict.require (
                        self.get () == node.get () && self->intact (),
                        "shared_from_this () returned another object");
                    sp::weak_ptr<SharedNode> weak = node->weak_from_this ();
                    if (sp::shared_ptr<SharedNode> locked = weak.lock ())
                      verdict.require (
                          locked == node,
                          "weak_from_this () locked another object");
                    schedule.point ();
                  }
              }),
        verdict);
    verdict.require (node.use_count () == 1,
                     "use_count after the join is "
                         + std::to_string (node.use_count ()));
  }
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- handoff
// --------------------------------------------------------------------------

inline void
handoff (Params const &p, lumex_test::Verdict &verdict)
{
  int const producers = std::max (1, p.threads / 2);
  int const consumers = std::max (1, p.threads - producers);
  int const per_producer = std::max (4, p.iterations / 4);
  lumex_test::ObjectLedger ledger (
      static_cast<std::size_t> (producers * per_producer));
  std::mutex mutex;
  std::deque<sp::shared_ptr<Probe>> queue;
  // Every object is queued twice (two owners that end on other threads).
  std::atomic<int> remaining (producers * per_producer * 2);
  join_or_fail (
      lumex_test::run_threads (
          producers + consumers,
          [&] (int index)
            {
              lumex_test::Schedule schedule = schedule_of (p, index);
              if (index < producers)
                {
                  for (int i = 0; i < per_producer; ++i)
                    {
                      sp::shared_ptr<Probe> made
                          = sp::make_shared<Probe> (ledger, i);
                      sp::shared_ptr<Probe> extra = made;
                      {
                        std::lock_guard<std::mutex> guard (mutex);
                        queue.push_back (made);
                        queue.push_back (extra);
                      }
                      schedule.point ();
                    }
                }
              else
                {
                  while (remaining.load () > 0)
                    {
                      sp::shared_ptr<Probe> taken;
                      {
                        std::lock_guard<std::mutex> guard (mutex);
                        if (!queue.empty ())
                          {
                            taken = queue.front ();
                            queue.pop_front ();
                          }
                      }
                      if (!taken)
                        {
                          std::this_thread::yield ();
                          continue;
                        }
                      verdict.require (taken->intact (),
                                       "a handed-off object is damaged");
                      sp::weak_ptr<Probe> watcher = taken;
                      taken.reset ();
                      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (watcher.expired ());
                      remaining.fetch_sub (1);
                    }
                }
            }),
      verdict);
  verdict.require (queue.empty (), "objects were left in the queue");
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- mailbox_churn
// --------------------------------------------------------------------

inline void
mailbox_churn (Params const &p, lumex_test::Verdict &verdict)
{
  int const boxes = std::max (2, p.threads);
  std::size_t const capacity = static_cast<std::size_t> (p.threads)
                                   * static_cast<std::size_t> (p.iterations)
                               + static_cast<std::size_t> (boxes) + 8;
  lumex_test::ObjectLedger ledger (capacity);
  struct Box
  {
    std::mutex mutex;
    sp::shared_ptr<Probe> content;
  };
  std::vector<Box> mail (static_cast<std::size_t> (boxes));
  for (std::size_t i = 0; i < mail.size (); ++i)
    mail[i].content = sp::make_shared<Probe> (ledger, 0);
  join_or_fail (
      lumex_test::run_threads (
          p.threads,
          [&] (int index)
            {
              lumex_test::Schedule schedule = schedule_of (p, index);
              std::vector<sp::weak_ptr<Probe>> old_ones;
              for (int i = 0; i < p.iterations; ++i)
                {
                  Box &box = mail[schedule.random ().below (
                      static_cast<std::uint32_t> (boxes))];
                  sp::shared_ptr<Probe> fresh
                      = sp::make_shared<Probe> (ledger, i);
                  sp::shared_ptr<Probe> replaced;
                  {
                    std::lock_guard<std::mutex> guard (box.mutex);
                    replaced = box.content;
                    box.content = fresh;
                  }
                  old_ones.push_back (replaced);
                  replaced.reset ();
                  schedule.point ();
                  if (old_ones.size () > 6)
                    {
                      for (std::size_t k = 0; k < old_ones.size (); ++k)
                        if (sp::shared_ptr<Probe> again = old_ones[k].lock ())
                          verdict.require (
                              again->intact (),
                              "a promoted replaced object is damaged");
                      old_ones.clear ();
                    }
                }
            }),
      verdict);
  for (std::size_t i = 0; i < mail.size (); ++i)
    mail[i].content.reset ();
  verdict.require (ledger.balanced (), ledger.report ());
}

// --- alias_and_convert
// ----------------------------------------------------------------

inline void
alias_and_convert (Params const &p, lumex_test::Verdict &verdict)
{
  std::atomic<int> live (0);
  {
    sp::shared_ptr<Both> both = sp::make_shared<Both> (&live);
    join_or_fail (
        lumex_test::run_threads (
            p.threads,
            [&] (int index)
              {
                lumex_test::Schedule schedule = schedule_of (p, index);
                for (int i = 0; i < p.iterations; ++i)
                  {
                    sp::shared_ptr<Right> right = both;
                    sp::shared_ptr<Left> left
                        = sp::static_pointer_cast<Left> (both);
                    sp::shared_ptr<long> alias (right,
                                                &both->right_payload[1]);
                    sp::shared_ptr<Both> back
                        = sp::dynamic_pointer_cast<Both> (right);
                    sp::weak_ptr<Right> weak = right;
                    verdict.require (*alias == 5 && back == both,
                                     "a conversion changed the object");
                    verdict.require (left.get ()
                                         == static_cast<Left *> (both.get ()),
                                     "wrong Left pointer");
                    schedule.point ();
                    if (sp::shared_ptr<Right> locked = weak.lock ())
                      verdict.require (
                          locked == right,
                          "weak promotion returned another pointer");
                  }
              }),
        verdict);
    verdict.require (both.use_count () == 1,
                     "use_count after the join is "
                         + std::to_string (both.use_count ()));
  }
  verdict.require (live.load () == 0, "the object was not destroyed");
}

// --- address_reuse_generations
// --------------------------------------------------------

/// The scenario of the ABA tests (see the file text). @p unsafe_observers
/// replaces the weak pointers with raw pointers that do not keep the block:
/// the checker must then report stale reads, which proves that it sees
/// address reuse.
inline void
address_reuse_run (Params const &p, lumex_test::Verdict &verdict,
                   bool unsafe_observers)
{
  lumex_test::ReusePool pool;
  if (unsafe_observers)
    pool.set_poisoning (false);
  int const slots = std::max (2, p.threads);
  struct Slot
  {
    std::mutex mutex;
    sp::shared_ptr<Generation> current;
    sp::weak_ptr<Generation> weak;
    std::uint64_t generation = 0;
    Generation *raw = nullptr;
  };
  std::vector<Slot> table (static_cast<std::size_t> (slots));
  std::atomic<std::uint64_t> next_generation (1);
  std::atomic<long> successful_locks (0);
  std::atomic<long> stale_reads (0);
  auto make_generation = [&] (std::uint64_t id)
    {
      return sp::allocate_shared<Generation> (
          lumex_test::ReuseAllocator<Generation> (pool), id);
    };
  join_or_fail (
      lumex_test::run_threads (
          p.threads,
          [&] (int index)
            {
              lumex_test::Schedule schedule = schedule_of (p, index);
              std::vector<std::pair<sp::weak_ptr<Generation>, std::uint64_t>>
                  kept;
              std::vector<std::pair<Generation *, std::uint64_t>> kept_raw;
              for (int i = 0; i < p.iterations; ++i)
                {
                  Slot &slot = table[schedule.random ().below (
                      static_cast<std::uint32_t> (slots))];
                  if (index % 2 == 0)
                    {
                      // Publisher: a new generation replaces the old one.
                      std::uint64_t const id = next_generation.fetch_add (1);
                      sp::shared_ptr<Generation> fresh = make_generation (id);
                      sp::shared_ptr<Generation> old;
                      {
                        std::lock_guard<std::mutex> guard (slot.mutex);
                        old = std::move (slot.current);
                        slot.current = fresh;
                        slot.weak = fresh;
                        slot.generation = id;
                        slot.raw = fresh.get ();
                      }
                      old.reset ();
                    }
                  else
                    {
                      // Observer: remembers the generation, promotes later.
                      sp::weak_ptr<Generation> weak;
                      std::uint64_t id = 0;
                      Generation *raw = nullptr;
                      {
                        std::lock_guard<std::mutex> guard (slot.mutex);
                        weak = slot.weak;
                        id = slot.generation;
                        raw = slot.raw;
                      }
                      if (unsafe_observers)
                        kept_raw.push_back (std::make_pair (raw, id));
                      else
                        kept.push_back (std::make_pair (weak, id));
                      schedule.point ();
                      if (unsafe_observers)
                        {
                          if (kept_raw.size () > 4)
                            {
                              for (std::size_t k = 0; k < kept_raw.size ();
                                   ++k)
                                if (kept_raw[k].first != nullptr
                                    && kept_raw[k].first->id
                                           != kept_raw[k].second)
                                  stale_reads.fetch_add (1);
                              kept_raw.clear ();
                            }
                        }
                      else if (kept.size () > 4)
                        {
                          for (std::size_t k = 0; k < kept.size (); ++k)
                            if (sp::shared_ptr<Generation> locked
                                = kept[k].first.lock ())
                              {
                                successful_locks.fetch_add (1);
                                verdict.require (
                                    locked->intact ()
                                        && locked->id == kept[k].second,
                                    "a weak pointer promoted another "
                                    "generation (address reuse)");
                              }
                          kept.clear ();
                        }
                    }
                }
            }),
      verdict);
  for (std::size_t i = 0; i < table.size (); ++i)
    {
      table[i].current.reset ();
      table[i].weak.reset ();
    }
  if (unsafe_observers)
    {
      if (stale_reads.load () == 0)
        verdict.fail ("the unsafe observers saw no stale read: the harness "
                      "cannot see address reuse");
    }
  else
    verdict.require (pool.live () == 0,
                     "blocks leaked: " + std::to_string (pool.live ()));
}

/// The checker form: observers hold weak pointers (must see no stale read).
inline void
address_reuse_generations (Params const &p, lumex_test::Verdict &verdict)
{
  address_reuse_run (p, verdict, false);
}

/// The negative control: observers hold raw pointers (must see stale reads).
inline void
address_reuse_unsafe_control (Params const &p, lumex_test::Verdict &verdict)
{
  address_reuse_run (p, verdict, true);
}
} // namespace scenario
} // namespace smart_ptr_test

#endif // !LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SCENARIOS_HPP

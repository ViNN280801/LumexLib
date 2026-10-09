// Tests of reclaim_or_retire, the extension of the module that scans the
// hazard slots for ONE address and reclaims the object on the spot when nobody
// names it, else retires it (engine::reclaim_or_retire and the member of
// hazard_pointer_obj_base). The lock-free atomic smart pointers use it to
// destroy a replaced value inside the replacing call.
//
// The unit tests drive the member and the engine function; the ABA tests run
// the scripted interleaving over the immediate-reuse allocator of the module's
// tests (a freed address is allocated again by the very next create),
// protected and unprotected: the protected run must keep the register right,
// the unprotected one must be damaged, which proves the script can see ABA.
// The stress tests run readers against a writer that replaces the shared
// object and reclaims the old one through the function, with a poisoned
// payload.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStress.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStructures.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
using lumex_hp_test::poisoned_node;
namespace hp = lumex::core::hazard_pointer;
namespace engine = lumex::core::hazard_pointer::engine;
using namespace lumex_hp_structures;

// An object whose retire node is not at offset zero: the slot holds the
// address of the node, not of the object.
struct padding
{
  long filler[3];
};

struct offset_node : padding, hp::hazard_pointer_obj_base<offset_node>
{
  explicit offset_node (std::atomic<int> &the_deleted) : deleted (&the_deleted)
  {
  }

  ~offset_node () { deleted->fetch_add (1); }

  std::atomic<int> *deleted;
};

// Counts the deleter calls and the thread they ran on.
struct counting_deleter
{
  template <class T>
  void
  operator() (T *p) const
  {
    calls ().fetch_add (1);
    delete p;
  }

  static std::atomic<int> &
  calls ()
  {
    static std::atomic<int> value (0);
    return value;
  }
};

struct counted_by_deleter
    : hp::hazard_pointer_obj_base<counted_by_deleter, counting_deleter>
{
};

// A deleter that reenters the module: it makes a holder, reclaims or retires
// another object and runs a pass.
struct chain_node;

struct chain_deleter
{
  void operator() (chain_node *p) const;
};

struct chain_node : hp::hazard_pointer_obj_base<chain_node, chain_deleter>
{
  chain_node (std::atomic<int> &the_deleted, int the_depth)
      : deleted (&the_deleted), depth (the_depth)
  {
  }

  std::atomic<int> *deleted;
  int depth;
};

void
chain_deleter::operator() (chain_node *p) const
{
  std::atomic<int> *const deleted = p->deleted;
  int const depth = p->depth;
  delete p;
  deleted->fetch_add (1);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  if (depth > 0)
    {
      (new chain_node (*deleted, depth - 1))->reclaim_or_retire ();
    }
  hp::clean_up ();
}
} // namespace

// --- unit tests -------------------------------------------------------------

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenAnObjectNobodyProtects_WhenReclaimOrRetire_ThenItIsDeletedBeforeTheCallReturns)
{
  counters_t counters;
  counted_node *node = new counted_node (counters, 1);
  engine::statistics_t const before = engine::statistics ();
  bool const now = node->reclaim_or_retire ();
  EXPECT_TRUE (now);
  EXPECT_EQ (counters.deleted.load (), 1) << "deleted inside the call";
  engine::statistics_t const after = engine::statistics ();
  EXPECT_EQ (after.retired - before.retired, 1u);
  EXPECT_EQ (after.reclaimed - before.reclaimed, 1u);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenAProtectedObject_WhenReclaimOrRetire_ThenItIsRetiredAndDeletedAfterTheProtectionEnds)
{
  counters_t counters;
  std::atomic<counted_node *> source (new counted_node (counters, 2));
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *const protected_node = holder.protect (source);
  source.store (nullptr);
  bool const now = protected_node->reclaim_or_retire ();
  EXPECT_FALSE (now) << "a slot names it";
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0) << "still protected";
  EXPECT_EQ (protected_node->value, 2) << "not poisoned";
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenTheCallersOwnProtection_WhenReclaimOrRetire_ThenItIsRetiredNotReclaimed)
{
  // The documented precondition: a caller that still holds the object keeps it
  // alive and sends it down the retire path.
  counters_t counters;
  std::atomic<counted_node *> source (new counted_node (counters, 3));
  hp::hazard_pointer own = hp::make_hazard_pointer ();
  counted_node *const node = own.protect (source);
  source.store (nullptr);
  EXPECT_FALSE (node->reclaim_or_retire ());
  EXPECT_EQ (counters.deleted.load (), 0);
  own.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenAnotherObjectProtected_WhenReclaimOrRetire_ThenOnlyTheNamedAddressCounts)
{
  counters_t counters;
  std::atomic<counted_node *> other_source (new counted_node (counters, 10));
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *const other = holder.protect (other_source);
  counted_node *mine = new counted_node (counters, 11);
  EXPECT_TRUE (mine->reclaim_or_retire ());
  EXPECT_EQ (counters.deleted.load (), 1);
  holder.reset_protection ();
  other_source.store (nullptr);
  other->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenTheNodeAtANonzeroOffset_WhenProtectedAndReclaimOrRetire_ThenTheSlotMatchesTheNode)
{
  std::atomic<int> deleted (0);
  std::atomic<offset_node *> source (new offset_node (deleted));
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  offset_node *const node = holder.protect (source);
  source.store (nullptr);
  EXPECT_FALSE (node->reclaim_or_retire ());
  hp::clean_up ();
  EXPECT_EQ (deleted.load (), 0);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (deleted.load (), 1);
  EXPECT_TRUE ((new offset_node (deleted))->reclaim_or_retire ());
  EXPECT_EQ (deleted.load (), 2);
}

TEST (LumexHazardPointerReclaimOrRetireTest,
      GivenACustomDeleter_WhenReclaimOrRetire_ThenItRunsOnTheCallingThread)
{
  counting_deleter::calls ().store (0);
  counted_by_deleter *node = new counted_by_deleter;
  EXPECT_TRUE (node->reclaim_or_retire ());
  EXPECT_EQ (counting_deleter::calls ().load (), 1);
  counted_by_deleter *kept = new counted_by_deleter;
  std::atomic<counted_by_deleter *> source (kept);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_by_deleter *const held = holder.protect (source);
  source.store (nullptr);
  EXPECT_FALSE (held->reclaim_or_retire (counting_deleter ()));
  EXPECT_EQ (counting_deleter::calls ().load (), 1);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counting_deleter::calls ().load (), 2);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenADeleterThatReentersTheModule_WhenReclaimOrRetire_ThenTheChainEndsWithoutDeadlock)
{
  std::atomic<int> deleted (0);
  (new chain_node (deleted, 5))->reclaim_or_retire ();
  hp::clean_up ();
  EXPECT_EQ (deleted.load (), 6);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenAnotherThread_WhenItCallsReclaimOrRetire_ThenTheDeleterRunsOnThatThread)
{
  std::thread::id deleter_thread;
  std::thread::id caller_thread;
  struct recording_node : hp::hazard_pointer_obj_base<recording_node>
  {
    explicit recording_node (std::thread::id &the_where) : where (&the_where)
    {
    }
    ~recording_node () { *where = std::this_thread::get_id (); }
    std::thread::id *where;
  };
  std::thread worker (
      [&]
        {
          caller_thread = std::this_thread::get_id ();
          EXPECT_TRUE (
              (new recording_node (deleter_thread))->reclaim_or_retire ());
        });
  worker.join ();
  EXPECT_EQ (deleter_thread, caller_thread);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    GivenManyScansWithoutProtection_WhenCounted_ThenRetiredMinusReclaimedStaysZero)
{
  hp::clean_up ();
  engine::statistics_t const before = engine::statistics ();
  counters_t counters;
  for (int i = 0; i < 1000; ++i)
    {
      EXPECT_TRUE ((new counted_node (counters, i))->reclaim_or_retire ());
    }
  engine::statistics_t const after = engine::statistics ();
  EXPECT_EQ (counters.deleted.load (), 1000);
  EXPECT_EQ (after.retired - before.retired, 1000u);
  EXPECT_EQ (after.reclaimed - before.reclaimed, 1000u);
}

// --- ABA: a scripted interleaving over the immediate-reuse allocator --------

namespace
{
// A register that holds one node. The reader reads the node (protected or
// not), compares its content and is about to replace it only if it still
// holds what it read; the hook runs in the window between the read and the
// compare-and-swap. The writer in the hook replaces the node and gives the old
// one back through reclaim_or_retire (protected policy) or frees it at once
// (unprotected), then allocates a new node, which lands in the same block when
// the old block was freed.
struct reg_node : hp::hazard_pointer_obj_base<reg_node, pool_deleter<reg_node>>
{
  explicit reg_node (long the_value) : value (the_value) {}
  long value;
};

struct aba_outcome
{
  bool address_reused;  // the new node got the old node's block
  bool swap_succeeded;  // the reader's compare-and-swap went through
  long final_value;     // what the register holds at the end
  long read_value;      // what the reader saw in the node it read
  long seen_after_swap; // content of the node the reader read, afterwards
};

template <bool Protected>
aba_outcome
run_register_script ()
{
  aba_outcome out = { false, false, 0, 0, 0 };
  recycling_pool<reg_node> pool;
  std::atomic<reg_node *> word (pool.create (1));
  void const *const a_address = word.load ();
  hp::hazard_pointer holder = hp::make_hazard_pointer ();

  // The reader.
  reg_node *seen = nullptr;
  if (Protected)
    {
      seen = holder.protect (word);
    }
  else
    {
      seen = word.load ();
    }
  out.read_value = seen->value;

  // The interference, in the window: A -> B, A given back, B -> A' (a new
  // node in A's block).
  reg_node *const b = pool.create (2);
  reg_node *const a = word.exchange (b);
  if (Protected)
    {
      // The reader still names A: the node must be retired, not reclaimed.
      EXPECT_FALSE (a->reclaim_or_retire (pool_deleter<reg_node> (&pool)));
    }
  else
    {
      pool.destroy (a);
    }
  reg_node *const a_again = pool.create (3);
  out.address_reused = static_cast<void const *> (a_again) == a_address;
  reg_node *const replaced_b = word.exchange (a_again);
  if (Protected)
    {
      // Nobody names B: it goes at once.
      EXPECT_TRUE (
          replaced_b->reclaim_or_retire (pool_deleter<reg_node> (&pool)));
    }
  else
    {
      pool.destroy (replaced_b);
    }

  // The reader's compare-and-swap, expecting the node it read.
  reg_node *expected = seen;
  reg_node *const replacement = pool.create (99);
  out.swap_succeeded = word.compare_exchange_strong (expected, replacement);
  out.final_value = word.load ()->value;
  // The content of the node the reader read: intact when protected.
  out.seen_after_swap = seen->value;
  holder.reset_protection ();
  if (out.swap_succeeded)
    {
      pool.destroy (a_again); // the node the swap replaced (unprotected run)
    }
  else
    {
      pool.destroy (replacement);
    }
  hp::clean_up ();
  reg_node *const last = word.exchange (nullptr);
  last->reclaim_or_retire (pool_deleter<reg_node> (&pool));
  hp::clean_up ();
  return out;
}
} // namespace

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    Aba_GivenTheReaderHoldsTheOldNode_WhenTheAddressWouldComeBack_ThenItDoesNotAndTheSwapFails)
{
  aba_outcome const out = run_register_script<true> ();
  EXPECT_FALSE (out.address_reused) << "the protected block was not reused";
  EXPECT_FALSE (out.swap_succeeded)
      << "the register changed behind the reader";
  EXPECT_EQ (out.read_value, 1);
  EXPECT_EQ (out.seen_after_swap, 1) << "the node it read is intact";
  EXPECT_EQ (out.final_value, 3);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    Aba_GivenNoProtection_WhenTheAddressComesBack_ThenTheScriptDamagesTheRegister)
{
  // The same script without the protection must go wrong, or the protected
  // run proves nothing.
  aba_outcome const out = run_register_script<false> ();
  EXPECT_TRUE (out.address_reused)
      << "the allocator did not hand the block back";
  EXPECT_TRUE (out.swap_succeeded)
      << "the stale compare-and-swap went through";
  EXPECT_NE (out.seen_after_swap, out.read_value)
      << "the node the reader read now holds another value";
}

// --- stress -----------------------------------------------------------------

namespace
{
// Readers protect the shared node, check its pair and repeat; writers replace
// it and reclaim the old node through the function, from several threads.
// Returns the number of broken pairs the readers saw.
long
run_replace_stress (unsigned readers, unsigned writers, unsigned writes,
                    std::uint64_t seed, long *reclaimed_now)
{
  std::atomic<poisoned_node *> shared (new poisoned_node (1));
  std::atomic<bool> stop (false);
  std::atomic<long> broken (0);
  std::atomic<long> now (0);
  std::atomic<unsigned> writers_left (writers);
  gate_t gate;
  std::vector<std::thread> threads;
  for (unsigned r = 0; r < readers; ++r)
    {
      threads.emplace_back (
          [&]
            {
              hp::hazard_pointer holder = hp::make_hazard_pointer ();
              gate.wait ();
              while (!stop.load ())
                {
                  poisoned_node *const node = holder.protect (shared);
                  if (!node->intact ())
                    {
                      broken.fetch_add (1);
                    }
                  holder.reset_protection ();
                }
            });
    }
  for (unsigned w = 0; w < writers; ++w)
    {
      threads.emplace_back (
          [&, w]
            {
              random_t generator (seed + w);
              gate.wait ();
              for (unsigned i = 0; i < writes; ++i)
                {
                  poisoned_node *const fresh = new poisoned_node (
                      (static_cast<std::uint64_t> (w) << 32) + i + 2);
                  poisoned_node *const old = shared.exchange (fresh);
                  if (old->reclaim_or_retire ())
                    {
                      now.fetch_add (1);
                    }
                  if (generator.below (16) == 0)
                    {
                      std::this_thread::yield ();
                    }
                }
              if (writers_left.fetch_sub (1) == 1)
                {
                  stop.store (true);
                }
            });
    }
  gate.open.store (true);
  for (std::size_t i = 0; i < threads.size (); ++i)
    {
      threads[i].join ();
    }
  poisoned_node *const last = shared.exchange (nullptr);
  last->reclaim_or_retire ();
  hp::clean_up ();
  if (reclaimed_now != nullptr)
    {
      *reclaimed_now = now.load ();
    }
  return broken.load ();
}
} // namespace

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    Stress_GivenReadersAndOneWriter_WhenTheWriterReclaimsThroughTheScan_ThenNoReaderSeesAFreedNode)
{
  std::uint64_t const seed = run_seed ();
  unsigned const writes = lumex_hp_test::stress_scale (20000);
  for (unsigned threads : thread_counts ())
    {
      long reclaimed_now = 0;
      long const broken
          = run_replace_stress (threads, 1, writes, seed, &reclaimed_now);
      EXPECT_EQ (broken, 0) << threads << " readers, seed " << seed;
      EXPECT_EQ (poisoned_node::alive ().load (), 0)
          << threads << " readers: every node was deleted exactly once";
      if (threads == 1)
        {
          EXPECT_GT (reclaimed_now, 0L)
              << "the scan reclaims at least some at once";
        }
    }
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    Stress_GivenManyWritersAndManyReaders_WhenTheyReplaceAndReclaim_ThenEveryNodeIsDeletedOnce)
{
  std::uint64_t const seed = run_seed ();
  unsigned const writes = lumex_hp_test::stress_scale (8000);
  unsigned const many = std::max (4u, lumex_hp_test::stress_threads ());
  long const broken
      = run_replace_stress (many, many, writes, seed + 7, nullptr);
  EXPECT_EQ (broken, 0) << "seed " << seed;
  EXPECT_EQ (poisoned_node::alive ().load (), 0);
}

TEST (
    LumexHazardPointerReclaimOrRetireTest,
    Stress_GivenThreadsThatComeAndGo_WhenWritersScanWhileCachesAreEvicted_ThenNothingIsLost)
{
  unsigned const rounds = lumex_hp_test::stress_scale (200);
  std::atomic<poisoned_node *> shared (new poisoned_node (1));
  std::atomic<long> broken (0);
  for (unsigned round = 0; round < rounds; ++round)
    {
      std::vector<std::thread> threads;
      for (unsigned t = 0; t < 4; ++t)
        {
          threads.emplace_back (
              [&, t]
                {
                  // Each thread makes a holder (taking a slot of its cache),
                  // reads and writes once, and exits: the cache is handed back
                  // while another thread scans.
                  hp::hazard_pointer holder = hp::make_hazard_pointer ();
                  poisoned_node *const node = holder.protect (shared);
                  if (!node->intact ())
                    {
                      broken.fetch_add (1);
                    }
                  holder.reset_protection ();
                  if (t % 2 == 0)
                    {
                      poisoned_node *const fresh = new poisoned_node (
                          static_cast<std::uint64_t> (round) * 8u + t + 2u);
                      shared.exchange (fresh)->reclaim_or_retire ();
                    }
                });
        }
      for (std::size_t i = 0; i < threads.size (); ++i)
        {
          threads[i].join ();
        }
    }
  shared.exchange (nullptr)->reclaim_or_retire ();
  hp::clean_up ();
  EXPECT_EQ (broken.load (), 0);
  EXPECT_EQ (poisoned_node::alive ().load (), 0);
}

// Tests of the engine behind hazard_pointer: the slot pool, the per-thread
// cache and its hand-back at thread exit, the retired lists and the
// reclamation passes. The observers are engine::statistics () and the
// retire/clean_up pair; "slots in use" is records - pooled_records, which
// returns to its starting value when every thread has given its slots back.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sys/mman.h>
#endif

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
namespace hp = lumex::core::hazard_pointer;
namespace engine = lumex::core::hazard_pointer::engine;

std::size_t
in_use ()
{
  engine::statistics_t const stats = engine::statistics ();
  return stats.records - stats.pooled_records;
}

// An object whose deleter retires the next one.
struct spawn_node;

struct spawn_deleter
{
  void operator() (spawn_node *p) const;
};

struct spawn_node : hp::hazard_pointer_obj_base<spawn_node, spawn_deleter>
{
  int *deleted;
  int depth;

  spawn_node (int *the_deleted, int the_depth)
      : deleted (the_deleted), depth (the_depth)
  {
  }
};

void
spawn_deleter::operator() (spawn_node *p) const
{
  int *deleted = p->deleted;
  int const depth = p->depth;
  delete p;
  ++*deleted;
  if (depth > 0)
    {
      (new spawn_node (deleted, depth - 1))->retire ();
    }
}

// Moves a holder to another thread.
struct holder_mover
{
  hp::hazard_pointer holder;

  explicit holder_mover (hp::hazard_pointer &&source)
      : holder (std::move (source))
  {
  }

  holder_mover (holder_mover &&other) : holder (std::move (other.holder)) {}

  void
  operator() ()
  {
    hp::hazard_pointer local (std::move (holder));
    EXPECT_FALSE (local.empty ());
  }
};

#if defined(__linux__)
// A bare retire node for tests that drive the engine directly.
struct fake_node
{
  engine::node_t node;
  std::atomic<int> *reclaimed;
};

void
reclaim_fake (engine::node_t *node)
{
  fake_node *fake = reinterpret_cast<fake_node *> (node);
  fake->reclaimed->fetch_add (1);
}

void
retire_fake (fake_node *fake, std::atomic<int> *counter)
{
  engine::init_node (fake->node);
  fake->node.reclaim = &reclaim_fake;
  fake->reclaimed = counter;
  engine::retire_node (&fake->node);
}
#endif
} // namespace

// --- records and the cache
// -----------------------------------------------------

TEST (LumexHazardPointerEngineTest,
      GivenManyMakeAndDestroy_WhenRepeated_ThenNoNewSlotIsCreated)
{
  {
    hp::hazard_pointer warm = hp::make_hazard_pointer ();
  }
  std::size_t const records = engine::statistics ().records;
  for (int i = 0; i < 100000; ++i)
    {
      hp::hazard_pointer holder = hp::make_hazard_pointer ();
      ASSERT_FALSE (holder.empty ());
    }
  EXPECT_EQ (engine::statistics ().records, records);
}

TEST (
    LumexHazardPointerEngineTest,
    GivenMoreHoldersThanTheCache_WhenAllAreReleased_ThenTheyAreReusedWithoutGrowth)
{
  std::size_t const count = 100;
  {
    std::vector<hp::hazard_pointer> holders;
    for (std::size_t i = 0; i < count; ++i)
      {
        holders.push_back (hp::make_hazard_pointer ());
      }
  }
  std::size_t const records = engine::statistics ().records;
  EXPECT_GE (records, count);
  {
    std::vector<hp::hazard_pointer> holders;
    for (std::size_t i = 0; i < count; ++i)
      {
        holders.push_back (hp::make_hazard_pointer ());
      }
  }
  EXPECT_EQ (engine::statistics ().records, records)
      << "the second round took only released slots";
}

TEST (LumexHazardPointerEngineTest,
      GivenSlotsInUse_WhenAskingTheCounters_ThenRecordsMinusPooledGrowsByThem)
{
  std::size_t const before = in_use ();
  std::vector<hp::hazard_pointer> holders;
  for (int i = 0; i < 40; ++i)
    {
      holders.push_back (hp::make_hazard_pointer ());
    }
  EXPECT_GE (in_use (), before + 40 - 8)
      << "at most one cache of free slots is hidden from the pool";
  holders.clear ();
}

TEST (
    LumexHazardPointerEngineTest,
    GivenAThreadThatMadeAndReleasedHolders_WhenItExits_ThenItsCachedSlotsReturnToThePool)
{
  {
    hp::hazard_pointer warm = hp::make_hazard_pointer ();
  }
  std::size_t const baseline = in_use ();
  std::thread worker (
      []
        {
          std::vector<hp::hazard_pointer> holders;
          for (int i = 0; i < 8; ++i)
            {
              holders.push_back (hp::make_hazard_pointer ());
            }
        });
  worker.join ();
  EXPECT_EQ (in_use (), baseline)
      << "the cache of the exited thread was not handed back";
}

TEST (
    LumexHazardPointerEngineTest,
    GivenThreadChurn_WhenWavesOfThreadsMakeHolders_ThenSlotsAreReusedAndReturned)
{
  {
    hp::hazard_pointer warm = hp::make_hazard_pointer ();
  }
  std::size_t const baseline = in_use ();
  std::size_t const threads = 32;
  std::size_t records_after_first_wave = 0;
  for (int wave = 0; wave < 3; ++wave)
    {
      std::vector<std::thread> workers;
      for (std::size_t t = 0; t < threads; ++t)
        {
          workers.emplace_back (
              []
                {
                  for (int round = 0; round < 50; ++round)
                    {
                      std::vector<hp::hazard_pointer> holders;
                      for (int i = 0; i < 5; ++i)
                        {
                          holders.push_back (hp::make_hazard_pointer ());
                        }
                    }
                });
        }
      for (std::thread &worker : workers)
        {
          worker.join ();
        }
      if (wave == 0)
        {
          records_after_first_wave = engine::statistics ().records;
        }
      EXPECT_EQ (in_use (), baseline) << "wave " << wave;
    }
  EXPECT_LE (engine::statistics ().records,
             records_after_first_wave + 5 * threads)
      << "later waves reuse the slots of the first one";
}

TEST (
    LumexHazardPointerEngineTest,
    GivenAHolderMovedToAnotherThread_WhenDestroyedThere_ThenTheSlotIsReusable)
{
  {
    hp::hazard_pointer warm = hp::make_hazard_pointer ();
  }
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  std::size_t const while_alive = in_use ();
  std::thread other ((holder_mover (std::move (holder))));
  other.join ();
  EXPECT_TRUE (holder.empty ());
  EXPECT_EQ (in_use () + 1, while_alive)
      << "the slot went to the pool when the other thread exited";
}

#if defined(__linux__)
namespace
{
pthread_key_t g_late_key;
std::atomic<int> g_late_runs (0);

struct late_state
{
  int round;
  hp::hazard_pointer held;
  late_state () : round (0) {}
};

// Runs in the second round of the thread-exit destructors, after the hook of
// the engine emptied the cache of the thread.
void
late_destructor (void *data)
{
  late_state *state = static_cast<late_state *> (data);
  if (state->round == 0)
    {
      state->round = 1;
      pthread_setspecific (g_late_key, state);
      return;
    }
  state->held = hp::hazard_pointer ();
  {
    hp::hazard_pointer fresh = hp::make_hazard_pointer ();
    std::atomic<counted_node *> nothing (nullptr);
    EXPECT_EQ (fresh.protect (nothing), nullptr);
  }
  g_late_runs.fetch_add (1);
  delete state;
}
} // namespace

TEST (
    LumexHazardPointerEngineTest,
    GivenADestructorAfterTheEngineHook_WhenItMakesAndReleasesHolders_ThenItWorks)
{
  {
    hp::hazard_pointer warm = hp::make_hazard_pointer ();
  }
  std::size_t const baseline = in_use ();
  ASSERT_EQ (pthread_key_create (&g_late_key, &late_destructor), 0);
  g_late_runs = 0;
  std::thread worker (
      []
        {
          late_state *state = new late_state;
          state->held = hp::make_hazard_pointer ();
          pthread_setspecific (g_late_key, state);
        });
  worker.join ();
  EXPECT_EQ (g_late_runs.load (), 1);
  EXPECT_EQ (in_use (), baseline) << "the late release reached the pool";
  pthread_key_delete (g_late_key);
}
#endif

// --- retired lists and passes -----------------------------------------------

TEST (LumexHazardPointerEngineTest,
      GivenRetiredUnprotectedObjects_WhenCleaningUp_ThenRetiredEqualsReclaimed)
{
  hp::clean_up ();
  engine::statistics_t const before = engine::statistics ();
  counters_t counters;
  for (int i = 0; i < 500; ++i)
    {
      (new counted_node (counters, i))->retire ();
    }
  hp::clean_up ();
  engine::statistics_t const after = engine::statistics ();
  EXPECT_EQ (after.retired - before.retired, 500u);
  EXPECT_EQ (after.reclaimed - before.reclaimed, 500u);
  EXPECT_EQ (counters.deleted.load (), 500);
}

TEST (
    LumexHazardPointerEngineTest,
    GivenManyRetires_WhenThePassThresholdIsCrossed_ThenPassesStayFewAndObjectsAreReclaimedOnTheWay)
{
  hp::clean_up ();
  engine::statistics_t const before = engine::statistics ();
  counters_t counters;
  int const total = 100000;
  for (int i = 0; i < total; ++i)
    {
      (new counted_node (counters, i))->retire ();
    }
  engine::statistics_t const during = engine::statistics ();
  EXPECT_GT (during.reclaimed - before.reclaimed, 0u)
      << "retire reclaims without a call to clean_up";
  // Each pass takes at least the threshold (1000 nodes, more with many
  // slots); a pass that does not settle its count would run on every retire.
  std::size_t const threshold
      = 2 * during.records < 1000 ? 1000 : 2 * during.records;
  EXPECT_LE (during.passes - before.passes,
             static_cast<std::size_t> (total) / threshold * 2 + 5)
      << "far more passes than the threshold allows";
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), total);
}

TEST (
    LumexHazardPointerEngineTest,
    GivenAProtectedObject_WhenManyPassesRun_ThenItSurvivesAndIsReclaimedAfterwards)
{
  counters_t counters;
  std::atomic<counted_node *> source (new counted_node (counters, 1));
  counted_node *kept = source.load ();
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), kept);
  source.store (nullptr);
  kept->retire ();
  for (int i = 0; i < 20000; ++i)
    {
      (new counted_node (counters, i))->retire ();
    }
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 20000) << "all but the protected one";
  EXPECT_EQ (kept->value, 1);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 20001);
}

TEST (LumexHazardPointerEngineTest,
      GivenRetiresFromManyThreads_WhenCleaningUp_ThenEveryObjectIsDeletedOnce)
{
  counters_t counters;
  unsigned const threads = lumex_hp_test::stress_threads ();
  unsigned const per_thread = lumex_hp_test::stress_scale (20000);
  std::vector<std::thread> workers;
  for (unsigned t = 0; t < threads; ++t)
    {
      workers.emplace_back (
          [&counters, per_thread]
            {
              for (unsigned i = 0; i < per_thread; ++i)
                {
                  (new counted_node (counters, static_cast<int> (i)))
                      ->retire ();
                }
            });
    }
  for (std::thread &worker : workers)
    {
      worker.join ();
    }
  hp::clean_up ();
  EXPECT_EQ (counters.constructed.load (),
             static_cast<int> (threads * per_thread));
  EXPECT_EQ (counters.deleted.load (), counters.constructed.load ());
}

TEST (
    LumexHazardPointerEngineTest,
    GivenAFewRetiredObjectsAndTwoSeconds_WhenRetiringMoreLater_ThenThePeriodTriggersAPass)
{
  hp::clean_up ();
  counters_t counters;
  // Arm the clock: the first sample after a pass only sets the deadline.
  for (int i = 0; i < 200; ++i)
    {
      (new counted_node (counters, i))->retire ();
    }
  engine::statistics_t const before = engine::statistics ();
  std::this_thread::sleep_for (std::chrono::milliseconds (2300));
  for (int i = 0; i < 200; ++i)
    {
      (new counted_node (counters, i))->retire ();
    }
  engine::statistics_t const after = engine::statistics ();
  EXPECT_GT (after.passes, before.passes)
      << "a few hundred retires, far below the count threshold, after the "
         "period";
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 400);
}

// --- deleters and the pass
// ---------------------------------------------------

TEST (
    LumexHazardPointerEngineTest,
    GivenADeleterThatRetiresDuringAPass_WhenCleaningUp_ThenTheNewObjectIsReclaimedToo)
{
  int deleted = 0;
  (new spawn_node (&deleted, 50))->retire ();
  hp::clean_up ();
  EXPECT_EQ (deleted, 51) << "the chain of retires made by the deleters";
}

// --- addresses that differ only above bit 31
// ---------------------------------

#if defined(__linux__)
TEST (
    LumexHazardPointerEngineTest,
    GivenTwoNodesWhoseAddressesDifferOnlyAboveBit31_WhenOneIsProtected_ThenOnlyThatOneSurvives)
{
  // The pass compares whole addresses: reserve 9 GiB of address space and
  // place one node at the start of the first 4 GiB boundary and another
  // exactly 4 GiB above it, so their low 32 bits agree.
  std::size_t const gib = std::size_t (1) << 30;
  void *const reserved
      = mmap (nullptr, 9 * gib, PROT_NONE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (reserved == MAP_FAILED)
    {
      GTEST_SKIP () << "cannot reserve 9 GiB of address space";
    }
  std::uintptr_t const base = reinterpret_cast<std::uintptr_t> (reserved);
  std::uintptr_t const low
      = (base + 4 * gib - 1) & ~(std::uintptr_t (4 * gib) - 1);
  std::uintptr_t const high = low + 4 * gib;
  ASSERT_LE (high + 4096, base + 9 * gib);
  ASSERT_EQ (
      mprotect (reinterpret_cast<void *> (low), 4096, PROT_READ | PROT_WRITE),
      0);
  ASSERT_EQ (
      mprotect (reinterpret_cast<void *> (high), 4096, PROT_READ | PROT_WRITE),
      0);

  fake_node *const protected_one = reinterpret_cast<fake_node *> (low);
  fake_node *const other = reinterpret_cast<fake_node *> (high);
  std::atomic<int> protected_count (0);
  std::atomic<int> other_count (0);

  for (int padding_nodes = 0; padding_nodes <= 12; padding_nodes += 12)
    {
      // 0 padding nodes: the pass matches node by node; 12: through its set.
      protected_count = 0;
      other_count = 0;
      engine::slot_t *slot = engine::acquire_slot ();
      slot->value.store (&protected_one->node);
      std::vector<std::unique_ptr<fake_node>> padding;
      std::atomic<int> padding_count (0);
      retire_fake (protected_one, &protected_count);
      retire_fake (other, &other_count);
      for (int i = 0; i < padding_nodes; ++i)
        {
          padding.push_back (std::unique_ptr<fake_node> (new fake_node));
          retire_fake (padding.back ().get (), &padding_count);
        }
      hp::clean_up ();
      EXPECT_EQ (protected_count.load (), 0) << "the protected node is kept";
      EXPECT_EQ (other_count.load (), 1)
          << "an address that differs above bit 31 is not protected ("
          << padding_nodes << " padding nodes)";
      EXPECT_EQ (padding_count.load (), padding_nodes);
      slot->value.store (nullptr);
      hp::clean_up ();
      EXPECT_EQ (protected_count.load (), 1);
      engine::release_slot (slot);
    }
  munmap (reserved, 9 * gib);
}
#endif

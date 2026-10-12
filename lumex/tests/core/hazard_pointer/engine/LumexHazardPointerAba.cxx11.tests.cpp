// ABA tests. A node address is freed and allocated again between a thread's
// read and its compare-and-swap; the compare-and-swap then succeeds although
// the structure is not what the thread saw. Hazard pointers prevent that
// because a node a thread protects is not reclaimed, so its address cannot
// come back.
//
// Every scenario runs twice with the same script: over the hazard policy,
// where the structure must come out right, and over the unprotected policy,
// where the script must damage it. The second run proves the test can see ABA;
// if the allocator did not reuse the address, or the checks were too weak, it
// would pass without damage and the test would fail. The scripts are
// deterministic (the interfering operations run from the hook, inside the
// ABA window, on the same thread). The stress tests then run the structures
// over the immediate-reuse allocator on many threads, with a seed that is
// printed and can be replayed (LUMEX_HP_SEED).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStress.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStructures.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using namespace lumex_hp_structures;
namespace hp = lumex::core::hazard_pointer;

// --- scripts
// ---------------------------------------------------------------------

struct script_result
{
  bool address_reused;         // the freed address was allocated again
  long reader_result;          // what the interrupted operation returned
  std::vector<long> remaining; // what the structure holds afterwards
};

// Stack: 3 (A) -> 2 (B) -> 1 (C). The reader reads A and its successor B and
// is about to swing the head. The hook pops A and B, takes B's block for a
// node that stays outside the stack, and pushes 30 into A's block. Without
// protection the head is A again but B's block is no longer in the stack:
// the reader's swing puts the outside node at the head.
template <class Policy>
script_result
run_stack_script ()
{
  script_result result = { false, 0, std::vector<long> () };
  treiber_stack_t<Policy> stack;
  stack.push (1);
  stack.push (2);
  stack.push (3);
  void const *const a_address = stack.top_address ();
  typename Policy::guard reader;
  typename Policy::guard writer;
  bool fired = false;
  stack.hook = [&] (int)
    {
      if (fired)
        {
          return;
        }
      fired = true;
      long value = 0;
      stack.pop (writer, value); // A
      void const *const b_address = stack.top_address ();
      stack.pop (writer, value); // B
      Policy::settle ();
      stack.pool.prefer (b_address);
      stack.pool.create (777); // outside the stack, in B's block
      stack.pool.prefer (a_address);
      stack.push (30);
      result.address_reused = stack.top_address () == a_address;
    };
  stack.pop (reader, result.reader_result);
  stack.hook = nullptr;
  result.remaining = stack.contents ();
  return result;
}

// Queue: dummy (X) -> 1 (Y) -> 2 -> 3. The reader reads the head X and its
// successor Y (value 1) and is about to swing the head. The hook dequeues
// three values, puts a node outside the queue into Y's block, enqueues 100
// into X's block and dequeues it, so X's block is the dummy again, then
// enqueues 200. The reader must return 200 and leave the queue empty.
template <class Policy>
script_result
run_queue_script ()
{
  script_result result = { false, 0, std::vector<long> () };
  ms_queue_t<Policy> queue;
  queue.enqueue_unguarded (1);
  queue.enqueue_unguarded (2);
  queue.enqueue_unguarded (3);
  void const *const x_address = queue.head_address ();
  typename Policy::guard first;
  typename Policy::guard second;
  typename Policy::guard w1;
  typename Policy::guard w2;
  typename Policy::guard w3;
  bool fired = false;
  queue.hook = [&] (int)
    {
      if (fired)
        {
          return;
        }
      fired = true;
      long value = 0;
      queue.dequeue (w1, w2, value); // 1, frees X
      void const *const y_address = queue.head_address ();
      queue.dequeue (w1, w2, value); // 2, frees Y (the dummy before)
      queue.dequeue (w1, w2, value); // 3
      Policy::settle ();
      queue.pool.prefer (y_address);
      queue.pool.create (777); // outside the queue, in Y's block
      queue.pool.prefer (x_address);
      queue.enqueue (w3, 100);
      queue.dequeue (w1, w2, value); // 100: its node is the dummy now
      result.address_reused = queue.head_address () == x_address;
      queue.enqueue (w3, 200);
    };
  queue.dequeue (first, second, result.reader_result);
  queue.hook = nullptr;
  result.remaining = queue.contents ();
  return result;
}

// List: 10 (A) -> 20 (B). The reader looks for the place of 15: after A, in
// front of B, and is about to link its node there. The hook removes 10 and
// 20, inserts 100 into B's block and 50 into A's block. Without protection
// A's block is a live node again whose link is B's address, so the reader's
// link succeeds and puts 15 after 50.
template <class Policy>
script_result
run_list_script ()
{
  script_result result = { false, 0, std::vector<long> () };
  michael_list_t<Policy> list;
  list.insert (10);
  list.insert (20);
  std::vector<void const *> const addresses = list.addresses ();
  bool fired = false;
  list.hook = [&] (int)
    {
      if (fired)
        {
          return;
        }
      fired = true;
      list.remove (10);
      list.remove (20);
      Policy::settle ();
      list.pool.prefer (addresses[1]);
      list.insert (100);
      list.pool.prefer (addresses[0]);
      list.insert (50);
      result.address_reused = list.first_address () == addresses[0];
    };
  result.reader_result = list.insert (15) ? 1 : 0;
  list.hook = nullptr;
  result.remaining = list.keys ();
  return result;
}

} // namespace

// ===========================================================================
TEST (LumexHazardPointerAbaTest,
      GivenAStackScript_WhenHazardPointersProtect_ThenTheStackStaysConsistent)
{
  script_result const result = run_stack_script<hazard_policy> ();
  EXPECT_FALSE (result.address_reused)
      << "A is protected, its block stays out";
  EXPECT_EQ (result.reader_result, 30);
  EXPECT_EQ (result.remaining, (std::vector<long>{ 1 }));
}

TEST (
    LumexHazardPointerAbaTest,
    GivenTheSameStackScript_WhenNothingProtects_ThenTheAbaProblemDamagesTheStack)
{
  script_result const result = run_stack_script<unprotected_policy> ();
  EXPECT_TRUE (result.address_reused) << "the allocator did not reuse A";
  EXPECT_NE (result.remaining, (std::vector<long>{ 1 }))
      << "the test cannot see ABA: the unprotected stack came out right";
}

TEST (LumexHazardPointerAbaTest,
      GivenAQueueScript_WhenHazardPointersProtect_ThenTheQueueStaysConsistent)
{
  script_result const result = run_queue_script<hazard_policy> ();
  EXPECT_FALSE (result.address_reused);
  EXPECT_EQ (result.reader_result, 200);
  EXPECT_TRUE (result.remaining.empty ());
}

TEST (
    LumexHazardPointerAbaTest,
    GivenTheSameQueueScript_WhenNothingProtects_ThenTheAbaProblemDamagesTheQueue)
{
  script_result const result = run_queue_script<unprotected_policy> ();
  EXPECT_TRUE (result.address_reused) << "the allocator did not reuse X";
  EXPECT_FALSE (result.reader_result == 200 && result.remaining.empty ())
      << "the test cannot see ABA: the unprotected queue came out right";
}

TEST (LumexHazardPointerAbaTest,
      GivenAListScript_WhenHazardPointersProtect_ThenTheListStaysSorted)
{
  script_result const result = run_list_script<hazard_policy> ();
  EXPECT_FALSE (result.address_reused);
  EXPECT_EQ (result.reader_result, 1);
  EXPECT_EQ (result.remaining, (std::vector<long>{ 15, 50, 100 }));
}

TEST (
    LumexHazardPointerAbaTest,
    GivenTheSameListScript_WhenNothingProtects_ThenTheAbaProblemBreaksTheOrder)
{
  script_result const result = run_list_script<unprotected_policy> ();
  EXPECT_TRUE (result.address_reused) << "the allocator did not reuse A";
  EXPECT_NE (result.remaining, (std::vector<long>{ 15, 50, 100 }))
      << "the test cannot see ABA: the unprotected list came out right";
}

// --- A -> B -> A on a plain atomic pointer
// -----------------------------------

namespace
{
struct plain_node
    : hp_ns::hazard_pointer_obj_base<plain_node, pool_deleter<plain_node>>
{
  int value;
  explicit plain_node (int v) : value (v) {}
};
} // namespace

TEST (
    LumexHazardPointerAbaTest,
    GivenAReaderHoldingA_WhenTheSourceGoesAToBToAgain_ThenTheAddressIsNotReusedWhileHeld)
{
  recycling_pool<plain_node> pool;
  plain_node *const a = pool.create (1);
  std::atomic<plain_node *> source (a);
  void const *const a_address = a;

  hp::hazard_pointer reader = hp::make_hazard_pointer ();
  plain_node *seen = reader.protect (source);
  ASSERT_EQ (seen, a);

  // A -> B: the writer replaces and retires A, then allocates again.
  plain_node *const b = pool.create (2);
  source.store (b);
  a->retire (pool_deleter<plain_node> (&pool));
  hp::clean_up ();
  plain_node *const c = pool.create (3);
  EXPECT_NE (static_cast<void const *> (c), a_address)
      << "A was reclaimed under a protecting reader";
  EXPECT_EQ (seen->value, 1) << "A is intact";

  // B -> A': try to put an object at A's address back.
  source.store (c);
  plain_node *stale = a;
  EXPECT_FALSE (reader.try_protect (stale, source))
      << "the source holds a different object now";
  EXPECT_EQ (stale, c);

  // Once the reader lets go, A's block is free and is reused at once.
  reader.reset_protection ();
  hp::clean_up ();
  plain_node *const d = pool.create (4);
  EXPECT_EQ (static_cast<void const *> (d), a_address)
      << "the allocator must reuse a freed block, or the test proves nothing";
  EXPECT_EQ (pool.reused (), 1);

  for (plain_node *node : { b, c, d })
    {
      source.store (nullptr);
      node->retire (pool_deleter<plain_node> (&pool));
    }
  hp::clean_up ();
  EXPECT_EQ (pool.live (), 0);
}

TEST (
    LumexHazardPointerAbaTest,
    GivenNoProtection_WhenTheSourceGoesAToBToAWithANewObject_ThenTheComparisonIsFooled)
{
  recycling_pool<plain_node> pool;
  plain_node *const a = pool.create (1);
  std::atomic<plain_node *> source (a);
  plain_node *const remembered = source.load ();
  // The reader decided on `remembered` earlier; meanwhile A -> B -> A'.
  plain_node *const b = pool.create (2);
  source.store (b);
  pool.destroy (a);
  plain_node *const again = pool.create (99);
  pool.destroy (b);
  source.store (again);
  EXPECT_EQ (source.load (), remembered)
      << "the address is back: a plain comparison cannot tell the objects "
         "apart";
  EXPECT_EQ (remembered->value, 99) << "and the reader sees another object";
  pool.destroy (again);
}

// --- protection against reuse during a scan
// ------------------------------------

TEST (
    LumexHazardPointerAbaTest,
    GivenAHolderReusedByAnotherThreadWhileAScanRuns_WhenReadersComeAndGo_ThenNoProtectedObjectIsReclaimed)
{
  // Slots are reused by other threads while passes run: a reader announces in
  // a slot another thread just released. Readers hold their object only
  // briefly and check it; a pass that reads a slot as free while a new owner
  // has just announced would reclaim a live object.
  recycling_pool<plain_node> pool;
  std::atomic<plain_node *> source (pool.create (1));
  std::atomic<bool> stop (false);
  std::atomic<long> violations (0);
  std::thread writer (
      [&]
        {
          int value = 2;
          while (!stop.load ())
            {
              plain_node *old = source.exchange (pool.create (value++));
              old->retire (pool_deleter<plain_node> (&pool));
            }
        });
  unsigned const rounds = lumex_hp_test::stress_scale (300);
  std::vector<std::thread> readers;
  for (unsigned t = 0; t < 4; ++t)
    {
      readers.emplace_back (
          [&]
            {
              for (unsigned i = 0; i < rounds; ++i)
                {
                  // A fresh holder per round: its slot is recycled between
                  // threads.
                  hp::hazard_pointer holder = hp::make_hazard_pointer ();
                  plain_node *node = holder.protect (source);
                  int const first = node->value;
                  std::this_thread::yield ();
                  if (node->value != first || node->value <= 0)
                    {
                      violations.fetch_add (1);
                    }
                }
            });
    }
  for (std::thread &reader : readers)
    {
      reader.join ();
    }
  stop.store (true);
  writer.join ();
  EXPECT_EQ (violations.load (), 0);
  source.load ()->retire (pool_deleter<plain_node> (&pool));
  hp::clean_up ();
  EXPECT_EQ (pool.live (), 0);
}

// --- stress over the immediate-reuse allocator
// ---------------------------------

TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenTheStackOverImmediateReuse_WhenProducersAndConsumersRun_ThenEveryValueComesOutOnce)
{
  std::uint64_t const seed = run_seed ();
  long const per_producer
      = static_cast<long> (lumex_hp_test::stress_scale (6000));
  for (unsigned threads : thread_counts ())
    {
      unsigned const producers = std::max (1u, threads / 2);
      unsigned const consumers = std::max (1u, threads - threads / 2);
      stack_outcome const outcome = run_stack_stress<hazard_policy> (
          producers, consumers, per_producer, seed);
      EXPECT_FALSE (outcome.duplicate) << threads << " threads, seed " << seed;
      EXPECT_EQ (outcome.popped, outcome.expected_count)
          << threads << " threads, seed " << seed;
      EXPECT_EQ (outcome.sum, outcome.expected_sum)
          << threads << " threads, seed " << seed;
    }
}

TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenManyProducersOneConsumerAndTheReverse_WhenTheStackRuns_ThenNothingIsLost)
{
  std::uint64_t const seed = run_seed ();
  long const per_producer
      = static_cast<long> (lumex_hp_test::stress_scale (4000));
  unsigned const many = std::max (4u, lumex_hp_test::stress_threads ());
  stack_outcome many_to_one
      = run_stack_stress<hazard_policy> (many, 1, per_producer, seed + 1);
  EXPECT_FALSE (many_to_one.duplicate);
  EXPECT_EQ (many_to_one.popped, many_to_one.expected_count);
  EXPECT_EQ (many_to_one.sum, many_to_one.expected_sum);
  stack_outcome one_to_many
      = run_stack_stress<hazard_policy> (1, many, per_producer * 2, seed + 2);
  EXPECT_FALSE (one_to_many.duplicate);
  EXPECT_EQ (one_to_many.popped, one_to_many.expected_count);
  EXPECT_EQ (one_to_many.sum, one_to_many.expected_sum);
}

TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenTheQueueOverImmediateReuse_WhenProducersAndConsumersRun_ThenEveryValueComesOutOnceInOrder)
{
  std::uint64_t const seed = run_seed ();
  long const per_producer
      = static_cast<long> (lumex_hp_test::stress_scale (5000));
  for (unsigned threads : thread_counts ())
    {
      unsigned const producers = std::max (1u, threads / 2);
      unsigned const consumers = std::max (1u, threads - threads / 2);
      queue_outcome const outcome = run_queue_stress<hazard_policy> (
          producers, consumers, per_producer, seed);
      EXPECT_FALSE (outcome.duplicate) << threads << " threads, seed " << seed;
      EXPECT_FALSE (outcome.order_broken)
          << threads << " threads, seed " << seed;
      EXPECT_EQ (outcome.dequeued, outcome.expected)
          << threads << " threads, seed " << seed;
    }
}

TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenTheListOverImmediateReuse_WhenThreadsInsertRemoveAndLookUp_ThenTheListIsSortedAndMembershipAgrees)
{
  std::uint64_t const seed = run_seed ();
  unsigned const operations = lumex_hp_test::stress_scale (20000);
  for (unsigned threads : thread_counts ())
    {
      list_outcome const outcome
          = run_list_stress<hazard_policy> (threads, operations, seed);
      EXPECT_FALSE (outcome.unsorted) << threads << " threads, seed " << seed;
      EXPECT_FALSE (outcome.membership_wrong)
          << threads << " threads, seed " << seed;
    }
}

TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenManyReadersAndOneWriterOrTheReverse_WhenTheListRuns_ThenItStaysConsistent)
{
  std::uint64_t const seed = run_seed ();
  unsigned const operations = lumex_hp_test::stress_scale (20000);
  unsigned const many = std::max (4u, lumex_hp_test::stress_threads ());
  // Threads below `writers_only_from` only look keys up: many readers and one
  // writer, then one reader and many writers.
  list_outcome const readers
      = run_list_stress<hazard_policy> (many + 1, operations, seed + 3, many);
  EXPECT_FALSE (readers.unsorted);
  EXPECT_FALSE (readers.membership_wrong);
  list_outcome const writers
      = run_list_stress<hazard_policy> (many + 1, operations, seed + 4, 1);
  EXPECT_FALSE (writers.unsorted);
  EXPECT_FALSE (writers.membership_wrong);
}

// The unprotected structures over the same allocator: the stress run must be
// able to see the damage. It is a race, so a run that finds nothing within the
// budget is reported as skipped, not failed; the scripts above are the
// deterministic proof.
TEST (
    LumexHazardPointerAbaTest,
    Stress_GivenTheUnprotectedStructures_WhenTheyRunOverImmediateReuse_ThenTheDamageShowsUp)
{
#if LUMEX_HP_TEST_TSAN
  GTEST_SKIP () << "the unprotected structures race on purpose";
#endif
  std::uint64_t const seed = run_seed ();
  bool damaged = false;
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::milliseconds (3000);
  for (unsigned round = 0; !damaged && std::chrono::steady_clock::now () < end;
       ++round)
    {
      stack_outcome const stack
          = run_stack_stress<unprotected_policy> (2, 4, 500, seed + round);
      damaged = damaged || stack.duplicate
                || stack.popped != stack.expected_count
                || stack.sum != stack.expected_sum;
      queue_outcome const queue
          = run_queue_stress<unprotected_policy> (2, 4, 500, seed + round);
      damaged = damaged || queue.duplicate || queue.order_broken
                || queue.dequeued != queue.expected;
      list_outcome const list
          = run_list_stress<unprotected_policy> (6, 300, seed + round);
      damaged = damaged || list.unsorted || list.membership_wrong;
    }
  if (!damaged)
    {
      GTEST_SKIP () << "no ABA damage showed up in the budget on this machine";
    }
  SUCCEED ();
}

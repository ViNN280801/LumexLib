// Stress tests of the protocol: readers against writers that retire what they
// replace, a lock-free stack, and a list traversed hand-over-hand against one
// writer. A reader that touches a deleted node sees the poison (the pair
// `second == ~first` is broken, the value is overwritten, the key goes
// backwards) or makes the sanitizer report a use after free. At the end the
// number of constructed and deleted nodes must agree after clean_up ().

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::poisoned_node;
namespace hp = lumex::core::hazard_pointer;

struct start_gate
{
  std::atomic<bool> open;
  start_gate () : open (false) {}
  void
  wait () const
  {
    while (!open.load (std::memory_order_acquire))
      {
        std::this_thread::yield ();
      }
  }
};

// --- a lock-free stack
// ---------------------------------------------------------

struct stack_node : hp::hazard_pointer_obj_base<stack_node>
{
  std::atomic<stack_node *> next;
  long value;
  explicit stack_node (long v) : next (nullptr), value (v) {}
  ~stack_node () { value = -1; }
};

struct hazard_stack
{
  std::atomic<stack_node *> head;
  hazard_stack () : head (nullptr) {}

  void
  push (long value)
  {
    stack_node *node = new stack_node (value);
    stack_node *top = head.load (std::memory_order_relaxed);
    do
      {
        node->next.store (top, std::memory_order_relaxed);
      }
    while (!head.compare_exchange_weak (top, node, std::memory_order_release,
                                        std::memory_order_relaxed));
  }

  // Returns false when the stack is empty.
  bool
  pop (hp::hazard_pointer &holder, long &value)
  {
    stack_node *top = head.load (std::memory_order_relaxed);
    for (;;)
      {
        if (!holder.try_protect (top, head))
          {
            continue;
          }
        if (top == nullptr)
          {
            return false;
          }
        stack_node *next = top->next.load (std::memory_order_relaxed);
        if (head.compare_exchange_strong (top, next, std::memory_order_acq_rel,
                                          std::memory_order_relaxed))
          {
            value = top->value;
            holder.reset_protection ();
            top->retire ();
            return true;
          }
      }
  }
};

// --- a queue-like list: one writer, hand-over-hand readers -----------------

struct list_node : hp::hazard_pointer_obj_base<list_node>
{
  std::atomic<list_node *> next;
  std::uint64_t first;
  std::uint64_t second;
  long key;

  explicit list_node (long k)
      : next (nullptr), first (static_cast<std::uint64_t> (k)),
        second (~static_cast<std::uint64_t> (k)), key (k)
  {
  }

  ~list_node ()
  {
    first = 0xDEADBEEFDEADBEEFull;
    second = 0xDEADBEEFDEADBEEFull;
    key = -1;
  }

  bool
  intact () const
  {
    return second == ~first && static_cast<long> (first) == key && key > 0;
  }
};

struct swmr_list
{
  std::atomic<list_node *> head;
  // The key of the first node still in the list (or one past the last key
  // when it is empty); written by the writer after each removal.
  std::atomic<long> head_key;
  // The writer's private view.
  list_node *tail;
  long size;
  long next_key;

  swmr_list ()
      : head (nullptr), head_key (1), tail (nullptr), size (0), next_key (1)
  {
  }

  void
  append ()
  {
    list_node *node = new list_node (next_key++);
    if (size == 0)
      {
        head.store (node, std::memory_order_seq_cst);
      }
    else
      {
        tail->next.store (node, std::memory_order_seq_cst);
      }
    tail = node;
    ++size;
  }

  void
  remove_first ()
  {
    list_node *old = head.load (std::memory_order_relaxed);
    list_node *after = old->next.load (std::memory_order_relaxed);
    head.store (after, std::memory_order_seq_cst);
    head_key.store (after != nullptr ? after->key : old->key + 1,
                    std::memory_order_seq_cst);
    --size;
    old->retire ();
  }

  // Walks the list with two holders; returns the number of nodes seen, or -1
  // when it saw a broken node or keys that do not increase.
  long
  walk (hp::hazard_pointer &current_holder, hp::hazard_pointer &next_holder)
  {
    for (;;)
      {
        long seen = 0;
        long last_key = 0;
        list_node *current = current_holder.protect (head);
        bool restart = false;
        while (current != nullptr)
          {
            if (!current->intact () || current->key <= last_key)
              {
                return -1;
              }
            last_key = current->key;
            ++seen;
            list_node *next = current->next.load (std::memory_order_acquire);
            while (!next_holder.try_protect (next, current->next))
              {
              }
            // The node is only known to be linked while the head has not
            // passed it; if it has, its successor may be gone.
            if (next != nullptr
                && head_key.load (std::memory_order_seq_cst) > current->key)
              {
                restart = true;
                break;
              }
            swap (current_holder, next_holder);
            current = next;
          }
        if (!restart)
          {
            current_holder.reset_protection ();
            next_holder.reset_protection ();
            return seen;
          }
      }
  }
};
} // namespace

TEST (
    LumexHazardPointerStressTest,
    Stress_GivenReadersAndWriters_WhenWritersReplaceAndRetire_ThenNoReaderSeesADeletedNode)
{
  hp::clean_up ();
  std::atomic<poisoned_node *> shared (new poisoned_node (1));
  unsigned const writers = 2;
  unsigned const readers = lumex_hp_test::stress_threads ();
  unsigned const swaps = lumex_hp_test::stress_scale (100000);
  std::atomic<unsigned> writers_done (0);
  std::atomic<long> violations (0);
  std::atomic<long> reads (0);
  start_gate gate;
  std::vector<std::thread> threads;
  for (unsigned w = 0; w < writers; ++w)
    {
      threads.emplace_back (
          [&, w]
            {
              gate.wait ();
              for (unsigned i = 0; i < swaps; ++i)
                {
                  poisoned_node *fresh = new poisoned_node (
                      static_cast<std::uint64_t> (w) * swaps + i + 2);
                  poisoned_node *old = shared.exchange (fresh);
                  old->retire ();
                }
              writers_done.fetch_add (1);
            });
    }
  for (unsigned r = 0; r < readers; ++r)
    {
      threads.emplace_back (
          [&]
            {
              hp::hazard_pointer holder = hp::make_hazard_pointer ();
              gate.wait ();
              long local_reads = 0;
              while (writers_done.load () < writers)
                {
                  poisoned_node *node = holder.protect (shared);
                  if (!node->intact ())
                    {
                      violations.fetch_add (1);
                    }
                  std::this_thread::yield ();
                  if (!node->intact ())
                    {
                      violations.fetch_add (1);
                    }
                  holder.reset_protection ();
                  ++local_reads;
                }
              reads.fetch_add (local_reads);
            });
    }
  gate.open.store (true, std::memory_order_release);
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  hp::clean_up ();
  EXPECT_EQ (violations.load (), 0) << "a reader saw a deleted node";
  EXPECT_GT (reads.load (), 0);
  EXPECT_EQ (poisoned_node::alive ().load (), 1)
      << "every replaced node was deleted exactly once";
  shared.load ()->retire ();
  hp::clean_up ();
  EXPECT_EQ (poisoned_node::alive ().load (), 0);
}

TEST (
    LumexHazardPointerStressTest,
    Stress_GivenManyShortLivedReaderThreads_WhenWritersRetire_ThenNoReaderSeesADeletedNode)
{
  hp::clean_up ();
  std::atomic<poisoned_node *> shared (new poisoned_node (1));
  std::atomic<bool> stop (false);
  std::atomic<long> violations (0);
  std::thread writer (
      [&]
        {
          std::uint64_t value = 2;
          while (!stop.load ())
            {
              poisoned_node *old
                  = shared.exchange (new poisoned_node (value++));
              old->retire ();
            }
        });
  unsigned const waves = lumex_hp_test::stress_scale (60) / 3 + 1;
  for (unsigned wave = 0; wave < waves; ++wave)
    {
      std::vector<std::thread> readers;
      for (unsigned t = 0; t < 8; ++t)
        {
          readers.emplace_back (
              [&]
                {
                  // The holder is made, used and dropped within the thread:
                  // its slot goes to the thread's cache and back to the pool
                  // at exit.
                  for (int round = 0; round < 20; ++round)
                    {
                      hp::hazard_pointer holder = hp::make_hazard_pointer ();
                      for (int i = 0; i < 20; ++i)
                        {
                          poisoned_node *node = holder.protect (shared);
                          if (!node->intact ())
                            {
                              violations.fetch_add (1);
                            }
                        }
                    }
                });
        }
      for (std::thread &reader : readers)
        {
          reader.join ();
        }
    }
  stop.store (true);
  writer.join ();
  hp::clean_up ();
  EXPECT_EQ (violations.load (), 0);
  shared.load ()->retire ();
  hp::clean_up ();
  EXPECT_EQ (poisoned_node::alive ().load (), 0);
}

TEST (
    LumexHazardPointerStressTest,
    Stress_GivenAStackUsedByManyThreads_WhenPushingAndPopping_ThenEveryValueComesOutOnce)
{
  hp::clean_up ();
  hazard_stack stack;
  unsigned const threads = lumex_hp_test::stress_threads ();
  long const per_thread
      = static_cast<long> (lumex_hp_test::stress_scale (30000));
  std::atomic<long> popped_count (0);
  std::atomic<long> popped_sum (0);
  std::atomic<long> bad_values (0);
  start_gate gate;
  std::vector<std::thread> workers;
  for (unsigned t = 0; t < threads; ++t)
    {
      workers.emplace_back (
          [&, t]
            {
              hp::hazard_pointer holder = hp::make_hazard_pointer ();
              gate.wait ();
              long const base = static_cast<long> (t) * per_thread;
              for (long i = 0; i < per_thread; ++i)
                {
                  stack.push (base + i + 1);
                  long value = 0;
                  if (stack.pop (holder, value))
                    {
                      if (value <= 0)
                        {
                          bad_values.fetch_add (1);
                        }
                      popped_count.fetch_add (1);
                      popped_sum.fetch_add (value);
                    }
                }
            });
    }
  gate.open.store (true, std::memory_order_release);
  for (std::thread &worker : workers)
    {
      worker.join ();
    }
  // Drain what is left.
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  long value = 0;
  while (stack.pop (holder, value))
    {
      popped_count.fetch_add (1);
      popped_sum.fetch_add (value);
    }
  holder.reset_protection ();
  hp::clean_up ();
  long const total = static_cast<long> (threads) * per_thread;
  EXPECT_EQ (bad_values.load (), 0) << "a popped node was already deleted";
  EXPECT_EQ (popped_count.load (), total);
  EXPECT_EQ (popped_sum.load (), total * (total + 1) / 2);
}

TEST (
    LumexHazardPointerStressTest,
    Stress_GivenOneWriterAndHandOverHandReaders_WhenNodesAreRemoved_ThenReadersSeeIncreasingIntactKeys)
{
  hp::clean_up ();
  swmr_list list;
  for (int i = 0; i < 8; ++i)
    {
      list.append ();
    }
  unsigned const readers = lumex_hp_test::stress_threads ();
  long const steps = static_cast<long> (lumex_hp_test::stress_scale (60000));
  std::atomic<bool> done (false);
  std::atomic<long> violations (0);
  std::atomic<long> walks (0);
  start_gate gate;
  std::vector<std::thread> threads;
  for (unsigned r = 0; r < readers; ++r)
    {
      threads.emplace_back (
          [&]
            {
              hp::hazard_pointer first = hp::make_hazard_pointer ();
              hp::hazard_pointer second = hp::make_hazard_pointer ();
              gate.wait ();
              long local_walks = 0;
              while (!done.load ())
                {
                  if (list.walk (first, second) < 0)
                    {
                      violations.fetch_add (1);
                    }
                  ++local_walks;
                }
              walks.fetch_add (local_walks);
            });
    }
  threads.emplace_back (
      [&]
        {
          gate.wait ();
          for (long i = 0; i < steps; ++i)
            {
              list.append ();
              if (list.size > 8)
                {
                  list.remove_first ();
                }
              if ((i % 7) == 0 && list.size > 1)
                {
                  list.remove_first ();
                }
            }
          done.store (true);
        });
  gate.open.store (true, std::memory_order_release);
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  EXPECT_EQ (violations.load (), 0)
      << "a reader saw a deleted or misordered node";
  EXPECT_GT (walks.load (), 0);
  while (list.size > 0)
    {
      list.remove_first ();
    }
  hp::clean_up ();
}

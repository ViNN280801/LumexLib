/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexHazardPointerTestStress.hpp
 * @brief The stress runs of the ABA tests, over the structures of
 * `LumexHazardPointerTestStructures.hpp`, for the GoogleTest suites and the
 * plain soak and thread sanitizer fixtures.
 * @details `run_stack_stress`, `run_queue_stress` and `run_list_stress` run
 * producers, consumers or mixed workers over the immediate-reuse allocator and
 * return what the invariants found (a lost or repeated value, broken order, an
 * unsorted list, a membership that disagrees with the operations that
 * succeeded). They are templates over the policy, so the same code runs the
 * protected and the unprotected structure. The header has no GoogleTest
 * dependency.
 */
#ifndef LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRESS_HPP
#define LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRESS_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStructures.hpp"

namespace lumex_hp_structures
{
/// Reads an unsigned number from the environment, `fallback` when unset.
inline unsigned
env_number (char const *name, unsigned fallback)
{
  char const *text = std::getenv (name);
  if (text == nullptr || *text == '\0')
    {
      return fallback;
    }
  return static_cast<unsigned> (std::strtoul (text, nullptr, 10));
}

inline std::vector<unsigned>
thread_counts ()
{
  unsigned const cores = std::max (1u, std::thread::hardware_concurrency ());
  std::vector<unsigned> counts = { 1, 2, 4, 8, cores * 2 };
  unsigned const limit = env_number ("LUMEX_HP_MAX_THREADS", 64);
  std::vector<unsigned> result;
  for (unsigned count : counts)
    {
      if (count <= limit
          && std::find (result.begin (), result.end (), count)
                 == result.end ())
        {
          result.push_back (count);
        }
    }
  std::sort (result.begin (), result.end ());
  return result;
}

struct gate_t
{
  std::atomic<bool> open;
  gate_t () : open (false) {}
  void
  wait () const
  {
    while (!open.load (std::memory_order_acquire))
      {
        std::this_thread::yield ();
      }
  }
};

// Yields now and then inside the ABA window, to widen it.
template <class Structure>
void
widen_window (Structure &structure, std::uint64_t seed)
{
  std::shared_ptr<random_t> generator (new random_t (seed));
  std::shared_ptr<std::mutex> lock (new std::mutex);
  structure.hook = [generator, lock] (int)
    {
      bool pause = false;
      {
        std::lock_guard<std::mutex> guard (*lock);
        pause = generator->below (4) == 0;
      }
      if (pause)
        {
          std::this_thread::yield ();
        }
    };
}

struct stack_outcome
{
  long popped;
  long sum;
  bool duplicate;
  long expected_count;
  long expected_sum;
};

// Producers push distinct values and consumers pop; every value must come out
// exactly once. `producers` and `consumers` are thread counts.
template <class Policy>
stack_outcome
run_stack_stress (unsigned producers, unsigned consumers, long per_producer,
                  std::uint64_t seed)
{
  treiber_stack_t<Policy> stack;
  widen_window (stack, seed);
  long const total = static_cast<long> (producers) * per_producer;
  std::vector<std::atomic<int>> seen (static_cast<std::size_t> (total + 1));
  for (std::size_t i = 0; i < seen.size (); ++i)
    {
      seen[i].store (0);
    }
  std::atomic<long> popped (0);
  std::atomic<long> sum (0);
  std::atomic<bool> duplicate (false);
  std::atomic<bool> producing (true);
  std::atomic<unsigned> producers_left (producers);
  gate_t gate;
  std::vector<std::thread> threads;
  for (unsigned p = 0; p < producers; ++p)
    {
      threads.emplace_back (
          [&, p]
            {
              gate.wait ();
              random_t generator (seed + p);
              for (long i = 0; i < per_producer; ++i)
                {
                  stack.push (static_cast<long> (p) * per_producer + i + 1);
                  if (generator.below (8) == 0)
                    {
                      std::this_thread::yield ();
                    }
                }
              if (producers_left.fetch_sub (1) == 1)
                {
                  producing.store (false);
                }
            });
    }
  for (unsigned c = 0; c < consumers; ++c)
    {
      threads.emplace_back (
          [&]
            {
              typename Policy::guard guard;
              gate.wait ();
              long local = 0;
              for (;;)
                {
                  long value = 0;
                  if (stack.pop (guard, value))
                    {
                      if (value >= 1 && value <= total)
                        {
                          if (seen[static_cast<std::size_t> (value)]
                                  .fetch_add (1)
                              != 0)
                            {
                              duplicate.store (true);
                            }
                        }
                      else
                        {
                          duplicate.store (true);
                        }
                      ++local;
                      sum.fetch_add (value);
                    }
                  else if (!producing.load ())
                    {
                      break;
                    }
                  if (local > total * 2)
                    {
                      break; // a cycle made by ABA
                    }
                }
              popped.fetch_add (local);
            });
    }
  gate.open.store (true, std::memory_order_release);
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  // What the consumers left behind (they stop when producers are done and the
  // stack looked empty once).
  typename Policy::guard guard;
  long value = 0;
  long drained = 0;
  while (drained < total * 2 && stack.pop (guard, value))
    {
      if (value >= 1 && value <= total)
        {
          if (seen[static_cast<std::size_t> (value)].fetch_add (1) != 0)
            {
              duplicate.store (true);
            }
        }
      else
        {
          duplicate.store (true);
        }
      ++drained;
      sum.fetch_add (value);
    }
  stack_outcome outcome;
  outcome.popped = popped.load () + drained;
  outcome.sum = sum.load ();
  outcome.duplicate = duplicate.load ();
  outcome.expected_count = total;
  outcome.expected_sum = total * (total + 1) / 2;
  return outcome;
}

struct queue_outcome
{
  bool order_broken;
  bool duplicate;
  long dequeued;
  long expected;
};

template <class Policy>
queue_outcome
run_queue_stress (unsigned producers, unsigned consumers, long per_producer,
                  std::uint64_t seed)
{
  ms_queue_t<Policy> queue;
  widen_window (queue, seed);
  long const total = static_cast<long> (producers) * per_producer;
  std::atomic<long> dequeued (0);
  std::atomic<bool> order_broken (false);
  std::atomic<bool> duplicate (false);
  std::atomic<unsigned> producers_left (producers);
  std::atomic<bool> producing (true);
  std::vector<std::atomic<int>> seen (static_cast<std::size_t> (total));
  for (std::size_t i = 0; i < seen.size (); ++i)
    {
      seen[i].store (0);
    }
  gate_t gate;
  std::vector<std::thread> threads;
  for (unsigned p = 0; p < producers; ++p)
    {
      threads.emplace_back (
          [&, p]
            {
              typename Policy::guard guard;
              gate.wait ();
              random_t generator (seed + p);
              for (long i = 0; i < per_producer; ++i)
                {
                  // value = producer * per_producer + sequence number
                  queue.enqueue (guard,
                                 static_cast<long> (p) * per_producer + i);
                  if (generator.below (8) == 0)
                    {
                      std::this_thread::yield ();
                    }
                }
              if (producers_left.fetch_sub (1) == 1)
                {
                  producing.store (false);
                }
            });
    }
  for (unsigned c = 0; c < consumers; ++c)
    {
      threads.emplace_back (
          [&]
            {
              typename Policy::guard first;
              typename Policy::guard second;
              std::vector<long> last (producers, -1);
              gate.wait ();
              long local = 0;
              for (;;)
                {
                  long value = 0;
                  if (queue.dequeue (first, second, value))
                    {
                      if (value < 0 || value >= total)
                        {
                          duplicate.store (true);
                        }
                      else
                        {
                          if (seen[static_cast<std::size_t> (value)]
                                  .fetch_add (1)
                              != 0)
                            {
                              duplicate.store (true);
                            }
                          std::size_t const producer
                              = static_cast<std::size_t> (value
                                                          / per_producer);
                          // One consumer sees one producer's values in order.
                          if (value <= last[producer])
                            {
                              order_broken.store (true);
                            }
                          last[producer] = value;
                        }
                      ++local;
                    }
                  else if (!producing.load ())
                    {
                      break;
                    }
                  if (local > total * 2)
                    {
                      break;
                    }
                }
              dequeued.fetch_add (local);
            });
    }
  gate.open.store (true, std::memory_order_release);
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  typename Policy::guard first;
  typename Policy::guard second;
  long value = 0;
  long drained = 0;
  while (drained < total * 2 && queue.dequeue (first, second, value))
    {
      if (value < 0 || value >= total
          || seen[static_cast<std::size_t> (value)].fetch_add (1) != 0)
        {
          duplicate.store (true);
        }
      ++drained;
    }
  queue_outcome outcome;
  outcome.order_broken = order_broken.load ();
  outcome.duplicate = duplicate.load ();
  outcome.dequeued = dequeued.load () + drained;
  outcome.expected = total;
  return outcome;
}

struct list_outcome
{
  bool unsorted;
  bool membership_wrong;
  long keys_left;
};

// Workers insert, remove and look up keys of a small range; afterwards every
// key is in the list exactly when its successful inserts exceed its
// successful removes, and the list is sorted.
template <class Policy>
list_outcome
run_list_stress (unsigned threads, unsigned operations, std::uint64_t seed,
                 unsigned writers_only_from = 0)
{
  michael_list_t<Policy> list;
  widen_window (list, seed);
  long const range = 64;
  std::vector<std::atomic<long>> balance (static_cast<std::size_t> (range));
  for (std::size_t i = 0; i < balance.size (); ++i)
    {
      balance[i].store (0);
    }
  gate_t gate;
  std::vector<std::thread> workers;
  for (unsigned t = 0; t < threads; ++t)
    {
      workers.emplace_back (
          [&, t]
            {
              random_t generator (seed * 31 + t);
              gate.wait ();
              for (unsigned i = 0; i < operations; ++i)
                {
                  long const key = static_cast<long> (
                      generator.below (static_cast<unsigned> (range)));
                  unsigned const choice = generator.below (10);
                  bool const reader_only = t < writers_only_from;
                  if (reader_only || choice < 4)
                    {
                      (void)list.contains (key);
                    }
                  else if (choice < 7)
                    {
                      if (list.insert (key))
                        {
                          balance[static_cast<std::size_t> (key)].fetch_add (
                              1);
                        }
                    }
                  else
                    {
                      if (list.remove (key))
                        {
                          balance[static_cast<std::size_t> (key)].fetch_sub (
                              1);
                        }
                    }
                }
            });
    }
  gate.open.store (true, std::memory_order_release);
  for (std::thread &worker : workers)
    {
      worker.join ();
    }
  list_outcome outcome = { false, false, 0 };
  std::vector<long> const keys = list.keys ();
  for (std::size_t i = 1; i < keys.size (); ++i)
    {
      if (keys[i] <= keys[i - 1])
        {
          outcome.unsorted = true;
        }
    }
  outcome.keys_left = static_cast<long> (keys.size ());
  for (long key = 0; key < range; ++key)
    {
      long const net = balance[static_cast<std::size_t> (key)].load ();
      bool const present
          = std::find (keys.begin (), keys.end (), key) != keys.end ();
      if (net < 0 || net > 1 || (net == 1) != present)
        {
          outcome.membership_wrong = true;
        }
    }
  return outcome;
}
} // namespace lumex_hp_structures

#endif // !LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRESS_HPP

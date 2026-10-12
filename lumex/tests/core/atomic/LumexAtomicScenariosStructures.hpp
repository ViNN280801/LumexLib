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
 * @file LumexAtomicScenariosStructures.hpp
 * @brief The classic ABA victims written on top of atomic shared pointers: a
 * Treiber stack and a Michael and Scott queue, with checkers and the
 * deliberately unsafe raw-pointer stack that proves the checkers see ABA.
 * @details `SharedStack<Engine>` and `SharedQueue<Engine>` keep every node
 * in a `std::shared_ptr` reached through the engine's atomic shared pointer,
 * the way the lock-free pointer structures of the literature are written when
 * a reclamation scheme is the smart pointer itself. Nodes come from a
 * `lumex_test::ReusePool` (the address of a destroyed node is the next
 * node's), carry a ledger entry, and the structures never free a node by
 * hand.
 *
 * `RawPointerStack` is the same Treiber stack over a raw
 * `std::atomic<Node*>` with the same immediate-reuse pool: the textbook ABA.
 * It is memory safe by construction (the pool never returns memory to the
 * system, nodes are trivially destructible, the freed node stays readable
 * and carries a `state` word), so a test can run the deterministic ABA
 * schedule on it without undefined behavior and read the corruption off the
 * `state` words: the CAS that wrongly succeeds leaves the head pointing at a
 * freed node. `TaggedIndexStack` is the classic cure (a version tag next to
 * an index) and must pass the same schedule.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_STRUCTURES_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_STRUCTURES_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"

namespace lumex_atomic_test
{
namespace scenario
{
/// "1, 2, 3" for a message.
inline std::string
join_ints (std::vector<int> const &values)
{
  std::string out;
  for (std::size_t i = 0; i < values.size (); ++i)
    out += (i == 0 ? "" : ", ") + std::to_string (values[i]);
  return out;
}

/**
 * @brief Releases a chain of shared nodes without recursion.
 * @details A node that holds the `shared_ptr` of the next one would destroy a
 * long chain by recursion (one frame per node). The destructor of a node hands
 * the pointer it holds to `release ()`: the outermost call in a thread
 * destroys it and then the ones its destruction handed over, in a loop. No
 * reference count is read (`use_count ()` is not a synchronization, and
 * reading it to decide whether to steal a member races with the thread that
 * has just dropped its reference).
 */
template <typename NodePtr> class ChainReleaser
{
public:
  static void
  release (NodePtr &first)
  {
    if (active ())
      {
        pending ().push_back (std::move (first));
        return;
      }
    active () = true;
    first.reset ();
    while (!pending ().empty ())
      {
        NodePtr node (std::move (pending ().back ()));
        pending ().pop_back ();
        node.reset ();
      }
    active () = false;
  }

private:
  static std::vector<NodePtr> &
  pending ()
  {
    static thread_local std::vector<NodePtr> list;
    return list;
  }

  static bool &
  active ()
  {
    static thread_local bool flag = false;
    return flag;
  }
};

// --- Treiber stack over atomic shared pointers
// -------------------------------

template <typename Engine> class SharedStack
{
  struct Node
  {
    Node (lumex_test::ObjectLedger &ledger, int v) : entry (ledger), value (v)
    {
    }

    /// Releases the chain behind this node without recursion.
    ~Node () { ChainReleaser<std::shared_ptr<Node>>::release (next); }

    lumex_test::LedgerEntry entry;
    int value;
    std::shared_ptr<Node> next;
  };

  typedef std::shared_ptr<Node> NodePtr;
  typedef typename Engine::template shared<Node> head_type;

public:
  SharedStack (lumex_test::ObjectLedger &ledger, lumex_test::ReusePool &pool)
      : ledger_ (ledger), pool_ (pool)
  {
  }

  ~SharedStack ()
  {
    // Release the chain iteratively: a long chain of shared_ptr would be
    // destroyed recursively.
    NodePtr cur = head_.exchange (NodePtr ());
    while (cur)
      {
        NodePtr next = std::move (cur->next);
        cur = std::move (next);
      }
  }

  SharedStack (SharedStack const &) = delete;
  SharedStack &operator= (SharedStack const &) = delete;

  void
  push (int value)
  {
    NodePtr node = lumex_test::make_pooled<Node> (pool_, ledger_, value);
    node->next = head_.load ();
    while (!head_.compare_exchange_weak (node->next, node))
      {
      }
  }

  bool
  pop (int &out)
  {
    return pop_node (out, nullptr);
  }

  /// Pops, parking at @p stall between reading the next node and the
  /// compare-exchange (the first time only).
  bool
  pop_stalled (int &out, lumex_test::StallPoint &stall)
  {
    return pop_node (out, &stall);
  }

  /// Pops two nodes and releases the second one first.
  void
  pop2_release_reversed (int &first, int &second)
  {
    NodePtr a;
    NodePtr b;
    first = take (a) ? a->value : -1;
    second = take (b) ? b->value : -1;
    b.reset ();
    a.reset ();
  }

  /// The values from the top, and whether every node is intact.
  bool
  consistent (std::string &why, std::vector<int> *values = nullptr)
  {
    NodePtr cur = head_.load ();
    int guard = 0;
    while (cur)
      {
        if (!cur->entry.intact ())
          {
            why = "the stack reaches a destroyed node";
            return false;
          }
        if (values != nullptr)
          values->push_back (cur->value);
        cur = cur->next;
        if (++guard > 1000000)
          {
            why = "the chain of the stack does not end";
            return false;
          }
      }
    return true;
  }

private:
  bool
  take (NodePtr &out)
  {
    out = head_.load ();
    while (out && !head_.compare_exchange_strong (out, out->next))
      {
      }
    return static_cast<bool> (out);
  }

  bool
  pop_node (int &out, lumex_test::StallPoint *stall)
  {
    NodePtr old = head_.load ();
    while (old)
      {
        NodePtr next = old->next;
        if (stall != nullptr)
          {
            stall->park ();
            stall = nullptr;
          }
        if (head_.compare_exchange_strong (old, next))
          {
            out = old->value;
            return true;
          }
      }
    return false;
  }

  lumex_test::ObjectLedger &ledger_;
  lumex_test::ReusePool &pool_;
  head_type head_;
};

// --- the unsafe raw-pointer Treiber stack
// ---------------------------------------

/**
 * @brief The Treiber stack over raw pointers with immediate address reuse.
 * @details Do not use it for anything but the deterministic ABA schedule: it
 * is the structure the checkers must catch. Safe to run (see the file
 * comment), wrong by design.
 */
class RawPointerStack
{
  struct Node
  {
    std::atomic<Node *> next;
    int value;
    std::atomic<int> state; // 1 live, 0 freed
  };

public:
  RawPointerStack () : head_ (nullptr) { pool_.set_poisoning (false); }

  RawPointerStack (RawPointerStack const &) = delete;
  RawPointerStack &operator= (RawPointerStack const &) = delete;

  void
  push (int value)
  {
    Node *node = new (pool_.allocate (sizeof (Node))) Node ();
    node->value = value;
    node->state.store (1);
    Node *old = head_.load ();
    do
      {
        node->next.store (old);
      }
    while (!head_.compare_exchange_weak (old, node));
  }

  bool
  pop (int &out)
  {
    Node *node = pop_node (nullptr);
    if (node == nullptr)
      return false;
    out = node->value;
    free_node (node);
    return true;
  }

  bool
  pop_stalled (int &out, lumex_test::StallPoint &stall)
  {
    Node *node = pop_node (&stall);
    if (node == nullptr)
      return false;
    out = node->value;
    free_node (node);
    return true;
  }

  void
  pop2_release_reversed (int &first, int &second)
  {
    Node *a = pop_node (nullptr);
    Node *b = pop_node (nullptr);
    first = a != nullptr ? a->value : -1;
    second = b != nullptr ? b->value : -1;
    if (b != nullptr)
      free_node (b);
    if (a != nullptr)
      free_node (a);
  }

  bool
  consistent (std::string &why, std::vector<int> *values = nullptr)
  {
    Node *cur = head_.load ();
    int guard = 0;
    while (cur != nullptr)
      {
        if (cur->state.load () != 1)
          {
            why = "the head chain reaches a freed node (ABA corrupted the "
                  "stack)";
            return false;
          }
        if (values != nullptr)
          values->push_back (cur->value);
        cur = cur->next.load ();
        if (++guard > 1000)
          {
            why = "the chain of the stack does not end";
            return false;
          }
      }
    return true;
  }

private:
  Node *
  pop_node (lumex_test::StallPoint *stall)
  {
    Node *old = head_.load ();
    while (old != nullptr)
      {
        Node *next = old->next.load ();
        if (stall != nullptr)
          {
            stall->park ();
            stall = nullptr;
          }
        if (head_.compare_exchange_strong (old, next))
          return old;
      }
    return nullptr;
  }

  void
  free_node (Node *node)
  {
    node->state.store (0);
    pool_.release (node, sizeof (Node));
  }

  std::atomic<Node *> head_;
  lumex_test::ReusePool pool_;
};

// --- the cure: a tagged index
// -------------------------------------------------------

/// The Treiber stack over slot indices with a version tag in the head word.
class TaggedIndexStack
{
  struct Slot
  {
    std::atomic<int> next;
    int value;
    std::atomic<int> state;
  };

public:
  explicit TaggedIndexStack (int capacity = 64)
      : slots_ (new Slot[static_cast<std::size_t> (capacity)]), head_ (0)
  {
    for (int i = capacity - 1; i >= 0; --i)
      {
        slot (i).state.store (0);
        slot (i).next.store (-1);
        slot (i).value = 0;
        free_.push_back (i);
      }
  }

  TaggedIndexStack (TaggedIndexStack const &) = delete;
  TaggedIndexStack &operator= (TaggedIndexStack const &) = delete;

  void
  push (int value)
  {
    int const index = acquire ();
    slot (index).value = value;
    slot (index).state.store (1);
    std::uint64_t old = head_.load ();
    std::uint64_t desired;
    do
      {
        slot (index).next.store (index_of (old));
        desired = make_word (tag_of (old) + 1, index);
      }
    while (!head_.compare_exchange_weak (old, desired));
  }

  bool
  pop (int &out)
  {
    int const index = pop_node (nullptr);
    if (index < 0)
      return false;
    out = slot (index).value;
    release (index);
    return true;
  }

  bool
  pop_stalled (int &out, lumex_test::StallPoint &stall)
  {
    int const index = pop_node (&stall);
    if (index < 0)
      return false;
    out = slot (index).value;
    release (index);
    return true;
  }

  void
  pop2_release_reversed (int &first, int &second)
  {
    int const a = pop_node (nullptr);
    int const b = pop_node (nullptr);
    first = a >= 0 ? slot (a).value : -1;
    second = b >= 0 ? slot (b).value : -1;
    if (b >= 0)
      release (b);
    if (a >= 0)
      release (a);
  }

  bool
  consistent (std::string &why, std::vector<int> *values = nullptr)
  {
    int cur = index_of (head_.load ());
    int guard = 0;
    while (cur >= 0)
      {
        if (slot (cur).state.load () != 1)
          {
            why = "the head chain reaches a freed slot";
            return false;
          }
        if (values != nullptr)
          values->push_back (slot (cur).value);
        cur = slot (cur).next.load ();
        if (++guard > 1000)
          {
            why = "the chain of the stack does not end";
            return false;
          }
      }
    return true;
  }

private:
  Slot &
  slot (int index)
  {
    return slots_[static_cast<std::size_t> (index)];
  }

  static std::uint64_t
  make_word (std::uint64_t tag, int index)
  {
    return (tag << 32) | static_cast<std::uint32_t> (index + 1);
  }

  static int
  index_of (std::uint64_t word)
  {
    return static_cast<int> (static_cast<std::uint32_t> (word)) - 1;
  }

  static std::uint64_t
  tag_of (std::uint64_t word)
  {
    return word >> 32;
  }

  int
  acquire ()
  {
    std::lock_guard<std::mutex> guard (free_mutex_);
    int const index = free_.back ();
    free_.pop_back ();
    return index;
  }

  void
  release (int index)
  {
    slot (index).state.store (0);
    std::lock_guard<std::mutex> guard (free_mutex_);
    free_.push_back (index);
  }

  int
  pop_node (lumex_test::StallPoint *stall)
  {
    std::uint64_t old = head_.load ();
    while (index_of (old) >= 0)
      {
        int const index = index_of (old);
        int const next = slot (index).next.load ();
        if (stall != nullptr)
          {
            stall->park ();
            stall = nullptr;
          }
        if (head_.compare_exchange_strong (old,
                                           make_word (tag_of (old) + 1, next)))
          return index;
      }
    return -1;
  }

  std::unique_ptr<Slot[]> slots_;
  std::atomic<std::uint64_t> head_;
  std::mutex free_mutex_;
  std::vector<int> free_;
};

// --- the deterministic ABA schedule on a stack
// ------------------------------------------

/**
 * @brief The classic ABA interleaving of the Treiber stack, forced.
 * @details The stack holds 3 (top, "A"), 2 ("B"), 1. A popper reads A and B
 * and is stopped before its compare-exchange. Another thread pops A and B,
 * releases B and then A (so A's address is the next one handed out) and
 * pushes 4, which takes A's address. The popper resumes: on a structure that
 * compares only the address it succeeds and installs the freed B as the new
 * head. Afterwards every one of 1, 2, 3, 4 must have been popped exactly
 * once and the stack must reach no freed node; the verdict records what is
 * wrong.
 */
template <typename Stack>
void
treiber_aba_schedule (Stack &stack, lumex_test::Verdict &verdict,
                      std::string const &label)
{
  stack.push (1);
  stack.push (2);
  stack.push (3);
  lumex_test::StallPoint stall;
  stall.arm (true);
  int stalled_value = -1;
  bool stalled_ok = false;
  std::thread popper (
      [&] { stalled_ok = stack.pop_stalled (stalled_value, stall); });
  if (!stall.wait_until_parked (1))
    {
      verdict.fail (label + ": the popper never reached the stall point");
      stall.release ();
      popper.join ();
      return;
    }
  int first = -1;
  int second = -1;
  stack.pop2_release_reversed (first, second);
  stack.push (4);
  stall.release ();
  popper.join ();

  std::vector<int> popped;
  popped.push_back (first);
  popped.push_back (second);
  if (stalled_ok)
    popped.push_back (stalled_value);
  std::string why;
  if (!stack.consistent (why))
    verdict.fail (label + ": " + why);
  int out = 0;
  for (int guard = 0; guard < 8 && stack.pop (out); ++guard)
    popped.push_back (out);
  std::sort (popped.begin (), popped.end ());
  std::vector<int> expected;
  for (int i = 1; i <= 4; ++i)
    expected.push_back (i);
  if (popped != expected)
    verdict.fail (label + ": the values popped are {" + join_ints (popped)
                  + "} instead of {1, 2, 3, 4} (a value was popped twice or "
                    "lost)");
}

// --- stress of the stack
// ------------------------------------------------------------------

/**
 * @brief Threads push unique values and pop concurrently; afterwards every
 * pushed value must have been popped exactly once, with the node ledger
 * balanced.
 */
template <typename Engine>
void
treiber_stress (Params const &p, lumex_test::Verdict &verdict)
{
  lumex_test::ObjectLedger ledger (node_capacity (p, 1));
  lumex_test::ReusePool pool;
  std::vector<std::vector<int>> popped (static_cast<std::size_t> (p.threads));
  {
    SharedStack<Engine> stack (ledger, pool);
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 11));
            std::vector<int> &mine = popped[static_cast<std::size_t> (t)];
            for (int i = 0; i < p.iterations; ++i)
              {
                stack.push (t * p.iterations + i);
                if (schedule.random ().below (2u) == 0u)
                  {
                    int out = 0;
                    if (stack.pop (out))
                      mine.push_back (out);
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    int out = 0;
    while (stack.pop (out))
      popped[0].push_back (out);
    std::string why;
    verdict.require (stack.consistent (why), why);
  }
  Engine::quiesce ();
  std::vector<int> all;
  for (std::size_t t = 0; t < popped.size (); ++t)
    all.insert (all.end (), popped[t].begin (), popped[t].end ());
  std::sort (all.begin (), all.end ());
  int const total = p.threads * p.iterations;
  bool exact = static_cast<int> (all.size ()) == total;
  for (int i = 0; exact && i < total; ++i)
    exact = all[static_cast<std::size_t> (i)] == i;
  verdict.require (exact, "treiber_stress: the popped values are not the "
                          "pushed values exactly once ("
                              + std::to_string (all.size ()) + " popped, "
                              + std::to_string (total) + " pushed)");
  check_balance (ledger, pool, verdict, "treiber_stress");
}

// --- Michael and Scott queue over atomic shared pointers
// -----------------------------------

template <typename Engine> class SharedQueue
{
  struct Node
  {
    Node (lumex_test::ObjectLedger &ledger, int v) : entry (ledger), value (v)
    {
    }

    /// Releases the chain behind this node without recursion.
    ~Node ()
    {
      std::shared_ptr<Node> following
          = next.exchange (std::shared_ptr<Node> ());
      ChainReleaser<std::shared_ptr<Node>>::release (following);
    }

    lumex_test::LedgerEntry entry;
    int value;
    typename Engine::template shared<Node> next;
  };

  typedef std::shared_ptr<Node> NodePtr;
  typedef typename Engine::template shared<Node> link_type;

public:
  SharedQueue (lumex_test::ObjectLedger &ledger, lumex_test::ReusePool &pool)
      : ledger_ (ledger), pool_ (pool)
  {
    NodePtr dummy = lumex_test::make_pooled<Node> (pool_, ledger_, -1);
    head_.store (dummy);
    tail_.store (dummy);
  }

  ~SharedQueue ()
  {
    int ignored = 0;
    while (dequeue (ignored))
      {
      }
    // The dummy is released by the two links; its next is empty.
  }

  SharedQueue (SharedQueue const &) = delete;
  SharedQueue &operator= (SharedQueue const &) = delete;

  void
  enqueue (int value)
  {
    NodePtr node = lumex_test::make_pooled<Node> (pool_, ledger_, value);
    for (;;)
      {
        NodePtr tail = tail_.load ();
        NodePtr next = tail->next.load ();
        if (next)
          {
            // The tail lags behind: help to advance it.
            tail_.compare_exchange_strong (tail, next);
            continue;
          }
        NodePtr empty;
        if (tail->next.compare_exchange_strong (empty, node))
          {
            tail_.compare_exchange_strong (tail, node);
            return;
          }
      }
  }

  bool
  dequeue (int &out)
  {
    for (;;)
      {
        NodePtr head = head_.load ();
        NodePtr tail = tail_.load ();
        NodePtr next = head->next.load ();
        if (!next)
          return false;
        if (head == tail)
          {
            tail_.compare_exchange_strong (tail, next);
            continue;
          }
        int const value = next->value;
        if (head_.compare_exchange_strong (head, next))
          {
            out = value;
            return true;
          }
      }
  }

private:
  lumex_test::ObjectLedger &ledger_;
  lumex_test::ReusePool &pool_;
  link_type head_;
  link_type tail_;
};

/**
 * @brief Producers enqueue numbered items, consumers dequeue concurrently:
 * each item comes out exactly once, in the order of its producer for every
 * consumer, and the node ledger balances.
 */
template <typename Engine>
void
queue_stress (Params const &p, lumex_test::Verdict &verdict)
{
  int const threads = std::max (2, p.threads);
  int const producers = std::max (1, threads / 2);
  int const consumers = std::max (1, threads - producers);
  int const per_producer = p.iterations;
  int const total = producers * per_producer;
  lumex_test::ObjectLedger ledger (static_cast<std::size_t> (total) + 64);
  lumex_test::ReusePool pool;
  std::atomic<int> consumed (0);
  std::vector<std::vector<int>> taken (static_cast<std::size_t> (consumers));
  {
    SharedQueue<Engine> queue (ledger, pool);
    std::string const error = lumex_test::run_threads (
        producers + consumers,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 12));
            if (t < producers)
              {
                for (int i = 0; i < per_producer; ++i)
                  {
                    queue.enqueue (t * per_producer + i);
                    schedule.point ();
                  }
                return;
              }
            std::vector<int> &mine
                = taken[static_cast<std::size_t> (t - producers)];
            std::vector<int> last (static_cast<std::size_t> (producers), -1);
            while (consumed.load () < total)
              {
                int out = 0;
                if (queue.dequeue (out))
                  {
                    consumed.fetch_add (1);
                    mine.push_back (out);
                    int const producer = out / per_producer;
                    int const sequence = out % per_producer;
                    if (sequence <= last[static_cast<std::size_t> (producer)])
                      verdict.fail (
                          "queue_stress: FIFO order of producer "
                          + std::to_string (producer) + " broken: item "
                          + std::to_string (sequence) + " came after "
                          + std::to_string (
                              last[static_cast<std::size_t> (producer)]));
                    last[static_cast<std::size_t> (producer)] = sequence;
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    int out = 0;
    verdict.require (!queue.dequeue (out),
                     "queue_stress: the queue is not empty at the end");
  }
  Engine::quiesce ();
  std::vector<int> all;
  for (std::size_t t = 0; t < taken.size (); ++t)
    all.insert (all.end (), taken[t].begin (), taken[t].end ());
  std::sort (all.begin (), all.end ());
  bool exact = static_cast<int> (all.size ()) == total;
  for (int i = 0; exact && i < total; ++i)
    exact = all[static_cast<std::size_t> (i)] == i;
  verdict.require (exact, "queue_stress: the dequeued items are not the "
                          "enqueued ones exactly once ("
                              + std::to_string (all.size ()) + " of "
                              + std::to_string (total) + ")");
  check_balance (ledger, pool, verdict, "queue_stress");
}

// --- Harris and Michael sorted list over atomic shared pointers
// -----------------------------

/**
 * @brief A lock-free sorted set of ints: the list of Harris with the logical
 * deletion mark of Michael, written over immutable links.
 * @details A node's `next` is an atomic shared pointer to a `Link`, an
 * immutable (successor, marked) pair; changing the successor or marking the
 * node replaces the whole link, so a compare-exchange on a `next` compares
 * the identity of a link and not its content. That is exactly where an
 * implementation that mixes up a new link at an old address with the old one
 * would fail (ABA on the link). Removal marks the node's link, then tries to
 * unlink; any traversal unlinks marked nodes it meets.
 */
template <typename Engine> class SharedSortedList
{
  struct Node;

  struct Link
  {
    Link (lumex_test::ObjectLedger &ledger, std::shared_ptr<Node> s, bool m)
        : entry (ledger), succ (std::move (s)), marked (m)
    {
    }

    lumex_test::LedgerEntry entry;
    std::shared_ptr<Node> succ;
    bool marked;
  };

  typedef std::shared_ptr<Link> LinkPtr;

  struct Node
  {
    Node (lumex_test::ObjectLedger &ledger, int k) : entry (ledger), key (k) {}

    lumex_test::LedgerEntry entry;
    int key;
    typename Engine::template shared<Link> next;
  };

  typedef std::shared_ptr<Node> NodePtr;

  struct Position
  {
    NodePtr pred;
    LinkPtr pred_link;
    NodePtr curr;
  };

public:
  SharedSortedList (lumex_test::ObjectLedger &ledger,
                    lumex_test::ReusePool &pool)
      : ledger_ (ledger), pool_ (pool),
        head_ (lumex_test::make_pooled<Node> (pool, ledger, -1))
  {
    head_->next.store (make_link (NodePtr (), false));
  }

  ~SharedSortedList ()
  {
    // Release the chain from the front so no destructor recurses deeply.
    NodePtr cur = head_;
    head_.reset ();
    while (cur)
      {
        LinkPtr link = cur->next.exchange (LinkPtr ());
        NodePtr following = link ? link->succ : NodePtr ();
        link.reset ();
        cur = std::move (following);
      }
  }

  SharedSortedList (SharedSortedList const &) = delete;
  SharedSortedList &operator= (SharedSortedList const &) = delete;

  bool
  insert (int key)
  {
    for (;;)
      {
        Position pos = find (key);
        if (pos.curr && pos.curr->key == key)
          return false;
        NodePtr node = lumex_test::make_pooled<Node> (pool_, ledger_, key);
        node->next.store (make_link (pos.curr, false));
        LinkPtr expected = pos.pred_link;
        if (pos.pred->next.compare_exchange_strong (expected,
                                                    make_link (node, false)))
          return true;
      }
  }

  bool
  remove (int key)
  {
    for (;;)
      {
        Position pos = find (key);
        if (!pos.curr || pos.curr->key != key)
          return false;
        LinkPtr curr_link = pos.curr->next.load ();
        if (curr_link->marked)
          continue;
        LinkPtr expected = curr_link;
        if (!pos.curr->next.compare_exchange_strong (
                expected, make_link (curr_link->succ, true)))
          continue;
        LinkPtr pred_expected = pos.pred_link;
        pos.pred->next.compare_exchange_strong (
            pred_expected, make_link (curr_link->succ, false));
        return true;
      }
  }

  bool
  contains (int key)
  {
    NodePtr curr = head_->next.load ()->succ;
    while (curr && curr->key < key)
      curr = curr->next.load ()->succ;
    return curr && curr->key == key && !curr->next.load ()->marked;
  }

  /// The members in order; false (and why) when the list is not sorted or
  /// reaches a destroyed object.
  bool
  consistent (std::string &why, std::vector<int> *members = nullptr)
  {
    int last = -1;
    NodePtr curr = head_->next.load ()->succ;
    int guard = 0;
    while (curr)
      {
        if (!curr->entry.intact ())
          {
            why = "the list reaches a destroyed node";
            return false;
          }
        LinkPtr link = curr->next.load ();
        if (!link || !link->entry.intact ())
          {
            why = "the list reaches a destroyed link";
            return false;
          }
        if (curr->key <= last)
          {
            why = "the keys are not strictly increasing";
            return false;
          }
        last = curr->key;
        if (members != nullptr && !link->marked)
          members->push_back (curr->key);
        curr = link->succ;
        if (++guard > 100000)
          {
            why = "the list does not end";
            return false;
          }
      }
    return true;
  }

private:
  LinkPtr
  make_link (NodePtr succ, bool marked)
  {
    return lumex_test::make_pooled<Link> (pool_, ledger_, std::move (succ),
                                          marked);
  }

  Position
  find (int key)
  {
    for (;;)
      {
        NodePtr pred = head_;
        LinkPtr pred_link = pred->next.load ();
        bool restart = false;
        while (!restart)
          {
            NodePtr curr = pred_link->succ;
            if (!curr)
              {
                Position pos = { pred, pred_link, curr };
                return pos;
              }
            LinkPtr curr_link = curr->next.load ();
            if (curr_link->marked)
              {
                // curr is logically deleted: unlink it, or start over.
                LinkPtr fresh = make_link (curr_link->succ, false);
                LinkPtr expected = pred_link;
                if (!pred->next.compare_exchange_strong (expected, fresh))
                  restart = true;
                else
                  pred_link = fresh;
                continue;
              }
            if (curr->key >= key)
              {
                Position pos = { pred, pred_link, curr };
                return pos;
              }
            pred = curr;
            pred_link = curr_link;
          }
      }
  }

  lumex_test::ObjectLedger &ledger_;
  lumex_test::ReusePool &pool_;
  NodePtr head_;
};

/**
 * @brief Threads insert, remove and look up keys of a small range; for every
 * key the successful inserts minus the successful removes is 1 when the key
 * is in the final list and 0 otherwise, the list is sorted and every object
 * of it is intact, and the ledger balances.
 */
template <typename Engine>
void
list_stress (Params const &p, lumex_test::Verdict &verdict)
{
  int const key_count = 12;
  lumex_test::ObjectLedger ledger (node_capacity (p, 12));
  lumex_test::ReusePool pool;
  std::vector<std::vector<long>> net (
      static_cast<std::size_t> (p.threads),
      std::vector<long> (static_cast<std::size_t> (key_count), 0));
  std::vector<int> members;
  {
    SharedSortedList<Engine> list (ledger, pool);
    std::string const error = lumex_test::run_threads (
        p.threads,
        [&] (int t)
          {
            lumex_test::Schedule schedule (
                p.schedule, lumex_test::derive_seed (
                                p.seed, static_cast<std::uint64_t> (t), 16));
            std::vector<long> &mine = net[static_cast<std::size_t> (t)];
            for (int i = 0; i < p.iterations; ++i)
              {
                int const key = static_cast<int> (schedule.random ().below (
                    static_cast<std::uint32_t> (key_count)));
                switch (schedule.random ().below (3u))
                  {
                  case 0:
                    if (list.insert (key))
                      ++mine[static_cast<std::size_t> (key)];
                    break;
                  case 1:
                    if (list.remove (key))
                      --mine[static_cast<std::size_t> (key)];
                    break;
                  default:
                    list.contains (key);
                    break;
                  }
                schedule.point ();
              }
          });
    verdict.require (error.empty (), error);
    std::string why;
    verdict.require (list.consistent (why, &members), "list_stress: " + why);
    for (int key = 0; key < key_count; ++key)
      {
        long sum = 0;
        for (std::size_t t = 0; t < net.size (); ++t)
          sum += net[t][static_cast<std::size_t> (key)];
        bool const present = std::find (members.begin (), members.end (), key)
                             != members.end ();
        verdict.require (sum == (present ? 1 : 0),
                         "list_stress: key " + std::to_string (key) + " has "
                             + std::to_string (sum)
                             + " more successful inserts than removes but is "
                             + (present ? "in" : "not in") + " the list");
        verdict.require (list.contains (key) == present,
                         "list_stress: contains disagrees with the traversal "
                         "for key "
                             + std::to_string (key));
      }
  }
  Engine::quiesce ();
  check_balance (ledger, pool, verdict, "list_stress");
}

// --- linearizability of the structures
// ----------------------------------------------------------

/**
 * @brief Short concurrent histories of the stack (or the queue) are checked
 * against the sequential LIFO (FIFO) specification.
 * @tparam Engine The atomic engine.
 * @tparam Queue True for the queue, false for the stack.
 */
template <typename Engine, bool Queue>
void
structure_linearizable (Params const &p, lumex_test::Verdict &verdict)
{
  int const threads = std::min (3, std::max (2, p.threads));
  bool reported = false;
  for (int round = 0; round < p.iterations && !reported; ++round)
    {
      lumex_test::ObjectLedger ledger (64);
      lumex_test::ReusePool pool;
      lumex_test::HistoryRecorder recorder (threads);
      std::uint64_t const round_seed = lumex_test::derive_seed (
          p.seed, static_cast<std::uint64_t> (round), 13);
      {
        SharedStack<Engine> *stack = nullptr;
        SharedQueue<Engine> *queue = nullptr;
        std::unique_ptr<SharedStack<Engine>> stack_owner;
        std::unique_ptr<SharedQueue<Engine>> queue_owner;
        if (Queue)
          {
            queue_owner.reset (new SharedQueue<Engine> (ledger, pool));
            queue = queue_owner.get ();
          }
        else
          {
            stack_owner.reset (new SharedStack<Engine> (ledger, pool));
            stack = stack_owner.get ();
          }
        std::string const error = lumex_test::run_threads (
            threads,
            [&] (int t)
              {
                lumex_test::Schedule schedule (
                    p.schedule,
                    lumex_test::derive_seed (round_seed,
                                             static_cast<std::uint64_t> (t)));
                int const ops
                    = 3 + static_cast<int> (schedule.random ().below (2u));
                for (int i = 0; i < ops; ++i)
                  {
                    std::uint64_t const invoke = recorder.begin ();
                    if (schedule.random ().below (2u) == 0u)
                      {
                        int const value = t * 10 + i;
                        if (Queue)
                          queue->enqueue (value);
                        else
                          stack->push (value);
                        recorder.end (
                            t,
                            Queue
                                ? static_cast<int> (lumex_test::queue_enqueue)
                                : static_cast<int> (lumex_test::stack_push),
                            invoke, value, 0, 0, 0);
                      }
                    else
                      {
                        int out = -1;
                        bool const ok
                            = Queue ? queue->dequeue (out) : stack->pop (out);
                        recorder.end (
                            t,
                            Queue
                                ? static_cast<int> (lumex_test::queue_dequeue)
                                : static_cast<int> (lumex_test::stack_pop),
                            invoke, 0, 0, ok ? out : -1, 0);
                      }
                    schedule.point ();
                  }
              });
        verdict.require (error.empty (), error);
      }
      Engine::quiesce ();
      std::vector<lumex_test::HistoryOp> const history = recorder.history ();
      lumex_test::Linearizability result;
      std::string text;
      if (Queue)
        {
          result = lumex_test::is_linearizable<lumex_test::QueueModel> (
              history, lumex_test::QueueModel::state_type ());
          if (result == lumex_test::Linearizability::not_linearizable)
            text = lumex_test::describe_history<lumex_test::QueueModel> (
                history);
        }
      else
        {
          result = lumex_test::is_linearizable<lumex_test::StackModel> (
              history, lumex_test::StackModel::state_type ());
          if (result == lumex_test::Linearizability::not_linearizable)
            text = lumex_test::describe_history<lumex_test::StackModel> (
                history);
        }
      if (result == lumex_test::Linearizability::not_linearizable)
        {
          verdict.fail (std::string ("structure_linearizable: round ")
                        + std::to_string (round) + " of the "
                        + (Queue ? "queue" : "stack")
                        + " is not linearizable:" + text);
          reported = true;
        }
      check_balance (ledger, pool, verdict, "structure_linearizable");
    }
}
/**
 * @brief Short concurrent histories of the sorted list are checked against
 * the sequential specification of a set.
 */
template <typename Engine>
void
list_linearizable (Params const &p, lumex_test::Verdict &verdict)
{
  int const threads = std::min (3, std::max (2, p.threads));
  bool reported = false;
  for (int round = 0; round < p.iterations && !reported; ++round)
    {
      lumex_test::ObjectLedger ledger (128);
      lumex_test::ReusePool pool;
      lumex_test::HistoryRecorder recorder (threads);
      std::uint64_t const round_seed = lumex_test::derive_seed (
          p.seed, static_cast<std::uint64_t> (round), 17);
      {
        SharedSortedList<Engine> list (ledger, pool);
        std::string const error = lumex_test::run_threads (
            threads,
            [&] (int t)
              {
                lumex_test::Schedule schedule (
                    p.schedule,
                    lumex_test::derive_seed (round_seed,
                                             static_cast<std::uint64_t> (t)));
                int const ops
                    = 3 + static_cast<int> (schedule.random ().below (2u));
                for (int i = 0; i < ops; ++i)
                  {
                    int const key
                        = static_cast<int> (schedule.random ().below (3u));
                    int const kind
                        = static_cast<int> (schedule.random ().below (3u));
                    std::uint64_t const invoke = recorder.begin ();
                    bool result = false;
                    if (kind == lumex_test::set_insert)
                      result = list.insert (key);
                    else if (kind == lumex_test::set_remove)
                      result = list.remove (key);
                    else
                      result = list.contains (key);
                    recorder.end (t, kind, invoke, key, 0, result ? 1 : 0, 0);
                    schedule.point ();
                  }
              });
        verdict.require (error.empty (), error);
      }
      Engine::quiesce ();
      std::vector<lumex_test::HistoryOp> const history = recorder.history ();
      if (lumex_test::is_linearizable<lumex_test::SetModel> (history, 0)
          == lumex_test::Linearizability::not_linearizable)
        {
          verdict.fail (
              "list_linearizable: round " + std::to_string (round)
              + " is not linearizable:"
              + lumex_test::describe_history<lumex_test::SetModel> (history));
          reported = true;
        }
      check_balance (ledger, pool, verdict, "list_linearizable");
    }
}
} // namespace scenario
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_SCENARIOS_STRUCTURES_HPP

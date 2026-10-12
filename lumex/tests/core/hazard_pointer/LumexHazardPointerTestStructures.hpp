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
 * @file LumexHazardPointerTestStructures.hpp
 * @brief Lock-free data structures for the ABA tests of the hazard pointer
 * module: a Treiber stack, a Michael-Scott queue and a Michael list set, each
 * in two versions (protected by hazard pointers and not protected at all) over
 * an allocator that hands a freed address out again at once.
 * @details The ABA problem needs a node address that is freed and then
 * allocated again between a thread's read and its compare-and-swap.
 * `recycling_pool` makes that certain: freed blocks go on a stack, the next
 * allocation takes the newest one, and a test can ask for a particular block
 * (`prefer`). The structures take a policy: `hazard_policy` protects what a
 * thread is about to touch and retires removed nodes through the module;
 * `unprotected_policy` reads without protection and frees a removed node on
 * the spot, which is exactly the unsafe version the hazard pointers exist to
 * prevent. Each structure has a `hook` called inside the window between a
 * thread's last read and its compare-and-swap; a scripted test runs the
 * interfering operations from there on the same thread, which makes the ABA
 * interleaving deterministic, and a stress test uses it to yield at random.
 * The header has no GoogleTest dependency, so the plain fixtures (soak, thread
 * sanitizer) include it too.
 */
#ifndef LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRUCTURES_HPP
#define LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRUCTURES_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace lumex_hp_structures
{
namespace hp_ns = lumex::core::hazard_pointer;

// ---------------------------------------------------------------------------
// Randomness with a replayable seed
// ---------------------------------------------------------------------------

/// A small xorshift generator; the state is the seed, so a run replays.
class random_t
{
public:
  explicit random_t (std::uint64_t seed)
      : state_ (seed != 0 ? seed : 0x9E3779B97F4A7C15ull)
  {
  }

  std::uint64_t
  next ()
  {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return state_;
  }

  /// A number in [0, bound).
  unsigned
  below (unsigned bound)
  {
    return static_cast<unsigned> (next () % bound);
  }

private:
  std::uint64_t state_;
};

/// The seed of a run: LUMEX_HP_SEED, or the clock. Printed once, so a failing
/// run can be repeated with LUMEX_HP_SEED=<value>.
inline std::uint64_t
run_seed ()
{
  static std::uint64_t const seed = []
    {
      char const *text = std::getenv ("LUMEX_HP_SEED");
      std::uint64_t value
          = text != nullptr && *text != '\0'
                ? std::strtoull (text, nullptr, 10)
                : static_cast<std::uint64_t> (std::chrono::steady_clock::now ()
                                                  .time_since_epoch ()
                                                  .count ());
      std::fprintf (stderr, "LUMEX_HP_SEED=%llu\n",
                    static_cast<unsigned long long> (value));
      return value;
    }();
  return seed;
}

// ---------------------------------------------------------------------------
// An allocator that gives the same address out again at once
// ---------------------------------------------------------------------------

/// Blocks of one node type. `destroy` runs the destructor and puts the block
/// on top of the free stack; `create` takes the block on top first, so a
/// freed address is allocated again by the very next `create`. The memory is
/// only returned when the pool dies, so a stale pointer reads another node
/// instead of crashing: the ABA problem shows up as wrong results.
template <class Node> class recycling_pool
{
public:
  recycling_pool () : live_ (0), reused_ (0) {}

  recycling_pool (recycling_pool const &) = delete;
  recycling_pool &operator= (recycling_pool const &) = delete;

  ~recycling_pool ()
  {
    for (std::size_t i = 0; i < blocks_.size (); ++i)
      {
        ::operator delete (blocks_[i]);
      }
  }

  template <class... Args>
  Node *
  create (Args &&...args)
  {
    void *memory = take ();
    return new (memory) Node (std::forward<Args> (args)...);
  }

  void
  destroy (Node *node)
  {
    node->~Node ();
    std::lock_guard<std::mutex> guard (lock_);
    free_.push_back (node);
    live_.fetch_sub (1);
  }

  /// Moves the freed block at `address` to the top of the free stack, so the
  /// next `create` returns it. False when that block is not free.
  bool
  prefer (void const *address)
  {
    std::lock_guard<std::mutex> guard (lock_);
    for (std::size_t i = 0; i < free_.size (); ++i)
      {
        if (free_[i] == address)
          {
            free_.erase (free_.begin () + static_cast<std::ptrdiff_t> (i));
            free_.push_back (const_cast<void *> (address));
            return true;
          }
      }
    return false;
  }

  /// Nodes created and not yet destroyed.
  long
  live () const
  {
    return live_.load ();
  }

  /// How many `create` calls got a block that had been freed before.
  long
  reused () const
  {
    return reused_.load ();
  }

private:
  void *
  take ()
  {
    std::lock_guard<std::mutex> guard (lock_);
    live_.fetch_add (1);
    if (!free_.empty ())
      {
        void *block = free_.back ();
        free_.pop_back ();
        reused_.fetch_add (1);
        return block;
      }
    void *block = ::operator new (sizeof (Node));
    blocks_.push_back (block);
    return block;
  }

  std::mutex lock_;
  std::vector<void *> free_;
  std::vector<void *> blocks_;
  std::atomic<long> live_;
  std::atomic<long> reused_;
};

/// The deleter of the hazard-protected nodes: gives the block back to the
/// pool.
template <class Node> struct pool_deleter
{
  recycling_pool<Node> *pool;

  pool_deleter () : pool (nullptr) {}
  explicit pool_deleter (recycling_pool<Node> *the_pool) : pool (the_pool) {}

  void
  operator() (Node *node) const
  {
    pool->destroy (node);
  }
};

/// A hook called inside the ABA window of a structure.
typedef std::function<void (int)> hook_t;

// ---------------------------------------------------------------------------
// The two policies
// ---------------------------------------------------------------------------

/// Hazard-pointer protection: what a thread touches is announced, and a
/// removed node is retired, so its block is reused only when nobody holds it.
struct hazard_policy
{
  static bool const protects = true;
  /// Rounds after which a retry loop gives up (a protected structure never
  /// comes near it).
  static std::size_t const give_up = 5000000;
  static std::size_t const walk_limit = 5000000;

  class guard
  {
  public:
    guard () : holder_ (hp_ns::make_hazard_pointer ()) {}

    template <class T>
    bool
    try_protect (T *&ptr, std::atomic<T *> const &source)
    {
      return holder_.try_protect (ptr, source);
    }

    template <class T, class Source, class Filter>
    bool
    try_protect (T *&ptr, Source const &source, Filter filter)
    {
      return holder_.try_protect (ptr, source, filter);
    }

    template <class T>
    void
    set (T const *ptr)
    {
      holder_.reset_protection (ptr);
    }

    void
    clear ()
    {
      holder_.reset_protection ();
    }

    void
    swap (guard &other)
    {
      holder_.swap (other.holder_);
    }

  private:
    hp_ns::hazard_pointer holder_;
  };

  template <class Node>
  static void
  retire (Node *node, recycling_pool<Node> &pool)
  {
    node->retire (pool_deleter<Node> (&pool));
  }

  static void
  settle ()
  {
    hp_ns::clean_up ();
  }
};

/// No protection at all: loads are plain, and a removed node is freed at once.
/// This is the version that suffers from ABA.
struct unprotected_policy
{
  static bool const protects = false;
  /// The unprotected versions can corrupt themselves into endless loops: they
  /// give up early.
  static std::size_t const give_up = 200;
  /// Nodes a search may pass (the lists of the tests hold at most 64).
  static std::size_t const walk_limit = 128;

  class guard
  {
  public:
    template <class T>
    bool
    try_protect (T *&ptr, std::atomic<T *> const &source)
    {
      T *current = source.load (std::memory_order_acquire);
      bool const same = current == ptr;
      ptr = current;
      return same;
    }

    template <class T, class Source, class Filter>
    bool
    try_protect (T *&ptr, Source const &source, Filter filter)
    {
      T *current = filter (source ());
      bool const same = current == ptr;
      ptr = current;
      return same;
    }

    template <class T>
    void
    set (T const *)
    {
    }
    void
    clear ()
    {
    }
    void
    swap (guard &)
    {
    }
  };

  template <class Node>
  static void
  retire (Node *node, recycling_pool<Node> &pool)
  {
    pool.destroy (node);
  }

  static void
  settle ()
  {
  }
};

// ---------------------------------------------------------------------------
// The Treiber stack
// ---------------------------------------------------------------------------

template <class Policy> class treiber_stack_t
{
public:
  typedef typename Policy::guard guard_type;

  /// Points of the hook.
  enum hook_point
  {
    /// `pop` read the next node and is about to swing the head.
    pop_window = 1
  };

  struct node_t : hp_ns::hazard_pointer_obj_base<node_t, pool_deleter<node_t>>
  {
    std::atomic<node_t *> next;
    long value;

    explicit node_t (long the_value) : next (nullptr), value (the_value) {}
  };

  treiber_stack_t () : head_ (nullptr) {}

  ~treiber_stack_t ()
  {
    guard_type guard;
    long value = 0;
    // Bounded: a corrupted (unprotected) stack can be a cycle.
    for (std::size_t i = 0; i < 100000 && pop (guard, value); ++i)
      {
      }
    Policy::settle ();
  }

  void
  push (long value)
  {
    node_t *node = pool.create (value);
    node_t *top = head_.load (std::memory_order_relaxed);
    do
      {
        node->next.store (top, std::memory_order_relaxed);
      }
    while (!head_.compare_exchange_weak (top, node, std::memory_order_release,
                                         std::memory_order_relaxed));
  }

  /// False when the stack is empty.
  bool
  pop (guard_type &guard, long &value)
  {
    node_t *top = head_.load (std::memory_order_acquire);
    for (std::size_t round = 0; round < Policy::give_up; ++round)
      {
        if (!guard.try_protect (top, head_))
          {
            continue;
          }
        if (top == nullptr)
          {
            guard.clear ();
            return false;
          }
        node_t *next = top->next.load (std::memory_order_acquire);
        if (hook)
          {
            hook (pop_window);
          }
        if (head_.compare_exchange_strong (top, next,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
          {
            value = top->value;
            guard.clear ();
            Policy::retire (top, pool);
            return true;
          }
      }
    guard.clear ();
    return false;
  }

  /// The values from the top down; for a quiescent stack.
  std::vector<long>
  contents () const
  {
    std::vector<long> values;
    for (node_t *node = head_.load (); node != nullptr;
         node = node->next.load ())
      {
        values.push_back (node->value);
        if (values.size () > 1000000)
          {
            break; // a cycle made by ABA
          }
      }
    return values;
  }

  void const *
  top_address () const
  {
    return head_.load ();
  }

  recycling_pool<node_t> pool;
  hook_t hook;

private:
  std::atomic<node_t *> head_;
};

// ---------------------------------------------------------------------------
// The Michael-Scott queue
// ---------------------------------------------------------------------------

template <class Policy> class ms_queue_t
{
public:
  typedef typename Policy::guard guard_type;

  enum hook_point
  {
    /// `dequeue` read the value and is about to swing the head.
    dequeue_window = 1
  };

  struct node_t : hp_ns::hazard_pointer_obj_base<node_t, pool_deleter<node_t>>
  {
    std::atomic<node_t *> next;
    long value;

    explicit node_t (long the_value) : next (nullptr), value (the_value) {}
  };

  ms_queue_t ()
  {
    node_t *dummy = pool.create (0);
    head_.store (dummy);
    tail_.store (dummy);
  }

  ~ms_queue_t ()
  {
    guard_type first;
    guard_type second;
    long value = 0;
    for (std::size_t i = 0; i < 100000 && dequeue (first, second, value); ++i)
      {
      }
    Policy::settle ();
    // The dummy that is left.
    pool.destroy (head_.load ());
  }

  void
  enqueue (guard_type &guard, long value)
  {
    node_t *node = pool.create (value);
    for (std::size_t round = 0; round < Policy::give_up; ++round)
      {
        node_t *tail = tail_.load (std::memory_order_acquire);
        if (!guard.try_protect (tail, tail_))
          {
            continue;
          }
        node_t *next = tail->next.load (std::memory_order_acquire);
        if (next != nullptr)
          {
            // The tail lags behind: help it forward.
            tail_.compare_exchange_strong (tail, next,
                                           std::memory_order_acq_rel);
            continue;
          }
        node_t *expected = nullptr;
        if (tail->next.compare_exchange_strong (expected, node,
                                                std::memory_order_acq_rel))
          {
            tail_.compare_exchange_strong (tail, node,
                                           std::memory_order_acq_rel);
            guard.clear ();
            return;
          }
      }
    guard.clear ();
  }

  /// Enqueues without the caller providing a guard.
  void
  enqueue_unguarded (long value)
  {
    guard_type guard;
    enqueue (guard, value);
  }

  /// False when the queue is empty. `first` protects the head, `second` the
  /// node after it.
  bool
  dequeue (guard_type &first, guard_type &second, long &value)
  {
    for (std::size_t round = 0; round < Policy::give_up; ++round)
      {
        node_t *head = head_.load (std::memory_order_acquire);
        if (!first.try_protect (head, head_))
          {
            continue;
          }
        node_t *tail = tail_.load (std::memory_order_acquire);
        node_t *next = head->next.load (std::memory_order_acquire);
        if (!second.try_protect (next, head->next))
          {
            continue;
          }
        if (head_.load (std::memory_order_acquire) != head)
          {
            continue;
          }
        if (next == nullptr)
          {
            first.clear ();
            second.clear ();
            return false;
          }
        if (head == tail)
          {
            tail_.compare_exchange_strong (tail, next,
                                           std::memory_order_acq_rel);
            continue;
          }
        long const candidate = next->value;
        if (hook)
          {
            hook (dequeue_window);
          }
        if (head_.compare_exchange_strong (head, next,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
          {
            value = candidate;
            first.clear ();
            second.clear ();
            Policy::retire (head, pool);
            return true;
          }
      }
    first.clear ();
    second.clear ();
    return false;
  }

  /// The values from the front; for a quiescent queue.
  std::vector<long>
  contents () const
  {
    std::vector<long> values;
    node_t *head = head_.load ();
    for (node_t *node = head->next.load (); node != nullptr;
         node = node->next.load ())
      {
        values.push_back (node->value);
        if (values.size () > 1000000)
          {
            break;
          }
      }
    return values;
  }

  void const *
  head_address () const
  {
    return head_.load ();
  }

  recycling_pool<node_t> pool;
  hook_t hook;

private:
  std::atomic<node_t *> head_;
  std::atomic<node_t *> tail_;
};

// ---------------------------------------------------------------------------
// The Michael list set (a sorted lock-free list with marked links)
// ---------------------------------------------------------------------------

template <class Policy> class michael_list_t
{
public:
  typedef typename Policy::guard guard_type;

  enum hook_point
  {
    /// `insert` found its place and is about to link the new node.
    insert_window = 1
  };

  typedef std::atomic<std::uintptr_t> cell_t;

  struct node_t : hp_ns::hazard_pointer_obj_base<node_t, pool_deleter<node_t>>
  {
    long key;
    cell_t next;

    explicit node_t (long the_key) : key (the_key), next (0) {}
  };

  michael_list_t () : head_ (0) {}

  ~michael_list_t ()
  {
    Policy::settle ();
    std::uintptr_t word = head_.load ();
    for (std::size_t i = 0; i < 100000 && pointer_of (word) != nullptr; ++i)
      {
        node_t *node = pointer_of (word);
        word = node->next.load ();
        pool.destroy (node);
      }
  }

  /// False when the key was there already.
  bool
  insert (long key)
  {
    guard_type owner, current, following;
    node_t *node = pool.create (key);
    for (std::size_t round = 0; round < Policy::give_up; ++round)
      {
        cell_t *previous = nullptr;
        node_t *found = nullptr;
        std::uintptr_t next_word = 0;
        if (find (key, owner, current, following, previous, found, next_word))
          {
            pool.destroy (node); // never published
            return false;
          }
        node->next.store (reinterpret_cast<std::uintptr_t> (found),
                          std::memory_order_relaxed);
        if (hook)
          {
            hook (insert_window);
          }
        std::uintptr_t expected = reinterpret_cast<std::uintptr_t> (found);
        if (previous->compare_exchange_strong (
                expected, reinterpret_cast<std::uintptr_t> (node),
                std::memory_order_acq_rel))
          {
            return true;
          }
      }
    return false;
  }

  /// False when the key was not there.
  bool
  remove (long key)
  {
    guard_type owner, current, following;
    for (std::size_t round = 0; round < Policy::give_up; ++round)
      {
        cell_t *previous = nullptr;
        node_t *found = nullptr;
        std::uintptr_t next_word = 0;
        if (!find (key, owner, current, following, previous, found, next_word))
          {
            return false;
          }
        // Mark the node as deleted, then unlink it; whoever unlinks retires.
        if (!found->next.compare_exchange_strong (next_word, next_word | 1,
                                                  std::memory_order_acq_rel))
          {
            continue;
          }
        std::uintptr_t expected = reinterpret_cast<std::uintptr_t> (found);
        if (previous->compare_exchange_strong (expected, next_word,
                                               std::memory_order_acq_rel))
          {
            Policy::retire (found, pool);
          }
        else
          {
            // Somebody else changed the link: a search unlinks the node.
            cell_t *p2 = nullptr;
            node_t *f2 = nullptr;
            std::uintptr_t n2 = 0;
            find (key, owner, current, following, p2, f2, n2);
          }
        return true;
      }
    return false;
  }

  bool
  contains (long key)
  {
    guard_type owner, current, following;
    cell_t *previous = nullptr;
    node_t *found = nullptr;
    std::uintptr_t next_word = 0;
    return find (key, owner, current, following, previous, found, next_word);
  }

  /// The keys in order; for a quiescent list.
  std::vector<long>
  keys () const
  {
    std::vector<long> result;
    std::uintptr_t word = head_.load ();
    while (pointer_of (word) != nullptr)
      {
        node_t *node = pointer_of (word);
        word = node->next.load ();
        if ((word & 1) == 0)
          {
            result.push_back (node->key);
          }
        if (result.size () > 1000000)
          {
            break;
          }
      }
    return result;
  }

  void const *
  first_address () const
  {
    return pointer_of (head_.load ());
  }

  /// The addresses of the nodes in order; for a quiescent list.
  std::vector<void const *>
  addresses () const
  {
    std::vector<void const *> result;
    std::uintptr_t word = head_.load ();
    while (pointer_of (word) != nullptr && result.size () < 1000000)
      {
        node_t *node = pointer_of (word);
        result.push_back (node);
        word = node->next.load ();
      }
    return result;
  }

  recycling_pool<node_t> pool;
  hook_t hook;

private:
  static node_t *
  pointer_of (std::uintptr_t word)
  {
    return reinterpret_cast<node_t *> (word & ~std::uintptr_t (1));
  }

  struct strip
  {
    node_t *
    operator() (std::uintptr_t word) const
    {
      return pointer_of (word);
    }
  };

  struct read_cell
  {
    cell_t const *cell;

    std::uintptr_t
    operator() () const
    {
      return cell->load (std::memory_order_acquire);
    }
  };

  // Finds the first node whose key is not below `key`. `previous` is the cell
  // that points to it, `found` the node (null at the end), `next_word` its
  // link. Unlinks and retires the marked nodes it passes. `owner` protects the
  // node that holds `previous`, `current` the found node, `following` its
  // successor.
  bool
  find (long key, guard_type &owner, guard_type &current,
        guard_type &following, cell_t *&previous, node_t *&found,
        std::uintptr_t &next_word)
  {
    std::size_t steps = 0;
  restart:
    previous = &head_;
    found = pointer_of (previous->load (std::memory_order_acquire));
    {
      read_cell source = { previous };
      while (!current.try_protect (found, source, strip ()))
        {
        }
    }
    for (;;)
      {
        if (found == nullptr || ++steps > Policy::walk_limit)
          {
            if (found != nullptr)
              {
                // Gave up inside a corrupted list: report the end.
                previous = &head_;
                found = pointer_of (head_.load ());
              }
            next_word = 0;
            return false;
          }
        std::uintptr_t word;
        node_t *after;
        for (;;)
          {
            word = found->next.load (std::memory_order_acquire);
            after = pointer_of (word);
            read_cell source = { &found->next };
            if (!following.try_protect (after, source, strip ()))
              {
                continue;
              }
            word = found->next.load (std::memory_order_acquire);
            if (pointer_of (word) == after)
              {
                break;
              }
          }
        // The node must still be linked where we found it.
        if (previous->load (std::memory_order_acquire)
            != reinterpret_cast<std::uintptr_t> (found))
          {
            ++steps;
            goto restart;
          }
        if ((word & 1) == 0)
          {
            if (found->key >= key)
              {
                next_word = word;
                return found->key == key;
              }
            previous = &found->next;
            owner.swap (current);
          }
        else
          {
            std::uintptr_t expected = reinterpret_cast<std::uintptr_t> (found);
            if (previous->compare_exchange_strong (
                    expected, reinterpret_cast<std::uintptr_t> (after),
                    std::memory_order_acq_rel))
              {
                Policy::retire (found, pool);
              }
            else
              {
                ++steps;
                goto restart;
              }
          }
        found = after;
        current.swap (following);
      }
  }

  cell_t head_;
};
} // namespace lumex_hp_structures

#endif // !LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_STRUCTURES_HPP

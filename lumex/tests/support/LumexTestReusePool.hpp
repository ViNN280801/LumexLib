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
 * @file LumexTestReusePool.hpp
 * @brief An allocator that hands the SAME address out again at once, for ABA
 * tests.
 * @details `ReusePool` keeps a last-in-first-out free list per block size, so
 * the block that was released last is the block the next allocation of that
 * size returns, from whatever thread. A test that builds its objects with
 * `std::allocate_shared (ReuseAllocator<T> (pool), ...)` therefore gets the
 * address of a just-destroyed object (and of its control block) back for the
 * next object: the schedule every ABA bug of a lock-free pointer structure
 * needs, produced on demand instead of by luck.
 *
 * The blocks belong to the pool for its whole life. A block on the free list
 * is filled with a poison pattern and, under AddressSanitizer, poisoned, so a
 * stale pointer that is read after the release is reported by the sanitizer
 * (or reads the pattern) instead of silently reading the old object; an
 * allocation unpoisons it again. `set_hook ()` installs a callback for the
 * four events (before and after an allocation or a release), called outside
 * the pool lock, which a test uses to count, record or stall a thread at the
 * allocator boundary. `set_poisoning (false)` turns the poison off for a
 * deliberately unsafe fixture that must run without a sanitizer report.
 *
 * The pool never calls the code under test; it only needs to outlive every
 * block it handed out (`live () == 0` before it is destroyed, which
 * `~ReusePool ()` does not require but `leaked ()` reports).
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_REUSE_POOL_HPP
#define LUMEX_TESTS_SUPPORT_TEST_REUSE_POOL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

#include "lumex/tests/support/LumexTestConfig.hpp"

#if LUMEX_TEST_HAS_ASAN
// The two entry points of <sanitizer/asan_interface.h>, declared here so the
// header needs no platform include after the project one.
extern "C" void __asan_poison_memory_region (void const volatile *address,
                                             std::size_t size);
extern "C" void __asan_unpoison_memory_region (void const volatile *address,
                                               std::size_t size);
#define LUMEX_TEST_ASAN_POISON(address, size)                                 \
  __asan_poison_memory_region ((address), (size))
#define LUMEX_TEST_ASAN_UNPOISON(address, size)                               \
  __asan_unpoison_memory_region ((address), (size))
#else
#define LUMEX_TEST_ASAN_POISON(address, size) ((void)(address), (void)(size))
#define LUMEX_TEST_ASAN_UNPOISON(address, size) ((void)(address), (void)(size))
#endif

namespace lumex_test
{
/// The four moments the pool reports to its hook.
enum class PoolEvent
{
  before_allocate,
  after_allocate,
  before_release,
  after_release
};

/// Byte that a released block is filled with.
enum : int
{
  pool_poison_byte = 0xDD
};

class ReusePool
{
public:
  /// Hook signature: the event and the address involved (null before an
  /// allocation).
  typedef std::function<void (PoolEvent, void *)> hook_type;

  ReusePool ()
      : allocations_ (0), releases_ (0), reuses_ (0), live_ (0), poison_ (true)
  {
  }

  ReusePool (ReusePool const &) = delete;
  ReusePool &operator= (ReusePool const &) = delete;

  ~ReusePool ()
  {
    for (std::size_t i = 0; i < blocks_.size (); ++i)
      {
        LUMEX_TEST_ASAN_UNPOISON (blocks_[i].address, blocks_[i].size);
        ::operator delete (blocks_[i].address);
      }
  }

  /// Installs @p hook (an empty function removes it). Call it before the
  /// threads start.
  void
  set_hook (hook_type hook)
  {
    hook_ = std::move (hook);
  }

  /// Turns the poisoning of released blocks on (default) or off.
  void
  set_poisoning (bool on)
  {
    poison_ = on;
  }

  /// Allocates @p size bytes; returns the most recently released block of
  /// exactly that size when there is one.
  void *
  allocate (std::size_t size)
  {
    notify (PoolEvent::before_allocate, nullptr);
    void *address = nullptr;
    bool reused = false;
    {
      std::lock_guard<std::mutex> guard (mutex_);
      std::vector<void *> &free_list = free_[size];
      if (!free_list.empty ())
        {
          address = free_list.back ();
          free_list.pop_back ();
          reused = true;
        }
      else
        {
          address = ::operator new (size);
          Block block = { address, size };
          blocks_.push_back (block);
        }
      allocations_.fetch_add (1, std::memory_order_relaxed);
      live_.fetch_add (1, std::memory_order_relaxed);
      if (reused)
        {
          reuses_.fetch_add (1, std::memory_order_relaxed);
          LUMEX_TEST_ASAN_UNPOISON (address, size);
        }
    }
    notify (PoolEvent::after_allocate, address);
    return address;
  }

  /// Releases a block of @p size bytes obtained from `allocate`.
  void
  release (void *address, std::size_t size)
  {
    notify (PoolEvent::before_release, address);
    if (poison_)
      std::memset (address, static_cast<int> (pool_poison_byte), size);
    {
      std::lock_guard<std::mutex> guard (mutex_);
      free_[size].push_back (address);
      releases_.fetch_add (1, std::memory_order_relaxed);
      live_.fetch_sub (1, std::memory_order_relaxed);
      if (poison_)
        LUMEX_TEST_ASAN_POISON (address, size);
    }
    notify (PoolEvent::after_release, address);
  }

  /// Number of allocations so far.
  long
  allocations () const
  {
    return allocations_.load (std::memory_order_relaxed);
  }

  /// Number of releases so far.
  long
  releases () const
  {
    return releases_.load (std::memory_order_relaxed);
  }

  /// Allocations that returned an address released earlier.
  long
  reuses () const
  {
    return reuses_.load (std::memory_order_relaxed);
  }

  /// Blocks allocated and not yet released.
  long
  live () const
  {
    return live_.load (std::memory_order_relaxed);
  }

  /// Same as `live ()`: blocks that were never released.
  long
  leaked () const
  {
    return live ();
  }

private:
  struct Block
  {
    void *address;
    std::size_t size;
  };

  void
  notify (PoolEvent event, void *address)
  {
    if (hook_)
      hook_ (event, address);
  }

  std::mutex mutex_;
  std::map<std::size_t, std::vector<void *>> free_;
  std::vector<Block> blocks_;
  std::atomic<long> allocations_;
  std::atomic<long> releases_;
  std::atomic<long> reuses_;
  std::atomic<long> live_;
  bool poison_;
  hook_type hook_;
};

/**
 * @brief Standard allocator over a `ReusePool`, for `std::allocate_shared`
 * and containers.
 */
template <typename T> class ReuseAllocator
{
public:
  typedef T value_type;

  explicit ReuseAllocator (ReusePool &pool) : pool_ (&pool) {}

  template <typename U>
  ReuseAllocator (ReuseAllocator<U> const &other) : pool_ (other.pool ())
  {
  }

  T *
  allocate (std::size_t n)
  {
    return static_cast<T *> (pool_->allocate (n * sizeof (T)));
  }

  void
  deallocate (T *address, std::size_t n)
  {
    pool_->release (address, n * sizeof (T));
  }

  /// The pool behind this allocator.
  ReusePool *
  pool () const
  {
    return pool_;
  }

private:
  ReusePool *pool_;
};

template <typename T, typename U>
bool
operator== (ReuseAllocator<T> const &lhs, ReuseAllocator<U> const &rhs)
{
  return lhs.pool () == rhs.pool ();
}

template <typename T, typename U>
bool
operator!= (ReuseAllocator<T> const &lhs, ReuseAllocator<U> const &rhs)
{
  return !(lhs == rhs);
}

/// A shared object whose object and control block come from @p pool.
template <typename T, typename... Args>
std::shared_ptr<T>
make_pooled (ReusePool &pool, Args &&...args)
{
  return std::allocate_shared<T> (ReuseAllocator<T> (pool),
                                  std::forward<Args> (args)...);
}
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_REUSE_POOL_HPP

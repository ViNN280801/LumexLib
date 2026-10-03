/**
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
 * @file LumexBitLock.hpp
 * @brief A lock held in one 32-bit word: one bit marks it as taken, a second
 * bit records that a thread sleeps on it.
 * @details This is the spin lock of the lock-based `atomic_shared_ptr` and
 * `atomic_weak_ptr`, ported from the lock-based method of the author's own
 * libc++ implementation of `std::atomic<std::shared_ptr<T>>` (P0718R2,
 * llvm-project pull request 194215; that implementation also has a lock-free
 * method, which is not ported). One change: libc++ can keep the two bits in
 * the low bits of its own control block pointer, while this library works
 * over the `std::shared_ptr` of any standard library, whose layout it must
 * not touch, so the bits live in a word of their own.
 *
 * A thread that finds the lock taken spins briefly (12 pause instructions,
 * then 4 yields, the spin policy of libstdc++'s `std::atomic::wait`), then
 * sets the sleeper bit and sleeps until the word changes. The spin is this
 * port's addition: the libc++ method parks at once in libc++'s global wait
 * table, while sleeping on the wait table of a C++11 build costs a mutex and
 * a condition variable. The unlocking thread clears both bits in one
 * exchange and, if the sleeper bit was set, wakes every sleeper. The woken
 * threads race for the lock again; a thread that loses sets the sleeper bit
 * again before it goes back to sleep. Sleeping without setting the bit again
 * is the lost wake-up that the libc++ pull request had to fix: the next unlock
 * would see no sleeper bit and wake nobody.
 *
 * The protected sections of the atomic smart pointers only move or copy a
 * smart pointer, so the lock is held briefly; sleeping matters when the
 * holder is preempted.
 *
 * Under ThreadSanitizer the lock is annotated as a mutex, so the sanitizer
 * orders the protected accesses through the lock and can report lock order
 * inversions that involve it.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SYNC_BIT_LOCK_HPP
#define LUMEX_CORE_ATOMIC_SYNC_BIT_LOCK_HPP

#include <atomic>
#include <cstdint>
#include <thread>

#if defined(__SANITIZE_THREAD__)
#include <sanitizer/tsan_interface.h>
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#include <sanitizer/tsan_interface.h>
#endif
#endif

#include "lumex/core/atomic/sync/LumexAtomicWait.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__SANITIZE_THREAD__)
#define LUMEX_ATOMIC_BIT_LOCK_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_ATOMIC_BIT_LOCK_TSAN 1
#endif
#endif

namespace lumex
{
namespace core
{
namespace atomic
{
namespace sync
{
inline namespace LUMEX_ATOMIC_WAIT_ABI_NAMESPACE
{
namespace Detail
{
/**
 * @brief ThreadSanitizer annotations of the lock word; no-ops in other
 * builds.
 * @param address Address of the lock word.
 */
inline void
tsan_mutex_pre_lock (void *address) LUMEX_NOEXCEPT
{
#if defined(LUMEX_ATOMIC_BIT_LOCK_TSAN)
  __tsan_mutex_pre_lock (address, 0);
#else
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (address);
#endif
}

/// @copydoc tsan_mutex_pre_lock
inline void
tsan_mutex_post_lock (void *address) LUMEX_NOEXCEPT
{
#if defined(LUMEX_ATOMIC_BIT_LOCK_TSAN)
  __tsan_mutex_post_lock (address, 0, 0);
#else
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (address);
#endif
}

/// @copydoc tsan_mutex_pre_lock
inline void
tsan_mutex_pre_unlock (void *address) LUMEX_NOEXCEPT
{
#if defined(LUMEX_ATOMIC_BIT_LOCK_TSAN)
  __tsan_mutex_pre_unlock (address, 0);
#else
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (address);
#endif
}

/// @copydoc tsan_mutex_pre_lock
inline void
tsan_mutex_post_unlock (void *address) LUMEX_NOEXCEPT
{
#if defined(LUMEX_ATOMIC_BIT_LOCK_TSAN)
  __tsan_mutex_post_unlock (address, 0);
#else
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (address);
#endif
}

/// @copydoc tsan_mutex_pre_lock
inline void
tsan_mutex_destroy (void *address) LUMEX_NOEXCEPT
{
#if defined(LUMEX_ATOMIC_BIT_LOCK_TSAN)
  __tsan_mutex_destroy (address, 0);
#else
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (address);
#endif
}

/**
 * @brief Lock in one 32-bit word that puts contended threads to sleep.
 * @details `lock ()` and `unlock ()` are `const`, so a `const` member
 * function of the owner (such as `load ()`) can take the lock.
 */
class BitLock
{
public:
  /**
   * @brief Bits of the lock word.
   */
  enum : std::uint32_t
  {
    /// The lock is taken.
    lock_bit = 1u,
    /// At least one thread sleeps until the lock is released. Set only
    /// while `lock_bit` is set.
    notify_bit = 2u
  };

  /**
   * @brief Creates a released lock.
   */
  LUMEX_CONSTEXPR
  BitLock () LUMEX_NOEXCEPT : word_ (0u) {}

  /**
   * @brief Destroys the lock. It must not be taken.
   */
  ~BitLock () { tsan_mutex_destroy (static_cast<void *> (&word_)); }

  BitLock (BitLock const &) = delete;
  BitLock &operator= (BitLock const &) = delete;

  /**
   * @brief Takes the lock, sleeping while another thread holds it.
   * @details Acquire ordering: the protected accesses of the previous holder
   * happen before those of the caller.
   */
  void
  lock () const LUMEX_NOEXCEPT
  {
    LUMEX_CONSTEXPR int relax_rounds = 12;
    LUMEX_CONSTEXPR int yield_rounds = 4;
    void *const address = static_cast<void *> (&word_);
    tsan_mutex_pre_lock (address);
    int round = 0;
    std::uint32_t state = word_.load (std::memory_order_relaxed);
    for (;;)
      {
        if ((state & lock_bit) == 0u)
          {
            if (word_.compare_exchange_weak (state, state | lock_bit,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed))
              break;
            continue;
          }
        if (round < relax_rounds + yield_rounds)
          {
            if (round < relax_rounds)
              cpu_relax ();
            else
              std::this_thread::yield ();
            ++round;
            state = word_.load (std::memory_order_relaxed);
            continue;
          }
        // Sleep only on a word that is taken and carries the sleeper bit:
        // the holder then wakes the sleepers when it releases the lock. The
        // bit is set again on every round, because each release clears it.
        if ((state & notify_bit) == 0u)
          {
            if (!word_.compare_exchange_weak (state, state | notify_bit,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed))
              continue;
            state |= notify_bit;
          }
        wait_until_changed (word_, state);
        state = word_.load (std::memory_order_relaxed);
      }
    tsan_mutex_post_lock (address);
  }

  /**
   * @brief Releases the lock and wakes the threads that sleep on it.
   * @details Release ordering. Both bits are cleared in one exchange, so the
   * woken threads and the threads that arrive later all see a released lock
   * without the sleeper bit.
   */
  void
  unlock () const LUMEX_NOEXCEPT
  {
    void *const address = static_cast<void *> (&word_);
    tsan_mutex_pre_unlock (address);
    std::uint32_t const previous
        = word_.exchange (0u, std::memory_order_release);
    if ((previous & notify_bit) != 0u)
      notify_all (word_);
    tsan_mutex_post_unlock (address);
  }

  /**
   * @brief Returns the current lock word (`lock_bit`, `notify_bit`).
   * @details For diagnostics and tests; the value may be stale as soon as it
   * is returned.
   */
  std::uint32_t
  state () const LUMEX_NOEXCEPT
  {
    return word_.load (std::memory_order_relaxed);
  }

private:
  mutable std::atomic<std::uint32_t> word_;
};

/**
 * @brief Holds a `BitLock` for the lifetime of the guard.
 */
class BitLockGuard
{
public:
  /**
   * @brief Takes @p lock.
   * @param lock The lock to hold; it must outlive the guard.
   */
  explicit BitLockGuard (BitLock const &lock) LUMEX_NOEXCEPT : lock_ (lock)
  {
    lock_.lock ();
  }

  /**
   * @brief Releases the lock.
   */
  ~BitLockGuard () { lock_.unlock (); }

  BitLockGuard (BitLockGuard const &) = delete;
  BitLockGuard &operator= (BitLockGuard const &) = delete;

private:
  BitLock const &lock_;
};

} // namespace Detail
} // namespace LUMEX_ATOMIC_WAIT_ABI_NAMESPACE
} // namespace sync
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SYNC_BIT_LOCK_HPP

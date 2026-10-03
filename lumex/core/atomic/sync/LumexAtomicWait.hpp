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
 * @file LumexAtomicWait.hpp
 * @brief Blocking wait for a change of a 32-bit atomic word, and the wake-up
 * that ends it, from C++11 on.
 * @details The lock-based atomic smart pointers of this module put threads to
 * sleep in two places: a thread that finds the internal lock taken, and a
 * thread inside `wait ()`. In both cases the thread waits until a 32-bit word
 * changes, and the thread that changes the word wakes it.
 *
 * From C++20 on, when the standard library defines `__cpp_lib_atomic_wait`
 * (`LUMEX_HAS_STD_ATOMIC_WAIT`), that is `std::atomic::wait` and
 * `std::atomic::notify_*`, which the standard
 * library maps to the address-keyed wait of the operating system (a futex on
 * Linux, `WaitOnAddress` on Windows).
 *
 * Before C++20 the standard has no address-keyed wait, so this header keeps a
 * table of 64 stripes, each a `std::mutex` with a `std::condition_variable`,
 * and hashes the address of the word to a stripe. A mutex and a condition
 * variable inside every atomic smart pointer would add about 90 bytes to each
 * object, and the constructors could no longer be `constexpr`, because the
 * constructor of `std::condition_variable` is not; `constinit` objects need
 * them. A shared table costs one mutex round trip per wake-up instead. Words
 * that hash to the same stripe share one condition variable, so a wake-up
 * reaches every sleeper of the stripe (`notify_one` acts as `notify_all`), and
 * each sleeper re-checks its own word and sleeps again if the word did not
 * change.
 *
 * The table is a function-local static of an inline function. On ELF and
 * Mach-O the function has default visibility, so one table serves the whole
 * process even when shared objects are built with hidden visibility. A Windows
 * DLL that instantiates the function gets a table of its own: before C++20, a
 * thread that sleeps through one DLL is not woken by a wake-up issued through
 * another DLL on the same word. Programs that share one atomic smart pointer
 * between DLLs should be built at C++20 or keep the operations on that object
 * in one module.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SYNC_ATOMIC_WAIT_HPP
#define LUMEX_CORE_ATOMIC_SYNC_ATOMIC_WAIT_HPP

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <thread>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @brief 1 when threads sleep through `std::atomic::wait`, 0 when they sleep
 * on the striped table.
 * @details `std::atomic::wait` is used when the standard library has it
 * (`LUMEX_HAS_STD_ATOMIC_WAIT`, `__cpp_lib_atomic_wait`). Defining
 * `LUMEX_ATOMIC_WAIT_FORCE_TABLE` before the first include selects the table
 * even then; it is meant for tests, which run the table at C++20 that way.
 */
#if defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)
#define LUMEX_ATOMIC_WAIT_USES_STD 0
#elif LUMEX_HAS_STD_ATOMIC_WAIT
#define LUMEX_ATOMIC_WAIT_USES_STD 1
#else
#define LUMEX_ATOMIC_WAIT_USES_STD 0
#endif

/**
 * @brief Name of the inline namespace that holds the waiting code.
 * @details The two ways of sleeping are not interchangeable: a thread that
 * sleeps on the table is not woken by `std::atomic::notify_all`. Each one
 * lives in its own inline namespace, so a program that mixes translation
 * units with different choices (for example built before and after C++20)
 * gets distinct symbols instead of two different definitions of one inline
 * function.
 */
#if LUMEX_ATOMIC_WAIT_USES_STD
#define LUMEX_ATOMIC_WAIT_ABI_NAMESPACE std_wait
#else
#define LUMEX_ATOMIC_WAIT_ABI_NAMESPACE table_wait
#endif

/**
 * @brief Visibility of the function that owns the waiting table.
 * @details Default visibility makes the dynamic linker merge the table of
 * every shared object into one, also when the objects are compiled with
 * `-fvisibility=hidden`. Windows has no such merge; see the file comment.
 */
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
#define LUMEX_ATOMIC_WAIT_TABLE_VISIBILITY                                    \
  __attribute__ ((visibility ("default")))
#else
#define LUMEX_ATOMIC_WAIT_TABLE_VISIBILITY
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
 * @brief Tells the processor that the calling thread is spinning.
 * @details On x86 this is the `pause` instruction, which makes a spin loop
 * cheaper for the other hardware thread of the core and for the memory bus.
 * Other targets give up the rest of the time slice instead.
 */
inline void
cpu_relax () LUMEX_NOEXCEPT
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  _mm_pause ();
#elif (defined(__GNUC__) || defined(__clang__))                               \
    && (defined(__i386__) || defined(__x86_64__))
  __builtin_ia32_pause ();
#else
  std::this_thread::yield ();
#endif
}

#if LUMEX_ATOMIC_WAIT_USES_STD

/**
 * @brief Blocks the calling thread while @p word holds @p old.
 * @details Returns once the word holds another value. A wake-up that finds
 * the word unchanged puts the thread back to sleep.
 * @param word The word to watch.
 * @param old The value the caller last saw.
 */
inline void
wait_until_changed (std::atomic<std::uint32_t> const &word,
                    std::uint32_t old) LUMEX_NOEXCEPT
{
  word.wait (old, std::memory_order_relaxed);
}

/**
 * @brief Wakes at least one thread sleeping in `wait_until_changed` on
 * @p word.
 * @param word The word whose value the caller has just changed.
 */
inline void
notify_one (std::atomic<std::uint32_t> &word) LUMEX_NOEXCEPT
{
  word.notify_one ();
}

/**
 * @brief Wakes every thread sleeping in `wait_until_changed` on @p word.
 * @param word The word whose value the caller has just changed.
 */
inline void
notify_all (std::atomic<std::uint32_t> &word) LUMEX_NOEXCEPT
{
  word.notify_all ();
}

#else // !LUMEX_ATOMIC_WAIT_USES_STD

/**
 * @brief One stripe of the waiting table: the sleepers of every word that
 * hashes to it share this mutex and condition variable.
 */
struct waiter_stripe_t
{
  waiter_stripe_t () : mutex (), condition () {}

  std::mutex mutex;
  std::condition_variable condition;
};

/**
 * @brief The whole waiting table.
 */
struct waiter_table_t
{
  /// Number of stripes.
  enum : std::size_t
  {
    stripe_count = 64
  };

  waiter_table_t () : stripes () {}

  std::array<waiter_stripe_t, stripe_count> stripes;
};

/**
 * @brief Returns the stripe that serves the word at @p address.
 * @details The table is built in static storage on first use and never
 * destroyed: a thread that still sleeps while the program exits never finds
 * a destroyed mutex, and no destructor runs at exit.
 * @param address Address of the watched word.
 * @return The stripe of that word.
 */
LUMEX_ATOMIC_WAIT_TABLE_VISIBILITY inline waiter_stripe_t &
stripe_for (void const *address) LUMEX_NOEXCEPT
{
  alignas (
      waiter_table_t) static unsigned char storage[sizeof (waiter_table_t)];
  static waiter_table_t *const table
      = ::new (static_cast<void *> (storage)) waiter_table_t ();
  // The words are at least four-byte aligned, so the two lowest bits carry
  // no information; the shifts mix the higher bits into the index.
  std::uintptr_t key = reinterpret_cast<std::uintptr_t> (address) >> 2;
  key ^= key >> 7;
  key ^= key >> 13;
  return table
      ->stripes[static_cast<std::size_t> (key % waiter_table_t::stripe_count)];
}

/**
 * @brief Blocks the calling thread while @p word holds @p old.
 * @details The value is re-read under the stripe mutex, and the waking side
 * changes the word before it takes that mutex, so a change made before the
 * thread falls asleep is never missed. Returns once the word holds another
 * value; wake-ups meant for other words of the stripe put the thread back to
 * sleep.
 * @param word The word to watch.
 * @param old The value the caller last saw.
 */
inline void
wait_until_changed (std::atomic<std::uint32_t> const &word,
                    std::uint32_t old) LUMEX_NOEXCEPT
{
  if (word.load (std::memory_order_relaxed) != old)
    return;
  waiter_stripe_t &stripe = stripe_for (&word);
  std::unique_lock<std::mutex> guard (stripe.mutex);
  while (word.load (std::memory_order_relaxed) == old)
    stripe.condition.wait (guard);
}

/**
 * @brief Wakes every thread sleeping in `wait_until_changed` on @p word.
 * @details Taking and releasing the stripe mutex before the broadcast orders
 * the change of the word before the re-check of every sleeper that has not
 * fallen asleep yet; the broadcast wakes the ones that have.
 * @param word The word whose value the caller has just changed.
 */
inline void
notify_all (std::atomic<std::uint32_t> &word) LUMEX_NOEXCEPT
{
  waiter_stripe_t &stripe = stripe_for (&word);
  {
    std::lock_guard<std::mutex> const guard (stripe.mutex);
  }
  stripe.condition.notify_all ();
}

/**
 * @brief Wakes at least one thread sleeping in `wait_until_changed` on
 * @p word.
 * @details The stripe is shared with other words, so a single wake-up could
 * reach a sleeper of another word and leave the sleepers of @p word asleep.
 * The call wakes the whole stripe instead.
 * @param word The word whose value the caller has just changed.
 */
inline void
notify_one (std::atomic<std::uint32_t> &word) LUMEX_NOEXCEPT
{
  notify_all (word);
}

#endif // LUMEX_ATOMIC_WAIT_USES_STD

} // namespace Detail
} // namespace LUMEX_ATOMIC_WAIT_ABI_NAMESPACE
} // namespace sync
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SYNC_ATOMIC_WAIT_HPP

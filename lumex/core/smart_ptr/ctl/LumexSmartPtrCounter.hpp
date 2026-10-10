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
 * @file LumexSmartPtrCounter.hpp
 * @brief `split_counter`: the packed reference counter of the control block,
 * one 64-bit atomic word `{count:32, ext:32}`.
 * @details The word is used for both groups of a control block (the strong
 * owners and the weak ledger). `count` (the low half) is the number of
 * references that were taken by an ordinary addition. `ext` (the high half,
 * two's complement) counts the owners in transit: the units that the
 * split-count engine (`core/atomic`) pre-transfers for the readers of an
 * atomic smart pointer that pinned the block with a "tick" in the atomic's
 * word, before the writer that swapped the word out drops its own count
 * (`transfer_ext`). Each such unit is later counted by its reader
 * (`take_and_settle`) or paid back (`settle`, `settle_n`). The engine takes a
 * unit back only after it has seen the deposit, so `ext` is never negative
 * when the engine uses it. A block is finished when `count` and `ext` are
 * both zero, which is one comparison of the whole word: `release`, `settle`,
 * `settle_n` and `release_with_ext` return true in the one read-modify-write
 * that makes the word zero. A program that never calls the `ext` operations
 * (every ordinary use of the pointers) sees `ext == 0` always and a plain
 * 32-bit counter.
 *
 * `use_count` is `count + ext`: the owners plus the owners in transit. It
 * never waits and it is never below the number of owners that hold a count
 * (the atomic smart pointers keep this bound; see `core/atomic`). It is exact
 * at quiescence. While a store or a compare-exchange of an atomic smart
 * pointer is in flight, the value may exceed the owners by the units of that
 * one operation (the lagging drop, the surplus of the pending transfer, and a
 * reader's transferred unit until it is counted or paid).
 *
 * `use_count_settled` is the waiting twin: it returns `count` only when `ext`
 * is zero, and spins (a bounded busy-wait, then `std::this_thread::yield`)
 * while it is not. Apart from the lagging drop of a slot's decrement it is
 * exact. It can block as long as a thread that holds a pin or a pending
 * deposit is suspended, so it must not be called from a signal handler that
 * interrupts such a thread, and it does not make the owner count stable: it
 * only reports a moment at which nothing was in transit.
 *
 * Memory orders. Taking a reference needs none (`add` is relaxed: the caller
 * already owns a count, or the block is pinned). Dropping is `release`; the
 * thread that sees the word become zero then runs an acquire fence, so every
 * access of every other owner to the object happens before the disposal
 * (the same protocol as the standard libraries and `boost::shared_ptr`).
 * ThreadSanitizer does not model fences, so under it the drop is an
 * `acq_rel` read-modify-write instead. `try_add` (the promotion of a weak
 * pointer, increment-if-nonzero) is an `acq_rel` compare-exchange.
 * `transfer_ext` is relaxed: it only moves a debt that the transferring
 * writer has already published through the swap of the atomic's word.
 */
#ifndef LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_COUNTER_HPP
#define LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_COUNTER_HPP

#include <atomic>
#include <cstdint>
#include <thread>

#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
namespace detail
{
/**
 * @brief One packed counter word `{count:32, ext:32}` (see the file text).
 * @details Eight bytes, lock-free where `std::atomic<std::uint64_t>` is.
 */
class split_counter
{
public:
  /// The word type.
  typedef std::uint64_t word_type;

  /// Largest count the 32-bit half holds before it would carry into `ext`.
  static LUMEX_CONSTEXPR word_type
  max_count () LUMEX_NOEXCEPT
  {
    return 0x7FFFFFFFu;
  }

  /// The value of one unit of `ext` in the word.
  static LUMEX_CONSTEXPR word_type
  ext_unit () LUMEX_NOEXCEPT
  {
    return word_type (1) << 32;
  }

  /// Creates a counter whose count is @p count and whose ext is 0.
  explicit split_counter (std::uint32_t count) LUMEX_NOEXCEPT : word_ (count)
  {
  }

  split_counter (split_counter const &) = delete;
  split_counter &operator= (split_counter const &) = delete;

  /// Adds @p n to `count`. Relaxed: see the file text.
  void
  add (std::uint32_t n = 1) LUMEX_NOEXCEPT
  {
    word_type const before = word_.fetch_add (n, std::memory_order_relaxed);
    LUMEX_SMART_PTR_DEBUG_ASSERT ((before & 0xFFFFFFFFu) + n <= max_count ());
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (before);
  }

  /// Drops one count; true when the word became zero (the caller finishes
  /// the group).
  bool
  release () LUMEX_NOEXCEPT
  {
    return drop (1);
  }

  /// Adds @p n to `ext` (the writer's transfer of the ticks it swapped out).
  void
  transfer_ext (std::uint32_t n) LUMEX_NOEXCEPT
  {
    word_.fetch_add (word_type (n) << 32, std::memory_order_relaxed);
  }

  /// Takes back @p n units of `ext` that this thread added with
  /// `transfer_ext` (a writer whose swap failed). Never makes the word zero:
  /// the caller still pins the block.
  void
  untransfer_ext (std::uint32_t n) LUMEX_NOEXCEPT
  {
    word_type const before
        = word_.fetch_sub (word_type (n) << 32, std::memory_order_relaxed);
    LUMEX_SMART_PTR_DEBUG_ASSERT (before != (word_type (n) << 32));
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (before);
  }

  /// Drops one count and @p n units of `ext` in one RMW; true when the word
  /// became zero.
  bool
  release_with_ext (std::uint32_t n) LUMEX_NOEXCEPT
  {
    return drop (word_type (1) + (word_type (n) << 32));
  }

  /// Counts one owner and pays one unit of `ext` in one RMW (the reader of a
  /// swapped-out pointer). Never makes the word zero: the count is at least 1.
  void
  take_and_settle () LUMEX_NOEXCEPT
  {
    word_.fetch_add (word_type (1) - ext_unit (), std::memory_order_relaxed);
  }

  /// True while the group is alive (`count` or `ext` not zero).
  bool
  alive () const LUMEX_NOEXCEPT
  {
    return load () != 0;
  }

  /// Pays one unit of `ext` back; true when the word became zero.
  bool
  settle () LUMEX_NOEXCEPT
  {
    return drop (ext_unit ());
  }

  /// Pays @p n units of `ext` back in one RMW; true when the word became zero.
  bool
  settle_n (std::uint32_t n) LUMEX_NOEXCEPT
  {
    return drop (word_type (n) << 32);
  }

  /**
   * @brief Increment-if-nonzero: adds one count unless the word is zero.
   * @return true when a count was taken. A word with `count == 0` and
   * `ext != 0` (an object that a reader of an atomic smart pointer has
   * pinned but not yet counted) is alive and can be promoted.
   */
  bool
  try_add () LUMEX_NOEXCEPT
  {
    word_type current = word_.load (std::memory_order_relaxed);
    for (;;)
      {
        if (current == 0)
          return false;
        if (word_.compare_exchange_weak (current, current + 1,
                                         std::memory_order_acq_rel,
                                         std::memory_order_relaxed))
          return true;
      }
  }

  /// The whole word (relaxed).
  word_type
  load () const LUMEX_NOEXCEPT
  {
    return word_.load (std::memory_order_relaxed);
  }

  /// The `count` half of @p word.
  static LUMEX_CONSTEXPR long
  count_of (word_type word) LUMEX_NOEXCEPT
  {
    return static_cast<long> (static_cast<std::int32_t> (
        static_cast<std::uint32_t> (word & 0xFFFFFFFFu)));
  }

  /// The `ext` half of @p word.
  static LUMEX_CONSTEXPR long
  ext_of (word_type word) LUMEX_NOEXCEPT
  {
    return static_cast<long> (
        static_cast<std::int32_t> (static_cast<std::uint32_t> (word >> 32)));
  }

  /**
   * @brief The number of owners.
   * @details `count` when it is positive; 1 when `count` is 0 and `ext` is
   * not (the object is alive, pinned by a reader), so `expired ()` and
   * `lock ()` agree; 0 otherwise. Exact when no atomic smart pointer that
   * shares the block has an operation in flight.
   */
  long
  use_count () const LUMEX_NOEXCEPT
  {
    word_type const word = load ();
    LUMEX_SMART_PTR_DEBUG_ASSERT (ext_of (word) >= 0);
    return count_of (word) + ext_of (word);
  }

  /**
   * @brief The number of owners once no owner is in transit.
   * @details Returns `count` at an instant at which `ext` is zero, waiting
   * (a bounded busy-wait, then yields) while it is not. A word that is zero
   * returns 0 at once. Exact apart from the lag of the decrement of a slot
   * (the owner that a `store` or a compare-exchange of an atomic smart
   * pointer has not dropped yet). It may block while a pinning thread is
   * suspended; it must not be called from a signal handler on a pinned
   * thread.
   */
  long
  use_count_settled () const LUMEX_NOEXCEPT
  {
    unsigned spins = 0;
    word_type word = load ();
    while (ext_of (word) != 0)
      {
        if (spins < settled_spin_limit ())
          ++spins;
        else
          std::this_thread::yield ();
        word = load ();
      }
    return count_of (word);
  }

private:
  /// Busy-wait iterations of `use_count_settled` before it yields.
  static LUMEX_CONSTEXPR unsigned
  settled_spin_limit () LUMEX_NOEXCEPT
  {
    return 64u;
  }

  bool
  drop (word_type unit) LUMEX_NOEXCEPT
  {
#if LUMEX_SMART_PTR_TSAN
    return word_.fetch_sub (unit, std::memory_order_acq_rel) == unit;
#else
    if (word_.fetch_sub (unit, std::memory_order_release) != unit)
      return false;
    std::atomic_thread_fence (std::memory_order_acquire);
    return true;
#endif
  }

  std::atomic<word_type> word_;
};
} // namespace detail
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_COUNTER_HPP

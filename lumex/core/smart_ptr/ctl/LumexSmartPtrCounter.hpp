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
 * two's complement) is a debt ledger that only the split-count engine
 * (`core/atomic`, phase 2) uses: a reader of an atomic smart pointer pins a
 * block with a "tick" in the atomic's word, and the writer that swaps the
 * word out moves the ticks it took into `ext` (`transfer_ext`) before it
 * drops its own count; each reader pays one unit back when it settles
 * (`settle`). A block is finished when `count` and `ext` are both zero, which
 * is one comparison of the whole word: `release` and `settle` return true in
 * the one read-modify-write that makes the word zero. A program that never
 * calls `transfer_ext` and `settle` (every ordinary use of the pointers) sees
 * `ext == 0` always and a plain 32-bit counter.
 *
 * `count` never goes negative, so the representation is unique (`ext` stays
 * within +-2^31). `ext` is transiently negative when a reader settles before
 * the writer's transfer; the writer still holds its count then, so the word
 * is not zero.
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

  /// Pays one unit of `ext` back; true when the word became zero.
  bool
  settle () LUMEX_NOEXCEPT
  {
    return drop (ext_unit ());
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
    long const count = count_of (word);
    if (count > 0)
      return count;
    return ext_of (word) != 0 ? 1 : 0;
  }

private:
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

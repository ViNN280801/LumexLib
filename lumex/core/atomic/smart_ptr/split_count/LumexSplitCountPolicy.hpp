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
 * @file LumexSplitCountPolicy.hpp
 * @brief The policy of the split-count engine: the named steps of its
 * protocol, the events a ledger counts, the guess of the word, the tick
 * limit, the reserve and the back-off.
 * @details The engine is a template on a policy so that the tests can run
 * the real engine code under a cooperative scheduler, count its events and
 * tear its speculative reads; the production policy `split_native_policy_t`
 * makes every hook an empty (or trivial) inline function, so it costs
 * nothing. A policy is a type with these static functions, all `noexcept`:
 *
 * - `at (split_point_t)`: called immediately before every step of the
 *   protocol that reads or writes shared memory (the word or a counter of a
 *   block). A test scheduler or gate blocks a thread there.
 * - `note (split_event_t, std::uint32_t)`: called after a step that changed
 *   the tick accounting, with its amount. A test ledger sums them; at
 *   quiescence `tick == untick + settle_ext + consumed` and
 *   `transfer - untransfer == settle_ext`.
 * - `guess (word)`: the speculative read of the slot word (two relaxed
 *   64-bit reads, not atomic as a pair). EVERY read of the word that is not
 *   an atomic operation goes through it, and every use of its result is
 *   validated by a whole-word compare-and-swap or by an acquire re-read. A
 *   test policy may return a torn value here.
 * - `ticked (std::uint32_t)`: called after every successful tick (a load, a
 *   writer pin, a compare-exchange pin) with the `L` the word holds now.
 * - `tick_limit ()`: a pin does not tick a word whose `L` has reached it
 *   (2^24 - 1 in production; a test lowers it to reach the back-off).
 * - `reserve ()`: the number of units a writer deposits in `ext` beyond the
 *   ticks it saw, so that ticks that arrive before its swap need no further
 *   access to the counter (8 in production; a test sets 0 to take the
 *   top-up path every time).
 * - `back_off ()`: what a pin does while `L` is at the limit (yield).
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_POLICY_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_POLICY_HPP

#include <cstdint>
#include <thread>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountWord.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

namespace lumex
{
namespace core
{
namespace atomic
{
namespace smart_ptr
{
inline namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
{
namespace Detail
{
/// The steps of the protocol before which `Policy::at` is called.
enum class split_point_t : unsigned
{
  load_read = 1,   ///< a pin reads the word (a guess or an atomic read)
  load_tick = 2,   ///< a pin ticks the word (or a load confirms an empty one)
  load_count = 3,  ///< a pinned load takes its own reference
  settle_read = 4, ///< a settle reads the word
  settle_untick = 5, ///< a settle takes a tick back from the word
  settle_ext = 6,    ///< a settle pays units of `ext` on the block
  swap = 7,          ///< a store or an exchange swaps the word
  cas_read = 8,      ///< a compare-exchange reads the word
  cas_pin = 9,       ///< a compare-exchange ticks a holder word to compare it
  cas = 10,          ///< a compare-exchange swaps the word
  transfer = 11, ///< a writer deposits units in (or takes them out of) `ext`
  release = 12   ///< a writer drops the reference the word owned
};

/// The changes of the tick accounting that `Policy::note` reports.
enum class split_event_t : unsigned
{
  tick = 1,       ///< a tick entered the word (amount 1)
  untick = 2,     ///< a settle took a tick back from the word (amount 1)
  consumed = 3,   ///< a successful swap consumed the writer's own tick (1)
  transfer = 4,   ///< units were deposited in `ext` (amount: their number)
  untransfer = 5, ///< deposited units were taken back (amount: their number)
  settle_ext = 6  ///< units of `ext` were paid by a tick (amount 1)
};

/**
 * @brief The production policy: no hooks, a plain speculative read, the full
 * 24-bit tick count, a reserve of 8 and a yield as back-off.
 */
struct split_native_policy_t
{
  static void
  at (split_point_t) LUMEX_NOEXCEPT
  {
  }

  static void
  note (split_event_t, std::uint32_t) LUMEX_NOEXCEPT
  {
  }

  static split_value_t
  guess (::lumex::core::atomic::dwcas::dwcas_word const &word) LUMEX_NOEXCEPT
  {
    return word.speculative_load ();
  }

  static void
  ticked (std::uint32_t) LUMEX_NOEXCEPT
  {
  }

  static std::uint32_t
  tick_limit () LUMEX_NOEXCEPT
  {
    return 0xFFFFFFu;
  }

  static std::uint32_t
  reserve () LUMEX_NOEXCEPT
  {
    return 8u;
  }

  static void
  back_off () LUMEX_NOEXCEPT
  {
    std::this_thread::yield ();
  }
};
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_POLICY_HPP

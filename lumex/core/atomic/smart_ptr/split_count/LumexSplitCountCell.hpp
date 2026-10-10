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
 * @file LumexSplitCountCell.hpp
 * @brief The split-count engine behind
 * `atomic_shared_ptr_lock_free_split_count` and
 * `atomic_weak_ptr_lock_free_split_count`: one 16-byte word updated by a
 * 128-bit compare-and-swap over the module's own control block.
 * @details The value lives in one `dwcas_word` (see `LumexSplitCountWord.hpp`
 * for the layout) next to the 32-bit wait counter of the other engines: 32
 * bytes. A non-empty word owns one `count` unit of its counter group: the
 * strong group of the block in the shared engine, the weak ledger in the weak
 * engine, and the strong group of the holder for a holder word (below).
 *
 * **Why ticks.** A loader cannot take a reference on a block it only knows
 * the address of: the block could be freed first. It therefore first pins the
 * block with a tick, one compare-and-swap of the whole word that raises `L`.
 * While a tick exists the block cannot be freed: either the word still names
 * it, or the writer that swapped it out has deposited the tick as a unit of
 * the block's `ext` ("owners in transit") before it swapped. `ext` is never
 * negative.
 *
 * **Load.** Tick (the acquire of this compare-and-swap synchronizes with the
 * release swap that published the block, its anchor and the object), read the
 * block (`anchor`, or the holder's owner and pointer), then decide on an
 * atomic read whether the own tick is still in the word:
 * - if the word still names the block with `L > 0`: count one reference on
 *   the owner (this is the linearization point of the load), then take a tick
 *   back from the word (ticks are fungible, so it need not be the loader's
 *   own; the un-tick is `release`). If the tick is gone by then, pay the unit
 *   the writer deposited for it (`settle`);
 * - otherwise the tick was deposited by the writer that swapped the word: one
 *   read-modify-write counts the reference and pays the unit.
 * The decision that the tick is not in the word is always taken on a value
 * read with acquire (a load, or the failure value of a compare-and-swap with
 * an acquire failure order), because only then the deposit of the winning
 * writer, sequenced before its swap, precedes the payment in the modification
 * order of the counter.
 *
 * **Store and exchange.** A word with `L == 0` is swapped by one
 * compare-and-swap, validated by the swap itself. A word with `L > 0` names a
 * block the writer owns nothing of, so the writer pins it first (a tick, like
 * a load), deposits `L - 1 + k` units in `ext` BEFORE the swap (its own tick
 * is excluded, because the swap consumes it; `k` is the reserve of the policy,
 * so that ticks arriving before the swap need no further access to the
 * counter), swaps (success `acq_rel`, failure `acquire`) and:
 * - on a failed swap that left the same block with more ticks, tops up the
 *   deposit, only upward, and retries;
 * - on a failed swap that removed the block (the writer's own tick went with
 *   it, deposited by the winner) pays `deposited + 1` units in ONE
 *   read-modify-write and starts again;
 * - on success the surplus (`deposited - (L - 1)`) goes back with the drop of
 *   the word's unit (`store`: one read-modify-write) or before the hand-over
 *   (`exchange`).
 * A compare-exchange does not pin: `expected` owns the block. It deposits the
 * ticks it sees (no reserve), swaps, and withdraws the deposit if the swap
 * fails. A compare-exchange whose `expected` does not fit the word compares a
 * holder word, which it first pins like a loader.
 *
 * **Holders (HG).** A pointer whose offset from the anchor does not fit in 40
 * bits is stored through a holder: a block (`ctl_holder`, `ctl_weak_holder`)
 * that owns a reference to the owner block (strong, or weak in the weak
 * engine) and keeps the stored pointer. The atomic object owns one strong
 * `count` unit of the HOLDER, readers tick and settle on the holder's strong
 * group, and a load of a holder word returns the OWNER (the holder never
 * escapes). In the shared engine every deposit, withdrawal and payment on a
 * holder is mirrored on the owner's strong `ext`, so that `use_count ()` of
 * the owner sees the pins; the owner is touched first when units leave and
 * the holder first when units enter. The reference of the holder on the owner
 * belongs to the word: whoever drops the word's unit (`store`, a successful
 * compare-exchange, the destructor, an image that was never installed) also
 * drops that reference, together with the surplus of the mirror in one
 * read-modify-write, after the holder was told to give it up (`disown`); the
 * `exchange` hand-over gives the reference to its result and adds none. The
 * owner stays alive for every pin that remains on the holder, because the
 * mirror unit of each such pin was deposited on the owner before the swap
 * that removed it, and the holder keeps itself alive by its own counter. So
 * the owner shows no extra reference once the replacing call has dropped the word's unit. The weak engine does not
 * mirror (weak loads do not change `use_count ()`) and keeps the weak
 * reference of its holder until the holder is disposed.
 *
 * **Why no address comparison can be fooled by a reused block address.** A
 * tick and a swap compare the whole word in one instruction (success means
 * the word names the live block); a settle compares while its own tick keeps
 * the block alive; a compare-exchange compares through `expected`, whose
 * reference keeps that block alive, and pins a holder word before it reads
 * it. A block freed, created again at the same address and installed again is
 * simply the live block, and ticks are fungible between the installations of
 * a block (also with different offsets). No tag and no epoch is needed.
 *
 * **Memory orders.** Every successful read-modify-write of the word is at
 * least `acq_rel` (`seq_cst` when the caller asks for it). The take-back of a
 * tick is `release` and MUST NOT be relaxed: a loader of a holder word reads
 * the holder's owner and pointer without owning a count on the holder, and
 * the only edge from those reads to the holder's destruction runs from the
 * un-tick (release) to the swap of the writer (acquire) and the drop. A
 * failed compare-and-swap is `relaxed`, except the swap of a writer, whose
 * failure value is the decision value of the rule above (`acquire`). The
 * counters of the block bring their own orders (`LumexSmartPtrCounter.hpp`).
 * The hardware backends of the word ignore the orders (every locked
 * instruction is a full barrier); they matter for the built-in backend that
 * ThreadSanitizer checks.
 *
 * **use_count and the linearization rule.** The linearization point of a
 * store, an exchange and a successful compare-exchange is its successful
 * swap; of a failing compare-exchange the load it performs; of a load that
 * finds its tick in the word its count step; of any other load the deposit
 * that covered its tick, made by the writer whose swap then succeeded. Owners
 * of a block at an instant are the live pointers, the non-empty slots naming
 * it and the loads linearized but not yet returned. `use_count ()` of the
 * block is `count + ext` and satisfies:
 * - G1, never below: at every instant `use_count () >= owners`, so a thread
 *   that is an owner and reads 1 is the sole owner;
 * - G2, bounded above: `use_count () == owners` except for (i) a store or a
 *   successful compare-exchange between its swap and its drop (+1, the
 *   lagging decrement; `exchange` has no such window), (ii) a writer between
 *   its deposit and the end of its call (+ other ticks + `k`), (iii) a unit
 *   deposited for a loader that has already counted itself, until it pays
 *   (+1). This holds for the owner of an alias stored through a holder as
 *   well;
 * - exact when nothing is in flight. It never waits;
 * - `ext` is bounded by `W * (P + k)` for `W` writers that have deposited and
 *   not yet returned or paid and `P` pins: the outstanding deposit of one
 *   writer is the pins its swap covers (at most `P`) plus its reserve.
 *
 * **Progress.** A compare-and-swap of the word fails only when another
 * operation succeeded on it; no operation waits for a counter that another
 * thread moves. The one wait is the back-off of a pin that finds `L` at the
 * limit of the policy (2^24 - 1 pins of one object at once). Allocation of a
 * holder may block in the allocator; running out of memory ends the program
 * (every operation is `noexcept`).
 *
 * **Notification and deleters.** Modifying operations do not wake waiters,
 * as in the standard; only `notify_one` and `notify_all` do. A replaced value
 * is destroyed in the replacing call, unless a pin on it is still
 * outstanding; then the last thread to pay for it destroys it. Deleters
 * therefore run inside `load`, `store`, `exchange`, `compare_exchange_*`,
 * `wait` and the destructor, only from the read-modify-write that makes the
 * counter zero, after the word was updated and with no lock held. At that
 * moment the running thread holds no pin and no deposit on that block, so a
 * deleter may use the same atomic object.
 *
 * @tparam Pointer `lumex::core::smart_ptr::shared_ptr<T>` or `weak_ptr<T>`.
 * @tparam Policy The hooks of the protocol (`LumexSplitCountPolicy.hpp`).
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_CELL_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_CELL_HPP

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountPolicy.hpp"
#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountTraits.hpp"
#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountWord.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
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
/**
 * @brief The split-count atomic cell (see the file text).
 */
template <typename Pointer, typename Policy = split_native_policy_t>
class split_count_cell : private epoch_wait_base
{
  typedef split_pointer_traits_t<Pointer> traits_type;
  typedef typename traits_type::element_type element_type;
  typedef typename traits_type::ledger_type ledger_type;
  typedef typename traits_type::holder_type holder_type;
  typedef split_strong_ledger_t holder_ledger_type;
  typedef split_word_t word;
  typedef split_block_t block_type;

public:
  /// The smart pointer the cell stores.
  typedef Pointer value_type;

  static LUMEX_CONSTEXPR bool is_always_lock_free = true;

  /// The empty value; usable for constant initialization.
  LUMEX_CONSTEXPR
  split_count_cell () LUMEX_NOEXCEPT : epoch_wait_base (), word_ () {}

  /// Holds @p desired; terminates on a CPU without CMPXCHG16B.
  explicit split_count_cell (Pointer desired) LUMEX_NOEXCEPT
      : epoch_wait_base (),
        word_ (take_image (desired))
  {
    ::lumex::core::atomic::dwcas::require_dwcas ();
  }

  split_count_cell (split_count_cell const &) = delete;
  split_count_cell &operator= (split_count_cell const &) = delete;

  /// Drops the unit the word owns. No operation may be in flight (the
  /// object is being destroyed), so the word has no ticks. The word is read
  /// atomically, not guessed: a guess may be torn (its halves from two
  /// moments), and the destructor must drop exactly the block that is there.
  ~split_count_cell ()
  {
    // Nobody else uses the object while it is destroyed, so the halves
    // cannot tear; the plain reads avoid a locked instruction (and the need
    // for the CPU feature) for an object that was never used.
    split_value_t const old = word_.speculative_load ();
    LUMEX_SMART_PTR_DEBUG_ASSERT (word::is_empty (old)
                                  || word::ticks_of (old) == 0u);
    drop_word (old);
  }

  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return true;
  }

  Pointer
  load (std::memory_order order) const LUMEX_NOEXCEPT
  {
    ::lumex::core::atomic::dwcas::require_dwcas ();
    split_value_t const pinned = pin (rmw_order (order));
    if (word::is_empty (pinned))
      return traits_type::adopt (nullptr, raw_pointer (pinned));
    block_type *const block = word::block_of (pinned);
    block_type *owner = block;
    element_type *stored = nullptr;
    if (word::is_holder (pinned))
      {
        holder_type const *const holder
            = static_cast<holder_type const *> (block);
        owner = holder->owner ();
        stored = static_cast<element_type *> (holder->pointer ());
      }
    else
      stored = block_pointer (block, pinned);
    count_and_settle (pinned, owner);
    return traits_type::adopt (owner, stored);
  }

  void
  store (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    ::lumex::core::atomic::dwcas::require_dwcas ();
    split_value_t const image = take_image (desired);
    swapped_t const swapped = swap_out (image, rmw_order (order));
    drop_word_with_ext (swapped.old, swapped.surplus);
  }

  Pointer
  exchange (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    ::lumex::core::atomic::dwcas::require_dwcas ();
    split_value_t const image = take_image (desired);
    swapped_t const swapped = swap_out (image, rmw_order (order));
    withdraw (swapped.old, swapped.surplus);
    return hand_over (swapped.old);
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order success,
                           std::memory_order failure) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, false, success, failure);
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, false, order,
                             failure_order (order));
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order success,
                         std::memory_order failure) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, true, success, failure);
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, true, order,
                             failure_order (order));
  }

  void
  wait (Pointer const &old, std::memory_order order) const LUMEX_NOEXCEPT
  {
    this->wait_while ([this, &old, order]
                        { return equivalent (load (order), old); });
  }

  using epoch_wait_base::notify_all;
  using epoch_wait_base::notify_one;

private:
  /// The result of `swap_out`: the swapped-out word (its ticks are already
  /// in `ext`) and the part of the deposit that the swap did not need.
  struct swapped_t
  {
    split_value_t old;
    std::uint32_t surplus;
  };

  /// The floor of the order of every successful read-modify-write of the
  /// word: `seq_cst` when asked for, `acq_rel` otherwise.
  static std::memory_order
  rmw_order (std::memory_order order) LUMEX_NOEXCEPT
  {
    return order == std::memory_order_seq_cst ? std::memory_order_seq_cst
                                              : std::memory_order_acq_rel;
  }

  /// The failure order that the one-order compare-exchange implies.
  static std::memory_order
  failure_order (std::memory_order order) LUMEX_NOEXCEPT
  {
    return order == std::memory_order_acq_rel   ? std::memory_order_acquire
           : order == std::memory_order_release ? std::memory_order_relaxed
                                                : order;
  }

  /// Same stored pointer and same block (or both empty).
  static bool
  equivalent (Pointer const &a, Pointer const &b) LUMEX_NOEXCEPT
  {
    return traits_type::control (a) == traits_type::control (b)
           && traits_type::stored (a) == traits_type::stored (b);
  }

  /// The stored pointer of an empty word.
  static element_type *
  raw_pointer (split_value_t value) LUMEX_NOEXCEPT
  {
    return reinterpret_cast<element_type *> (
        static_cast<std::uintptr_t> (word::raw_of (value)));
  }

  /// The stored pointer of a block word; @p block is alive.
  static element_type *
  block_pointer (block_type const *block, split_value_t value) LUMEX_NOEXCEPT
  {
    return reinterpret_cast<element_type *> (static_cast<std::uintptr_t> (
        static_cast<std::uint64_t> (block->anchor ())
        + word::offset_of (value)));
  }

  /// The owner a holder block refers to.
  static block_type *
  owner_of (block_type const *holder) LUMEX_NOEXCEPT
  {
    return static_cast<holder_type const *> (holder)->owner ();
  }

  /**
   * @brief The word for @p desired; the reference moves into the word.
   * @details A block word takes over the count of @p desired, which is left
   * empty. A holder word owns the new holder's reference; @p desired keeps
   * its own count and its owner drops it as usual. An empty pointer gives
   * an empty word and owns nothing. A pure function of the block and the
   * stored pointer.
   */
  static split_value_t
  take_image (Pointer &desired) LUMEX_NOEXCEPT
  {
    block_type *const block = traits_type::control (desired);
    element_type *const stored = traits_type::stored (desired);
    if (block == nullptr)
      return word::empty_word (word::address_of (stored));
    std::uint64_t const offset
        = word::address_of (stored)
          - static_cast<std::uint64_t> (block->anchor ());
    if (word::fits (offset))
      {
        traits_type::detach (desired);
        return word::block_word (block, offset);
      }
    return word::holder_word (traits_type::make_holder (block, stored));
  }

  // -- The counter group of a word --
  //
  // The group G of a word is the ledger of the block for a block word and
  // the strong group of the holder for a holder word. In the shared engine
  // every unit that enters or leaves the ext of a holder also enters or
  // leaves the ext of its owner (the mirror).

  /// Deposits @p n units in the `ext` of the group of @p w (mirrored for a
  /// holder of the shared engine; the holder first).
  static void
  deposit (split_value_t w, std::uint32_t n) LUMEX_NOEXCEPT
  {
    if (n == 0u)
      return;
    block_type *const block = word::block_of (w);
    Policy::at (split_point_t::transfer);
    if (word::is_holder (w))
      {
        holder_ledger_type::transfer (block, n);
        if (traits_type::mirror_holder)
          owner_of (block)->transfer_strong_ext (n);
      }
    else
      ledger_type::transfer (block, n);
    Policy::note (split_event_t::transfer, n);
  }

  /// Takes back @p n units that this thread deposited (the owner's mirror
  /// first). Never finishes a block: the caller pins it.
  static void
  withdraw (split_value_t w, std::uint32_t n) LUMEX_NOEXCEPT
  {
    if (n == 0u)
      return;
    block_type *const block = word::block_of (w);
    Policy::note (split_event_t::untransfer, n);
    Policy::at (split_point_t::transfer);
    if (word::is_holder (w))
      {
        if (traits_type::mirror_holder)
          owner_of (block)->untransfer_strong_ext (n);
        holder_ledger_type::untransfer (block, n);
      }
    else
      ledger_type::untransfer (block, n);
  }

  /// Pays @p n units of `ext` of the group of @p w in one read-modify-write
  /// (the owner's mirror first); finishes the block if that makes it zero.
  static void
  pay (split_value_t w, std::uint32_t n) LUMEX_NOEXCEPT
  {
    block_type *const block = word::block_of (w);
    Policy::at (split_point_t::settle_ext);
    if (word::is_holder (w))
      {
        if (traits_type::mirror_holder)
          {
            if (n == 1u)
              owner_of (block)->settle_strong ();
            else
              owner_of (block)->settle_strong_n (n);
          }
        if (n == 1u)
          holder_ledger_type::settle (block);
        else
          holder_ledger_type::settle_n (block, n);
      }
    else if (n == 1u)
      ledger_type::settle (block);
    else
      ledger_type::settle_n (block, n);
  }

  /// L9: counts one reference on @p owner and pays the unit of the tick
  /// pinned on @p w, the swapped-out path.
  static void
  take_and_pay (split_value_t w, block_type *owner) LUMEX_NOEXCEPT
  {
    block_type *const block = word::block_of (w);
    Policy::at (split_point_t::settle_ext);
    if (word::is_holder (w))
      {
        if (traits_type::mirror_holder)
          owner->take_and_settle_strong ();
        else
          ledger_type::add (owner);
        holder_ledger_type::settle (block);
      }
    else
      ledger_type::take_and_settle (block);
  }

  /// Drops the unit that the word @p w owned (an image that was never
  /// installed, or a swapped-out word without surplus). For a holder word of
  /// the shared engine the reference of the holder on its owner leaves with
  /// the word's unit (F1).
  static void
  drop_word (split_value_t w) LUMEX_NOEXCEPT
  {
    if (word::is_empty (w))
      return;
    Policy::at (split_point_t::release);
    if (word::is_holder (w))
      {
        release_owner<traits_type::mirror_holder> (word::block_of (w), 0u);
        holder_ledger_type::release (word::block_of (w));
      }
    else
      ledger_type::release (word::block_of (w));
  }

  /// F1, the shared engine: the holder gives up its reference on the owner
  /// (`disown`, so that the holder's disposal does not drop it again) and the
  /// dropper of the word's unit drops it, together with the @p surplus of the
  /// mirror in one read-modify-write. Every pin that is still on the holder
  /// has its mirror unit in the owner's `ext` (deposited before the swap), so
  /// the owner stays alive for it. The caller still owns the word's unit on
  /// the holder, which keeps @p holder alive here, and releases it after this
  /// call: the `disown` write is ordered before the disposal of the holder by
  /// that release (and the acquire on zero).
  template <bool Mirror>
  static typename std::enable_if<Mirror>::type
  release_owner (block_type *holder, std::uint32_t surplus) LUMEX_NOEXCEPT
  {
    holder_type *const h = static_cast<holder_type *> (holder);
    block_type *const owner = h->owner ();
    h->disown ();
    if (surplus == 0u)
      owner->release_strong ();
    else
      owner->release_strong_with_ext (surplus);
  }

  /// The weak engine: the holder keeps its weak reference until it is
  /// disposed (there is no mirror, so nothing else keeps the owner's block).
  template <bool Mirror>
  static typename std::enable_if<!Mirror>::type
  release_owner (block_type *, std::uint32_t) LUMEX_NOEXCEPT
  {
  }

  /// Hand-over of a holder word to the result of an exchange: the shared
  /// engine hands over the holder's reference on the owner (`disown`), the
  /// weak engine takes a new one.
  template <bool Mirror>
  static typename std::enable_if<Mirror>::type
  take_owner_reference (block_type *holder, block_type *) LUMEX_NOEXCEPT
  {
    static_cast<holder_type *> (holder)->disown ();
  }

  template <bool Mirror>
  static typename std::enable_if<!Mirror>::type
  take_owner_reference (block_type *, block_type *owner) LUMEX_NOEXCEPT
  {
    ledger_type::add (owner);
  }

  /// S6: drops the unit of the swapped-out word @p w and takes back the
  /// @p surplus of the deposit in one read-modify-write (the owner's mirror
  /// first, for a holder word).
  static void
  drop_word_with_ext (split_value_t w, std::uint32_t surplus) LUMEX_NOEXCEPT
  {
    if (surplus == 0u || word::is_empty (w))
      {
        drop_word (w);
        return;
      }
    block_type *const block = word::block_of (w);
    Policy::note (split_event_t::untransfer, surplus);
    Policy::at (split_point_t::release);
    if (word::is_holder (w))
      {
        release_owner<traits_type::mirror_holder> (block, surplus);
        holder_ledger_type::release_with_ext (block, surplus);
      }
    else
      ledger_type::release_with_ext (block, surplus);
  }

  /// S5 and S12: the writer's own tick left the word with the swap of another
  /// writer, who deposited a unit for it. Pays the unit and the writer's own
  /// @p deposited units in one read-modify-write.
  static void
  pay_back (split_value_t w, std::uint32_t deposited) LUMEX_NOEXCEPT
  {
    if (deposited != 0u)
      Policy::note (split_event_t::untransfer, deposited);
    Policy::note (split_event_t::settle_ext, 1u);
    pay (w, deposited + 1u);
  }

  /// The previous value of an exchange, which takes over the unit of the
  /// swapped-out word. A holder word hands over the holder's reference on the
  /// owner (shared engine; the weak engine takes a new one) and the holder's
  /// unit is dropped.
  static Pointer
  hand_over (split_value_t old) LUMEX_NOEXCEPT
  {
    if (word::is_empty (old))
      return traits_type::adopt (nullptr, raw_pointer (old));
    block_type *const block = word::block_of (old);
    if (!word::is_holder (old))
      return traits_type::adopt (block, block_pointer (block, old));
    holder_type const *const holder = static_cast<holder_type const *> (block);
    block_type *const owner = holder->owner ();
    element_type *const stored
        = static_cast<element_type *> (holder->pointer ());
    Policy::at (split_point_t::load_count);
    take_owner_reference<traits_type::mirror_holder> (block, owner);
    Pointer result = traits_type::adopt (owner, stored);
    Policy::at (split_point_t::release);
    holder_ledger_type::release (block);
    return result;
  }

  /**
   * @brief Pins the current value: ticks a non-empty word (the tick is now
   * in the word) or confirms an empty one; returns the word before the tick.
   * @details L0 to L3. The linearization point of a load that finds its tick
   * in the word is its count step, not this one.
   */
  split_value_t
  pin (std::memory_order order) const LUMEX_NOEXCEPT
  {
    Policy::at (split_point_t::load_read);
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (word::is_empty (seen))
          {
            Policy::at (split_point_t::load_tick);
            split_value_t const found = word_.compare_exchange_strong (
                seen, seen, order, std::memory_order_relaxed);
            if (found == seen)
              return seen;
            seen = found;
            continue;
          }
        if (word::ticks_of (seen) >= Policy::tick_limit ())
          {
            Policy::back_off ();
            Policy::at (split_point_t::load_read);
            seen = word_.load (std::memory_order_relaxed);
            continue;
          }
        Policy::at (split_point_t::load_tick);
        split_value_t const found = word_.compare_exchange_strong (
            seen, word::with_tick (seen), order, std::memory_order_relaxed);
        if (found == seen)
          {
            Policy::note (split_event_t::tick, 1u);
            Policy::ticked (word::ticks_of (seen) + 1u);
            return seen;
          }
        seen = found;
      }
  }

  /**
   * @brief L5 to L9: the loader counts itself and settles its tick.
   * @details If the tick is in the word (a guess, then an acquire re-read):
   * count, then settle (un-tick, or pay if a writer deposited it meanwhile:
   * a brief over-report). If it is not: one read-modify-write that counts
   * and pays.
   */
  void
  count_and_settle (split_value_t pinned,
                    block_type *owner) const LUMEX_NOEXCEPT
  {
    Policy::at (split_point_t::load_count);
    std::uint64_t const tag = pinned.lo;
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (seen.lo == tag && word::ticks_of (seen) != 0u)
          {
            ledger_type::add (owner);
            settle (pinned);
            return;
          }
        seen = word_.load (std::memory_order_acquire);
        if (seen.lo == tag && word::ticks_of (seen) != 0u)
          continue;
        Policy::note (split_event_t::settle_ext, 1u);
        take_and_pay (pinned, owner);
        return;
      }
  }

  /**
   * @brief L7 and L8: settles the tick pinned on @p pinned, exactly once.
   * @details Takes a tick back from the word while it names the same block
   * with `L > 0`; otherwise a writer that swapped the block out deposited
   * the unit of the tick, and this pays it. The decision "not in the word"
   * is made on an acquire read, never on a guess.
   */
  void
  settle (split_value_t pinned) const LUMEX_NOEXCEPT
  {
    std::uint64_t const tag = pinned.lo;
    Policy::at (split_point_t::settle_read);
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (seen.lo == tag && word::ticks_of (seen) != 0u)
          {
            Policy::at (split_point_t::settle_untick);
            split_value_t const found = word_.compare_exchange_strong (
                seen, word::without_tick (seen), std::memory_order_release,
                std::memory_order_relaxed);
            if (found == seen)
              {
                Policy::note (split_event_t::untick, 1u);
                return;
              }
            seen = found;
            continue;
          }
        Policy::at (split_point_t::settle_read);
        seen = word_.load (std::memory_order_acquire);
        if (seen.lo == tag && word::ticks_of (seen) != 0u)
          continue;
        Policy::note (split_event_t::settle_ext, 1u);
        pay (pinned, 1u);
        return;
      }
  }

  /**
   * @brief S1 to S5: swaps @p image into the word and returns the old word.
   * @details A word without ticks is swapped by one compare-and-swap. A word
   * with ticks is pinned first, its ticks (the writer's own excluded) are
   * deposited together with the reserve, and the swap is retried until it
   * succeeds or the block is gone (see the file text). The caller drops the
   * old word's unit (`store`) or hands it over (`exchange`) after it took
   * back the surplus.
   */
  swapped_t
  swap_out (split_value_t image, std::memory_order order) LUMEX_NOEXCEPT
  {
    Policy::at (split_point_t::swap);
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (word::is_empty (seen) || word::ticks_of (seen) == 0u)
          {
            Policy::at (split_point_t::swap);
            split_value_t const found = word_.compare_exchange_strong (
                seen, image, order, std::memory_order_relaxed);
            if (found == seen)
              {
                swapped_t const result = { seen, 0u };
                return result;
              }
            seen = found;
            continue;
          }
        if (word::ticks_of (seen) >= Policy::tick_limit ())
          {
            Policy::back_off ();
            Policy::at (split_point_t::swap);
            seen = word_.load (std::memory_order_relaxed);
            continue;
          }
        Policy::at (split_point_t::load_tick);
        split_value_t const ticked = word_.compare_exchange_strong (
            seen, word::with_tick (seen), order, std::memory_order_relaxed);
        if (!(ticked == seen))
          {
            seen = ticked;
            continue;
          }
        Policy::note (split_event_t::tick, 1u);
        Policy::ticked (word::ticks_of (seen) + 1u);
        split_value_t const pinned = seen;
        split_value_t current = word::with_tick (seen);
        std::uint32_t moved = word::ticks_of (seen) + Policy::reserve ();
        deposit (current, moved);
        for (;;)
          {
            Policy::at (split_point_t::swap);
            split_value_t const found = word_.compare_exchange_strong (
                current, image, order, std::memory_order_acquire);
            if (found == current)
              {
                // The writer's own tick left with the word: consumed.
                Policy::note (split_event_t::consumed, 1u);
                swapped_t const result
                    = { current, moved - (word::ticks_of (current) - 1u) };
                return result;
              }
            if (found.lo != pinned.lo || word::ticks_of (found) == 0u)
              {
                // The block was swapped out by another writer, who deposited
                // for the own tick: pay it and the own deposit at once.
                pay_back (current, moved);
                seen = found;
                break;
              }
            // The same block with other ticks (any installation of it): top
            // up the deposit when the ticks outgrew it, never lower it.
            std::uint32_t const need = word::ticks_of (found) - 1u;
            if (need > moved)
              {
                std::uint32_t const total = need + Policy::reserve ();
                deposit (found, total - moved);
                moved = total;
              }
            current = found;
          }
      }
  }

  bool
  compare_exchange (Pointer &expected, Pointer &desired, bool weak,
                    std::memory_order success,
                    std::memory_order failure) LUMEX_NOEXCEPT
  {
    ::lumex::core::atomic::dwcas::require_dwcas ();
    block_type *const expected_block = traits_type::control (expected);
    element_type *const expected_stored = traits_type::stored (expected);
    split_value_t want = word::empty_word (word::address_of (expected_stored));
    if (expected_block != nullptr)
      {
        std::uint64_t const offset
            = word::address_of (expected_stored)
              - static_cast<std::uint64_t> (expected_block->anchor ());
        if (!word::fits (offset))
          return compare_exchange_holder (expected, desired, weak, success,
                                          failure);
        want = word::block_word (expected_block, offset);
      }
    bool built = false;
    split_value_t image = word::empty_word (0u);
    Policy::at (split_point_t::cas_read);
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (word::same_value (seen, want))
          {
            if (!built)
              {
                image = take_image (desired);
                built = true;
              }
            std::uint32_t const ticks
                = word::is_empty (seen) ? 0u : word::ticks_of (seen);
            // S9: no pin, `expected` keeps the block alive.
            deposit (seen, ticks);
            Policy::at (split_point_t::cas);
            split_value_t const found = word_.compare_exchange_strong (
                seen, image, rmw_order (success), std::memory_order_relaxed);
            if (found == seen)
              {
                drop_word (seen);
                return true;
              }
            withdraw (seen, ticks);
            seen = found;
            if (!weak)
              continue;
          }
        // Not equivalent, or the one attempt of the weak form failed: the
        // failure is this load, and a strong form whose value came back
        // tries again.
        Pointer current = load (failure);
        if (!weak && equivalent (current, expected))
          {
            Policy::at (split_point_t::cas_read);
            seen = Policy::guess (word_);
            continue;
          }
        if (built)
          drop_word (image);
        expected = std::move (current);
        return false;
      }
  }

  /**
   * @brief S12: the compare-exchange for an @p expected whose offset does not
   * fit; only a holder word can be equivalent to it.
   * @details A holder can be freed at any time, so it is pinned (ticked)
   * before its owner and pointer are read. On a match the ticks of the word
   * (the comparer's own excluded, the swap consumes it) are deposited and the
   * word is swapped; a failed swap is handled as in a store.
   */
  bool
  compare_exchange_holder (Pointer &expected, Pointer &desired, bool weak,
                           std::memory_order success,
                           std::memory_order failure) LUMEX_NOEXCEPT
  {
    block_type const *const expected_block = traits_type::control (expected);
    void const *const expected_pointer
        = split_erase (traits_type::stored (expected));
    bool built = false;
    split_value_t image = word::empty_word (0u);
    Policy::at (split_point_t::cas_read);
    split_value_t seen = Policy::guess (word_);
    for (;;)
      {
        if (word::is_holder (seen))
          {
            if (word::ticks_of (seen) >= Policy::tick_limit ())
              {
                Policy::back_off ();
                Policy::at (split_point_t::cas_read);
                seen = word_.load (std::memory_order_relaxed);
                continue;
              }
            Policy::at (split_point_t::cas_pin);
            split_value_t const found = word_.compare_exchange_strong (
                seen, word::with_tick (seen), rmw_order (success),
                std::memory_order_relaxed);
            if (!(found == seen))
              {
                seen = found;
                continue;
              }
            Policy::note (split_event_t::tick, 1u);
            Policy::ticked (word::ticks_of (seen) + 1u);
            split_value_t const pinned = seen;
            holder_type const *const holder
                = static_cast<holder_type const *> (word::block_of (pinned));
            if (holder->owner () == expected_block
                && holder->pointer () == expected_pointer)
              {
                if (!built)
                  {
                    image = take_image (desired);
                    built = true;
                  }
                split_value_t current = word::with_tick (pinned);
                std::uint32_t moved = word::ticks_of (pinned);
                deposit (current, moved);
                for (;;)
                  {
                    Policy::at (split_point_t::cas);
                    split_value_t const swapped
                        = word_.compare_exchange_strong (
                            current, image, rmw_order (success),
                            std::memory_order_acquire);
                    if (swapped == current)
                      {
                        Policy::note (split_event_t::consumed, 1u);
                        drop_word_with_ext (
                            current, moved - (word::ticks_of (current) - 1u));
                        return true;
                      }
                    if (swapped.lo != pinned.lo
                        || word::ticks_of (swapped) == 0u)
                      {
                        // The holder was swapped out by another writer: pay
                        // the own tick and the own deposit at once, then
                        // look at the word again.
                        pay_back (current, moved);
                        seen = swapped;
                        break;
                      }
                    std::uint32_t const need = word::ticks_of (swapped) - 1u;
                    if (need > moved)
                      {
                        deposit (swapped, need - moved);
                        moved = need;
                      }
                    current = swapped;
                  }
                continue;
              }
            // Another holder: take the own tick back.
            settle (pinned);
          }
        Pointer current = load (failure);
        if (!weak && equivalent (current, expected))
          {
            Policy::at (split_point_t::cas_read);
            seen = Policy::guess (word_);
            continue;
          }
        if (built)
          drop_word (image);
        expected = std::move (current);
        return false;
      }
  }

  // mutable: a load is a compare-and-swap of the word.
  mutable ::lumex::core::atomic::dwcas::dwcas_word word_;
};

#if __cplusplus < 201703L
template <typename Pointer, typename Policy>
LUMEX_CONSTEXPR bool split_count_cell<Pointer, Policy>::is_always_lock_free;
#endif
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_CELL_HPP

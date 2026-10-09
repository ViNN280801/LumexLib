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
 * @file LumexAtomicSmartPtrLockFreeCell.hpp
 * @brief The lock-free engine behind `atomic_shared_ptr_lock_free` and
 * `atomic_weak_ptr_lock_free`: an immutable heap box behind one
 * `std::atomic` pointer, protected by hazard pointers.
 * @details The stored value is a `box_t` (see `LumexAtomicSmartPtrBox.hpp`)
 * that holds an ordinary `std::shared_ptr` or `std::weak_ptr`. The cell is
 * one `std::atomic<box_t *>` (null for the empty value, so the default and
 * `nullptr` constructors are `constexpr`) and the 32-bit wait counter of the
 * other engines. The user-visible type stays the standard smart pointer, so
 * aliasing pointers, custom deleters, `make_shared` and
 * `enable_shared_from_this` work unchanged, and no control block of this
 * library exists.
 *
 * Algorithm. Hazard pointers (Michael) guard the box a thread is about to
 * use: the reader announces the box address in a slot of the hazard domain
 * (`core/hazard_pointer`), issues a fence and checks that the cell still
 * holds it. The announce-validate protocol of Anderson, Blelloch and Wei for
 * atomic reference counted pointers is the same idea applied to loading a
 * `shared_ptr`; here the box stands for the control block, because a
 * `std::shared_ptr` offers no way to take a reference to a control block
 * that is only known by address.
 *
 * - `load`: protect the box, copy its smart pointer, drop the protection.
 *   Nothing in the cell is written; no reader waits for another thread.
 * - `store`, `exchange`: allocate a box (none for the empty value), swap the
 *   pointer in, hand the old box to the reclaim policy. `exchange` returns a
 *   copy of the old box's value made before the hand-over (readers may be
 *   copying it, it cannot be moved out).
 * - `compare_exchange_strong`: protect the current box, test the equivalence
 *   of its value with `expected` (same stored pointer and shared ownership,
 *   or both empty); not equivalent: `expected` receives a copy, no
 *   allocation; equivalent: build the new box once and swap it in with a
 *   pointer compare-exchange *from the protected box*. A box that is
 *   protected cannot have been freed and allocated again, so the pointer
 *   compare-exchange has no ABA problem and needs no version tag. When it
 *   fails because the cell changed, the loop protects the new box and tests
 *   again, so the strong form fails only when the value is not equivalent.
 *   The weak form makes one attempt. A caller that retries
 *   a failed strong exchange only ever retries because another thread made
 *   progress, so the operation is lock-free; `compare_exchange_weak` is the
 *   same with one attempt.
 * - `wait`, `notify_one`, `notify_all`: the epoch counter shared with the
 *   other engines.
 *
 * Destruction of the replaced value. A removed box is destroyed by the
 * policy template argument: `reclaim::immediate` scans the hazard slots for
 * that one box (cost proportional to the number of slots) and destroys it in
 * the call when no reader names it (the deleter of the replaced value, and
 * `use_count ()` of everything it owned, then behave as in the lock-based
 * engine); only a box a reader holds at that moment goes to the retired list
 * of the hazard domain. `reclaim::deferred` always retires. The deleter of a
 * replaced value therefore runs on the replacing thread, inside the call or
 * (retired boxes) inside a later replacing call of any thread, never on a
 * thread that only reads; it may use any atomic smart pointer. The number of
 * boxes that wait is bounded by the hazard module (see its README). The
 * destructor of the cell destroys the last box at once. With the immediate
 * policy a cell that had to retire a box (a reader named it) also runs a pass
 * of the hazard domain (`clean_up ()`), so everything the cell ever held is
 * destroyed by the time the cell is, as with the other engines (a deleter may
 * therefore refer to objects that die with the atomic). With the deferred
 * policy boxes retired earlier stay with the domain until a pass reclaims
 * them, and the deleters of replaced values must outlive the atomic.
 *
 * `is_always_lock_free` and `is_lock_free ()` are true in the steady-state
 * sense: no operation waits for another thread. The first operation of a
 * thread takes a hazard record (a wait-free cache hit afterwards), and an
 * operation that allocates a box may block in the allocator. An allocation
 * failure inside a `noexcept` operation calls `std::terminate`, as does
 * running out of hazard records.
 *
 * Memory orders. Every operation publishes with at least release and
 * observes with at least acquire, because the box contents travel through
 * the pointer; `seq_cst` requests are honoured. `relaxed` and `consume`
 * loads are acquire loads.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LOCK_FREE_CELL_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LOCK_FREE_CELL_HPP

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrBox.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/hazard_pointer/LumexHazardPointer"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @brief Named places in the lock-free engine where a test may stall a
 * thread; empty unless `LUMEX_ATOMIC_SMART_PTR_TEST_HOOKS` is defined.
 * @details A test that defines the macro also defines
 * `LUMEX_ATOMIC_SMART_PTR_TEST_POINT (id)` (a call of its own handler with
 * one of `lumex::core::atomic::smart_ptr::Detail::test_point_id_t`), builds
 * the engine into its own executable and stalls threads there to make a race
 * deterministic: `load_protected` is after a `load` protected the box and
 * before it copies the value, `cas_checked` after a compare-exchange found
 * the current value equivalent and before it swaps the pointer.
 */
#if !defined(LUMEX_ATOMIC_SMART_PTR_TEST_HOOKS)
#define LUMEX_ATOMIC_SMART_PTR_TEST_POINT(id) static_cast<void> (0)
#endif

static_assert (ATOMIC_POINTER_LOCK_FREE == 2,
               "the lock-free engine needs pointer atomics that are always "
               "lock-free");

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
/// Ids of the test points (see `LUMEX_ATOMIC_SMART_PTR_TEST_POINT`).
enum test_point_id_t
{
  load_protected = 1,
  cas_checked = 2
};

/**
 * @brief Lock-free atomic cell over an ordinary smart pointer.
 * @tparam Pointer `std::shared_ptr<T>` or `std::weak_ptr<T>`.
 * @tparam Reclaim `reclaim::immediate` or `reclaim::deferred`.
 */
template <typename Pointer, typename Reclaim>
class lock_free_cell : private epoch_wait_base
{
  using traits_type = smart_ptr_traits_t<Pointer>;
  using probe_type = typename traits_type::probe_t;
  using box_type = box_t<Pointer>;
  using holder_type = ::lumex::core::hazard_pointer::hazard_pointer;

public:
  static LUMEX_CONSTEXPR bool is_always_lock_free = true;

  LUMEX_CONSTEXPR
  lock_free_cell () LUMEX_NOEXCEPT : epoch_wait_base (),
                                     retired_ (false),
                                     word_ (nullptr)
  {
  }

  explicit lock_free_cell (Pointer desired) LUMEX_NOEXCEPT
      : epoch_wait_base (),
        retired_ (false),
        word_ (make_box (std::move (desired)))
  {
  }

  lock_free_cell (lock_free_cell const &) = delete;
  lock_free_cell &operator= (lock_free_cell const &) = delete;

  /// Destroys the last box at once: nobody uses the object any more. With
  /// the immediate policy, a box this cell had to retire (a reader named it
  /// at that moment) is reclaimed by a pass of the hazard domain before the
  /// destructor returns, so everything the cell ever held is destroyed when
  /// the cell is, as with the other engines; no reader names those boxes any
  /// more, because none may be inside an operation of a destroyed object.
  ~lock_free_cell ()
  {
    delete word_.exchange (nullptr, std::memory_order_acquire);
    settle (Reclaim ());
  }

  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return true;
  }

  Pointer
  load (std::memory_order order) const LUMEX_NOEXCEPT
  {
    holder_type holder = acquire_holder ();
    box_type const *const box = protect (holder, order);
    LUMEX_ATOMIC_SMART_PTR_TEST_POINT (load_protected);
    return box == nullptr ? Pointer () : box->value;
  }

  void
  store (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    box_type *const old = word_.exchange (make_box (std::move (desired)),
                                          publish_order (order));
    dispose (old);
  }

  Pointer
  exchange (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    box_type *const old = word_.exchange (make_box (std::move (desired)),
                                          publish_order (order));
    if (old == nullptr)
      return Pointer ();
    // A copy: readers may be copying the box's value right now.
    Pointer result (old->value);
    dispose (old);
    return result;
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order success,
                           std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, false, success);
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, false, order);
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order success,
                         std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, true, success);
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, desired, true, order);
  }

  void
  wait (Pointer const &old, std::memory_order order) const LUMEX_NOEXCEPT
  {
    this->wait_while ([this, &old, order]
                        { return holds_equivalent (old, order); });
  }

  using epoch_wait_base::notify_all;
  using epoch_wait_base::notify_one;

private:
  /// Reads the cell with the validation order of `protect`.
  struct word_source
  {
    std::atomic<box_type *> const *word;
    std::memory_order order;

    box_type *
    operator() () const LUMEX_NOEXCEPT
    {
      return word->load (order);
    }
  };

  /// The word holds the box pointer itself.
  struct identity_filter
  {
    box_type *
    operator() (box_type *box) const LUMEX_NOEXCEPT
    {
      return box;
    }
  };

  /// A hazard record; running out of memory ends the program, because every
  /// operation is `noexcept`.
  static holder_type
  acquire_holder () LUMEX_NOEXCEPT
  {
    try
      {
        return ::lumex::core::hazard_pointer::make_hazard_pointer ();
      }
    catch (...)
      {
        std::terminate ();
      }
  }

  /// Order of the read that validates an announcement, and of loads: the
  /// box contents come through the pointer, so at least acquire.
  static std::memory_order
  observe_order (std::memory_order order) LUMEX_NOEXCEPT
  {
    return order == std::memory_order_seq_cst ? std::memory_order_seq_cst
                                              : std::memory_order_acquire;
  }

  /// Order of the swap that publishes a box and takes the old one: release
  /// for the new box, acquire for the old one.
  static std::memory_order
  publish_order (std::memory_order order) LUMEX_NOEXCEPT
  {
    return order == std::memory_order_seq_cst ? std::memory_order_seq_cst
                                              : std::memory_order_acq_rel;
  }

  /// The box for @p value, or null for the value of a default-constructed
  /// smart pointer (which the null word stands for). The value is moved in.
  /// Running out of memory ends the program.
  static box_type *
  make_box (Pointer &&value) LUMEX_NOEXCEPT
  {
    if (traits_type::is_plain_empty (value))
      return nullptr;
    try
      {
        return new box_type (std::move (value));
      }
    catch (...)
      {
        std::terminate ();
      }
  }

  /// Announces and validates the box the cell holds; the returned box (or
  /// null) cannot be destroyed while @p holder keeps its protection.
  box_type *
  protect (holder_type &holder, std::memory_order order) const LUMEX_NOEXCEPT
  {
    word_source const source = { &word_, observe_order (order) };
    box_type *box = source ();
    if (box == nullptr)
      return nullptr;
    while (!holder.try_protect (box, source, identity_filter ()))
      {
      }
    return box;
  }

  /// Hands a removed box to the policy. The caller holds no hazard pointer
  /// that names it.
  void
  dispose (box_type *box) LUMEX_NOEXCEPT
  {
    if (box != nullptr && !dispose_box (box, Reclaim ()))
      note_retired (Reclaim ());
  }

  /// A retired box of this cell may outlive it unless the destructor
  /// reclaims it (immediate policy); the deferred policy leaves that to the
  /// domain, by definition.
  void
  note_retired (reclaim::immediate) LUMEX_NOEXCEPT
  {
    retired_.store (true, std::memory_order_relaxed);
  }

  void
  note_retired (reclaim::deferred) LUMEX_NOEXCEPT
  {
  }

  void
  settle (reclaim::immediate) LUMEX_NOEXCEPT
  {
    if (retired_.load (std::memory_order_relaxed))
      ::lumex::core::hazard_pointer::clean_up ();
  }

  void
  settle (reclaim::deferred) LUMEX_NOEXCEPT
  {
  }

  bool
  compare_exchange (Pointer &expected, Pointer &desired, bool weak,
                    std::memory_order order) LUMEX_NOEXCEPT
  {
    // Declared before the hazard record, so they are released after it.
    probe_type const expected_probe = traits_type::probe (expected);
    probe_type current_probe;
    Pointer const empty;
    box_type *fresh = nullptr;
    bool built = false;
    holder_type holder = acquire_holder ();
    box_type *current = protect (holder, order);
    for (;;)
      {
        Pointer const &value = current == nullptr ? empty : current->value;
        if (!traits_type::equivalent (value, expected, expected_probe,
                                      current_probe))
          {
            expected = value;
            delete fresh;
            return false;
          }
        if (!built)
          {
            fresh = make_box (std::move (desired));
            built = true;
          }
        LUMEX_ATOMIC_SMART_PTR_TEST_POINT (cas_checked);
        box_type *seen = current;
        bool const exchanged
            = weak ? word_.compare_exchange_weak (seen, fresh,
                                                  publish_order (order),
                                                  std::memory_order_relaxed)
                   : word_.compare_exchange_strong (seen, fresh,
                                                    publish_order (order),
                                                    std::memory_order_relaxed);
        if (exchanged)
          {
            // This thread's own announcement would keep the old box alive.
            holder.reset_protection ();
            dispose (current);
            return true;
          }
        // The cell changed under us (or, weak, failed spuriously).
        current = protect (holder, order);
        if (weak)
          {
            expected = current == nullptr ? empty : current->value;
            delete fresh;
            return false;
          }
      }
  }

  bool
  holds_equivalent (Pointer const &old,
                    std::memory_order order) const LUMEX_NOEXCEPT
  {
    probe_type const old_probe = traits_type::probe (old);
    probe_type current_probe;
    Pointer const empty;
    holder_type holder = acquire_holder ();
    box_type const *const box = protect (holder, order);
    return traits_type::equivalent (box == nullptr ? empty : box->value, old,
                                    old_probe, current_probe);
  }

  std::atomic<bool> retired_;
  std::atomic<box_type *> word_;
};

#if __cplusplus < 201703L
template <typename Pointer, typename Reclaim>
LUMEX_CONSTEXPR bool lock_free_cell<Pointer, Reclaim>::is_always_lock_free;
#endif
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LOCK_FREE_CELL_HPP

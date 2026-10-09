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
 * @file LumexAtomicSmartPtrBox.hpp
 * @brief The immutable heap box of the lock-free engine, and the policies
 * that say when a replaced box is destroyed.
 * @details The lock-free engine (`LumexAtomicSmartPtrLockFreeCell.hpp`) never
 * modifies a stored smart pointer in place. It keeps the value in a `box_t`
 * allocated on the heap and publishes the box through one `std::atomic`
 * pointer. A box is immutable after publication, so many readers may copy its
 * smart pointer at the same time (a concurrent const access), and a box that
 * a reader still names is protected by a hazard pointer
 * (`core/hazard_pointer`) and cannot be freed under it. `box_t` carries the
 * retire node of the hazard module (two words) and the smart pointer (two
 * words): 32 bytes.
 *
 * `reclaim::immediate` and `reclaim::deferred` choose what happens to the
 * box a `store`, `exchange` or successful compare-exchange removed:
 *
 * - `immediate` (the default): the writer scans the hazard slots for that one
 *   box (`hazard_pointer_obj_base::reclaim_or_retire`) and destroys it before
 *   the call returns when no reader names it, so `use_count ()` and the
 *   deleter of the replaced value behave as with the lock-based engine. A box
 *   some reader is copying at that moment is retired instead and destroyed
 *   by a later reclamation pass. The scan costs one read of every hazard
 *   slot ever created per replacing operation.
 * - `deferred`: the box is retired at once (`retire`) and destroyed by a
 *   reclamation pass of the hazard domain on whichever thread runs it, up to
 *   a threshold of retired objects later. `store` is then cheaper, and the
 *   replaced value outlives the call.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`, except
 * the two policy tags, which are template arguments of
 * `atomic_shared_ptr_lock_free` and `atomic_weak_ptr_lock_free`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_BOX_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_BOX_HPP

#include <utility>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/hazard_pointer/LumexHazardPointer"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
/**
 * @brief When the value that a `store`, `exchange` or compare-exchange
 * replaced is destroyed (template argument of the lock-free engine).
 */
namespace reclaim
{
/// Tries to destroy the replaced value inside the replacing call.
struct immediate
{
};

/// Leaves the replaced value to the reclamation passes of the hazard domain.
struct deferred
{
};
} // namespace reclaim

namespace Detail
{
/**
 * @brief One published value of the lock-free engine: a retire node and an
 * immutable smart pointer.
 * @tparam Pointer `std::shared_ptr<T>` or `std::weak_ptr<T>`.
 */
template <typename Pointer>
struct box_t
    : ::lumex::core::hazard_pointer::hazard_pointer_obj_base<box_t<Pointer>>
{
  explicit box_t (Pointer &&desired) LUMEX_NOEXCEPT
      : value (std::move (desired))
  {
  }

  /// Never changed after the box is published.
  Pointer const value;
};

/**
 * @brief Hands a box that was removed from its cell to the policy.
 * @details The caller holds no hazard pointer that names the box.
 */
template <typename Pointer>
void
dispose_box (box_t<Pointer> *box, reclaim::immediate) LUMEX_NOEXCEPT
{
  static_cast<void> (box->reclaim_or_retire ());
}

/// @copydoc dispose_box(box_t<Pointer>*,reclaim::immediate)
template <typename Pointer>
void
dispose_box (box_t<Pointer> *box, reclaim::deferred) LUMEX_NOEXCEPT
{
  box->retire ();
}
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_BOX_HPP

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
 * @file LumexSplitCountTraits.hpp
 * @brief What the split-count engine needs to know about the pointer it
 * stores: the counter group its references use (the ledger), its holder
 * block, and how to take a pointer apart and put it together.
 * @details Two pointer kinds of `lumex::core::smart_ptr` are supported:
 *
 * | Pointer | Ledger of a block word | Holder | Reference of a holder on its
 * owner | | --- | --- | --- | --- | | `shared_ptr<T>` | the strong group
 * (`add_strong`, `transfer_strong_ext`, `untransfer_strong_ext`,
 * `settle_strong`, `settle_strong_n`, `take_and_settle_strong`,
 * `release_strong`, `release_strong_with_ext`) | `ctl_holder` | strong | |
 * `weak_ptr<T>` | the weak ledger (the same operations on the weak ledger) |
 * `ctl_weak_holder` | weak |
 *
 * A holder word always uses the strong group of the holder itself: the
 * atomic object owns one strong reference to the holder, and the readers
 * tick and settle on it; the holder's own reference on its owner is of the
 * kind of the pointer. The pointer classes are taken apart through the
 * module's documented `detail::access` friend: `control`, `stored`, `detach`
 * (the count moves to the caller), `adopt` (a count the caller owns moves
 * into a pointer).
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_TRAITS_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_TRAITS_HPP

#include <cstdint>
#include <exception>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlKinds.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
#include "lumex/core/smart_ptr/weak/LumexWeakPtr.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
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
namespace Detail
{
/// The strong group of a control block.
struct split_strong_ledger_t
{
  typedef ::lumex::core::smart_ptr::detail::ctl_base block_type;

  static void
  add (block_type *block) LUMEX_NOEXCEPT
  {
    block->add_strong ();
  }

  static void
  transfer (block_type *block, std::uint32_t ticks) LUMEX_NOEXCEPT
  {
    block->transfer_strong_ext (ticks);
  }

  static void
  settle (block_type *block) LUMEX_NOEXCEPT
  {
    block->settle_strong ();
  }

  static void
  settle_n (block_type *block, std::uint32_t n) LUMEX_NOEXCEPT
  {
    block->settle_strong_n (n);
  }

  static void
  untransfer (block_type *block, std::uint32_t ticks) LUMEX_NOEXCEPT
  {
    block->untransfer_strong_ext (ticks);
  }

  static void
  take_and_settle (block_type *block) LUMEX_NOEXCEPT
  {
    block->take_and_settle_strong ();
  }

  static void
  release_with_ext (block_type *block, std::uint32_t n) LUMEX_NOEXCEPT
  {
    block->release_strong_with_ext (n);
  }

  static void
  release (block_type *block) LUMEX_NOEXCEPT
  {
    block->release_strong ();
  }
};

/// The weak ledger of a control block.
struct split_weak_ledger_t
{
  typedef ::lumex::core::smart_ptr::detail::ctl_base block_type;

  static void
  add (block_type *block) LUMEX_NOEXCEPT
  {
    block->add_weak ();
  }

  static void
  transfer (block_type *block, std::uint32_t ticks) LUMEX_NOEXCEPT
  {
    block->transfer_weak_ext (ticks);
  }

  static void
  settle (block_type *block) LUMEX_NOEXCEPT
  {
    block->settle_weak ();
  }

  static void
  settle_n (block_type *block, std::uint32_t n) LUMEX_NOEXCEPT
  {
    block->settle_weak_n (n);
  }

  static void
  untransfer (block_type *block, std::uint32_t ticks) LUMEX_NOEXCEPT
  {
    block->untransfer_weak_ext (ticks);
  }

  static void
  take_and_settle (block_type *block) LUMEX_NOEXCEPT
  {
    block->take_and_settle_weak ();
  }

  static void
  release_with_ext (block_type *block, std::uint32_t n) LUMEX_NOEXCEPT
  {
    block->release_weak_with_ext (n);
  }

  static void
  release (block_type *block) LUMEX_NOEXCEPT
  {
    block->release_weak ();
  }
};

/// A stored pointer as the `void *` a holder keeps.
template <typename E>
void *
split_erase (E *pointer) LUMEX_NOEXCEPT
{
  return const_cast<void *> (static_cast<void const volatile *> (pointer));
}

/**
 * @brief Allocates the holder of an alias; running out of memory ends the
 * program, because every operation of the engine is `noexcept`.
 */
template <typename Holder>
Holder *
split_make_holder (::lumex::core::smart_ptr::detail::ctl_base *owner,
                   void *pointer) LUMEX_NOEXCEPT
{
  Holder *holder = nullptr;
  LUMEX_SMART_PTR_TRY { holder = Holder::create (owner, pointer); }
  LUMEX_SMART_PTR_CATCH_ALL { std::terminate (); }
  return holder;
}

/// The traits of a pointer kind; specialized below.
template <typename Pointer> struct split_pointer_traits_t;

/// `lumex::core::smart_ptr::shared_ptr<T>`.
template <typename T>
struct split_pointer_traits_t<::lumex::core::smart_ptr::shared_ptr<T>>
{
  typedef ::lumex::core::smart_ptr::shared_ptr<T> pointer_type;
  typedef typename pointer_type::element_type element_type;
  typedef ::lumex::core::smart_ptr::detail::ctl_base block_type;
  typedef ::lumex::core::smart_ptr::detail::access access_type;
  typedef split_strong_ledger_t ledger_type;
  typedef ::lumex::core::smart_ptr::detail::ctl_holder holder_type;

  /// Pins of a holder word are mirrored in the owner's strong `ext`, so that
  /// `use_count ()` of the owner sees them (HG, see the cell).
  static bool const mirror_holder = true;

  /// The block of @p pointer (null for an empty one).
  static block_type *
  control (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return access_type::control (pointer);
  }

  /// The stored pointer of @p pointer.
  static element_type *
  stored (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return pointer.get ();
  }

  /// Empties @p pointer without dropping its count (the caller owns it).
  static void
  detach (pointer_type &pointer) LUMEX_NOEXCEPT
  {
    element_type *stored_pointer = nullptr;
    access_type::detach (pointer, stored_pointer);
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (stored_pointer);
  }

  /// A pointer that takes over one count of @p block the caller owns (no
  /// count for a null @p block).
  static pointer_type
  adopt (block_type *block, element_type *stored_pointer) LUMEX_NOEXCEPT
  {
    return access_type::template adopt<T> (block, stored_pointer);
  }

  /// A new holder of @p owner and @p stored_pointer (one strong reference
  /// on @p owner).
  static holder_type *
  make_holder (block_type *owner, element_type *stored_pointer) LUMEX_NOEXCEPT
  {
    return split_make_holder<holder_type> (owner,
                                           split_erase (stored_pointer));
  }
};

/// `lumex::core::smart_ptr::weak_ptr<T>`.
template <typename T>
struct split_pointer_traits_t<::lumex::core::smart_ptr::weak_ptr<T>>
{
  typedef ::lumex::core::smart_ptr::weak_ptr<T> pointer_type;
  typedef typename pointer_type::element_type element_type;
  typedef ::lumex::core::smart_ptr::detail::ctl_base block_type;
  typedef ::lumex::core::smart_ptr::detail::access access_type;
  typedef split_weak_ledger_t ledger_type;
  typedef ::lumex::core::smart_ptr::detail::ctl_weak_holder holder_type;

  /// Weak loads do not change `use_count ()`: no mirror.
  static bool const mirror_holder = false;

  /// The block of @p pointer (null for an empty one).
  static block_type *
  control (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return access_type::control (pointer);
  }

  /// The stored pointer of @p pointer.
  static element_type *
  stored (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return access_type::stored (pointer);
  }

  /// Empties @p pointer without dropping its weak count.
  static void
  detach (pointer_type &pointer) LUMEX_NOEXCEPT
  {
    element_type *stored_pointer = nullptr;
    access_type::detach_weak (pointer, stored_pointer);
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (stored_pointer);
  }

  /// A weak pointer that takes over one weak count of @p block the caller
  /// owns (no count for a null @p block).
  static pointer_type
  adopt (block_type *block, element_type *stored_pointer) LUMEX_NOEXCEPT
  {
    return access_type::template adopt_weak<T> (block, stored_pointer);
  }

  /// A new holder of @p owner and @p stored_pointer (one weak reference on
  /// @p owner).
  static holder_type *
  make_holder (block_type *owner, element_type *stored_pointer) LUMEX_NOEXCEPT
  {
    return split_make_holder<holder_type> (owner,
                                           split_erase (stored_pointer));
  }
};
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_TRAITS_HPP

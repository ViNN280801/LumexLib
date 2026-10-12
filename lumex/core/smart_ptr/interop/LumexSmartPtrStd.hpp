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
 * @file LumexSmartPtrStd.hpp
 * @brief Explicit conversions between the module's `shared_ptr` and
 * `std::shared_ptr`: `from_std` and `to_std`.
 * @details The two families have different control blocks and counts, so
 * there is no implicit conversion in either direction: wrapping costs one
 * allocation and the result's `use_count ()` and `owner_before ()` are those
 * of the new block, not of the original pointer.
 *
 * - `from_std (p)` returns a `shared_ptr<T>` whose control block owns a copy
 *   of `p`; the stored pointer is `p.get ()`. A `std::shared_ptr` that was
 *   made by `to_std` is recognised (by its deleter) and unwrapped: the
 *   original `shared_ptr` is returned, re-aliased to `p.get ()`. An empty
 *   `p` gives an empty pointer; an alias of an empty owner gives an alias of
 *   an empty owner.
 * - `to_std (p)` returns a `std::shared_ptr<T>` that owns a copy of `p`
 *   through a deleter and stores `p.get ()`. A `shared_ptr` that was made by
 *   `from_std` is unwrapped: the original `std::shared_ptr` is returned,
 *   re-aliased to `p.get ()`.
 *
 * Unwrapping needs the module's header on both sides and the same copy of
 * the header-only code (the identity of the wrapper is a per-library type
 * identity). Weak pointers do not cross: lock first, convert, and make the
 * weak pointer from the result.
 */
#ifndef LUMEX_CORE_SMART_PTR_INTEROP_SMART_PTR_STD_HPP
#define LUMEX_CORE_SMART_PTR_INTEROP_SMART_PTR_STD_HPP

#include <cstdint>
#include <memory>
#include <utility>

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlKinds.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrTraits.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
namespace detail
{
/// The key under which a `ctl_std` block answers `query` with its
/// `std::shared_ptr<void>`.
class std_owner_key
{
};

/// A control block that owns a `std::shared_ptr<void>` (the owner part of
/// the wrapped pointer).
class ctl_std final : public ctl_base
{
public:
  /// Allocates a block that owns @p owner.
  static ctl_std *
  create (std::shared_ptr<void> owner)
  {
    ctl_std *created = nullptr;
    create_block<ctl_std> (
        block_allocator<ctl_std> (), [&] (void *memory)
          { created = ::new (memory) ctl_std (std::move (owner)); });
    return created;
  }

  void *
  query (void const *key) LUMEX_NOEXCEPT override
  {
    return key == type_id<std_owner_key> ()
               ? static_cast<void *> (std::addressof (owner_))
               : nullptr;
  }

private:
  explicit ctl_std (std::shared_ptr<void> &&owner) LUMEX_NOEXCEPT
      : ctl_base (),
        owner_ (std::move (owner))
  {
  }

  void
  dispose () LUMEX_NOEXCEPT override
  {
    owner_.reset ();
  }

  void
  destroy () LUMEX_NOEXCEPT override
  {
    destroy_block (this, block_allocator<ctl_std> ());
  }

  std::shared_ptr<void> owner_;
};

/// The deleter of the `std::shared_ptr` that `to_std` makes: it owns the
/// module's pointer (as `shared_ptr<void>`, only the owner part matters).
class lumex_owner_deleter
{
public:
  explicit lumex_owner_deleter (shared_ptr<void> owner)
      : owner_ (std::move (owner))
  {
  }

  template <class P>
  void
  operator() (P) LUMEX_NOEXCEPT
  {
    owner_.reset ();
  }

  /// The owner the deleter holds.
  shared_ptr<void> const &
  owner () const LUMEX_NOEXCEPT
  {
    return owner_;
  }

private:
  shared_ptr<void> owner_;
};
} // namespace detail

/**
 * @brief Wraps a `std::shared_ptr` (see the file text).
 * @details Throws `std::bad_alloc` when the control block cannot be
 * allocated.
 */
template <class T>
shared_ptr<T>
from_std (std::shared_ptr<T> const &p)
{
  typedef typename shared_ptr<T>::element_type element_type;
  element_type *const stored = p.get ();
  if (p.use_count () == 0)
    return shared_ptr<T> (shared_ptr<void> (), stored);
  if (detail::lumex_owner_deleter *const wrapper
      = std::get_deleter<detail::lumex_owner_deleter> (p))
    return shared_ptr<T> (wrapper->owner (), stored);
  detail::ctl_std *const block = detail::ctl_std::create (
      std::shared_ptr<void> (p, static_cast<void *> (nullptr)));
  block->set_anchor (reinterpret_cast<std::uintptr_t> (stored));
  return detail::access::adopt<T> (block, stored);
}

/// Wraps a `std::shared_ptr`, taking it over (no extra reference of it).
template <class T>
shared_ptr<T>
from_std (std::shared_ptr<T> &&p)
{
  typedef typename shared_ptr<T>::element_type element_type;
  element_type *const stored = p.get ();
  if (p.use_count () == 0)
    return shared_ptr<T> (shared_ptr<void> (), stored);
  if (detail::lumex_owner_deleter *const wrapper
      = std::get_deleter<detail::lumex_owner_deleter> (p))
    return shared_ptr<T> (wrapper->owner (), stored);
  // (The rvalue aliasing constructor of std is C++20.) Take the owner part,
  // then give up the argument, so no reference of it stays behind.
  std::shared_ptr<void> owner (p, static_cast<void *> (nullptr));
  p.reset ();
  detail::ctl_std *const block = detail::ctl_std::create (std::move (owner));
  block->set_anchor (reinterpret_cast<std::uintptr_t> (stored));
  return detail::access::adopt<T> (block, stored);
}

/**
 * @brief Wraps a `shared_ptr` of this module into a `std::shared_ptr`.
 * @details Throws `std::bad_alloc` when the standard control block cannot be
 * allocated (the pointer @p p is untouched then).
 */
template <class T>
std::shared_ptr<T>
to_std (shared_ptr<T> const &p)
{
  typedef typename shared_ptr<T>::element_type element_type;
  element_type *const stored = p.get ();
  detail::ctl_base *const block = detail::access::control (p);
  if (block == nullptr)
    return std::shared_ptr<T> (std::shared_ptr<void> (), stored);
  if (void *const owner
      = block->query (detail::type_id<detail::std_owner_key> ()))
    return std::shared_ptr<T> (*static_cast<std::shared_ptr<void> *> (owner),
                               stored);
  return std::shared_ptr<T> (
      stored, detail::lumex_owner_deleter (
                  shared_ptr<void> (p, static_cast<void *> (nullptr))));
}
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_INTEROP_SMART_PTR_STD_HPP

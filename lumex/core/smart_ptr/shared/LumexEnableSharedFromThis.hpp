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
 * @file LumexEnableSharedFromThis.hpp
 * @brief `enable_shared_from_this<T>`: lets an object that is owned by a
 * `shared_ptr` of this module hand out further owners of itself
 * ([util.smartptr.enab]).
 * @details Derive publicly from `enable_shared_from_this<T>` (with `T` the
 * derived class). The constructors of `shared_ptr` that take over a raw
 * pointer (`shared_ptr<T> (Y *)`, with a deleter, from a `unique_ptr`) and
 * `make_shared` / `allocate_shared` store a weak reference to the new
 * control block in the base, once; `shared_from_this ()` promotes it
 * (throwing `bad_weak_ptr` when no owner exists, for example in a
 * constructor or for an object that is not owned) and `weak_from_this ()`
 * returns it (C++17). The aliasing constructor, copies and conversions do
 * not touch the base, as in the standard.
 *
 * The module's base is distinct from `std::enable_shared_from_this`; a class
 * may derive from both, and each family sets its own. A class that derives
 * from the standard base only is rejected by `shared_ptr<T> (Y *)` with a
 * `static_assert` (see `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS`).
 */
#ifndef LUMEX_CORE_SMART_PTR_SHARED_ENABLE_SHARED_FROM_THIS_HPP
#define LUMEX_CORE_SMART_PTR_SHARED_ENABLE_SHARED_FROM_THIS_HPP

#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/shared/LumexBadWeakPtr.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
#include "lumex/core/smart_ptr/weak/LumexWeakPtr.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
/**
 * @brief Base class that gives an object access to the `shared_ptr` that
 * owns it.
 * @tparam T The derived class (the type the `shared_ptr` manages or a base
 * of it).
 */
template <class T> class enable_shared_from_this
{
public:
  /// An owner of `*this`; throws `bad_weak_ptr` when none exists.
  shared_ptr<T>
  shared_from_this ()
  {
    return shared_ptr<T> (weak_this_);
  }

  /// An owner of `*this` as a pointer to const.
  shared_ptr<T const>
  shared_from_this () const
  {
    return shared_ptr<T const> (weak_this_);
  }

  /// A weak reference to `*this` (empty if no owner ever took it).
  weak_ptr<T>
  weak_from_this () LUMEX_NOEXCEPT
  {
    return weak_this_;
  }

  /// A weak reference to `*this` as a pointer to const.
  weak_ptr<T const>
  weak_from_this () const LUMEX_NOEXCEPT
  {
    return weak_ptr<T const> (weak_this_);
  }

protected:
  LUMEX_CONSTEXPR_CTOR
  enable_shared_from_this () LUMEX_NOEXCEPT {}

  // A copy is a new object: it has no owner yet.
  enable_shared_from_this (enable_shared_from_this const &) LUMEX_NOEXCEPT {}

  enable_shared_from_this &
  operator= (enable_shared_from_this const &) LUMEX_NOEXCEPT
  {
    return *this;
  }

  ~enable_shared_from_this () = default;

private:
  friend class detail::access;

  mutable weak_ptr<T> weak_this_;
};
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_SHARED_ENABLE_SHARED_FROM_THIS_HPP

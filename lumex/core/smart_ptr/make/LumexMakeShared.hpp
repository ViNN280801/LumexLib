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
 * @file LumexMakeShared.hpp
 * @brief `make_shared` and `allocate_shared`: create the object and its
 * control block in one allocation ([util.smartptr.shared.create]).
 * @details `allocate_shared<T> (alloc, args...)` allocates ONE block with a
 * copy of @p alloc rebound to the block type and constructs the object in it
 * with `allocator_traits::construct` of an allocator rebound to the value
 * type; the block is deallocated with a rebound copy of the allocator when
 * the last owner and the last weak pointer are gone (the object is destroyed
 * with `allocator_traits::destroy` as soon as the last owner is gone, and its
 * memory stays with the block until then). `make_shared<T> (args...)` is the
 * same with the module's allocator (`operator new`, aligned for the type on
 * every standard, over-aligned types included) and constructs the object with
 * `::new (p) T (args...)`. If the constructor throws, the memory goes back to
 * the allocator and the exception propagates. A class with a base
 * `enable_shared_from_this` has its weak reference set; a class with only the
 * standard `std::enable_shared_from_this` base is rejected by a
 * `static_assert`, as by `shared_ptr (Y *)`.
 *
 * The array forms of C++20 (`make_shared<T[]>`, `make_shared<T[N]>`) are not
 * provided; naming one is a compile error with a message. Allocators must
 * have raw pointers.
 */
#ifndef LUMEX_CORE_SMART_PTR_MAKE_MAKE_SHARED_HPP
#define LUMEX_CORE_SMART_PTR_MAKE_MAKE_SHARED_HPP

#include <cstdint>
#include <type_traits>
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
/**
 * @brief Creates a `T` constructed from @p args in one allocation made with
 * a copy of @p alloc.
 * @tparam T The object type: not an array; cv-qualified types are allowed
 * and constructed as their unqualified type.
 * @tparam A The allocator type (any type with the allocator requirements,
 * rebindable to the block).
 * @return A `shared_ptr<T>` that owns the new object.
 */
template <class T, class A, class... Args>
shared_ptr<T>
allocate_shared (A const &alloc, Args &&...args)
{
  static_assert (!std::is_array<T>::value,
                 "smart_ptr: make_shared / allocate_shared of an array "
                 "(C++20) is not provided");
  typedef typename std::remove_cv<T>::type value_type;
  detail::check_std_enable_shared_from_this<value_type> ();
  typedef detail::ctl_inplace<T, A> block_type;
  block_type *const block
      = block_type::create (alloc, std::forward<Args> (args)...);
  value_type *const object = block->object ();
  block->set_anchor (reinterpret_cast<std::uintptr_t> (object));
  shared_ptr<T> result = detail::access::adopt<T> (block, object);
  detail::esft_dispatch<value_type>::apply (result, object);
  return result;
}

/**
 * @brief Creates a `T` constructed from @p args in one allocation.
 * @details See the file text; equivalent to `allocate_shared<T>` with the
 * module's stateless allocator.
 */
template <class T, class... Args>
shared_ptr<T>
make_shared (Args &&...args)
{
  return smart_ptr::allocate_shared<T> (
      detail::block_allocator<typename std::remove_cv<T>::type> (),
      std::forward<Args> (args)...);
}
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_MAKE_MAKE_SHARED_HPP

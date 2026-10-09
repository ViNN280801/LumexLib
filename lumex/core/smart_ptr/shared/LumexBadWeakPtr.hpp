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
 * @file LumexBadWeakPtr.hpp
 * @brief `bad_weak_ptr`: the exception that `shared_ptr (weak_ptr)` and
 * `shared_from_this ()` throw when the object is gone.
 * @details The class derives from `std::bad_weak_ptr`, so
 * `catch (std::bad_weak_ptr const &)` and `catch (std::exception const &)`
 * handlers see it as well; it is a type of the module, not an alias
 * ([util.smartptr.weak.bad]). `what ()` is the one of the standard class.
 */
#ifndef LUMEX_CORE_SMART_PTR_SHARED_BAD_WEAK_PTR_HPP
#define LUMEX_CORE_SMART_PTR_SHARED_BAD_WEAK_PTR_HPP

#include <memory>

namespace lumex
{
namespace core
{
namespace smart_ptr
{
/**
 * @brief Thrown by `shared_ptr<T> (weak_ptr<Y> const &)` and by
 * `enable_shared_from_this::shared_from_this ()` when the weak pointer is
 * expired.
 */
class bad_weak_ptr : public std::bad_weak_ptr
{
public:
  bad_weak_ptr () = default;
};
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_SHARED_BAD_WEAK_PTR_HPP

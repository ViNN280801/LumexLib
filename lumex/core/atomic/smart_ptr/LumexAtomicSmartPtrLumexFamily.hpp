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
 * @file LumexAtomicSmartPtrLumexFamily.hpp
 * @brief The equivalence traits of the module's own pointer family
 * (`lumex::core::smart_ptr::shared_ptr` / `weak_ptr`) for the lock-based
 * cell, which `atomic_shared_ptr_lock_based_lumex` and
 * `atomic_weak_ptr_lock_based_lumex` use.
 * @details Two pointers of the family are equivalent when they store the same
 * pointer and share the control block, or are both empty
 * ([util.smartptr.atomic]). Unlike `std::weak_ptr`, a weak pointer of the
 * family exposes its stored pointer (`detail::access::stored`), so the
 * equivalence of weak pointers is exact and needs no `lock ()`.
 *
 * Included by `LumexAtomicSharedPtr.hpp` and `LumexAtomicWeakPtr.hpp` where
 * `LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY` is 1. Everything here is an
 * implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LUMEX_FAMILY_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LUMEX_FAMILY_HPP

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
#include "lumex/core/smart_ptr/weak/LumexWeakPtr.hpp"
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
/// Equivalence of `lumex::core::smart_ptr::shared_ptr`: same stored pointer
/// and same control block (or both empty).
template <typename T>
struct smart_ptr_traits_t<::lumex::core::smart_ptr::shared_ptr<T>>
{
  typedef ::lumex::core::smart_ptr::shared_ptr<T> pointer_type;
  typedef ::lumex::core::smart_ptr::detail::access access_type;

  /// A pointer of the family needs no extra state to be compared.
  struct probe_t
  {
  };

  static probe_t
  probe (pointer_type const &) LUMEX_NOEXCEPT
  {
    return probe_t ();
  }

  /// True for a pointer that owns nothing and stores a null pointer.
  static bool
  is_plain_empty (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return access_type::control (pointer) == nullptr
           && pointer.get () == nullptr;
  }

  static bool
  equivalent (pointer_type const &current, pointer_type const &expected,
              probe_t const &, probe_t &) LUMEX_NOEXCEPT
  {
    return current.get () == expected.get ()
           && access_type::control (current)
                  == access_type::control (expected);
  }
};

/// Equivalence of `lumex::core::smart_ptr::weak_ptr`: same stored pointer
/// and same control block (or both empty), read without `lock ()`.
template <typename T>
struct smart_ptr_traits_t<::lumex::core::smart_ptr::weak_ptr<T>>
{
  typedef ::lumex::core::smart_ptr::weak_ptr<T> pointer_type;
  typedef ::lumex::core::smart_ptr::detail::access access_type;

  /// A pointer of the family needs no extra state to be compared.
  struct probe_t
  {
  };

  static probe_t
  probe (pointer_type const &) LUMEX_NOEXCEPT
  {
    return probe_t ();
  }

  /// True for a weak pointer that observes nothing and stores null.
  static bool
  is_plain_empty (pointer_type const &pointer) LUMEX_NOEXCEPT
  {
    return access_type::control (pointer) == nullptr
           && access_type::stored (pointer) == nullptr;
  }

  static bool
  equivalent (pointer_type const &current, pointer_type const &expected,
              probe_t const &, probe_t &) LUMEX_NOEXCEPT
  {
    return access_type::stored (current) == access_type::stored (expected)
           && access_type::control (current)
                  == access_type::control (expected);
  }
};
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_LUMEX_FAMILY_HPP

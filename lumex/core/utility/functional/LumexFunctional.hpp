/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexFunctional.hpp
 * @brief `identity` and `less`, the function objects of this library that
 * correspond to `std::identity` (C++20) and `std::less<void>` (C++14).
 * @details Both are transparent (they have a nested `is_transparent`), take
 * their arguments by forwarding reference and work in constant expressions.
 * They are classes of this library in every standard from C++11, never
 * aliases of the standard ones, so code that passes them to an algorithm as a
 * default projection and a default comparison is the same in every standard.
 * Each converts implicitly to and from its standard twin where the twin
 * exists (`identity` and `std::identity` with the C++20 `<functional>`,
 * `LUMEX_HAS_STD_RANGES`; `less` and `std::less<void>` from C++14,
 * `LUMEX_HAS_STD_TRANSPARENT_OPERATORS`), so `std::map<K, V, std::less<>> m
 * (lumex::core::utility::functional::less ())` and a function that takes a
 * `std::identity` accept the function objects of this library. The
 * conversions are `constexpr` and `noexcept`; the classes stay empty.
 *
 * `less` is the counterpart of `std::less<void>`, not of `std::ranges::less`:
 * both compare with `<`, but the ranges one also requires
 * `std::totally_ordered_with` of its operands, which a C++11 form cannot
 * express. Both give a strict total order for pointers into different objects
 * (they compare as `void const volatile *`).
 */
#ifndef LUMEX_CORE_UTILITY_FUNCTIONAL_HPP
#define LUMEX_CORE_UTILITY_FUNCTIONAL_HPP

#include <functional>
#include <type_traits>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace functional
{
/**
 * @brief The function object that returns its argument unchanged, as the same
 * reference (the counterpart of `std::identity`, C++20).
 * @details `identity () (value)` has the type `T &&` for an argument of type
 * `T`: an lvalue stays an lvalue, an rvalue stays an rvalue, nothing is
 * copied or moved. It is the default projection of an algorithm that takes
 * one.
 */
struct identity
{
  /** @brief Marks the object as transparent to heterogeneous lookup. */
  using is_transparent = void;

#if LUMEX_HAS_STD_RANGES
  /** @brief The empty object; trivial, as `std::identity`. */
  identity () = default;

  /** @brief Implicit construction from `std::identity` (C++20). */
  LUMEX_CONSTEXPR_CTOR
  identity (std::identity /*unused*/) LUMEX_NOEXCEPT {}

  /** @brief Implicit conversion to `std::identity` (C++20). */
  LUMEX_CONSTEXPR
  operator std::identity () const LUMEX_NOEXCEPT { return std::identity (); }
#endif

  /**
   * @brief Returns `value` as it came.
   * @param value The argument, forwarded.
   * @return `value` as `T &&`.
   */
  template <typename T>
  LUMEX_CONSTEXPR T &&
  operator() (T &&value) const LUMEX_NOEXCEPT
  {
    return static_cast<T &&> (value);
  }
};

namespace detail
{
/**
 * @brief Both operands are pointers to objects (an array counts as one after
 * the array-to-pointer conversion), so `<` between them is the built-in
 * comparison of pointers, which is only unspecified for pointers into
 * different objects.
 */
template <typename T, typename U>
struct are_object_pointers
    : std::integral_constant<
          bool, std::is_pointer<typename std::decay<T>::type>::value
                    && std::is_pointer<typename std::decay<U>::type>::value
                    && std::is_convertible<typename std::decay<T>::type,
                                           void const volatile *>::value
                    && std::is_convertible<typename std::decay<U>::type,
                                           void const volatile *>::value>
{
};

/** @brief The comparison is `left < right`. */
template <typename T, typename U>
LUMEX_CONSTEXPR auto
compare_less (T &&left, U &&right, std::false_type)
    LUMEX_NOEXCEPT_IF (noexcept (static_cast<T &&> (left)
                                 < static_cast<U &&> (right)))
        -> decltype (static_cast<T &&> (left) < static_cast<U &&> (right))
{
  return static_cast<T &&> (left) < static_cast<U &&> (right);
}

/** @brief Two pointers compare as the strict total order of `std::less`. */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
compare_less (T &&left, U &&right, std::true_type) LUMEX_NOEXCEPT
{
  return std::less<void const volatile *> () (
      static_cast<void const volatile *> (left),
      static_cast<void const volatile *> (right));
}
} // namespace detail

/**
 * @brief The function object that compares two operands of any types with
 * `<` (the counterpart of `std::less<void>`, C++14).
 * @details `less () (left, right)` is `left < right` with both operands
 * forwarded, so a mixed comparison such as `std::string` with
 * `char const *` works, and the result is whatever `<` returns. If both
 * operands are pointers to objects the order is the strict total order of
 * `std::less<void const volatile *>`, as the standard requires of
 * `std::less<void>`. It does not take part in overload resolution when the
 * operands cannot be compared with `<`.
 */
struct less
{
  /** @brief Marks the object as transparent to heterogeneous lookup. */
  using is_transparent = void;

#if LUMEX_HAS_STD_TRANSPARENT_OPERATORS
  /** @brief The empty object; trivial, as `std::less<void>`. */
  less () = default;

  /** @brief Implicit construction from `std::less<void>` (C++14). */
  LUMEX_CONSTEXPR_CTOR
  less (std::less<void> /*unused*/) LUMEX_NOEXCEPT {}

  /** @brief Implicit conversion to `std::less<void>` (C++14). */
  LUMEX_CONSTEXPR
  operator std::less<void> () const LUMEX_NOEXCEPT
  {
    return std::less<void> ();
  }
#endif

  /**
   * @brief Compares `left` with `right`.
   * @param left The left operand, forwarded.
   * @param right The right operand, forwarded.
   * @return `left < right`.
   */
  template <typename T, typename U>
  LUMEX_CONSTEXPR auto
  operator() (T &&left, U &&right) const
      LUMEX_NOEXCEPT_IF (noexcept (static_cast<T &&> (left)
                                   < static_cast<U &&> (right)))
          -> decltype (static_cast<T &&> (left) < static_cast<U &&> (right))
  {
    return detail::compare_less (static_cast<T &&> (left),
                                 static_cast<U &&> (right),
                                 detail::are_object_pointers<T, U> ());
  }
};
} // namespace functional
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_FUNCTIONAL_HPP

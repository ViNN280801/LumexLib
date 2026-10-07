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
 * @file Unexpected.hpp
 * @brief `unexpected`, the wrapper that carries an error into an `expected`;
 * an analogue of C++23 `std::unexpected`.
 * @details Constructing an `expected` from an `unexpected<E>` puts it in the
 * error state, and `make_unexpected<E>()` of `Expected.hpp` builds one in
 * place. The class has the members of `std::unexpected`: the converting and
 * in-place constructors (also from an `std::initializer_list`), `error()` in
 * the four value categories, `swap()`, the non-member `swap()` and the
 * comparisons `==` and `!=` between `unexpected` objects, and, from C++17, the
 * deduction guide. Header-only, part of `lumex::expected` and usable from
 * C++11; the class is also visible at global scope.
 */
#ifndef LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP
#define LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP

#include <initializer_list>
#include <type_traits>
#include <utility>

#include "lumex/core/expected/result/ExpectedTypes.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// ====================== unexpected class (C++23 analogue)
// ======================

namespace lumex
{
namespace core
{
namespace expected
{
namespace error
{
template <typename ErrorType> class unexpected;

namespace detail
{
/// @brief `T` is a specialization of `unexpected`.
template <typename T> struct is_unexpected : std::false_type
{
};

template <typename ErrorType>
struct is_unexpected<unexpected<ErrorType>> : std::true_type
{
};

namespace swap_adl
{
using std::swap;

template <typename T, typename = void>
struct is_swappable_impl : std::false_type
{
};

template <typename T>
struct is_swappable_impl<
    T, lumex::core::utility::traits::meta::void_t<decltype (swap (
           std::declval<T &> (), std::declval<T &> ()))>> : std::true_type
{
};

template <typename T, bool Swappable = is_swappable_impl<T>::value>
struct is_nothrow_swappable_impl : std::false_type
{
};

template <typename T>
struct is_nothrow_swappable_impl<T, true>
    : std::integral_constant<bool,
                             LUMEX_NOEXCEPT_IF (swap (std::declval<T &> (),
                                                      std::declval<T &> ()))>
{
};
} // namespace swap_adl

/// @brief `swap (a, b)` found by argument-dependent lookup (or `std::swap`)
/// is well-formed for two `T` lvalues (`std::is_swappable`, C++17).
template <typename T> struct is_swappable : swap_adl::is_swappable_impl<T>
{
};

/// @brief Such a `swap` is `noexcept` (`std::is_nothrow_swappable`, C++17).
template <typename T>
struct is_nothrow_swappable : swap_adl::is_nothrow_swappable_impl<T>
{
};
} // namespace detail

/**
 * @brief Wrapper that holds an error value for `expected`.
 * @details Used to construct an `expected` in the error state. Analogue of
 * `std::unexpected` from C++23.
 * @tparam ErrorType Type of the stored error value.
 * @note An `unexpected` object is always in the error state.
 */
template <typename ErrorType> class unexpected
{
public:
  /**
   * @brief Constructs from a const lvalue error.
   * @param[in] error Const reference to the error to store.
   * @note Not declared `noexcept`; throws whatever the copy constructor of
   * `ErrorType` throws.
   */
  explicit unexpected (ErrorType const &error) : m_error (error) {}

  /**
   * @brief Constructs from an rvalue error.
   * @param[in] error Rvalue reference to the error to store.
   * @note Not declared `noexcept`; throws whatever the move constructor of
   * `ErrorType` throws.
   */
  explicit unexpected (ErrorType &&error) : m_error (std::move (error)) {}

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @return Reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType &
  error () &
  {
    return m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @return Const reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType const &
  error () const &
  {
    return m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @return Rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType &&
  error () &&
  {
    return std::move (m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @return Const rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType const &&
  error () const &&
  {
    return std::move (m_error);
  }

private:
  /**
   * @brief Stored error value.
   * @details Holds an `ErrorType` object that describes the error.
   */
  ErrorType m_error; ///< Stored error value.
};

} // namespace error
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::error::unexpected;

#endif // !LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP

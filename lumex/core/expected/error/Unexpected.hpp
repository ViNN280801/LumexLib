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
 * C++11. The class has no global alias: write
 * `lumex::core::expected::error::unexpected` or a using-declaration of your
 * own.
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
// No `using std::swap` here: these traits probe the swap a type brings with
// itself (found by argument-dependent lookup), exactly as std::is_swappable
// does at C++17, where std::swap is constrained and cannot make a `const` type
// look swappable.
template <typename T, typename = void>
struct adl_swappable_impl : std::false_type
{
};

template <typename T>
struct adl_swappable_impl<
    T, lumex::core::utility::traits::meta::void_t<decltype (swap (
           std::declval<T &> (), std::declval<T &> ()))>> : std::true_type
{
};

template <typename T, bool Swappable = adl_swappable_impl<T>::value>
struct adl_nothrow_impl : std::false_type
{
};

template <typename T>
struct adl_nothrow_impl<T, true>
    : std::integral_constant<bool,
                             LUMEX_NOEXCEPT_IF (swap (std::declval<T &> (),
                                                      std::declval<T &> ()))>
{
};

/// @brief The `std::swap` fallback and its Constraints: the type is
/// MoveConstructible and MoveAssignable (the C++17 signature of `std::swap`).
/// Spelled out because below C++17 MSVC's `std::swap` carries no constraints,
/// so the swap expression alone would report a `const` type, or one with a
/// deleted assignment, as swappable.
template <typename T>
struct std_swap_fallback
    : std::integral_constant<bool, std::is_move_constructible<T>::value
                                       && std::is_move_assignable<T>::value>
{
};
} // namespace swap_adl

/// @brief `T` has a swap of its own (found by argument-dependent lookup), or
/// the `std::swap` fallback applies (`std::is_swappable`, C++17). Below C++17
/// the standard libraries differ for the fallback, so its Constraints are
/// spelled out here.
template <typename T>
struct is_swappable
    : std::integral_constant<bool,
                             swap_adl::adl_swappable_impl<T>::value
                                 || swap_adl::std_swap_fallback<T>::value>
{
};

/// @brief Such a `swap` is `noexcept` (`std::is_nothrow_swappable`, C++17).
/// For the fallback this is `std::swap`'s own `noexcept`: nothrow move
/// construction and nothrow move assignment.
template <typename T, bool = is_swappable<T>::value>
struct is_nothrow_swappable_impl : std::false_type
{
};

template <typename T>
struct is_nothrow_swappable_impl<T, true>
    : std::integral_constant<
          bool, swap_adl::adl_nothrow_impl<T>::value
                    || (swap_adl::std_swap_fallback<T>::value
                        && std::is_nothrow_move_constructible<T>::value
                        && std::is_nothrow_move_assignable<T>::value)>
{
};

/// @brief Such a `swap` is `noexcept` (`std::is_nothrow_swappable`, C++17).
template <typename T>
struct is_nothrow_swappable : is_nothrow_swappable_impl<T>
{
};

} // namespace detail

/**
 * @brief Wrapper that holds an error value for `expected`.
 * @details Used to construct an `expected` in the error state. Analogue of
 * `std::unexpected` from C++23.
 * @tparam ErrorType Type of the stored error value: an object type that is
 * not an array, not cv-qualified and not a specialization of `unexpected`.
 * @note An `unexpected` object is always in the error state.
 */
template <typename ErrorType> class unexpected
{
  LUMEX_STATIC_ASSERT_MSG (std::is_object<ErrorType>::value,
                           "unexpected<E>: E must be an object type (not a "
                           "reference, a function or void)");
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<ErrorType>::value,
                           "unexpected<E>: E must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!detail::is_unexpected<ErrorType>::value,
                           "unexpected<E>: E must not be a specialization of "
                           "unexpected");
  LUMEX_STATIC_ASSERT_MSG (!std::is_const<ErrorType>::value
                               && !std::is_volatile<ErrorType>::value,
                           "unexpected<E>: E must not be cv-qualified");

public:
  /**
   * @brief Constructs the error from one argument.
   * @tparam Err Type of the argument; defaults to `ErrorType`.
   * @param[in] error Value the stored error is constructed from.
   * @note The constructor is `explicit` and takes anything `ErrorType` can be
   * constructed from, so `unexpected<std::string> ("text")` and a move-only
   * error both work. Not selected for an `unexpected` (that is the copy or
   * move constructor) and for the `in_place` tag.
   * @note `noexcept` when the construction of `ErrorType` is.
   */
  template <
      typename Err = ErrorType,
      typename = typename std::enable_if<
          !std::is_same<lumex::core::utility::traits::meta::CleanType<Err>,
                        unexpected>::value
          && !std::is_same<lumex::core::utility::traits::meta::CleanType<Err>,
                           lumex::core::expected::result::in_place_tag>::value
          && std::is_constructible<ErrorType, Err>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit unexpected (Err &&error)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, Err>::value)
      : m_error (std::forward<Err> (error))
  {
  }

  /**
   * @brief Constructs the error in place from several arguments.
   * @tparam Args Argument types forwarded to the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @note `noexcept` when the construction of `ErrorType` is.
   */
  template <typename... Args,
            typename = typename std::enable_if<
                std::is_constructible<ErrorType, Args...>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit unexpected (
      lumex::core::expected::result::in_place_tag /* unused */, Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, Args...>::value)
      : m_error (std::forward<Args> (args)...)
  {
  }

  /**
   * @brief Constructs the error in place from an initializer list and further
   * arguments.
   * @tparam U Element type of the initializer list.
   * @tparam Args Argument types forwarded after the list.
   * @param[in] list Initializer list passed to the `ErrorType` constructor.
   * @param[in] args Arguments forwarded after the list.
   * @note `noexcept` when the construction of `ErrorType` is.
   */
  template <typename U, typename... Args,
            typename = typename std::enable_if<std::is_constructible<
                ErrorType, std::initializer_list<U> &, Args...>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit unexpected (
      lumex::core::expected::result::in_place_tag /* unused */,
      std::initializer_list<U> list, Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, std::initializer_list<U> &,
                                        Args...>::value)
      : m_error (list, std::forward<Args> (args)...)
  {
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @return Reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  LUMEX_CONSTEXPR_CXX14 ErrorType &
      error ()
      & LUMEX_NOEXCEPT
  {
    return m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @return Const reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &
  error () const &LUMEX_NOEXCEPT
  {
    return m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @return Rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  LUMEX_CONSTEXPR_CXX14 ErrorType &&
      error ()
      && LUMEX_NOEXCEPT
  {
    return std::move (m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @return Const rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  LUMEX_CONSTEXPR_CXX14 ErrorType const &&
  error () const &&LUMEX_NOEXCEPT
  {
    return std::move (m_error);
  }

  /**
   * @brief Exchanges the stored error with the one of `other`.
   * @param[in,out] other The `unexpected` to swap with.
   * @note `noexcept` when swapping two `ErrorType` objects is.
   */
  LUMEX_CONSTEXPR_CXX14 void
  swap (unexpected &other)
      LUMEX_NOEXCEPT_IF (detail::is_nothrow_swappable<ErrorType>::value)
  {
    using std::swap;
    swap (m_error, other.m_error);
  }

private:
  /**
   * @brief Stored error value.
   * @details Holds an `ErrorType` object that describes the error.
   */
  ErrorType m_error; ///< Stored error value.
};

/**
 * @brief Exchanges two `unexpected` objects.
 * @tparam ErrorType Error type; must be swappable.
 * @param[in,out] lhs First object.
 * @param[in,out] rhs Second object.
 * @note Takes part in overload resolution only when `ErrorType` is
 * swappable.
 */
template <typename ErrorType>
LUMEX_CONSTEXPR_CXX14
    typename std::enable_if<detail::is_swappable<ErrorType>::value>::type
    swap (unexpected<ErrorType> &lhs, unexpected<ErrorType> &rhs)
        LUMEX_NOEXCEPT_IF (detail::is_nothrow_swappable<ErrorType>::value)
{
  lhs.swap (rhs);
}

/**
 * @brief Compares the errors of two `unexpected` objects.
 * @tparam ErrorType Error type of the left operand.
 * @tparam OtherError Error type of the right operand; the two errors must be
 * comparable with `==`.
 * @param[in] lhs Left operand.
 * @param[in] rhs Right operand.
 * @return `lhs.error () == rhs.error ()`.
 * @note Takes part in overload resolution only when the two errors can be
 * compared with `==`.
 */
template <typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    std::is_convertible<decltype (std::declval<ErrorType const &> ()
                                  == std::declval<OtherError const &> ()),
                        bool>::value,
    bool>::type
operator== (unexpected<ErrorType> const &lhs,
            unexpected<OtherError> const &rhs)
{
  return static_cast<bool> (lhs.error () == rhs.error ());
}

#if !LUMEX_HAS_IMPL_THREE_WAY_COMPARISON
/**
 * @brief Compares the errors of two `unexpected` objects for inequality.
 * @tparam ErrorType Error type of the left operand.
 * @tparam OtherError Error type of the right operand.
 * @param[in] lhs Left operand.
 * @param[in] rhs Right operand.
 * @return `!(lhs == rhs)`.
 * @note Before C++20 only: from C++20 the compiler rewrites `a != b` from
 * `operator==`, as it does for `std::unexpected`. Takes part in overload
 * resolution only when the two errors can be compared with `==`.
 */
template <typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    std::is_convertible<decltype (std::declval<ErrorType const &> ()
                                  == std::declval<OtherError const &> ()),
                        bool>::value,
    bool>::type
operator!= (unexpected<ErrorType> const &lhs,
            unexpected<OtherError> const &rhs)
{
  return !static_cast<bool> (lhs.error () == rhs.error ());
}
#endif

// `__cplusplus`, not LUMEX_HAS_DEDUCTION_GUIDES: GCC 8 supports the guide but
// reports `__cpp_deduction_guides` 201606L at C++17.
#if __cplusplus >= 201703L
/// @brief `unexpected (error)` deduces `unexpected<decltype (error)>` (C++17).
template <typename ErrorType> unexpected (ErrorType) -> unexpected<ErrorType>;
#endif

} // namespace error
} // namespace expected
} // namespace core
} // namespace lumex

// `unexpected` has no global alias: the MinGW runtime declares a global
// function `unexpected` in <eh.h>, and a global name for the class would stop
// a file from including both. Write
// `lumex::core::expected::error::unexpected`; the namespace
// `lumex::core::expected::result` has `using error::unexpected`.

#endif // !LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP

/**
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
 * @file SuccessFailure.hpp
 * @brief The return markers `success()` and `failure()` for functions that
 * return an `Expected`.
 * @details `return success();`, `return success(value);` and
 * `return failure(error);` produce `success_t` and `failure_t` objects that
 * convert implicitly into the `Expected<T, E>` the function returns, so the
 * return statement need not spell that type. A conversion takes part in
 * overload resolution only when the target value or error type can be
 * constructed from what the marker holds; `success()` without an argument
 * converts into `Expected<void, E>` or into an `Expected` whose value type is
 * default-constructible.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_SUCCESS_FAILURE_HPP
#define LUMEX_CORE_EXPECTED_RESULT_SUCCESS_FAILURE_HPP

#include <type_traits>
#include <utility>

#include "lumex/core/expected/result/Expected.hpp"
#include "lumex/core/expected/result/ExpectedTypes.hpp"
#include "lumex/core/expected/result/ExpectedVoid.hpp"

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ====================== success() / failure() markers ======================
// Sugar for `return` in a function whose return type is already an Expected:
// `return success();`, `return success(value);`, `return failure(error);`.
// A marker carries what it was given and converts into any Expected the target
// declaration accepts; nothing about the marker is tied to one Expected type.

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
/**
 * @brief Marker returned by success(value): success carrying a value.
 * @details The conversion is implicit by design - that is the point of the
 * marker - and is removed by SFINAE when the target success type cannot be
 * built from the stored value, so a wrong target is a compile error at the
 * `return`, not a silent reinterpretation.
 * @tparam ValueType Stored success type (decayed, never a reference).
 */
template <typename ValueType> struct success_t
{
  /**
   * @brief Constructs from a const lvalue value.
   * @param[in] value Value to store.
   * @note Noexcept if `ValueType`'s copy constructor does not throw.
   */
  explicit success_t (ValueType const &value) : m_value (value) {}

  /**
   * @brief Constructs from an rvalue value.
   * @param[in] value Value to store.
   * @note Noexcept if `ValueType`'s move constructor does not throw.
   */
  explicit success_t (ValueType &&value) : m_value (std::move (value)) {}

  /**
   * @brief Returns the stored value (mutable lvalue).
   * @return Reference to the stored `ValueType`.
   * @note This function does not throw.
   */
  ValueType &
  value () &
  {
    return m_value;
  }

  /**
   * @brief Returns the stored value (const lvalue).
   * @return Const reference to the stored `ValueType`.
   * @note This function does not throw.
   */
  ValueType const &
  value () const &
  {
    return m_value;
  }

  /**
   * @brief Returns the stored value (rvalue).
   * @return Rvalue reference to the stored `ValueType`.
   * @note This function does not throw.
   */
  ValueType &&
  value () &&
  {
    return std::move (m_value);
  }

  /**
   * @brief Implicit conversion into a successful `Expected` (rvalue marker).
   * @tparam Target Success type of the produced `Expected`.
   * @tparam ErrorType Error type of the produced `Expected`.
   * @return `Expected<Target, ErrorType>` holding the stored value.
   * @note Removed by SFINAE when `Target` cannot be built from `ValueType &&`;
   * use the const overload for a named marker.
   */
  template <typename Target, typename ErrorType,
            typename = typename std::enable_if<
                std::is_constructible<Target, ValueType &&>::value>::type>
  operator Expected<Target, ErrorType> () &&
  {
    return Expected<Target, ErrorType> (std::move (m_value));
  }

  /**
   * @brief Implicit conversion into a successful `Expected` (const lvalue
   * marker).
   * @tparam Target Success type of the produced `Expected`.
   * @tparam ErrorType Error type of the produced `Expected`.
   * @return `Expected<Target, ErrorType>` holding a copy of the stored value.
   * @note Removed by SFINAE when `Target` cannot be built from
   * `ValueType const &`.
   */
  template <typename Target, typename ErrorType,
            typename = typename std::enable_if<
                std::is_constructible<Target, ValueType const &>::value>::type>
  operator Expected<Target, ErrorType> () const &
  {
    return Expected<Target, ErrorType> (m_value);
  }

private:
  ValueType m_value; ///< Stored success value.
};

/**
 * @brief Marker returned by success(): a successful call with no value.
 * @details Converts into `Expected<void, ErrorType>` (success without a value)
 * or into `Expected<Target, ErrorType>` when `Target` is
 * default-constructible; the `void` success type is what keeps the two
 * conversions from competing.
 * @note Stateless: it carries no data.
 */
template <> struct success_t<void>
{
  /**
   * @brief Implicit conversion into a successful `Expected<Target,
   * ErrorType>`.
   * @tparam Target Success type; must be default-constructible.
   * @tparam ErrorType Error type of the produced `Expected`.
   * @return `Expected<Target, ErrorType>` in the success state, holding a
   * default-constructed value.
   * @note Removed by SFINAE when `Target` is not default-constructible (`void`
   * included), which sends `Expected<void, E>` to the overload below.
   */
  template <typename Target, typename ErrorType,
            typename = typename std::enable_if<
                std::is_default_constructible<Target>::value>::type>
  operator Expected<Target, ErrorType> () const
  {
    return Expected<Target, ErrorType> ();
  }

  /**
   * @brief Implicit conversion into a successful `Expected<void, ErrorType>`.
   * @tparam ErrorType Error type of the produced `Expected`.
   * @return `Expected<void, ErrorType>` in the success state.
   */
  template <typename ErrorType>
  operator Expected<void, ErrorType> () const
  {
    return Expected<void, ErrorType> ();
  }
};

/**
 * @brief Marker returned by failure(error): a failed call carrying an error.
 * @details The stored error does not have to match the target error type
 * exactly - anything the target error type can be built from works, so a
 * literal, a `std::string` or a richer error type can all be returned the same
 * way. The conversion is implicit by design and removed by SFINAE when the
 * target error type cannot be built from the stored error.
 * @tparam ErrorType Stored error type (decayed, never a reference).
 */
template <typename ErrorType> struct failure_t
{
  /**
   * @brief Constructs from a const lvalue error.
   * @param[in] error Error to store.
   * @note Noexcept if `ErrorType`'s copy constructor does not throw.
   */
  explicit failure_t (ErrorType const &error) : m_error (error) {}

  /**
   * @brief Constructs from an rvalue error.
   * @param[in] error Error to store.
   * @note Noexcept if `ErrorType`'s move constructor does not throw.
   */
  explicit failure_t (ErrorType &&error) : m_error (std::move (error)) {}

  /**
   * @brief Returns the stored error (const lvalue).
   * @return Const reference to the stored `ErrorType`.
   * @note This function does not throw.
   */
  ErrorType const &
  error () const &
  {
    return m_error;
  }

  /**
   * @brief Returns the stored error (rvalue).
   * @return Rvalue reference to the stored `ErrorType`.
   * @note This function does not throw.
   */
  ErrorType &&
  error () &&
  {
    return std::move (m_error);
  }

  /**
   * @brief Implicit conversion into a failed `Expected` (rvalue marker).
   * @tparam Target Success type of the produced `Expected`.
   * @tparam TargetError Error type of the produced `Expected`.
   * @return `Expected<Target, TargetError>` in the error state.
   * @note Removed by SFINAE when `TargetError` cannot be built from
   * `ErrorType &&`; use the const overload for a named marker.
   */
  template <typename Target, typename TargetError,
            typename = typename std::enable_if<
                std::is_constructible<TargetError, ErrorType &&>::value>::type>
  operator Expected<Target, TargetError> () &&
  {
    return Expected<Target, TargetError> (unexpect, std::move (m_error));
  }

  /**
   * @brief Implicit conversion into a failed `Expected` (const lvalue marker).
   * @tparam Target Success type of the produced `Expected`.
   * @tparam TargetError Error type of the produced `Expected`.
   * @return `Expected<Target, TargetError>` in the error state.
   * @note Removed by SFINAE when `TargetError` cannot be built from
   * `ErrorType const &`.
   */
  template <typename Target, typename TargetError,
            typename = typename std::enable_if<std::is_constructible<
                TargetError, ErrorType const &>::value>::type>
  operator Expected<Target, TargetError> () const &
  {
    return Expected<Target, TargetError> (unexpect, m_error);
  }

private:
  ErrorType m_error; ///< Stored error value.
};

/**
 * @brief Creates a marker for a successful call that carries no value.
 * @details Lets a function whose return type is an `Expected` write
 * `return success();`. It converts into `Expected<void, ErrorType>` and into
 * `Expected<Target, ErrorType>` when `Target` is default-constructible, so one
 * spelling covers both shapes.
 * @return `success_t<void>` marker; the target `Expected` is built by the
 * marker's conversion at the `return`.
 * @note Marked `[[nodiscard]]`: the marker is only useful when used.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is a marker; construct or return an Expected with it.")
LUMEX_CONSTEXPR_FUNCTION success_t<void>
success () LUMEX_NOEXCEPT
{
  return success_t<void> ();
}

/**
 * @brief Creates a marker for a successful call that carries a value.
 * @details Lets a function whose return type is an `Expected` write
 * `return success(value);` instead of spelling the whole
 * `Expected<SuccessType, ErrorType>` out.
 * @tparam ValueType Deduced type of the success value; stored decayed.
 * @param[in] value Value the produced `Expected` will hold.
 * @return `success_t<ValueType>` marker holding `value`.
 * @note Marked `[[nodiscard]]`: the marker is only useful when used.
 */
template <typename ValueType>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is a marker; construct or return an Expected with it.")
LUMEX_CONSTEXPR_FUNCTION
    success_t<typename std::decay<ValueType>::type> success (ValueType &&value)
{
  using StoredValueType = typename std::decay<ValueType>::type;
  return success_t<StoredValueType> (std::forward<ValueType> (value));
}

/**
 * @brief Creates a marker for a failed call that carries an error.
 * @details Lets a function whose return type is an `Expected` write
 * `return failure(error);` instead of `Expected<T, E>(unexpect, error)`. The
 * stored error may be the target error type itself, a type convertible to it
 * (for example a string literal for an `Expected<T, std::string>`), or any
 * richer error type the target error can be built from.
 * @tparam ErrorType Deduced type of the stored error; stored decayed.
 * @param[in] error Error the produced `Expected` will hold.
 * @return `failure_t<ErrorType>` marker holding `error`.
 * @note Marked `[[nodiscard]]`: the marker is only useful when used.
 */
template <typename ErrorType>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is a marker; construct or return an Expected with it.")
LUMEX_CONSTEXPR_FUNCTION
    failure_t<typename std::decay<ErrorType>::type> failure (ErrorType &&error)
{
  using StoredErrorType = typename std::decay<ErrorType>::type;
  return failure_t<StoredErrorType> (std::forward<ErrorType> (error));
}

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_EXPECTED_RESULT_SUCCESS_FAILURE_HPP

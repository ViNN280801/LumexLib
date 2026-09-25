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

#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cassert>
#include <utility>

#include "ExpectedTypes.hpp"
#include "lumex/core/expected/error/BadExpectedAccess.hpp"
#include "lumex/core/expected/error/Unexpected.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @brief Class that mimics std::expected from C++23.
 * @link https://en.cppreference.com/w/cpp/utility/expected
 * @details Holds either a value of type SuccessType,
 *          or an error of type ErrorType. Functions can return either a
 * success result, or error information without exceptions for expected
 * failures. The implementation uses a union to save memory and manually
 * manages the lifetime of the stored objects.
 * @tparam SuccessType Success value type.
 * @tparam ErrorType Error type.
 * @note This implementation is not thread-safe by default. Accessing
 * `Expected` from several threads without external synchronization is
 * undefined behavior if `SuccessType` or `ErrorType` are not thread-safe.
 * @warning Using `Expected` with types that have non-trivial
 * constructors/destructors or allocate memory, may be slower than C++23
 * `std::expected`, because object lifetime is managed by hand.
 */

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
using error::BadExpectedAccess;
using error::Unexpected;

template <typename SuccessType, typename ErrorType> class Expected
{
public:
  // ====================== Static assertions ====================== //
  // References are forbidden because:
  // 1. This class (and its specializations) is meant to
  //    own the stored success or error value. If SuccessType or ErrorType were
  //    references, for example `int&`, Expected would not own the object, only
  //    refer to it. That can dangle if the original object referred to by
  //    SuccessType (or ErrorType) is destroyed before Expected.
  // 2. Union restriction: the C++ standard forbids reference members in a
  // `union`,
  //    because references are neither movable nor copyable.
  // 3. C++23 compatibility: std::expected also forbids reference types for
  //    the error parameter for the same reasons. Behavior stays consistent and
  //    predictable.
  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<SuccessType>::value,
                           "Expected<T,E>: T must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<SuccessType>::value,
                           "Expected<T,E>: T must not be a function type");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::decay<SuccessType>::type,
                    in_place_tag>::value,
      "Expected<T,E>: T must not be in_place_tag");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::decay<SuccessType>::type, unexpect_t>::value,
      "Expected<T,E>: T must not be unexpect_t");

  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<ErrorType>::value,
                           "Expected<T,E>: E must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<ErrorType>::value,
                           "Expected<T,E>: E must not be a function type");

  // ====================== Aliases ====================== //
  using value_type = SuccessType;
  using error_type = ErrorType;
  using unexpected_type = Unexpected<ErrorType>;

  /**
   * @brief Alias that rebases Expected to another success value.
   * @tparam U New success value type.
   * @note This alias simplifies type conversions in user code and tests.
   */
  template <typename U> using rebind = Expected<U, ErrorType>;

  // ====================== Constructors ====================== //

  /**
   * @brief Default constructor.
   * @details Creates an `Expected` in the success state holding the value
   * `SuccessType()`, default-initialized. This constructor is available only
   * if `SuccessType` is default-constructible.
   * @note This constructor requires `SuccessType` to be default-constructible.
   *       If `SuccessType` has no default constructor, this constructor is a
   * compile error.
   * @throws May throw if the default constructor of `SuccessType` throws.
   */
  LUMEX_CONSTEXPR_CTOR
  Expected () : m_storage (), m_has_value (true)
  {
    new (std::addressof (m_storage.m_value)) SuccessType ();
  }

  /**
   * @brief Copy constructor.
   * @details Creates a new `Expected` by copying the state and the contained
   * value or error from `other`.
   * @param[in] other `Expected` object to copy.
   * @note Noexcept depends on the constructors of copy of `SuccessType` and
   * `ErrorType`.
   * @throws May throw if the copy constructor of `SuccessType` or `ErrorType`
   * throws.
   */
  LUMEX_CONSTEXPR_CTOR
  Expected (Expected const &other) : m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (std::addressof (m_storage.m_value))
          SuccessType (other.m_storage.m_value);
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (other.m_storage.m_error);
  }

  /**
   * @brief Move constructor.
   * @details Creates a new `Expected` by moving the state and contained value
   * or error from `other`. After the constructor, `other` is valid but
   * unspecified.
   * @param[in] other `Expected` object to move.
   * @note Conditionally `noexcept` if the constructors of move of
   * `SuccessType` and `ErrorType` do not throw.
   * @throws May throw if the constructor of move of `SuccessType` or
   * `ErrorType` throws.
   */
  LUMEX_CONSTEXPR_CTOR
  Expected (Expected &&other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (std::addressof (m_storage.m_value))
          SuccessType (std::move (other.m_storage.m_value));
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (std::move (other.m_storage.m_error));
  }

  /**
   * @brief Constructor from `Unexpected` (copy).
   * @details Creates an `Expected` in the error state by copying the error
   * from `unexp`.
   * @param[in] unexp Const reference to an `Unexpected` that holds an error.
   * @note Noexcept depends on the constructor of copy of `ErrorType`.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  LUMEX_CONSTEXPR_CTOR explicit Expected (Unexpected<ErrorType> const &unexp)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (unexp.error ());
  }

  /**
   * @brief Constructor from `Unexpected` (move).
   * @details Creates an `Expected` in the error state by moving the error
   * value from `unexp`. After the constructor, `unexp` is valid but
   * unspecified.
   * @param[in] unexp Rvalue reference to an `Unexpected` that holds the error.
   * @note Noexcept depends on the constructor of move of `ErrorType`.
   * @throws May throw if the move constructor of `ErrorType` throws.
   */
  LUMEX_CONSTEXPR_CTOR explicit Expected (Unexpected<ErrorType> &&unexp)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::move (unexp).error ());
  }

  /**
   * @brief Constructor from a success value (implicit conversion).
   * @details Creates an `Expected` in the success state holding the value
   * `val`. This constructor implicitly converts `U` to `Expected<SuccessType,
   * ErrorType>` specialization.
   * @tparam U Input type convertible to `SuccessType`.
   * @param[in] val Value stored in the `Expected`.
   * @note SFINAE-constrained to avoid conflicts with other constructors.
   * @throws May throw if constructing `SuccessType` from `U` throws.
   */
  template <
      typename U = SuccessType,
      typename std::enable_if<
          !std::is_same<typename std::decay<U>::type, Expected>::value
              && !std::is_same<typename std::decay<U>::type,
                               in_place_tag>::value
              && !std::is_same<typename std::decay<U>::type, unexpect_t>::value
              && !std::is_same<typename std::decay<U>::type,
                               Unexpected<ErrorType>>::value
              && std::is_convertible<U &&, SuccessType>::value
              && std::is_constructible<SuccessType, U &&>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR
  Expected (U &&val)
      : m_storage (), m_has_value (true)
  {
    new (std::addressof (m_storage.m_value))
        SuccessType (std::forward<U> (val));
  }

  /**
   * @brief In-place constructor for the success value.
   * @details Creates an `Expected` in the success state, constructing
   * `SuccessType` in place using the forwarded arguments.
   * @tparam Args Argument types forwarded to the `SuccessType` constructor.
   * @param[in] unused `in_place_tag` that selects this constructor.
   * @param[in] args Arguments forwarded to the `SuccessType` constructor.
   * @note Avoids extra copies or moves when creating the value.
   * @throws May throw if the `SuccessType` constructor throws.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit Expected (in_place_tag /* unused */,
                                          Args &&...args)
      : m_storage (), m_has_value (true)
  {
    new (std::addressof (m_storage.m_value))
        SuccessType (std::forward<Args> (args)...);
  }

  /**
   * @brief Constructs the error in place via the unexpect tag.
   * @details Puts the object in the error state and constructs E directly in
   * storage.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit Expected (unexpect_t /*unused*/,
                                          Args &&...args)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::forward<Args> (args)...);
  }

  /**
   * @brief Constructor from `Unexpected` (copy) for implicit error
   * construction.
   * @details Creates an `Expected` in the error state by copying the error
   * from `unex`. Alternative way to initialize `Expected` with an error.
   * @tparam Err Error type convertible to `ErrorType`.
   * @param[in] unex Const reference to `Unexpected<Err>` that holds an error.
   * @note Noexcept depends on the constructor of copy of `ErrorType`.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  template <
      typename Err = ErrorType,
      typename = typename std::enable_if<
          std::is_constructible<ErrorType, Err const &>::value
          && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
          && !std::is_same<typename std::decay<Err>::type,
                           unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit Expected (Unexpected<Err> const &unex)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (unex.error ());
  }

  /**
   * @brief Constructor from `Unexpected` (move) for implicit error
   * construction.
   * @details Creates an `Expected` in the error state by moving the error
   * value from `unex`. Alternative way to initialize `Expected` with an error
   * without copying.
   * @tparam Err Error type convertible to `ErrorType`.
   * @param[in] unex Rvalue reference to `Unexpected<Err>` that holds an error.
   * @note Noexcept depends on the constructor of move of `ErrorType`.
   * @throws May throw if the move constructor of `ErrorType` throws.
   */
  template <
      typename Err = ErrorType,
      typename = typename std::enable_if<
          std::is_constructible<ErrorType, Err &&>::value
          && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
          && !std::is_same<typename std::decay<Err>::type,
                           unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit Expected (Unexpected<Err> &&unex)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::move (unex).error ());
  }

  /**
   * @brief Destructor.
   * @details Destroys the stored `SuccessType` value or `ErrorType` error
   * depending on the current state. Destroys the active union member so
   * resources are released.
   * @note Does not throw (`noexcept`) if the destructors of `SuccessType` and
   * `ErrorType` do not throw.
   */
  LUMEX_CONSTEXPR_DTOR ~Expected () LUMEX_NOEXCEPT { destroy_value (); }

  // ====================== Assignment Operators ====================== //

  /**
   * @brief Copy assignment operator.
   * @details Assigns another `Expected` into this object.
   *          Uses copy-and-swap for the strong exception
   * guarantee).
   * @param[in] other `Expected` object to assignment.
   * @return Reference to this `Expected`.
   * @note Conditionally `noexcept` if move/assignment constructors and
   * operators of `SuccessType` and `ErrorType` do not throw.
   * @throws May throw if the copy constructor of `Expected` or `std::swap`
   * throw.
   */
  LUMEX_CONSTEXPR_FUNCTION Expected &
  operator= (Expected const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value
              &&std::is_nothrow_move_assignable<SuccessType>::value
                  &&std::is_nothrow_move_assignable<ErrorType>::value)
  {
    // 1. Make a temporary copy. If this throws,
    // *this stays in its original valid state.
    Expected temp (other);

    // 2. Swap with the temporary. This does not throw.
    swap (temp);
    return *this;

    // `temp` is destroyed on return, releasing the old *this resources
  }

  /**
   * @brief Move assignment operator.
   * @details Assigns another `Expected` into this object by move.
   *          Uses `swap` to exchange resources without extra allocations.
   * @param[in] other `Expected` object to move.
   * @return Reference to this `Expected`.
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_CONSTEXPR_FUNCTION Expected &
  operator= (Expected &&other) LUMEX_NOEXCEPT
  {
    // Just swap resources. No new/delete.
    swap (other);
    return *this;
  }

  // ====================== Observers ======================

  /**
   * @brief Checks whether this `Expected` holds a success value.
   * @return `true` if the object holds a value (success state), `false`
   * otherwise (the error).
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION bool
  has_value () const LUMEX_NOEXCEPT
  {
    return m_has_value;
  }

  /**
   * @brief Implicit conversion to `bool`.
   * @details Lets `Expected` be used in conditional expressions (for example,
   * `if (myExpected)`).
   * @return `true` if the object holds a success value, `false` otherwise.
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION explicit
  operator bool () const LUMEX_NOEXCEPT
  {
    return has_value ();
  }

  /**
   * @brief Returns a mutable lvalue reference to the success value.
   * @warning Calling this while `Expected` is in the error state, this throws
   * `BadExpectedAccess<ErrorType>` specialization.
   * @return Reference to the `SuccessType` value.
   * @throws BadExpectedAccess<ErrorType> if the object does not hold a value.
   * @note Use when you know `Expected` holds a value, or are ready to handle
   * the exception. Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType &
  value () &
  {
    if (!m_has_value)
      throw BadExpectedAccess<ErrorType> (m_storage.m_error);
    return m_storage.m_value;
  }

  /**
   * @brief Returns an rvalue reference to the success value (for moving).
   * @warning Calling this while `Expected` is in the error state, this throws
   * `BadExpectedAccess<ErrorType>` specialization.
   * @return Rvalue reference to the `SuccessType` value.
   * @throws BadExpectedAccess<ErrorType> if the object does not hold a value.
   * @note Intended for moving the value out of `Expected`. After the call,
   * `Expected` remains in a valid but unspecified state. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType &&
  value () &&
  {
    if (!m_has_value)
      throw BadExpectedAccess<ErrorType> (
          std::move (m_storage.m_error)); // Move the error into the exception
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Returns a const lvalue reference to the success value.
   * @warning Calling this while `Expected` is in the error state, this throws
   * `BadExpectedAccess<ErrorType>` specialization.
   * @return Const reference to the `SuccessType` value.
   * @throws BadExpectedAccess<ErrorType> if the object does not hold a value.
   * @note Use to read the value without modifying it. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &
  value () const &
  {
    if (!m_has_value)
      throw BadExpectedAccess<ErrorType> (m_storage.m_error);
    return m_storage.m_value;
  }

  /**
   * @brief Returns a const rvalue reference to the success value.
   * @warning Calling this while `Expected` is in the error state, this throws
   * `BadExpectedAccess<ErrorType>` specialization.
   * @return Const rvalue reference to the `SuccessType` value.
   * @throws BadExpectedAccess<ErrorType> if the object does not hold a value.
   * @note Intended for moving a const value out of `Expected`. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
  value () const &&
  {
    if (!m_has_value)
      throw BadExpectedAccess<ErrorType> (
          std::move (m_storage.m_error)); // Move the error into the exception
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Precondition violation is undefined behavior (checked via assert
   * in debug builds).
   * @return Reference to the `ErrorType` error.
   * @note Use when you know `Expected` holds an error. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType &
  error () &
  {
    // Precondition: !m_has_value.
    // Violation is UB (undefined behavior) for two reasons:
    //
    // 1) Interface-level contract (compatibility with C++23).
    //    In std::expected, error() is an unchecked observer with the
    //    precondition that the object holds an error. If the precondition is
    //    violated, behavior is undefined. Checked access is value(), which
    //    throws bad_expected_access<E>. This library keeps the same model:
    //    error() does not add checks or throws, so it stays noexcept and has
    //    no extra cost.
    //
    // 2) Language rules for union (implementation level).
    //    Expected storage is a union: either the value member is active
    //    (or a dummy marker for void success), or m_error is active. If
    //    m_has_value == true, m_error is not active. Accessing an inactive
    //    union member is UB in C++. Therefore reading m_storage.m_error when
    //    m_has_value == true is itself undefined behavior at the language
    //    level.
    //
    // Why this method does not throw:
    //  - The method must stay usable in noexcept contexts (destructors, swap,
    //  emergency paths).
    //  - Zero release cost: assert is removed under NDEBUG, with no extra
    //  branches or throws.
    //  - Checked, catchable access is already provided by value() (and a prior
    //    has_value() check).
    //
    // Practical consequence:
    //  - In debug, assert fires immediately and shows the contract violation.
    //  - In release the caller is responsible: before calling error() it must
    //    guarantee !has_value() (for example via if (!has_value()) ...).
    LUMEX_ASSERT (
        !m_has_value
        && "Calling error() while value is present is undefined behavior.");
    return m_storage.m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Precondition violation is undefined behavior (checked via assert
   * in debug builds).
   * @return Const reference to the `ErrorType` error.
   * @note Use this function to read the error without modifying it. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &
  error () const &
  {
    LUMEX_ASSERT (
        !m_has_value
        && "Calling error() while value is present is undefined behavior.");
    return m_storage.m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Precondition violation is undefined behavior (checked via assert
   * in debug builds).
   * @return Rvalue reference to the `ErrorType` error.
   * @note This function is intended to move the error out of `Expected`. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType &&
  error () &&
  {
    LUMEX_ASSERT (
        !m_has_value
        && "Calling error() while value is present is undefined behavior.");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Precondition violation is undefined behavior (checked via assert
   * in debug builds).
   * @return Const rvalue reference to the `ErrorType` error.
   * @note This function is intended to move a const error out of `Expected`.
   * Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &&
  error () const &&
  {
    LUMEX_ASSERT (
        !m_has_value
        && "Calling error() while value is present is undefined behavior.");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns the stored value if present, otherwise the default value.
   * @tparam U Default-value type; must be convertible to `SuccessType`.
   * @param[in] default_value Value returned if the object does not hold a
   * success value.
   * @return The stored value or `default_value`.
   * @note This function does not throw.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U = typename std::remove_cv<SuccessType>::type>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained value or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType value_or (U &&default_value) const &
  {
    return m_has_value
               ? m_storage.m_value
               : static_cast<SuccessType> (std::forward<U> (default_value));
  }

  /**
   * @brief Returns the stored value by move if present, otherwise the default
   * value.
   * @tparam U Default-value type; must be convertible to `SuccessType`.
   * @param[in] default_value Value returned if the object does not hold a
   * success value.
   * @return The stored value moved out of `Expected`, or `default_value`.
   * @note This function does not throw. If `Expected` holds a value, it is
   * moved. After that, `Expected` remains in a valid but unspecified state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U = typename std::remove_cv<SuccessType>::type>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained value or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType value_or (U &&default_value) &&
  {
    return m_has_value
               ? std::move (m_storage.m_value)
               : static_cast<SuccessType> (std::forward<U> (default_value));
  }

  /**
   * @brief Returns the error if present, otherwise the given default error.
   * @details For an rvalue object: if an error is present it is moved out;
   * otherwise returns a copy/move constructed from `default_error`.
   *
   * @tparam G Default-argument type (defaults to ErrorType).
   * @param default_error Default error value.
   * @return ErrorType
   *
   * @note The rvalue overload avoids an extra copy
   *       of the real error by moving it.
   * @par Exception guarantees
   *      May throw if copy/move of `ErrorType` or `G -> ErrorType`
   *      can throw.
   */
  template <typename G = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the error or a "
                             "default-constructed substitute; should be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or (G &&default_error) &&
  {
    if (!m_has_value)
      return std::move (m_storage.m_error);
    return static_cast<ErrorType> (std::forward<G> (default_error));
  }

  /**
   * @brief Returns the stored error if present, otherwise the default error.
   * @tparam U Default-error type; must be convertible to `ErrorType`.
   * @param[in] default_error Error returned if the object holds a success
   * value.
   * @return The stored error or `default_error`.
   * @note This function does not throw.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained error or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or (U &&default_error) const &
  {
    return !m_has_value
               ? m_storage.m_error
               : static_cast<ErrorType> (std::forward<U> (default_error));
  }

  /**
   * @brief Dereference operator (lvalue).
   * @details Returns a mutable lvalue reference to the stored success value.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType &
      operator* ()
      & LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value && "Dereferencing Expected without a value.");
    return m_storage.m_value;
  }

  /**
   * @brief Dereference operator (rvalue).
   * @details Returns an rvalue reference to the stored success value for
   * moving.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Rvalue reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check. After the call `Expected` remains in a valid but unspecified state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType &&
      operator* ()
      && LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value && "Dereferencing Expected without a value.");
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Const dereference operator (lvalue).
   * @details Returns a const lvalue reference to the stored success value.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Const reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &
  operator* () const &LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value && "Dereferencing Expected without a value.");
    return m_storage.m_value;
  }

  /**
   * @brief Const dereference operator (rvalue).
   * @details Returns a const rvalue reference to the stored success value.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Const rvalue reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check. After the call `Expected` remains in a valid but unspecified state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
  operator* () const &&LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value && "Dereferencing Expected without a value.");
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Member-access operator (lvalue).
   * @details Returns a pointer to the stored success value.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Pointer to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType *
  operator->() LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "Accessing Expected members without a value.");
    return std::addressof (m_storage.m_value);
  }

  /**
   * @brief Const member-access operator (lvalue).
   * @details Returns a const pointer to the stored success value.
   * @warning Assumes `Expected` holds a value. If it does not, behavior
   * is undefined (`assert` fires).
   * @return Const pointer to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType const *
  operator->() const LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "Accessing Expected members without a value.");
    return std::addressof (m_storage.m_value);
  }

  // ====================== Modifiers ======================

  /**
   * @brief Constructs a `SuccessType` value in place, destroying the current
   * contents.
   * @details This function first destroys the current stored value (or error),
   * then constructs a new `SuccessType` success value in place from the
   * forwarded arguments.
   * @tparam Args Argument types for the `SuccessType` constructor.
   * @param[in] args Arguments forwarded to the `SuccessType` constructor.
   * @return Reference to the newly constructed `SuccessType` value.
   * @note May throw if the `SuccessType` constructor throws. On exception
   *       the `Expected` object may be left in an invalid state.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_FUNCTION SuccessType &
  emplace (Args &&...args)
  {
    *this = Expected (in_place, std::forward<Args> (args)...);
    return m_storage.m_value;
  }

  /**
   * @brief Constructs an `ErrorType` value in place, destroying the current
   * contents.
   * @details This function first destroys the current stored value (or error),
   * then constructs a new `ErrorType` error in place from the forwarded
   * arguments.
   * @tparam Args Argument types for the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @return Reference to the newly constructed `ErrorType` error.
   * @note May throw if the `ErrorType` constructor throws. On exception
   *       the `Expected` object may be left in an invalid state.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_FUNCTION ErrorType &
  emplace_error (Args &&...args)
  {
    destroy_value (); // Destroy the current active member
    new (std::addressof (m_storage.m_error)) ErrorType (
        std::forward<Args> (args)...); // Construct the error in place
    m_has_value = false;               // Set the error state
    return m_storage.m_error;
  }

  /**
   * @brief Exchanges contents with another `Expected` object.
   * @details Swaps the `m_has_value` flag and, as needed, the contents (value
   * or error) with another `Expected`. If both objects hold values (or both
   * hold errors), `std::swap` is used. If one holds a value and the other an
   * error, contents are moved so both objects change state.
   * @param[in,out] other The other `Expected` object to swap with.
   * @note The noexcept guarantee depends on
   * `std::is_nothrow_move_constructible` and, from C++17,
   * `std::is_nothrow_swappable` for `SuccessType` and `ErrorType`.
   * @throws May throw if move constructors or `std::swap` of
   *         `SuccessType` or `ErrorType` throw.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  swap (Expected &other)
#if __cplusplus >= 201703L
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible_v<SuccessType>
                             &&std::is_nothrow_move_constructible_v<ErrorType>
                                 &&std::is_nothrow_swappable_v<SuccessType>
                                     &&std::is_nothrow_swappable_v<ErrorType>)
#else
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_move_constructible<SuccessType>::value
              &&std::is_nothrow_move_constructible<ErrorType>::value)
#endif
  {
    if (this == &other)
      return;

    if (m_has_value && other.m_has_value)
      {
        std::swap (m_storage.m_value, other.m_storage.m_value);
      }
    else if (!m_has_value && !other.m_has_value)
      { // Both hold an error
        std::swap (m_storage.m_error, other.m_storage.m_error);
      }
    else
      { // One holds void success, the other an error. Move is required.
        if (m_has_value)
          {
            ErrorType temp_error (std::move (other.m_storage.m_error));
            other.m_storage.m_error.~ErrorType ();
            new (std::addressof (other.m_storage.m_value))
                SuccessType (std::move (m_storage.m_value));
            m_storage.m_value.~SuccessType ();
            new (std::addressof (m_storage.m_error))
                ErrorType (std::move (temp_error));
          }
        else
          {
            SuccessType temp_value (std::move (other.m_storage.m_value));
            other.m_storage.m_value.~SuccessType ();
            new (std::addressof (other.m_storage.m_error))
                ErrorType (std::move (m_storage.m_error));
            m_storage.m_error.~ErrorType ();
            new (std::addressof (m_storage.m_value))
                SuccessType (std::move (temp_value));
          }
        std::swap (m_has_value, other.m_has_value);
      }
  }

  // ====================== Monadic Operations ======================

  /**
   * @brief Applies 'func' to the contained value if present.
   * @details If Expected holds a value, 'func' is called with that value,
   *          and the result of 'func' is returned. 'func' must return
   * Expected<U, ErrorType>. If Expected holds an error, 'func' is not called,
   *          and the current error is returned.
   * @tparam FunctionType Function type that takes SuccessType and returns
   * Expected<U, ErrorType>.
   * @param func Function to apply.
   * @return Expected<U, ErrorType> holding the result of 'func' or the current
   * error.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, SuccessType &val) {
      {
        std::forward<FunctionType> (f) (val)
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (
      FunctionType func) & -> std::invoke_result_t<FunctionType, SuccessType &>
  {
    if (m_has_value)
      return func (m_storage.m_value);
    return std::invoke_result_t<FunctionType, SuccessType &> (
        Unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ReturnType
            = typename std::result_of<FunctionType (SuccessType &)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ReturnType>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func) & -> ReturnType
  {
    if (m_has_value)
      return func (m_storage.m_value);
    return ReturnType (Unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (const lvalue) if present,
   * and returns `Expected`.
   * @details If `Expected` holds a value, `func` is called with a const lvalue
   * reference to it, and the result of `func` is returned. `func` must return
   * `Expected<U, ErrorType>` specialization. If `Expected` holds an error,
   * `func` is not called, and a new `Expected` holding the current error is
   * returned.
   * @tparam FunctionType Function type that takes `const SuccessType &` and
   * returns `Expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on const lvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const SuccessType &val) {
      {
        std::forward<FunctionType> (f) (val)
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func)
      const & -> std::invoke_result_t<FunctionType, const SuccessType &>
  {
    if (m_has_value)
      return func (m_storage.m_value);
    return std::invoke_result_t<FunctionType, SuccessType const &> (
        Unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc
      = typename std::result_of<FunctionType (SuccessType const &)>::type,
      typename
      = typename std::enable_if<lumex::core::utility::traits::value::
                                    is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func) const & -> ResultOfFunc
  {
    if (m_has_value)
      return func (m_storage.m_value);
    return ResultOfFunc (Unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (rvalue) if present, and
   * returns `Expected`.
   * @details If `Expected` holds a value, `func` is called with an rvalue
   * reference to it (for moving), and the result of `func` is returned. `func`
   * must return `Expected<U, ErrorType>` specialization. If `Expected` holds
   * an error, `func` is not called, and a new `Expected` holding the current
   * error moved out of `Expected` is returned.
   * @tparam FunctionType Function type that takes `SuccessType &&` and returns
   * `Expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on rvalue `Expected` objects,
   * providing move semantics.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, SuccessType &&val) {
      {
        std::forward<FunctionType> (f) (std::move (val))
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func)
      && -> std::invoke_result_t<FunctionType, SuccessType &&>
  {
    if (m_has_value)
      return func (std::move (m_storage.m_value));
    return std::invoke_result_t<FunctionType, SuccessType &&> (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (SuccessType &&)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func) && -> ResultOfFunc
  {
    if (m_has_value)
      return func (std::move (m_storage.m_value));
    return ResultOfFunc (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (const rvalue) if present,
   * and returns `Expected`.
   * @details If `Expected` holds a value, `func` is called with a const rvalue
   * reference to it, and the result of `func` is returned. `func` must return
   * `Expected<U, ErrorType>` specialization. If `Expected` holds an error,
   * `func` is not called, and a new `Expected` holding the current error moved
   * out of `Expected` is returned.
   * @tparam FunctionType Function type that takes `const SuccessType &&` and
   * returns `Expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on const rvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const SuccessType &&val) {
      {
        std::forward<FunctionType> (f) (std::move (val))
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func)
      const && -> std::invoke_result_t<FunctionType, const SuccessType &&>
  {
    if (m_has_value)
      return func (std::move (m_storage.m_value));
    return std::invoke_result_t<FunctionType, SuccessType const &&> (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc
      = typename std::result_of<FunctionType (SuccessType const &&)>::type,
      typename
      = typename std::enable_if<lumex::core::utility::traits::value::
                                    is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType func) const && -> ResultOfFunc
  {
    if (m_has_value)
      return func (std::move (m_storage.m_value));
    return ResultOfFunc (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies 'func' to the contained value if present and transforms it.
   * @details If Expected holds a value, 'func' is called with that value,
   *          and a new Expected holding the result of 'func' is returned.
   *          If Expected holds an error, 'func' is not called,
   *          and an Expected holding the current error is returned.
   * @tparam FunctionType Function type that takes SuccessType and returns U.
   * @param func Function to apply.
   * @return Expected<U, ErrorType> holding the transformed value or the
   * current error.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, SuccessType &val) {
      {
        std::forward<FunctionType> (f) (val)
      } -> std::convertible_to<typename std::remove_cv_t<SuccessType>>;
    } && (!std::is_void_v<std::invoke_result_t<FunctionType, SuccessType &>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, SuccessType &>>)
  LUMEX_CONSTEXPR_FUNCTION auto transform (FunctionType func) & -> Expected<
      std::invoke_result_t<FunctionType, SuccessType &>, ErrorType>
  {
    if (m_has_value)
      return Expected<std::invoke_result_t<FunctionType, SuccessType &>,
                      ErrorType> (in_place, func (m_storage.m_value));
    return Expected<std::invoke_result_t<FunctionType, SuccessType &>,
                    ErrorType> (Unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (SuccessType &)>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType func) & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func (m_storage.m_value));
    return ReturnType (Unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (const lvalue) if present,
   * and transforms it.
   * @details If `Expected` holds a value, `func` is called with a const lvalue
   * reference to it, and a new `Expected` holding the result of `func` is
   * returned. If `Expected` holds an error, `func` is not called and an
   * `Expected` holding the current error is returned.
   * @tparam FunctionType Function type that takes `const SuccessType &` and
   * returns `U`.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the transformed value or the
   * current error.
   * @note This overload lets `transform` be used on const lvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const SuccessType &val) {
      {
        std::forward<FunctionType> (f) (val)
      } -> std::convertible_to<typename std::remove_cv_t<SuccessType>>;
    }
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const SuccessType &>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, const SuccessType &>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform (FunctionType func) const & -> Expected<
          std::invoke_result_t<FunctionType, const SuccessType &>, ErrorType>
  {
    if (m_has_value)
      return Expected<std::invoke_result_t<FunctionType, SuccessType const &>,
                      ErrorType> (in_place, func (m_storage.m_value));
    return Expected<std::invoke_result_t<FunctionType, SuccessType const &>,
                    ErrorType> (Unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType,
      // Use std::result_of to obtain the return type of func
      typename ResultOfFunc
      = typename std::result_of<FunctionType (SuccessType const &)>::type,
      typename ReturnType = Expected<ResultOfFunc, ErrorType>,
      // SFINAE: this overload exists only if func can be called
      // with SuccessType const & and the return type is neither void nor
      // Expected.
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType func) const & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func (m_storage.m_value));
    // The error is not handled; it is moved into a new Expected in the
    // unexpect state
    return ReturnType (Unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (rvalue) if present and
   * transforms it.
   * @details If `Expected` holds a value, `func` is called with an rvalue
   * reference to it (for moving), and a new `Expected` holding the result of
   * `func` is returned. If `Expected` holds an error, `func` is not called and
   * an `Expected` holding the current error moved from `Expected`.
   * @tparam FunctionType Function type that takes `SuccessType &&` and returns
   * `U`.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the transformed value or the
   * current error.
   * @note This overload lets `transform` be used on rvalue `Expected` objects,
   * providing move semantics.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, SuccessType &&val) {
      {
        std::forward<FunctionType> (f) (std::move (val))
      } -> std::convertible_to<typename std::remove_cv_t<SuccessType>>;
    } && (!std::is_void_v<std::invoke_result_t<FunctionType, SuccessType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, SuccessType &&>>)
  LUMEX_CONSTEXPR_FUNCTION auto transform (FunctionType func) && -> Expected<
      std::invoke_result_t<FunctionType, SuccessType &&>, ErrorType>
  {
    if (m_has_value)
      return Expected<std::invoke_result_t<FunctionType, SuccessType &&>,
                      ErrorType> (in_place,
                                  func (std::move (m_storage.m_value)));
    return Expected<std::invoke_result_t<FunctionType, SuccessType &&>,
                    ErrorType> (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (SuccessType &&)>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType func) && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func (std::move (m_storage.m_value)));
    return ReturnType (Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` to the contained value (const rvalue) if present,
   * and transforms it.
   * @details If `Expected` holds a value, `func` is called with a const rvalue
   * reference to it, and a new `Expected` holding the result of `func` is
   * returned. If `Expected` holds an error, `func` is not called and an
   * `Expected` holding the current error moved from `Expected`.
   * @tparam FunctionType Function type that takes `const SuccessType &&` and
   * returns `U`.
   * @param[in] func Function to apply.
   * @return `Expected<U, ErrorType>` holding the transformed value or the
   * current error.
   * @note This overload lets `transform` be used on const rvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * of `Expected` from an error throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const SuccessType &&val) {
      {
        std::forward<FunctionType> (f) (std::move (val))
      } -> std::convertible_to<typename std::remove_cv_t<SuccessType>>;
    }
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const SuccessType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, const SuccessType &&>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform (FunctionType func) const && -> Expected<
          std::invoke_result_t<FunctionType, const SuccessType &&>, ErrorType>
  {
    if (m_has_value)
      return Expected<std::invoke_result_t<FunctionType, SuccessType const &&>,
                      ErrorType> (in_place,
                                  func (std::move (m_storage.m_value)));
    return Expected<std::invoke_result_t<FunctionType, SuccessType const &&>,
                    ErrorType> (
        Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc
      = typename std::result_of<FunctionType (SuccessType const &&)>::type,
      typename ReturnType = Expected<ResultOfFunc, ErrorType>,
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType func) const && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func (std::move (m_storage.m_value)));
    return ReturnType (Unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies 'func' to the contained error if present.
   * @details If Expected holds an error, 'func' is called with that error,
   *          and the result of 'func' is returned. 'func' must return
   * Expected<SuccessType, F_E>. If Expected holds a value, 'func' is not
   * called, and the current value is returned.
   * @tparam FunctionType Function type that takes ErrorType and returns
   * Expected<SuccessType, F_E>.
   * @param func Function to apply.
   * @return Expected<SuccessType, F_E> holding the current value or the result
   * of 'func'.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, ErrorType &err) {
      {
        std::forward<FunctionType> (f) (err)
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (
      FunctionType func) & -> std::invoke_result_t<FunctionType, ErrorType &>
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place, m_storage.m_value);
    return func (m_storage.m_error);
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func) & -> ResultOfFunc
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place, m_storage.m_value);
    return func (m_storage.m_error);
  }
#endif

  /**
   * @brief Applies `func` to the contained error (const lvalue) if present,
   * and returns `Expected`.
   * @details If `Expected` holds an error, `func` is called with a const
   * lvalue reference to it, and the result of `func` is returned. `func` must
   * return `Expected<SuccessType, F_E>`. If `Expected` holds a value, `func`
   * is not called, and a new `Expected` holding the current value is returned.
   * @tparam FunctionType Function type that takes `const ErrorType &` and
   * returns `Expected<SuccessType, F_E>`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * result of `func`.
   * @note This overload lets `or_else` be used on const lvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const ErrorType &err) {
      {
        std::forward<FunctionType> (f) (err)
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func)
      const & -> std::invoke_result_t<FunctionType, const ErrorType &>
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place, m_storage.m_value);
    return func (m_storage.m_error);
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func) const & -> ResultOfFunc
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place, m_storage.m_value);
    return func (m_storage.m_error);
  }
#endif

  /**
   * @brief Applies `func` to the contained error (rvalue) if present, and
   * returns `Expected`.
   * @details If `Expected` holds an error, `func` is called with an rvalue
   * reference to it (for moving), and the result of `func` is returned. `func`
   * must return `Expected<SuccessType, F_E>`. If `Expected` holds a value,
   * `func` is not called, and a new `Expected` holding the current value is
   * returned.
   * @tparam FunctionType Function type that takes `ErrorType &&` and returns
   * `Expected<SuccessType, F_E>`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * result of `func`.
   * @note This overload lets `or_else` be used on rvalue `Expected` objects,
   * providing move semantics.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, ErrorType &&err) {
      {
        std::forward<FunctionType> (f) (std::move (err))
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (
      FunctionType func) && -> std::invoke_result_t<FunctionType, ErrorType &&>
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place,
                                               std::move (m_storage.m_value));
    return func (std::move (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &&)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func) && -> ResultOfFunc
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place,
                                               std::move (m_storage.m_value));
    return func (std::move (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` to the contained error (const rvalue) if present,
   * and returns `Expected`.
   * @details If `Expected` holds an error, `func` is called with a const
   * rvalue reference to it, and the result of `func` is returned. `func` must
   * return `Expected<SuccessType, F_E>`. If `Expected` holds a value, `func`
   * is not called, and a new `Expected` holding the current value is returned.
   * @tparam FunctionType Function type that takes `const ErrorType &&` and
   * returns `Expected<SuccessType, F_E>`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * result of `func`.
   * @note This overload lets `or_else` be used on const rvalue `Expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const ErrorType &&err) {
      {
        std::forward<FunctionType> (f) (std::move (err))
      } -> lumex::core::utility::traits::value::is_expected_concept;
    }
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func)
      const && -> std::invoke_result_t<FunctionType, const ErrorType &&>
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place,
                                               std::move (m_storage.m_value));
    return func (std::move (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &&)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType func) const && -> ResultOfFunc
  {
    if (m_has_value)
      return Expected<SuccessType, ErrorType> (in_place,
                                               std::move (m_storage.m_value));
    return func (std::move (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies 'func' to the contained error if present and transforms it.
   * @details If Expected holds an error, 'func' is called with that error,
   *          and a new Expected holding the transformed error is returned.
   *          If Expected holds a value, 'func' is not called,
   *          and an Expected holding the current error is returned.
   * @tparam FunctionType Function type that takes ErrorType and returns F_E.
   * @param func Function to apply.
   * @return Expected<SuccessType, F_E> holding the current value or the
   * transformed error.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, ErrorType &err) {
      {
        std::forward<FunctionType> (f) (err)
      } -> std::convertible_to<typename std::remove_cv_t<ErrorType>>;
    } && (!std::is_void_v<std::invoke_result_t<FunctionType, ErrorType &>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, ErrorType &>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform_error (FunctionType func) & -> Expected<
          SuccessType, std::invoke_result_t<FunctionType, ErrorType &>>
  {
    if (m_has_value)
      return Expected<SuccessType,
                      std::invoke_result_t<FunctionType, ErrorType &>> (
          in_place, m_storage.m_value);
    return Expected<SuccessType,
                    std::invoke_result_t<FunctionType, ErrorType &>> (
        Unexpected<std::invoke_result_t<FunctionType, ErrorType &>> (
            func (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &)>::type,
            typename ReturnType = Expected<SuccessType, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType func) & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, m_storage.m_value);
    return ReturnType (Unexpected<ResultOfFunc> (func (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` to the contained error (const lvalue) if present and
   * transforms it.
   * @details If `Expected` holds an error, `func` is called with a const
   * lvalue reference to it, and a new `Expected` holding the transformed error
   * is returned. If `Expected` holds a value, `func` is not called, and an
   * `Expected` holding the current value is returned.
   * @tparam FunctionType Function type that takes `const ErrorType &` and
   * returns `F_E`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * transformed error.
   * @note This overload lets `transform_error` be used on const lvalue
   * `Expected` objects.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const ErrorType &err) {
      {
        std::forward<FunctionType> (f) (err)
      } -> std::convertible_to<typename std::remove_cv_t<ErrorType>>;
    }
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const ErrorType &>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, const ErrorType &>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform_error (FunctionType func) const & -> Expected<
          SuccessType, std::invoke_result_t<FunctionType, const ErrorType &>>
  {
    if (m_has_value)
      return Expected<SuccessType,
                      std::invoke_result_t<FunctionType, ErrorType const &>> (
          in_place, m_storage.m_value);
    return Expected<SuccessType,
                    std::invoke_result_t<FunctionType, ErrorType const &>> (
        Unexpected<std::invoke_result_t<FunctionType, ErrorType const &>> (
            func (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &)>::type,
            typename ReturnType = Expected<SuccessType, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType func) const & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, m_storage.m_value);
    return ReturnType (Unexpected<ResultOfFunc> (func (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` to the contained error (rvalue) if present and
   * transforms it.
   * @details If `Expected` holds an error, `func` is called with an rvalue
   * reference to it (for moving), and a new `Expected` holding the transformed
   * error is returned. If `Expected` holds a value, `func` is not called, and
   * an `Expected` holding the current value is returned.
   * @tparam FunctionType Function type that takes `ErrorType &&` and returns
   * `F_E`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * result of `func`.
   * @note This overload lets `transform_error` be used on rvalue `Expected`
   * objects, providing move semantics.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, ErrorType &&err) {
      {
        std::forward<FunctionType> (f) (std::move (err))
      } -> std::convertible_to<typename std::remove_cv_t<ErrorType>>;
    } && (!std::is_void_v<std::invoke_result_t<FunctionType, ErrorType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, ErrorType &&>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform_error (FunctionType func) && -> Expected<
          SuccessType, std::invoke_result_t<FunctionType, ErrorType &&>>
  {
    if (m_has_value)
      return Expected<SuccessType,
                      std::invoke_result_t<FunctionType, ErrorType &&>> (
          in_place, std::move (m_storage.m_value));
    return Expected<SuccessType,
                    std::invoke_result_t<FunctionType, ErrorType &&>> (
        Unexpected<std::invoke_result_t<FunctionType, ErrorType &&>> (
            func (std::move (m_storage.m_error))));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &&)>::type,
            typename ReturnType = Expected<SuccessType, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType func) && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, std::move (m_storage.m_value));
    return ReturnType (
        Unexpected<ResultOfFunc> (func (std::move (m_storage.m_error))));
  }
#endif

  /**
   * @brief Applies `func` to the contained error (const rvalue) if present and
   * transforms it.
   * @details If `Expected` holds an error, `func` is called with a const
   * rvalue reference to it, and a new `Expected` holding the transformed error
   * is returned. If `Expected` holds a value, `func` is not called, and an
   * `Expected` holding the current value is returned.
   * @tparam FunctionType Function type that takes `const ErrorType &&` and
   * returns `F_E`.
   * @param[in] func Function to apply.
   * @return `Expected<SuccessType, F_E>` holding the current value or the
   * result of `func`.
   * @note This overload lets `transform_error` be used on const rvalue
   * `Expected` objects.
   * @throws May throw if `func` throws or the constructor
   * `Expected` from a value throws.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires requires (FunctionType &&f, const ErrorType &&err) {
      {
        std::forward<FunctionType> (f) (std::move (err))
      } -> std::convertible_to<typename std::remove_cv_t<ErrorType>>;
    }
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const ErrorType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_v<
                 std::invoke_result_t<FunctionType, const ErrorType &&>>)
  LUMEX_CONSTEXPR_FUNCTION
      auto transform_error (FunctionType func) const && -> Expected<
          SuccessType, std::invoke_result_t<FunctionType, const ErrorType &&>>
  {
    if (m_has_value)
      return Expected<SuccessType,
                      std::invoke_result_t<FunctionType, ErrorType const &&>> (
          in_place, std::move (m_storage.m_value));
    return Expected<SuccessType,
                    std::invoke_result_t<FunctionType, ErrorType const &&>> (
        Unexpected<std::invoke_result_t<FunctionType, ErrorType const &&>> (
            func (std::move (m_storage.m_error))));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &&)>::type,
            typename ReturnType = Expected<SuccessType, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType func) const && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, std::move (m_storage.m_value));
    return ReturnType (
        Unexpected<ResultOfFunc> (func (std::move (m_storage.m_error))));
  }
#endif

private:
  /**
   * @brief Union that stores either a success value or an error value.
   * @details Used to save memory, because an `Expected` object at any time
   *          holds only one of the two: either `SuccessType` or `ErrorType`.
   *          Union-member lifetimes are managed manually.
   * @note Copy/move constructors and assignment operators are deleted,
   *       because member lifetimes are managed by `Expected`.
   */
  union Storage
  {
    /**
     * @brief Stored success value.
     * @details Active when `Expected` is in the success state (`m_has_value ==
     * true`).
     */
    SuccessType m_value;

    /**
     * @brief Stored error value.
     * @details Active when `Expected` is in the error state (`m_has_value ==
     * false`).
     */
    ErrorType m_error;

    /**
     * @brief Default constructor for `Storage`.
     * @details Does not initialize members; their lifetime
     *          is managed manually from outside.
     * @note Guaranteed not to throw (`noexcept`).
     */
    Storage () LUMEX_NOEXCEPT {}
    /**
     * @brief Default destructor for `Storage`.
     * @details Does not destroy members; their lifetime
     *          is managed manually from outside.
     * @note Guaranteed not to throw (`noexcept`).
     */
    ~Storage () LUMEX_NOEXCEPT {}

    /**
     * @brief Deleted copy constructor.
     * @details Copying `Storage` is forbidden to avoid double management of
     *          the active member lifetime.
     */
    Storage (Storage const &) = delete;

    /**
     * @brief Deleted copy assignment.
     * @details Assignment is forbidden for the same reasons as the copy
     * constructor.
     */
    Storage &operator= (Storage const &) = delete;

    /**
     * @brief Deleted move constructor.
     * @details Move is forbidden because transferring the active member is
     * done explicitly by the outer class.
     */
    Storage (Storage &&) = delete;

    /**
     * @brief Deleted move assignment.
     * @details Forbidden to prevent uncontrolled switching of the active
     * member.
     */
    Storage &operator= (Storage &&) = delete;
  };

  /**
   * @brief `Storage` union that holds either a `SuccessType` value or an
   * `ErrorType` error.
   * @details This member is central to `Expected` because it holds the actual
   * data. Access `m_value` or `m_error` only after checking `m_has_value`.
   */
  Storage m_storage;
  /**
   * @brief Flag: `true` if `Expected` holds a success value, `false` if it
   * holds an error.
   * @details This member selects which `m_storage` union member is active and
   * therefore which object (`SuccessType` or `ErrorType`) must be constructed
   * or destroyed.
   */
  bool m_has_value;

  /**
   * @brief Destroys the active member of the `m_storage` union.
   * @details Depending on `m_has_value`, the destructor of either `m_value`
   * (SuccessType) or `m_error` (ErrorType) is called. This keeps resource
   * management correct when the `Expected` destructor runs or the object
   * changes state (for example, `emplace`).
   * @note Does not throw (`noexcept`) if the destructors of `SuccessType` and
   * `ErrorType` do not throw.
   */
  void
  destroy_value () LUMEX_NOEXCEPT
  {
    if (m_has_value)
      m_storage.m_value.~SuccessType ();
    else
      m_storage.m_error.~ErrorType ();
  }
};

// ====================== Non-member functions ======================

/**
 * @brief Compares two `Expected<SuccessType, ErrorType>` objects for equality.
 * @details Two objects are equal if they are in the same state
 *          (both success or both error) and:
 *          - on success their values compare equal (`*lhs == *rhs`);
 *          - on error their errors compare equal (`lhs.error() ==
 * rhs.error()`).
 *
 * @tparam SuccessType Success value type.
 * @tparam ErrorType   Error type.
 * @param[in] lhs Left-hand operand.
 * @param[in] rhs Right-hand operand.
 * @return `true` if the objects match in state and contents, otherwise
 * `false`.
 *
 * @note Requires `operator==` for both `SuccessType` and `ErrorType`.
 * @par Thread safety
 *      Not thread-safe if the same instances are accessed concurrently without
 * synchronization.
 * @par Exception guarantees
 *      May throw if `operator==` of `SuccessType` or `ErrorType` throws.
 */
template <typename SuccessType, typename ErrorType>
LUMEX_CONSTEXPR_FUNCTION bool
operator== (Expected<SuccessType, ErrorType> const &lhs,
            Expected<SuccessType, ErrorType> const &rhs)
{
  if (lhs.has_value () != rhs.has_value ())
    return false;
  if (lhs.has_value ())
    return *lhs == *rhs;
  return lhs.error () == rhs.error ();
}

/**
 * @brief Compares two `Expected<void, ErrorType>` objects for equality.
 * @details Two objects are equal if:
 *          - both are in the success state (then they are always equal);
 *          - or both hold errors that compare equal (`lhs.error() ==
 * rhs.error()`).
 *
 * @tparam ErrorType Error type.
 * @param[in] lhs Left-hand operand.
 * @param[in] rhs Right-hand operand.
 * @return `true` if the objects match in state and (on error) error value,
 * otherwise `false`.
 *
 * @note Requires a correct `operator==` for `ErrorType`.
 * @par Thread safety
 *      Not thread-safe under concurrent access to the same instances.
 * @par Exception guarantees
 *      May throw if `operator==` of `ErrorType` throws.
 */
template <typename ErrorType>
LUMEX_CONSTEXPR_FUNCTION bool
operator== (Expected<void, ErrorType> const &lhs,
            Expected<void, ErrorType> const &rhs)
{
  if (lhs.has_value () != rhs.has_value ())
    return false;
  if (lhs.has_value ())
    return true; // Both are void success
  return lhs.error () == rhs.error ();
}

/**
 * @brief Exchanges the contents of two `Expected<SuccessType, ErrorType>`
 * objects.
 * @details Calls `lhs.swap(rhs)`, delegating to the class implementation.
 *
 * @tparam SuccessType Success value type.
 * @tparam ErrorType   Error type.
 * @param[in,out] lhs Left-hand operand of the swap.
 * @param[in,out] rhs Right-hand operand of the swap.
 *
 * @note Marked `noexcept` via `LUMEX_NOEXCEPT`; the actual guarantee
 *       depends on the `swap` exception guarantees of
 * `SuccessType`/`ErrorType` in the class method.
 * @par Thread safety
 *      Not thread-safe for the same objects without external synchronization.
 * @par Performance
 *      Swap is typically O(1) and does not allocate.
 */
template <typename SuccessType, typename ErrorType>
LUMEX_CONSTEXPR_FUNCTION void
swap (Expected<SuccessType, ErrorType> &lhs,
      Expected<SuccessType, ErrorType> &rhs) LUMEX_NOEXCEPT
{
  lhs.swap (rhs);
}

/**
 * @brief Exchanges the contents of two `Expected<void, ErrorType>` objects.
 * @details Delegates to the matching class specialization `swap`.
 *
 * @tparam ErrorType Error type.
 * @param[in,out] lhs Left-hand operand of the swap.
 * @param[in,out] rhs Right-hand operand of the swap.
 *
 * @note The actual `noexcept` guarantee comes from `Expected<void,
 * ErrorType>::swap`.
 * @par Thread safety
 *      Not thread-safe without external synchronization on the same instances.
 */
template <typename ErrorType>
LUMEX_CONSTEXPR_FUNCTION void
swap (Expected<void, ErrorType> &lhs,
      Expected<void, ErrorType> &rhs) LUMEX_NOEXCEPT
{
  lhs.swap (rhs);
}

// ====================== Helper functions make_expected/make_unexpected
// ======================

/**
 * @brief Creates a successful `Expected<T, ErrorType>` from a value.
 * @details Constructs the success state in place, avoiding extra copies/moves.
 *
 * @tparam ErrorType Error type.
 * @tparam U_val     Input-value type; after `std::decay`, `T =
 * std::decay_t<U_val>`.
 * @param[in] val    Value used to initialize the success result.
 * @return `Expected<std::decay_t<U_val>, ErrorType>` in the success state.
 *
 * @note Marked `[[nodiscard]]` (via macro) so the result is not discarded.
 * @par Exception guarantees
 *      May throw if constructing `T` from `U_val` throws.
 * @par Thread safety
 *      Thread safety depends on `T`, `ErrorType`, and their constructors.
 */
template <typename ErrorType, typename U_val>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an expected result; should always be used.")
LUMEX_CONSTEXPR_FUNCTION Expected<typename std::decay<U_val>::type,
                                  ErrorType> make_expected (U_val &&val)
{
  return Expected<typename std::decay<U_val>::type, ErrorType> (
      in_place, std::forward<U_val> (val));
}

/**
 * @brief Creates a successful `Expected<void, ErrorType>` specialization.
 * @details Returns an object in the success state with no value.
 *
 * @tparam ErrorType Error type.
 * @return `Expected<void, ErrorType>` in the success state.
 *
 * @note Marked `[[nodiscard]]` (via macro). Useful for APIs where success
 * itself matters.
 * @par Exception guarantees
 *      Does not throw if constructing `Expected<void, ErrorType>` does not
 * throw.
 */
template <typename ErrorType>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an expected result; should always be used.")
LUMEX_CONSTEXPR_FUNCTION Expected<void, ErrorType> make_expected ()
{
  return Expected<void, ErrorType> (in_place);
}

/**
 * @brief Creates an error `Expected<SuccessType, E>` from an error value.
 * @details Wraps the given error in `Unexpected<E>` and returns the matching
 * `Expected` in the error state.
 *
 * @tparam SuccessType Success-value type (parameterizes the returned
 * `Expected`).
 * @tparam U_err       Input error type; the resulting error type is `E =
 * std::decay_t<U_err>`.
 * @param[in] err      Error object stored (copied or moved) inside `Expected`.
 * @return `Expected<SuccessType, std::decay_t<U_err>>` in the error state.
 *
 * @note Marked `[[nodiscard]]` (via macro).
 * @par Exception guarantees
 *      May throw if copy/move of `E` throws.
 * @par Thread safety
 *      Thread safety depends on the properties of error type `E`.
 * @deprecated Use make_unexpected<E>(...) returning Unexpected<E> and
 * Expected(unexpect_t, ...) instead of this function.
 */
template <typename SuccessType, typename U_err>
LUMEX_ATTRIBUTE_DEPRECATED_MSG (
    "Use make_unexpected<E>(...) returning Unexpected<E> and "
    "Expected(unexpect_t, ...) instead.")
LUMEX_CONSTEXPR_FUNCTION
    Expected<SuccessType, typename std::decay<U_err>::type> make_unexpected (
        U_err &&err)
{
  return Expected<SuccessType, typename std::decay<U_err>::type> (
      Unexpected<typename std::decay<U_err>::type> (
          std::forward<U_err> (err)));
}

/**
 * @brief Creates an error `Expected<void, ErrorType>` from an error value.
 * @details Wraps the given error in `Unexpected<ErrorType>` and returns
 * `Expected<void, ErrorType>` in the error state.
 *
 * @tparam ErrorType Error type.
 * @param[in] err Error object (copied or moved).
 * @return `Expected<void, ErrorType>` in the error state.
 *
 * @note Marked `[[nodiscard]]` (via macro).
 * @par Exception guarantees
 *      May throw if copy/move of `ErrorType` throws.
 * @par Thread safety
 *      Not thread-safe if the returned object is shared without
 * synchronization.
 * @deprecated Use make_unexpected<E>(...) returning Unexpected<E> and
 * Expected(unexpect_t, ...) instead of this function.
 */
template <typename ErrorType>
LUMEX_ATTRIBUTE_DEPRECATED_MSG (
    "Use make_unexpected<E>(...) returning Unexpected<E> and "
    "Expected(unexpect_t, ...) instead.")
LUMEX_CONSTEXPR_FUNCTION Expected<void, ErrorType> make_unexpected (
    ErrorType &&err)
{
  return Expected<void, ErrorType> (
      Unexpected<ErrorType> (std::forward<ErrorType> (err)));
}

/**
 * @brief Standard error factory: creates Unexpected<E> in place.
 */
template <typename E, typename... Args>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an unexpected value; should always be used.")
LUMEX_CONSTEXPR_FUNCTION
    Unexpected<typename std::decay<E>::type> make_unexpected (Args &&...args)
{
  using Err = typename std::decay<E>::type;
  return Unexpected<Err> (Err (std::forward<Args> (args)...));
}

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::result::Expected;
using lumex::core::expected::result::make_unexpected;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP

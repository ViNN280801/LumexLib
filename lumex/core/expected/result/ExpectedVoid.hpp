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
 * @file ExpectedVoid.hpp
 * @brief The specialization `expected<void, ErrorType>` for operations that
 * succeed without a value; an implementation of C++23 `std::expected<void,
 * E>`.
 * @details It follows [expected.void] of the working draft and keeps the
 * interface of the primary template where it applies to an absent value:
 * `has_value()`, `has_error()`, `value()` (which only checks the state and
 * throws `bad_expected_access` on an error), `error()`, `error_or()`,
 * `emplace()`, `emplace_error()`, `swap()`, the converting constructors from
 * an `expected<void, G>`, the assignment from an `unexpected`, and the monadic
 * operations, whose functions take no arguments in the success state. The
 * header includes `Expected.hpp`, so it is enough for both forms. The
 * differences from `std::expected` listed at the class `expected` in
 * `Expected.hpp` apply here too, and so does the table of what is `constexpr`
 * in each standard.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#include "Expected.hpp"
#include "ExpectedDetail.hpp"
#include "ExpectedStorage.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ====================== Specialization for expected<void, ErrorType>
// ======================

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
/**
 * @brief `expected` specialization when the success value is absent (`void`).
 * @see https://eel.is/c++draft/expected.void
 * @details Lets `expected` be used for functions that either succeed
 *          without returning a value, or return an error.
 *          It is the analogue of `std::expected<void, E>` from C++23.
 * @tparam ErrorType Error type.
 * @note Here `has_value()` means success without a returned value,
 *       and `!has_value()` means the object holds an error.
 * @note The storage is the one of the primary template with the empty class
 * `Unit` as the value (so the copy and move constructors, the assignments and
 * the destructor are the ones of [expected.void.cons], [expected.void.assign]
 * and [expected.void.dtor]: deleted, left out or trivial by the properties of
 * `ErrorType` alone). Works from C++11; the table of what is `constexpr` in
 * each standard is at the class `expected` in `Expected.hpp`.
 */
template <typename ErrorType>
class expected<void, ErrorType>
    : private detail::storage::expected_base<Unit, ErrorType>
{
  using base_type = detail::storage::expected_base<Unit, ErrorType>;

public:
  // ====================== Aliases ====================== //
  using value_type = void;
  using error_type = ErrorType;
  using unexpected_type = unexpected<ErrorType>;

  /**
   * @brief Alias that rebases expected to another success value.
   * @tparam U New success value type.
   * @note This alias simplifies type conversions in user code and tests.
   */
  template <typename U> using rebind = expected<U, ErrorType>;

  // ====================== Constructors ====================== //
  // The copy and the move constructor are the implicit ones: they come from
  // the bases and are deleted, left out or trivial as the standard says.

  /**
   * @brief Default constructor (successful void state)
   * @details Creates an `expected` in the success state, without a value.
   * @note Guaranteed not to throw (`noexcept`) and usable in a constant
   * expression.
   */
  LUMEX_CONSTEXPR_CTOR
  expected () LUMEX_NOEXCEPT : base_type (detail::value_tag ()) {}

  /**
   * @brief Constructor in the success state (the `in_place` tag).
   * @details Creates an `expected` in the success state, as
   * `std::expected<void, E> (std::in_place)`.
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_CONSTEXPR_CTOR explicit expected (in_place_tag /* unused */)
      LUMEX_NOEXCEPT : base_type (detail::value_tag ())
  {
  }

  /**
   * @brief Constructs the error in place via the unexpect tag.
   * @tparam Args Argument types forwarded to the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @note Takes part in overload resolution only when `ErrorType` is
   * constructible from `Args`; `noexcept` when that construction is.
   */
  template <typename... Args,
            typename = typename std::enable_if<
                std::is_constructible<ErrorType, Args...>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit expected (unexpect_t /*unused*/,
                                          Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, Args...>::value)
      : base_type (detail::error_tag (), detail::fwd<Args> (args)...)
  {
  }

  /**
   * @brief Constructs the error in place from an initializer list.
   * @tparam U Element type of the initializer list.
   * @tparam Args Argument types forwarded after the list.
   * @param[in] list Initializer list passed to the `ErrorType` constructor.
   * @param[in] args Arguments forwarded after the list.
   * @note Takes part in overload resolution only when `ErrorType` is
   * constructible from the list and `Args`.
   */
  template <typename U, typename... Args,
            typename = typename std::enable_if<std::is_constructible<
                ErrorType, std::initializer_list<U> &, Args...>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit expected (unexpect_t /*unused*/,
                                          std::initializer_list<U> list,
                                          Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, std::initializer_list<U> &,
                                        Args...>::value)
      : base_type (detail::error_tag (), list, detail::fwd<Args> (args)...)
  {
  }

  /**
   * @brief Converting constructor from an `expected<void, G>` (copy,
   * implicit).
   * @details Creates an `expected` with the state of `other`, copying the
   * error into one of this type. Implicit when `G const &` converts to
   * `ErrorType` ([expected.void.cons]/12 to /16).
   * @tparam U `void` (the success type of `other`).
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert.
   * @throws May throw if the conversion of the error throws.
   */
  template <
      typename U, typename G,
      typename std::enable_if<
          std::is_void<U>::value
              && std::is_constructible<ErrorType, G const &>::value
              && !detail::unexpected_from_expected<ErrorType, U, G>::value
              && std::is_convertible<G const &, ErrorType>::value,
          int>::type
      = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected (expected<U, G> const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::convert_tag (), other)
  {
  }

  /**
   * @brief Converting constructor from an `expected<void, G>` (copy,
   * explicit).
   * @details As the implicit one, for a `G` that `ErrorType` is constructible
   * from but not convertible from.
   * @tparam U `void` (the success type of `other`).
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert.
   * @throws May throw if the conversion of the error throws.
   */
  template <
      typename U, typename G,
      typename std::enable_if<
          std::is_void<U>::value
              && std::is_constructible<ErrorType, G const &>::value
              && !detail::unexpected_from_expected<ErrorType, U, G>::value
              && !std::is_convertible<G const &, ErrorType>::value,
          int>::type
      = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 explicit expected (
      expected<U, G> const &other)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::convert_tag (), other)
  {
  }

  /**
   * @brief Converting constructor from an `expected<void, G>` (move,
   * implicit).
   * @details Creates an `expected` with the state of `other`, moving the error
   * into one of this type. Implicit when `G` converts to `ErrorType`.
   * @tparam U `void` (the success type of `other`).
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert; its error is moved from.
   * @throws May throw if the conversion of the error throws.
   */
  template <
      typename U, typename G,
      typename std::enable_if<
          std::is_void<U>::value && std::is_constructible<ErrorType, G>::value
              && !detail::unexpected_from_expected<ErrorType, U, G>::value
              && std::is_convertible<G, ErrorType>::value,
          int>::type
      = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected (expected<U, G> &&other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::convert_tag (), detail::mv (other))
  {
  }

  /**
   * @brief Converting constructor from an `expected<void, G>` (move,
   * explicit).
   * @details As the implicit one, for a `G` that `ErrorType` is constructible
   * from but not convertible from.
   * @tparam U `void` (the success type of `other`).
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert; its error is moved from.
   * @throws May throw if the conversion of the error throws.
   */
  template <
      typename U, typename G,
      typename std::enable_if<
          std::is_void<U>::value && std::is_constructible<ErrorType, G>::value
              && !detail::unexpected_from_expected<ErrorType, U, G>::value
              && !std::is_convertible<G, ErrorType>::value,
          int>::type
      = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 explicit expected (expected<U, G> &&other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::convert_tag (), detail::mv (other))
  {
  }

  /**
   * @brief Constructor from `unexpected<G>` (copy, implicit).
   * @details Creates an `expected` in the error state by copying the error
   * from `unex`, so a function that returns an `expected<void, E>` can write
   * `return unexpected<E> (error);`. Implicit when `G const &` converts to
   * `ErrorType` ([expected.void.cons]/17 to /21).
   * @tparam G Error type of `unex`; `ErrorType` is constructible from it.
   * @param[in] unex Const reference to `unexpected<G>` that holds an error.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  template <typename G = ErrorType,
            typename std::enable_if<
                std::is_constructible<ErrorType, G const &>::value
                    && std::is_convertible<G const &, ErrorType>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  expected (unexpected<G> const &unex) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::error_tag (), unex.error ())
  {
  }

  /**
   * @brief Constructor from `unexpected<G>` (copy, explicit).
   * @details As the implicit one, for a `G` that `ErrorType` is constructible
   * from but not convertible from, so the conversion must be written out.
   * @tparam G Error type of `unex`.
   * @param[in] unex Const reference to `unexpected<G>` that holds an error.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  template <typename G = ErrorType,
            typename std::enable_if<
                std::is_constructible<ErrorType, G const &>::value
                    && !std::is_convertible<G const &, ErrorType>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR explicit expected (unexpected<G> const &unex)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::error_tag (), unex.error ())
  {
  }

  /**
   * @brief Constructor from `unexpected<G>` (move, implicit).
   * @details Creates an `expected` in the error state by moving the error
   * value from `unex`. Implicit when `G` converts to `ErrorType`.
   * @tparam G Error type of `unex`; `ErrorType` is constructible from it.
   * @param[in] unex Rvalue reference to `unexpected<G>` that holds the error.
   * @throws May throw if the move constructor of `ErrorType` throws.
   */
  template <
      typename G = ErrorType,
      typename std::enable_if<std::is_constructible<ErrorType, G>::value
                                  && std::is_convertible<G, ErrorType>::value,
                              int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR
  expected (unexpected<G> &&unex)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::error_tag (), detail::move_error (unex))
  {
  }

  /**
   * @brief Constructor from `unexpected<G>` (move, explicit).
   * @details As the implicit one, for a `G` that `ErrorType` is constructible
   * from but not convertible from.
   * @tparam G Error type of `unex`; `ErrorType` is constructible from it.
   * @param[in] unex Rvalue reference to `unexpected<G>` that holds the error.
   * @throws May throw if the move constructor of `ErrorType` throws.
   */
  template <
      typename G = ErrorType,
      typename std::enable_if<std::is_constructible<ErrorType, G>::value
                                  && !std::is_convertible<G, ErrorType>::value,
                              int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR explicit expected (unexpected<G> &&unex)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::error_tag (), detail::move_error (unex))
  {
  }

  // The destructor is the implicit one: trivial when ErrorType is trivially
  // destructible ([expected.void.dtor]/2).

  // ====================== Assignment Operators ====================== //
  // The copy and the move assignment are the implicit ones, see above.

  /**
   * @brief Replaces the contents with an error (copy).
   * @details If the object holds an error, it is assigned; otherwise the
   * error is constructed ([expected.void.assign]/12).
   * @tparam G Error type of `unex`; `ErrorType` is constructible and
   * assignable from it.
   * @param[in] unex The new error.
   * @return Reference to this `expected`, now in the error state.
   * @throws May throw if copying the error throws.
   */
  template <typename G,
            typename std::enable_if<
                std::is_constructible<ErrorType, G const &>::value
                    && std::is_assignable<ErrorType &, G const &>::value,
                int>::type
            = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 expected &
  operator= (unexpected<G> const &unex)
  {
    assign_error (unex.error ());
    return *this;
  }

  /**
   * @brief Replaces the contents with an error (move).
   * @details As the copy assignment from an `unexpected`, moving the error.
   * @tparam G Error type of `unex`; `ErrorType` is constructible and
   * assignable from it.
   * @param[in] unex The new error; it is moved from.
   * @return Reference to this `expected`, now in the error state.
   * @throws May throw if moving the error throws.
   */
  template <typename G, typename std::enable_if<
                            std::is_constructible<ErrorType, G>::value
                                && std::is_assignable<ErrorType &, G>::value,
                            int>::type
                        = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 expected &
  operator= (unexpected<G> &&unex)
  {
    assign_error (detail::move_error (unex));
    return *this;
  }

  // ====================== Observers ======================

  /**
   * @brief Checks whether this `expected` is in the success state.
   * @return `true` if the object is a success, `false` if it holds an error.
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
   * @brief Checks whether this `expected` holds an error.
   * @details `!has_value ()` ([expected.void.obs]/2 of the working draft,
   * which C++23 does not have).
   * @return `true` if the object holds an error, `false` otherwise.
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION bool
  has_error () const LUMEX_NOEXCEPT
  {
    return !m_has_value;
  }

  /**
   * @brief Explicit conversion to `bool`.
   * @details Lets `expected` be used where a condition is expected (for
   * example, `if (myExpected)`); elsewhere the conversion must be written out.
   * @return `true` if the object is a success, `false` otherwise.
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION explicit
  operator bool () const LUMEX_NOEXCEPT
  {
    return m_has_value;
  }

  /**
   * @brief Checks the state; throws if the object holds an error.
   * @details The `void` counterpart of `value ()` ([expected.void.obs]/4 and
   * /5): there is no value to return.
   * @throws bad_expected_access<ErrorType> that holds a copy of the error if
   * the object does not hold a success.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () const &
  {
    LUMEX_STATIC_ASSERT_MSG (std::is_copy_constructible<ErrorType>::value,
                             "value (): ErrorType must be copy constructible");
    return m_has_value
               ? void ()
               : detail::throw_bad_expected_access<ErrorType> (
                     static_cast<ErrorType const &> (m_storage.m_error));
  }

  /**
   * @brief Checks the state; throws if the object holds an error (rvalue).
   * @details As the lvalue form, moving the error into the exception
   * ([expected.void.obs]/6 and /7).
   * @throws bad_expected_access<ErrorType> that holds the moved error if the
   * object does not hold a success.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () &&
  {
    LUMEX_STATIC_ASSERT_MSG (
        std::is_copy_constructible<ErrorType>::value
            && std::is_move_constructible<ErrorType>::value,
        "value (): ErrorType must be copy and move constructible");
    if (!m_has_value)
      detail::throw_bad_expected_access<ErrorType> (
          detail::mv (m_storage.m_error));
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &
      error ()
      & LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected<void> that holds no error"),
           m_storage.m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &
  error () const &LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected<void> that holds no error"),
           m_storage.m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Rvalue reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &&
      error ()
      && LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected<void> that holds no error"),
           static_cast<ErrorType &&> (m_storage.m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const rvalue reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &&
  error () const &&LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected<void> that holds no error"),
           static_cast<ErrorType const &&> (m_storage.m_error);
  }

  /**
   * @brief Returns the stored error if present, otherwise the default error.
   * @tparam G Default-error type; must be convertible to `ErrorType`.
   * @param[in] default_error Error returned if the object is a success.
   * @return The stored error or `default_error`.
   * @note Not declared `noexcept`: copying the stored error or converting
   * `default_error` to `ErrorType` may throw.
   */
  template <typename G = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained error or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or (G &&default_error) const &
  {
    LUMEX_STATIC_ASSERT_MSG (
        std::is_copy_constructible<ErrorType>::value
            && std::is_convertible<G, ErrorType>::value,
        "error_or (): ErrorType must be copy constructible and G "
        "convertible to it");
    return m_has_value
               ? static_cast<ErrorType> (detail::fwd<G> (default_error))
               : static_cast<ErrorType const &> (m_storage.m_error);
  }

  /**
   * @brief Returns the error by move if present, otherwise the default error.
   * @tparam G Default-error type; must be convertible to `ErrorType`.
   * @param[in] default_error Error returned if the object is a success.
   * @return The moved error or `default_error`.
   * @note Not declared `noexcept`: moving the stored error or converting
   * `default_error` to `ErrorType` may throw.
   */
  template <typename G = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the error or a "
                             "default-constructed substitute; should be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType error_or (G &&default_error) &&
  {
    LUMEX_STATIC_ASSERT_MSG (
        std::is_move_constructible<ErrorType>::value
            && std::is_convertible<G, ErrorType>::value,
        "error_or (): ErrorType must be move constructible and G "
        "convertible to it");
    return m_has_value
               ? static_cast<ErrorType> (detail::fwd<G> (default_error))
               : static_cast<ErrorType &&> (m_storage.m_error);
  }

  /**
   * @brief Dereference operator (void).
   * @details Does nothing but check the precondition ([expected.void.obs]/3).
   * @warning Assumes `expected` is in the success state. If it is not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   */
  LUMEX_CONSTEXPR_CXX14 void
  operator* () const LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
        m_has_value
        && "operator* called on an Expected<void> that holds an error");
  }

  // ====================== Modifiers ======================

  /**
   * @brief Puts the object in the success state.
   * @details If the object holds an error, it is destroyed
   * ([expected.void.assign]/14).
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  emplace () LUMEX_NOEXCEPT
  {
    if (!m_has_value)
      {
        detail::destroy_at (detail::address_of (m_storage.m_error));
        detail::construct_at (detail::address_of (m_storage.m_value));
        m_has_value = true;
      }
  }

  /**
   * @brief Constructs an `ErrorType` value in place, replacing the current
   * contents.
   * @details An addition of this library: the new error is built by
   * `reinit-expected` ([expected.object.assign]/1) in the place of the current
   * error, so if the `ErrorType` constructor throws, this object keeps its
   * previous contents.
   * @tparam Args Argument types for the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @return Reference to the newly constructed `ErrorType` error.
   * @note Takes part in overload resolution only when `ErrorType` is
   * constructible from `Args`.
   */
  template <typename... Args,
            typename = typename std::enable_if<
                std::is_constructible<ErrorType, Args...>::value>::type>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 ErrorType &
  emplace_error (Args &&...args)
  {
    if (m_has_value)
      detail::reinit (m_storage.m_error, m_storage.m_value,
                      detail::fwd<Args> (args)...);
    else
      detail::reinit (m_storage.m_error, m_storage.m_error,
                      detail::fwd<Args> (args)...);
    m_has_value = false;
    return m_storage.m_error;
  }

  /**
   * @brief Exchanges contents with another `expected<void, ErrorType>`.
   * @details Two successes are left as they are; two errors are swapped with
   * `swap` found by argument-dependent lookup; a success and an error move the
   * error across (Table 73 of [expected.void.swap]).
   * @param[in,out] other The other `expected` object to swap with.
   * @note Takes part in overload resolution only when `ErrorType` is
   * swappable and move constructible, like `std::expected`. `noexcept` when
   * `ErrorType` is nothrow move constructible and nothrow swappable.
   * @throws May throw if the move constructor or `swap` of `ErrorType`
   * throws.
   */
  template <typename Self = expected,
            typename = typename std::enable_if<
                detail::can_swap_errors<Self, ErrorType>::value>::type>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap (expected &other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<ErrorType>::value
                             &&detail::is_nothrow_swappable<ErrorType>::value)
  {
    this->swap_storage (other);
  }

  // ====================== Monadic Operations ======================
  // Each operation exists for the four value categories of the object. The
  // function is taken as a forwarding reference and called as by std::invoke,
  // the constraint is written once in detail:: and used as a requires-clause
  // from C++20 and as std::enable_if before. A function of the success state
  // takes no arguments.

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (lvalue), and returns its `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and its result is returned. If it holds an error, `func` is not
   * called and an `expected` of the result type that holds the current error
   * is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters that returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called and returns an
   * `expected`, and `ErrorType` can be copied or moved the way the lvalue
   * call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_and_then<FunctionType, ErrorType, ErrorType &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func) & -> detail::result_clean_t<FunctionType>
  {
    using ResultType = detail::result_clean_t<FunctionType>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (const lvalue), and returns its `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and its result is returned. If it holds an error, `func` is not
   * called and an `expected` of the result type that holds the current error
   * is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters that returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called and returns an
   * `expected`, and `ErrorType` can be copied or moved the way the const
   * lvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType,
                                   ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (
      FunctionType &&func) const & -> detail::result_clean_t<FunctionType>
  {
    using ResultType = detail::result_clean_t<FunctionType>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (rvalue), and returns its `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and its result is returned. If it holds an error, `func` is not
   * called and an `expected` of the result type that holds the current error
   * is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters that returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called and returns an
   * `expected`, and `ErrorType` can be copied or moved the way the rvalue
   * call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_and_then<FunctionType, ErrorType, ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func) && -> detail::result_clean_t<FunctionType>
  {
    using ResultType = detail::result_clean_t<FunctionType>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (const rvalue), and returns its `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and its result is returned. If it holds an error, `func` is not
   * called and an `expected` of the result type that holds the current error
   * is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters that returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called and returns an
   * `expected`, and `ErrorType` can be copied or moved the way the const
   * rvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType,
                                   ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (
      FunctionType &&func) const && -> detail::result_clean_t<FunctionType>
  {
    using ResultType = detail::result_clean_t<FunctionType>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (lvalue) if present, and
   * returns its `expected`.
   * @details If the object holds an error, `func` is called with an lvalue
   * reference to it and its result is returned. If it is a success, `func` is
   * not called and an `expected<void, G>` in the success state is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<void, G>`; the error type `G` may differ from `ErrorType`.
   * @return The result of `func` or a success, as an `expected<void, G>`
   * without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`; an `expected` with a value type other than
   * `void` fails a `static_assert`.
   * @throws May throw if `func` throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_or_else<FunctionType, void, void, ErrorType &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, void, void, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      & -> detail::result_clean_t<FunctionType, ErrorType &>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, void>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType ()
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (const lvalue) if present,
   * and returns its `expected`.
   * @details If the object holds an error, `func` is called with a const
   * lvalue reference to it and its result is returned. If it is a success,
   * `func` is not called and an `expected<void, G>` in the success state is
   * returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<void, G>`; the error type `G` may differ from `ErrorType`.
   * @return The result of `func` or a success, as an `expected<void, G>`
   * without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`; an `expected` with a value type other than
   * `void` fails a `static_assert`.
   * @throws May throw if `func` throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_or_else<FunctionType, void, void,
                                  ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, void, void, ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType &&func)
      const & -> detail::result_clean_t<FunctionType, ErrorType const &>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType const &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, void>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType ()
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (rvalue) if present, and
   * returns its `expected`.
   * @details If the object holds an error, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and its result is
   * returned. If it is a success, `func` is not called and an `expected<void,
   * G>` in the success state is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<void, G>`; the error type `G` may differ from `ErrorType`.
   * @return The result of `func` or a success, as an `expected<void, G>`
   * without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`; an `expected` with a value type other than
   * `void` fails a `static_assert`.
   * @throws May throw if `func` throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_or_else<FunctionType, void, void, ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, void, void, ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      && -> detail::result_clean_t<FunctionType, ErrorType &&>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, void>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType ()
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (const rvalue) if present,
   * and returns its `expected`.
   * @details If the object holds an error, `func` is called with a const
   * rvalue reference to it and its result is returned. If it is a success,
   * `func` is not called and an `expected<void, G>` in the success state is
   * returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<void, G>`; the error type `G` may differ from `ErrorType`.
   * @return The result of `func` or a success, as an `expected<void, G>`
   * without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`; an `expected` with a value type other than
   * `void` fails a `static_assert`.
   * @throws May throw if `func` throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_or_else<FunctionType, void, void,
                                  ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, void, void, ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType &&func)
      const && -> detail::result_clean_t<FunctionType, ErrorType const &&>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, ErrorType const &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, void>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType ()
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (lvalue), and wraps the result in an `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If it
   * holds an error, `func` is not called and an `expected` holding the current
   * error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters; its result may be any type:
   * `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called and `ErrorType`
   * can be copied or moved the way the lvalue call needs. A result type that
   * `expected` cannot hold (a reference, an array, `in_place_tag`,
   * `unexpect_t`, an `unexpected`) fails the `static_assert` of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_transform<FunctionType, ErrorType, ErrorType &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func)
      & -> expected<detail::result_xform_t<FunctionType>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType>, ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (const lvalue), and wraps the result in an `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If it
   * holds an error, `func` is not called and an `expected` holding the current
   * error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters; its result may be any type:
   * `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called and `ErrorType`
   * can be copied or moved the way the const lvalue call needs. A result type
   * that `expected` cannot hold (a reference, an array, `in_place_tag`,
   * `unexpect_t`, an `unexpected`) fails the `static_assert` of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform<FunctionType, ErrorType,
                                    ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType &&func)
      const & -> expected<detail::result_xform_t<FunctionType>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType>, ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (rvalue), and wraps the result in an `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If it
   * holds an error, `func` is not called and an `expected` holding the current
   * error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters; its result may be any type:
   * `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called and `ErrorType`
   * can be copied or moved the way the rvalue call needs. A result type that
   * `expected` cannot hold (a reference, an array, `in_place_tag`,
   * `unexpect_t`, an `unexpected`) fails the `static_assert` of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_transform<FunctionType, ErrorType, ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func)
      && -> expected<detail::result_xform_t<FunctionType>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType>, ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` if this `expected<void, ErrorType>` is a success
   * (const rvalue), and wraps the result in an `expected`.
   * @details If the object is in the success state, `func` is called with no
   * arguments and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If it
   * holds an error, `func` is not called and an `expected` holding the current
   * error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function without parameters; its result may be any type:
   * `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called and `ErrorType`
   * can be copied or moved the way the const rvalue call needs. A result type
   * that `expected` cannot hold (a reference, an array, `in_place_tag`,
   * `unexpect_t`, an `unexpected`) fails the `static_assert` of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform<FunctionType, ErrorType,
                                    ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType &&func)
      const && -> expected<detail::result_xform_t<FunctionType>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType>, ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (lvalue) if present, and
   * wraps the result in the error of a new `expected`.
   * @details If the object holds an error, `func` is called with an lvalue
   * reference to it and an `expected<void, G>` holding its result as the error
   * is returned. If it is a success, `func` is not called and an
   * `expected<void, G>` in the success state is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<void, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, void, void,
                                          ErrorType &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, void, void, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func)
      & -> expected<void, detail::result_xform_t<FunctionType, ErrorType &>>
  {
    using ResultType
        = expected<void, detail::result_xform_t<FunctionType, ErrorType &>>;
    return m_has_value ? ResultType ()
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (const lvalue) if present,
   * and wraps the result in the error of a new `expected`.
   * @details If the object holds an error, `func` is called with a const
   * lvalue reference to it and an `expected<void, G>` holding its result as
   * the error is returned. If it is a success, `func` is not called and an
   * `expected<void, G>` in the success state is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<void, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, void, void,
                                          ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, void, void, ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType &&func) const & -> expected<
      void, detail::result_xform_t<FunctionType, ErrorType const &>>
  {
    using ResultType
        = expected<void,
                   detail::result_xform_t<FunctionType, ErrorType const &>>;
    return m_has_value ? ResultType ()
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (rvalue) if present, and
   * wraps the result in the error of a new `expected`.
   * @details If the object holds an error, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and an `expected<void,
   * G>` holding its result as the error is returned. If it is a success,
   * `func` is not called and an `expected<void, G>` in the success state is
   * returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<void, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, void, void,
                                          ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, void, void, ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func)
      && -> expected<void, detail::result_xform_t<FunctionType, ErrorType &&>>
  {
    using ResultType
        = expected<void, detail::result_xform_t<FunctionType, ErrorType &&>>;
    return m_has_value ? ResultType ()
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (const rvalue) if present,
   * and wraps the result in the error of a new `expected`.
   * @details If the object holds an error, `func` is called with a const
   * rvalue reference to it and an `expected<void, G>` holding its result as
   * the error is returned. If it is a success, `func` is not called and an
   * `expected<void, G>` in the success state is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<void, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, void, void,
                                          ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, void, void, ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType &&func) const && -> expected<
      void, detail::result_xform_t<FunctionType, ErrorType const &&>>
  {
    using ResultType
        = expected<void,
                   detail::result_xform_t<FunctionType, ErrorType const &&>>;
    return m_has_value ? ResultType ()
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_error));
  }

private:
  template <typename, typename> friend class expected;

  // The members of the storage `expected` is built on (the names are those
  // of the exposition-only members of the standard: `has_val` and the union
  // that holds `unex`).
  using base_type::m_has_value;
  using base_type::m_storage;

  /**
   * @brief Builds the success state from a call.
   * @details Used by `transform`: the call is made and its `void` result
   * dropped, so the stored state is the success state.
   * @tparam Fn Type of the function.
   * @tparam Args Types of its arguments.
   * @param[in] fn The function.
   * @param[in] args Arguments of the call.
   */
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected (detail::invoke_value_tag /*unused*/,
                                          Fn &&fn, Args &&...args)
      : base_type (detail::value_tag (),
                   (static_cast<void> (detail::invoke_call (
                        detail::fwd<Fn> (fn), detail::fwd<Args> (args)...)),
                    Unit ()))
  {
  }

  /**
   * @brief Builds an error from the result of a call.
   * @details Counterpart of the constructor above for `transform_error`.
   * @tparam Fn Type of the function.
   * @tparam Args Types of its arguments.
   * @param[in] tag Selects the constructor.
   * @param[in] fn The function.
   * @param[in] args Arguments of the call.
   */
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected (detail::invoke_error_tag tag,
                                          Fn &&fn, Args &&...args)
      : base_type (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...)
  {
  }

  /**
   * @brief Makes `err` the error: assigns it if there is one, otherwise
   * constructs it ([expected.void.assign]/12).
   * @tparam GF The error as the caller forwards it (`G const &` or `G`).
   * @param[in] err The new error.
   */
  template <typename GF>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  assign_error (GF &&err)
  {
    if (m_has_value)
      {
        detail::construct_at (detail::address_of (m_storage.m_error),
                              detail::fwd<GF> (err));
        m_has_value = false;
      }
    else
      m_storage.m_error = detail::fwd<GF> (err);
  }
};

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP

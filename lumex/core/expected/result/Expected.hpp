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
 * @file Expected.hpp
 * @brief `expected<SuccessType, ErrorType>`, an analogue of C++23
 * `std::expected` usable from C++11, with its non-member functions and
 * factories.
 * @details The class holds either a value or an error in a union and manages
 * their lifetimes by hand. It provides the observers of `std::expected`
 * (`has_value()`, `value()`, which throws `bad_expected_access`, `error()`,
 * `value_or()`, `error_or()`, `operator*`), `emplace()`, `swap()` and the
 * monadic operations `and_then()`, `transform()`, `or_else()` and
 * `transform_error()`, which are constrained with concepts from C++20 and with
 * SFINAE before. The header also declares the non-member `operator==` and
 * `swap()`, `make_expected()` and `make_unexpected<E>()`; the overloads of
 * `make_unexpected()` that return an `expected` are deprecated. The
 * specialization for a `void` value is in `ExpectedVoid.hpp`. Header-only,
 * part of `lumex::expected`; `expected` and `make_unexpected` are also visible
 * at global scope.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP

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

#include <cassert>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "ExpectedDetail.hpp"
#include "ExpectedTypes.hpp"
#include "lumex/core/expected/error/BadExpectedAccess.hpp"
#include "lumex/core/expected/error/Unexpected.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
using error::bad_expected_access;
using error::unexpected;

/**
 * @brief Class that mimics std::expected from C++23.
 * @see https://en.cppreference.com/w/cpp/utility/expected
 * @details Holds either a value of type SuccessType,
 *          or an error of type ErrorType. Functions can return either a
 * success result, or error information without exceptions for expected
 * failures. The implementation uses a union to save memory and manually
 * manages the lifetime of the stored objects.
 * @tparam SuccessType Success value type.
 * @tparam ErrorType Error type.
 * @note This implementation is not thread-safe by default. Accessing
 * `expected` from several threads without external synchronization is
 * undefined behavior if `SuccessType` or `ErrorType` are not thread-safe.
 * @warning Using `expected` with types that have non-trivial
 * constructors/destructors or allocate memory, may be slower than C++23
 * `std::expected`, because object lifetime is managed by hand.
 * @note Works from C++11. A member that is not const, or whose body needs
 * more than one statement, is marked `LUMEX_CONSTEXPR_CXX14`: it is
 * `constexpr` from C++14 and an ordinary function at C++11, where a
 * `constexpr` member function is implicitly const (so `error () &` would
 * collide with `error () const &`) and its body must be a single return
 * statement.
 * @par Differences from std::expected
 * - `error()`, `operator*` and `operator->` check their precondition with
 *   `LUMEX_ASSERT`, which aborts in every build including `NDEBUG`, where the
 *   standard leaves the call undefined.
 * - The tags are `in_place_tag` / `in_place` and `unexpect_t` / `unexpect` of
 *   this module, not `std::in_place_t` and `std::unexpect_t`, so that the
 *   class works from C++11.
 * - The copy and move assignments, the assignment from a value or an
 *   `unexpected`, and `emplace()` build the new contents first and then swap
 *   them in (the strong guarantee), so they need a movable `SuccessType` and
 *   `ErrorType` and have no `noexcept` requirement.
 * - The copy constructor and the assignments are not removed when
 *   `SuccessType` or `ErrorType` cannot be copied (C++11 cannot delete a
 *   member conditionally), so `std::is_copy_constructible` is `true` and the
 *   copy fails inside the constructor; the class is never trivially copyable
 *   and cannot be used in a constant expression.
 * - `emplace_error()`, `rebind`, `Unit`, `make_expected()` and the
 *   `success()` / `failure()` markers are additions.
 */
template <typename SuccessType, typename ErrorType> class expected
{
public:
  // ====================== Static assertions ====================== //
  // References are forbidden because:
  // 1. This class (and its specializations) is meant to
  //    own the stored success or error value. If SuccessType or ErrorType were
  //    references, for example `int&`, expected would not own the object, only
  //    refer to it. That can dangle if the original object referred to by
  //    SuccessType (or ErrorType) is destroyed before expected.
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
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<SuccessType>::value,
                           "Expected<T,E>: T must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_void<SuccessType>::value,
                           "Expected<T,E>: T must not be cv void; "
                           "use Expected<void,E>");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::remove_cv<SuccessType>::type,
                    in_place_tag>::value,
      "Expected<T,E>: T must not be in_place_tag");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::remove_cv<SuccessType>::type,
                    unexpect_t>::value,
      "Expected<T,E>: T must not be unexpect_t");
  LUMEX_STATIC_ASSERT_MSG (
      !detail::is_unexpected<
          typename std::remove_cv<SuccessType>::type>::value,
      "Expected<T,E>: T must not be a specialization of unexpected");

  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<ErrorType>::value,
                           "Expected<T,E>: E must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<ErrorType>::value,
                           "Expected<T,E>: E must not be a function type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_void<ErrorType>::value,
                           "Expected<T,E>: E must not be void");
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<ErrorType>::value,
                           "Expected<T,E>: E must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_const<ErrorType>::value
                               && !std::is_volatile<ErrorType>::value,
                           "Expected<T,E>: E must not be cv-qualified");
  LUMEX_STATIC_ASSERT_MSG (
      !detail::is_unexpected<ErrorType>::value,
      "Expected<T,E>: E must not be a specialization of unexpected");

  // ====================== Aliases ====================== //
  using value_type = SuccessType;
  using error_type = ErrorType;
  using unexpected_type = unexpected<ErrorType>;

  /**
   * @brief Alias that rebases expected to another success value.
   * @tparam U New success value type.
   * @note This alias simplifies type conversions in user code and tests.
   */
  template <typename U> using rebind = expected<U, ErrorType>;

  // ====================== Constructors ====================== //

  /**
   * @brief Default constructor.
   * @details Creates an `expected` in the success state holding the value
   * `SuccessType()`, value-initialized.
   * @note Takes part in overload resolution only when `SuccessType` is
   * default-constructible, so `std::is_default_constructible` tells the
   * truth, like `std::expected`. `noexcept` when the default constructor of
   * `SuccessType` is.
   * @throws May throw if the default constructor of `SuccessType` throws.
   */
  template <typename U = SuccessType,
            typename = typename std::enable_if<
                std::is_default_constructible<U>::value>::type>
  LUMEX_CONSTEXPR_CXX14
  expected () LUMEX_NOEXCEPT_IF (
      std::is_nothrow_default_constructible<SuccessType>::value)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value)) SuccessType ();
  }

  /**
   * @brief Copy constructor.
   * @details Creates a new `expected` by copying the state and the contained
   * value or error from `other`.
   * @param[in] other `expected` object to copy.
   * @note Not declared `noexcept`.
   * @throws May throw if the copy constructor of `SuccessType` or `ErrorType`
   * throws.
   */
  LUMEX_CONSTEXPR_CXX14
  expected (expected const &other) : m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (other.m_storage.m_value);
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (other.m_storage.m_error);
  }

  /**
   * @brief Move constructor.
   * @details Creates a new `expected` by moving the state and contained value
   * or error from `other`. After the constructor, `other` is valid but
   * unspecified.
   * @param[in] other `expected` object to move.
   * @note Conditionally `noexcept` if the constructors of move of
   * `SuccessType` and `ErrorType` do not throw.
   * @throws May throw if the constructor of move of `SuccessType` or
   * `ErrorType` throws.
   */
  LUMEX_CONSTEXPR_CXX14
  expected (expected &&other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (std::move (other.m_storage.m_value));
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (std::move (other.m_storage.m_error));
  }

  /**
   * @brief Converting constructor from an `expected<U, G>` (copy, implicit).
   * @details Creates an `expected` with the state of `other`, copying the
   * value or the error into one of this type. Implicit when both conversions
   * `U const &` to `SuccessType` and `G const &` to `ErrorType` are.
   * @tparam U Success type of `other`.
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert.
   * @note Takes part in overload resolution only when `SuccessType` and
   * `ErrorType` can be constructed from the other types and `other` cannot be
   * converted as a whole (the value constructor then does it), like
   * `std::expected`. For a `bool` success type that check is skipped:
   * `expected<bool, E>` built from an `expected<int, E>` converts the value
   * inside, it does not collapse the state to `true` or `false`.
   * @throws May throw if the conversion of the value or the error throws.
   */
  template <typename U, typename G,
            typename std::enable_if<
                std::is_constructible<SuccessType, U const &>::value
                    && std::is_constructible<ErrorType, G const &>::value
                    && !detail::constructs_from_expected<
                        SuccessType, ErrorType, U, G>::value
                    && std::is_convertible<U const &, SuccessType>::value
                    && std::is_convertible<G const &, ErrorType>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CXX14
  expected (expected<U, G> const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<SuccessType, U const &>::value
          &&std::is_nothrow_constructible<ErrorType, G const &>::value)
      : m_storage (), m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (other.m_storage.m_value);
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (other.m_storage.m_error);
  }

  /**
   * @brief Converting constructor from an `expected<U, G>` (copy, explicit).
   * @details As the implicit one, for the case where at least one of the two
   * conversions is itself explicit.
   * @tparam U Success type of `other`.
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert.
   * @throws May throw if the conversion of the value or the error throws.
   */
  template <typename U, typename G,
            typename std::enable_if<
                std::is_constructible<SuccessType, U const &>::value
                    && std::is_constructible<ErrorType, G const &>::value
                    && !detail::constructs_from_expected<
                        SuccessType, ErrorType, U, G>::value
                    && !(std::is_convertible<U const &, SuccessType>::value
                         && std::is_convertible<G const &, ErrorType>::value),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CXX14 explicit expected (expected<U, G> const &other)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, U const &>::value
              &&std::is_nothrow_constructible<ErrorType, G const &>::value)
      : m_storage (), m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (other.m_storage.m_value);
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (other.m_storage.m_error);
  }

  /**
   * @brief Converting constructor from an `expected<U, G>` (move, implicit).
   * @details Creates an `expected` with the state of `other`, moving the
   * value or the error into one of this type. Implicit when both conversions
   * `U` to `SuccessType` and `G` to `ErrorType` are.
   * @tparam U Success type of `other`.
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert; its value or error is moved from.
   * @throws May throw if the conversion of the value or the error throws.
   */
  template <
      typename U, typename G,
      typename std::enable_if<std::is_constructible<SuccessType, U>::value
                                  && std::is_constructible<ErrorType, G>::value
                                  && !detail::constructs_from_expected<
                                      SuccessType, ErrorType, U, G>::value
                                  && std::is_convertible<U, SuccessType>::value
                                  && std::is_convertible<G, ErrorType>::value,
                              int>::type
      = 0>
  LUMEX_CONSTEXPR_CXX14
  expected (expected<U, G> &&other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<SuccessType, U>::value
          &&std::is_nothrow_constructible<ErrorType, G>::value)
      : m_storage (), m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (std::move (other.m_storage.m_value));
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (std::move (other.m_storage.m_error));
  }

  /**
   * @brief Converting constructor from an `expected<U, G>` (move, explicit).
   * @details As the implicit one, for the case where at least one of the two
   * conversions is itself explicit.
   * @tparam U Success type of `other`.
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert; its value or error is moved from.
   * @throws May throw if the conversion of the value or the error throws.
   */
  template <typename U, typename G,
            typename std::enable_if<
                std::is_constructible<SuccessType, U>::value
                    && std::is_constructible<ErrorType, G>::value
                    && !detail::constructs_from_expected<
                        SuccessType, ErrorType, U, G>::value
                    && !(std::is_convertible<U, SuccessType>::value
                         && std::is_convertible<G, ErrorType>::value),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CXX14 explicit expected (expected<U, G> &&other)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, U>::value
              &&std::is_nothrow_constructible<ErrorType, G>::value)
      : m_storage (), m_has_value (other.m_has_value)
  {
    if (m_has_value)
      new (detail::voidify (m_storage.m_value))
          SuccessType (std::move (other.m_storage.m_value));
    else
      new (std::addressof (m_storage.m_error))
          ErrorType (std::move (other.m_storage.m_error));
  }

  /**
   * @brief Constructor from a success value (implicit conversion).
   * @details Creates an `expected` in the success state holding the value
   * `val`. Implicit when `U` converts to `SuccessType`.
   * @tparam U Input type convertible to `SuccessType`.
   * @param[in] val Value stored in the `expected`.
   * @note Takes part in overload resolution only when `SuccessType` can be
   * constructed from `U`, and not for an `expected`, an `unexpected`, an
   * `in_place_tag` or an `unexpect_t` (and, for a `bool` success type, not for
   * any other `expected`, which would convert to `bool` silently). `noexcept`
   * when the construction of `SuccessType` is.
   * @throws May throw if constructing `SuccessType` from `U` throws.
   */
  template <
      typename U = SuccessType,
      typename std::enable_if<
          !std::is_same<detail::remove_cvref_t<U>, expected>::value
              && !std::is_same<detail::remove_cvref_t<U>, in_place_tag>::value
              && !std::is_same<detail::remove_cvref_t<U>, unexpect_t>::value
              && !detail::is_unexpected<detail::remove_cvref_t<U>>::value
              && !(std::is_same<typename std::remove_cv<SuccessType>::type,
                                bool>::value
                   && lumex::core::utility::traits::value::is_expected<
                       detail::remove_cvref_t<U>>::value)
              && std::is_constructible<SuccessType, U>::value
              && std::is_convertible<U, SuccessType>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CXX14
  expected (U &&val)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<SuccessType, U>::value)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value))
        SuccessType (std::forward<U> (val));
  }

  /**
   * @brief Constructor from a success value (explicit).
   * @details As the implicit one, for a `U` that `SuccessType` is
   * constructible from but not convertible from.
   * @tparam U Input type `SuccessType` is constructible from.
   * @param[in] val Value stored in the `expected`.
   * @throws May throw if constructing `SuccessType` from `U` throws.
   */
  template <
      typename U = SuccessType,
      typename std::enable_if<
          !std::is_same<detail::remove_cvref_t<U>, expected>::value
              && !std::is_same<detail::remove_cvref_t<U>, in_place_tag>::value
              && !std::is_same<detail::remove_cvref_t<U>, unexpect_t>::value
              && !detail::is_unexpected<detail::remove_cvref_t<U>>::value
              && !(std::is_same<typename std::remove_cv<SuccessType>::type,
                                bool>::value
                   && lumex::core::utility::traits::value::is_expected<
                       detail::remove_cvref_t<U>>::value)
              && std::is_constructible<SuccessType, U>::value
              && !std::is_convertible<U, SuccessType>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CXX14 explicit expected (U &&val)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<SuccessType, U>::value)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value))
        SuccessType (std::forward<U> (val));
  }

  /**
   * @brief In-place constructor for the success value.
   * @details Creates an `expected` in the success state, constructing
   * `SuccessType` in place using the forwarded arguments.
   * @tparam Args Argument types forwarded to the `SuccessType` constructor.
   * @param[in] args Arguments forwarded to the `SuccessType` constructor.
   * @note Avoids extra copies or moves when creating the value. Takes part in
   * overload resolution only when `SuccessType` is constructible from `Args`;
   * `noexcept` when that construction is.
   * @throws May throw if the `SuccessType` constructor throws.
   */
  template <typename... Args,
            typename = typename std::enable_if<
                std::is_constructible<SuccessType, Args...>::value>::type>
  LUMEX_CONSTEXPR_CXX14 explicit expected (in_place_tag /* unused */,
                                           Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, Args...>::value)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value))
        SuccessType (std::forward<Args> (args)...);
  }

  /**
   * @brief In-place constructor for the success value from an initializer
   * list.
   * @tparam U Element type of the initializer list.
   * @tparam Args Argument types forwarded after the list.
   * @param[in] list Initializer list passed to the `SuccessType` constructor.
   * @param[in] args Arguments forwarded after the list.
   * @note Takes part in overload resolution only when `SuccessType` is
   * constructible from the list and `Args`.
   * @throws May throw if the `SuccessType` constructor throws.
   */
  template <
      typename U, typename... Args,
      typename = typename std::enable_if<std::is_constructible<
          SuccessType, std::initializer_list<U> &, Args...>::value>::type>
  LUMEX_CONSTEXPR_CXX14 explicit expected (in_place_tag /* unused */,
                                           std::initializer_list<U> list,
                                           Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<
              SuccessType, std::initializer_list<U> &, Args...>::value)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value))
        SuccessType (list, std::forward<Args> (args)...);
  }

  /**
   * @brief Constructs the error in place via the unexpect tag.
   * @details Puts the object in the error state and constructs E directly in
   * storage.
   * @tparam Args Argument types forwarded to the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @note Takes part in overload resolution only when `ErrorType` is
   * constructible from `Args`; `noexcept` when that construction is.
   */
  template <typename... Args,
            typename = typename std::enable_if<
                std::is_constructible<ErrorType, Args...>::value>::type>
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpect_t /*unused*/,
                                           Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, Args...>::value)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::forward<Args> (args)...);
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
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpect_t /*unused*/,
                                           std::initializer_list<U> list,
                                           Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, std::initializer_list<U> &,
                                        Args...>::value)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (list, std::forward<Args> (args)...);
  }

  /**
   * @brief Constructor from `unexpected<G>` (copy, implicit).
   * @details Creates an `expected` in the error state by copying the error
   * from `unex`, so a function that returns an `expected` can write
   * `return unexpected<E> (error);`. Implicit when `G const &` converts to
   * `ErrorType`.
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
  LUMEX_CONSTEXPR_CXX14
  expected (unexpected<G> const &unex) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<ErrorType, G const &>::value)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (unex.error ());
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
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpected<G> const &unex)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<ErrorType, G const &>::value)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (unex.error ());
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
  LUMEX_CONSTEXPR_CXX14
  expected (unexpected<G> &&unex)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::move (unex).error ());
  }

  /**
   * @brief Constructor from `unexpected<G>` (move, explicit).
   * @details As the implicit one, for a `G` that `ErrorType` is constructible
   * from but not convertible from.
   * @tparam G Error type of `unex`.
   * @param[in] unex Rvalue reference to `unexpected<G>` that holds the error.
   * @throws May throw if the move constructor of `ErrorType` throws.
   */
  template <
      typename G = ErrorType,
      typename std::enable_if<std::is_constructible<ErrorType, G>::value
                                  && !std::is_convertible<G, ErrorType>::value,
                              int>::type
      = 0>
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpected<G> &&unex)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
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
  LUMEX_CONSTEXPR_DTOR ~expected () LUMEX_NOEXCEPT { destroy_value (); }

  // ====================== Assignment Operators ====================== //

  /**
   * @brief Copy assignment operator.
   * @details Assigns another `expected` into this object.
   *          Uses copy-and-swap for the strong exception
   * guarantee).
   * @param[in] other `expected` object to assignment.
   * @return Reference to this `expected`.
   * @note Conditionally `noexcept` if move/assignment constructors and
   * operators of `SuccessType` and `ErrorType` do not throw.
   * @throws May throw if the copy constructor of `expected` or `std::swap`
   * throw.
   */
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (expected const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value
              &&std::is_nothrow_move_assignable<SuccessType>::value
                  &&std::is_nothrow_move_assignable<ErrorType>::value)
  {
    // 1. Make a temporary copy. If this throws,
    // *this stays in its original valid state.
    expected temp (other);

    // 2. Swap with the temporary. This throws only if moving or swapping
    // SuccessType or ErrorType throws.
    swap (temp);
    return *this;

    // `temp` is destroyed on return, releasing the old *this resources
  }

  /**
   * @brief Move assignment operator.
   * @details Assigns another `expected` into this object by move.
   *          Uses `swap` to exchange resources without extra allocations.
   * @param[in] other `expected` object to move.
   * @return Reference to this `expected`.
   * @note `noexcept` when moving and move-assigning `SuccessType` and
   * `ErrorType` do not throw, like `std::expected`.
   */
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (expected &&other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_assignable<SuccessType>::value
              &&std::is_nothrow_move_constructible<ErrorType>::value
                  &&std::is_nothrow_move_assignable<ErrorType>::value)
  {
    // Just swap resources. No new/delete.
    swap (other);
    return *this;
  }

  /**
   * @brief Replaces the contents with a new success value.
   * @details The value is constructed in a temporary `expected`, which is then
   * swapped in, like the copy assignment: if the construction throws, this
   * object keeps its previous contents.
   * @tparam U Type of the new value; `SuccessType` is constructible and
   * assignable from it.
   * @param[in] val The new value.
   * @return Reference to this `expected`.
   * @note Takes part in overload resolution only for a `U` that is not an
   * `expected` or an `unexpected`, like `std::expected`.
   * @throws May throw if constructing `SuccessType` from `U` throws.
   */
  template <typename U = SuccessType,
            typename std::enable_if<
                !std::is_same<expected, detail::remove_cvref_t<U>>::value
                    && !detail::is_unexpected<detail::remove_cvref_t<U>>::value
                    && std::is_constructible<SuccessType, U>::value
                    && std::is_assignable<SuccessType &, U>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (U &&val)
  {
    expected temp (in_place, std::forward<U> (val));
    swap (temp);
    return *this;
  }

  /**
   * @brief Replaces the contents with an error (copy).
   * @details The error is copied into a temporary `expected` in the error
   * state, which is then swapped in; if the copy throws, this object keeps its
   * previous contents.
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
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (unexpected<G> const &unex)
  {
    expected temp (unexpect, unex.error ());
    swap (temp);
    return *this;
  }

  /**
   * @brief Replaces the contents with an error (move).
   * @details The error is moved into a temporary `expected` in the error
   * state, which is then swapped in.
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
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (unexpected<G> &&unex)
  {
    expected temp (unexpect, std::move (unex).error ());
    swap (temp);
    return *this;
  }

  // ====================== Observers ======================

  /**
   * @brief Checks whether this `expected` holds a success value.
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
   * @brief Explicit conversion to `bool`.
   * @details Lets `expected` be used where a condition is expected (for
   * example, `if (myExpected)`); elsewhere the conversion must be written out.
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
   * @warning Calling this while `expected` is in the error state, this throws
   * `bad_expected_access<ErrorType>` specialization.
   * @return Reference to the `SuccessType` value.
   * @throws bad_expected_access<ErrorType> if the object does not hold a
   * value.
   * @note Use when you know `expected` holds a value, or are ready to handle
   * the exception. Use `[[nodiscard]]` so the returned value is handled.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType &
  value () &
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (m_storage.m_error);
    return m_storage.m_value;
  }

  /**
   * @brief Returns an rvalue reference to the success value (for moving).
   * @warning Calling this while `expected` is in the error state, this throws
   * `bad_expected_access<ErrorType>` specialization.
   * @return Rvalue reference to the `SuccessType` value.
   * @throws bad_expected_access<ErrorType> if the object does not hold a
   * value.
   * @note Intended for moving the value out of `expected`. After the call,
   * `expected` remains in a valid but unspecified state. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType &&
  value () &&
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (
          std::move (m_storage.m_error)); // Move the error into the exception
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Returns a const lvalue reference to the success value.
   * @warning Calling this while `expected` is in the error state, this throws
   * `bad_expected_access<ErrorType>` specialization.
   * @return Const reference to the `SuccessType` value.
   * @throws bad_expected_access<ErrorType> if the object does not hold a
   * value.
   * @note Use to read the value without modifying it. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType const &
  value () const &
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (m_storage.m_error);
    return m_storage.m_value;
  }

  /**
   * @brief Returns a const rvalue reference to the success value.
   * @warning Calling this while `expected` is in the error state, this throws
   * `bad_expected_access<ErrorType>` specialization.
   * @return Const rvalue reference to the `SuccessType` value.
   * @throws bad_expected_access<ErrorType> if the object does not hold a
   * value.
   * @note Intended for moving a const value out of `expected`. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType const &&
  value () const &&
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (
          std::move (m_storage.m_error)); // A const error is copied
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Reference to the `ErrorType` error.
   * @note Use when you know `expected` holds an error. Use `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &
  error () &
  {
    // Precondition: !m_has_value.
    // In std::expected, error() is an unchecked observer: calling it without
    // an error is undefined behavior, and checked access is value(), which
    // throws bad_expected_access<E>. Here the storage is a union, so reading
    // m_storage.m_error while m_has_value is true would read an inactive
    // member, which is undefined behavior in C++.
    //
    // This method checks the precondition with LUMEX_ASSERT instead of
    // throwing. LUMEX_ASSERT is active in every build, NDEBUG included: a
    // violation prints the condition, the file and the line, and aborts the
    // program before the inactive member is read. The check costs one branch
    // in every build. Because error() never throws, it stays usable where an
    // exception must not escape (destructors, swap, emergency paths);
    // checked, catchable access is value() or a prior has_value() check.
    LUMEX_ASSERT (!m_has_value
                  && "error() called on an Expected that holds a value");
    return m_storage.m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const reference to the `ErrorType` error.
   * @note Use this function to read the error without modifying it. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType const &
  error () const &
  {
    LUMEX_ASSERT (!m_has_value
                  && "error() called on an Expected that holds a value");
    return m_storage.m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Rvalue reference to the `ErrorType` error.
   * @note This function is intended to move the error out of `expected`. Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &&
  error () &&
  {
    LUMEX_ASSERT (!m_has_value
                  && "error() called on an Expected that holds a value");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const rvalue reference to the `ErrorType` error.
   * @note This function is intended to move a const error out of `expected`.
   * Use
   * `[[nodiscard]]`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType const &&
  error () const &&
  {
    LUMEX_ASSERT (!m_has_value
                  && "error() called on an Expected that holds a value");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns the stored value if present, otherwise the default value.
   * @tparam U Default-value type; must be convertible to `SuccessType`.
   * @param[in] default_value Value returned if the object does not hold a
   * success value.
   * @details Returns the stored value, or `default_value` when the object
   * holds an error.
   * @note Not declared `noexcept`: copying the stored value or converting
   * `default_value` to `SuccessType` may throw.
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
   * @details Returns the stored value moved out of `expected`, or
   * `default_value` when the object holds an error.
   * @note Not declared `noexcept`: moving the stored value or converting
   * `default_value` to `SuccessType` may throw. If `expected` holds a value,
   * it is moved. After that, `expected` remains in a valid but unspecified
   * state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U = typename std::remove_cv<SuccessType>::type>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained value or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType value_or (U &&default_value) &&
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
  LUMEX_CONSTEXPR_CXX14 ErrorType error_or (G &&default_error) &&
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
   * @note Not declared `noexcept`: copying the stored error or converting
   * `default_error` to `ErrorType` may throw.
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
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType &
      operator* ()
      & LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator* called on an Expected that holds an error");
    return m_storage.m_value;
  }

  /**
   * @brief Dereference operator (rvalue).
   * @details Returns an rvalue reference to the stored success value for
   * moving.
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Rvalue reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check. After the call `expected` remains in a valid but unspecified state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType &&
      operator* ()
      && LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator* called on an Expected that holds an error");
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Const dereference operator (lvalue).
   * @details Returns a const lvalue reference to the stored success value.
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType const &
  operator* () const &LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator* called on an Expected that holds an error");
    return m_storage.m_value;
  }

  /**
   * @brief Const dereference operator (rvalue).
   * @details Returns a const rvalue reference to the stored success value.
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const rvalue reference to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check. After the call `expected` remains in a valid but unspecified state.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType const &&
  operator* () const &&LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator* called on an Expected that holds an error");
    return std::move (m_storage.m_value);
  }

  /**
   * @brief Member-access operator (lvalue).
   * @details Returns a pointer to the stored success value.
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Pointer to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType *
  operator->() LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator-> called on an Expected that holds an error");
    return std::addressof (m_storage.m_value);
  }

  /**
   * @brief Const member-access operator (lvalue).
   * @details Returns a const pointer to the stored success value.
   * @warning Assumes `expected` holds a value. If it does not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @return Const pointer to the `SuccessType` value.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained value; should always be used.")
  LUMEX_CONSTEXPR_CXX14 SuccessType const *
  operator->() const LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (m_has_value
                  && "operator-> called on an Expected that holds an error");
    return std::addressof (m_storage.m_value);
  }

  // ====================== Modifiers ======================

  /**
   * @brief Replaces the current contents with a new `SuccessType` value.
   * @details This function constructs the new value from the forwarded
   * arguments in a temporary `expected`, then move-assigns that temporary to
   * this object, which swaps the contents; the previous value (or error) is
   * destroyed together with the temporary.
   * @tparam Args Argument types for the `SuccessType` constructor.
   * @param[in] args Arguments forwarded to the `SuccessType` constructor.
   * @return Reference to the new `SuccessType` value.
   * @note May throw if the `SuccessType` constructor throws; this object then
   * keeps its previous contents. The move assignment is `noexcept`, so an
   * exception thrown while swapping calls `std::terminate()`.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CXX14 SuccessType &
  emplace (Args &&...args)
  {
    *this = expected (in_place, std::forward<Args> (args)...);
    return m_storage.m_value;
  }

  /**
   * @brief Replaces the contents with a new `SuccessType` value built from an
   * initializer list.
   * @details As `emplace (args...)`, passing `list` before the arguments.
   * @tparam U Element type of the initializer list.
   * @tparam Args Argument types for the `SuccessType` constructor.
   * @param[in] list Initializer list passed to the `SuccessType` constructor.
   * @param[in] args Arguments forwarded after the list.
   * @return Reference to the new `SuccessType` value.
   * @note Takes part in overload resolution only when `SuccessType` is
   * constructible from the list and `Args`.
   */
  template <
      typename U, typename... Args,
      typename = typename std::enable_if<std::is_constructible<
          SuccessType, std::initializer_list<U> &, Args...>::value>::type>
  LUMEX_CONSTEXPR_CXX14 SuccessType &
  emplace (std::initializer_list<U> list, Args &&...args)
  {
    *this = expected (in_place, list, std::forward<Args> (args)...);
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
   *       the `expected` object may be left in an invalid state.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CXX14 ErrorType &
  emplace_error (Args &&...args)
  {
    destroy_value (); // Destroy the current active member
    new (std::addressof (m_storage.m_error)) ErrorType (
        std::forward<Args> (args)...); // Construct the error in place
    m_has_value = false;               // Set the error state
    return m_storage.m_error;
  }

  /**
   * @brief Exchanges contents with another `expected` object.
   * @details Swaps the `m_has_value` flag and, as needed, the contents (value
   * or error) with another `expected`. If both objects hold values (or both
   * hold errors), `std::swap` is used. If one holds a value and the other an
   * error, contents are moved so both objects change state.
   * @param[in,out] other The other `expected` object to swap with.
   * @note The noexcept guarantee depends on
   * `std::is_nothrow_move_constructible` and on whether swapping two
   * `SuccessType` and two `ErrorType` objects is `noexcept` (a `swap` found by
   * argument-dependent lookup, or `std::swap`), in every standard.
   * @throws May throw if move constructors or `std::swap` of
   *         `SuccessType` or `ErrorType` throw.
   */
  LUMEX_CONSTEXPR_CXX14 void
  swap (expected &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value
              &&detail::is_nothrow_swappable<SuccessType>::value
                  &&detail::is_nothrow_swappable<ErrorType>::value)
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
      { // One holds a value, the other an error. Move is required.
        if (m_has_value)
          {
            ErrorType temp_error (std::move (other.m_storage.m_error));
            other.m_storage.m_error.~ErrorType ();
            new (detail::voidify (other.m_storage.m_value))
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
            new (detail::voidify (m_storage.m_value))
                SuccessType (std::move (temp_value));
          }
        std::swap (m_has_value, other.m_has_value);
      }
  }

  // ====================== Monadic Operations ======================
  // Each operation exists for the four value categories of the object. The
  // function is taken as a forwarding reference and called as by std::invoke,
  // the constraint is written once in detail:: and used as a requires-clause
  // from C++20 and as std::enable_if before.

  /**
   * @brief Calls `func` with the contained value (lvalue) if present, and
   * returns its `expected`.
   * @details If `expected` holds a value, `func` is called with an lvalue
   * reference to it and its result is returned. If `expected` holds an error,
   * `func` is not called and an `expected` of the result type that holds the
   * current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function works too).
   * @param[in] func Function that takes the value and returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the value
   * and returns an `expected`, and `ErrorType` can be copied or moved the way
   * the lvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType, ErrorType &,
                                   SuccessType &>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_and_then<
          FunctionType, ErrorType, ErrorType &, SuccessType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func)
      & -> detail::result_clean_t<FunctionType, SuccessType &>
  {
    using ResultType = detail::result_clean_t<FunctionType, SuccessType &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    if (m_has_value)
      return detail::invoke_call (std::forward<FunctionType> (func),
                                  m_storage.m_value);
    return ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained value (const lvalue) if present,
   * and returns its `expected`.
   * @details If `expected` holds a value, `func` is called with a const lvalue
   * reference to it and its result is returned. If `expected` holds an error,
   * `func` is not called and an `expected` of the result type that holds the
   * current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function works too).
   * @param[in] func Function that takes the value and returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the value
   * and returns an `expected`, and `ErrorType` can be copied or moved the way
   * the const lvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType, ErrorType const &,
                                   SuccessType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType const &,
                SuccessType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func)
      const & -> detail::result_clean_t<FunctionType, SuccessType const &>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, SuccessType const &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    if (m_has_value)
      return detail::invoke_call (std::forward<FunctionType> (func),
                                  m_storage.m_value);
    return ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained value (rvalue) if present, and
   * returns its `expected`.
   * @details If `expected` holds a value, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and its result is
   * returned. If `expected` holds an error, `func` is not called and an
   * `expected` of the result type that holds the current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function works too).
   * @param[in] func Function that takes the value and returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the value
   * and returns an `expected`, and `ErrorType` can be copied or moved the way
   * the rvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType, ErrorType &&,
                                   SuccessType &&>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_and_then<
          FunctionType, ErrorType, ErrorType &&, SuccessType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func)
      && -> detail::result_clean_t<FunctionType, SuccessType &&>
  {
    using ResultType = detail::result_clean_t<FunctionType, SuccessType &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    if (m_has_value)
      return detail::invoke_call (std::forward<FunctionType> (func),
                                  std::move (m_storage.m_value));
    return ResultType (unexpect, std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained value (const rvalue) if present,
   * and returns its `expected`.
   * @details If `expected` holds a value, `func` is called with a const rvalue
   * reference to it and its result is returned. If `expected` holds an error,
   * `func` is not called and an `expected` of the result type that holds the
   * current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function works too).
   * @param[in] func Function that takes the value and returns an `expected`
   * with the same error type as this object.
   * @return The result of `func` or the current error, as an
   * `expected<U, ErrorType>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the value
   * and returns an `expected`, and `ErrorType` can be copied or moved the way
   * the const rvalue call needs; an `expected` with another error type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the error
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_and_then<FunctionType, ErrorType, ErrorType const &&,
                                   SuccessType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_and_then<
                FunctionType, ErrorType, ErrorType const &&,
                SuccessType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType &&func)
      const && -> detail::result_clean_t<FunctionType, SuccessType const &&>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, SuccessType const &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    if (m_has_value)
      return detail::invoke_call (std::forward<FunctionType> (func),
                                  std::move (m_storage.m_value));
    return ResultType (unexpect, std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (lvalue) if present, and
   * returns its `expected`.
   * @details If `expected` holds an error, `func` is called with an lvalue
   * reference to it and its result is returned. If `expected` holds a value,
   * `func` is not called and an `expected` of the result type that holds the
   * current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<SuccessType, G>`; the error type `G` may differ from
   * `ErrorType`.
   * @return The result of `func` or the current value, as an
   * `expected<SuccessType, G>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`, and `SuccessType` can be copied or moved the
   * way the lvalue call needs; an `expected` with another value type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the value
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_or_else<FunctionType, SuccessType, SuccessType &,
                                  ErrorType &>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_or_else<
          FunctionType, SuccessType, SuccessType &, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      & -> detail::result_clean_t<FunctionType, ErrorType &>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    if (m_has_value)
      return ResultType (in_place, m_storage.m_value);
    return detail::invoke_call (std::forward<FunctionType> (func),
                                m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (const lvalue) if present,
   * and returns its `expected`.
   * @details If `expected` holds an error, `func` is called with a const
   * lvalue reference to it and its result is returned. If `expected` holds a
   * value, `func` is not called and an `expected` of the result type that
   * holds the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<SuccessType, G>`; the error type `G` may differ from
   * `ErrorType`.
   * @return The result of `func` or the current value, as an
   * `expected<SuccessType, G>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`, and `SuccessType` can be copied or moved the
   * way the const lvalue call needs; an `expected` with another value type
   * fails a `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the value
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_or_else<FunctionType, SuccessType, SuccessType const &,
                            ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, SuccessType, SuccessType const &,
                ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      const & -> detail::result_clean_t<FunctionType, ErrorType const &>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType const &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    if (m_has_value)
      return ResultType (in_place, m_storage.m_value);
    return detail::invoke_call (std::forward<FunctionType> (func),
                                m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (rvalue) if present, and
   * returns its `expected`.
   * @details If `expected` holds an error, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and its result is
   * returned. If `expected` holds a value, `func` is not called and an
   * `expected` of the result type that holds the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<SuccessType, G>`; the error type `G` may differ from
   * `ErrorType`.
   * @return The result of `func` or the current value, as an
   * `expected<SuccessType, G>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`, and `SuccessType` can be copied or moved the
   * way the rvalue call needs; an `expected` with another value type fails a
   * `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the value
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_or_else<FunctionType, SuccessType, SuccessType &&,
                                  ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<
                detail::can_or_else<FunctionType, SuccessType, SuccessType &&,
                                    ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      && -> detail::result_clean_t<FunctionType, ErrorType &&>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    if (m_has_value)
      return ResultType (in_place, std::move (m_storage.m_value));
    return detail::invoke_call (std::forward<FunctionType> (func),
                                std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (const rvalue) if present,
   * and returns its `expected`.
   * @details If `expected` holds an error, `func` is called with a const
   * rvalue reference to it and its result is returned. If `expected` holds a
   * value, `func` is not called and an `expected` of the result type that
   * holds the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns an
   * `expected<SuccessType, G>`; the error type `G` may differ from
   * `ErrorType`.
   * @return The result of `func` or the current value, as an
   * `expected<SuccessType, G>` without `const`, `volatile` and reference.
   * @note The overload exists only when `func` can be called with the error
   * and returns an `expected`, and `SuccessType` can be copied or moved the
   * way the const rvalue call needs; an `expected` with another value type
   * fails a `static_assert`.
   * @throws May throw if `func` throws or if copying or moving the value
   * throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_or_else<FunctionType, SuccessType, SuccessType const &&,
                            ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_or_else<
                FunctionType, SuccessType, SuccessType const &&,
                ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType &&func)
      const && -> detail::result_clean_t<FunctionType, ErrorType const &&>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, ErrorType const &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    if (m_has_value)
      return ResultType (in_place, std::move (m_storage.m_value));
    return detail::invoke_call (std::forward<FunctionType> (func),
                                std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained value (lvalue) if present, and
   * wraps the result in an `expected`.
   * @details If `expected` holds a value, `func` is called with an lvalue
   * reference to it and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If
   * `expected` holds an error, `func` is not called and an `expected` holding
   * the current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function or to a data member works
   * too).
   * @param[in] func Function that takes the value; its result may be any
   * type: `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the value
   * and `ErrorType` can be copied or moved the way the lvalue call needs. A
   * result type that `expected` cannot hold (a reference, an array,
   * `in_place_tag`, `unexpect_t`, an `unexpected`) fails the `static_assert`
   * of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform<FunctionType, ErrorType, ErrorType &,
                                    SuccessType &>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_transform<
          FunctionType, ErrorType, ErrorType &, SuccessType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func) & -> expected<
      detail::result_xform_t<FunctionType, SuccessType &>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType &>,
                   ErrorType>;
    if (m_has_value)
      return ResultType (detail::invoke_value_tag (),
                         std::forward<FunctionType> (func), m_storage.m_value);
    return ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained value (const lvalue) if present,
   * and wraps the result in an `expected`.
   * @details If `expected` holds a value, `func` is called with a const lvalue
   * reference to it and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If
   * `expected` holds an error, `func` is not called and an `expected` holding
   * the current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function or to a data member works
   * too).
   * @param[in] func Function that takes the value; its result may be any
   * type: `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the value
   * and `ErrorType` can be copied or moved the way the const lvalue call
   * needs. A result type that `expected` cannot hold (a reference, an array,
   * `in_place_tag`, `unexpect_t`, an `unexpected`) fails the `static_assert`
   * of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform<FunctionType, ErrorType, ErrorType const &,
                                    SuccessType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType const &,
                SuccessType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func) const & -> expected<
      detail::result_xform_t<FunctionType, SuccessType const &>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType const &>,
                   ErrorType>;
    if (m_has_value)
      return ResultType (detail::invoke_value_tag (),
                         std::forward<FunctionType> (func), m_storage.m_value);
    return ResultType (unexpect, m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained value (rvalue) if present, and
   * wraps the result in an `expected`.
   * @details If `expected` holds a value, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and an `expected` holding
   * its result is returned; a `void` result gives an `expected<void,
   * ErrorType>` in the success state. If `expected` holds an error, `func` is
   * not called and an `expected` holding the current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function or to a data member works
   * too).
   * @param[in] func Function that takes the value; its result may be any
   * type: `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the value
   * and `ErrorType` can be copied or moved the way the rvalue call needs. A
   * result type that `expected` cannot hold (a reference, an array,
   * `in_place_tag`, `unexpect_t`, an `unexpected`) fails the `static_assert`
   * of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform<FunctionType, ErrorType, ErrorType &&,
                                    SuccessType &&>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_transform<
          FunctionType, ErrorType, ErrorType &&, SuccessType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func) && -> expected<
      detail::result_xform_t<FunctionType, SuccessType &&>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType &&>,
                   ErrorType>;
    if (m_has_value)
      return ResultType (detail::invoke_value_tag (),
                         std::forward<FunctionType> (func),
                         std::move (m_storage.m_value));
    return ResultType (unexpect, std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained value (const rvalue) if present,
   * and wraps the result in an `expected`.
   * @details If `expected` holds a value, `func` is called with a const rvalue
   * reference to it and an `expected` holding its result is returned; a `void`
   * result gives an `expected<void, ErrorType>` in the success state. If
   * `expected` holds an error, `func` is not called and an `expected` holding
   * the current error is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke` (a pointer to a member function or to a data member works
   * too).
   * @param[in] func Function that takes the value; its result may be any
   * type: `void`, a value, or an `expected` (which gives an `expected` of an
   * `expected`, as in `std::expected`).
   * @return `expected<U, ErrorType>` where `U` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the value
   * and `ErrorType` can be copied or moved the way the const rvalue call
   * needs. A result type that `expected` cannot hold (a reference, an array,
   * `in_place_tag`, `unexpect_t`, an `unexpected`) fails the `static_assert`
   * of `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (
        detail::can_transform<FunctionType, ErrorType, ErrorType const &&,
                              SuccessType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform<
                FunctionType, ErrorType, ErrorType const &&,
                SuccessType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType &&func) const && -> expected<
      detail::result_xform_t<FunctionType, SuccessType const &&>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType const &&>,
                   ErrorType>;
    if (m_has_value)
      return ResultType (detail::invoke_value_tag (),
                         std::forward<FunctionType> (func),
                         std::move (m_storage.m_value));
    return ResultType (unexpect, std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (lvalue) if present, and
   * wraps the result in the error of a new `expected`.
   * @details If `expected` holds an error, `func` is called with an lvalue
   * reference to it and an `expected<SuccessType, G>` holding its result as
   * the error is returned. If `expected` holds a value, `func` is not called
   * and an `expected` holding the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<SuccessType, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`, and `SuccessType` can be copied or moved the
   * way the lvalue call needs. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, SuccessType,
                                          SuccessType &, ErrorType &>::value)
#else
  template <
      typename FunctionType,
      typename = typename std::enable_if<detail::can_transform_error<
          FunctionType, SuccessType, SuccessType &, ErrorType &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func) & -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType &>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType &>>;
    if (m_has_value)
      return ResultType (in_place, m_storage.m_value);
    return ResultType (detail::invoke_error_tag (),
                       std::forward<FunctionType> (func), m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (const lvalue) if present,
   * and wraps the result in the error of a new `expected`.
   * @details If `expected` holds an error, `func` is called with a const
   * lvalue reference to it and an `expected<SuccessType, G>` holding its
   * result as the error is returned. If `expected` holds a value, `func` is
   * not called and an `expected` holding the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<SuccessType, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`, and `SuccessType` can be copied or moved the
   * way the const lvalue call needs. An error type that `expected` cannot hold
   * (a reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, SuccessType,
                                          SuccessType const &,
                                          ErrorType const &>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, SuccessType, SuccessType const &,
                ErrorType const &>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func) const & -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType const &>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType const &>>;
    if (m_has_value)
      return ResultType (in_place, m_storage.m_value);
    return ResultType (detail::invoke_error_tag (),
                       std::forward<FunctionType> (func), m_storage.m_error);
  }

  /**
   * @brief Calls `func` with the contained error (rvalue) if present, and
   * wraps the result in the error of a new `expected`.
   * @details If `expected` holds an error, `func` is called with an rvalue
   * reference to it (so the callee can move from it) and an
   * `expected<SuccessType, G>` holding its result as the error is returned. If
   * `expected` holds a value, `func` is not called and an `expected` holding
   * the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<SuccessType, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`, and `SuccessType` can be copied or moved the
   * way the rvalue call needs. An error type that `expected` cannot hold (a
   * reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, SuccessType,
                                          SuccessType &&, ErrorType &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, SuccessType, SuccessType &&,
                ErrorType &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func) && -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType &&>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType &&>>;
    if (m_has_value)
      return ResultType (in_place, std::move (m_storage.m_value));
    return ResultType (detail::invoke_error_tag (),
                       std::forward<FunctionType> (func),
                       std::move (m_storage.m_error));
  }

  /**
   * @brief Calls `func` with the contained error (const rvalue) if present,
   * and wraps the result in the error of a new `expected`.
   * @details If `expected` holds an error, `func` is called with a const
   * rvalue reference to it and an `expected<SuccessType, G>` holding its
   * result as the error is returned. If `expected` holds a value, `func` is
   * not called and an `expected` holding the current value is returned.
   * @tparam FunctionType Type of `func`, forwarded and called as by
   * `std::invoke`.
   * @param[in] func Function that takes the error and returns the new error
   * (any type except `void`; an `expected` is allowed).
   * @return `expected<SuccessType, G>` where `G` is the result type of `func`
   * without `const` and `volatile`.
   * @note The overload exists only when `func` can be called with the error
   * and does not return `void`, and `SuccessType` can be copied or moved the
   * way the const rvalue call needs. An error type that `expected` cannot hold
   * (a reference, an array, an `unexpected`) fails the `static_assert` of
   * `expected`.
   * @throws May throw if `func` throws or if constructing the result throws.
   */
#if LUMEX_HAS_CONCEPTS
  template <typename FunctionType>
    requires (detail::can_transform_error<FunctionType, SuccessType,
                                          SuccessType const &&,
                                          ErrorType const &&>::value)
#else
  template <typename FunctionType,
            typename = typename std::enable_if<detail::can_transform_error<
                FunctionType, SuccessType, SuccessType const &&,
                ErrorType const &&>::value>::type>
#endif
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType &&func) const && -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType const &&>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType const &&>>;
    if (m_has_value)
      return ResultType (in_place, std::move (m_storage.m_value));
    return ResultType (detail::invoke_error_tag (),
                       std::forward<FunctionType> (func),
                       std::move (m_storage.m_error));
  }

private:
  template <typename, typename> friend class expected;

  /**
   * @brief Builds a success value from the result of a call.
   * @details Used by the monadic operations: the stored value is initialized
   * with `detail::invoke_call (fn, args...)` itself, so the result is
   * constructed in place.
   * @tparam Fn Type of the function.
   * @tparam Args Types of its arguments.
   * @param[in] fn The function.
   * @param[in] args Arguments of the call.
   */
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CXX14 explicit expected (detail::invoke_value_tag /*unused*/,
                                           Fn &&fn, Args &&...args)
      : m_storage (), m_has_value (true)
  {
    new (detail::voidify (m_storage.m_value))
        SuccessType (detail::invoke_call (std::forward<Fn> (fn),
                                          std::forward<Args> (args)...));
  }

  /**
   * @brief Builds an error from the result of a call.
   * @details Counterpart of the constructor above for `transform_error`.
   * @tparam Fn Type of the function.
   * @tparam Args Types of its arguments.
   * @param[in] fn The function.
   * @param[in] args Arguments of the call.
   */
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CXX14 explicit expected (detail::invoke_error_tag /*unused*/,
                                           Fn &&fn, Args &&...args)
      : m_storage (), m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (detail::invoke_call (
        std::forward<Fn> (fn), std::forward<Args> (args)...));
  }

  /**
   * @brief Union that stores either a success value or an error value.
   * @details Used to save memory, because an `expected` object at any time
   *          holds only one of the two: either `SuccessType` or `ErrorType`.
   *          Union-member lifetimes are managed manually.
   * @note Copy/move constructors and assignment operators are deleted,
   *       because member lifetimes are managed by `expected`.
   */
  union Storage
  {
    /**
     * @brief Stored success value.
     * @details Active when `expected` is in the success state (`m_has_value ==
     * true`).
     */
    SuccessType m_value;

    /**
     * @brief Stored error value.
     * @details Active when `expected` is in the error state (`m_has_value ==
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
   * @details This member is central to `expected` because it holds the actual
   * data. Access `m_value` or `m_error` only after checking `m_has_value`.
   */
  Storage m_storage;
  /**
   * @brief Flag: `true` if `expected` holds a success value, `false` if it
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
   * management correct when the `expected` destructor runs or the object
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

// The specialization for a `void` success value is defined in
// ExpectedVoid.hpp; declaring it here keeps `transform` of an `expected<T, E>`
// with a function that returns `void` from naming the primary template for
// `void`.
template <typename ErrorType> class expected<void, ErrorType>;

// ====================== Non-member functions ======================

/**
 * @brief Compares two `expected<SuccessType, ErrorType>` objects for equality.
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
LUMEX_CONSTEXPR_CXX14 bool
operator== (expected<SuccessType, ErrorType> const &lhs,
            expected<SuccessType, ErrorType> const &rhs)
{
  if (lhs.has_value () != rhs.has_value ())
    return false;
  if (lhs.has_value ())
    return *lhs == *rhs;
  return lhs.error () == rhs.error ();
}

/**
 * @brief Compares two `expected<void, ErrorType>` objects for equality.
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
LUMEX_CONSTEXPR_CXX14 bool
operator== (expected<void, ErrorType> const &lhs,
            expected<void, ErrorType> const &rhs)
{
  if (lhs.has_value () != rhs.has_value ())
    return false;
  if (lhs.has_value ())
    return true; // Both are void success
  return lhs.error () == rhs.error ();
}

/**
 * @brief Exchanges the contents of two `expected<SuccessType, ErrorType>`
 * objects.
 * @details Calls `lhs.swap(rhs)`, delegating to the class implementation.
 *
 * @tparam SuccessType Success value type.
 * @tparam ErrorType   Error type.
 * @param[in,out] lhs Left-hand operand of the swap.
 * @param[in,out] rhs Right-hand operand of the swap.
 *
 * @note Marked `noexcept` unconditionally via `LUMEX_NOEXCEPT`, unlike the
 * member `swap`, whose `noexcept` depends on `SuccessType` and `ErrorType`.
 * An exception thrown by the member `swap` therefore calls
 * `std::terminate()`.
 * @par Thread safety
 *      Not thread-safe for the same objects without external synchronization.
 * @par Performance
 *      Swap is typically O(1) and does not allocate.
 */
template <typename SuccessType, typename ErrorType>
LUMEX_CONSTEXPR_CXX14 void
swap (expected<SuccessType, ErrorType> &lhs,
      expected<SuccessType, ErrorType> &rhs) LUMEX_NOEXCEPT
{
  lhs.swap (rhs);
}

/**
 * @brief Exchanges the contents of two `expected<void, ErrorType>` objects.
 * @details Delegates to the matching class specialization `swap`.
 *
 * @tparam ErrorType Error type.
 * @param[in,out] lhs Left-hand operand of the swap.
 * @param[in,out] rhs Right-hand operand of the swap.
 *
 * @note Marked `noexcept` unconditionally via `LUMEX_NOEXCEPT`, unlike the
 * member `swap`, whose `noexcept` depends on `ErrorType`. An exception thrown
 * by the member `swap` therefore calls `std::terminate()`.
 * @par Thread safety
 *      Not thread-safe without external synchronization on the same instances.
 */
template <typename ErrorType>
LUMEX_CONSTEXPR_CXX14 void
swap (expected<void, ErrorType> &lhs,
      expected<void, ErrorType> &rhs) LUMEX_NOEXCEPT
{
  lhs.swap (rhs);
}

// ====================== Helper functions make_expected/make_unexpected
// ======================

/**
 * @brief Creates a successful `expected<T, ErrorType>` from a value.
 * @details Constructs the success state in place, avoiding extra copies/moves.
 *
 * @tparam ErrorType Error type.
 * @tparam U_val     Input-value type; after `std::decay`, `T =
 * std::decay_t<U_val>`.
 * @param[in] val    Value used to initialize the success result.
 * @return `expected<std::decay_t<U_val>, ErrorType>` in the success state.
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
LUMEX_CONSTEXPR_FUNCTION expected<typename std::decay<U_val>::type,
                                  ErrorType> make_expected (U_val &&val)
{
  return expected<typename std::decay<U_val>::type, ErrorType> (
      in_place, std::forward<U_val> (val));
}

/**
 * @brief Creates a successful `expected<void, ErrorType>` specialization.
 * @details Returns an object in the success state with no value.
 *
 * @tparam ErrorType Error type.
 * @return `expected<void, ErrorType>` in the success state.
 *
 * @note Marked `[[nodiscard]]` (via macro). Useful for APIs where success
 * itself matters.
 * @par Exception guarantees
 *      Does not throw if constructing `expected<void, ErrorType>` does not
 * throw.
 */
template <typename ErrorType>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an expected result; should always be used.")
LUMEX_CONSTEXPR_FUNCTION expected<void, ErrorType> make_expected ()
{
  return expected<void, ErrorType> (in_place);
}

/**
 * @brief Creates an error `expected<SuccessType, E>` from an error value.
 * @details Wraps the given error in `unexpected<E>` and returns the matching
 * `expected` in the error state.
 *
 * @tparam SuccessType Success-value type (parameterizes the returned
 * `expected`).
 * @tparam U_err       Input error type; the resulting error type is `E =
 * std::decay_t<U_err>`.
 * @param[in] err      Error object stored (copied or moved) inside `expected`.
 * @return `expected<SuccessType, std::decay_t<U_err>>` in the error state.
 *
 * @note Marked `[[nodiscard]]` (via macro).
 * @par Exception guarantees
 *      May throw if copy/move of `E` throws.
 * @par Thread safety
 *      Thread safety depends on the properties of error type `E`.
 * @deprecated Use make_unexpected<E>(...) returning unexpected<E> and
 * expected(unexpect_t, ...) instead of this function.
 */
template <typename SuccessType, typename U_err>
LUMEX_ATTRIBUTE_DEPRECATED_MSG (
    "Use make_unexpected<E>(...) returning Unexpected<E> and "
    "Expected(unexpect_t, ...) instead.")
LUMEX_CONSTEXPR_FUNCTION
    expected<SuccessType, typename std::decay<U_err>::type> make_unexpected (
        U_err &&err)
{
  return expected<SuccessType, typename std::decay<U_err>::type> (
      unexpected<typename std::decay<U_err>::type> (
          std::forward<U_err> (err)));
}

/**
 * @brief Creates an error `expected<void, ErrorType>` from an error value.
 * @details Wraps the given error in `unexpected<ErrorType>` and returns
 * `expected<void, ErrorType>` in the error state.
 *
 * @tparam ErrorType Error type.
 * @param[in] err Error object (copied or moved).
 * @return `expected<void, ErrorType>` in the error state.
 *
 * @note Marked `[[nodiscard]]` (via macro).
 * @par Exception guarantees
 *      May throw if copy/move of `ErrorType` throws.
 * @par Thread safety
 *      Not thread-safe if the returned object is shared without
 * synchronization.
 * @deprecated Use make_unexpected<E>(...) returning unexpected<E> and
 * expected(unexpect_t, ...) instead of this function.
 */
template <typename ErrorType>
LUMEX_ATTRIBUTE_DEPRECATED_MSG (
    "Use make_unexpected<E>(...) returning Unexpected<E> and "
    "Expected(unexpect_t, ...) instead.")
LUMEX_CONSTEXPR_FUNCTION expected<void, ErrorType> make_unexpected (
    ErrorType &&err)
{
  return expected<void, ErrorType> (
      unexpected<ErrorType> (std::forward<ErrorType> (err)));
}

/**
 * @brief Standard error factory: creates unexpected<E> in place.
 */
template <typename E, typename... Args>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an unexpected value; should always be used.")
LUMEX_CONSTEXPR_FUNCTION
    unexpected<typename std::decay<E>::type> make_unexpected (Args &&...args)
{
  using Err = typename std::decay<E>::type;
  return unexpected<Err> (Err (std::forward<Args> (args)...));
}

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::result::expected;
using lumex::core::expected::result::make_unexpected;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_HPP

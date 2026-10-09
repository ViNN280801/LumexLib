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
 * @brief `expected<SuccessType, ErrorType>`, an implementation of C++23
 * `std::expected` usable from C++11, with its non-member functions and
 * factories.
 * @details The class holds either a value or an error and follows
 * [expected.expected] of the working draft (C++26, which includes the
 * resolutions of LWG 3687, 3703, 3754, 3836, 3843, 3866, 3877, 3886, 3891,
 * 3938, 3940, 3951, 3973, 4025, 4026, 4031, 4141, 4222 and 4366): the
 * constructors (from a value, from `unexpected`, in place, from an
 * initializer list, and converting from an `expected` of other types), the
 * assignment from a value or an `unexpected`, the observers (`has_value()`,
 * `has_error()`, `value()`, which throws `bad_expected_access`, `error()`,
 * `value_or()`, `error_or()`, `operator*`), `emplace()`, `swap()` and the
 * monadic operations `and_then()`, `transform()`, `or_else()` and
 * `transform_error()`, which take the function as a forwarding reference, call
 * it as `std::invoke` does, and are constrained with concepts from C++20 and
 * with SFINAE before. The header also declares the non-member `operator==`
 * (with another `expected`, a value or an `unexpected`; `!=` and the reversed
 * forms before C++20) and `swap()`, `make_expected()` and
 * `make_unexpected<E>()`, which returns an `unexpected<E>`. The specialization
 * for a `void` value is in `ExpectedVoid.hpp`; the storage and the layers
 * that give the class its special member functions are in
 * `ExpectedStorage.hpp`. Header-only, part of `lumex::expected`; `expected`
 * and `make_unexpected` are also visible at global scope, `unexpected` is not.
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

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#include "ExpectedDetail.hpp"
#include "ExpectedStorage.hpp"
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
 * @brief Implementation of C++23 `std::expected<T, E>` for C++11 and newer.
 * @see https://en.cppreference.com/w/cpp/utility/expected
 * @see https://eel.is/c++draft/expected.expected
 * @details Holds either a value of type SuccessType, or an error of type
 * ErrorType, in one union, with no allocation ([expected.object.general]/1).
 * Functions can return either a success result, or error information without
 * exceptions for expected failures.
 * @tparam SuccessType Success value type.
 * @tparam ErrorType Error type.
 * @note This implementation is not thread-safe by default. Accessing
 * `expected` from several threads without external synchronization is
 * undefined behavior if `SuccessType` or `ErrorType` are not thread-safe.
 * @par Special member functions
 * The copy and move constructors, the assignments and the destructor are the
 * ones the standard describes, built from a chain of base classes (see
 * `ExpectedStorage.hpp`): the copy constructor and the copy assignment are
 * deleted unless `SuccessType` and `ErrorType` can be copied
 * ([expected.object.cons]/9, [expected.object.assign]/4), the move
 * constructor and the move assignment take part in overload resolution only if
 * they can be moved ([expected.object.cons]/11, [expected.object.assign]/6),
 * and each of the five is trivial when the standard says it is ([expected.
 * object.cons]/10 and /16, [expected.object.dtor]/2,
 * [expected.object.assign]/5 and /10). So `std::is_copy_constructible`,
 * `std::is_trivially_copyable` and the other traits of `expected<T, E>` give
 * what they give for `std::expected`. The assignments build the new contents
 * the way `reinit-expected` of [expected.object.assign]/1 does: no temporary
 * `expected`, the strong guarantee where `SuccessType` or `ErrorType` can be
 * moved without throwing.
 * @par constexpr
 * Every member that the standard declares `constexpr` is `constexpr` as far
 * as the language of the standard in use allows:
 * - C++11: the constructors from a value, `in_place_tag`, `unexpect_t` and
 *   `unexpected`, the default constructor, and the copy and move constructors
 *   and the destructor when `SuccessType` and `ErrorType` are trivially
 *   copyable and destructible; `has_value()`, `has_error()`, `operator bool`,
 *   the observers on a const object (`operator*`, `operator->`, `value()`,
 *   `error()`, `value_or()`, `error_or()`), the four monadic operations on a
 *   const object, `==` and `!=`. A C++11 `constexpr` member function is
 *   implicitly const, so no member that is not const can be `constexpr`.
 * - C++14: also the observers and monadic operations on an object that is not
 *   const (a `constexpr` function may have several statements and change an
 *   object, N3652; they are marked with `LUMEX_CONSTEXPR_CXX14`).
 * - C++17: also the monadic operations with a lambda (constexpr lambdas,
 * P0170).
 * - C++20 and C++23: also the operations that change the active alternative of
 *   the union: the copy and move constructors, the converting constructors
 *   from another `expected` and the destructor for types that are not
 *   trivial, the assignments, `emplace()`, `emplace_error()` and `swap()`
 *   (a union member may become active in a constant expression, P1330;
 *   `std::construct_at`, P0784; try blocks, P1002; marked with
 *   `LUMEX_EXPECTED_CONSTEXPR_CXX20`).
 * In every standard the types must themselves be usable in a constant
 * expression (literal; a non-trivial destructor of `SuccessType` or
 * `ErrorType` needs C++20).
 * @par Differences from std::expected
 * - `error()`, `operator*` and `operator->` check their precondition with
 *   `LUMEX_ASSERT`, which aborts in every build including `NDEBUG`, where the
 *   standard leaves the call undefined (the "hardened preconditions" of the
 *   draft, always on).
 * - The tags are `in_place_tag` / `in_place` and `unexpect_t` / `unexpect` of
 *   this module, not `std::in_place_t` and `std::unexpect_t`, so that the
 *   class works from C++11.
 * - Constructors and `value_or`, `error_or` that the standard declares
 *   without `noexcept` are `noexcept` when the construction they do cannot
 *   throw ([res.on.exception.handling]/5 lets an implementation strengthen an
 *   exception specification).
 * - `rebind` is as in the standard; `emplace_error()`, `Unit`,
 *   `make_expected()` and the `success()` / `failure()` markers are
 *   additions.
 */
template <typename SuccessType, typename ErrorType>
class expected : private detail::storage::expected_base<SuccessType, ErrorType>
{
  using base_type = detail::storage::expected_base<SuccessType, ErrorType>;
  using traits_type = detail::storage::expected_traits<SuccessType, ErrorType>;

public:
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
  // The copy and the move constructor are the implicit ones: they come from
  // the bases and are deleted, left out or trivial as the standard says.

  /**
   * @brief Default constructor.
   * @details Creates an `expected` in the success state holding the value
   * `SuccessType()`, value-initialized ([expected.object.cons]/2).
   * @note Takes part in overload resolution only when `SuccessType` is
   * default-constructible, so `std::is_default_constructible` tells the
   * truth, like `std::expected`. `noexcept` when the default constructor of
   * `SuccessType` is.
   * @throws May throw if the default constructor of `SuccessType` throws.
   */
  template <typename U = SuccessType,
            typename = typename std::enable_if<
                std::is_default_constructible<U>::value>::type>
  LUMEX_CONSTEXPR_CTOR
  expected () LUMEX_NOEXCEPT_IF (
      std::is_nothrow_default_constructible<SuccessType>::value)
      : base_type (detail::value_tag ())
  {
  }

  /**
   * @brief Converting constructor from an `expected<U, G>` (copy, implicit).
   * @details Creates an `expected` with the state of `other`, copying the
   * value or the error into one of this type. Implicit when both conversions
   * `U const &` to `SuccessType` and `G const &` to `ErrorType` are
   * ([expected.object.cons]/17 to /22).
   * @tparam U Success type of `other`.
   * @tparam G Error type of `other`.
   * @param[in] other `expected` to convert.
   * @note Takes part in overload resolution only when `SuccessType` and
   * `ErrorType` can be constructed from the other types and `other` cannot be
   * converted as a whole (the value constructor then does it), like
   * `std::expected`. For a `bool` success type that check is skipped:
   * `expected<bool, E>` built from an `expected<int, E>` converts the value
   * inside, it does not collapse the state to `true` or `false` (LWG 3836).
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
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected (expected<U, G> const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<SuccessType, U const &>::value
          &&std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::convert_tag (), other)
  {
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
  LUMEX_EXPECTED_CONSTEXPR_CXX20 explicit expected (
      expected<U, G> const &other)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, U const &>::value
              &&std::is_nothrow_constructible<ErrorType, G const &>::value)
      : base_type (detail::convert_tag (), other)
  {
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
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected (expected<U, G> &&other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_constructible<SuccessType, U>::value
          &&std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::convert_tag (), detail::mv (other))
  {
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
  LUMEX_EXPECTED_CONSTEXPR_CXX20 explicit expected (expected<U, G> &&other)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, U>::value
              &&std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::convert_tag (), detail::mv (other))
  {
  }

  /**
   * @brief Constructor from a success value (implicit conversion).
   * @details Creates an `expected` in the success state holding the value
   * `val`. Implicit when `U` converts to `SuccessType`
   * ([expected.object.cons]/23 to /26).
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
      typename U = typename std::remove_cv<SuccessType>::type,
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
  LUMEX_CONSTEXPR_CTOR
  expected (U &&val)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<SuccessType, U>::value)
      : base_type (detail::value_tag (), detail::fwd<U> (val))
  {
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
      typename U = typename std::remove_cv<SuccessType>::type,
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
  LUMEX_CONSTEXPR_CTOR explicit expected (U &&val)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<SuccessType, U>::value)
      : base_type (detail::value_tag (), detail::fwd<U> (val))
  {
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
  LUMEX_CONSTEXPR_CTOR explicit expected (in_place_tag /* unused */,
                                          Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<SuccessType, Args...>::value)
      : base_type (detail::value_tag (), detail::fwd<Args> (args)...)
  {
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
  LUMEX_CONSTEXPR_CTOR explicit expected (in_place_tag /* unused */,
                                          std::initializer_list<U> list,
                                          Args &&...args)
      LUMEX_NOEXCEPT_IF (
          std::is_nothrow_constructible<
              SuccessType, std::initializer_list<U> &, Args...>::value)
      : base_type (detail::value_tag (), list, detail::fwd<Args> (args)...)
  {
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
   * @brief Constructor from `unexpected<G>` (copy, implicit).
   * @details Creates an `expected` in the error state by copying the error
   * from `unex`, so a function that returns an `expected` can write
   * `return unexpected<E> (error);`. Implicit when `G const &` converts to
   * `ErrorType` ([expected.object.cons]/27 to /31).
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
  LUMEX_CONSTEXPR_CTOR explicit expected (unexpected<G> &&unex)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<ErrorType, G>::value)
      : base_type (detail::error_tag (), detail::move_error (unex))
  {
  }

  // The destructor is the implicit one: trivial when SuccessType and
  // ErrorType are trivially destructible ([expected.object.dtor]/2).

  // ====================== Assignment Operators ====================== //
  // The copy and the move assignment are the implicit ones, see above.

  /**
   * @brief Replaces the contents with a new success value.
   * @details If the object holds a value, it is assigned. Otherwise the error
   * is destroyed and the value constructed in its place by `reinit-expected`
   * ([expected.object.assign]/12): no temporary `expected`, and if the
   * construction throws, the object keeps its previous contents.
   * @tparam U Type of the new value; `SuccessType` is constructible and
   * assignable from it.
   * @param[in] val The new value.
   * @return Reference to this `expected`.
   * @note Takes part in overload resolution only for a `U` that is not an
   * `expected` or an `unexpected`, and only if `SuccessType` is nothrow
   * constructible from `U` or `SuccessType` or `ErrorType` can be moved
   * without throwing, like `std::expected`.
   * @throws May throw if constructing or assigning `SuccessType` from `U`
   * throws.
   */
  template <
      typename U = typename std::remove_cv<SuccessType>::type,
      typename std::enable_if<
          !std::is_same<expected, detail::remove_cvref_t<U>>::value
              && !detail::is_unexpected<detail::remove_cvref_t<U>>::value
              && std::is_constructible<SuccessType, U>::value
              && std::is_assignable<SuccessType &, U>::value
              && (std::is_nothrow_constructible<SuccessType, U>::value
                  || std::is_nothrow_move_constructible<SuccessType>::value
                  || std::is_nothrow_move_constructible<ErrorType>::value),
          int>::type
      = 0>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 expected &
  operator= (U &&val)
  {
    if (m_has_value)
      m_storage.m_value = detail::fwd<U> (val);
    else
      {
        detail::reinit (m_storage.m_value, m_storage.m_error,
                        detail::fwd<U> (val));
        m_has_value = true;
      }
    return *this;
  }

  /**
   * @brief Replaces the contents with an error (copy).
   * @details If the object holds an error, it is assigned. Otherwise the
   * value is destroyed and the error constructed in its place by
   * `reinit-expected` ([expected.object.assign]/16); if the copy throws, this
   * object keeps its previous contents.
   * @tparam G Error type of `unex`; `ErrorType` is constructible and
   * assignable from it.
   * @param[in] unex The new error.
   * @return Reference to this `expected`, now in the error state.
   * @note Takes part in overload resolution only if `ErrorType` is nothrow
   * constructible from `G const &` or `SuccessType` or `ErrorType` can be
   * moved without throwing, like `std::expected`.
   * @throws May throw if copying the error throws.
   */
  template <
      typename G,
      typename std::enable_if<
          std::is_constructible<ErrorType, G const &>::value
              && std::is_assignable<ErrorType &, G const &>::value
              && (std::is_nothrow_constructible<ErrorType, G const &>::value
                  || std::is_nothrow_move_constructible<SuccessType>::value
                  || std::is_nothrow_move_constructible<ErrorType>::value),
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
  template <
      typename G,
      typename std::enable_if<
          std::is_constructible<ErrorType, G>::value
              && std::is_assignable<ErrorType &, G>::value
              && (std::is_nothrow_constructible<ErrorType, G>::value
                  || std::is_nothrow_move_constructible<SuccessType>::value
                  || std::is_nothrow_move_constructible<ErrorType>::value),
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
   * @brief Checks whether this `expected` holds an error.
   * @details `!has_value ()` ([expected.object.obs]/8 of the working draft,
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
   * @return `true` if the object holds a success value, `false` otherwise.
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
    LUMEX_STATIC_ASSERT_MSG (std::is_copy_constructible<ErrorType>::value,
                             "value (): ErrorType must be copy constructible");
    if (!m_has_value)
      detail::throw_bad_expected_access<ErrorType> (
          static_cast<ErrorType const &> (m_storage.m_error));
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
    LUMEX_STATIC_ASSERT_MSG (
        std::is_copy_constructible<ErrorType>::value
            && std::is_constructible<ErrorType, ErrorType &&>::value,
        "value (): ErrorType must be copy and move constructible");
    if (!m_has_value)
      detail::throw_bad_expected_access<ErrorType> (
          detail::mv (m_storage.m_error)); // Move the error into the exception
    return static_cast<SuccessType &&> (m_storage.m_value);
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
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &
  value () const &
  {
    LUMEX_STATIC_ASSERT_MSG (std::is_copy_constructible<ErrorType>::value,
                             "value (): ErrorType must be copy constructible");
    return m_has_value
               ? static_cast<SuccessType const &> (m_storage.m_value)
               : (detail::throw_bad_expected_access<ErrorType> (
                      static_cast<ErrorType const &> (m_storage.m_error)),
                  static_cast<SuccessType const &> (m_storage.m_value));
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
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
  value () const &&
  {
    LUMEX_STATIC_ASSERT_MSG (
        std::is_copy_constructible<ErrorType>::value
            && std::is_constructible<ErrorType, ErrorType const &&>::value,
        "value (): ErrorType must be copy and move constructible");
    return m_has_value
               ? static_cast<SuccessType const &&> (m_storage.m_value)
               : (detail::throw_bad_expected_access<ErrorType> (
                      static_cast<ErrorType const &&> (
                          m_storage.m_error)), // A const error is copied
                  static_cast<SuccessType const &&> (m_storage.m_value));
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
      error ()
      & LUMEX_NOEXCEPT
  {
    // Precondition: !m_has_value.
    // In std::expected, error() is an unchecked observer: calling it without
    // an error is undefined behavior, and checked access is value(), which
    // throws bad_expected_access<E>. Here the storage is a union, so reading
    // m_storage.m_error while m_has_value is true would read an inactive
    // member, which is undefined behavior in C++.
    //
    // This method checks the precondition with LUMEX_ASSERT instead of
    // throwing (the "hardened precondition" of [expected.object.obs]).
    // LUMEX_ASSERT is active in every build, NDEBUG included: a violation
    // prints the condition, the file and the line, and aborts the program
    // before the inactive member is read. The check costs one branch in every
    // build. error() never throws, so it stays usable where an exception must
    // not escape (destructors, swap, emergency paths); checked, catchable
    // access is value() or a prior has_value() check.
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected that holds a value"),
           m_storage.m_error;
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
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &
  error () const &LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected that holds a value"),
           m_storage.m_error;
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
      error ()
      && LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected that holds a value"),
           static_cast<ErrorType &&> (m_storage.m_error);
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
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &&
  error () const &&LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               !m_has_value
               && "error() called on an Expected that holds a value"),
           static_cast<ErrorType const &&> (m_storage.m_error);
  }

  /**
   * @brief Returns the stored value if present, otherwise the default value.
   * @tparam U Default-value type; must be convertible to `SuccessType`.
   * @param[in] default_value Value returned if the object does not hold a
   * success value.
   * @details Returns the stored value, or `default_value` when the object
   * holds an error ([expected.object.obs]/19 and /20).
   * @note Not declared `noexcept`: copying the stored value or converting
   * `default_value` to `SuccessType` may throw.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U = typename std::remove_cv<SuccessType>::type>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained value or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION SuccessType value_or (U &&default_value) const &
  {
    LUMEX_STATIC_ASSERT_MSG (
        std::is_copy_constructible<SuccessType>::value
            && std::is_convertible<U, SuccessType>::value,
        "value_or (): SuccessType must be copy constructible and U "
        "convertible to it");
    return m_has_value
               ? static_cast<SuccessType const &> (m_storage.m_value)
               : static_cast<SuccessType> (detail::fwd<U> (default_value));
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
    LUMEX_STATIC_ASSERT_MSG (
        std::is_move_constructible<SuccessType>::value
            && std::is_convertible<U, SuccessType>::value,
        "value_or (): SuccessType must be move constructible and U "
        "convertible to it");
    return m_has_value
               ? static_cast<SuccessType &&> (m_storage.m_value)
               : static_cast<SuccessType> (detail::fwd<U> (default_value));
  }

  /**
   * @brief Returns the error if present, otherwise the given default error.
   * @details For an rvalue object: if an error is present it is moved out;
   * otherwise returns a copy/move constructed from `default_error`
   * ([expected.object.obs]/25 and /26).
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
   * @brief Returns the stored error if present, otherwise the default error.
   * @tparam G Default-error type; must be convertible to `ErrorType`.
   * @param[in] default_error Error returned if the object holds a success
   * value.
   * @return The stored error or `default_error`.
   * @note Not declared `noexcept`: copying the stored error or converting
   * `default_error` to `ErrorType` may throw.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
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
    return LUMEX_ASSERT (
               m_has_value
               && "operator* called on an Expected that holds an error"),
           m_storage.m_value;
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
    return LUMEX_ASSERT (
               m_has_value
               && "operator* called on an Expected that holds an error"),
           static_cast<SuccessType &&> (m_storage.m_value);
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
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &
  operator* () const &LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               m_has_value
               && "operator* called on an Expected that holds an error"),
           static_cast<SuccessType const &> (m_storage.m_value);
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
  LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
  operator* () const &&LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               m_has_value
               && "operator* called on an Expected that holds an error"),
           static_cast<SuccessType const &&> (m_storage.m_value);
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
    return LUMEX_ASSERT (
               m_has_value
               && "operator-> called on an Expected that holds an error"),
           detail::address_of (m_storage.m_value);
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
  LUMEX_CONSTEXPR_FUNCTION SuccessType const *
  operator->() const LUMEX_NOEXCEPT
  {
    return LUMEX_ASSERT (
               m_has_value
               && "operator-> called on an Expected that holds an error"),
           detail::address_of (m_storage.m_value);
  }

  // ====================== Modifiers ======================

  /**
   * @brief Replaces the current contents with a new `SuccessType` value.
   * @details Destroys the current value or error and constructs the new value
   * in place from the forwarded arguments ([expected.object.assign]/19). The
   * construction cannot throw, which is what the constraint requires, so
   * unlike the assignments there is nothing to keep and no temporary.
   * @tparam Args Argument types for the `SuccessType` constructor.
   * @param[in] args Arguments forwarded to the `SuccessType` constructor.
   * @return Reference to the new `SuccessType` value.
   * @note Takes part in overload resolution only when `SuccessType` is nothrow
   * constructible from `Args`, like `std::expected`; to replace the contents
   * with a value that may throw, assign
   * `expected (in_place, args...)`.
   */
  template <typename... Args,
            typename = typename std::enable_if<std::is_nothrow_constructible<
                SuccessType, Args...>::value>::type>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 SuccessType &
  emplace (Args &&...args) LUMEX_NOEXCEPT
  {
    reset_to_value_slot ();
    return *detail::construct_at (detail::address_of (m_storage.m_value),
                                  detail::fwd<Args> (args)...);
  }

  /**
   * @brief Replaces the contents with a new `SuccessType` value built from an
   * initializer list.
   * @details As `emplace (args...)`, passing `list` before the arguments
   * ([expected.object.assign]/21).
   * @tparam U Element type of the initializer list.
   * @tparam Args Argument types for the `SuccessType` constructor.
   * @param[in] list Initializer list passed to the `SuccessType` constructor.
   * @param[in] args Arguments forwarded after the list.
   * @return Reference to the new `SuccessType` value.
   * @note Takes part in overload resolution only when `SuccessType` is nothrow
   * constructible from the list and `Args`.
   */
  template <
      typename U, typename... Args,
      typename = typename std::enable_if<std::is_nothrow_constructible<
          SuccessType, std::initializer_list<U> &, Args...>::value>::type>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 SuccessType &
  emplace (std::initializer_list<U> list, Args &&...args) LUMEX_NOEXCEPT
  {
    reset_to_value_slot ();
    return *detail::construct_at (detail::address_of (m_storage.m_value), list,
                                  detail::fwd<Args> (args)...);
  }

  /**
   * @brief Constructs an `ErrorType` value in place, replacing the current
   * contents.
   * @details The error counterpart of `emplace ()`, an addition of this
   * library: the new error is built by `reinit-expected`
   * ([expected.object.assign]/1) in the place of the current value or error,
   * so if the `ErrorType` constructor throws, this object keeps its previous
   * contents.
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
   * @brief Exchanges contents with another `expected` object.
   * @details If both objects hold values (or both hold errors), the values
   * (errors) are swapped with `swap` found by argument-dependent lookup. If
   * one holds a value and the other an error, the contents are moved across
   * with the exception safety of Table 72 of [expected.object.swap]: the
   * object that was changed first is restored if a later move throws.
   * @param[in,out] other The other `expected` object to swap with.
   * @note Takes part in overload resolution only when `SuccessType` and
   * `ErrorType` are swappable and move constructible and one of them can be
   * moved without throwing, like `std::expected`. `noexcept` when both are
   * nothrow move constructible and nothrow swappable, in every standard.
   * @throws May throw if a move constructor or `swap` of `SuccessType` or
   * `ErrorType` throws.
   */
  template <typename Self = expected,
            typename = typename std::enable_if<detail::can_swap_values<
                Self, SuccessType, ErrorType>::value>::type>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap (expected &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<SuccessType>::value
          &&std::is_nothrow_move_constructible<ErrorType>::value
              &&detail::is_nothrow_swappable<SuccessType>::value
                  &&detail::is_nothrow_swappable<ErrorType>::value)
  {
    this->swap_storage (other);
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
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func),
                                              m_storage.m_value)
                       : ResultType (unexpect, m_storage.m_error);
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
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType &&func)
      const & -> detail::result_clean_t<FunctionType, SuccessType const &>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, SuccessType const &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func),
                                              m_storage.m_value)
                       : ResultType (unexpect, m_storage.m_error);
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
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_value))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
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
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then (FunctionType &&func)
      const && -> detail::result_clean_t<FunctionType, SuccessType const &&>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, SuccessType const &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::error_type, ErrorType>::value,
        "and_then: the function must return an expected with the "
        "error type of the object it is called on");
    return m_has_value ? detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_value))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
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
    return m_has_value ? ResultType (in_place, m_storage.m_value)
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
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
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType &&func)
      const & -> detail::result_clean_t<FunctionType, ErrorType const &>
  {
    using ResultType = detail::result_clean_t<FunctionType, ErrorType const &>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType (in_place, m_storage.m_value)
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
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
    return m_has_value ? ResultType (in_place, detail::mv (m_storage.m_value))
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_error));
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
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else (FunctionType &&func)
      const && -> detail::result_clean_t<FunctionType, ErrorType const &&>
  {
    using ResultType
        = detail::result_clean_t<FunctionType, ErrorType const &&>;
    LUMEX_STATIC_ASSERT_MSG (
        std::is_same<typename ResultType::value_type, SuccessType>::value,
        "or_else: the function must return an expected with the "
        "value type of the object it is called on");
    return m_has_value ? ResultType (in_place, detail::mv (m_storage.m_value))
                       : detail::invoke_call (detail::fwd<FunctionType> (func),
                                              detail::mv (m_storage.m_error));
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
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_value)
                       : ResultType (unexpect, m_storage.m_error);
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
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType &&func) const & -> expected<
      detail::result_xform_t<FunctionType, SuccessType const &>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType const &>,
                   ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_value)
                       : ResultType (unexpect, m_storage.m_error);
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
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_value))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
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
  LUMEX_CONSTEXPR_FUNCTION auto
  transform (FunctionType &&func) const && -> expected<
      detail::result_xform_t<FunctionType, SuccessType const &&>, ErrorType>
  {
    using ResultType
        = expected<detail::result_xform_t<FunctionType, SuccessType const &&>,
                   ErrorType>;
    return m_has_value ? ResultType (detail::invoke_value_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_value))
                       : ResultType (unexpect, detail::mv (m_storage.m_error));
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
    return m_has_value ? ResultType (in_place, m_storage.m_value)
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_error);
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
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType &&func) const & -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType const &>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType const &>>;
    return m_has_value ? ResultType (in_place, m_storage.m_value)
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     m_storage.m_error);
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
    return m_has_value ? ResultType (in_place, detail::mv (m_storage.m_value))
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_error));
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
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error (FunctionType &&func) const && -> expected<
      SuccessType, detail::result_xform_t<FunctionType, ErrorType const &&>>
  {
    using ResultType
        = expected<SuccessType,
                   detail::result_xform_t<FunctionType, ErrorType const &&>>;
    return m_has_value ? ResultType (in_place, detail::mv (m_storage.m_value))
                       : ResultType (detail::invoke_error_tag (),
                                     detail::fwd<FunctionType> (func),
                                     detail::mv (m_storage.m_error));
  }

private:
  template <typename, typename> friend class expected;

  // The members of the storage `expected` is built on (the names are those
  // of the exposition-only members of the standard: `has_val` and the union
  // of `val` and `unex`).
  using base_type::m_has_value;
  using base_type::m_storage;

  /**
   * @brief Builds a success value from the result of a call.
   * @details Used by the monadic operations: the stored value is initialized
   * with `detail::invoke_call (fn, args...)` itself, so the result is
   * constructed in place.
   * @tparam Fn Type of the function.
   * @tparam Args Types of its arguments.
   * @param[in] tag Selects the constructor.
   * @param[in] fn The function.
   * @param[in] args Arguments of the call.
   */
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected (detail::invoke_value_tag tag,
                                          Fn &&fn, Args &&...args)
      : base_type (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...)
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
   * @brief Ends the life of the current alternative and leaves the object in
   * the success state with no value constructed (the first half of
   * `emplace`, [expected.object.assign]/19).
   */
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  reset_to_value_slot () LUMEX_NOEXCEPT
  {
    if (m_has_value)
      detail::destroy_at (detail::address_of (m_storage.m_value));
    else
      {
        detail::destroy_at (detail::address_of (m_storage.m_error));
        m_has_value = true;
      }
  }

  /**
   * @brief Makes `err` the error: assigns it if there is one, otherwise
   * replaces the value (`reinit-expected`, [expected.object.assign]/16).
   * @tparam GF The error as the caller forwards it (`G const &` or `G`).
   * @param[in] err The new error.
   */
  template <typename GF>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  assign_error (GF &&err)
  {
    if (m_has_value)
      {
        detail::reinit (m_storage.m_error, m_storage.m_value,
                        detail::fwd<GF> (err));
        m_has_value = false;
      }
    else
      m_storage.m_error = detail::fwd<GF> (err);
  }
};

// The specialization for a `void` success value is defined in
// ExpectedVoid.hpp; declaring it here keeps `transform` of an `expected<T, E>`
// with a function that returns `void` from naming the primary template for
// `void`.
template <typename ErrorType> class expected<void, ErrorType>;

// ====================== Non-member functions ======================

/**
 * @brief Compares two `expected` objects with a value type for equality.
 * @details Two objects are equal if they are in the same state (both success
 * or both error) and:
 *          - on success their values compare equal (`*lhs == *rhs`);
 *          - on error their errors compare equal (`lhs.error() ==
 * rhs.error()`).
 * The types need not be the same: an `expected<int, E>` compares with an
 * `expected<long, E>`.
 *
 * @tparam SuccessType Success value type of the left operand.
 * @tparam ErrorType   Error type of the left operand.
 * @tparam OtherSuccess Success value type of the right operand.
 * @tparam OtherError   Error type of the right operand.
 * @param[in] lhs Left-hand operand.
 * @param[in] rhs Right-hand operand.
 * @return `true` if the objects match in state and contents, otherwise
 * `false`.
 *
 * @note Takes part in overload resolution only when the value types are not
 * `void` and both pairs of types can be compared with `==`.
 * @par Thread safety
 *      Not thread-safe if the same instances are accessed concurrently without
 * synchronization.
 * @par Exception guarantees
 *      May throw if `operator==` of `SuccessType` or `ErrorType` throws.
 */
template <typename SuccessType, typename ErrorType, typename OtherSuccess,
          typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value && !std::is_void<OtherSuccess>::value
        && detail::is_equality_comparable<SuccessType, OtherSuccess>::value
        && detail::is_equality_comparable<ErrorType, OtherError>::value,
    bool>::type
operator== (expected<SuccessType, ErrorType> const &lhs,
            expected<OtherSuccess, OtherError> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? false
             : (lhs.has_value ()
                    ? static_cast<bool> (*lhs == *rhs)
                    : static_cast<bool> (lhs.error () == rhs.error ()));
}

/**
 * @brief Compares two `expected<void, E>` objects for equality.
 * @details Two objects are equal if:
 *          - both are in the success state (then they are always equal);
 *          - or both hold errors that compare equal (`lhs.error() ==
 * rhs.error()`).
 * The error types need not be the same.
 *
 * @tparam ErrorType Error type of the left operand.
 * @tparam OtherError Error type of the right operand.
 * @param[in] lhs Left-hand operand.
 * @param[in] rhs Right-hand operand.
 * @return `true` if the objects match in state and (on error) error value,
 * otherwise `false`.
 *
 * @note Takes part in overload resolution only when the errors can be
 * compared with `==`.
 * @par Thread safety
 *      Not thread-safe under concurrent access to the same instances.
 * @par Exception guarantees
 *      May throw if `operator==` of the error types throws.
 */
template <typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator== (expected<void, ErrorType> const &lhs,
            expected<void, OtherError> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? false
             : (lhs.has_value ()
                    ? true // Both are void success
                    : static_cast<bool> (lhs.error () == rhs.error ()));
}

/**
 * @brief Compares an `expected` with a value for equality.
 * @details Equal when the object holds a value and that value compares equal
 * to `value` (`*lhs == value`); an object that holds an error is never equal.
 *
 * @tparam SuccessType Success value type of the `expected`.
 * @tparam ErrorType   Error type of the `expected`.
 * @tparam Value       Type of the compared value.
 * @param[in] lhs   The `expected`.
 * @param[in] value The value to compare with.
 * @return `true` if `lhs` holds a value equal to `value`.
 *
 * @note Takes part in overload resolution only for a `Value` that is not an
 * `expected` or an `unexpected`, when the value type of `lhs` is not `void`
 * and can be compared with `Value` using `==`.
 */
template <typename SuccessType, typename ErrorType, typename Value>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value
        && !lumex::core::utility::traits::value::is_expected<Value>::value
        && !detail::is_unexpected<Value>::value
        && detail::is_equality_comparable<SuccessType, Value>::value,
    bool>::type
operator== (expected<SuccessType, ErrorType> const &lhs, Value const &value)
{
  return lhs.has_value () && static_cast<bool> (*lhs == value);
}

/**
 * @brief Compares an `expected` with an `unexpected` for equality.
 * @details Equal when the object holds an error that compares equal to the
 * error of `unex` (`lhs.error () == unex.error ()`); an object that holds a
 * value (or, for `expected<void, E>`, success) is never equal.
 *
 * @tparam SuccessType Success value type of the `expected` (`void` included).
 * @tparam ErrorType   Error type of the `expected`.
 * @tparam OtherError  Error type of the `unexpected`.
 * @param[in] lhs  The `expected`.
 * @param[in] unex The `unexpected` to compare with.
 * @return `true` if `lhs` holds an error equal to the one of `unex`.
 *
 * @note Takes part in overload resolution only when the errors can be
 * compared with `==`.
 */
template <typename SuccessType, typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator== (expected<SuccessType, ErrorType> const &lhs,
            unexpected<OtherError> const &unex)
{
  return !lhs.has_value ()
         && static_cast<bool> (lhs.error () == unex.error ());
}

#if !LUMEX_HAS_IMPL_THREE_WAY_COMPARISON
// Before C++20 the reversed `==` and every `!=` are written out; from C++20
// the compiler rewrites them from the `operator==` above, as it does for
// `std::expected`.

/// @brief `lhs != rhs` for two `expected` objects with a value type: `!(lhs ==
/// rhs)`.
template <typename SuccessType, typename ErrorType, typename OtherSuccess,
          typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value && !std::is_void<OtherSuccess>::value
        && detail::is_equality_comparable<SuccessType, OtherSuccess>::value
        && detail::is_equality_comparable<ErrorType, OtherError>::value,
    bool>::type
operator!= (expected<SuccessType, ErrorType> const &lhs,
            expected<OtherSuccess, OtherError> const &rhs)
{
  return !(lhs == rhs);
}

/// @brief `lhs != rhs` for two `expected<void, E>` objects: `!(lhs == rhs)`.
template <typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator!= (expected<void, ErrorType> const &lhs,
            expected<void, OtherError> const &rhs)
{
  return !(lhs == rhs);
}

/// @brief `value == rhs`: the reversed form of `rhs == value`.
template <typename SuccessType, typename ErrorType, typename Value>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value
        && !lumex::core::utility::traits::value::is_expected<Value>::value
        && !detail::is_unexpected<Value>::value
        && detail::is_equality_comparable<SuccessType, Value>::value,
    bool>::type
operator== (Value const &value, expected<SuccessType, ErrorType> const &rhs)
{
  return rhs == value;
}

/// @brief `lhs != value`: `!(lhs == value)`.
template <typename SuccessType, typename ErrorType, typename Value>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value
        && !lumex::core::utility::traits::value::is_expected<Value>::value
        && !detail::is_unexpected<Value>::value
        && detail::is_equality_comparable<SuccessType, Value>::value,
    bool>::type
operator!= (expected<SuccessType, ErrorType> const &lhs, Value const &value)
{
  return !(lhs == value);
}

/// @brief `value != rhs`: `!(rhs == value)`.
template <typename SuccessType, typename ErrorType, typename Value>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    !std::is_void<SuccessType>::value
        && !lumex::core::utility::traits::value::is_expected<Value>::value
        && !detail::is_unexpected<Value>::value
        && detail::is_equality_comparable<SuccessType, Value>::value,
    bool>::type
operator!= (Value const &value, expected<SuccessType, ErrorType> const &rhs)
{
  return !(rhs == value);
}

/// @brief `unex == rhs`: the reversed form of `rhs == unex`.
template <typename SuccessType, typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator== (unexpected<OtherError> const &unex,
            expected<SuccessType, ErrorType> const &rhs)
{
  return rhs == unex;
}

/// @brief `lhs != unex`: `!(lhs == unex)`.
template <typename SuccessType, typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator!= (expected<SuccessType, ErrorType> const &lhs,
            unexpected<OtherError> const &unex)
{
  return !(lhs == unex);
}

/// @brief `unex != rhs`: `!(rhs == unex)`.
template <typename SuccessType, typename ErrorType, typename OtherError>
LUMEX_CONSTEXPR_FUNCTION typename std::enable_if<
    detail::is_equality_comparable<ErrorType, OtherError>::value, bool>::type
operator!= (unexpected<OtherError> const &unex,
            expected<SuccessType, ErrorType> const &rhs)
{
  return !(rhs == unex);
}
#endif // !LUMEX_HAS_IMPL_THREE_WAY_COMPARISON

/**
 * @brief Exchanges the contents of two `expected` objects of the same type
 * (also `expected<void, E>`).
 * @details Calls `lhs.swap(rhs)`, delegating to the class implementation
 * ([expected.object.swap]/5, [expected.void.swap]/5).
 *
 * @tparam SuccessType Success value type (`void` included).
 * @tparam ErrorType   Error type.
 * @param[in,out] lhs Left-hand operand of the swap.
 * @param[in,out] rhs Right-hand operand of the swap.
 *
 * @note Takes part in overload resolution only when the member `swap` can be
 * called, and is `noexcept` exactly when it is, like
 * `std::swap (std::expected &, std::expected &)`.
 * @par Thread safety
 *      Not thread-safe for the same objects without external synchronization.
 * @par Performance
 *      Swap is typically O(1) and does not allocate.
 */
template <typename SuccessType, typename ErrorType>
LUMEX_EXPECTED_CONSTEXPR_CXX20 auto
swap (expected<SuccessType, ErrorType> &lhs,
      expected<SuccessType, ErrorType> &rhs)
    LUMEX_NOEXCEPT_IF (noexcept (lhs.swap (rhs))) -> decltype (lhs.swap (rhs))
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
      in_place, detail::fwd<U_val> (val));
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
 * @brief Creates an `unexpected<E>` from the arguments of its error.
 * @details `make_unexpected<E> (args...)` builds `E` from `args...` and wraps
 * it: `make_unexpected<int> (1)` and `make_unexpected<std::string> (3u, 'z')`
 * are both well-formed. The result converts to an `expected<T, E>` in the
 * error state (`return make_unexpected<E> (args...);`).
 * @tparam E Error type; the result is `unexpected<std::decay_t<E>>`.
 * @tparam Args Types of the arguments `E` is constructed from.
 * @param[in] args Arguments `E` is constructed from.
 * @return The `unexpected` that holds the new error.
 * @note There is no overload that returns an `expected`: the overloads
 * `make_unexpected<T> (error)` and `make_unexpected (error)` of 1.x, which
 * returned an `expected<T, E>` and `expected<void, E>`, were removed because
 * they made `make_unexpected<int> (1)` ambiguous. Write
 * `expected<T, E> (unexpect, error)` or `unexpected<E> (error)` instead.
 */
template <typename E, typename... Args>
LUMEX_ATTRIBUTE_NODISCARD (
    "Return value is an unexpected value; should always be used.")
LUMEX_CONSTEXPR_FUNCTION
    unexpected<typename std::decay<E>::type> make_unexpected (Args &&...args)
{
  using Err = typename std::decay<E>::type;
  return unexpected<Err> (Err (detail::fwd<Args> (args)...));
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

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
 * @file LumexOptional.hpp
 * @brief C++11 implementation of `std::optional` (C++17) and of its C++23
 * monadic operations: a type-safe nullable value container.
 * @details `optional<T>` holds a `T` or nothing. The class is the module's own
 * on every standard and is never an alias of `std::optional`: from C++17 it
 * converts implicitly to and from `std::optional<T>` (the same `T`) and
 * compares with it. The storage is a union `{ dummy, value }` followed by the
 * engaged flag, so `sizeof` equals that of `std::optional`, and a chain of
 * class templates (see `LumexOptionalDetail.hpp`), whose last member is the
 * only data member of `optional`, gives it the special member functions of the
 * standard: trivially copy/move constructible, trivially copy/move assignable
 * and trivially destructible when `T` is, and a copy or move that is deleted
 * (or ignored, for a move) when `T` cannot do it. Constructors, the observers
 * and the trivial operations are `constexpr` from C++11; the operations that
 * change the active member of the union
 * (`emplace`, assignment, non-trivial `reset` and `swap`) are `constexpr`
 * from C++20 (`LUMEX_OPTIONAL_CONSTEXPR_CXX20`).
 *
 * As `std::optional`, a moved-from `optional` keeps its (moved-from) value,
 * the assignment between two engaged optionals assigns the value, and a
 * throwing `T` leaves the engaged state as the standard describes
 * ([optional.assign]). The converting constructors and assignments, the
 * explicit rules and the monadic operations `and_then`, `transform`,
 * `or_else` follow [optional.ctor], [optional.assign] and [optional.monadic];
 * the monadic operations are SFINAE-constrained (C++11 has no `requires`): a
 * call whose function is not invocable, whose `and_then` result is not an
 * `optional` or whose `transform` result cannot be held, is not viable
 * instead of failing a `static_assert`. `lumex_bad_optional_access` derives
 * from `std::logic_error`; `std::bad_optional_access` exists only from C++17
 * (see `LumexOptionalTags.hpp`). This header declares everything in
 * `lumex::core::optional::opt` and puts no name at global scope; the global
 * aliases (`optional`, `nullopt`, `make_optional`,
 * `lumex_bad_optional_access`) are in `LumexOptionalGlobals.hpp`, which the
 * umbrella `LumexOptional` includes.
 */
#ifndef LUMEX_CORE_OPTIONAL_OPT_HPP
#define LUMEX_CORE_OPTIONAL_OPT_HPP

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

#include <functional>       // For std::hash
#include <initializer_list> // For std::initializer_list
#include <type_traits>      // For std::is_*, std::decay
#include <utility>          // For std::forward, std::move, std::swap
#if __cplusplus >= 201703L
#include <optional> // For std::optional, the twin the class converts to/from
#endif

#include "lumex/core/optional/opt/LumexOptionalDetail.hpp"
#include "lumex/core/optional/opt/LumexOptionalTags.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace optional
{
namespace opt
{
/**
 * @brief A C++11 implementation of `std::optional` for type-safe nullable
 * values.
 * @details An `optional` either holds a `T` or is empty. The value lives in a
 * union member next to a `bool`, so there is no allocation and `sizeof` is
 * that of `std::optional<T>`. The special member functions are those of
 * [optional.ctor], [optional.dtor] and [optional.assign]: trivial when the
 * ones of `T` are, deleted when `T` cannot be copied (or, for the moves,
 * ignored by overload resolution when `T` cannot be moved), and written out
 * otherwise. After a move the source still holds its moved-from value.
 *
 * @tparam T The type of the value to be held: an object type that is not an
 * array, `in_place_t` or `nullopt_t`.
 */
template <typename T> class optional
{
  static_assert (std::is_object<T>::value && !std::is_array<T>::value
                     && !detail::is_tag_type<T>::value,
                 "optional<T>: T must be an object type that is not an "
                 "array, in_place_t or nullopt_t");

  using base_type = detail::opt_move_assign_base<T>;

  /// @brief The value, the flag and the special member functions. A member,
  /// not a base class: the compiler copies a member as one object of
  /// `sizeof (base_type)` bytes (one 8-byte move for `optional<int>`), where
  /// it copies a base class subobject without its tail padding (a 4-byte and
  /// a 1-byte move).
  base_type m_p;

  template <typename U> friend class optional;

  /// @brief The constraints of `optional (U &&)` that do not depend on the
  /// implicit/explicit split ([optional.ctor]/22).
  template <typename U>
  using enables_value_ctor = std::integral_constant<
      bool,
      std::is_constructible<T, U>::value
          && !std::is_same<detail::remove_cvref_t<U>, in_place_t>::value
          && !std::is_same<detail::remove_cvref_t<U>, optional>::value
          && !(detail::is_cv_bool<T>::value
               && detail::is_optional_specialization<
                   detail::remove_cvref_t<U>>::value)
#if __cplusplus >= 201703L
          && !std::is_same<detail::remove_cvref_t<U>, std::optional<T>>::value
#endif
      >;

  /// @brief The constraints of the converting constructors from
  /// `optional<U>` once `T` is constructible from the value ([optional.ctor]).
  template <typename U>
  using enables_from_optional = std::integral_constant<
      bool, detail::is_cv_bool<T>::value
                || !detail::converts_from_any_cvref<T, optional<U>>::value>;

  /// @brief The extra constraints of the converting assignments from
  /// `optional<U>` ([optional.assign]).
  template <typename U>
  using enables_assign_from_optional = std::integral_constant<
      bool, !detail::converts_from_any_cvref<T, optional<U>>::value
                && !detail::assigns_from_any_cvref<T, optional<U>>::value>;

  /// @brief Private constructor of `transform`: the value is built from the
  /// result of the call, never from a converting constructor, so an `optional`
  /// result is held, not flattened.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  optional (detail::invoke_tag tag, Fn &&fn, Args &&...args)
      : m_p (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...)
  {
  }

public:
  /**
   * @brief Type alias for the contained value type.
   */
  using value_type = T;

  // - Constructors -

  /**
   * @brief Default constructor. Creates an empty `optional`.
   * @details `constexpr` and `noexcept` from C++11; with a trivially
   * destructible `T` the object is a literal type.
   */
  LUMEX_CONSTEXPR_CTOR
  optional () LUMEX_NOEXCEPT : m_p () {}

  /**
   * @brief Constructs an empty `optional` from `nullopt`.
   */
  LUMEX_CONSTEXPR_CTOR
  optional (nullopt_t /*unused*/) LUMEX_NOEXCEPT : m_p () {}

  /**
   * @brief Copy constructor.
   * @details Trivial when `T` is trivially copy constructible; deleted when
   * `T` is not copy constructible (so `std::is_copy_constructible` is false
   * for `optional<std::unique_ptr<int>>`); copies the value otherwise. The
   * `noexcept` specification follows `T`.
   */
  optional (optional const &) = default;

  /**
   * @brief Move constructor.
   * @details Trivial when `T` is trivially move constructible; not
   * available (an rvalue is copied) when `T` is not move constructible;
   * moves the value otherwise. The source keeps its (moved-from) value, as
   * `std::optional`'s. `noexcept` when `T`'s move constructor is.
   */
  optional (optional &&) = default;

  /**
   * @brief In-place constructor. Constructs the contained value directly.
   * @tparam Args Types of the arguments of the constructor of `T`.
   * @param tag `in_place`.
   * @param args Arguments to forward to the constructor of `T`.
   */
  template <typename... Args,
            typename std::enable_if<std::is_constructible<T, Args...>::value,
                                    int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR explicit optional (in_place_t tag, Args &&...args)
      : m_p (tag, detail::fwd<Args> (args)...)
  {
  }

  /**
   * @brief In-place constructor with an `std::initializer_list`.
   * @tparam U The type of elements in the `initializer_list`.
   * @tparam Args Types of the further arguments.
   * @param tag `in_place`.
   * @param ilist An `std::initializer_list` for the constructor of `T`.
   * @param args Additional arguments to forward to the constructor of `T`.
   */
  template <
      typename U, typename... Args,
      typename std::enable_if<
          std::is_constructible<T, std::initializer_list<U> &, Args...>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR explicit optional (in_place_t tag,
                                          std::initializer_list<U> ilist,
                                          Args &&...args)
      : m_p (tag, ilist, detail::fwd<Args> (args)...)
  {
  }

  /**
   * @brief Constructs the value from `value` (implicit when `U` converts to
   * `T` implicitly).
   * @details [optional.ctor]/22: viable when `T` is constructible from `U`,
   * `U` is neither `in_place_t` nor `optional`, and `T` is not `bool` with an
   * `optional` argument. `optional<std::string> s = "abc";` compiles.
   * @tparam U Type of the argument; `T` when it is a braced list.
   * @param value The value to construct from.
   */
  template <typename U = T,
            typename std::enable_if<enables_value_ctor<U &&>::value
                                        && std::is_convertible<U &&, T>::value,
                                    int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  optional (U &&value)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U &&>::value)
      : m_p (in_place, detail::fwd<U> (value))
  {
  }

  /// @brief The same constructor, `explicit` when `U` does not convert to `T`
  /// implicitly.
  template <typename U = T, typename std::enable_if<
                                enables_value_ctor<U &&>::value
                                    && !std::is_convertible<U &&, T>::value,
                                int>::type
                            = 0>
  LUMEX_CONSTEXPR_CTOR explicit optional (U &&value)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U &&>::value)
      : m_p (in_place, detail::fwd<U> (value))
  {
  }

  /**
   * @brief Converting copy constructor from `optional<U>` (implicit when
   * `const U &` converts to `T` implicitly).
   * @details [optional.ctor]/27. Constexpr from C++20 only: a constant
   * expression cannot choose the active member of the union by the state of
   * another object before.
   * @tparam U The value type of the source.
   * @param rhs The source; unchanged.
   */
  template <typename U, typename std::enable_if<
                            std::is_constructible<T, U const &>::value
                                && enables_from_optional<U>::value
                                && std::is_convertible<U const &, T>::value,
                            int>::type
                        = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  optional (optional<U> const &rhs)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U const &>::value)
      : m_p ()
  {
    if (rhs.has_value ())
      m_p.construct (*rhs);
  }

  /// @brief The same constructor, `explicit` when `const U &` does not convert
  /// to `T` implicitly.
  template <typename U, typename std::enable_if<
                            std::is_constructible<T, U const &>::value
                                && enables_from_optional<U>::value
                                && !std::is_convertible<U const &, T>::value,
                            int>::type
                        = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 explicit optional (optional<U> const &rhs)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U const &>::value)
      : m_p ()
  {
    if (rhs.has_value ())
      m_p.construct (*rhs);
  }

  /**
   * @brief Converting move constructor from `optional<U>` (implicit when `U`
   * converts to `T` implicitly).
   * @details The value of an engaged `rhs` is moved; `rhs` stays engaged.
   * @tparam U The value type of the source.
   * @param rhs The source.
   */
  template <typename U,
            typename std::enable_if<std::is_constructible<T, U>::value
                                        && enables_from_optional<U>::value
                                        && std::is_convertible<U, T>::value,
                                    int>::type
            = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  optional (optional<U> &&rhs)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U>::value)
      : m_p ()
  {
    if (rhs.has_value ())
      m_p.construct (detail::mv (*rhs));
  }

  /// @brief The same constructor, `explicit` when `U` does not convert to `T`
  /// implicitly.
  template <typename U,
            typename std::enable_if<std::is_constructible<T, U>::value
                                        && enables_from_optional<U>::value
                                        && !std::is_convertible<U, T>::value,
                                    int>::type
            = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 explicit optional (optional<U> &&rhs)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_constructible<T, U>::value)
      : m_p ()
  {
    if (rhs.has_value ())
      m_p.construct (detail::mv (*rhs));
  }

#if __cplusplus >= 201703L
  /**
   * @brief Implicit conversion from `std::optional<T>` (C++17), copying.
   * @details A constructor template that accepts exactly `std::optional<T>`.
   * `constexpr` for a trivially copyable `T` (delegating to the trivial copy
   * of `optional`) and from C++20 for any `T`.
   * @tparam StdOptional Deduced from the argument; only `std::optional<T>`.
   * @param other The `std::optional<T>` to copy.
   */
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_copy_constructible<T>::value
                    && std::is_trivially_copy_constructible<T>::value
                    && std::is_trivially_move_constructible<T>::value,
                int>::type
            = 0>
  constexpr optional (StdOptional const &other) noexcept (
      std::is_nothrow_copy_constructible<T>::value)
      : optional (other.has_value () ? optional (in_place, *other)
                                     : optional ())
  {
  }

  /// @brief The same for a `T` that is not trivially copyable.
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_copy_constructible<T>::value
                    && !(std::is_trivially_copy_constructible<T>::value
                         && std::is_trivially_move_constructible<T>::value),
                int>::type
            = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  optional (StdOptional const &other) noexcept (
      std::is_nothrow_copy_constructible<T>::value)
      : m_p ()
  {
    if (other.has_value ())
      m_p.construct (*other);
  }

  /**
   * @brief Implicit conversion from `std::optional<T>` (C++17), moving.
   * @details The value of an rvalue `std::optional<T>` is moved; the source
   * keeps its (moved-from) value. `constexpr` as the copying form.
   * @tparam StdOptional Deduced from the argument; only an rvalue
   * `std::optional<T>` is accepted.
   * @param other The `std::optional<T>` to move from.
   */
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_move_constructible<T>::value
                    && std::is_trivially_move_constructible<T>::value,
                int>::type
            = 0>
  constexpr optional (StdOptional &&other) noexcept (
      std::is_nothrow_move_constructible<T>::value)
      : optional (other.has_value () ? optional (in_place, std::move (*other))
                                     : optional ())
  {
  }

  /// @brief The same for a `T` that is not trivially move constructible.
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_move_constructible<T>::value
                    && !std::is_trivially_move_constructible<T>::value,
                int>::type
            = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  optional (StdOptional &&other) noexcept (
      std::is_nothrow_move_constructible<T>::value)
      : m_p ()
  {
    if (other.has_value ())
      m_p.construct (std::move (*other));
  }

  /**
   * @brief Implicit conversion to `std::optional<T>` (C++17), copying.
   * @details A conversion function template that accepts exactly
   * `std::optional<T>` as the target; `constexpr` for a `T` the standard type
   * can hold in a constant expression.
   * @tparam StdOptional Deduced from the target; only `std::optional<T>`.
   * @return A `std::optional<T>` with a copy of the value, or empty.
   */
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_copy_constructible<T>::value,
                int>::type
            = 0>
  constexpr
  operator StdOptional () const & noexcept (
      std::is_nothrow_copy_constructible<T>::value)
  {
    return m_p.m_engaged ? std::optional<T> (m_p.m_u.value)
                         : std::optional<T> ();
  }

  /**
   * @brief Implicit conversion to `std::optional<T>` (C++17), moving.
   * @details Used for an rvalue `optional`: the value is moved out and the
   * source keeps its (moved-from) value.
   * @tparam StdOptional Deduced from the target; only `std::optional<T>`.
   * @return A `std::optional<T>` with the moved value, or empty.
   */
  template <typename StdOptional,
            typename std::enable_if<
                std::is_same<StdOptional, std::optional<T>>::value
                    && std::is_move_constructible<T>::value,
                int>::type
            = 0>
  constexpr
  operator StdOptional () && noexcept (
      std::is_nothrow_move_constructible<T>::value)
  {
    return m_p.m_engaged ? std::optional<T> (static_cast<T &&> (m_p.m_u.value))
                         : std::optional<T> ();
  }
#endif

  // ============ Destructor ============
  /**
   * @brief Destructor: trivial when `T` is trivially destructible, destroys
   * the value otherwise.
   */
  ~optional () = default;

  // ============ Assignment operators ============

  /**
   * @brief Assigns `nullopt` to the `optional`, making it empty.
   * @return A reference to `*this`.
   */
  LUMEX_CONSTEXPR_CXX14 optional &
  operator= (nullopt_t /*unused*/) LUMEX_NOEXCEPT
  {
    m_p.reset_value ();
    return *this;
  }

  /**
   * @brief Copy assignment.
   * @details Trivial when `T` is trivially copy constructible, copy
   * assignable and destructible; deleted unless `T` is copy constructible and
   * copy assignable. Otherwise: both engaged, the value is assigned (a
   * throwing assignment of `T` leaves both engaged); only `other` engaged,
   * the value is copy constructed (a throwing constructor leaves `*this`
   * empty); only `*this` engaged, it is destroyed. `other` is unchanged.
   * @return A reference to `*this`.
   */
  optional &operator= (optional const &) = default;

  /**
   * @brief Move assignment.
   * @details Trivial when `T` is trivially move constructible, move
   * assignable and destructible; not available (a copy is made) unless `T` is
   * move constructible and move assignable. Otherwise as the copy assignment,
   * with moves; `other` keeps its (moved-from) value.
   * @return A reference to `*this`.
   */
  optional &operator= (optional &&) = default;

  /**
   * @brief Assigns a value: assigns it to the contained value when there is
   * one, constructs it otherwise.
   * @details [optional.assign]/12. Not viable when `T` is a scalar and `U` is
   * `T` (the assignment then goes through the converting constructor and the
   * trivial move assignment).
   * @tparam U Type of the value.
   * @param value The value.
   * @return A reference to `*this`.
   */
  template <
      typename U = T,
      typename std::enable_if<
          !std::is_same<detail::remove_cvref_t<U>, optional>::value
              && std::is_constructible<T, U>::value
              && std::is_assignable<T &, U>::value
              && !(std::is_scalar<T>::value
                   && std::is_same<T, typename std::decay<U>::type>::value),
          int>::type
      = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 optional &
  operator= (U &&value)
  {
    if (m_p.m_engaged)
      m_p.m_u.value = detail::fwd<U> (value);
    else
      m_p.construct (detail::fwd<U> (value));
    return *this;
  }

  /**
   * @brief Converting copy assignment from `optional<U>`.
   * @details [optional.assign]/17: assigns, constructs or destroys by the
   * state of the two objects as the copy assignment does.
   * @tparam U The value type of the source.
   * @param rhs The source; unchanged.
   * @return A reference to `*this`.
   */
  template <typename U, typename std::enable_if<
                            !std::is_same<T, U>::value
                                && std::is_constructible<T, U const &>::value
                                && std::is_assignable<T &, U const &>::value
                                && enables_assign_from_optional<U>::value,
                            int>::type
                        = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 optional &
  operator= (optional<U> const &rhs)
  {
    if (m_p.m_engaged && rhs.has_value ())
      m_p.m_u.value = *rhs;
    else if (rhs.has_value ())
      m_p.construct (*rhs);
    else
      m_p.reset_value ();
    return *this;
  }

  /**
   * @brief Converting move assignment from `optional<U>`; `rhs` stays
   * engaged.
   * @tparam U The value type of the source.
   * @param rhs The source.
   * @return A reference to `*this`.
   */
  template <typename U, typename std::enable_if<
                            !std::is_same<T, U>::value
                                && std::is_constructible<T, U>::value
                                && std::is_assignable<T &, U>::value
                                && enables_assign_from_optional<U>::value,
                            int>::type
                        = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 optional &
  operator= (optional<U> &&rhs)
  {
    if (m_p.m_engaged && rhs.has_value ())
      m_p.m_u.value = detail::mv (*rhs);
    else if (rhs.has_value ())
      m_p.construct (detail::mv (*rhs));
    else
      m_p.reset_value ();
    return *this;
  }

  // ============ Observers ============

  /**
   * @brief Pointer to the contained value; undefined behavior when empty.
   */
  LUMEX_CONSTEXPR T const *
  operator->() const LUMEX_NOEXCEPT
  {
    return detail::address_of (m_p.m_u.value);
  }

  /// @copydoc operator->() const
  LUMEX_CONSTEXPR_CXX14 T *
  operator->() LUMEX_NOEXCEPT
  {
    return detail::address_of (m_p.m_u.value);
  }

  /**
   * @brief The contained value as a const lvalue; undefined behavior when
   * empty.
   */
  LUMEX_CONSTEXPR T const &
  operator* () const &LUMEX_NOEXCEPT
  {
    return m_p.m_u.value;
  }

  /// @brief The contained value as an lvalue; undefined behavior when empty.
  LUMEX_CONSTEXPR_CXX14 T &
      operator* ()
      & LUMEX_NOEXCEPT
  {
    return m_p.m_u.value;
  }

  /// @brief The contained value as a const rvalue; undefined behavior when
  /// empty.
  LUMEX_CONSTEXPR T const &&
  operator* () const &&LUMEX_NOEXCEPT
  {
    return static_cast<T const &&> (m_p.m_u.value);
  }

  /// @brief The contained value as an rvalue; undefined behavior when empty.
  LUMEX_CONSTEXPR_CXX14 T &&
      operator* ()
      && LUMEX_NOEXCEPT
  {
    return static_cast<T &&> (m_p.m_u.value);
  }

  /**
   * @brief Explicit conversion to `bool`: whether a value is held.
   */
  LUMEX_CONSTEXPR explicit
  operator bool () const LUMEX_NOEXCEPT
  {
    return m_p.m_engaged;
  }

  /**
   * @brief Whether the `optional` contains a value.
   */
  LUMEX_CONSTEXPR bool
  has_value () const LUMEX_NOEXCEPT
  {
    return m_p.m_engaged;
  }

  /**
   * @brief The contained value as an lvalue.
   * @throws lumex_bad_optional_access If the optional does not contain a
   * value.
   */
  LUMEX_CONSTEXPR_CXX14 T &
  value () &
  {
    return m_p.m_engaged
               ? m_p.m_u.value
               : (detail::throw_bad_optional_access (), m_p.m_u.value);
  }

  /// @brief The contained value as a const lvalue.
  /// @throws lumex_bad_optional_access If empty.
  LUMEX_CONSTEXPR T const &
  value () const &
  {
    return m_p.m_engaged
               ? m_p.m_u.value
               : (detail::throw_bad_optional_access (), m_p.m_u.value);
  }

  /// @brief The contained value as an rvalue.
  /// @throws lumex_bad_optional_access If empty.
  LUMEX_CONSTEXPR_CXX14 T &&
  value () &&
  {
    return m_p.m_engaged ? static_cast<T &&> (m_p.m_u.value)
                         : (detail::throw_bad_optional_access (),
                            static_cast<T &&> (m_p.m_u.value));
  }

  /// @brief The contained value as a const rvalue.
  /// @throws lumex_bad_optional_access If empty.
  LUMEX_CONSTEXPR T const &&
  value () const &&
  {
    return m_p.m_engaged ? static_cast<T const &&> (m_p.m_u.value)
                         : (detail::throw_bad_optional_access (),
                            static_cast<T const &&> (m_p.m_u.value));
  }

  /**
   * @brief The contained value (copied) or `default_value` converted to `T`.
   * @tparam U A type convertible to `T`.
   * @param default_value The value to return if the optional is empty.
   */
  template <typename U>
  LUMEX_CONSTEXPR T
  value_or (U &&default_value) const &
  {
    static_assert (std::is_copy_constructible<T>::value
                       && std::is_convertible<U &&, T>::value,
                   "value_or: T must be copy constructible and U convertible "
                   "to T");
    return m_p.m_engaged ? m_p.m_u.value
                         : static_cast<T> (detail::fwd<U> (default_value));
  }

  /// @brief The contained value (moved) or `default_value` converted to `T`.
  template <typename U>
  LUMEX_CONSTEXPR_CXX14 T
  value_or (U &&default_value) &&
  {
    static_assert (std::is_move_constructible<T>::value
                       && std::is_convertible<U &&, T>::value,
                   "value_or: T must be move constructible and U convertible "
                   "to T");
    return m_p.m_engaged ? static_cast<T &&> (m_p.m_u.value)
                         : static_cast<T> (detail::fwd<U> (default_value));
  }

  // - Monadic operations (C++23) -

  /**
   * @brief If a value is held, calls `fn` with it and returns the result,
   * which is an `optional`; otherwise an empty one of that type.
   * @details Four forms for the value category of `*this`; the result type
   * is `remove_cvref_t` of the call result. Not viable when `fn` cannot be
   * called with the value or its result is not an `optional`.
   * @tparam Fn Type of the function.
   * @param fn The function.
   */
  template <typename Fn>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (Fn &&fn) & -> typename detail::and_then_result<void, Fn, T &>::type
  {
    using result_type = typename detail::and_then_result<void, Fn, T &>::type;
    return m_p.m_engaged ? result_type (detail::invoke (detail::fwd<Fn> (fn),
                                                        m_p.m_u.value))
                         : result_type ();
  }

  /// @copydoc and_then
  template <typename Fn>
  LUMEX_CONSTEXPR auto
  and_then (Fn &&fn) const & ->
      typename detail::and_then_result<void, Fn, T const &>::type
  {
    using result_type =
        typename detail::and_then_result<void, Fn, T const &>::type;
    return m_p.m_engaged ? result_type (detail::invoke (detail::fwd<Fn> (fn),
                                                        m_p.m_u.value))
                         : result_type ();
  }

  /// @copydoc and_then
  template <typename Fn>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (Fn &&fn) && -> typename detail::and_then_result<void, Fn, T>::type
  {
    using result_type = typename detail::and_then_result<void, Fn, T>::type;
    return m_p.m_engaged
               ? result_type (detail::invoke (
                     detail::fwd<Fn> (fn), static_cast<T &&> (m_p.m_u.value)))
               : result_type ();
  }

  /// @copydoc and_then
  template <typename Fn>
  LUMEX_CONSTEXPR auto
  and_then (Fn &&fn) const && ->
      typename detail::and_then_result<void, Fn, T const>::type
  {
    using result_type =
        typename detail::and_then_result<void, Fn, T const>::type;
    return m_p.m_engaged ? result_type (detail::invoke (
                               detail::fwd<Fn> (fn),
                               static_cast<T const &&> (m_p.m_u.value)))
                         : result_type ();
  }

  /**
   * @brief If a value is held, calls `fn` with it and returns an `optional`
   * that holds the result; otherwise an empty `optional` of that type.
   * @details The result type is `optional<remove_cv_t<call result>>`; a call
   * result that is itself an `optional` is held, not flattened. Not viable
   * when `fn` cannot be called with the value or the result cannot be held.
   * @tparam Fn Type of the function.
   * @param fn The function.
   */
  template <typename Fn>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (Fn &&fn) & ->
      typename detail::transform_result<void, Fn, T &>::type
  {
    using result_type = typename detail::transform_result<void, Fn, T &>::type;
    return m_p.m_engaged ? result_type (detail::invoke_tag (),
                                        detail::fwd<Fn> (fn), m_p.m_u.value)
                         : result_type ();
  }

  /// @copydoc transform
  template <typename Fn>
  LUMEX_CONSTEXPR auto
  transform (Fn &&fn) const & ->
      typename detail::transform_result<void, Fn, T const &>::type
  {
    using result_type =
        typename detail::transform_result<void, Fn, T const &>::type;
    return m_p.m_engaged ? result_type (detail::invoke_tag (),
                                        detail::fwd<Fn> (fn), m_p.m_u.value)
                         : result_type ();
  }

  /// @copydoc transform
  template <typename Fn>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (Fn &&fn) && ->
      typename detail::transform_result<void, Fn, T>::type
  {
    using result_type = typename detail::transform_result<void, Fn, T>::type;
    return m_p.m_engaged
               ? result_type (detail::invoke_tag (), detail::fwd<Fn> (fn),
                              static_cast<T &&> (m_p.m_u.value))
               : result_type ();
  }

  /// @copydoc transform
  template <typename Fn>
  LUMEX_CONSTEXPR auto
  transform (Fn &&fn) const && ->
      typename detail::transform_result<void, Fn, T const>::type
  {
    using result_type =
        typename detail::transform_result<void, Fn, T const>::type;
    return m_p.m_engaged
               ? result_type (detail::invoke_tag (), detail::fwd<Fn> (fn),
                              static_cast<T const &&> (m_p.m_u.value))
               : result_type ();
  }

  /**
   * @brief Returns `*this` (copied) if a value is held, otherwise the result
   * of `fn ()`, which must be an `optional<T>`.
   * @details Not viable when `T` is not copy constructible, `fn` cannot be
   * called without arguments or its result is not `optional<T>` (cv and
   * reference ignored).
   * @tparam Fn Type of the function.
   * @param fn The function.
   */
  template <typename Fn,
            typename std::enable_if<
                detail::or_else_enabled<
                    Fn, optional, std::is_copy_constructible<T>::value>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR optional
  or_else (Fn &&fn) const &
  {
    return m_p.m_engaged ? *this
                         : optional (detail::invoke (detail::fwd<Fn> (fn)));
  }

  /// @brief Returns `*this` (moved) if a value is held, otherwise the result
  /// of `fn ()`. Needs a move constructible `T`.
  template <typename Fn,
            typename std::enable_if<
                detail::or_else_enabled<
                    Fn, optional, std::is_move_constructible<T>::value>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CXX14 optional
  or_else (Fn &&fn) &&
  {
    return m_p.m_engaged ? static_cast<optional &&> (*this)
                         : optional (detail::invoke (detail::fwd<Fn> (fn)));
  }

  // - Modifiers -

  /**
   * @brief Swaps the contents of this `optional` with another.
   * @details Both engaged: the values are swapped. One engaged: its value is
   * moved to the empty one and the source becomes empty. Both empty: nothing.
   * `noexcept` when `T` is nothrow move constructible and nothrow swappable.
   * @param other The `optional` object to swap with.
   */
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 void
  swap (optional &other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<T>::value
                             &&detail::is_nothrow_swappable<T>::value)
  {
    static_assert (std::is_move_constructible<T>::value
                       && detail::is_swappable<T>::value,
                   "swap: T must be move constructible and swappable");
    if (m_p.m_engaged && other.m_p.m_engaged)
      {
        using std::swap;
        swap (m_p.m_u.value, other.m_p.m_u.value);
      }
    else if (m_p.m_engaged)
      {
        other.m_p.construct (static_cast<T &&> (m_p.m_u.value));
        m_p.reset_value ();
      }
    else if (other.m_p.m_engaged)
      {
        m_p.construct (static_cast<T &&> (other.m_p.m_u.value));
        other.m_p.reset_value ();
      }
  }

  /**
   * @brief Resets the `optional` to an empty state, destroying a held value.
   * @details `constexpr` from C++14 for a trivially destructible `T`, from
   * C++20 for any `T`.
   */
  LUMEX_CONSTEXPR_CXX14 void
  reset () LUMEX_NOEXCEPT
  {
    m_p.reset_value ();
  }

  /**
   * @brief Constructs a new value in-place, destroying any existing value.
   * @details A throwing constructor of `T` leaves the `optional` empty.
   * @tparam Args Types of the arguments of the constructor of `T`.
   * @param args Arguments to forward to the constructor of `T`.
   * @return A reference to the new value.
   */
  template <typename... Args,
            typename std::enable_if<std::is_constructible<T, Args...>::value,
                                    int>::type
            = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 T &
  emplace (Args &&...args)
  {
    m_p.reset_value ();
    m_p.construct (detail::fwd<Args> (args)...);
    return m_p.m_u.value;
  }

  /**
   * @brief Constructs a new value in-place from an `initializer_list`,
   * destroying any existing value.
   * @tparam U The type of elements in the `initializer_list`.
   * @tparam Args Types of the further arguments.
   * @param ilist An `std::initializer_list` for the constructor of `T`.
   * @param args Additional arguments to forward to the constructor of `T`.
   * @return A reference to the new value.
   */
  template <
      typename U, typename... Args,
      typename std::enable_if<
          std::is_constructible<T, std::initializer_list<U> &, Args...>::value,
          int>::type
      = 0>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 T &
  emplace (std::initializer_list<U> ilist, Args &&...args)
  {
    m_p.reset_value ();
    m_p.construct (ilist, detail::fwd<Args> (args)...);
    return m_p.m_u.value;
  }
};

#if __cplusplus >= 201703L
/// @brief Deduction guide: `optional (value)` is `optional<decay_t<value>>`.
template <typename T> optional (T) -> optional<T>;
#endif

// - Non-member comparison operators -

/**
 * @brief Equality comparison operator for two `optional` objects.
 * @details
 *   - Returns `true` if both optionals are empty.
 *   - Returns `false` if one is empty and the other has a value.
 *   - Returns `true` if both have values and their contained values are equal
 * (`*lhs == *rhs`).
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if the optionals are equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator== (optional<T> const &lhs, optional<U> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? false
             : (!lhs.has_value () || *lhs == *rhs);
}

/**
 * @brief Inequality comparison operator for two `optional` objects.
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if the optionals are not equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator!= (optional<T> const &lhs, optional<U> const &rhs)
{
  return !(lhs == rhs);
}

/**
 * @brief Less-than comparison operator for two `optional` objects.
 * @details
 *   - Returns `false` if `rhs` is empty (nothing is less than empty).
 *   - Returns `true` if `lhs` is empty and `rhs` has a value (empty is less
 * than any value).
 *   - Returns `true` if both have values and `*lhs < *rhs`.
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if `lhs` is less than `rhs`, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator< (optional<T> const &lhs, optional<U> const &rhs)
{
  return !rhs.has_value () ? false : (!lhs.has_value () || *lhs < *rhs);
}

/**
 * @brief Less-than-or-equal-to comparison operator for two `optional`
 * objects.
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if `lhs` is less than or equal to `rhs`, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator<= (optional<T> const &lhs, optional<U> const &rhs)
{
  return !(rhs < lhs);
}

/**
 * @brief Greater-than comparison operator for two `optional` objects.
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if `lhs` is greater than `rhs`, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator> (optional<T> const &lhs, optional<U> const &rhs)
{
  return rhs < lhs;
}

/**
 * @brief Greater-than-or-equal-to comparison operator for two `optional`
 * objects.
 * @tparam T Type of value in the left-hand side optional.
 * @tparam U Type of value in the right-hand side optional.
 * @param lhs The left-hand side `optional` object.
 * @param rhs The right-hand side `optional` object.
 * @return `true` if `lhs` is greater than or equal to `rhs`, `false`
 * otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator>= (optional<T> const &lhs, optional<U> const &rhs)
{
  return !(lhs < rhs);
}

// Comparison with nullopt
/**
 * @brief Equality comparison between a `optional` and `nullopt`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional is empty, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator== (optional<T> const &opt, nullopt_t /*unused*/) LUMEX_NOEXCEPT
{
  return !opt.has_value ();
}

/**
 * @brief Equality comparison between `nullopt` and a `optional`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional is empty, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator== (nullopt_t /*unused*/, optional<T> const &opt) LUMEX_NOEXCEPT
{
  return !opt.has_value ();
}

/**
 * @brief Inequality comparison between a `optional` and `nullopt`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional has a value, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator!= (optional<T> const &opt, nullopt_t /*unused*/) LUMEX_NOEXCEPT
{
  return opt.has_value ();
}

/**
 * @brief Inequality comparison between `nullopt` and a `optional`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional has a value, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator!= (nullopt_t /*unused*/, optional<T> const &opt) LUMEX_NOEXCEPT
{
  return opt.has_value ();
}

/**
 * @brief Less-than comparison between a `optional` and `nullopt`.
 * @details An optional with a value is never less than `nullopt`.
 * @tparam T Type of value in the optional.
 * @return `false`.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator< (optional<T> const & /*unused_opt*/,
           nullopt_t /*unused_nullopt*/) LUMEX_NOEXCEPT
{
  return false;
}

/**
 * @brief Less-than comparison between `nullopt` and a `optional`.
 * @details `nullopt` is less than any `optional` that contains a value.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if `opt` has a value, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator< (nullopt_t /*unused_nullopt*/, optional<T> const &opt) LUMEX_NOEXCEPT
{
  return opt.has_value ();
}

/**
 * @brief Less-than-or-equal-to comparison between a `optional` and
 * `nullopt`.
 * @details An optional with a value is never less than or equal to `nullopt`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional is empty, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator<= (optional<T> const &opt,
            nullopt_t /*unused_nullopt*/) LUMEX_NOEXCEPT
{
  return !opt.has_value ();
}

/**
 * @brief Less-than-or-equal-to comparison between `nullopt` and a
 * `optional`.
 * @details `nullopt` is always less than or equal to any `optional`.
 * @tparam T Type of value in the optional.
 * @return `true`.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator<= (nullopt_t /*unused_nullopt*/,
            optional<T> const & /*unused_opt*/) LUMEX_NOEXCEPT
{
  return true;
}

/**
 * @brief Greater-than comparison between a `optional` and `nullopt`.
 * @details A `optional` with a value is always greater than `nullopt`.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if the optional has a value, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator> (optional<T> const &opt, nullopt_t /*unused_nullopt*/) LUMEX_NOEXCEPT
{
  return opt.has_value ();
}

/**
 * @brief Greater-than comparison between `nullopt` and a `optional`.
 * @details `nullopt` is never greater than any `optional`.
 * @tparam T Type of value in the optional.
 * @return `false`.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator> (nullopt_t /*unused_nullopt*/,
           optional<T> const & /*unused_opt*/) LUMEX_NOEXCEPT
{
  return false;
}

/**
 * @brief Greater-than-or-equal-to comparison between a `optional` and
 * `nullopt`.
 * @details A `optional` is always greater than or equal to `nullopt`.
 * @tparam T Type of value in the optional.
 * @return `true`.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator>= (optional<T> const & /*unused_opt*/,
            nullopt_t /*unused_nullopt*/) LUMEX_NOEXCEPT
{
  return true;
}

/**
 * @brief Greater-than-or-equal-to comparison between `nullopt` and a
 * `optional`.
 * @details `nullopt` is greater than or equal to a `optional` only if the
 * latter is empty.
 * @tparam T Type of value in the optional.
 * @param opt The `optional` object.
 * @return `true` if `opt` is empty, `false` otherwise.
 */
template <typename T>
LUMEX_CONSTEXPR bool
operator>= (nullopt_t /*unused_nullopt*/,
            optional<T> const &opt) LUMEX_NOEXCEPT
{
  return !opt.has_value ();
}

// Comparison with values
/**
 * @brief Equality comparison between a `optional` and a value.
 * @details Returns `true` if the optional has a value and that value is equal
 * to `value`.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator== (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt == value : false;
}

/**
 * @brief Equality comparison between a value and a `optional`.
 * @details Returns `true` if the optional has a value and `value` is equal to
 * that value.
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator== (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value == *opt : false;
}

/**
 * @brief Inequality comparison between a `optional` and a value.
 * @details Returns `true` if the optional is empty, or if it has a value and
 * that value is not equal to `value`.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if not equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator!= (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt != value : true;
}

/**
 * @brief Inequality comparison between a value and a `optional`.
 * @details Returns `true` if the optional is empty, or if it has a value and
 * `value` is not equal to that value.
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if not equal, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator!= (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value != *opt : true;
}

/**
 * @brief Less-than comparison between a `optional` and a value.
 * @details Returns `true` if the optional is empty (empty is less than any
 * value), or if it has a value and that value is less than `value`.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if less than, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator< (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt < value : true;
}

/**
 * @brief Less-than comparison between a value and a `optional`.
 * @details Returns `true` if the optional has a value and `value` is less than
 * that value. Returns `false` if the optional is empty (nothing is less than
 * empty).
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if less than, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator< (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value < *opt : false;
}

/**
 * @brief Less-than-or-equal-to comparison between a `optional` and a
 * value.
 * @details Returns `true` if the optional is empty, or if it has a value and
 * that value is less than or equal to `value`.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if less than or equal to, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator<= (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt <= value : true;
}

/**
 * @brief Less-than-or-equal-to comparison between a value and a
 * `optional`.
 * @details Returns `true` if the optional has a value and `value` is less than
 * or equal to that value. Returns `false` if the optional is empty.
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if less than or equal to, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator<= (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value <= *opt : false;
}

/**
 * @brief Greater-than comparison between a `optional` and a value.
 * @details Returns `true` if the optional has a value and that value is
 * greater than `value`. Returns `false` if the optional is empty.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if greater than, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator> (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt > value : false;
}

/**
 * @brief Greater-than comparison between a value and a `optional`.
 * @details Returns `true` if the optional is empty, or if it has a value and
 * `value` is greater than that value.
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if greater than, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator> (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value > *opt : true;
}

/**
 * @brief Greater-than-or-equal-to comparison between a `optional` and a
 * value.
 * @details Returns `true` if the optional has a value and that value is
 * greater than or equal to `value`. Returns `false` if the optional is empty.
 * @tparam T Type of value in the optional.
 * @tparam U Type of the value to compare against.
 * @param opt The `optional` object.
 * @param value The value to compare with.
 * @return `true` if greater than or equal to, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator>= (optional<T> const &opt, U const &value)
{
  return opt.has_value () ? *opt >= value : false;
}

/**
 * @brief Greater-than-or-equal-to comparison between a value and a
 * `optional`.
 * @details Returns `true` if the optional is empty, or if it has a value and
 * `value` is greater than or equal to that value.
 * @tparam T Type of the value to compare against.
 * @tparam U Type of value in the optional.
 * @param value The value to compare with.
 * @param opt The `optional` object.
 * @return `true` if greater than or equal to, `false` otherwise.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator>= (T const &value, optional<U> const &opt)
{
  return opt.has_value () ? value >= *opt : true;
}

// - Specialized algorithms -

/**
 * @brief Swaps the contents of two `optional` objects.
 * @details This is a non-member `swap` function following the standard library
 *          pattern, which calls the member `swap` function.
 * @tparam T The type of value contained in the optionals.
 * @param lhs The first `optional` object.
 * @param rhs The second `optional` object.
 * @note The `noexcept` specification depends on the member `swap` function.
 */
template <typename T>
LUMEX_OPTIONAL_CONSTEXPR_CXX20
    typename std::enable_if<std::is_move_constructible<T>::value
                            && detail::is_swappable<T>::value>::type
    swap (optional<T> &lhs, optional<T> &rhs)
        LUMEX_NOEXCEPT_IF (noexcept (lhs.swap (rhs)))
{
  lhs.swap (rhs);
}

/**
 * @brief Creates a `optional` object containing a copy or move of a
 * value.
 * @details A factory function to conveniently construct a `optional` from
 * an existing value. It uses `std::decay` to get the underlying type and
 * `std::forward` for perfect forwarding.
 * @tparam T The type of the value to store in the optional.
 * @param value The value to initialize the optional with.
 * @return A `optional` containing the provided value.
 */
template <typename T>
LUMEX_CONSTEXPR optional<typename std::decay<T>::type>
make_optional (T &&value)
{
  return optional<typename std::decay<T>::type> (std::forward<T> (value));
}

/**
 * @brief Creates a `optional` object by in-place construction.
 * @details A factory function to conveniently construct a `optional`
 *          by directly constructing its contained value using the provided
 * arguments.
 * @tparam T The type of the value to construct inside the optional.
 * @tparam Args Variadic template arguments for the constructor of `T`.
 * @param args Arguments to forward to the constructor of `T`.
 * @return A `optional` containing the newly constructed value.
 */
template <typename T, typename... Args>
LUMEX_CONSTEXPR optional<T>
make_optional (Args &&...args)
{
  return optional<T> (in_place, std::forward<Args> (args)...);
}

/**
 * @brief Creates a `optional` object by in-place construction with an
 * initializer list.
 * @details A factory function to conveniently construct a `optional`
 *          by directly constructing its contained value using an initializer
 * list and additional arguments.
 * @tparam T The type of the value to construct inside the optional.
 * @tparam U The type of elements in the `initializer_list`.
 * @tparam Args Variadic template arguments for the constructor of `T`.
 * @param ilist An `std::initializer_list` to pass to the constructor of `T`.
 * @param args Additional arguments to forward to the constructor of `T`.
 * @return A `optional` containing the newly constructed value.
 */
template <typename T, typename U, typename... Args>
LUMEX_CONSTEXPR optional<T>
make_optional (std::initializer_list<U> ilist, Args &&...args)
{
  return optional<T> (in_place, ilist, std::forward<Args> (args)...);
}

#if __cplusplus >= 201703L
// Comparisons of an `optional` with a `std::optional` (C++17). Without them
// `lumex_opt == std_opt` would pick the comparison with a value above and
// compare `T` with the whole `std::optional`, which is false for two empty
// optionals. Both operand types are named, so these are the most specialized
// candidates in both orders and no conversion is needed. The result follows
// the optional against optional rules of the standard.

/**
 * @brief `==` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Equal: both empty, or both hold equal values.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator== (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? false
             : (!lhs.has_value () || *lhs == *rhs);
}

/**
 * @brief `==` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Equal: both empty, or both hold equal values.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator== (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? false
             : (!lhs.has_value () || *lhs == *rhs);
}

/**
 * @brief `!=` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Not equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator!= (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? true
             : (lhs.has_value () && *lhs != *rhs);
}

/**
 * @brief `!=` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Not equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator!= (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return lhs.has_value () != rhs.has_value ()
             ? true
             : (lhs.has_value () && *lhs != *rhs);
}

/**
 * @brief `<` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Less: empty is less than any value.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator< (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return rhs.has_value () && (!lhs.has_value () || *lhs < *rhs);
}

/**
 * @brief `<` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Less: empty is less than any value.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator< (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return rhs.has_value () && (!lhs.has_value () || *lhs < *rhs);
}

/**
 * @brief `<=` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Less or equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator<= (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return !lhs.has_value () || (rhs.has_value () && *lhs <= *rhs);
}

/**
 * @brief `<=` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Less or equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator<= (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return !lhs.has_value () || (rhs.has_value () && *lhs <= *rhs);
}

/**
 * @brief `>` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Greater.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator> (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return lhs.has_value () && (!rhs.has_value () || *lhs > *rhs);
}

/**
 * @brief `>` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Greater.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator> (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return lhs.has_value () && (!rhs.has_value () || *lhs > *rhs);
}

/**
 * @brief `>=` between an `optional` (left) and a `std::optional` (right),
 * C++17.
 * @return Greater or equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator>= (optional<T> const &lhs, std::optional<U> const &rhs)
{
  return !rhs.has_value () || (lhs.has_value () && *lhs >= *rhs);
}

/**
 * @brief `>=` between a `std::optional` (left) and an `optional` (right),
 * C++17.
 * @return Greater or equal.
 */
template <typename T, typename U>
LUMEX_CONSTEXPR bool
operator>= (std::optional<T> const &lhs, optional<U> const &rhs)
{
  return !rhs.has_value () || (lhs.has_value () && *lhs >= *rhs);
}

#endif

} // namespace opt
} // namespace optional
} // namespace core
} // namespace lumex

// Hash support. Specialize on the Lumex type with a global qualifier: inside
// namespace std, bare `optional<T>` would bind to `std::optional` and redefine
// the STL specialization (MSVC C2953).
namespace std
{
/**
 * @brief Specialization of `std::hash` for Lumex `optional`.
 * @details Enables Lumex `optional` to be used as a key in hash-based
 * containers like `std::unordered_map` or `std::unordered_set`. The hash value
 * is 0 if the optional is empty, otherwise it's the hash of the contained
 * value.
 * @tparam T The type of the value contained in the `optional`.
 * `std::hash<T>` must be defined for this specialization to work.
 */
template <typename T> struct hash<::lumex::core::optional::opt::optional<T>>
{
  /**
   * @brief Computes the hash value for a Lumex `optional` object.
   * @param opt The `optional` object to hash.
   * @return The hash value.
   */
  std::size_t
  operator() (::lumex::core::optional::opt::optional<T> const &opt) const
  {
    return opt.has_value () ? hash<T>{}(*opt) : 0;
  }
};
} // namespace std

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_OPTIONAL_OPT_HPP

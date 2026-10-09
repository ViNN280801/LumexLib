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
 * @file ExpectedStorage.hpp
 * @brief Internal storage of `expected`: the union of the two alternatives,
 * and the layers that give `expected` the special member functions the
 * standard describes.
 * @details Everything here lives in `lumex::core::expected::result::detail`
 * and is not an interface for consumers. `expected<T, E>` and
 * `expected<void, E>` ([expected.object.general], [expected.void.general])
 * derive from `expected_base<T, E>` (for `void` with the empty class `Unit` as
 * `T`), which is a chain of class templates, one layer for each special member
 * function whose existence and triviality the standard fixes:
 * - `expected_storage`: the union `{ val, unex }` and `has_val` of
 *   [expected.object.general], the constructors that start one alternative,
 *   and the algorithms the layers above and `expected` share
 *   (`reinit-expected` of [expected.object.assign], the swap of
 *   [expected.object.swap]);
 * - `expected_destruct_base`: the destructor, trivial when `T` and `E` are
 *   trivially destructible ([expected.object.dtor]/2);
 * - `expected_copy_base`, `expected_move_base`, `expected_copy_assign_base`,
 *   `expected_move_assign_base`: the copy and move constructors and
 *   assignments, each defaulted (so trivial) when the standard says it is
 *   trivial ([expected.object.cons]/10 and /16, [expected.object.assign]/5 and
 *   /10) and written out otherwise.
 *
 * A member whose condition does not hold is deleted in its layer (the
 * copy constructor and the copy assignment: [expected.object.cons]/9,
 * [expected.object.assign]/4) or deleted in its layer so that the defaulted
 * member of every layer above, and of `expected`, is defined as deleted and
 * ignored by overload resolution (the move constructor and the move
 * assignment, which the standard leaves out by a Constraints element:
 * [expected.object.cons]/11, [expected.object.assign]/6, so an rvalue is
 * copied). A deleted function in a layer, unlike one added by an extra base
 * class, leaves `std::is_trivially_copyable` true for the other members that
 * are trivial, as for a class that declares them itself.
 * @note This is the technique of libstdc++ and of the MSVC STL (a layer
 * of base classes per special member function), because C++11 has no
 * `requires` and no conditionally defaulted member.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_STORAGE_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_STORAGE_HPP

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

#include <type_traits>
#include <utility>

#include "ExpectedDetail.hpp"
#include "ExpectedTypes.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
namespace detail
{
// ====================== reinit-expected ====================== //

/**
 * @brief The `reinit-expected` of [expected.object.assign]/1 for the case that
 * the construction of the new object cannot throw: destroy the old object,
 * construct the new one.
 * @tparam Movable Whether `N` is nothrow move constructible (not used here).
 * @tparam N Type of the new object.
 * @tparam O Type of the old object.
 * @tparam Args Types of the constructor arguments.
 * @param newval Storage of the new object.
 * @param oldval The old object.
 * @param args Arguments of the new object.
 */
template <typename Movable, typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit_nothrow (std::true_type /* nothrow constructible */, Movable, N &newval,
                O &oldval, Args &&...args)
{
  detail::destroy_at (detail::address_of (oldval));
  detail::construct_at (detail::address_of (newval),
                        detail::fwd<Args> (args)...);
}

/// @brief `reinit-expected`: the construction may throw, but `N` can be moved
/// without throwing: build a temporary first.
template <typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit_nothrow (std::false_type, std::true_type /* nothrow movable */,
                N &newval, O &oldval, Args &&...args)
{
  N tmp (detail::fwd<Args> (args)...);
  detail::destroy_at (detail::address_of (oldval));
  detail::construct_at (detail::address_of (newval), detail::mv (tmp));
}

/// @brief `reinit-expected`: neither can be done without throwing: save the
/// old object and put it back if the construction throws.
template <typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit_nothrow (std::false_type, std::false_type, N &newval, O &oldval,
                Args &&...args)
{
  O tmp (detail::mv (oldval));
  detail::destroy_at (detail::address_of (oldval));
  try
    {
      detail::construct_at (detail::address_of (newval),
                            detail::fwd<Args> (args)...);
    }
  catch (...)
    {
      detail::construct_at (detail::address_of (oldval), detail::mv (tmp));
      throw;
    }
}

/// @brief `reinit-expected` when the old object is the `Unit` that stands for
/// `void`: there is nothing to destroy or to put back, so the new object is
/// built directly ([expected.void.assign]/1.2, /12.1 and [expected.void.swap]
/// build `unex` with `construct_at` and nothing else).
template <typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit_dispatch (std::true_type /* old object is Unit */, N &newval, O &,
                 Args &&...args)
{
  detail::construct_at (detail::address_of (newval),
                        detail::fwd<Args> (args)...);
}

/// @brief `reinit-expected` for an old object that holds something.
template <typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit_dispatch (std::false_type, N &newval, O &oldval, Args &&...args)
{
  detail::reinit_nothrow (
      std::integral_constant<
          bool, std::is_nothrow_constructible<N, Args...>::value> (),
      std::integral_constant<bool,
                             std::is_nothrow_move_constructible<N>::value> (),
      newval, oldval, detail::fwd<Args> (args)...);
}

/// @brief `reinit-expected` (T = N, U = O): ends the life of `oldval` and
/// starts `newval` with `args...`, keeping the old object if that throws.
template <typename N, typename O, typename... Args>
LUMEX_EXPECTED_CONSTEXPR_CXX20 void
reinit (N &newval, O &oldval, Args &&...args)
{
  detail::reinit_dispatch (std::is_same<O, Unit> (), newval, oldval,
                           detail::fwd<Args> (args)...);
}

// Everything below lives in its own namespace: the namespaces of the base
// classes of `expected` are among the namespaces argument-dependent lookup
// searches for a call that has an `expected` argument, and `detail` has
// function templates (`construct_at`, `destroy_at`, ...) that must not be
// candidates of a call in the program that uses `expected`.
namespace storage
{
// ====================== Checks of the template arguments
// ====================== //

/**
 * @brief The requirements on the types of `expected<T, E>`
 * ([expected.object.general]/2, [expected.un.general]/2), as `static_assert`s.
 * @details The base class of the storage (so that these messages come before
 * any error the union would give for the same type, a union cannot hold a
 * reference, and `expected` has one base only: a compiler that does not
 * share the storage of several empty bases would make the object bigger).
 * @tparam T Value type.
 * @tparam E Error type.
 */
template <typename T, typename E> struct expected_checks
{
  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<T>::value,
                           "Expected<T,E>: T must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<T>::value,
                           "Expected<T,E>: T must not be a function type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<T>::value,
                           "Expected<T,E>: T must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_void<T>::value,
                           "Expected<T,E>: T must not be cv void; "
                           "use Expected<void,E>");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::remove_cv<T>::type, in_place_tag>::value,
      "Expected<T,E>: T must not be in_place_tag");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_same<typename std::remove_cv<T>::type, unexpect_t>::value,
      "Expected<T,E>: T must not be unexpect_t");
  LUMEX_STATIC_ASSERT_MSG (
      !detail::is_unexpected<typename std::remove_cv<T>::type>::value,
      "Expected<T,E>: T must not be a specialization of unexpected");

  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<E>::value,
                           "Expected<T,E>: E must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<E>::value,
                           "Expected<T,E>: E must not be a function type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_void<E>::value,
                           "Expected<T,E>: E must not be void");
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<E>::value,
                           "Expected<T,E>: E must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_const<E>::value
                               && !std::is_volatile<E>::value,
                           "Expected<T,E>: E must not be cv-qualified");
  LUMEX_STATIC_ASSERT_MSG (
      !detail::is_unexpected<E>::value,
      "Expected<T,E>: E must not be a specialization of unexpected");
};

/**
 * @brief The requirements on the error type of `expected<void, E>`
 * ([expected.void.general]/2), as `static_assert`s.
 * @tparam E Error type.
 */
template <typename E> struct expected_void_checks
{
  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<E>::value,
                           "Expected<void,E>: E must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<E>::value,
                           "Expected<void,E>: E must not be a function type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_void<E>::value,
                           "Expected<void,E>: E must not be void");
  LUMEX_STATIC_ASSERT_MSG (!std::is_array<E>::value,
                           "Expected<void,E>: E must not be an array type");
  LUMEX_STATIC_ASSERT_MSG (!std::is_const<E>::value
                               && !std::is_volatile<E>::value,
                           "Expected<void,E>: E must not be cv-qualified");
  LUMEX_STATIC_ASSERT_MSG (
      !detail::is_unexpected<E>::value,
      "Expected<void,E>: E must not be a specialization of unexpected");
};

/// The checks for the value type `T`: those of `expected<void, E>` for the
/// `Unit` that stands for `void`, those of `expected<T, E>` otherwise.
template <typename T, typename E>
using checks_of = typename std::conditional<std::is_same<T, Unit>::value,
                                            expected_void_checks<E>,
                                            expected_checks<T, E>>::type;

// ====================== Properties of T and E ====================== //

/**
 * @brief The conditions the standard puts on the special member functions of
 * `expected<T, E>`, evaluated once for a pair of types.
 * @details `T` is the value type as written (possibly `const`), as the
 * standard writes `is_copy_constructible_v<T>`; `expected<void, E>` uses
 * `Unit` for `T`, for which every condition holds, so the conditions of
 * [expected.void] follow from those of [expected.object] with nothing left of
 * `T`.
 * @tparam T Value type (`Unit` for `void`).
 * @tparam E Error type.
 */
template <typename T, typename E> struct expected_traits
{
  /// @brief [expected.object.dtor]/2.
  using trivial_destructor = std::integral_constant<
      bool, std::is_trivially_destructible<T>::value
                && std::is_trivially_destructible<E>::value>;

  /// @brief [expected.object.cons]/9: the copy constructor exists.
  using copy_constructible
      = std::integral_constant<bool,
                               std::is_copy_constructible<T>::value
                                   && std::is_copy_constructible<E>::value>;

  /// @brief The copy constructor cannot throw (not required by the standard,
  /// which declares it without an exception specification;
  /// [res.on.exception.handling]/5 lets an implementation add one, and the
  /// standard libraries do).
  using nothrow_copy_constructible = std::integral_constant<
      bool, std::is_nothrow_copy_constructible<T>::value
                && std::is_nothrow_copy_constructible<E>::value>;

  /// @brief [expected.object.cons]/10: the copy constructor is trivial.
  using trivially_copy_constructible = std::integral_constant<
      bool, std::is_trivially_copy_constructible<T>::value
                && std::is_trivially_copy_constructible<E>::value>;

  /// @brief [expected.object.cons]/11: the move constructor exists.
  using move_constructible
      = std::integral_constant<bool,
                               std::is_move_constructible<T>::value
                                   && std::is_move_constructible<E>::value>;

  /// @brief [expected.object.cons]/15: the move constructor is `noexcept`.
  using nothrow_move_constructible = std::integral_constant<
      bool, std::is_nothrow_move_constructible<T>::value
                && std::is_nothrow_move_constructible<E>::value>;

  /// @brief [expected.object.cons]/16: the move constructor is trivial.
  using trivially_move_constructible = std::integral_constant<
      bool, std::is_trivially_move_constructible<T>::value
                && std::is_trivially_move_constructible<E>::value>;

  /// @brief [expected.object.assign]/4: the copy assignment exists.
  using copy_assignable = std::integral_constant<
      bool, std::is_copy_assignable<T>::value
                && std::is_copy_constructible<T>::value
                && std::is_copy_assignable<E>::value
                && std::is_copy_constructible<E>::value
                && (std::is_nothrow_move_constructible<T>::value
                    || std::is_nothrow_move_constructible<E>::value)>;

  /// @brief The copy assignment cannot throw (an exception specification the
  /// standard does not give, added as above).
  using nothrow_copy_assignable = std::integral_constant<
      bool, std::is_nothrow_copy_constructible<T>::value
                && std::is_nothrow_copy_assignable<T>::value
                && std::is_nothrow_copy_constructible<E>::value
                && std::is_nothrow_copy_assignable<E>::value>;

  /// @brief [expected.object.assign]/5: the copy assignment is trivial.
  using trivially_copy_assignable = std::integral_constant<
      bool, std::is_trivially_copy_constructible<T>::value
                && std::is_trivially_copy_assignable<T>::value
                && std::is_trivially_destructible<T>::value
                && std::is_trivially_copy_constructible<E>::value
                && std::is_trivially_copy_assignable<E>::value
                && std::is_trivially_destructible<E>::value>;

  /// @brief [expected.object.assign]/6: the move assignment exists.
  using move_assignable = std::integral_constant<
      bool, std::is_move_constructible<T>::value
                && std::is_move_assignable<T>::value
                && std::is_move_constructible<E>::value
                && std::is_move_assignable<E>::value
                && (std::is_nothrow_move_constructible<T>::value
                    || std::is_nothrow_move_constructible<E>::value)>;

  /// @brief [expected.object.assign]/9: the move assignment is `noexcept`.
  using nothrow_move_assignable = std::integral_constant<
      bool, std::is_nothrow_move_assignable<T>::value
                && std::is_nothrow_move_constructible<T>::value
                && std::is_nothrow_move_assignable<E>::value
                && std::is_nothrow_move_constructible<E>::value>;

  /// @brief [expected.object.assign]/10: the move assignment is trivial.
  using trivially_move_assignable = std::integral_constant<
      bool, std::is_trivially_move_constructible<T>::value
                && std::is_trivially_move_assignable<T>::value
                && std::is_trivially_destructible<T>::value
                && std::is_trivially_move_constructible<E>::value
                && std::is_trivially_move_assignable<E>::value
                && std::is_trivially_destructible<E>::value>;

  /// @brief What a layer does with its special member function: 0 leaves it
  /// to the compiler (trivial), 1 writes it out, 2 deletes it.
  template <bool Exists, bool Trivial>
  using member_kind
      = std::integral_constant<int, !Exists ? 2 : (Trivial ? 0 : 1)>;

  /// @brief The layer of the copy constructor.
  using copy_constructor_kind
      = member_kind<copy_constructible::value,
                    trivially_copy_constructible::value>;

  /// @brief The layer of the move constructor.
  using move_constructor_kind
      = member_kind<move_constructible::value,
                    trivially_move_constructible::value>;

  /// @brief The layer of the copy assignment.
  using copy_assignment_kind
      = member_kind<copy_assignable::value, trivially_copy_assignable::value>;

  /// @brief The layer of the move assignment.
  using move_assignment_kind
      = member_kind<move_assignable::value, trivially_move_assignable::value>;
};

// ====================== The union ====================== //

/// @brief The alternative of the union that holds nothing.
struct empty_alternative
{
};

/**
 * @brief The anonymous union of [expected.object.general]
 * (`union { remove_cv_t<T> val; E unex; }`), given a name so that its
 * constructors can start either alternative in a mem-initializer, which is
 * what lets `expected` be constructed in a constant expression from C++11.
 * @details The union is trivially destructible when both alternatives are; it
 * has a destructor that does nothing otherwise, because which alternative to
 * destroy is known only to the `expected` around it. It starts with a third
 * alternative that holds nothing, for the constructors that build the
 * alternative in their body.
 * @tparam V Value type without cv-qualifiers.
 * @tparam E Error type.
 * @tparam TrivialDestructor Both alternatives are trivially destructible.
 */
template <typename V, typename E, bool TrivialDestructor> union expected_union;

/// @brief `expected_union` with a trivial destructor.
template <typename V, typename E> union expected_union<V, E, true>
{
  empty_alternative m_empty; ///< Active before the body of a constructor ran.
  V m_value;                 ///< `val`.
  E m_error;                 ///< `unex`.

  /// @brief Holds nothing yet.
  LUMEX_CONSTEXPR_CTOR
  expected_union () : m_empty () {}

  /// @brief Starts `val`, direct-non-list-initialized with `args...`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_union (value_tag, Args &&...args)
      : m_value (detail::fwd<Args> (args)...)
  {
  }

  /// @brief Starts `unex`, direct-non-list-initialized with `args...`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_union (error_tag, Args &&...args)
      : m_error (detail::fwd<Args> (args)...)
  {
  }

  /// @brief Starts `val` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_union (invoke_value_tag, Fn &&fn, Args &&...args)
      : m_value (detail::invoke_call (detail::fwd<Fn> (fn),
                                      detail::fwd<Args> (args)...))
  {
  }

  /// @brief Starts `unex` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_union (invoke_error_tag, Fn &&fn, Args &&...args)
      : m_error (detail::invoke_call (detail::fwd<Fn> (fn),
                                      detail::fwd<Args> (args)...))
  {
  }
};

/// @brief `expected_union` with a destructor that does nothing.
template <typename V, typename E> union expected_union<V, E, false>
{
  empty_alternative m_empty; ///< Active before the body of a constructor ran.
  V m_value;                 ///< `val`.
  E m_error;                 ///< `unex`.

  /// @brief Holds nothing yet.
  LUMEX_CONSTEXPR_CTOR
  expected_union () : m_empty () {}

  /// @brief Starts `val`, direct-non-list-initialized with `args...`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_union (value_tag, Args &&...args)
      : m_value (detail::fwd<Args> (args)...)
  {
  }

  /// @brief Starts `unex`, direct-non-list-initialized with `args...`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_union (error_tag, Args &&...args)
      : m_error (detail::fwd<Args> (args)...)
  {
  }

  /// @brief Starts `val` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_union (invoke_value_tag, Fn &&fn, Args &&...args)
      : m_value (detail::invoke_call (detail::fwd<Fn> (fn),
                                      detail::fwd<Args> (args)...))
  {
  }

  /// @brief Starts `unex` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_union (invoke_error_tag, Fn &&fn, Args &&...args)
      : m_error (detail::invoke_call (detail::fwd<Fn> (fn),
                                      detail::fwd<Args> (args)...))
  {
  }

  /// @brief Does nothing: `expected` destroys the active alternative.
  LUMEX_CONSTEXPR_DTOR ~expected_union () {}
};

// ====================== Value categories of the source ======================
// //

/// @brief The type the contents of an `expected` are read as when it is
/// reached through `Rhs`: `T const &` for an lvalue reference to a const
/// object, `T &&` for an rvalue.
template <typename Rhs, typename T>
using source_of =
    typename std::conditional<std::is_lvalue_reference<Rhs>::value, T const &,
                              T &&>::type;

// ====================== The storage ====================== //

/**
 * @brief `has_val` and the union of [expected.object.general], the
 * constructors that start one alternative, and the algorithms that move
 * contents between two storages.
 * @tparam T Value type as written (possibly `const`, `Unit` for `void`).
 * @tparam E Error type.
 */
template <typename T, typename E> struct expected_storage : checks_of<T, E>
{
  /// @brief The type of `val`: `remove_cv_t<T>`.
  using val_type = typename std::remove_cv<T>::type;

  /// @brief The union `{ val, unex }`.
  using union_type
      = expected_union<val_type, E,
                       expected_traits<T, E>::trivial_destructor::value>;

  union_type m_storage; ///< `val` or `unex`.
  bool m_has_value;     ///< `has_val`.

  /// @brief Starts `val` with `args...`; `has_value ()` is `true`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_storage (value_tag, Args &&...args)
      : m_storage (value_tag (), detail::fwd<Args> (args)...),
        m_has_value (true)
  {
  }

  /// @brief Starts `unex` with `args...`; `has_value ()` is `false`.
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit expected_storage (error_tag, Args &&...args)
      : m_storage (error_tag (), detail::fwd<Args> (args)...),
        m_has_value (false)
  {
  }

  /// @brief Starts `val` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_storage (invoke_value_tag tag, Fn &&fn, Args &&...args)
      : m_storage (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...),
        m_has_value (true)
  {
  }

  /// @brief Starts `unex` with the result of `invoke_call (fn, args...)`.
  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  expected_storage (invoke_error_tag tag, Fn &&fn, Args &&...args)
      : m_storage (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...),
        m_has_value (false)
  {
  }

  /**
   * @brief Copies or moves the alternative of another storage of the same
   * type: [expected.object.cons]/6 and /12.
   * @tparam Rhs `Layer const &` (copy) or `Layer` (move) where `Layer` is a
   * class derived from `expected_storage`.
   * @param rhs The source.
   */
  template <typename Rhs>
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected_storage (from_storage_tag, Rhs &&rhs)
      : m_storage (), m_has_value (rhs.m_has_value)
  {
    if (m_has_value)
      detail::construct_at (
          detail::address_of (m_storage.m_value),
          static_cast<source_of<Rhs, T>> (rhs.m_storage.m_value));
    else
      detail::construct_at (
          detail::address_of (m_storage.m_error),
          static_cast<source_of<Rhs, E>> (rhs.m_storage.m_error));
  }

  /**
   * @brief Converts the alternative of an `expected<U, G>`: the converting
   * constructors of [expected.object.cons]/19 and [expected.void.cons]/14.
   * @details The source is read through its observers, so it can be an
   * `expected` of any type.
   * @tparam Rhs `expected<U, G> const &` or `expected<U, G>`.
   * @param rhs The source.
   */
  template <typename Rhs>
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected_storage (convert_tag, Rhs &&rhs)
      : m_storage (), m_has_value (rhs.has_value ())
  {
    if (m_has_value)
      init_value (std::is_same<val_type, Unit> (), detail::fwd<Rhs> (rhs));
    else
      detail::construct_at (detail::address_of (m_storage.m_error),
                            detail::fwd<Rhs> (rhs).error ());
  }

  /// @brief The conversion of a value from a source (the class `val_type`
  /// is built from `*rhs`).
  template <typename Rhs>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  init_value (std::false_type, Rhs &&rhs)
  {
    detail::construct_at (detail::address_of (m_storage.m_value),
                          *detail::fwd<Rhs> (rhs));
  }

  /// @brief `Unit` stands for `void`: there is no value to read.
  template <typename Rhs>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  init_value (std::true_type, Rhs &&)
  {
    detail::construct_at (detail::address_of (m_storage.m_value));
  }

  /**
   * @brief Assigns the contents of another storage of the same type:
   * [expected.object.assign]/2 and /7.
   * @tparam Rhs `Layer const &` (copy) or `Layer` (move).
   * @param rhs The source.
   */
  template <typename Rhs>
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  assign_from (Rhs &&rhs)
  {
    if (m_has_value && rhs.m_has_value)
      m_storage.m_value
          = static_cast<source_of<Rhs, T>> (rhs.m_storage.m_value);
    else if (m_has_value)
      {
        detail::reinit (
            m_storage.m_error, m_storage.m_value,
            static_cast<source_of<Rhs, E>> (rhs.m_storage.m_error));
        m_has_value = false;
      }
    else if (rhs.m_has_value)
      {
        detail::reinit (
            m_storage.m_value, m_storage.m_error,
            static_cast<source_of<Rhs, T>> (rhs.m_storage.m_value));
        m_has_value = true;
      }
    else
      m_storage.m_error
          = static_cast<source_of<Rhs, E>> (rhs.m_storage.m_error);
  }

  /// @brief How a value and an error are exchanged: 0 for `Unit` (the
  /// `void` specialization moves the error only), 1 when `E` is nothrow move
  /// constructible, 2 otherwise.
  using swap_mode = std::integral_constant<
      int, std::is_same<val_type, Unit>::value
               ? 0
               : (std::is_nothrow_move_constructible<E>::value ? 1 : 2)>;

  /**
   * @brief Exchanges the contents with another storage: Table 72 of
   * [expected.object.swap] and Table 73 of [expected.void.swap].
   * @param rhs The other storage.
   */
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap_storage (expected_storage &rhs)
  {
    if (m_has_value)
      {
        if (rhs.m_has_value)
          {
            using std::swap;
            swap (m_storage.m_value, rhs.m_storage.m_value);
          }
        else
          swap_value_with_error (swap_mode (), rhs);
      }
    else
      {
        if (rhs.m_has_value)
          rhs.swap_value_with_error (swap_mode (), *this);
        else
          {
            using std::swap;
            swap (m_storage.m_error, rhs.m_storage.m_error);
          }
      }
  }

  /// @brief `this` is a success and `rhs` holds an error, for `expected<void,
  /// E>`: [expected.void.swap]: the error is moved across.
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap_value_with_error (std::integral_constant<int, 0>, expected_storage &rhs)
  {
    detail::construct_at (detail::address_of (m_storage.m_error),
                          detail::mv (rhs.m_storage.m_error));
    detail::destroy_at (detail::address_of (rhs.m_storage.m_error));
    detail::construct_at (detail::address_of (rhs.m_storage.m_value));
    m_has_value = false;
    rhs.m_has_value = true;
  }

  /// @brief `this` holds a value, `rhs` an error, and `E` is nothrow move
  /// constructible: park the error, move the value across, put the error
  /// back ([expected.object.swap], Table 72).
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap_value_with_error (std::integral_constant<int, 1>, expected_storage &rhs)
  {
    E tmp (detail::mv (rhs.m_storage.m_error));
    detail::destroy_at (detail::address_of (rhs.m_storage.m_error));
    try
      {
        detail::construct_at (detail::address_of (rhs.m_storage.m_value),
                              detail::mv (m_storage.m_value));
        detail::destroy_at (detail::address_of (m_storage.m_value));
        detail::construct_at (detail::address_of (m_storage.m_error),
                              detail::mv (tmp));
      }
    catch (...)
      {
        detail::construct_at (detail::address_of (rhs.m_storage.m_error),
                              detail::mv (tmp));
        throw;
      }
    m_has_value = false;
    rhs.m_has_value = true;
  }

  /// @brief As above, but `E` may throw when moved: park the value instead.
  LUMEX_EXPECTED_CONSTEXPR_CXX20 void
  swap_value_with_error (std::integral_constant<int, 2>, expected_storage &rhs)
  {
    val_type tmp (detail::mv (m_storage.m_value));
    detail::destroy_at (detail::address_of (m_storage.m_value));
    try
      {
        detail::construct_at (detail::address_of (m_storage.m_error),
                              detail::mv (rhs.m_storage.m_error));
        detail::destroy_at (detail::address_of (rhs.m_storage.m_error));
        detail::construct_at (detail::address_of (rhs.m_storage.m_value),
                              detail::mv (tmp));
      }
    catch (...)
      {
        detail::construct_at (detail::address_of (m_storage.m_value),
                              detail::mv (tmp));
        throw;
      }
    m_has_value = false;
    rhs.m_has_value = true;
  }
};

// ====================== The destructor ====================== //

/// @brief The layer that destroys the active alternative; trivial.
template <typename T, typename E,
          bool Trivial = expected_traits<T, E>::trivial_destructor::value>
struct expected_destruct_base : expected_storage<T, E>
{
  using storage_type = expected_storage<T, E>;
  using storage_type::storage_type;
};

/// @brief The layer that destroys the active alternative:
/// [expected.object.dtor]/1.
template <typename T, typename E>
struct expected_destruct_base<T, E, false> : expected_storage<T, E>
{
  using storage_type = expected_storage<T, E>;
  using storage_type::storage_type;

  expected_destruct_base (expected_destruct_base const &) = default;
  expected_destruct_base (expected_destruct_base &&) = default;
  expected_destruct_base &operator= (expected_destruct_base const &) = default;
  expected_destruct_base &operator= (expected_destruct_base &&) = default;

  /// @brief Destroys `val` if `has_value ()`, otherwise `unex`.
  LUMEX_CONSTEXPR_DTOR ~expected_destruct_base ()
  {
    if (this->m_has_value)
      detail::destroy_at (detail::address_of (this->m_storage.m_value));
    else
      detail::destroy_at (detail::address_of (this->m_storage.m_error));
  }
};

// ====================== The copy constructor ====================== //

/// @brief The layer of the copy constructor. `Kind` is 0 (the compiler's
/// constructor, trivial when the layer below is: [expected.object.cons]/10),
/// 1 (written out) or 2 (deleted: [expected.object.cons]/9). The other three
/// members are defaulted, so they stay what the layers below make them.
template <typename T, typename E,
          int Kind = expected_traits<T, E>::copy_constructor_kind::value>
struct expected_copy_base : expected_destruct_base<T, E>
{
  using base_type = expected_destruct_base<T, E>;
  using base_type::base_type;
};

/// @brief The copy constructor is written out: [expected.object.cons]/6.
template <typename T, typename E>
struct expected_copy_base<T, E, 1> : expected_destruct_base<T, E>
{
  using base_type = expected_destruct_base<T, E>;
  using base_type::base_type;

  /// @brief Copies the alternative of `rhs`.
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected_copy_base (expected_copy_base const &rhs) LUMEX_NOEXCEPT_IF (
      expected_traits<T, E>::nothrow_copy_constructible::value)
      : base_type (from_storage_tag (), rhs)
  {
  }

  expected_copy_base (expected_copy_base &&) = default;
  expected_copy_base &operator= (expected_copy_base const &) = default;
  expected_copy_base &operator= (expected_copy_base &&) = default;
};

/// @brief The copy constructor is deleted.
template <typename T, typename E>
struct expected_copy_base<T, E, 2> : expected_destruct_base<T, E>
{
  using base_type = expected_destruct_base<T, E>;
  using base_type::base_type;

  expected_copy_base (expected_copy_base const &) = delete;
  expected_copy_base (expected_copy_base &&) = default;
  expected_copy_base &operator= (expected_copy_base const &) = default;
  expected_copy_base &operator= (expected_copy_base &&) = default;
};

// ====================== The move constructor ====================== //

/// @brief The layer of the move constructor. `Kind` is 0 (the compiler's
/// constructor, trivial when the layer below is: [expected.object.cons]/16),
/// 1 (written out) or 2 (deleted, which makes the defaulted move constructor
/// of every layer above, and of `expected`, defined as deleted, and overload
/// resolution ignores such a constructor: [expected.object.cons]/11 leaves it
/// out by a Constraints element, so an rvalue is copied).
template <typename T, typename E,
          int Kind = expected_traits<T, E>::move_constructor_kind::value>
struct expected_move_base : expected_copy_base<T, E>
{
  using base_type = expected_copy_base<T, E>;
  using base_type::base_type;
};

/// @brief The move constructor is written out: [expected.object.cons]/12 and
/// /15.
template <typename T, typename E>
struct expected_move_base<T, E, 1> : expected_copy_base<T, E>
{
  using base_type = expected_copy_base<T, E>;
  using base_type::base_type;

  /// @brief Moves the alternative of `rhs`.
  LUMEX_EXPECTED_CONSTEXPR_CXX20
  expected_move_base (expected_move_base &&rhs) LUMEX_NOEXCEPT_IF (
      expected_traits<T, E>::nothrow_move_constructible::value)
      : base_type (from_storage_tag (), detail::mv (rhs))
  {
  }

  expected_move_base (expected_move_base const &) = default;
  expected_move_base &operator= (expected_move_base const &) = default;
  expected_move_base &operator= (expected_move_base &&) = default;
};

/// @brief The move constructor is deleted (left out).
template <typename T, typename E>
struct expected_move_base<T, E, 2> : expected_copy_base<T, E>
{
  using base_type = expected_copy_base<T, E>;
  using base_type::base_type;

  expected_move_base (expected_move_base const &) = default;
  expected_move_base (expected_move_base &&) = delete;
  expected_move_base &operator= (expected_move_base const &) = default;
  expected_move_base &operator= (expected_move_base &&) = default;
};

// ====================== The copy assignment ====================== //

/// @brief The layer of the copy assignment. `Kind` is 0 (the compiler's
/// operator, trivial when the layer below is: [expected.object.assign]/5),
/// 1 (written out) or 2 (deleted: [expected.object.assign]/4).
template <typename T, typename E,
          int Kind = expected_traits<T, E>::copy_assignment_kind::value>
struct expected_copy_assign_base : expected_move_base<T, E>
{
  using base_type = expected_move_base<T, E>;
  using base_type::base_type;
};

/// @brief The copy assignment is written out: [expected.object.assign]/2.
template <typename T, typename E>
struct expected_copy_assign_base<T, E, 1> : expected_move_base<T, E>
{
  using base_type = expected_move_base<T, E>;
  using base_type::base_type;

  expected_copy_assign_base (expected_copy_assign_base const &) = default;
  expected_copy_assign_base (expected_copy_assign_base &&) = default;
  expected_copy_assign_base &operator= (expected_copy_assign_base &&)
      = default;

  /// @brief Assigns the contents of `rhs`.
  LUMEX_EXPECTED_CONSTEXPR_CXX20 expected_copy_assign_base &
  operator= (expected_copy_assign_base const &rhs)
      LUMEX_NOEXCEPT_IF (expected_traits<T, E>::nothrow_copy_assignable::value)
  {
    this->assign_from (rhs);
    return *this;
  }
};

/// @brief The copy assignment is deleted.
template <typename T, typename E>
struct expected_copy_assign_base<T, E, 2> : expected_move_base<T, E>
{
  using base_type = expected_move_base<T, E>;
  using base_type::base_type;

  expected_copy_assign_base (expected_copy_assign_base const &) = default;
  expected_copy_assign_base (expected_copy_assign_base &&) = default;
  expected_copy_assign_base &operator= (expected_copy_assign_base const &)
      = delete;
  expected_copy_assign_base &operator= (expected_copy_assign_base &&)
      = default;
};

// ====================== The move assignment ====================== //

/// @brief The layer of the move assignment. `Kind` is 0 (the compiler's
/// operator, trivial when the layer below is: [expected.object.assign]/10),
/// 1 (written out) or 2 (deleted, so ignored by overload resolution as the
/// move constructor above: [expected.object.assign]/6).
template <typename T, typename E,
          int Kind = expected_traits<T, E>::move_assignment_kind::value>
struct expected_move_assign_base : expected_copy_assign_base<T, E>
{
  using base_type = expected_copy_assign_base<T, E>;
  using base_type::base_type;
};

/// @brief The move assignment is written out: [expected.object.assign]/7 and
/// /9.
template <typename T, typename E>
struct expected_move_assign_base<T, E, 1> : expected_copy_assign_base<T, E>
{
  using base_type = expected_copy_assign_base<T, E>;
  using base_type::base_type;

  expected_move_assign_base (expected_move_assign_base const &) = default;
  expected_move_assign_base (expected_move_assign_base &&) = default;
  expected_move_assign_base &operator= (expected_move_assign_base const &)
      = default;

  /// @brief Assigns the contents of `rhs` by move.
  LUMEX_EXPECTED_CONSTEXPR_CXX20 expected_move_assign_base &
  operator= (expected_move_assign_base &&rhs)
      LUMEX_NOEXCEPT_IF (expected_traits<T, E>::nothrow_move_assignable::value)
  {
    this->assign_from (detail::mv (rhs));
    return *this;
  }
};

/// @brief The move assignment is deleted (left out).
template <typename T, typename E>
struct expected_move_assign_base<T, E, 2> : expected_copy_assign_base<T, E>
{
  using base_type = expected_copy_assign_base<T, E>;
  using base_type::base_type;

  expected_move_assign_base (expected_move_assign_base const &) = default;
  expected_move_assign_base (expected_move_assign_base &&) = default;
  expected_move_assign_base &operator= (expected_move_assign_base const &)
      = default;
  expected_move_assign_base &operator= (expected_move_assign_base &&) = delete;
};

/// @brief The chain of layers `expected<T, E>` is built on.
template <typename T, typename E>
using expected_base = expected_move_assign_base<T, E>;

} // namespace storage
} // namespace detail
} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_STORAGE_HPP

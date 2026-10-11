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
 * @file LumexOptionalDetail.hpp
 * @brief Internal machinery of `optional`: the storage union, the layers that
 * give `optional` the special member functions the standard describes, an
 * `INVOKE` for C++11 and the traits the converting operations use.
 * @details Everything here lives in `lumex::core::optional::opt::detail` and
 * is not an interface for consumers. `optional<T>` holds, as its only
 * member, the last of a chain of class templates, one layer for each special
 * member function whose existence and triviality the standard fixes
 * ([optional.ctor], [optional.dtor], [optional.assign]):
 * - `opt_destruct_base`: the union `{ dummy, value }`, the engaged flag, the
 *   in-place constructors and the destructor, trivial when `T` is trivially
 *   destructible ([optional.dtor]/2);
 * - `opt_copy_ctor_base`, `opt_move_ctor_base`, `opt_copy_assign_base`,
 *   `opt_move_assign_base`: each defaulted (so trivial) when the standard
 *   says it is trivial, written out otherwise, and deleted in its layer when
 *   `T` lacks the operation, so that the defaulted member of every layer
 *   above, and of `optional`, is defined as deleted (a deleted copy) or
 *   deleted and then ignored by overload resolution (a move: an rvalue is
 *   copied).
 *
 * C++11 has no `requires` and no conditionally defaulted member, so a layer
 * per special member function is the technique of the standard libraries.
 */
#ifndef LUMEX_CORE_OPTIONAL_OPT_DETAIL_HPP
#define LUMEX_CORE_OPTIONAL_OPT_DETAIL_HPP

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

#include <functional>       // For std::reference_wrapper
#include <initializer_list> // For std::initializer_list
#include <new>              // For placement new
#include <type_traits>      // For std::is_*, std::decay
#include <utility>          // For std::forward, std::move, std::swap
// <memory> is a heavy header: it is included only where it is used, for
// std::construct_at (C++20) and for std::addressof on a compiler without
// __builtin_addressof.
#if __cplusplus >= 202002L                                                    \
    || !(defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 7)           \
         || (defined(_MSC_VER) && _MSC_VER >= 1911))
#include <memory>
#endif

#include "lumex/core/optional/opt/LumexOptionalTags.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @def LUMEX_OPTIONAL_CONSTEXPR_CXX20
 * @brief `constexpr` where the language lets a constant expression start the
 * lifetime of an object in a union (`std::construct_at` on an inactive
 * member, P0784, P1330): C++20 with a `constexpr` of 201907L and
 * `std::construct_at`; empty before.
 * @details The members that change the active member of the union (a
 * non-trivial copy or move, assignment from a value or an `optional`,
 * `emplace`, `swap`, the non-trivial `reset`) are marked with it. The
 * constructors, the observers and the trivial special members are `constexpr`
 * from C++11 and the trivial `reset` from C++14.
 */
#if __cplusplus >= 202002L && defined(__cpp_constexpr)                        \
    && __cpp_constexpr >= 201907L                                             \
    && defined(__cpp_lib_constexpr_dynamic_alloc)                             \
    && __cpp_lib_constexpr_dynamic_alloc >= 201907L
#define LUMEX_OPTIONAL_CONSTEXPR_CXX20 constexpr
#define LUMEX_OPTIONAL_HAS_CONSTRUCT_AT 1
#else
#define LUMEX_OPTIONAL_CONSTEXPR_CXX20
#define LUMEX_OPTIONAL_HAS_CONSTRUCT_AT 0
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace optional
{
namespace opt
{
template <typename T> class optional;

namespace detail
{
// ====================== Language helpers ====================== //

/// @brief `std::remove_cvref_t` of C++20.
template <typename T>
using remove_cvref_t =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

/// @brief The CWG 1558 safe `void_t`.
template <typename... Ts> struct make_void
{
  using type = void;
};

/// @brief `std::void_t` of C++17.
template <typename... Ts> using void_t = typename make_void<Ts...>::type;

/// @brief `static_cast<T &&> (t)`: `std::forward` is not `constexpr` in C++11.
template <typename T>
LUMEX_CONSTEXPR T &&
fwd (typename std::remove_reference<T>::type &t) LUMEX_NOEXCEPT
{
  return static_cast<T &&> (t);
}

/// @brief `static_cast<remove_reference_t<T> &&> (t)`: `std::move` is not
/// `constexpr` in C++11.
template <typename T>
LUMEX_CONSTEXPR typename std::remove_reference<T>::type &&
mv (T &&t) LUMEX_NOEXCEPT
{
  return static_cast<typename std::remove_reference<T>::type &&> (t);
}

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 7)                \
    || (defined(_MSC_VER) && _MSC_VER >= 1911)
#define LUMEX_OPTIONAL_ADDRESSOF(object) __builtin_addressof (object)
#else
#define LUMEX_OPTIONAL_ADDRESSOF(object) std::addressof (object)
#endif

/// @brief The address of `object` also when `T` overloads `operator&`, usable
/// in a constant expression from C++11.
template <typename T>
LUMEX_CONSTEXPR T *
address_of (T &object) LUMEX_NOEXCEPT
{
  return LUMEX_OPTIONAL_ADDRESSOF (object);
}

/// @brief The address of `object` as a `void *`, also for a `const` object.
template <typename T>
inline void *
voidify (T &object) LUMEX_NOEXCEPT
{
  return const_cast<void *> (
      static_cast<void const volatile *> (detail::address_of (object)));
}

/// @brief `std::construct_at`: `constexpr` from C++20, a placement new
/// before. `T` is not `const`: the union stores the value without it.
template <typename T, typename... Args>
LUMEX_OPTIONAL_CONSTEXPR_CXX20 T *
construct_at (T *where, Args &&...args)
{
#if LUMEX_OPTIONAL_HAS_CONSTRUCT_AT
  return std::construct_at (where, detail::fwd<Args> (args)...);
#else
  return ::new (detail::voidify (*where)) T (detail::fwd<Args> (args)...);
#endif
}

/// @brief Throws `lumex_bad_optional_access`; a function, so that the
/// `constexpr` observers of C++11 can name it in a conditional expression.
LUMEX_ATTRIBUTE_NORETURN inline void
throw_bad_optional_access ()
{
  throw lumex_bad_optional_access ();
}

// ====================== Traits ====================== //

/// @brief Whether `T` is a specialization of `optional` (no cv, no reference).
template <typename T> struct is_optional_specialization : std::false_type
{
};

template <typename T>
struct is_optional_specialization<optional<T>> : std::true_type
{
};

/// @brief Whether `T` is `in_place_t` or `nullopt_t`, cv-qualified or not:
/// the types an `optional` may not hold.
template <typename T>
struct is_tag_type
    : std::integral_constant<
          bool,
          std::is_same<typename std::remove_cv<T>::type, in_place_t>::value
              || std::is_same<typename std::remove_cv<T>::type,
                              nullopt_t>::value>
{
};

/// @brief `std::conjunction` of C++17: the first false trait ends the
/// evaluation, so a later trait is not instantiated. The constraints of the
/// converting operations name the cheap exclusions first (`T` is `U`, `U` is
/// `optional`) so that a `T` whose constructor template tests
/// `is_copy_constructible<optional<T>>` (such as `std::any`) is not asked
/// about `optional<T>` while `optional<T>` is being declared.
template <typename... B> struct conjunction : std::true_type
{
};

template <typename B> struct conjunction<B> : B
{
};

template <typename B, typename... Rest>
struct conjunction<B, Rest...>
    : std::conditional<bool (B::value), conjunction<Rest...>, B>::type
{
};

/// @brief `std::negation` of C++17.
template <typename B>
struct negation : std::integral_constant<bool, !bool (B::value)>
{
};

/// @brief `converts-from-any-cvref<T, W>` of [optional.ctor]/1: `T` is
/// constructible or convertible from `W` of any of the four forms.
template <typename T, typename W>
struct converts_from_any_cvref
    : std::integral_constant<bool,
                             std::is_constructible<T, W &>::value
                                 || std::is_convertible<W &, T>::value
                                 || std::is_constructible<T, W>::value
                                 || std::is_convertible<W, T>::value
                                 || std::is_constructible<T, W const &>::value
                                 || std::is_convertible<W const &, T>::value
                                 || std::is_constructible<T, W const>::value
                                 || std::is_convertible<W const, T>::value>
{
};

/// @brief `T` is assignable from `W` of any of the four forms
/// ([optional.assign]/21).
template <typename T, typename W>
struct assigns_from_any_cvref
    : std::integral_constant<bool,
                             std::is_assignable<T &, W &>::value
                                 || std::is_assignable<T &, W &&>::value
                                 || std::is_assignable<T &, W const &>::value
                                 || std::is_assignable<T &, W const &&>::value>
{
};

/// @brief Whether `T` is `bool`, with or without `const` and `volatile`.
template <typename T>
struct is_cv_bool : std::is_same<typename std::remove_cv<T>::type, bool>
{
};

// is_swappable and is_nothrow_swappable: the standard ones from C++17, an
// ADL based detection below.
#if __cplusplus >= 201703L
template <typename T> using is_swappable = std::is_swappable<T>;
template <typename T>
using is_nothrow_swappable = std::is_nothrow_swappable<T>;
#else
namespace swap_probe
{
using std::swap;
template <typename T>
auto probe (int)
    -> decltype (swap (std::declval<T &> (), std::declval<T &> ()),
                 std::true_type ());
template <typename T> auto probe (...) -> std::false_type;
template <typename T, bool = decltype (probe<T> (0))::value>
struct nothrow : std::false_type
{
};
template <typename T>
struct nothrow<T, true>
    : std::integral_constant<bool, noexcept (swap (std::declval<T &> (),
                                                   std::declval<T &> ()))>
{
};
} // namespace swap_probe

template <typename T> struct is_swappable : decltype (swap_probe::probe<T> (0))
{
};
template <typename T> struct is_nothrow_swappable : swap_probe::nothrow<T>
{
};
#endif

// ====================== INVOKE for C++11 ====================== //

template <typename T> struct is_reference_wrapper : std::false_type
{
};

template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{
};

/// @brief The object of a pointer to member, when the first argument is an
/// object of the class (or derived from it).
template <typename Class, typename Obj>
LUMEX_CONSTEXPR typename std::enable_if<
    std::is_base_of<Class, typename std::decay<Obj>::type>::value,
    Obj &&>::type
member_target (Obj &&obj) LUMEX_NOEXCEPT
{
  return detail::fwd<Obj> (obj);
}

/// @brief The object of a pointer to member, when the first argument is a
/// `std::reference_wrapper`.
template <typename Class, typename Obj>
LUMEX_CONSTEXPR typename std::enable_if<
    is_reference_wrapper<typename std::decay<Obj>::type>::value
        && !std::is_base_of<Class, typename std::decay<Obj>::type>::value,
    decltype (std::declval<Obj> ().get ())>::type
member_target (Obj &&obj) LUMEX_NOEXCEPT
{
  return obj.get ();
}

/// @brief The object of a pointer to member, when the first argument is a
/// pointer-like object.
template <typename Class, typename Obj>
LUMEX_CONSTEXPR typename std::enable_if<
    !is_reference_wrapper<typename std::decay<Obj>::type>::value
        && !std::is_base_of<Class, typename std::decay<Obj>::type>::value,
    decltype (*std::declval<Obj> ())>::type
member_target (Obj &&obj)
{
  return *detail::fwd<Obj> (obj);
}

/// @brief `INVOKE` of a pointer to a member function.
template <typename Member, typename Class, typename Obj, typename... Args>
LUMEX_CONSTEXPR auto
invoke (Member Class::*pmf, Obj &&obj, Args &&...args) ->
    typename std::enable_if<
        std::is_function<Member>::value,
        decltype ((detail::member_target<Class> (detail::fwd<Obj> (obj))
                   .*pmf) (detail::fwd<Args> (args)...))>::type
{
  return (detail::member_target<Class> (detail::fwd<Obj> (obj))
          .*pmf) (detail::fwd<Args> (args)...);
}

/// @brief `INVOKE` of a pointer to a data member.
template <typename Member, typename Class, typename Obj>
LUMEX_CONSTEXPR auto
invoke (Member Class::*pmd, Obj &&obj) -> typename std::enable_if<
    !std::is_function<Member>::value,
    decltype (detail::member_target<Class> (detail::fwd<Obj> (obj))
              .*pmd)>::type
{
  return detail::member_target<Class> (detail::fwd<Obj> (obj)).*pmd;
}

/// @brief `INVOKE` of a function object or a function pointer.
template <typename Fn, typename... Args>
LUMEX_CONSTEXPR auto
invoke (Fn &&fn, Args &&...args)
    -> decltype (detail::fwd<Fn> (fn) (detail::fwd<Args> (args)...))
{
  return detail::fwd<Fn> (fn) (detail::fwd<Args> (args)...);
}

template <typename AlwaysVoid, typename Fn, typename... Args>
struct invoke_result_impl
{
};

template <typename Fn, typename... Args>
struct invoke_result_impl<void_t<decltype (detail::invoke (
                              std::declval<Fn> (), std::declval<Args> ()...))>,
                          Fn, Args...>
{
  using type = decltype (detail::invoke (std::declval<Fn> (),
                                         std::declval<Args> ()...));
};

/// @brief The type of `INVOKE (declval<Fn> (), declval<Args> ()...)`; absent
/// (a substitution failure) when the call is ill-formed.
template <typename Fn, typename... Args>
using invoke_result_t = typename invoke_result_impl<void, Fn, Args...>::type;

// ====================== Tags of the storage ====================== //

/// @brief Tag: build the value from the result of a call.
struct invoke_tag
{
};

// ====================== The union ====================== //

/// @brief The member of the union that is active while no value is held.
struct empty_member
{
};

/// @brief Whether `T` has a copy assignment that takes a non-const reference
/// (`T &operator= (T &)`, as `std::auto_ptr`) and none that takes a const one.
template <typename T>
struct has_nonconst_copy_assign
    : std::integral_constant<
          bool,
          std::is_class<typename std::remove_cv<T>::type>::value
              && std::is_assignable<typename std::remove_cv<T>::type &,
                                    typename std::remove_cv<T>::type &>::value
              && !std::is_assignable<
                  typename std::remove_cv<T>::type &,
                  typename std::remove_cv<T>::type const &>::value>
{
};

/// @brief Whether `T` has a copy constructor that takes a non-const reference
/// and none that takes a const one.
template <typename T>
struct has_nonconst_copy_ctor
    : std::integral_constant<
          bool, std::is_class<typename std::remove_cv<T>::type>::value
                    && std::is_constructible<
                        typename std::remove_cv<T>::type,
                        typename std::remove_cv<T>::type &>::value
                    && !std::is_constructible<
                        typename std::remove_cv<T>::type,
                        typename std::remove_cv<T>::type const &>::value>
{
};

/**
 * @def LUMEX_OPTIONAL_UNION_BODY
 * @brief The members and constructors every form of `optional_union` shares.
 */
#define LUMEX_OPTIONAL_UNION_BODY                                             \
  empty_member dummy;                                                         \
  typename std::remove_cv<T>::type value;                                     \
                                                                              \
  LUMEX_CONSTEXPR_CTOR                                                        \
  optional_union () LUMEX_NOEXCEPT : dummy () {}                              \
                                                                              \
  template <typename... Args>                                                 \
  LUMEX_CONSTEXPR_CTOR explicit optional_union (in_place_t, Args &&...args)   \
      : value (detail::fwd<Args> (args)...)                                   \
  {                                                                           \
  }                                                                           \
                                                                              \
  template <typename Fn, typename... Args>                                    \
  LUMEX_CONSTEXPR_CTOR optional_union (invoke_tag, Fn &&fn, Args &&...args)   \
      : value (detail::invoke (detail::fwd<Fn> (fn),                          \
                               detail::fwd<Args> (args)...))                  \
  {                                                                           \
  }

/**
 * @brief `{ dummy, value }`, the union of [optional.optional.general]. The
 * value is stored without `const` (as both standard libraries do), so that an
 * `optional<const U>` can be re-emplaced in a constant expression.
 * @details Eight forms. Whether the destructor is trivial (`T` trivially
 * destructible) or empty (the value is destroyed by `opt_destruct_base`)
 * selects two. The other two flags tell a `T` whose copy assignment
 * (`NonConstAssign`) or copy constructor (`NonConstCtor`) takes a non-const
 * reference (`std::auto_ptr`). The implicit copy operation of a class that
 * holds such a `T` takes a non-const reference too, and the layers declare
 * theirs `= default` with `const &`, which is ill-formed before C++20
 * (P0641). The union therefore declares the copy operation that cannot
 * work deleted, with a `const &` parameter, and so fixes the signature of
 * every layer above; the move operations are defaulted (a `T` with
 * `T &operator= (T &&) = default` stays move assignable). For any other `T` it
 * declares nothing.
 */
template <typename T,
          bool TrivialDestructor = std::is_trivially_destructible<T>::value,
          bool NonConstAssign = has_nonconst_copy_assign<T>::value,
          bool NonConstCtor = has_nonconst_copy_ctor<T>::value>
union optional_union;

template <typename T> union optional_union<T, true, false, false>
{
  LUMEX_OPTIONAL_UNION_BODY
};

template <typename T> union optional_union<T, true, true, false>
{
  LUMEX_OPTIONAL_UNION_BODY
  optional_union (optional_union const &) = default;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = delete;
  optional_union &operator= (optional_union &&) = default;
};

template <typename T> union optional_union<T, true, false, true>
{
  LUMEX_OPTIONAL_UNION_BODY
  optional_union (optional_union const &) = delete;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = default;
  optional_union &operator= (optional_union &&) = default;
};

template <typename T> union optional_union<T, true, true, true>
{
  LUMEX_OPTIONAL_UNION_BODY
  optional_union (optional_union const &) = delete;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = delete;
  optional_union &operator= (optional_union &&) = default;
};

template <typename T> union optional_union<T, false, false, false>
{
  LUMEX_OPTIONAL_UNION_BODY

  LUMEX_CONSTEXPR_DTOR
  ~optional_union () {}
};

template <typename T> union optional_union<T, false, true, false>
{
  LUMEX_OPTIONAL_UNION_BODY

  LUMEX_CONSTEXPR_DTOR
  ~optional_union () {}
  optional_union (optional_union const &) = default;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = delete;
  optional_union &operator= (optional_union &&) = default;
};

template <typename T> union optional_union<T, false, false, true>
{
  LUMEX_OPTIONAL_UNION_BODY

  LUMEX_CONSTEXPR_DTOR
  ~optional_union () {}
  optional_union (optional_union const &) = delete;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = default;
  optional_union &operator= (optional_union &&) = default;
};

template <typename T> union optional_union<T, false, true, true>
{
  LUMEX_OPTIONAL_UNION_BODY

  LUMEX_CONSTEXPR_DTOR
  ~optional_union () {}
  optional_union (optional_union const &) = delete;
  optional_union (optional_union &&) = default;
  optional_union &operator= (optional_union const &) = delete;
  optional_union &operator= (optional_union &&) = default;
};

#undef LUMEX_OPTIONAL_UNION_BODY

// ====================== Layer: destructor ====================== //

/**
 * @brief The union, the flag and the destructor; this form is for a
 * trivially destructible `T`, so the destructor is trivial.
 */
template <typename T, bool = std::is_trivially_destructible<T>::value>
struct opt_destruct_base
{
  optional_union<T> m_u;
  bool m_engaged;

  LUMEX_CONSTEXPR_CTOR
  opt_destruct_base () LUMEX_NOEXCEPT : m_u (), m_engaged (false) {}

  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit opt_destruct_base (in_place_t tag,
                                                   Args &&...args)
      : m_u (tag, detail::fwd<Args> (args)...), m_engaged (true)
  {
  }

  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  opt_destruct_base (invoke_tag tag, Fn &&fn, Args &&...args)
      : m_u (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...),
        m_engaged (true)
  {
  }

  /// @brief Ends the value's life; nothing to destroy for this `T`.
  LUMEX_CONSTEXPR_CXX14 void
  reset_value () LUMEX_NOEXCEPT
  {
    m_engaged = false;
  }

  /// @brief Starts the value's life with `args...`; the flag is set last, so
  /// a throwing constructor leaves the object empty.
  template <typename... Args>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 void
  construct (Args &&...args)
  {
    detail::construct_at (detail::address_of (m_u.value),
                          detail::fwd<Args> (args)...);
    m_engaged = true;
  }
};

/// @brief The same for a `T` with a non-trivial destructor.
template <typename T> struct opt_destruct_base<T, false>
{
  optional_union<T> m_u;
  bool m_engaged;

  LUMEX_CONSTEXPR_CTOR
  opt_destruct_base () LUMEX_NOEXCEPT : m_u (), m_engaged (false) {}

  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit opt_destruct_base (in_place_t tag,
                                                   Args &&...args)
      : m_u (tag, detail::fwd<Args> (args)...), m_engaged (true)
  {
  }

  template <typename Fn, typename... Args>
  LUMEX_CONSTEXPR_CTOR
  opt_destruct_base (invoke_tag tag, Fn &&fn, Args &&...args)
      : m_u (tag, detail::fwd<Fn> (fn), detail::fwd<Args> (args)...),
        m_engaged (true)
  {
  }

  opt_destruct_base (opt_destruct_base const &) = default;
  opt_destruct_base (opt_destruct_base &&) = default;
  opt_destruct_base &operator= (opt_destruct_base const &) = default;
  opt_destruct_base &operator= (opt_destruct_base &&) = default;

  LUMEX_CONSTEXPR_DTOR
  ~opt_destruct_base ()
  {
    if (m_engaged)
      m_u.value.~T ();
  }

  LUMEX_OPTIONAL_CONSTEXPR_CXX20 void
  reset_value () LUMEX_NOEXCEPT
  {
    if (m_engaged)
      {
        m_u.value.~T ();
        m_engaged = false;
      }
  }

  template <typename... Args>
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 void
  construct (Args &&...args)
  {
    detail::construct_at (detail::address_of (m_u.value),
                          detail::fwd<Args> (args)...);
    m_engaged = true;
  }
};

// ====================== Layer: copy constructor ====================== //

/// @brief 0 deleted, 1 trivial, 2 written out ([optional.ctor]/6).
template <typename T>
struct copy_ctor_kind
    : std::integral_constant<
          int, !std::is_copy_constructible<T>::value
                   ? 0
                   : (std::is_trivially_copy_constructible<T>::value ? 1 : 2)>
{
};

template <typename T, int = copy_ctor_kind<T>::value>
struct opt_copy_ctor_base;

template <typename T> struct opt_copy_ctor_base<T, 1> : opt_destruct_base<T>
{
  using base_type = opt_destruct_base<T>;
  using base_type::base_type;
  opt_copy_ctor_base () = default;
  opt_copy_ctor_base (opt_copy_ctor_base const &) = default;
  opt_copy_ctor_base (opt_copy_ctor_base &&) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base const &) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base &&) = default;
};

template <typename T> struct opt_copy_ctor_base<T, 2> : opt_destruct_base<T>
{
  using base_type = opt_destruct_base<T>;
  using base_type::base_type;
  opt_copy_ctor_base () = default;
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  opt_copy_ctor_base (opt_copy_ctor_base const &other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_copy_constructible<T>::value)
      : base_type ()
  {
    if (other.m_engaged)
      this->construct (other.m_u.value);
  }
  opt_copy_ctor_base (opt_copy_ctor_base &&) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base const &) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base &&) = default;
};

template <typename T> struct opt_copy_ctor_base<T, 0> : opt_destruct_base<T>
{
  using base_type = opt_destruct_base<T>;
  using base_type::base_type;
  opt_copy_ctor_base () = default;
  opt_copy_ctor_base (opt_copy_ctor_base const &) = delete;
  opt_copy_ctor_base (opt_copy_ctor_base &&) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base const &) = default;
  opt_copy_ctor_base &operator= (opt_copy_ctor_base &&) = default;
};

// ====================== Layer: move constructor ====================== //

template <typename T>
struct move_ctor_kind
    : std::integral_constant<
          int, !std::is_move_constructible<T>::value
                   ? 0
                   : (std::is_trivially_move_constructible<T>::value ? 1 : 2)>
{
};

template <typename T, int = move_ctor_kind<T>::value>
struct opt_move_ctor_base;

template <typename T> struct opt_move_ctor_base<T, 1> : opt_copy_ctor_base<T>
{
  using base_type = opt_copy_ctor_base<T>;
  using base_type::base_type;
  opt_move_ctor_base () = default;
  opt_move_ctor_base (opt_move_ctor_base const &) = default;
  opt_move_ctor_base (opt_move_ctor_base &&) = default;
  opt_move_ctor_base &operator= (opt_move_ctor_base const &) = default;
  opt_move_ctor_base &operator= (opt_move_ctor_base &&) = default;
};

template <typename T> struct opt_move_ctor_base<T, 2> : opt_copy_ctor_base<T>
{
  using base_type = opt_copy_ctor_base<T>;
  using base_type::base_type;
  opt_move_ctor_base () = default;
  opt_move_ctor_base (opt_move_ctor_base const &) = default;
  LUMEX_OPTIONAL_CONSTEXPR_CXX20
  opt_move_ctor_base (opt_move_ctor_base &&other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<T>::value)
      : base_type ()
  {
    if (other.m_engaged)
      this->construct (detail::mv (other.m_u.value));
  }
  opt_move_ctor_base &operator= (opt_move_ctor_base const &) = default;
  opt_move_ctor_base &operator= (opt_move_ctor_base &&) = default;
};

template <typename T> struct opt_move_ctor_base<T, 0> : opt_copy_ctor_base<T>
{
  using base_type = opt_copy_ctor_base<T>;
  using base_type::base_type;
  opt_move_ctor_base () = default;
  opt_move_ctor_base (opt_move_ctor_base const &) = default;
  opt_move_ctor_base (opt_move_ctor_base &&) = delete;
  opt_move_ctor_base &operator= (opt_move_ctor_base const &) = default;
  opt_move_ctor_base &operator= (opt_move_ctor_base &&) = default;
};

// ====================== Layer: copy assignment ====================== //

template <typename T>
struct copy_assign_kind
    : std::integral_constant<
          int, !(std::is_copy_constructible<T>::value
                 && std::is_copy_assignable<T>::value)
                   ? 0
                   : ((std::is_trivially_copy_constructible<T>::value
                       && std::is_trivially_copy_assignable<T>::value
                       && std::is_trivially_destructible<T>::value)
                          ? 1
                          : 2)>
{
};

template <typename T, int = copy_assign_kind<T>::value>
struct opt_copy_assign_base;

template <typename T> struct opt_copy_assign_base<T, 1> : opt_move_ctor_base<T>
{
  using base_type = opt_move_ctor_base<T>;
  using base_type::base_type;
  opt_copy_assign_base () = default;
  opt_copy_assign_base (opt_copy_assign_base const &) = default;
  opt_copy_assign_base (opt_copy_assign_base &&) = default;
  opt_copy_assign_base &operator= (opt_copy_assign_base const &) = default;
  opt_copy_assign_base &operator= (opt_copy_assign_base &&) = default;
};

template <typename T> struct opt_copy_assign_base<T, 2> : opt_move_ctor_base<T>
{
  using base_type = opt_move_ctor_base<T>;
  using base_type::base_type;
  opt_copy_assign_base () = default;
  opt_copy_assign_base (opt_copy_assign_base const &) = default;
  opt_copy_assign_base (opt_copy_assign_base &&) = default;

  /// @brief [optional.assign]/3: assigns the value when both hold one,
  /// copy-constructs it when only `other` does, destroys it when only `*this`
  /// does. A throwing `T` leaves the engaged state as it was.
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 opt_copy_assign_base &
  operator= (opt_copy_assign_base const &other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_copy_constructible<T>::value
                             &&std::is_nothrow_copy_assignable<T>::value)
  {
    if (this->m_engaged && other.m_engaged)
      this->m_u.value = other.m_u.value;
    else if (other.m_engaged)
      this->construct (other.m_u.value);
    else
      this->reset_value ();
    return *this;
  }
  opt_copy_assign_base &operator= (opt_copy_assign_base &&) = default;
};

template <typename T> struct opt_copy_assign_base<T, 0> : opt_move_ctor_base<T>
{
  using base_type = opt_move_ctor_base<T>;
  using base_type::base_type;
  opt_copy_assign_base () = default;
  opt_copy_assign_base (opt_copy_assign_base const &) = default;
  opt_copy_assign_base (opt_copy_assign_base &&) = default;
  opt_copy_assign_base &operator= (opt_copy_assign_base const &) = delete;
  opt_copy_assign_base &operator= (opt_copy_assign_base &&) = default;
};

// ====================== Layer: move assignment ====================== //

template <typename T>
struct move_assign_kind
    : std::integral_constant<
          int, !(std::is_move_constructible<T>::value
                 && std::is_move_assignable<T>::value)
                   ? 0
                   : ((std::is_trivially_move_constructible<T>::value
                       && std::is_trivially_move_assignable<T>::value
                       && std::is_trivially_destructible<T>::value)
                          ? 1
                          : 2)>
{
};

template <typename T, int = move_assign_kind<T>::value>
struct opt_move_assign_base;

template <typename T>
struct opt_move_assign_base<T, 1> : opt_copy_assign_base<T>
{
  using base_type = opt_copy_assign_base<T>;
  using base_type::base_type;
  opt_move_assign_base () = default;
  opt_move_assign_base (opt_move_assign_base const &) = default;
  opt_move_assign_base (opt_move_assign_base &&) = default;
  opt_move_assign_base &operator= (opt_move_assign_base const &) = default;
  opt_move_assign_base &operator= (opt_move_assign_base &&) = default;
};

template <typename T>
struct opt_move_assign_base<T, 2> : opt_copy_assign_base<T>
{
  using base_type = opt_copy_assign_base<T>;
  using base_type::base_type;
  opt_move_assign_base () = default;
  opt_move_assign_base (opt_move_assign_base const &) = default;
  opt_move_assign_base (opt_move_assign_base &&) = default;
  opt_move_assign_base &operator= (opt_move_assign_base const &) = default;

  /// @brief [optional.assign]/8: as the copy assignment, with moves. `other`
  /// keeps its (moved-from) value.
  LUMEX_OPTIONAL_CONSTEXPR_CXX20 opt_move_assign_base &
  operator= (opt_move_assign_base &&other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<T>::value
                             &&std::is_nothrow_move_assignable<T>::value)
  {
    if (this->m_engaged && other.m_engaged)
      this->m_u.value = detail::mv (other.m_u.value);
    else if (other.m_engaged)
      this->construct (detail::mv (other.m_u.value));
    else
      this->reset_value ();
    return *this;
  }
};

template <typename T>
struct opt_move_assign_base<T, 0> : opt_copy_assign_base<T>
{
  using base_type = opt_copy_assign_base<T>;
  using base_type::base_type;
  opt_move_assign_base () = default;
  opt_move_assign_base (opt_move_assign_base const &) = default;
  opt_move_assign_base (opt_move_assign_base &&) = default;
  opt_move_assign_base &operator= (opt_move_assign_base const &) = default;
  opt_move_assign_base &operator= (opt_move_assign_base &&) = delete;
};

// ====================== Results of the monadic operations ================

/// @brief Result of `and_then`: `remove_cvref_t` of the call, which must be a
/// specialization of `optional`; absent otherwise.
template <typename AlwaysVoid, typename Fn, typename Arg>
struct and_then_result
{
};

template <typename Fn, typename Arg>
struct and_then_result<
    typename std::enable_if<is_optional_specialization<
        remove_cvref_t<invoke_result_t<Fn, Arg>>>::value>::type,
    Fn, Arg>
{
  using type = remove_cvref_t<invoke_result_t<Fn, Arg>>;
};

/// @brief Whether `U` can be held by an `optional` and constructed from the
/// call result `R`. From C++17 a prvalue result of the type `U` itself is
/// built in place (guaranteed elision), so `U` need not be movable.
template <typename U, typename R>
struct is_transform_target
    : std::integral_constant<
          bool,
          std::is_object<U>::value && !std::is_array<U>::value
              && !is_tag_type<U>::value
              && (std::is_constructible<U, R>::value
#if __cplusplus >= 201703L
                  || std::is_same<typename std::remove_cv<R>::type, U>::value
#endif
                  )>
{
};

/// @brief Result of `transform`: `optional<remove_cv_t<call result>>`; absent
/// when the call is ill-formed or its result cannot be held.
template <typename AlwaysVoid, typename Fn, typename Arg>
struct transform_result
{
};

template <typename Fn, typename Arg>
struct transform_result<
    typename std::enable_if<is_transform_target<
        typename std::remove_cv<invoke_result_t<Fn, Arg>>::type,
        invoke_result_t<Fn, Arg>>::value>::type,
    Fn, Arg>
{
  using type
      = optional<typename std::remove_cv<invoke_result_t<Fn, Arg>>::type>;
};

/// @brief Whether `Fn ()` is a valid call whose result, without cv and
/// reference, is `Opt`.
template <typename AlwaysVoid, typename Fn, typename Opt>
struct or_else_result_is : std::false_type
{
};

template <typename Fn, typename Opt>
struct or_else_result_is<void_t<invoke_result_t<Fn>>, Fn, Opt>
    : std::is_same<remove_cvref_t<invoke_result_t<Fn>>, Opt>
{
};

/// @brief `or_else` is viable: `Fn ()` returns `Opt` and the value can be
/// copied or moved (`Allowed`, computed from `T` by the caller).
template <typename Fn, typename Opt, bool Allowed>
struct or_else_enabled
    : std::integral_constant<
          bool, Allowed && or_else_result_is<void, Fn, Opt>::value>
{
};

} // namespace detail
} // namespace opt
} // namespace optional
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_OPTIONAL_OPT_DETAIL_HPP

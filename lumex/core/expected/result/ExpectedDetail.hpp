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
 * @file ExpectedDetail.hpp
 * @brief Internal helpers of `expected`: the constraints of the monadic
 * operations, the traits behind the converting constructors and the
 * comparisons, and `invoke_call`, the standard INVOKE for C++11.
 * @details Everything here lives in `lumex::core::expected::result::detail`
 * and is not an interface for consumers. The header exists so that the primary
 * template and the `void` specialization of `expected` share one definition of
 * each rule instead of repeating it in every overload. It builds on
 * `Unexpected.hpp` (the traits of the error side) and only forward declares
 * `expected`, so it is included by `Expected.hpp` and `ExpectedVoid.hpp`.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_DETAIL_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_DETAIL_HPP

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

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "lumex/core/expected/error/Unexpected.hpp"
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
template <typename SuccessType, typename ErrorType> class expected;

namespace detail
{
namespace traits = lumex::core::utility::traits;

using error::detail::is_nothrow_swappable;
using error::detail::is_swappable;
using error::detail::is_unexpected;

/// @brief `T` without `const`, `volatile` and reference (`std::remove_cvref_t`
/// of C++20).
template <typename T> using remove_cvref_t = traits::meta::CleanType<T>;

// ====================== Storage ====================== //

/**
 * @brief The address of `object` as a `void *`, also for a `const` or
 * `volatile` object.
 * @details Placement new takes a `void *`, which a `const SuccessType *` is
 * not; this is how `expected<const T, E>` constructs its value in place.
 * @tparam T Type of the object, possibly cv-qualified.
 * @param[in] object The object whose address is taken.
 * @return The address of `object`.
 */
template <typename T>
inline void *
voidify (T &object) LUMEX_NOEXCEPT
{
  return const_cast<void *> (
      static_cast<void const volatile *> (std::addressof (object)));
}

// ====================== Calls ====================== //

/**
 * @brief Tag: build the value of an `expected` from the result of a call.
 * @details The private constructor that takes it initializes the stored value
 * with `invoke_call (fn, args...)` itself, so the result of the call is
 * constructed in place (no extra move, and a result type that cannot be
 * moved works from C++17), and a `void` result leaves an `expected<void, E>`
 * in the success state.
 */
struct invoke_value_tag
{
};

/**
 * @brief Tag: build the error of an `expected` from the result of a call.
 * @details Counterpart of `invoke_value_tag` for `transform_error`.
 */
struct invoke_error_tag
{
};

/// @brief `Fn` may be called with `Args...` (INVOKE, as `std::is_invocable`).
template <typename Fn, typename... Args>
struct is_callable_with : traits::invoke::is_invocable<Fn &&, Args...>
{
};

/// @brief Result type of calling an `Fn` of the given value category with
/// `Args...`; absent (a substitution failure) when the call is ill-formed.
template <typename Fn, typename... Args>
using invoke_result_t = traits::invoke::invoke_result_t<Fn &&, Args...>;

/// @brief The call result without `const`, `volatile` and reference; the type
/// `and_then` and `or_else` return.
template <typename Fn, typename... Args>
using result_clean_t = remove_cvref_t<invoke_result_t<Fn, Args...>>;

/// @brief The call result without `const` and `volatile`; the value or error
/// type `transform` and `transform_error` build.
template <typename Fn, typename... Args>
using result_xform_t =
    typename std::remove_cv<invoke_result_t<Fn, Args...>>::type;

template <typename AlwaysVoid, typename Fn, typename... Args>
struct returns_expected_impl : std::false_type
{
};

template <typename Fn, typename... Args>
struct returns_expected_impl<
    traits::meta::void_t<invoke_result_t<Fn, Args...>>, Fn, Args...>
    : traits::value::is_expected<result_clean_t<Fn, Args...>>
{
};

/// @brief `Fn` may be called with `Args...` and the result, without `const`,
/// `volatile` and reference, is an `expected`.
template <typename Fn, typename... Args>
using returns_expected = returns_expected_impl<void, Fn, Args...>;

template <typename AlwaysVoid, typename Fn, typename... Args>
struct returns_value_impl : std::false_type
{
};

template <typename Fn, typename... Args>
struct returns_value_impl<traits::meta::void_t<invoke_result_t<Fn, Args...>>,
                          Fn, Args...>
    : std::integral_constant<
          bool, !std::is_void<invoke_result_t<Fn, Args...>>::value>
{
};

/// @brief `Fn` may be called with `Args...` and the result is not `void`.
template <typename Fn, typename... Args>
using returns_value = returns_value_impl<void, Fn, Args...>;

// ====================== Constraints of the monadic operations
// ================ `Fn` is the function type as deduced from the forwarding
// reference, `Args...` what the call receives, and the `...Arg` type the way
// the object that is not handed to the call is copied or moved into the
// result: `E &`, `E const &`, `E` or `E const` for the four value categories
// of the object. A `void` value type (the specialization `expected<void, E>`)
// has nothing to copy.

/// @brief `and_then`: `Fn (Args...)` returns an `expected`, and the error can
/// be copied into it.
template <typename Fn, typename ErrorType, typename ErrorArg, typename... Args>
struct can_and_then
    : std::integral_constant<
          bool, returns_expected<Fn, Args...>::value
                    && std::is_constructible<ErrorType, ErrorArg>::value>
{
};

/// @brief `or_else`: `Fn (Args...)` returns an `expected`, and the value can
/// be copied into it.
template <typename Fn, typename ValueType, typename ValueArg, typename... Args>
struct can_or_else
    : std::integral_constant<
          bool, returns_expected<Fn, Args...>::value
                    && (std::is_void<ValueType>::value
                        || std::is_constructible<ValueType, ValueArg>::value)>
{
};

/// @brief `transform`: `Fn (Args...)` may be called (its result may be any
/// type, `void` included), and the error can be copied.
template <typename Fn, typename ErrorType, typename ErrorArg, typename... Args>
struct can_transform
    : std::integral_constant<
          bool, is_callable_with<Fn, Args...>::value
                    && std::is_constructible<ErrorType, ErrorArg>::value>
{
};

/// @brief `transform_error`: `Fn (Args...)` returns a value (`void` cannot be
/// an error type), and the value can be copied.
template <typename Fn, typename ValueType, typename ValueArg, typename... Args>
struct can_transform_error
    : std::integral_constant<
          bool, returns_value<Fn, Args...>::value
                    && (std::is_void<ValueType>::value
                        || std::is_constructible<ValueType, ValueArg>::value)>
{
};

// ====================== Converting constructors ====================== //

/// @brief `unexpected<ErrorType>` can be built from an `expected<U, G>` in any
/// of its four value categories (the standard's `__cons_from_expected` for the
/// error part).
template <typename ErrorType, typename U, typename G>
struct unexpected_from_expected
    : std::integral_constant<
          bool, std::is_constructible<error::unexpected<ErrorType>,
                                      expected<U, G> &>::value
                    || std::is_constructible<error::unexpected<ErrorType>,
                                             expected<U, G>>::value
                    || std::is_constructible<error::unexpected<ErrorType>,
                                             expected<U, G> const &>::value
                    || std::is_constructible<error::unexpected<ErrorType>,
                                             expected<U, G> const>::value>
{
};

/// @brief `ValueType` can be built from, or converted from, an `expected<U,
/// G>` in any of its four value categories (the standard's
/// `converts-from-any-cvref`).
template <typename ValueType, typename U, typename G>
struct value_from_expected
    : std::integral_constant<
          bool,
          std::is_constructible<ValueType, expected<U, G> &>::value
              || std::is_constructible<ValueType, expected<U, G>>::value
              || std::is_constructible<ValueType,
                                       expected<U, G> const &>::value
              || std::is_constructible<ValueType, expected<U, G> const>::value
              || std::is_convertible<expected<U, G> &, ValueType>::value
              || std::is_convertible<expected<U, G>, ValueType>::value
              || std::is_convertible<expected<U, G> const &, ValueType>::value
              || std::is_convertible<expected<U, G> const, ValueType>::value>
{
};

/// @brief The converting constructors of `expected` step aside: `ValueType` or
/// `unexpected<ErrorType>` can be built from the whole `expected<U, G>`, so
/// the value constructor converts it instead. A `bool` value type never counts
/// the value side: `expected<int, E>` is constructible into `bool` through its
/// `operator bool`, yet `expected<bool, E>` still converts the value inside
/// (the resolution of LWG 3836, which the value constructor backs by refusing
/// every `expected` for a `bool`).
template <typename ValueType, typename ErrorType, typename U, typename G>
struct constructs_from_expected
    : std::integral_constant<
          bool,
          (!std::is_same<typename std::remove_cv<ValueType>::type, bool>::value
           && value_from_expected<ValueType, U, G>::value)
              || unexpected_from_expected<ErrorType, U, G>::value>
{
};

// ====================== Comparison ====================== //

template <typename L, typename R, typename = void>
struct is_equality_comparable : std::false_type
{
};

/// @brief `lhs == rhs` is well-formed for a `L const &` and a `R const &` and
/// its result converts to `bool`.
template <typename L, typename R>
struct is_equality_comparable<
    L, R,
    traits::meta::void_t<decltype (std::declval<L const &> ()
                                   == std::declval<R const &> ())>>
    : std::is_convertible<decltype (std::declval<L const &> ()
                                    == std::declval<R const &> ()),
                          bool>
{
};

// ====================== INVOKE ====================== //

#if __cplusplus >= 201703L

/**
 * @brief Calls `fn (args...)` or, for a pointer to a member, applies it to the
 * first argument: the standard INVOKE.
 * @details From C++17 this is `std::invoke`; before that the overloads below
 * give the same result for a function object, a pointer to a member function
 * or to a data member, and an object, a `std::reference_wrapper` or a pointer
 * as the first argument.
 */
template <typename Fn, typename... Args>
LUMEX_CONSTEXPR_FUNCTION auto
invoke_call (Fn &&fn, Args &&...args)
    -> decltype (std::invoke (std::forward<Fn> (fn),
                              std::forward<Args> (args)...))
{
  return std::invoke (std::forward<Fn> (fn), std::forward<Args> (args)...);
}

#else

namespace invoke_detail
{
template <typename T> struct is_reference_wrapper : std::false_type
{
};

template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{
};

/// The object operand is of the class of the member pointer (or derived).
template <typename Class, typename Object>
struct is_class_object
    : std::is_base_of<Class, typename std::decay<Object>::type>
{
};

/// The object operand is a `std::reference_wrapper`.
template <typename Object>
struct is_wrapped_object
    : is_reference_wrapper<typename std::decay<Object>::type>
{
};

/// The object operand is neither: it is dereferenced (a pointer).
template <typename Class, typename Object>
struct is_pointer_object
    : std::integral_constant<bool, !is_class_object<Class, Object>::value
                                       && !is_wrapped_object<Object>::value>
{
};
} // namespace invoke_detail

// A pointer to a member function and an object of the class.
template <typename Member, typename Class, typename Object, typename... Args,
          typename = typename std::enable_if<
              std::is_function<Member>::value
              && invoke_detail::is_class_object<Class, Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object, Args &&...args)
    -> decltype ((std::forward<Object> (object)
                  .*member) (std::forward<Args> (args)...))
{
  return (std::forward<Object> (object)
          .*member) (std::forward<Args> (args)...);
}

// A pointer to a member function and a std::reference_wrapper.
template <typename Member, typename Class, typename Object, typename... Args,
          typename = typename std::enable_if<
              std::is_function<Member>::value
              && invoke_detail::is_wrapped_object<Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object, Args &&...args)
    -> decltype ((object.get ().*member) (std::forward<Args> (args)...))
{
  return (object.get ().*member) (std::forward<Args> (args)...);
}

// A pointer to a member function and a pointer to the object.
template <typename Member, typename Class, typename Object, typename... Args,
          typename = typename std::enable_if<
              std::is_function<Member>::value
              && invoke_detail::is_pointer_object<Class, Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object, Args &&...args)
    -> decltype (((*std::forward<Object> (object))
                  .*member) (std::forward<Args> (args)...))
{
  return ((*std::forward<Object> (object))
          .*member) (std::forward<Args> (args)...);
}

// A pointer to a data member and an object of the class.
template <typename Member, typename Class, typename Object,
          typename = typename std::enable_if<
              !std::is_function<Member>::value
              && invoke_detail::is_class_object<Class, Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object)
    -> decltype (std::forward<Object> (object).*member)
{
  return std::forward<Object> (object).*member;
}

// A pointer to a data member and a std::reference_wrapper.
template <typename Member, typename Class, typename Object,
          typename = typename std::enable_if<
              !std::is_function<Member>::value
              && invoke_detail::is_wrapped_object<Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object)
    -> decltype (object.get ().*member)
{
  return object.get ().*member;
}

// A pointer to a data member and a pointer to the object.
template <typename Member, typename Class, typename Object,
          typename = typename std::enable_if<
              !std::is_function<Member>::value
              && invoke_detail::is_pointer_object<Class, Object>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Member Class::*member, Object &&object)
    -> decltype ((*std::forward<Object> (object)).*member)
{
  return (*std::forward<Object> (object)).*member;
}

// Anything else: a function, a function object, a lambda.
template <typename Fn, typename... Args,
          typename = typename std::enable_if<!std::is_member_pointer<
              typename std::decay<Fn>::type>::value>::type>
LUMEX_CONSTEXPR_CXX14 auto
invoke_call (Fn &&fn, Args &&...args)
    -> decltype (std::forward<Fn> (fn) (std::forward<Args> (args)...))
{
  return std::forward<Fn> (fn) (std::forward<Args> (args)...);
}

#endif // __cplusplus >= 201703L

} // namespace detail
} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_DETAIL_HPP

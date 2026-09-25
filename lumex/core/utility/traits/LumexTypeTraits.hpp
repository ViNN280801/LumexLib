/**
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

#ifndef LUMEX_CORE_UTILITY_TRAITS_HPP
#define LUMEX_CORE_UTILITY_TRAITS_HPP

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#endif
#if __cplusplus >= 202002L
#include <concepts>
#endif

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @file LumexTypeTraits.hpp
 * @brief Every general-purpose type trait of LumexLib, one sub-namespace of
 * `lumex::core::utility::traits` per topic: `meta`, `invoke`, `stream`,
 * `range`, `string`, `tuple`, `value`, `enums`, `numeric`.
 * @details Traits that depend on a module's own types (for example the
 * formatter traits of `core/fmt`) stay in that module.
 */

namespace lumex
{
namespace core
{
namespace optional
{
namespace opt
{
template <typename T> class optional;
}
} // namespace optional
} // namespace core
} // namespace lumex

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
template <typename SuccessType, typename ErrorType> class Expected;
} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

namespace lumex
{
namespace core
{
namespace utility
{
namespace traits
{
// ---------------------------------------------------------------------
// meta: void_t, has_type, type_identity, CleanType, default_return
// ---------------------------------------------------------------------

namespace meta
{
template <typename...> struct make_void
{
  using type = void;
};

template <typename... Ts> using void_t = typename make_void<Ts...>::type;

template <typename T, typename = void> struct has_type : std::false_type
{
};

template <typename T>
struct has_type<T, void_t<typename T::type>> : std::true_type
{
};

/**
 * @brief `type` is `T`. Used to keep a parameter out of template argument
 * deduction (C++20 `std::type_identity`).
 */
template <typename T> struct type_identity
{
  using type = T;
};

#if __cplusplus >= 202002L
template <typename TypeToClean>
using CleanType = std::remove_cvref_t<TypeToClean>;
#else
template <typename TypeToClean>
using CleanType = typename std::remove_cv<
    typename std::remove_reference<TypeToClean>::type>::type;
#endif

template <typename T> using CleanTypeOf = CleanType<T>;

template <typename TargetType, typename SourceType>
inline CleanType<TargetType>
safe_cast (SourceType &&value) LUMEX_NOEXCEPT_IF (noexcept (
    static_cast<CleanType<TargetType>> (std::forward<SourceType> (value))))
{
  return static_cast<CleanType<TargetType>> (std::forward<SourceType> (value));
}

// ============================ Default Return Type
// ============================ //
/**
 * @brief Helper providing a default value for a type, primarily meant for
 * exception-safe fallbacks.
 * @tparam T Type of the value to return.
 * @note The compiler implicitly instantiates specializations for every type
 * via the primary template; explicit specializations below are only needed for
 * special cases (void, pointers).
 */
template <typename T> struct default_return
{
  LUMEX_STATIC_ASSERT_MSG (
      std::is_default_constructible<T>::value,
      "Type must be default-constructible. For "
      "non-default-constructible types, consider using "
      "std::optional or providing an explicit default value.");

  LUMEX_STATIC_ASSERT_MSG (
      !std::is_reference<T>::value,
      "Reference types cannot have default values. Use a pointer "
      "or value type instead.");

  LUMEX_STATIC_ASSERT_MSG (
      !std::is_abstract<T>::value,
      "Abstract classes cannot be instantiated. Use a pointer or a "
      "concrete derived type instead.");

  static LUMEX_CONSTEXPR T
  value () LUMEX_NOEXCEPT_IF (std::is_nothrow_default_constructible<T>::value)
  {
    return T{};
  }
};

// void{} is not a valid expression.
template <> struct default_return<void>
{
  static LUMEX_CONSTEXPR void
  value () LUMEX_NOEXCEPT
  {
  }
};

// Explicit for clarity (T*{} also works, but nullptr is more readable).
template <typename T> struct default_return<T *>
{
  static LUMEX_CONSTEXPR T *
  value () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};
// ================================================================================
// //

template <typename T> using RemovePtr = typename std::remove_pointer<T>::type;
/**
 * @brief `type` is what a pointer or reference points to: `T` for `T *`,
 * `T &` and `T` itself.
 */
template <typename T> struct indirection_of
{
  using type = T;
};

template <typename T> struct indirection_of<T *>
{
  using type = T;
};

template <typename T> struct indirection_of<T &>
{
  using type = T;
};

template <typename T>
using indirection_of_t = typename indirection_of<T>::type;

#if __cplusplus >= 202002L
/** @brief A pointer to a class type. */
template <typename T>
concept PointerToClass
    = std::is_pointer_v<T> && std::is_class_v<std::remove_pointer_t<T>>;

/** @brief An lvalue reference to a class type. */
template <typename T>
concept LvalueRefToClass = std::is_lvalue_reference_v<T>
                           && std::is_class_v<std::remove_reference_t<T>>;

/** @brief `T` is complete here (`sizeof (T)` is valid). */
template <typename T>
concept CompleteType = requires { sizeof (T); };

/** @brief Going from `From` to `To` does not drop `const` or `volatile`. */
template <typename From, typename To>
concept PreserveCV
    = ((!std::is_const_v<From> || std::is_const_v<To>)
       && (!std::is_volatile_v<From> || std::is_volatile_v<To>));

/**
 * @brief A value that can be copied out of raw bytes: trivially copyable,
 * standard layout, neither a pointer nor a reference.
 */
template <typename T>
concept Extractible
    = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
      && !std::is_pointer_v<T> && !std::is_reference_v<T>;

/** @brief `std::byte`, `char` or `unsigned char`. */
template <typename T>
concept ByteLike = std::same_as<T, std::byte> || std::same_as<T, char>
                   || std::same_as<T, unsigned char>;
#endif
} // namespace meta

// ---------------------------------------------------------------------
// invoke: invoke_result, result_of, is_invocable, is_callable
// ---------------------------------------------------------------------

namespace invoke
{
namespace detail
{
template <typename T> struct is_reference_wrapper : std::false_type
{
};

template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{
};

template <typename T> struct invoke_impl
{
  template <typename Func, typename... Args>
  static auto call (Func &&func, Args &&...args)
      -> decltype (std::forward<Func> (func) (std::forward<Args> (args)...));
};

template <typename B, typename MT> struct invoke_impl<MT B::*>
{
  template <typename T, typename Td = typename std::decay<T>::type,
            typename
            = typename std::enable_if<std::is_base_of<B, Td>::value>::type>
  static auto get (T &&arg) -> T &&;

  template <typename T, typename Td = typename std::decay<T>::type,
            typename
            = typename std::enable_if<is_reference_wrapper<Td>::value>::type>
  static auto get (T &&arg) -> decltype (arg.get ());

  template <typename T, typename Td = typename std::decay<T>::type,
            typename
            = typename std::enable_if<!std::is_base_of<B, Td>::value>::type,
            typename
            = typename std::enable_if<!is_reference_wrapper<Td>::value>::type>
  static auto get (T &&arg) -> decltype (*std::forward<T> (arg));

  template <typename T, typename... Args, typename MT1,
            typename
            = typename std::enable_if<std::is_function<MT1>::value>::type>
  static auto call (MT1 B::*pmf, T &&arg, Args &&...args)
      -> decltype ((invoke_impl::get (std::forward<T> (arg))
                    .*pmf) (std::forward<Args> (args)...));

  template <typename T>
  static auto call (MT B::*pmd, T &&arg)
      -> decltype (invoke_impl::get (std::forward<T> (arg)).*pmd);
};

template <typename Func, typename... Args,
          typename FuncDecayed = typename std::decay<Func>::type>
auto INVOKE (Func &&func, Args &&...args)
    -> decltype (invoke_impl<FuncDecayed>::call (
        std::forward<Func> (func), std::forward<Args> (args)...));

// SFINAE core: if INVOKE(...) is well-formed, expose ::type =
// decltype(INVOKE(...))
template <typename AlwaysVoid, typename /*F*/, typename... /*Args*/>
struct invoke_result_impl
{ /* no ::type when ill-formed */
};

template <typename F, typename... Args>
struct invoke_result_impl<meta::void_t<decltype (INVOKE (
                              std::declval<F> (), std::declval<Args> ()...))>,
                          F, Args...>
{
  using type
      = decltype (INVOKE (std::declval<F> (), std::declval<Args> ()...));
};
} // namespace detail

template <typename Func, typename... Args>
struct invoke_result : detail::invoke_result_impl<void, Func, Args...>
{
};

template <typename Func, typename... Args>
using invoke_result_t = typename invoke_result<Func, Args...>::type;

template <typename Sig> struct result_of; // not defined

template <typename Func, typename... Args>
struct result_of<Func (Args...)> : invoke_result<Func, Args...>
{
};

template <typename Sig> using result_of_t = typename result_of<Sig>::type;

template <typename Func, typename... Args>
struct is_invocable : meta::has_type<invoke_result<Func, Args...>>
{
};

template <typename Func, typename... Args>
LUMEX_CONSTEXPR bool is_invocable_v = is_invocable<Func, Args...>::value;

// Signature-based callable check (e.g. is_callable_signature<Functor(int)>).
// NOTE: kept distinct from is_callable<Func, Args...> below (variadic-args
// form), which is the std::is_invocable-like Callable Named Requirement check
// ported from PeakExpertWeb.
template <typename Sig> struct is_callable_signature;

template <typename Func, typename... Args>
struct is_callable_signature<Func (Args...)> : is_invocable<Func, Args...>
{
};

/**
 * @brief Type trait checking whether Func is Callable with Args... (Callable
 * Named Requirement).
 * @details Equivalent to std::is_invocable (C++17) for C++11; reuses the same
 * INVOKE/invoke_result/has_type machinery defined above in the `detail`
 * namespace.
 * @see https://en.cppreference.com/w/cpp/named_req/Callable
 */
template <typename Func, typename... Args>
struct is_callable : meta::has_type<invoke_result<Func, Args...>>
{
};

// Variable templates are formally a C++14 feature, but this stays ungated:
// LumexExceptionWrapper.hpp (whose own tests are pinned to CXX_STANDARD 11)
// already relies on is_callable_v, and every compiler this library targets
// accepts it under -std=c++11 as a tolerated extension.
template <typename Func, typename... Args>
LUMEX_CONSTEXPR bool is_callable_v = is_callable<Func, Args...>::value;
} // namespace invoke

// ---------------------------------------------------------------------
// stream: can a type be written with `std::ostream << value`?
// ---------------------------------------------------------------------

namespace stream
{
namespace detail
{
/** @brief `std::declval<std::ostream &>() << std::declval<T>()` compiles. */
template <typename T, typename Enable = void>
struct is_streamable_expression : std::false_type
{
};

template <typename T>
struct is_streamable_expression<
    T, meta::void_t<decltype (std::declval<std::ostream &> ()
                              << std::declval<T> ())>> : std::true_type
{
};

/**
 * @brief Smart pointers that lumex::core::string::utility::stringify streams
 * as their raw address below C++20 (its own operator<< overloads, which the
 * expression check above cannot see).
 */
template <typename T> struct is_address_streamed : std::false_type
{
};

#if __cplusplus < 202002L
template <typename T, typename D>
struct is_address_streamed<std::unique_ptr<T, D>> : std::true_type
{
};
template <typename T>
struct is_address_streamed<std::shared_ptr<T>> : std::true_type
{
};
#endif
} // namespace detail

/**
 * @brief `std::declval<std::ostream &>() << std::declval<T>()` is
 * well-formed: exactly what the standard library streams, without the
 * smart-pointer extras of `is_streamable`.
 */
template <typename T>
struct is_ostreamable : detail::is_streamable_expression<T>
{
};

/**
 * @brief Every type in `Args` (after `std::decay`) is `is_ostreamable`; true
 * for an empty pack.
 */
template <typename... Args> struct all_ostreamable;

template <> struct all_ostreamable<> : std::true_type
{
};

template <typename First, typename... Rest>
struct all_ostreamable<First, Rest...>
    : std::integral_constant<
          bool, is_ostreamable<typename std::decay<First>::type>::value
                    && all_ostreamable<Rest...>::value>
{
};

#if __cplusplus >= 201402L
/** @brief `is_ostreamable<std::decay_t<T>>::value`. */
template <typename T>
LUMEX_CONSTEXPR bool is_ostreamable_v = is_ostreamable<std::decay_t<T>>::value;

/** @brief `all_ostreamable<Args...>::value`. */
template <typename... Args>
LUMEX_CONSTEXPR bool all_ostreamable_v = all_ostreamable<Args...>::value;
#endif

/**
 * @brief `std::true_type` when `std::declval<std::ostream &>() <<
 * std::declval<T>()` is well-formed (or, below C++20, `T` is a
 * `std::unique_ptr` / `std::shared_ptr`), otherwise `std::false_type`.
 * @details One primary template decides through `detail` helpers, so no two
 * partial specializations can compete for the same `T`.
 * @tparam T The type to check (not decayed; see `is_streamable_v`).
 * @tparam Enable Kept for SFINAE-style use; leave it defaulted.
 */
template <typename T, typename Enable = void>
struct is_streamable
    : std::integral_constant<bool,
                             detail::is_streamable_expression<T>::value
                                 || detail::is_address_streamed<T>::value>
{
};

/// @cond DO_NOT_DOCUMENT
// Fundamental types, spelled out for C++11 compilers whose expression SFINAE
// is weak.
template <> struct is_streamable<bool> : std::true_type
{
};
template <> struct is_streamable<char> : std::true_type
{
};
template <> struct is_streamable<signed char> : std::true_type
{
};
template <> struct is_streamable<unsigned char> : std::true_type
{
};
template <> struct is_streamable<wchar_t> : std::true_type
{
};
template <> struct is_streamable<short> : std::true_type
{
};
template <> struct is_streamable<unsigned short> : std::true_type
{
};
template <> struct is_streamable<int> : std::true_type
{
};
template <> struct is_streamable<unsigned int> : std::true_type
{
};
template <> struct is_streamable<long> : std::true_type
{
};
template <> struct is_streamable<unsigned long> : std::true_type
{
};
template <> struct is_streamable<long long> : std::true_type
{
};
template <> struct is_streamable<unsigned long long> : std::true_type
{
};
template <> struct is_streamable<float> : std::true_type
{
};
template <> struct is_streamable<double> : std::true_type
{
};
template <> struct is_streamable<long double> : std::true_type
{
};
template <> struct is_streamable<char const *> : std::true_type
{
};
template <> struct is_streamable<char *> : std::true_type
{
};
template <> struct is_streamable<std::string> : std::true_type
{
};
/// @endcond

/**
 * @brief `value` is true when every type in `Args` (after `std::decay`) is
 * streamable; true for an empty pack.
 */
template <typename... Args> struct all_streamable;

template <> struct all_streamable<> : std::true_type
{
};

template <typename First, typename... Rest>
struct all_streamable<First, Rest...>
    : std::integral_constant<
          bool, is_streamable<typename std::decay<First>::type>::value
                    && all_streamable<Rest...>::value>
{
};

#if __cplusplus >= 201402L
/** @brief `is_streamable<std::decay_t<T>>::value`. */
template <typename T>
LUMEX_CONSTEXPR bool is_streamable_v = is_streamable<std::decay_t<T>>::value;

/** @brief `all_streamable<Args...>::value`. */
template <typename... Args>
LUMEX_CONSTEXPR bool all_streamable_v = all_streamable<Args...>::value;
#endif

#if __cplusplus >= 202002L
/** @brief A type that `std::ostream` can write with `operator<<`. */
template <typename T>
concept Streamable = requires (T &&type, std::ostream &ostream) {
  ostream << std::forward<T> (type);
};

/**
 * @brief Every type in `Args` (after `std::decay_t`) is `Streamable`. An
 * empty pack satisfies it.
 */
template <typename... Args>
concept AllStreamable = (Streamable<std::decay_t<Args>> && ...);
#endif
} // namespace stream

// ---------------------------------------------------------------------
// range: can a `Range const &` be walked; container shape
// ---------------------------------------------------------------------

namespace range
{
/**
 * @brief `type` is what `*std::begin(range)` yields for a `Range const &`
 * that has `std::begin`, `std::end`, `!=` and `++`; absent otherwise, so it
 * can drive SFINAE.
 */
template <typename Range, typename Enable = void> struct range_reference
{
};

template <typename Range>
struct range_reference<
    Range,
    meta::void_t<decltype (std::begin (std::declval<Range const &> ())
                           != std::end (std::declval<Range const &> ())),
                 decltype (++std::declval<decltype (std::begin (
                               std::declval<Range const &> ())) &> ()),
                 decltype (*std::begin (std::declval<Range const &> ()))>>
{
  using type = decltype (*std::begin (std::declval<Range const &> ()));
};

/** @brief `Range const &` can be walked (see `range_reference`). */
template <typename Range, typename Enable = void>
struct is_iterable : std::false_type
{
};

template <typename Range>
struct is_iterable<Range, meta::void_t<typename range_reference<Range>::type>>
    : std::true_type
{
};

/**
 * @brief `Range` is iterable and its elements (after `std::decay`) are
 * streamable.
 */
template <typename Range, typename Enable = void>
struct has_streamable_elements : std::false_type
{
};

template <typename Range>
struct has_streamable_elements<
    Range, meta::void_t<typename range_reference<Range>::type>>
    : stream::is_streamable<
          typename std::decay<typename range_reference<Range>::type>::type>
{
};

/**
 * @brief `Range` is iterable and its element reference converts to `To`
 * (for example `std::string const &`).
 */
template <typename Range, typename To, typename Enable = void>
struct has_elements_convertible_to : std::false_type
{
};

template <typename Range, typename To>
struct has_elements_convertible_to<
    Range, To, meta::void_t<typename range_reference<Range>::type>>
    : std::is_convertible<typename range_reference<Range>::type, To>
{
};

/** @brief `T` has a nested type `key_type` (associative containers). */
template <typename T, typename Enable = void>
struct has_key_type : std::false_type
{
};

template <typename T>
struct has_key_type<T, meta::void_t<typename T::key_type>> : std::true_type
{
};

/** @brief `T` has a nested type `mapped_type` (maps). */
template <typename T, typename Enable = void>
struct has_mapped_type : std::false_type
{
};

template <typename T>
struct has_mapped_type<T, meta::void_t<typename T::mapped_type>>
    : std::true_type
{
};

/** @brief `std::declval<T>().size()` exists and converts to `std::size_t`. */
template <typename T, typename Enable = void>
struct has_convertible_size : std::false_type
{
};

template <typename T>
struct has_convertible_size<
    T, meta::void_t<decltype (std::declval<T> ().size ())>>
    : std::is_convertible<decltype (std::declval<T> ().size ()), std::size_t>
{
};

/**
 * @brief `std::declval<T>()[std::size_t]` exists and its result converts to
 * `To` (for example a byte type for a byte buffer).
 */
template <typename T, typename To, typename Enable = void>
struct has_convertible_indexed_access : std::false_type
{
};

template <typename T, typename To>
struct has_convertible_indexed_access<
    T, To,
    meta::void_t<decltype (std::declval<T> ()[std::declval<std::size_t> ()])>>
    : std::is_convertible<
          decltype (std::declval<T> ()[std::declval<std::size_t> ()]), To>
{
};
} // namespace range

// ---------------------------------------------------------------------
// string: contiguous strings of a character type
// ---------------------------------------------------------------------

namespace string
{
/**
 * @brief `T` is a contiguous string of `Char`: it has `data()` convertible to
 * `Char const *`, `size()`, `T::npos` and `T::value_type == Char`.
 * @details True for `std::basic_string`, `std::basic_string_view`,
 * `LumexStringView` / `LumexWStringView`; false for containers such as
 * `std::vector<char>` (no `npos`).
 */
template <typename T, typename Char, typename Enable = void>
struct is_string_like : std::false_type
{
};

template <typename T, typename Char>
struct is_string_like<
    T, Char,
    meta::void_t<decltype (std::declval<T const &> ().data ()),
                 decltype (std::declval<T const &> ().size ()),
                 decltype (T::npos), typename T::value_type>>
    : std::integral_constant<
          bool, std::is_same<typename T::value_type, Char>::value
                    && std::is_convertible<
                        decltype (std::declval<T const &> ().data ()),
                        Char const *>::value>
{
};

/**
 * @brief `T` is a string of its own `value_type` (see `is_string_like`), for
 * any character type: `std::string`, `std::wstring`, string views.
 */
template <typename T, typename Enable = void>
struct is_any_string : std::false_type
{
};

template <typename T>
struct is_any_string<T, meta::void_t<typename T::value_type>>
    : is_string_like<T, typename T::value_type>
{
};
#if __cplusplus >= 202002L
/** @brief Converts to `std::string_view` or `std::string`. */
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>
                     || std::convertible_to<T, std::string>;
#endif
} // namespace string

// ---------------------------------------------------------------------
// tuple: pair-like types
// ---------------------------------------------------------------------

namespace tuple
{
/** @brief `T` is a `std::pair` or a two-element `std::tuple`. */
template <typename T> struct is_pair_like : std::false_type
{
};

template <typename First, typename Second>
struct is_pair_like<std::pair<First, Second>> : std::true_type
{
};

template <typename First, typename Second>
struct is_pair_like<std::tuple<First, Second>> : std::true_type
{
};
} // namespace tuple

// ---------------------------------------------------------------------
// value: optional / expected wrappers
// ---------------------------------------------------------------------

namespace value
{
template <typename T> struct is_optional : std::false_type
{
};

template <typename T> struct is_optional<T const> : is_optional<T>
{
};

template <typename T> struct is_optional<T volatile> : is_optional<T>
{
};

template <typename T> struct is_optional<T const volatile> : is_optional<T>
{
};

template <typename T>
struct is_optional<lumex::core::optional::opt::optional<T>> : std::true_type
{
};

#if __cplusplus >= 201703L
template <typename T> struct is_optional<std::optional<T>> : std::true_type
{
};
#endif

template <typename T>
LUMEX_CONSTEXPR bool is_optional_v = is_optional<T>::value;

/** @brief `T` is a `lumex::core::expected::result::Expected<S, E>`. */
template <typename T> struct is_expected : std::false_type
{
};

template <typename S, typename E>
struct is_expected<lumex::core::expected::result::Expected<S, E>>
    : std::true_type
{
};

template <typename T>
LUMEX_CONSTEXPR bool is_expected_v = is_expected<T>::value;

#if __cplusplus >= 202002L
/** @brief Concept form of `is_expected`. */
template <typename T>
concept is_expected_concept = is_expected<T>::value;
#endif
/**
 * @brief `T` looks like an optional: `has_value ()` and unary `*` work on a
 * `T const &` (std::optional, LumexLib optional, similar wrappers).
 */
template <typename T, typename Enable = void>
struct is_optional_like : std::false_type
{
};

template <typename T>
struct is_optional_like<
    T, meta::void_t<decltype (std::declval<T const &> ().has_value ()),
                    decltype (*std::declval<T const &> ())>> : std::true_type
{
};
} // namespace value

// ---------------------------------------------------------------------
// enums: reflected enums (LUMEX_DEFINE_REFLECTED_ENUM)
// ---------------------------------------------------------------------

namespace enums
{
/**
 * @brief `T` is an enum with a `toString (T)` found by argument-dependent
 * lookup that returns something convertible to `char const *`, as generated
 * by `LUMEX_DEFINE_REFLECTED_ENUM` at namespace scope.
 * @note An enum reflected inside a class gets a static member `toString`,
 * which argument-dependent lookup does not find.
 */
template <typename T, typename Enable = void>
struct is_reflected_enum : std::false_type
{
};

template <typename T>
struct is_reflected_enum<
    T, meta::void_t<decltype (toString (std::declval<T> ()))>>
    : std::integral_constant<
          bool,
          std::is_enum<T>::value
              && std::is_convertible<decltype (toString (std::declval<T> ())),
                                     char const *>::value>
{
};
} // namespace enums

// ---------------------------------------------------------------------
// numeric: arithmetic type pairs
// ---------------------------------------------------------------------

namespace numeric
{
/**
 * @brief Both types (without cv / reference) are arithmetic with a
 * specialized `std::numeric_limits`, so `SafeComparator` can compare them.
 */
template <typename T, typename U> struct is_safe_comparable
{
  using clean_T = meta::CleanType<T>;
  using clean_U = meta::CleanType<U>;
  static bool const value = std::is_arithmetic<clean_T>::value
                            && std::is_arithmetic<clean_U>::value
                            && std::numeric_limits<clean_T>::is_specialized
                            && std::numeric_limits<clean_U>::is_specialized;
};
#if __cplusplus >= 202002L
/** @brief Arithmetic with a specialized `std::numeric_limits`. */
template <typename T>
concept ArithmeticType
    = std::is_arithmetic_v<T> && std::numeric_limits<T>::is_specialized;

/** @brief Both types are `ArithmeticType` (concept form of the trait). */
template <typename T, typename U>
concept SafeComparable = ArithmeticType<T> && ArithmeticType<U>;
#endif
} // namespace numeric
} // namespace traits
} // namespace utility
} // namespace core
} // namespace lumex

/**
 * @brief Primary template for reflected enum metadata generated by
 * LUMEX_DEFINE_ENUM_TRAITS.
 * @note Explicit specializations are generated by LUMEX_DEFINE_ENUM_TRAITS
 *       ([temp.expl.spec]/4 requires this declaration to be reachable).
 * @note This is a lightweight values/first/last/size reflection helper (no
 * toString()). A richer X-macro-based reflection system with toString()
 * support lives in `core/reflection` - do not confuse the two;
 * LUMEX_DEFINE_ENUM_TRAITS intentionally does not reuse that name.
 */
template <typename Enum> struct lumex_enum_traits_t;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LUMEX_DEFINE_ENUM_TRAITS(EnumName, UnderlyingType, ...)               \
  enum class EnumName : UnderlyingType                                        \
  {                                                                           \
    __VA_ARGS__                                                               \
  };                                                                          \
                                                                              \
  template <> struct lumex_enum_traits_t<EnumName>                            \
  {                                                                           \
    static LUMEX_CONSTEXPR auto values = [] {                                 \
      using enum EnumName;                                                    \
      return std::array{ __VA_ARGS__ };                                       \
    }();                                                                      \
    static LUMEX_CONSTEXPR EnumName first = values.front ();                  \
    static LUMEX_CONSTEXPR std::size_t size = values.size ();                 \
    static LUMEX_CONSTEXPR EnumName last = values.back ();                    \
  }

#endif // !LUMEX_CORE_UTILITY_TRAITS_HPP

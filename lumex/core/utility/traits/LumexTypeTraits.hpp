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
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <optional>
#endif

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
namespace utility
{
namespace traits
{
template <typename...> struct make_void
{
  using type = void;
};

template <typename... Ts> using void_t = typename make_void<Ts...>::type;

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
struct invoke_result_impl<
    void_t<decltype (INVOKE (std::declval<F> (), std::declval<Args> ()...))>,
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

template <typename T, typename = void> struct has_type : std::false_type
{
};

template <typename T>
struct has_type<T, void_t<typename T::type>> : std::true_type
{
};

template <typename Func, typename... Args>
struct is_invocable : has_type<invoke_result<Func, Args...>>
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
struct is_callable : has_type<invoke_result<Func, Args...>>
{
};

// Variable templates are formally a C++14 feature, but this stays ungated:
// LumexExceptionWrapper.hpp (whose own tests are pinned to CXX_STANDARD 11)
// already relies on is_callable_v, and every compiler this library targets
// accepts it under -std=c++11 as a tolerated extension.
template <typename Func, typename... Args>
LUMEX_CONSTEXPR bool is_callable_v = is_callable<Func, Args...>::value;

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

// ---------------------------------------------------------------------
// Stream traits: can a type be written with `std::ostream << value`?
// ---------------------------------------------------------------------

namespace detail
{
/** @brief `std::declval<std::ostream &>() << std::declval<T>()` compiles. */
template <typename T, typename Enable = void>
struct is_streamable_expression : std::false_type
{
};

template <typename T>
struct is_streamable_expression<
    T,
    void_t<decltype (std::declval<std::ostream &> () << std::declval<T> ())>>
    : std::true_type
{
};

/**
 * @brief Smart pointers that lumex::core::string::format::stringify streams
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

// ---------------------------------------------------------------------
// Range traits: can a `Range const &` be walked like a range-based `for`?
// ---------------------------------------------------------------------

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
    Range, void_t<decltype (std::begin (std::declval<Range const &> ())
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
struct is_iterable<Range, void_t<typename range_reference<Range>::type>>
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
struct has_streamable_elements<Range,
                               void_t<typename range_reference<Range>::type>>
    : is_streamable<
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
    Range, To, void_t<typename range_reference<Range>::type>>
    : std::is_convertible<typename range_reference<Range>::type, To>
{
};
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
    static LUMEX_CONSTEXPR auto values = []                                   \
      {                                                                       \
        using enum EnumName;                                                  \
        return std::array{ __VA_ARGS__ };                                     \
      }();                                                                    \
    static LUMEX_CONSTEXPR EnumName first = values.front ();                  \
    static LUMEX_CONSTEXPR std::size_t size = values.size ();                 \
    static LUMEX_CONSTEXPR EnumName last = values.back ();                    \
  }

#endif // !LUMEX_CORE_UTILITY_TRAITS_HPP

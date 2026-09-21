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
#include <type_traits>
#include <utility>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if __cplusplus >= 201703L
#include <optional>
#endif

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
template <typename Enum> struct EnumTraits;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LUMEX_DEFINE_ENUM_TRAITS(EnumName, UnderlyingType, ...)               \
  enum class EnumName : UnderlyingType                                        \
  {                                                                           \
    __VA_ARGS__                                                               \
  };                                                                          \
                                                                              \
  template <> struct EnumTraits<EnumName>                                     \
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

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
 * @file LumexAttributes.hpp
 * @brief `LUMEX_ATTRIBUTE_*` macros that spell C++ attributes portably, and
 * the structure packing macros `LUMEX_PACK_BEGIN` and `LUMEX_PACK_END`.
 * @details Each attribute macro expands to the standard attribute when the
 * compiled standard has it, otherwise to a GCC, Clang or MSVC extension with
 * the same effect, and to nothing where neither exists. The macros cover
 * nodiscard (with a reason), noreturn, noinline, packed, carries_dependency,
 * deprecated (with or without a reason), fallthrough, maybe_unused, likely and
 * unlikely, no_unique_address (C++20), assume (C++23) and indeterminate
 * (C++26). `LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR(...)` silences an unused
 * expression by casting it to `void`; on GCC below C++17 it also passes the
 * value on, because GCC reports a `LUMEX_ATTRIBUTE_NODISCARD` result that is
 * only cast to `void` (see the macro). `LUMEX_ATTRIBUTE_PACKED` is empty for
 * MSVC, which packs through `LUMEX_PACK_BEGIN(n)` and `LUMEX_PACK_END`
 * instead; those two work with MSVC, GCC and Clang and are empty elsewhere.
 */
#ifndef LUMEX_CORE_UTILITY_ATTR_HPP
#define LUMEX_CORE_UTILITY_ATTR_HPP

#include <type_traits>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if __cplusplus < 201703L && defined(__GNUC__) && !defined(__clang__)
namespace lumex
{
namespace core
{
namespace utility
{
namespace attr
{
namespace detail
{
/**
 * @brief The right operand of the comma that
 * `LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR` builds on GCC below C++17.
 */
struct unused_value_sink_t
{
};

/**
 * @brief Takes the left operand of the comma and drops it, for a type that is
 * not a class or union (arithmetic, enumeration, pointer, array, function).
 * @details A function argument counts as a use of the value, which a cast to
 * `void` does not for GCC's `warn_unused_result`. The parameter is passed by
 * value so that an array or a function decays: a reference to an array of
 * unknown bound (`int values[] = { ... }` in a template) cannot be formed
 * before C++20.
 * @return The sink, so that the whole expression has a type and can be cast
 * to `void`.
 */
template <typename Value>
LUMEX_CONSTEXPR typename std::enable_if<!std::is_class<Value>::value
                                            && !std::is_union<Value>::value,
                                        unused_value_sink_t>::type
operator, (Value, unused_value_sink_t) LUMEX_NOEXCEPT
{
  return unused_value_sink_t ();
}

/**
 * @brief Takes the left operand of the comma and drops it, for a class or
 * union type.
 * @details The object is only bound to a reference, never read, copied or
 * moved, so a class without a copy constructor works.
 * @return The sink.
 */
template <typename Value>
LUMEX_CONSTEXPR typename std::enable_if<std::is_class<Value>::value
                                            || std::is_union<Value>::value,
                                        unused_value_sink_t>::type
operator, (Value const &, unused_value_sink_t) LUMEX_NOEXCEPT
{
  return unused_value_sink_t ();
}
} // namespace detail
} // namespace attr
} // namespace utility
} // namespace core
} // namespace lumex
#endif

// @link https://en.cppreference.com/w/cpp/language/attributes.html

// [[nodiscard]] and [[nodiscard("reason")]]
#if __cplusplus < 201703L
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_NODISCARD(msg)                                        \
  __attribute__ ((                                                            \
      warn_unused_result)) // https://stackoverflow.com/questions/53169938/attribute-warn-unused-result-vs-attribute-warn-unused-result
#elif defined(_MSC_VER)
#define LUMEX_ATTRIBUTE_NODISCARD(msg)                                        \
  _Check_return_ // https://learn.microsoft.com/en-us/cpp/code-quality/annotating-function-behavior?view=msvc-170
#else
#define LUMEX_ATTRIBUTE_NODISCARD(msg)
#endif
#elif LUMEX_HAS_NODISCARD_MESSAGE
#define LUMEX_ATTRIBUTE_NODISCARD(msg) [[nodiscard (msg)]]
#else // C++17, or a later mode without the reason form (GCC 8 and 9)
#define LUMEX_ATTRIBUTE_NODISCARD(msg)                                        \
  [[nodiscard]] // https://en.cppreference.com/w/cpp/language/attributes/nodiscard
#endif

// --- Lumex Standard Attribute Macros ---

// [[noreturn]]
#if __cplusplus >= 201103L
#define LUMEX_ATTRIBUTE_NORETURN [[noreturn]]
#elif defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_NORETURN __attribute__ ((noreturn))
#elif defined(_MSC_VER)
#define LUMEX_ATTRIBUTE_NORETURN __declspec (noreturn)
#else
#define LUMEX_ATTRIBUTE_NORETURN
#endif

// [[noinline]]
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_NOINLINE __attribute__ ((noinline))
#elif defined(_MSC_VER)
#define LUMEX_ATTRIBUTE_NOINLINE __declspec (noinline)
#else
#define LUMEX_ATTRIBUTE_NOINLINE
#endif

// @link
// https://stackoverflow.com/questions/11770451/what-is-the-meaning-of-attribute-packed-aligned4
// @note For MSVC, use LUMEX_PACK_BEGIN/LUMEX_PACK_END around struct
// definitions instead of LUMEX_ATTRIBUTE_PACKED
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_PACKED __attribute__ ((packed))
#elif defined(_MSC_VER)
// MSVC uses pragma pack instead of attribute-packed
// Use LUMEX_PACK_BEGIN/LUMEX_PACK_END macros around struct definitions
#define LUMEX_ATTRIBUTE_PACKED
#else
#define LUMEX_ATTRIBUTE_PACKED
#endif

// Packing control macros for MSVC compatibility
// @details These macros provide portable packing control across GCC/Clang and
// MSVC
// @note GCC/Clang support #pragma pack for MSVC compatibility, so these macros
// work on all platforms
// @usage:
//   LUMEX_PACK_BEGIN(1)
//   struct MyStruct {
//     char c;
//     int i;
//   };
//   LUMEX_PACK_END
#if defined(_MSC_VER)
#define LUMEX_PACK_BEGIN(alignment) __pragma (pack (push, alignment))
#define LUMEX_PACK_END __pragma (pack (pop))
#elif defined(__GNUC__) || defined(__clang__)
// GCC/Clang support MSVC-style #pragma pack for compatibility
// GCC 4.9+ and Clang support __pragma (same as MSVC)
#if (defined(__GNUC__)                                                        \
     && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)))            \
    || defined(__clang__)
#define LUMEX_PACK_BEGIN(alignment) __pragma (pack (push, alignment))
#define LUMEX_PACK_END __pragma (pack (pop))
#else
// Fallback to _Pragma for older GCC versions
// Define common alignment values directly
#define LUMEX_PACK_BEGIN_1 _Pragma ("pack(push, 1)")
#define LUMEX_PACK_BEGIN_2 _Pragma ("pack(push, 2)")
#define LUMEX_PACK_BEGIN_4 _Pragma ("pack(push, 4)")
#define LUMEX_PACK_BEGIN_8 _Pragma ("pack(push, 8)")
#define LUMEX_PACK_BEGIN(alignment) LUMEX_PACK_BEGIN_##alignment
#define LUMEX_PACK_END _Pragma ("pack(pop)")
#endif
#else
#define LUMEX_PACK_BEGIN(alignment)
#define LUMEX_PACK_END
#endif

// [[carries_dependency]]
#if __cplusplus >= 201103L
#define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY [[carries_dependency]]
#elif defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY __attribute__ ((carries_dependency))
#elif defined(_MSC_VER)
#define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY _CARRIES_DEPENDENCY_
#else
#define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY
#endif

// [[deprecated]] and [[deprecated("reason")]]
#if __cplusplus >= 201402L
#define LUMEX_ATTRIBUTE_DEPRECATED [[deprecated]]
#define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) [[deprecated (msg)]]
#elif defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))
#define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) __attribute__ ((deprecated (msg)))
#elif defined(_MSC_VER)
#define LUMEX_ATTRIBUTE_DEPRECATED __declspec (deprecated)
#define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) __declspec (deprecated (msg))
#else
#define LUMEX_ATTRIBUTE_DEPRECATED
#define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg)
#endif

// [[fallthrough]]
#if __cplusplus >= 201703L
#define LUMEX_ATTRIBUTE_FALLTHROUGH [[fallthrough]]
#elif (defined(__clang__) && __has_cpp_attribute(fallthrough))                \
    || (defined(__GNUC__) && (__GNUC__ >= 7))
#define LUMEX_ATTRIBUTE_FALLTHROUGH __attribute__ ((fallthrough))
#else
#define LUMEX_ATTRIBUTE_FALLTHROUGH
#endif

// [[maybe_unused]]
// Variadic so commas inside template args / call args stay one argument.
//
// A `(void)` cast discards a value on purpose, and Clang and the C++17
// [[nodiscard]] accept it for a result they would otherwise report. Below
// C++17 LUMEX_ATTRIBUTE_NODISCARD is __attribute__ ((warn_unused_result)),
// and GCC still reports (-Wunused-result) a result that is only cast to void.
// There the expression becomes the left operand of a comma whose right operand
// is a sink: the overloaded operator takes the value as an argument, which
// counts as using it, and does nothing with it. The sink works for any
// expression, a void one (the built-in comma) and a name, an array or a
// function too, and evaluates it once.
#if __cplusplus < 201703L && defined(__GNUC__) && !defined(__clang__)
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR(...)                                 \
  (void)((__VA_ARGS__),                                                       \
         ::lumex::core::utility::attr::detail::unused_value_sink_t ())
#else
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR(...) (void)(__VA_ARGS__)
#endif
#if __cplusplus >= 201703L
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED __attribute__ ((unused))
#else
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED
#endif

// [[likely]] / [[unlikely]]
// LUMEX_ATTRIBUTE_LIKELY_COND(cond) and LUMEX_ATTRIBUTE_UNLIKELY_COND(cond)
// wrap a condition and exist in every standard. Below C++20 they pass the hint
// through __builtin_expect on GCC and Clang (the condition is converted to
// bool first, so a pointer works too) and are the plain condition elsewhere.
// The statement attributes LUMEX_ATTRIBUTE_LIKELY and LUMEX_ATTRIBUTE_UNLIKELY
// are empty below C++20: no compiler has a GNU spelling for them (GCC rejects
// __attribute__ ((likely)) before a statement, Clang ignores it with a
// warning).
#if __cplusplus >= 202002L
#define LUMEX_ATTRIBUTE_LIKELY [[likely]]
#define LUMEX_ATTRIBUTE_UNLIKELY [[unlikely]]
#define LUMEX_ATTRIBUTE_LIKELY_COND(cond) (cond)
#define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (cond)
#else
#define LUMEX_ATTRIBUTE_LIKELY
#define LUMEX_ATTRIBUTE_UNLIKELY
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATTRIBUTE_LIKELY_COND(cond) (__builtin_expect (!!(cond), 1))
#define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (__builtin_expect (!!(cond), 0))
#else
#define LUMEX_ATTRIBUTE_LIKELY_COND(cond) (cond)
#define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (cond)
#endif
#endif

// [[no_unique_address]] -> C++20
#if __cplusplus >= 202002L
#define LUMEX_ATTRIBUTE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define LUMEX_ATTRIBUTE_NO_UNIQUE_ADDRESS
#endif

// [[assume(expr)]] -> C++23
#if __cplusplus >= 202302L
#define LUMEX_ATTRIBUTE_ASSUME(expr) [[assume (expr)]]
#else
#define LUMEX_ATTRIBUTE_ASSUME(expr)
#endif

// [[indeterminate]] -> C++26
#if __cplusplus >= 202602L
#define LUMEX_ATTRIBUTE_INDETERMINATE [[indeterminate]]
#else
#define LUMEX_ATTRIBUTE_INDETERMINATE
#endif

// [[optimize_for_synchronized]] -> TM TS
#define LUMEX_ATTRIBUTE_OPTIMIZE_FOR_SYNCHRONIZED

#endif // !LUMEX_CORE_UTILITY_ATTR_HPP

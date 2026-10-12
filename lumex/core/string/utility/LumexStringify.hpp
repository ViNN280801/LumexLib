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
 * @file LumexStringify.hpp
 * @brief Concatenates the `operator<<` text of any number of arguments into
 * one `std::string`.
 *
 * @details `stringify("channel=", 2, " flow=", 1.5)` returns
 * `"channel=2 flow=1.5"`. Every argument must be streamable. A call with an
 * argument that cannot be streamed finds no overload, in every standard: one
 * `std::enable_if` form over `detail::are_stringifiable`, which is the
 * concept `AllStringifiable` from C++20 and the trait
 * `traits::stream::all_streamable` below it (the same argument lists), so the
 * call can be detected with SFINAE and never fails inside the body. Below
 * C++20, `std::unique_ptr` and `std::shared_ptr` stream their raw address.
 *
 * What counts as streamable follows the standard library of the build, not a
 * fixed list: `wchar_t`, `char16_t`, `char32_t` and the pointers to them
 * stream as numbers or addresses up to C++17 (`stringify (L'A')` is `"65"`),
 * and have no overload from C++20, where the library deletes
 * `operator<<` of a narrow stream for them (see
 * `traits::stream::is_streamable`).
 *
 * `stringify_v2` is the same function under its older name, kept as a
 * forwarder: it has no condition of its own, the one of `stringify` decides.
 *
 * Nothing here is placed in the global namespace, so the names cannot clash
 * with a consumer's own `stringify`. Call it qualified, or bring it in with a
 * local `using lumex::core::string::utility::stringify;`.
 */
#ifndef LUMEX_CORE_STRING_UTILITY_STRINGIFY_HPP
#define LUMEX_CORE_STRING_UTILITY_STRINGIFY_HPP

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace string
{
namespace utility
{
/**
 * @brief Streams the raw address held by a `std::unique_ptr`.
 * @details Declared here so `stringify` finds it by ordinary lookup. The
 * standard library has the same overload only since C++20 and only in newer
 * releases; where it has one, this one is more specialized and prints the
 * same text.
 */
template <typename T, typename D>
std::ostream &
operator<< (std::ostream &ostream, std::unique_ptr<T, D> const &ptr)
{
  return ostream << ptr.get ();
}

/** @brief Streams the raw address held by a `std::shared_ptr`. */
template <typename T>
std::ostream &
operator<< (std::ostream &ostream, std::shared_ptr<T> const &ptr)
{
  return ostream << ptr.get ();
}

namespace detail
{
#if LUMEX_HAS_CONCEPTS
/**
 * @brief `stringify` accepts every type in `Args`: the argument lists of the
 * concept `AllStringifiable` (C++20).
 */
template <typename... Args>
struct are_stringifiable
    : std::integral_constant<bool, lumex::core::utility::traits::stream::
                                       detail::AllStringifiable<Args...>>
{
};
#else
/**
 * @brief `stringify` accepts every type in `Args`: the argument lists of
 * `traits::stream::all_streamable` (below C++20, or where the compiler has no
 * concepts).
 */
template <typename... Args>
struct are_stringifiable
    : lumex::core::utility::traits::stream::all_streamable<Args...>
{
};
#endif
} // namespace detail

/**
 * @brief Concatenates the streamed text of `args`.
 * @details Takes part in overload resolution only when every argument is
 * streamable (`detail::are_stringifiable`), in every standard.
 * @return The text. Calling it with no arguments picks the overload below.
 * @note O(total length of the text).
 */
template <typename... Args,
          typename std::enable_if<detail::are_stringifiable<Args...>::value,
                                  int>::type
          = 0>
std::string
stringify (Args &&...args)
{
  std::ostringstream oss;
#if LUMEX_HAS_FOLD_EXPRESSIONS
  ((oss << std::forward<Args> (args)), ...);
#else
  int expanded[] = { 0, ((oss << std::forward<Args> (args)), 0)... };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expanded);
#endif
  return oss.str ();
}

/**
 * @brief No arguments: an empty string, without creating a stream.
 */
inline std::string
stringify () LUMEX_NOEXCEPT
{
  return {};
}

/**
 * @brief Same as `stringify`, kept under its name: it was the variant of
 * `stringify` that could be detected with SFINAE.
 * @details The return type is that of `stringify` for the same arguments, so
 * the overload exists exactly when `stringify` accepts them.
 * @return The text, or an empty string for no arguments.
 */
template <typename... Args>
auto
stringify_v2 (Args &&...args)
    -> decltype (lumex::core::string::utility::stringify (
        std::forward<Args> (args)...))
{
  // Qualified, so that no function of that name found by ADL on the argument
  // types takes the call.
  return lumex::core::string::utility::stringify (
      std::forward<Args> (args)...);
}
} // namespace utility
} // namespace string
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_UTILITY_STRINGIFY_HPP

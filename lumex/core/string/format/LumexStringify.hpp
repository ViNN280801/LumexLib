/**
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
 * `"channel=2 flow=1.5"`. Every argument must be streamable: a C++20 concept
 * constraint, a `static_assert` below C++20. Below C++20, `std::unique_ptr`
 * and `std::shared_ptr` stream their raw address.
 *
 * Nothing here is placed in the global namespace, so the names cannot clash
 * with a consumer's own `stringify`. Call it qualified, or bring it in with a
 * local `using lumex::core::string::format::stringify;`.
 */
#ifndef LUMEX_CORE_STRING_FORMAT_HPP
#define LUMEX_CORE_STRING_FORMAT_HPP

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace string
{
namespace format
{
#if __cplusplus < 202002L
/**
 * @brief Streams the raw address held by a `std::unique_ptr`.
 * @details Declared here so `stringify` finds it by ordinary lookup.
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
#endif

#if __cplusplus >= 202002L

/**
 * @brief Concatenates the streamed text of `args`.
 * @return The text, or an empty string for no arguments.
 * @note O(total length of the text).
 */
template <lumex::core::utility::traits::AllStreamable... Args>
std::string
stringify (Args &&...args)
{
  LUMEX_CONSTEXPR_IF (sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args> (args)), ...);
    return oss.str ();
  }
}

#elif __cplusplus >= 201703L

/** @copydoc stringify */
template <typename... Args>
std::string
stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::all_streamable_v<Args...>,
      "All arguments must be streamable");

  LUMEX_CONSTEXPR_IF (sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args> (args)), ...);
    return oss.str ();
  }
}

#elif __cplusplus >= 201402L

/** @copydoc stringify */
template <typename... Args>
std::string
stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::all_streamable_v<Args...>,
      "All arguments must be streamable");

  std::ostringstream oss;
  int expanded[] = { 0, ((oss << std::forward<Args> (args)), 0)... };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expanded);
  return oss.str ();
}

#else

/** @copydoc stringify */
template <typename... Args>
std::string
stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::all_streamable<Args...>::value,
      "All arguments must be streamable");

  std::ostringstream oss;
  int expanded[] = { 0, ((oss << std::forward<Args> (args)), 0)... };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expanded);
  return oss.str ();
}

#endif

/**
 * @brief No arguments: an empty string, without creating a stream.
 */
inline std::string
stringify () LUMEX_NOEXCEPT
{
  return {};
}

#if __cplusplus >= 202002L

/**
 * @brief Same result as `stringify`, constrained with a `requires`
 * clause instead of a constrained template parameter.
 */
template <typename... Args>
  requires lumex::core::utility::traits::AllStreamable<Args...>
std::string
stringify_v2 (Args &&...args)
{
  LUMEX_CONSTEXPR_IF (sizeof...(args) == 0) { return {}; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args> (args)), ...);
    return oss.str ();
  }
}

#endif
} // namespace format
} // namespace string
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_FORMAT_HPP

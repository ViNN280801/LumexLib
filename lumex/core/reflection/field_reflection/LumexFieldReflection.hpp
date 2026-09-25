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
 * @file LumexFieldReflection.hpp
 * @brief Serializes a simple aggregate (no constructors, no private
 *        members, no base classes) to JSON by walking its fields.
 * @details The header parses as C++11 (`tuple_size`). Indexed `get`
 *          is C++14 (friend-injected field types). `to_json` needs
 *          field names, which `LumexAggregateFields.hpp` can recover
 *          only from C++20 (`LUMEX_FUNCTION_NAME` of a pointer NTTP).
 *          JSON is vendored nlohmann 3.12.0 at
 *          `3rdparty/nlohmann/json.hpp`. Optional-like fields
 *          (`has_value` plus unary `*`) are omitted when empty, so
 *          both `std::optional` and `optional` work.
 */

#ifndef LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP
#define LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include <nlohmann/json.hpp>

namespace lumex
{
namespace core
{
namespace reflection
{
namespace field_reflection
{
namespace detail
{
template <typename... T>
void
swallow (T const &...) LUMEX_NOEXCEPT
{
}

template <typename U>
void
append_one (
    nlohmann::json &j, char const *name, U const &field,
    typename std::enable_if<
        lumex::core::utility::traits::value::is_optional_like<U>::value,
        int>::type
    = 0)
{
  if (field.has_value ())
    j[name] = *field;
}

template <typename U>
void
append_one (
    nlohmann::json &j, char const *name, U const &field,
    typename std::enable_if<
        !lumex::core::utility::traits::value::is_optional_like<U>::value,
        int>::type
    = 0)
{
  j[name] = field;
}

#if __cplusplus >= 202002L
template <typename Struct, std::size_t I>
void
append_field (nlohmann::json &j, Struct const &obj, char const *name)
{
  append_one (j, name, get<I> (obj));
}

template <typename Struct, std::size_t... I>
void
append_all (nlohmann::json &j, Struct const &obj,
            std::array<char const *, sizeof...(I)> const &names,
            index_sequence<I...>)
{
  swallow ((append_field<Struct, I> (j, obj, names[I]), 0)...);
}
#endif
} // namespace detail

/**
 * @brief Serializes a simple aggregate to JSON, visiting every field.
 * @details Rule per field:
 *          - a type with `has_value()` and unary `*` is written only
 *            if `has_value()` is true;
 *          - everything else is always written.
 * @tparam Struct A simple standard-layout aggregate (no user
 *         constructors, no private or protected members, no base
 *         classes, no virtual functions), at most 32 fields.
 * @param obj The aggregate instance to serialize.
 * @return A `nlohmann::json` object with one key per written field,
 *         named after the field.
 */
#if __cplusplus >= 202002L
template <typename Struct>
inline nlohmann::json
to_json (Struct const &obj)
{
  nlohmann::json j = nlohmann::json::object ();
  std::array<char const *, tuple_size<Struct>::value> const names
      = names_as_array<Struct> ();
  detail::append_all (j, obj, names,
                      typename detail::make_index_sequence<
                          tuple_size<Struct>::value>::type ());
  return j;
}
#endif
} // namespace field_reflection
} // namespace reflection
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP

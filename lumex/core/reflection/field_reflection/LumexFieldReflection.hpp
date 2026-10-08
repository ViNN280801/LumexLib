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
 * @file LumexFieldReflection.hpp
 * @brief Serializes a simple aggregate (no constructors, no private
 *        members, no base classes) to JSON by walking its fields.
 * @details Works from C++11. `to_json` needs the field names of the
 *          aggregate. They come from a registration next to the type,
 *          `LUMEX_DEFINE_FIELD_NAMES (Type, a, b, c)` of
 *          `LumexAggregateFields.hpp`, in every standard; without one,
 *          from C++20 the compiler supplies them (`LUMEX_FUNCTION_NAME` of
 *          a pointer NTTP), and below C++20 `to_json` of an unregistered
 *          type is a `static_assert` that names the macro. The fields are
 *          read with `get<I>`: a registered aggregate through its registered
 *          members, any other from C++14 through the automatic `get`. JSON
 *          is vendored nlohmann 3.12.0 at `3rdparty/nlohmann/json.hpp` (it
 *          needs C++11). A field of an
 *          aggregate type is converted by nlohmann in the usual way, that is
 *          through a `to_json (nlohmann::json &, Type const &)` that ADL
 *          finds; it may call `field_reflection::to_json`. Optional-like
 *          fields (`has_value` plus unary `*`) are omitted when empty, so
 *          both `std::optional` and `optional` work.
 */

#ifndef LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP
#define LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <nlohmann/json.hpp>

#include "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

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

// Aggregates whose names are available (registered, or from the compiler at
// C++20, or without fields).
template <typename Struct>
inline nlohmann::json
to_json_fields (Struct const &obj, std::true_type)
{
  nlohmann::json j = nlohmann::json::object ();
  std::array<char const *, tuple_size<Struct>::value> const names
      = names_as_array<Struct> ();
  append_all (
      j, obj, names,
      typename make_index_sequence<tuple_size<Struct>::value>::type ());
  return j;
}

// No names: the static_assert is the only diagnostic.
template <typename Struct>
inline nlohmann::json
to_json_fields (Struct const &, std::false_type)
{
  LUMEX_STATIC_ASSERT_MSG (
      sizeof (Struct) == 0,
      "to_json needs the field names of the aggregate: register them with "
      "LUMEX_DEFINE_FIELD_NAMES (Type, field, ...) next to the definition of "
      "the type, or build for C++20 (names then come from the compiler)");
  return nlohmann::json::object ();
}
} // namespace detail

/**
 * @brief Serializes a simple aggregate to JSON, visiting every field.
 * @details Rule per field:
 *          - a type with `has_value()` and unary `*` is written only
 *            if `has_value()` is true;
 *          - everything else is always written.
 *
 *          The keys are the names of the fields: the registered ones
 *          (`LUMEX_DEFINE_FIELD_NAMES`) in every standard, otherwise, from
 *          C++20, the ones the compiler supplies. Below C++20 an aggregate
 *          without a registration is a `static_assert`, except an aggregate
 *          without fields, which gives an empty object.
 * @tparam Struct A simple standard-layout aggregate (no user
 *         constructors, no private or protected members, no base
 *         classes, no virtual functions), at most 32 fields.
 * @param obj The aggregate instance to serialize.
 * @return A `nlohmann::json` object with one key per written field,
 *         named after the field.
 */
template <typename Struct>
inline nlohmann::json
to_json (Struct const &obj)
{
  typedef typename std::remove_cv<Struct>::type bare_t;
  return detail::to_json_fields (
      obj, std::integral_constant<bool, detail::names_source<bare_t>::value
                                            != 3> ());
}
} // namespace field_reflection
} // namespace reflection
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP

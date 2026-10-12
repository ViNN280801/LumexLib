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
 *          needs C++11).
 *
 *          A field is written by this rule, the first that fits:
 *          - an optional-like field (`has_value` plus unary `*`) is omitted
 *            when empty, otherwise its value is written by this rule, so both
 *            `std::optional` and `optional` work;
 *          - a type that nlohmann can convert (numbers, `bool`, strings,
 *            enums, `nlohmann::json`, standard containers of such types, and
 *            any type with a `to_json (nlohmann::json &, Type const &)` that
 *            ADL finds) is converted by nlohmann: an own `to_json` of an
 *            aggregate therefore has priority over the recursion below, and
 *            it may call `field_reflection::to_json` itself;
 *          - a map whose keys convert to `std::string` becomes an object, any
 *            other container (or `std::array`) an array, each element by
 *            this rule: containers of aggregates, and containers of
 *            containers, are written element by element;
 *          - any other class is taken for a nested aggregate and written as
 *            a JSON object by `to_json` itself, with the names of its own
 *            registration or, from C++20, of the compiler. Below C++20 a
 *            nested aggregate without a registration is the `static_assert`
 *            of `to_json` that names the macro.
 */

#ifndef LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP
#define LUMEX_CORE_REFLECTION_FIELD_REFLECTION_FIELD_REFLECTION_HPP

#include <array>
#include <cstddef>
#include <string>
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
// Declared here, defined below: a nested aggregate is written by to_json
// itself.
template <typename Struct> inline nlohmann::json to_json (Struct const &obj);

namespace detail
{
template <typename... T>
void
swallow (T const &...) LUMEX_NOEXCEPT
{
}

// How a value is written (see value_kind).
LUMEX_CONSTEXPR int k_kind_direct = 0;
LUMEX_CONSTEXPR int k_kind_optional = 1;
LUMEX_CONSTEXPR int k_kind_map = 2;
LUMEX_CONSTEXPR int k_kind_array = 3;
LUMEX_CONSTEXPR int k_kind_aggregate = 4;

// nlohmann converts U: a builtin type, a string, an enum, nlohmann::json, a
// standard container of such types, or a type with a `to_json (json &, U
// const &)` that ADL finds. nlohmann's constructor from a compatible type is
// constrained by exactly that, so this is the test of whether it can.
template <typename U>
struct is_json_convertible : std::is_constructible<nlohmann::json, U const &>
{
};

// A map: iterable, with a key type and a mapped type.
template <typename U>
struct is_map_like
    : std::integral_constant<
          bool,
          lumex::core::utility::traits::range::is_iterable<U>::value
              && lumex::core::utility::traits::range::has_key_type<U>::value
              && lumex::core::utility::traits::range::has_mapped_type<
                  U>::value>
{
};

// A class that is none of the above is taken for a nested aggregate.
template <typename U>
struct is_aggregate_candidate
    : std::integral_constant<bool, std::is_class<U>::value
                                       && !std::is_union<U>::value>
{
};

// The rule that applies to a value of type U, the first that fits. Every
// condition is a SFINAE-friendly trait, none looks into U as an aggregate (a
// non-aggregate class must not reach aggregate_traits).
template <typename U>
struct value_kind
    : std::integral_constant<
          int,
          is_json_convertible<U>::value ? k_kind_direct
          : lumex::core::utility::traits::value::is_optional_like<U>::value
              ? k_kind_optional
          : is_map_like<U>::value ? k_kind_map
          : lumex::core::utility::traits::range::is_iterable<U>::value
              ? k_kind_array
          : is_aggregate_candidate<U>::value ? k_kind_aggregate
                                             : k_kind_direct>
{
};

template <typename U> nlohmann::json to_value (U const &value);

template <typename U, int Kind = value_kind<U>::value> struct value_writer;

// nlohmann converts it (or it is left to nlohmann to report that it cannot).
template <typename U> struct value_writer<U, k_kind_direct>
{
  static nlohmann::json
  write (U const &value)
  {
    return nlohmann::json (value);
  }
};

// An optional-like value inside a container or another optional: null when
// empty. (As a field of an aggregate an empty one is omitted instead.)
template <typename U> struct value_writer<U, k_kind_optional>
{
  static nlohmann::json
  write (U const &value)
  {
    if (!value.has_value ())
      return nlohmann::json (nullptr);
    return to_value (*value);
  }
};

// A map with string keys: an object, each mapped value by the same rule.
template <typename U> struct value_writer<U, k_kind_map>
{
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_constructible<std::string,
                             typename U::key_type const &>::value),
      "to_json writes a map of aggregates (or of other values nlohmann cannot "
      "convert) only with keys that std::string can be made from");

  static nlohmann::json
  write (U const &value)
  {
    nlohmann::json j = nlohmann::json::object ();
    for (auto const &entry : value)
      j[std::string (entry.first)] = to_value (entry.second);
    return j;
  }
};

// Any other container, std::array included: an array, each element by the
// same rule.
template <typename U> struct value_writer<U, k_kind_array>
{
  static nlohmann::json
  write (U const &value)
  {
    nlohmann::json j = nlohmann::json::array ();
    for (auto const &element : value)
      j.push_back (to_value (element));
    return j;
  }
};

// A nested aggregate: an object, with its own names.
template <typename U> struct value_writer<U, k_kind_aggregate>
{
  static nlohmann::json
  write (U const &value)
  {
    return ::lumex::core::reflection::field_reflection::to_json (value);
  }
};

template <typename U>
nlohmann::json
to_value (U const &value)
{
  return value_writer<U>::write (value);
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
    j[name] = to_value (*field);
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
  j[name] = to_value (field);
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
 * @details Rule per field, the first that fits (the file comment has the
 *          reasons):
 *          - a type with `has_value()` and unary `*` is written only if
 *            `has_value()` is true, its value by this same rule;
 *          - a type nlohmann can convert (numbers, strings, enums, standard
 *            containers of those, `nlohmann::json`, and any type with a
 *            `to_json (nlohmann::json &, Type const &)` found by ADL, which
 *            therefore has priority over the next lines) is converted by
 *            nlohmann;
 *          - a map with keys convertible to `std::string` is an object, any
 *            other container or `std::array` an array, each element by this
 *            rule (an empty optional-like element is `null`);
 *          - any other class is a nested aggregate and becomes an object
 *            through `to_json` itself, at any depth.
 *
 *          The keys are the names of the fields: the registered ones
 *          (`LUMEX_DEFINE_FIELD_NAMES`) in every standard, otherwise, from
 *          C++20, the ones the compiler supplies. Below C++20 an aggregate
 *          without a registration, the nested ones included, is a
 *          `static_assert`, except an aggregate without fields, which gives an
 *          empty object. A nested aggregate with a `to_json` of its own
 *          needs no names.
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

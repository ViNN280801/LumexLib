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
 * @file LumexJsonSchemaTraverser.hpp
 * @brief Recursive walker for a JSON Schema draft-7 subset.
 *
 * @details Supported keywords: `type` (a string or an array of type names),
 * `properties`, `required`, `items` (a single schema), `const`, `enum`.
 * Everything else in the schema is ignored. The walker is API-agnostic: the
 * validator and the normalizer base both call it.
 */
#ifndef LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_TRAVERSER_HPP
#define LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_TRAVERSER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumex/applied/json/schema/LumexJsonSchemaException.hpp"

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace schema
{
/**
 * @brief What one walk produces.
 * @details `validate_only` runs every type, `required`, `const` and `enum`
 * check and throws on the first violation, but never builds an output tree
 * (the result is `null`). `validate_and_normalize` runs the same checks and
 * also returns the canonical, schema-shaped document: unknown keys are
 * dropped, a missing required leaf becomes `null`, a missing optional field
 * is omitted. One recursive walk serves both modes so they cannot drift.
 */
LUMEX_DEFINE_REFLECTED_ENUM (LumexJsonSchemaCheckMode, std::uint8_t,
                             (validate_only), (validate_and_normalize))

/**
 * @class LumexJsonSchemaTraverser
 * @brief Stateless schema walker. All members are static.
 *
 * @details Rules per node:
 * - `const` present: `null` input passes; any other value must equal it.
 * - `type` contains `object`: the input must be an object. Each property in
 *   `properties` is walked when present and not `null`. A missing or `null`
 *   required object/array is an error unless its own `type` allows `null`
 *   (then the result holds `null`). A missing required leaf becomes `null`.
 * - `type` contains `array`: the input must be an array. With `items`, every
 *   element is walked; without it, the array passes through unchanged.
 * - Otherwise the node is a leaf: `null` input passes; any other value must
 *   match one of the listed types and, when present, `enum`. A float with a
 *   zero fractional part satisfies `integer`.
 *
 * A schema node without `const` must have `type`; a node missing both throws
 * `nlohmann::json::out_of_range`.
 */
class LumexJsonSchemaTraverser final
{
public:
  LumexJsonSchemaTraverser () = delete;

  /**
   * @brief Walks `input` against `schema`.
   * @param schema The schema tree itself (`type`, `properties`, ...), not an
   * envelope around it.
   * @param input The parsed document.
   * @param mode Whether to build the normalized result.
   * @param path JSON path of `input`, used in exception messages.
   * @return The normalized document for `validate_and_normalize`, `null` for
   * `validate_only`.
   * @throws LumexJsonSchemaException On the first violation.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the normalized document is the result")
  static nlohmann::json
  run (nlohmann::json const &schema, nlohmann::json const &input,
       LumexJsonSchemaCheckMode mode
       = LumexJsonSchemaCheckMode::validate_and_normalize,
       std::string const &path = "$")
  {
    return _traverse_node (schema, input, path, mode);
  }

  /**
   * @brief Parses raw text into a document.
   * @throws LumexJsonSchemaException With `parse_error` and path `$` when the
   * text is not valid JSON (an empty view included).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the parsed document is the result")
  static nlohmann::json
  parse (lumex::core::string_view::view::LumexStringView raw)
  {
    try
      {
        if (raw.data () == nullptr)
          return nlohmann::json::parse (std::string ());
        return nlohmann::json::parse (raw.data (), raw.data () + raw.size ());
      }
    catch (nlohmann::json::parse_error const &exc)
      {
        throw LumexJsonSchemaException (LumexJsonSchemaFailure::parse_error,
                                        "$", exc.what ());
      }
  }

private:
  static bool
  _type_list_contains (nlohmann::json const &type_field,
                       char const *name) LUMEX_NOEXCEPT
  {
    if (type_field.is_string ())
      return type_field.get_ref<std::string const &> () == name;
    if (type_field.is_array ())
      {
        for (nlohmann::json::const_iterator it = type_field.cbegin ();
             it != type_field.cend (); ++it)
          {
            if (it->is_string ()
                && it->get_ref<std::string const &> () == name)
              return true;
          }
      }
    return false;
  }

  static bool
  _is_composite_type (nlohmann::json const &type_field) LUMEX_NOEXCEPT
  {
    return _type_list_contains (type_field, "object")
           || _type_list_contains (type_field, "array");
  }

  static bool
  _matches_leaf_type (nlohmann::json const &type_field,
                      nlohmann::json const &value)
  {
    if (_type_list_contains (type_field, "string") && value.is_string ())
      return true;
    if (_type_list_contains (type_field, "boolean") && value.is_boolean ())
      return true;
    if (_type_list_contains (type_field, "number") && value.is_number ())
      return true;
    if (_type_list_contains (type_field, "null") && value.is_null ())
      return true;
    if (_type_list_contains (type_field, "integer"))
      {
        if (value.is_number_integer ())
          return true;
        // Producers that compute a whole number in floating point still
        // satisfy "integer" when the fractional part is zero.
        if (value.is_number_float ())
          {
            double const number = value.get<double> ();
            if (std::floor (number) == number)
              return true;
          }
      }
    return false;
  }

  static bool
  _contains_value (nlohmann::json const &list, nlohmann::json const &value)
  {
#if __cplusplus >= 202002L && defined(__cpp_lib_ranges)
    return std::ranges::find (list, value) != list.end ();
#else
    return std::find (list.begin (), list.end (), value) != list.end ();
#endif
  }

  static bool
  _contains_name (std::vector<std::string> const &names,
                  std::string const &name)
  {
#if __cplusplus >= 202002L && defined(__cpp_lib_ranges)
    return std::ranges::find (names, name) != names.end ();
#else
    return std::find (names.begin (), names.end (), name) != names.end ();
#endif
  }

  static nlohmann::json
  _traverse_leaf (nlohmann::json const &schema, nlohmann::json const &input,
                  std::string const &path, LumexJsonSchemaCheckMode mode)
  {
    if (input.is_null ())
      return nullptr;

    nlohmann::json const &type_field = schema.at ("type");
    if (!_matches_leaf_type (type_field, input))
      {
        throw LumexJsonSchemaException (
            LumexJsonSchemaFailure::type_mismatch, path,
            "expected type " + type_field.dump () + ", got "
                + input.type_name () + " (" + input.dump () + ")");
      }

    nlohmann::json::const_iterator const allowed = schema.find ("enum");
    if (allowed != schema.cend () && !_contains_value (*allowed, input))
      {
        throw LumexJsonSchemaException (
            LumexJsonSchemaFailure::enum_mismatch, path,
            "expected one of " + allowed->dump () + ", got " + input.dump ());
      }
    return mode == LumexJsonSchemaCheckMode::validate_and_normalize
               ? input
               : nlohmann::json (nullptr);
  }

  static nlohmann::json
  _traverse_object (nlohmann::json const &schema, nlohmann::json const &input,
                    std::string const &path, LumexJsonSchemaCheckMode mode)
  {
    if (!input.is_object ())
      {
        throw LumexJsonSchemaException (
            LumexJsonSchemaFailure::missing_required, path,
            std::string ("expected object, got ") + input.type_name ());
      }

    bool const build
        = mode == LumexJsonSchemaCheckMode::validate_and_normalize;
    nlohmann::json::const_iterator const properties
        = schema.find ("properties");
    if (properties == schema.cend ())
      return build ? nlohmann::json::object () : nlohmann::json (nullptr);

    std::vector<std::string> required;
    nlohmann::json::const_iterator const required_field
        = schema.find ("required");
    if (required_field != schema.cend ())
      {
        for (nlohmann::json::const_iterator it = required_field->cbegin ();
             it != required_field->cend (); ++it)
          required.push_back (it->get<std::string> ());
      }

    nlohmann::json result
        = build ? nlohmann::json::object () : nlohmann::json (nullptr);
    for (nlohmann::json::const_iterator it = properties->cbegin ();
         it != properties->cend (); ++it)
      {
        std::string const &name = it.key ();
        nlohmann::json const &property_schema = it.value ();
        std::string const child_path = path + "." + name;

        nlohmann::json::const_iterator const child = input.find (name);
        if (child != input.cend () && !child->is_null ())
          {
            nlohmann::json value
                = _traverse_node (property_schema, *child, child_path, mode);
            if (build)
              result[name] = std::move (value);
            continue;
          }

        if (!_contains_name (required, name))
          continue; // optional and missing: omitted from the result

        nlohmann::json::const_iterator const property_type
            = property_schema.find ("type");
        bool const composite = property_type != property_schema.cend ()
                               && _is_composite_type (*property_type);
        if (composite)
          {
            // A required object/array whose own type also allows "null" may
            // be absent or null: both normalize to null. Otherwise it is an
            // error.
            if (_type_list_contains (*property_type, "null"))
              {
                if (build)
                  result[name] = nullptr;
                continue;
              }
            throw LumexJsonSchemaException (
                LumexJsonSchemaFailure::missing_required, child_path,
                "required object/array field is missing from input");
          }

        if (build)
          result[name] = nullptr; // required leaf, missing: null
      }
    return result;
  }

  static nlohmann::json
  _traverse_array (nlohmann::json const &schema, nlohmann::json const &input,
                   std::string const &path, LumexJsonSchemaCheckMode mode)
  {
    if (!input.is_array ())
      {
        throw LumexJsonSchemaException (
            LumexJsonSchemaFailure::missing_required, path,
            std::string ("expected array, got ") + input.type_name ());
      }

    bool const build
        = mode == LumexJsonSchemaCheckMode::validate_and_normalize;
    nlohmann::json::const_iterator const items = schema.find ("items");
    if (items == schema.cend ())
      return build ? input : nlohmann::json (nullptr); // untyped array

    nlohmann::json result
        = build ? nlohmann::json::array () : nlohmann::json (nullptr);
    for (std::size_t index = 0; index < input.size (); ++index)
      {
        nlohmann::json value
            = _traverse_node (*items, input[index],
                              path + "[" + std::to_string (index) + "]", mode);
        if (build)
          result.push_back (std::move (value));
      }
    return result;
  }

  static nlohmann::json
  _traverse_node (nlohmann::json const &schema, nlohmann::json const &input,
                  std::string const &path, LumexJsonSchemaCheckMode mode)
  {
    nlohmann::json::const_iterator const constant = schema.find ("const");
    if (constant != schema.cend ())
      {
        if (input.is_null ())
          return nullptr;
        if (input != *constant)
          {
            throw LumexJsonSchemaException (
                LumexJsonSchemaFailure::const_mismatch, path,
                "expected const " + constant->dump () + ", got "
                    + input.dump ());
          }
        return mode == LumexJsonSchemaCheckMode::validate_and_normalize
                   ? input
                   : nlohmann::json (nullptr);
      }

    nlohmann::json const &type_field = schema.at ("type");
    if (_type_list_contains (type_field, "object"))
      return _traverse_object (schema, input, path, mode);
    if (_type_list_contains (type_field, "array"))
      return _traverse_array (schema, input, path, mode);
    return _traverse_leaf (schema, input, path, mode);
  }
};
} // namespace schema
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_TRAVERSER_HPP

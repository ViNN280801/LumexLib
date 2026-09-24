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
 * @file LumexJsonSchemaValidator.hpp
 * @brief Validate-only checker for one schema tree.
 */
#ifndef LUMEX_APPLIED_JSON_VALIDATION_JSON_SCHEMA_VALIDATOR_HPP
#define LUMEX_APPLIED_JSON_VALIDATION_JSON_SCHEMA_VALIDATOR_HPP

#include <exception>
#include <utility>

#include <nlohmann/json.hpp>

#include "lumex/applied/json/schema/LumexJsonSchemaTraverser.hpp"
#include "lumex/applied/json/validation/ILumexJsonSchemaValidator.hpp"

#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace validation
{
/**
 * @class LumexJsonSchemaValidator
 * @brief Parses raw JSON text and walks it against one schema tree in
 *        `validate_only` mode. It never normalizes or rewrites fields; use a
 *        `LumexJsonSchemaNormalizer` subclass for that.
 */
class LumexJsonSchemaValidator final : public ILumexJsonSchemaValidator
{
public:
  /**
   * @param schema The schema tree itself (`type`, `properties`, ...), not an
   * envelope around it.
   * @param strict `true`: throw on the first violation. `false`: report the
   * violation through the `error` argument and return normally.
   */
  explicit LumexJsonSchemaValidator (nlohmann::json schema, bool strict = true)
      : _schema (std::move (schema)), _strict (strict)
  {
  }

  /** @brief The schema tree this validator checks against. */
  LUMEX_ATTRIBUTE_NODISCARD ("the schema is the point of the call")
  nlohmann::json const &
  schema () const LUMEX_NOEXCEPT
  {
    return _schema;
  }

  /** @brief Whether violations throw (`true`) or are reported (`false`). */
  LUMEX_ATTRIBUTE_NODISCARD ("the mode is the point of the call")
  bool
  is_strict () const LUMEX_NOEXCEPT
  {
    return _strict;
  }

  /**
   * @brief Strict: throws on the first violation. Non-strict: never throws
   * for a violation and discards it; use the `error` overload to read it.
   * @throws lumex::applied::json::schema::LumexJsonSchemaException Strict
   * mode only.
   */
  void
  validate (lumex::core::string_view::view::LumexStringView raw) const override
  {
    std::exception_ptr ignored;
    validate (raw, ignored);
  }

  void
  validate (lumex::core::string_view::view::LumexStringView raw,
            std::exception_ptr &error) const override
  {
    error = nullptr;
    try
      {
        _check (raw);
      }
    catch (std::exception const &)
      {
        if (_strict)
          throw;
        error = std::current_exception ();
      }
  }

private:
  void
  _check (lumex::core::string_view::view::LumexStringView raw) const
  {
    using lumex::applied::json::schema::LumexJsonSchemaCheckMode;
    using lumex::applied::json::schema::LumexJsonSchemaTraverser;
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (LumexJsonSchemaTraverser::run (
        _schema, LumexJsonSchemaTraverser::parse (raw),
        LumexJsonSchemaCheckMode::validate_only));
  }

  nlohmann::json _schema;
  bool _strict;
};
} // namespace validation
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_VALIDATION_JSON_SCHEMA_VALIDATOR_HPP

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
 * @file LumexJsonSchemaNormalizer.hpp
 * @brief Abstract base for per-operation normalizers built on the schema
 * walker.
 */
#ifndef LUMEX_APPLIED_JSON_NORMALIZATION_JSON_SCHEMA_NORMALIZER_HPP
#define LUMEX_APPLIED_JSON_NORMALIZATION_JSON_SCHEMA_NORMALIZER_HPP

#include <exception>
#include <string>

#include <nlohmann/json.hpp>

#include "lumex/applied/json/normalization/ILumexJsonNormalizer.hpp"
#include "lumex/applied/json/schema/LumexJsonSchemaTraverser.hpp"

#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace normalization
{
/**
 * @class LumexJsonSchemaNormalizer
 * @brief Shares the schema walker and the strict/non-strict error policy
 *        between concrete normalizers.
 *
 * @details Deliberately abstract: which schema applies and which legacy
 * fields to rewrite depend on the concrete operation, so `normalize()` (from
 * `ILumexJsonNormalizer`) and `validate()` are left to subclasses. A
 * subclass typically parses with `parse()`, rewrites legacy forms, then calls
 * `normalize_against_schema()` or `validate_against_schema()` inside a
 * `try` block whose `catch` calls `report_or_rethrow(error)`. Checking
 * without normalization is `LumexJsonSchemaValidator`.
 */
class LumexJsonSchemaNormalizer : public ILumexJsonNormalizer
{
public:
  /**
   * @brief Text of the exception held by `error`.
   * @return `what()` of a `std::exception`, `"<unknown exception>"` for any
   * other type, and `""` for a null `error`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the message is the point of the call")
  static std::string
  get_exception_message (std::exception_ptr const &error) LUMEX_NOEXCEPT
  {
    if (!error)
      return std::string ();
    // std::exception_ptr exposes no message; rethrowing and catching is the
    // standard way to read it.
    try
      {
        std::rethrow_exception (error);
      }
    catch (std::exception const &exc)
      {
        return exc.what ();
      }
    catch (...)
      {
        return "<unknown exception>";
      }
  }

  /**
   * @brief Checks without building anything. Strict: throws on the first
   * violation. Non-strict: never throws for a violation (the usual
   * implementation forwards to the `error` overload and ignores it).
   * @throws lumex::applied::json::schema::LumexJsonSchemaException Strict
   * mode only.
   */
  virtual void
  validate (lumex::core::string_view::view::LumexStringView raw) const
      = 0;

  /**
   * @brief Same check with a non-throwing option; see
   * `ILumexJsonNormalizer::normalize` for the meaning of `error`.
   */
  virtual void validate (lumex::core::string_view::view::LumexStringView raw,
                         std::exception_ptr &error) const
      = 0;

  /** @brief Whether violations throw (`true`) or are reported (`false`). */
  LUMEX_ATTRIBUTE_NODISCARD ("the mode is the point of the call")
  bool
  is_strict () const LUMEX_NOEXCEPT
  {
    return _strict;
  }

protected:
  /**
   * @param strict `true`: throw on the first violation. `false`: report it
   * through `report_or_rethrow`.
   */
  explicit LumexJsonSchemaNormalizer (bool strict) LUMEX_NOEXCEPT
      : _strict (strict)
  {
  }

  /**
   * @brief Call only from inside a `catch` block.
   * @details Strict: rethrows the current exception. Non-strict: stores
   * `std::current_exception()` in `error` and returns normally, so the
   * caller can read the full message without an exception escaping
   * `normalize()` or `validate()`.
   */
  void
  report_or_rethrow (std::exception_ptr &error) const
  {
    if (_strict)
      throw;
    error = std::current_exception ();
  }

  /**
   * @brief Parses raw text.
   * @throws lumex::applied::json::schema::LumexJsonSchemaException With
   * `parse_error` when the text is not valid JSON.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the parsed document is the result")
  static nlohmann::json
  parse (lumex::core::string_view::view::LumexStringView raw)
  {
    return lumex::applied::json::schema::LumexJsonSchemaTraverser::parse (raw);
  }

  /**
   * @brief Single entry into the shared walker; `mode` decides whether the
   * normalized tree is built. Prefer it over the two wrappers below when
   * `normalize()` and `validate()` share one walk over a larger structure
   * (for example a loop over elements).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the normalized document is the result")
  static nlohmann::json
  process_against_schema (
      nlohmann::json const &schema, nlohmann::json const &input,
      lumex::applied::json::schema::LumexJsonSchemaCheckMode mode,
      std::string const &path = "$")
  {
    return lumex::applied::json::schema::LumexJsonSchemaTraverser::run (
        schema, input, mode, path);
  }

  /** @brief `validate_and_normalize`: returns the built tree. */
  LUMEX_ATTRIBUTE_NODISCARD ("the normalized document is the result")
  static nlohmann::json
  normalize_against_schema (nlohmann::json const &schema,
                            nlohmann::json const &input,
                            std::string const &path = "$")
  {
    return process_against_schema (
        schema, input,
        lumex::applied::json::schema::LumexJsonSchemaCheckMode::
            validate_and_normalize,
        path);
  }

  /** @brief `validate_only`: throws on the first violation, builds nothing. */
  static void
  validate_against_schema (nlohmann::json const &schema,
                           nlohmann::json const &input,
                           std::string const &path = "$")
  {
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (process_against_schema (
        schema, input,
        lumex::applied::json::schema::LumexJsonSchemaCheckMode::validate_only,
        path));
  }

private:
  bool _strict;
};
} // namespace normalization
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_NORMALIZATION_JSON_SCHEMA_NORMALIZER_HPP

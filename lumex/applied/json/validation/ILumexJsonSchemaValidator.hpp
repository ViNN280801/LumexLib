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
 * @file ILumexJsonSchemaValidator.hpp
 * @brief Contract for checking raw JSON text against a schema.
 */
#ifndef LUMEX_APPLIED_JSON_VALIDATION_I_JSON_SCHEMA_VALIDATOR_HPP
#define LUMEX_APPLIED_JSON_VALIDATION_I_JSON_SCHEMA_VALIDATOR_HPP

#include <exception>

#include "lumex/core/string_view/view/LumexStringView.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace validation
{
/**
 * @class ILumexJsonSchemaValidator
 * @brief Checks raw JSON text against the schema of the implementation.
 *        Builds nothing: a conforming document just returns.
 */
class ILumexJsonSchemaValidator
{
public:
  virtual ~ILumexJsonSchemaValidator () = default;

  /**
   * @brief Checks `raw`, including whether it parses. A strict
   * implementation throws on the first violation; a non-strict one never
   * throws for a violation and discards it.
   * @throws lumex::applied::json::schema::LumexJsonSchemaException Strict
   * implementations only.
   */
  virtual void
  validate (lumex::core::string_view::view::LumexStringView raw) const
      = 0;

  /**
   * @brief Same check with a non-throwing option.
   * @param raw The JSON text.
   * @param error Reset to `nullptr` first. A non-strict implementation
   * stores the violation here (`std::current_exception()`) and returns
   * normally. A strict implementation ignores it and throws.
   */
  virtual void validate (lumex::core::string_view::view::LumexStringView raw,
                         std::exception_ptr &error) const
      = 0;
};
} // namespace validation
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_VALIDATION_I_JSON_SCHEMA_VALIDATOR_HPP

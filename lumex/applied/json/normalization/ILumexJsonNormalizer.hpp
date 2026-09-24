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
 * @file ILumexJsonNormalizer.hpp
 * @brief Contract for turning raw JSON text into a canonical document.
 */
#ifndef LUMEX_APPLIED_JSON_NORMALIZATION_I_JSON_NORMALIZER_HPP
#define LUMEX_APPLIED_JSON_NORMALIZATION_I_JSON_NORMALIZER_HPP

#include <exception>

#include <nlohmann/json.hpp>

#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace normalization
{
/**
 * @class ILumexJsonNormalizer
 * @brief Parses raw JSON text, checks it and returns the canonical document.
 */
class ILumexJsonNormalizer
{
public:
  virtual ~ILumexJsonNormalizer () = default;

  /**
   * @brief Returns the canonical document. A strict implementation throws
   * on the first violation (including unparsable text); a non-strict one
   * returns a `null` document instead of throwing.
   * @throws lumex::applied::json::schema::LumexJsonSchemaException Strict
   * implementations only.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the normalized document is the result")
  virtual nlohmann::json
  normalize (lumex::core::string_view::view::LumexStringView raw) const
      = 0;

  /**
   * @brief Same as `normalize(raw)` with a non-throwing option.
   * @param raw The JSON text.
   * @param error Reset to `nullptr` first. A non-strict implementation
   * stores the violation here and returns a default-constructed (`null`)
   * document that must not be used. A strict implementation ignores it and
   * throws.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the normalized document is the result")
  virtual nlohmann::json
  normalize (lumex::core::string_view::view::LumexStringView raw,
             std::exception_ptr &error) const
      = 0;
};
} // namespace normalization
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_NORMALIZATION_I_JSON_NORMALIZER_HPP

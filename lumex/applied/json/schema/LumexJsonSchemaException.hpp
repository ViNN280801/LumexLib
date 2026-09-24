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
 * @file LumexJsonSchemaException.hpp
 * @brief Exception thrown by the JSON schema engine on the first violation.
 */
#ifndef LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_EXCEPTION_HPP
#define LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_EXCEPTION_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace schema
{
/**
 * @brief Why a document failed schema validation.
 * @details `parse_error`: the text is not JSON. `missing_required`: a
 * required object or array is absent, or the value is not an object or an
 * array where the schema requires one. `type_mismatch`: a leaf value does
 * not match `type`. `const_mismatch`: a value differs from `const`.
 * `enum_mismatch`: a leaf value is not listed in `enum`.
 */
LUMEX_DEFINE_REFLECTED_ENUM (LumexJsonSchemaFailure, std::uint8_t,
                             (parse_error), (missing_required),
                             (type_mismatch), (const_mismatch),
                             (enum_mismatch))

/**
 * @class LumexJsonSchemaException
 * @brief Carries the failure reason, the JSON path of the offending value
 * (`$.items[2].name`) and a human-readable detail.
 * @details `what()` is `[LumexJsonSchemaException] <reason> at '<path>':
 * <detail>`.
 */
class LumexJsonSchemaException final : public std::runtime_error
{
public:
  LumexJsonSchemaException (LumexJsonSchemaFailure reason, std::string path,
                            std::string detail)
      : std::runtime_error (_format_message (reason, path, detail)),
        _reason (reason), _path (std::move (path)),
        _detail (std::move (detail))
  {
  }

  LUMEX_ATTRIBUTE_NODISCARD ("the failure reason is the point of the call")
  LumexJsonSchemaFailure
  reason () const LUMEX_NOEXCEPT
  {
    return _reason;
  }

  LUMEX_ATTRIBUTE_NODISCARD ("the path is the point of the call")
  std::string const &
  path () const LUMEX_NOEXCEPT
  {
    return _path;
  }

  LUMEX_ATTRIBUTE_NODISCARD ("the detail is the point of the call")
  std::string const &
  detail () const LUMEX_NOEXCEPT
  {
    return _detail;
  }

private:
  static std::string
  _format_message (LumexJsonSchemaFailure reason, std::string const &path,
                   std::string const &detail)
  {
    std::string message ("[LumexJsonSchemaException] ");
    message += toString (reason);
    message += " at '";
    message += path;
    message += "': ";
    message += detail;
    return message;
  }

  LumexJsonSchemaFailure _reason;
  std::string _path;
  std::string _detail;
};
} // namespace schema
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_SCHEMA_JSON_SCHEMA_EXCEPTION_HPP

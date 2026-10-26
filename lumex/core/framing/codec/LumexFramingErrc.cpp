/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string>
#include <system_error>

#include "lumex/core/framing/codec/LumexFramingErrc.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace codec
{
namespace
{
class framing_category_impl final : public std::error_category
{
public:
  char const *
  name () const LUMEX_NOEXCEPT override
  {
    return "lumex.framing";
  }

  std::string
  message (int value) const override
  {
    // The switch is on the int: a value outside the enum is possible here.
    switch (value)
      {
      case static_cast<int> (framing_errc::frame_too_large):
        return "frame larger than the codec limit";
      case static_cast<int> (framing_errc::bad_checksum):
        return "frame check value mismatch";
      case static_cast<int> (framing_errc::bad_escape):
        return "invalid escape sequence or encoding";
      case static_cast<int> (framing_errc::bad_header):
        return "invalid frame header";
      case static_cast<int> (framing_errc::truncated):
        return "frame shorter than its length";
      case static_cast<int> (framing_errc::payload_too_large):
        return "payload larger than the codec can carry";
      default:
        return "unknown framing error";
      }
  }
};
} // namespace

std::error_category const &
framing_category () LUMEX_NOEXCEPT
{
  // Never destroyed: an error code of this category may still be compared or
  // printed by a static destructor of another module at exit.
  static framing_category_impl const *const instance
      = new framing_category_impl ();
  return *instance;
}
} // namespace codec
} // namespace framing
} // namespace core
} // namespace lumex

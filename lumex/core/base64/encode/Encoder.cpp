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

#define LUMEX_IMPLEMENTATION
#include <cstddef>
#include <string>
#include <vector>

#include "Encoder.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::codec::Constants;
using namespace lumex::core::base64::codec::detail;

namespace
{
/// @brief Read-only view of the caller's bytes for `detail::_encode_impl`.
/// The same in every standard, so the exported function does not depend on
/// the standard the library is built with.
struct byte_range_t
{
  byte_type const *first;
  std::size_t count;

  std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return count;
  }

  bool
  empty () const LUMEX_NOEXCEPT
  {
    return count == 0;
  }

  byte_type
  operator[] (std::size_t index) const LUMEX_NOEXCEPT
  {
    return first[index];
  }
};
} // namespace

LUMEX_PUBLIC_API
std::string
encoder::encode (void const *data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return {};

  byte_range_t const bytes = { static_cast<byte_type const *> (data), size };
  return detail::_encode_impl (bytes);
}

LUMEX_PUBLIC_API
std::string
encoder::encode (std::vector<byte_type> const &data)
{
  return detail::_encode_impl (data);
}

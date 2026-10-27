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

/**
 * @file LumexProtocolContext.hpp
 * @brief `decode_context_t`: the session values a decoder may read
 * (`union_by_context_`), and `LUMEX_PROTOCOL_CONTEXT_KEY` for their keys.
 * @details A layout is chosen by a value of the session for some devices
 * (the type of the device, learned from an earlier reply). The decoder sees
 * the session through this view: a lookup of a named integer. The session
 * context of the index fills it; a test fills it with a function.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_CONTEXT_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_CONTEXT_HPP

#include <cstdint>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/**
 * @brief A read-only view of the session values: `lookup (state, key,
 * value)` sets `value` and returns true when the session knows `key`.
 */
struct decode_context_t
{
  /// The session object, passed back to `lookup`.
  void const *state;
  /// Finds the value of `key`.
  bool (*lookup) (void const *state, char const *key, std::int64_t &value);

  /// @brief The value of `key`; false when the session does not know it.
  bool
  find (char const *key, std::int64_t &value) const
  {
    return lookup != nullptr && lookup (state, key, value);
  }
};

/**
 * @brief Defines the key `name_` of a session value for
 * `union_by_context_<name_, ...>`: `LUMEX_PROTOCOL_CONTEXT_KEY
 * (device_type);` looks up `"device_type"`.
 */
#define LUMEX_PROTOCOL_CONTEXT_KEY(name_)                                     \
  struct name_                                                                \
  {                                                                           \
    static LUMEX_CONSTEXPR char const *                                       \
    context_key () LUMEX_NOEXCEPT                                             \
    {                                                                         \
      return #name_;                                                          \
    }                                                                         \
  }
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_CONTEXT_HPP

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
 * @file LumexProtocolRegistryText.hpp
 * @brief The text of the registry types: `key_to_string`, `access_to_string`,
 * `key_kind_name`.
 * @details The spelling is the one the describe line, the verification
 * report and the exports use, so a key reads the same everywhere:
 * `index 5`, `typed 3/5`, `unit_param 2/9`, `mnemonic "AZ"`,
 * `modbus 0x0100`.
 */
#ifndef LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_TEXT_HPP
#define LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_TEXT_HPP

#include <cstddef>
#include <cstdio>
#include <string>

#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
// snprintf is the standard function; the secure variants are not portable.
#pragma warning(disable : 4996)
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace registry
{
/// @brief The name of a key kind (`"index"`, `"typed"`, `"unit_param"`,
/// `"mnemonic"`, `"modbus"`).
inline char const *
key_kind_name (registry_key_kind kind) LUMEX_NOEXCEPT
{
  static char const *const names[]
      = { "index", "typed", "unit_param", "mnemonic", "modbus" };
  unsigned const at = static_cast<unsigned> (kind);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "unknown";
}

/// @brief The access as text: `"R"`, `"W"` or `"RW"`.
inline char const *
access_to_string (access_t access) LUMEX_NOEXCEPT
{
  static char const *const names[] = { "?", "R", "W", "RW" };
  unsigned const at = static_cast<unsigned> (access);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "?";
}

/// @brief The key as text (see the file description).
inline std::string
key_to_string (registry_key_t const &key)
{
  char buffer[48];
  if (key.kind == registry_key_kind::index)
    std::snprintf (buffer, sizeof (buffer), "index %u",
                   static_cast<unsigned> (key.first));
  else if (key.kind == registry_key_kind::typed
           || key.kind == registry_key_kind::unit_param)
    std::snprintf (buffer, sizeof (buffer), "%s %u/%u",
                   key_kind_name (key.kind), static_cast<unsigned> (key.first),
                   static_cast<unsigned> (key.second));
  else if (key.kind == registry_key_kind::modbus)
    std::snprintf (buffer, sizeof (buffer), "modbus 0x%04X",
                   static_cast<unsigned> (key.first));
  else
    {
      std::size_t length = 0;
      while (length < 3 && key.mnemonic[length] != '\0')
        ++length;
      return "mnemonic \"" + std::string (key.mnemonic, length) + "\"";
    }
  return buffer;
}
} // namespace registry
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_TEXT_HPP

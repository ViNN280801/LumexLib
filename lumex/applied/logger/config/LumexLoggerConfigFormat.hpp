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

#ifndef LUMEX_APPLIED_LOGGER_CONFIG_LOGGER_CONFIG_FORMAT_HPP
#define LUMEX_APPLIED_LOGGER_CONFIG_LOGGER_CONFIG_FORMAT_HPP

#include <cstdint>

#include "lumex/LumexExport.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace logger
{
/**
 * @brief Compile-time format of the logger enable/config file.
 * @details Exactly one format is selected by LUMEX_LOGGER_CONFIG_FORMAT when
 *          lumex::logger is configured. The path extension does not affect
 *          parsing.
 */
enum class logger_config_format_t : std::uint8_t
{
  plain_text = 0, ///< Prefixed KEY=value lines (historical enable_logs).
  ini,            ///< INI via LumexSettingsINI ([logger] section).
  json,           ///< JSON via vendored nlohmann/json.
  yaml,           ///< Reserved; throws until LumexSettingsYAML exists.
  xml             ///< XML via LumexXml (<logger> child elements).
};

#if (defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)                           \
     + defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)                                \
     + defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)                               \
     + defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)                               \
     + defined(LUMEX_LOGGER_CONFIG_FORMAT_XML))                               \
    != 1
#error "Exactly one LUMEX_LOGGER_CONFIG_FORMAT_* definition is required"
#endif

/**
 * @brief Returns the only logger config format compiled into this target.
 * @return Compile-time logger config format.
 * @note The function is constexpr and does not inspect a file name.
 */
inline LUMEX_CONSTEXPR_FUNCTION logger_config_format_t
configured_logger_config_format () LUMEX_NOEXCEPT
{
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
  return logger_config_format_t::plain_text;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
  return logger_config_format_t::ini;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
  return logger_config_format_t::json;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
  return logger_config_format_t::yaml;
#else
  return logger_config_format_t::xml;
#endif
}

} // namespace logger
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_LOGGER_CONFIG_LOGGER_CONFIG_FORMAT_HPP

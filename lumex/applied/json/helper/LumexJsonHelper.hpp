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
 * @file LumexJsonHelper.hpp
 * @brief Non-throwing helpers for JSON configuration files and documents.
 */
#ifndef LUMEX_APPLIED_JSON_HELPER_HPP
#define LUMEX_APPLIED_JSON_HELPER_HPP

#include <cstddef>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <string>
#include <type_traits>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <nlohmann/json.hpp>

#include "lumex/applied/json/diagnostics/LumexJsonDiagnostics.hpp"

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace helper
{
namespace Detail
{
/**
 * @brief Whether a value is an empty string. `set_value` refuses to store
 * one. Non-string types are never empty.
 */
inline bool
is_empty_value (std::string const &value) LUMEX_NOEXCEPT
{
  return value.empty ();
}

inline bool
is_empty_value (char const *value) LUMEX_NOEXCEPT
{
  return value == nullptr || value[0] == '\0';
}

inline bool
is_empty_value (char *value) LUMEX_NOEXCEPT
{
  return value == nullptr || value[0] == '\0';
}

#if __cplusplus >= 201703L
inline bool
is_empty_value (std::string_view value) LUMEX_NOEXCEPT
{
  return value.empty ();
}
#endif

template <typename T>
inline bool
is_empty_value (T const &) LUMEX_NOEXCEPT
{
  return false;
}
} // namespace Detail

/**
 * @class LumexJsonHelper
 * @brief One API for flat and sectioned JSON configurations.
 *
 * @details Keys live either at the root (empty `section`) or inside a named
 * section. A section name may be a dot-separated path (`"a.b"`) into nested
 * objects: reads treat a missing or non-object step as "not found", writes
 * create it (overwriting a non-object value on the way).
 *
 * No member throws. Failures return `false`, an empty document or the
 * default value, and are described through
 * `lumex::applied::json::diagnostics::set_diagnostic_reporter`.
 *
 * File operations (`load_config`, `save_config`, `is_json_file_ok`,
 * `write_value`, `edit_value`) hold one process-wide recursive mutex, so a
 * read-modify-write in `write_value` or `edit_value` is atomic with respect
 * to other helper calls. Callers that do their own read-modify-write can
 * lock the same mutex through `get_file_mutex()`.
 *
 * Header-only: the mutex is a function-local static, so it is shared within
 * one binary (executable or shared library), not across binaries.
 */
class LumexJsonHelper final
{
public:
  using level_type
      = lumex::applied::json::diagnostics::LumexJsonDiagnosticLevel;

  LumexJsonHelper () = delete;

  /**
   * @brief Loads a JSON document from a file.
   * @return The document, or an empty (`null`) document when the name is
   * empty, the file cannot be opened or its content is not valid JSON.
   */
  static nlohmann::json
  load_config (std::string const &filename) LUMEX_NOEXCEPT
  {
    std::lock_guard<std::recursive_mutex> const lock (get_file_mutex ());
    return _load_config_impl (filename);
  }

  /**
   * @brief Writes `config` to a file, indented by four spaces.
   * @return `true` on success; `false` for an empty name or a write error.
   */
  static bool
  save_config (nlohmann::json const &config,
               std::string const &filename) LUMEX_NOEXCEPT
  {
    std::lock_guard<std::recursive_mutex> const lock (get_file_mutex ());
    return _save_config_impl (config, filename);
  }

  /**
   * @brief The mutex that serializes the file operations of this helper.
   * @details Recursive, so a caller holding it can still call the file
   * operations.
   */
  static std::recursive_mutex &
  get_file_mutex () LUMEX_NOEXCEPT
  {
    static std::recursive_mutex file_mutex;
    return file_mutex;
  }

  /**
   * @brief Whether the file exists and parses as JSON.
   * @details On a parse error the diagnostic names the line and column of
   * the offending byte.
   */
  static bool
  is_json_file_ok (std::string const &filename) LUMEX_NOEXCEPT
  {
    std::lock_guard<std::recursive_mutex> const lock (get_file_mutex ());
    return _is_json_file_ok_impl (filename);
  }

  /**
   * @brief Whether `key` exists in `section` (or at the root) and is not
   * `null`.
   */
  template <typename KeyType>
  static bool
  has_key (nlohmann::json const &config, KeyType const &key,
           std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    try
      {
        nlohmann::json const &target = _get_target_section (config, section);
        nlohmann::json::const_iterator const it = target.find (key);
        return it != target.cend () && !it->is_null ();
      }
    catch (std::exception const &exc)
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME,
                 "Exception checking key '", key, "': ", exc.what (),
                 ", returning false.");
        return false;
      }
  }

  /**
   * @brief Whether a top-level key named `section` exists and is not `null`.
   * @details An empty `section` names the root and is always present. The
   * name is not split on dots.
   */
  static bool
  has_section (nlohmann::json const &config,
               std::string const &section) LUMEX_NOEXCEPT
  {
    if (section.empty ())
      return true;
    if (config.empty () || !config.is_object ())
      return false;
    try
      {
        nlohmann::json::const_iterator const it = config.find (section);
        return it != config.cend () && !it->is_null ();
      }
    catch (std::exception const &exc)
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME,
                 "Exception checking section '", section, "': ", exc.what (),
                 ", returning false.");
        return false;
      }
  }

  /**
   * @brief Creates a top-level object named `section` when it is missing.
   * @details An empty `section` is a no-op success. The name is not split on
   * dots.
   * @return `true` when the section exists as an object afterwards; `false`
   * when `config` is not an object or the key holds a non-object value.
   */
  static bool
  create_section (nlohmann::json &config,
                  std::string const &section) LUMEX_NOEXCEPT
  {
    if (section.empty ())
      return true;
    if (!config.is_object ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Configuration is not a JSON object, cannot create section '",
                 section, "', returning false.");
        return false;
      }
    try
      {
        nlohmann::json::iterator const it = config.find (section);
        if (it == config.end ())
          {
            config[section] = nlohmann::json::object ();
            _report (level_type::info, LUMEX_FUNCTION_NAME, "Section '",
                     section, "' created successfully.");
          }
        else if (!it->is_object ())
          {
            _report (level_type::warning, LUMEX_FUNCTION_NAME, "Key '",
                     section,
                     "' exists but is not a JSON object, cannot create "
                     "section, returning false.");
            return false;
          }
        return true;
      }
    catch (std::exception const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Exception creating section '", section, "': ", exc.what (),
                 ", returning false.");
        return false;
      }
  }

  /**
   * @brief Reads `key` as `T` from `section` (or the root).
   * @return The stored value, or `default_value` when the key is missing,
   * `null` or holds a value that does not convert to `T`.
   */
  template <typename T, typename KeyType>
  static T
  get_value (nlohmann::json const &config, KeyType const &key,
             T const &default_value,
             std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    try
      {
        nlohmann::json const &target = _get_target_section (config, section);
        nlohmann::json::const_iterator const it = target.find (key);
        if (it != target.cend () && !it->is_null ())
          return it->template get<T> ();
      }
    catch (std::exception const &exc)
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME,
                 "Exception getting value for key '", key, "': ", exc.what (),
                 ", returning default.");
      }
    return default_value;
  }

  /**
   * @brief `get_value` for a string literal default; returns `std::string`.
   */
  static std::string
  get_value (nlohmann::json const &config, char const *key,
             char const *default_value,
             std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    return _get_string (config, key, default_value, section);
  }

  /** @copydoc get_value(nlohmann::json const&, char const*, char const*,
   * std::string const&) */
  static std::string
  get_value (nlohmann::json const &config, std::string const &key,
             char const *default_value,
             std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    return _get_string (config, key, default_value, section);
  }

  /**
   * @brief Stores `value` under `key` in `section` (or the root), creating
   * the section path when needed.
   * @return `false` for an empty string value (nothing is stored) or when
   * the assignment throws.
   */
  template <typename T, typename KeyType>
  static bool
  set_value (nlohmann::json &config, KeyType const &key, T const &value,
             std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    try
      {
        if (Detail::is_empty_value (value))
          {
            _report (level_type::warning, LUMEX_FUNCTION_NAME,
                     "Attempting to set empty value for key '", key,
                     "', returning false.");
            return false;
          }
        nlohmann::json &target = _get_target_section (config, section);
        target[key] = value;
        return true;
      }
    catch (std::exception const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Exception setting value for key '", key, "': ", exc.what (),
                 ", returning false.");
        return false;
      }
  }

  /**
   * @brief Loads the file (an unreadable or missing file counts as an empty
   * object), stores `value` and saves it back, under the file mutex.
   */
  template <typename T, typename KeyType>
  static bool
  write_value (std::string const &filename, KeyType const &key, T const &value,
               std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    std::lock_guard<std::recursive_mutex> const lock (get_file_mutex ());
    if (filename.empty ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Config filename is empty, returning false.");
        return false;
      }
    nlohmann::json config = _load_config_impl (filename);
    if (config.is_null ())
      config = nlohmann::json::object ();
    if (!set_value (config, key, value, section))
      return false;
    return _save_config_impl (config, filename);
  }

  /**
   * @brief Like `write_value`, but only replaces a key that already exists
   * (not `null`) in a non-empty file.
   */
  template <typename T, typename KeyType>
  static bool
  edit_value (std::string const &filename, KeyType const &key, T const &value,
              std::string const &section = std::string ()) LUMEX_NOEXCEPT
  {
    std::lock_guard<std::recursive_mutex> const lock (get_file_mutex ());
    if (filename.empty ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Config filename is empty, returning false.");
        return false;
      }
    nlohmann::json config = _load_config_impl (filename);
    if (config.is_null () || config.empty ())
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME,
                 "Config file is empty or invalid, returning false.");
        return false;
      }
    if (!has_key (config, key, section))
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME, "Key '", key,
                 "' not found in config, nothing to edit, returning false.");
        return false;
      }
    if (!set_value (config, key, value, section))
      return false;
    return _save_config_impl (config, filename);
  }

private:
  template <typename... Parts>
  static void
  _report (level_type level, char const *function,
           Parts const &...parts) LUMEX_NOEXCEPT
  {
    lumex::applied::json::diagnostics::Detail::report (level, '[', function,
                                                       "] ", parts...);
  }

  static nlohmann::json
  _load_config_impl (std::string const &filename) LUMEX_NOEXCEPT
  {
    if (filename.empty ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Config filename is empty, returning empty JSON object.");
        return nlohmann::json ();
      }
    try
      {
        std::ifstream json_file (filename.c_str ());
        if (!json_file.is_open ())
          {
            _report (level_type::error, LUMEX_FUNCTION_NAME,
                     "Failed to open config file: ", filename, ".");
            return nlohmann::json ();
          }
        nlohmann::json config;
        json_file >> config;
        return config;
      }
    catch (nlohmann::json::parse_error const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Failed to parse config file ", filename, ": ", exc.what (),
                 ".");
      }
    catch (std::exception const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Unexpected error reading config file ", filename, ": ",
                 exc.what (), ".");
      }
    return nlohmann::json ();
  }

  static bool
  _save_config_impl (nlohmann::json const &config,
                     std::string const &filename) LUMEX_NOEXCEPT
  {
    if (filename.empty ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Config filename is empty, returning false.");
        return false;
      }
    try
      {
        std::ofstream json_file (filename.c_str ());
        if (!json_file.is_open ())
          {
            _report (level_type::error, LUMEX_FUNCTION_NAME,
                     "Failed to open config file for writing: ", filename,
                     ", returning false.");
            return false;
          }
        json_file << std::setw (4) << config << '\n';
        json_file.close ();
        if (json_file.fail ())
          {
            _report (level_type::error, LUMEX_FUNCTION_NAME,
                     "Error writing config to file ", filename,
                     ", returning false.");
            return false;
          }
        return true;
      }
    catch (std::exception const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Error writing config to file ", filename, ": ", exc.what (),
                 ", returning false.");
        return false;
      }
  }

  static bool
  _is_json_file_ok_impl (std::string const &filename) LUMEX_NOEXCEPT
  {
    if (filename.empty ())
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Config filename is empty, returning false.");
        return false;
      }
    try
      {
        std::ifstream json_file (filename.c_str ());
        if (!json_file.is_open ())
          {
            _report (level_type::error, LUMEX_FUNCTION_NAME,
                     "Failed to open config file: ", filename, ".");
            return false;
          }
        try
          {
            nlohmann::json config;
            json_file >> config;
            _report (level_type::debug, LUMEX_FUNCTION_NAME, "Config file ",
                     filename, " parsed successfully.");
            return true;
          }
        catch (nlohmann::json::parse_error const &exc)
          {
            json_file.clear ();
            json_file.seekg (0);
            std::string const content (
                (std::istreambuf_iterator<char> (json_file)),
                std::istreambuf_iterator<char> ());
            std::size_t line = 1;
            std::size_t column = 1;
            for (std::size_t i = 0; i < exc.byte && i < content.size (); ++i)
              {
                if (content[i] == '\n')
                  {
                    ++line;
                    column = 1;
                  }
                else
                  ++column;
              }
            _report (level_type::error, LUMEX_FUNCTION_NAME,
                     "Failed to parse config file ", filename, " at line ",
                     line, ", column ", column, " (byte ", exc.byte,
                     "): ", exc.what (), ".");
            return false;
          }
      }
    catch (std::exception const &exc)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Error reading config file ", filename, ": ", exc.what (),
                 ".");
        return false;
      }
    catch (...)
      {
        _report (level_type::error, LUMEX_FUNCTION_NAME,
                 "Unknown error reading config file ", filename, ".");
        return false;
      }
  }

  template <typename KeyType>
  static std::string
  _get_string (nlohmann::json const &config, KeyType const &key,
               char const *default_value,
               std::string const &section) LUMEX_NOEXCEPT
  {
    try
      {
        nlohmann::json const &target = _get_target_section (config, section);
        nlohmann::json::const_iterator const it = target.find (key);
        if (it != target.cend () && !it->is_null ())
          return it->template get<std::string> ();
      }
    catch (std::exception const &exc)
      {
        _report (level_type::warning, LUMEX_FUNCTION_NAME,
                 "Exception getting string value for key '", key,
                 "': ", exc.what (), ", returning default.");
      }
    return default_value != nullptr ? std::string (default_value)
                                    : std::string ();
  }

  /** @brief Splits `section` on dots; empty components are skipped. */
  template <typename Visitor>
  static bool
  _walk_section_path (std::string const &section, Visitor visit)
  {
    std::size_t position = 0;
    while (position < section.size ())
      {
        std::size_t const dot = section.find ('.', position);
        std::size_t const end
            = dot == std::string::npos ? section.size () : dot;
        std::string const component
            = section.substr (position, end - position);
        position = end + 1;
        if (component.empty ())
          continue;
        if (!visit (component))
          return false;
      }
    return true;
  }

  /**
   * @brief Writable target: the root (turned into an object if needed) or
   * the section path, creating or overwriting every step as an object.
   */
  static nlohmann::json &
  _get_target_section (nlohmann::json &config, std::string const &section)
  {
    if (!config.is_object ())
      config = nlohmann::json::object ();
    nlohmann::json *current = &config;
    _walk_section_path (
        section, [&current] (std::string const &component) -> bool {
          nlohmann::json::iterator const it = current->find (component);
          if (it == current->end () || !it->is_object ())
            {
              (*current)[component] = nlohmann::json::object ();
              _report (level_type::warning,
                       "LumexJsonHelper::_get_target_section",
                       "Creating or overwriting key '", component,
                       "' to an object in section traversal.");
            }
          current = &(*current)[component];
          return true;
        });
    return *current;
  }

  /**
   * @brief Read-only target: the root or the section path, or a shared empty
   * object when the root is not an object or any step is missing or not an
   * object.
   */
  static nlohmann::json const &
  _get_target_section (nlohmann::json const &config,
                       std::string const &section)
  {
    static nlohmann::json const empty_object = nlohmann::json::object ();
    if (!config.is_object ())
      return empty_object;
    nlohmann::json const *current = &config;
    bool const found = _walk_section_path (
        section, [&current] (std::string const &component) -> bool {
          nlohmann::json::const_iterator const it = current->find (component);
          if (it == current->cend () || !it->is_object ())
            return false;
          current = &(*it);
          return true;
        });
    return found ? *current : empty_object;
  }
};
} // namespace helper
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_HELPER_HPP

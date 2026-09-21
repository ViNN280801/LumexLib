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

#ifndef LUMEX_APPLIED_SETTINGS_JSON_SETTINGS_JSON_HPP
#define LUMEX_APPLIED_SETTINGS_JSON_SETTINGS_JSON_HPP

#include "lumex/LumexExport.hpp"

#include <unordered_map>

#include "lumex/applied/settings/interface/ILumexSettings.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace settings
{
namespace json
{
namespace Constants
{
LUMEX_CONST_STR JSON_FILE_EXTENSION = ".json"; ///< JSON file extension.
LUMEX_CONST_STR SETTINGS_ROOT_NAME
    = "settings"; ///< Implicit section for a flat root object.
}

/**
 * @class LumexSettingsJSON
 * @brief A lightweight, C++11-based JSON settings manager.
 *
 * Implements `ILumexSettings` with the same section/key/value contract as
 * `LumexSettingsINI` and `LumexSettingsXML`. Parsing and writing go through
 * vendored nlohmann/json only.
 *
 * Load mapping:
 * - The document must be a JSON object.
 * - If any value is an object, those keys are sections and their scalar
 *   children are keys. Scalar children of the root itself are stored under
 *   the implicit section `settings`.
 * - Otherwise the root is a single section named `settings` (flat
 *   `{ "LEVEL": "DEBUG" }`). Nested `{ "logger": { "LEVEL": "DEBUG" } }`
 *   loads as section `logger`.
 *
 * Save always writes an object of objects:
 * `{ "section": { "key": "value" } }`.
 *
 * When `LUMEX_SETTINGS_WITH_JSON` is not defined (nlohmann header not
 * present), `load` / `save` / `is_json_valid` return false. The factory
 * then returns nullptr for `LumexSettingsExtensions::JSON`.
 *
 * @note This class is not designed to be thread-safe for concurrent writes.
 *       External synchronization is required if instances are shared across
 * threads.
 */
class LUMEX_API LumexSettingsJSON : public ILumexSettings
{
public:
  /**
   * @brief Loads and parses a JSON file from the given path.
   * @details Validates the file with `is_json_valid()` first. On success the
   *          in-memory store is replaced with the parsed sections and keys.
   * @param[in] path The filesystem path to the JSON file.
   * @return `true` if the file is valid and at least one key was loaded,
   *         `false` otherwise.
   */
  bool load (std::string const &path) override;
  bool load (char const *path);

  /**
   * @brief Saves the current settings as an object of objects.
   * @details If the parent directory does not exist, it will be created.
   * @param[in] path The filesystem path to save the JSON file to.
   * @return `true` if the file is written successfully, `false` otherwise.
   */
  bool save (std::string const &path) const override;
  bool save (char const *path) const;

  /**
   * @brief Retrieves a string value for a given section and key.
   * @return The value, or an empty string if the section or key is missing.
   */
  std::string get (std::string const &section,
                   std::string const &key) const override;
  std::string get (char const *section, char const *key) const;

  /**
   * @brief Adds or updates a key-value pair in a specific section.
   * @details Creates the section if it does not exist. Empty section, key,
   *          or value is ignored (same as `LumexSettingsINI`).
   */
  void add (std::string const &section, std::string const &key,
            std::string const &value) override;
  void add (char const *section, char const *key, char const *value);

  /**
   * @brief Removes a key-value pair from a section.
   * @details Missing section or key is a no-op. Removing the last key does
   *          not remove the section itself.
   */
  void remove (std::string const &section, std::string const &key) override;
  void remove (char const *section, char const *key);

  /**
   * @brief Checks that a path is a readable JSON object document.
   */
  static bool is_json_valid (std::string const &path);
  static bool is_json_valid (char const *path);

private:
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      _settings;
#ifdef _WIN32
#pragma warning(pop)
#endif

  bool _load_with_parser (std::string const &path);
  bool _save_with_parser (std::string const &path) const;
};
} // namespace json
} // namespace settings
} // namespace applied
} // namespace lumex

using LumexSettingsJSON = lumex::applied::settings::json::LumexSettingsJSON;

#endif // !LUMEX_APPLIED_SETTINGS_JSON_SETTINGS_JSON_HPP

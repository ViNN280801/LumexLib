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

#ifndef LUMEX_APPLIED_SETTINGS_INI_SETTINGS_INI_HPP
#define LUMEX_APPLIED_SETTINGS_INI_SETTINGS_INI_HPP

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
namespace ini
{
namespace Constants
{
LUMEX_CONST_STR INI_FILE_EXTENSION = ".ini"; ///< INI file extension.
LUMEX_CONST_STR REGEX_SECTION
    = R"(^\s*\[([^\]]+)\]\s*$)"; ///< Regex for section headers.

// FIX(Test:
// LumexSettingsINITest.GivenSpecialCharactersInValues_WhenSave_ThenQuotesIfNecessary):
// Fixed regex to properly handle quoted and unquoted values
// The pattern now correctly matches quoted strings (any content except quotes)
// or unquoted strings (no special chars)
LUMEX_CONST_STR REGEX_KEY_VALUE
    = R"(^\s*([^=\s]+)\s*=\s*(?:\"((?:[^\"\\]|\\.)*)\"|([^,"#;]*))\s*(?:[#;].*)?$)"; ///< Regex for key-value
                                                                                     ///< pairs.

// UTF-8 Byte Order Mark (BOM) constants
LUMEX_CONST_NUM int UTF8_BOM_SIZE = 3; ///< Size of the UTF-8 BOM in bytes.
LUMEX_CONST_NUM unsigned char UTF8_BOM_0 = 0xEF; ///< First byte of UTF-8 BOM.
LUMEX_CONST_NUM unsigned char UTF8_BOM_1 = 0xBB; ///< Second byte of UTF-8 BOM.
LUMEX_CONST_NUM unsigned char UTF8_BOM_2 = 0xBF; ///< Third byte of UTF-8 BOM.
}

/**
 * @class LumexSettingsINI
 * @brief A lightweight, C++11-based INI settings manager.
 *
 * This class provides a simple, yet robust, interface for handling INI
 * configuration files. It implements the ILumexSettings interface and uses
 * a standard std::unordered_map for in-memory storage, ensuring efficient
 * key-value lookups.
 *
 * All file I/O and parsing logic is self-contained and implemented using C++11
 * features, with no external dependencies beyond the Lumex framework itself.
 * It includes validation checks before loading and after saving to ensure
 * data integrity.
 *
 * @note This class is not designed to be thread-safe for concurrent writes.
 *       External synchronization is required if instances are shared across
 * threads.
 */
class LUMEX_API LumexSettingsINI : public ILumexSettings
{
public:
  /**
   * @brief Loads and parses an INI file from the given path.
   * @details Before loading, this method validates the file's syntax and
   *          readability using `is_ini_valid()`. If validation passes,
   *          it clears any current settings and populates the internal map
   *          with the data from the file.
   * @param[in] path The filesystem path to the INI file.
   * @return `true` if the file is successfully validated and loaded,
   *         `false` otherwise.
   */
  bool load (std::string const &path) override;
  bool load (char const *path);

  /**
   * @brief Saves the current settings to an INI file.
   * @details Writes all sections and key-value pairs to the specified path.
   *          If the parent directory does not exist, it will be created.
   *          After writing the file, it is re-validated to ensure its
   *          integrity.
   * @param[in] path The filesystem path to save the INI file to.
   * @return `true` if the file is saved and validated successfully,
   *         `false` otherwise.
   */
  bool save (std::string const &path) const override;
  bool save (char const *path) const;

  /**
   * @brief Retrieves a string value for a given section and key.
   * @param[in] section The name of the INI section.
   * @param[in] key The name of the key within the section.
   * @return The corresponding value as a std::string. If the section or key
   *         is not found, returns an empty string.
   */
  std::string get (std::string const &section,
                   std::string const &key) const override;
  std::string get (char const *section, char const *key) const;

  /**
   * @brief Adds or updates a key-value pair in a specific section.
   * @details If the section does not exist, it is created. If the key already
   *          exists within the section, its value is overwritten with the
   *          new value.
   * @param[in] section The name of the section.
   * @param[in] key The name of the key.
   * @param[in] value The string value to associate with the key.
   */
  void add (std::string const &section, std::string const &key,
            std::string const &value) override;
  void add (char const *section, char const *key, char const *value);

  /**
   * @brief Removes a key-value pair from a section.
   * @details If the section or key does not exist, the operation has no
   *          effect. Removing the last key from a section does not remove
   *          the section itself.
   * @param[in] section The name of the section.
   * @param[in] key The name of the key to remove.
   */
  void remove (std::string const &section, std::string const &key) override;
  void remove (char const *section, char const *key);

  /**
   * @brief Performs a static check on an INI file for basic validity.
   * @details Checks if the file is readable and uses regular expressions to
   *          verify that it follows a basic INI structure, containing only
   *          `[section]` headers, `key=value` pairs, comments, and empty
   * lines.
   * @param[in] path The path to the INI file.
   * @return `true` if the file is readable and syntactically valid,
   *         `false` otherwise.
   */
  static bool is_ini_valid (std::string const &path);
  static bool is_ini_valid (char const *path);

private:
  /**
   * @brief Hash table to save settings of INI file.
   * @details Key - section name
   *          Value - internal hash table of key-value pairs, where:
   *              Key - setting name
   *              Value - setting value
   */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      m_settings;
#ifdef _WIN32
#pragma warning(pop)
#endif

  /**
   * @brief Internal implementation for loading and parsing the INI file.
   * @details This method reads the file line by line, trims whitespace, and
   *          populates the internal `m_settings` map. It handles section
   *          headers and key-value pairs.
   * @param[in] path The path to the INI file to parse.
   * @return `true` if the file was opened and at least one setting was
   *         loaded, `false` otherwise.
   */
  bool _load_with_parser (std::string const &path);
  /**
   * @brief Internal implementation for saving the settings to a file.
   * @details This method iterates through the `m_settings` map and writes
   *          each section and its key-value pairs to the specified file.
   *          It ensures the parent directory exists before writing and
   *          validates the output file's integrity after saving.
   * @param[in] path The path where the INI file will be saved.
   * @return `true` if the file was successfully written and validated,
   *         `false` otherwise.
   */
  bool _save_with_parser (std::string const &path) const;
};
} // namespace ini
} // namespace settings
} // namespace applied
} // namespace lumex

using LumexSettingsINI = lumex::applied::settings::ini::LumexSettingsINI;

#endif // !LUMEX_APPLIED_SETTINGS_INI_SETTINGS_INI_HPP

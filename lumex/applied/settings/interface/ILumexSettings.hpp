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
 * @file ILumexSettings.hpp
 * @brief The interface every settings format implements: load, save, and get,
 * add or remove a string value by section and key.
 * @details `ILumexSettings` is what `LumexSettingsFactory` returns and what
 * `LumexSettingsGuard` works on, so code written against it does not depend on
 * the file format. It is declared in `lumex::applied::settings` rather than in
 * a namespace named after its directory, because the Windows SDK header
 * `combaseapi.h` defines `interface` as a macro. A global alias of the same
 * name is provided.
 */
#ifndef LUMEX_APPLIED_SETTINGS_INTERFACE_HPP
#define LUMEX_APPLIED_SETTINGS_INTERFACE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "lumex/LumexExport.hpp"

#include <string>

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace settings
{
// Last-dir token "interface" is a Windows SDK macro (combaseapi.h).
// Types from lumex/applied/settings/interface stay in settings.
/**
 * @brief Interface of a settings store kept in a configuration file.
 * @details This interface defines the contract for loading, saving, and
 * manipulating application settings stored in configuration files.
 * Implementations should handle platform-specific file operations and parsing
 * logic.
 * @note Thread safety: the implementations in this library
 * (`LumexSettingsINI`, `LumexSettingsJSON`, `LumexSettingsXML`) do not
 * synchronize; an instance shared between threads needs external
 * synchronization.
 * @note Exceptions: the member functions are not `noexcept`. The
 * implementations in this library report a failed load or save through the
 * return value, including the nlohmann/json `type_error` that
 * `LumexSettingsJSON` gets for a value that is not valid UTF-8, but an
 * exception such as `std::bad_alloc` can still propagate from a member.
 * @note Their `save()` writes the whole file to `<path>.tmp` and renames it
 * over `path`, so a failed save leaves the previous file unchanged
 * (`storage/LumexSettingsStorage.hpp`).
 */
class LUMEX_API ILumexSettings
{
public:
  /// @brief Defaulted virtual destructor for polymorphic behavior.
  virtual ~ILumexSettings () = default;

  /**
   * @brief Load settings from a configuration file.
   * @details "Load" means that the settings have been successfully read from
   * the file to the internal storage and can be retrieved by the `get` method
   * or modified by the `add` and `remove` methods.
   * @param path The filesystem path to the configuration file.
   * @return `true` if the settings were loaded successfully, `false`
   * otherwise.
   * @note The file format (e.g., JSON, INI) is implementation-defined.
   */
  virtual bool load (std::string const &path) = 0;

  /**
   * @brief Save settings to a configuration file.
   * @details "Save" means that the settings have been successfully written to
   * the file from the internal storage.
   * @param path The filesystem path to the target configuration file.
   * @return `true` if the settings were saved successfully, `false` otherwise.
   */
  virtual bool save (std::string const &path) const = 0;

  /**
   * @brief Retrieve a setting value by section and key.
   * @param section The section name in the configuration file.
   * @param key The key name within the section.
   * @return The value associated with the key, or an empty string if not
   * found.
   * @note Keys and sections are case-sensitive.
   */
  virtual std::string get (std::string const &section,
                           std::string const &key) const
      = 0;

  /**
   * @brief Set or update a setting value.
   * @param section The section name in the configuration file.
   * @param key The key name within the section.
   * @param value The new value to assign.
   * @note The implementations in this library create a missing section or key
   * and overwrite an existing value; they do nothing when the section, the key
   * or the value is empty.
   */
  virtual void add (std::string const &section, std::string const &key,
                    std::string const &value)
      = 0;

  /**
   * @brief Remove a setting by section and key.
   * @param section The section name in the configuration file.
   * @param key The key name within the section.
   * @note Do nothing if the section or key does not exist.
   */
  virtual void remove (std::string const &section, std::string const &key) = 0;
};
} // namespace settings
} // namespace applied
} // namespace lumex

using ILumexSettings = lumex::applied::settings::ILumexSettings;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_SETTINGS_INTERFACE_HPP

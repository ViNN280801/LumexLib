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

#ifndef LUMEX_APPLIED_SETTINGS_FACTORY_HPP
#define LUMEX_APPLIED_SETTINGS_FACTORY_HPP

#include "lumex/LumexExport.hpp"

#include <memory> ///< For RAII-managed pointers on base interface.

#include "lumex/applied/settings/ini/SupportedConfigExtensions.hpp"
#include "lumex/applied/settings/interface/ILumexSettings.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace settings
{
namespace factory
{
/**
 * @brief Factory class for creating `ILumexSettings` instances.
 * @details Provides a static method to instantiate settings objects based on
 * the desired configuration file format (INI, XML when
 * `LUMEX_SETTINGS_WITH_XML` is defined, JSON when
 * `LUMEX_SETTINGS_WITH_JSON` is defined).
 * @note Thread-safe: The factory method is stateless and can be called
 * concurrently.
 */
class LUMEX_API LumexSettingsFactory
{
public:
  /**
   * @brief Creates a settings object for the specified file format.
   * @param ext The configuration file format (e.g.,
   * `SupportedConfigExtensions::INI`).
   * @return A `std::unique_ptr` to the new `ILumexSettings` instance.
   * @throws std::invalid_argument If the format is unsupported.
   * @warning The caller assumes ownership of the returned pointer, which
   * means:
   *          - The caller is responsible for the lifetime of the returned
   * object.
   *          - The pointer must not be manually deleted! It is managed by
   * `std::unique_ptr`.
   *          - Ownership is non-shared, see
   * https://en.cppreference.com/w/cpp/memory/unique_ptr.
   */
  static std::unique_ptr<ILumexSettings> create (LumexSettingsExtensions ext);
};
} // namespace factory
} // namespace settings
} // namespace applied
} // namespace lumex

using LumexSettingsFactory
    = lumex::applied::settings::factory::LumexSettingsFactory;

#endif // !LUMEX_APPLIED_SETTINGS_FACTORY_HPP

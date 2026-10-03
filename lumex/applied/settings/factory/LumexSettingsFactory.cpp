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
#include "LumexSettingsFactory.hpp"
#include "lumex/applied/settings/ini/LumexSettingsINI.hpp"
#if defined(LUMEX_SETTINGS_WITH_JSON)
#include "lumex/applied/settings/json/LumexSettingsJSON.hpp"
#endif
#if defined(LUMEX_SETTINGS_WITH_XML)
#include "lumex/applied/settings/xml/LumexSettingsXML.hpp"
#endif

namespace lumex
{
namespace applied
{
namespace settings
{
namespace factory
{
LUMEX_PUBLIC_API
std::unique_ptr<ILumexSettings>
LumexSettingsFactory::create (LumexSettingsExtensions ext)
{
  switch (ext)
    {
    case LumexSettingsExtensions::INI:
      return std::unique_ptr<LumexSettingsINI> (new LumexSettingsINI ());
    case LumexSettingsExtensions::XML:
#if defined(LUMEX_SETTINGS_WITH_XML)
      return std::unique_ptr<LumexSettingsXML> (new LumexSettingsXML ());
#else
      return nullptr;
#endif
    case LumexSettingsExtensions::JSON:
#if defined(LUMEX_SETTINGS_WITH_JSON)
      return std::unique_ptr<LumexSettingsJSON> (new LumexSettingsJSON ());
#else
      return nullptr;
#endif
    default:
      return nullptr;
    }
}
} // namespace factory
} // namespace settings
} // namespace applied
} // namespace lumex

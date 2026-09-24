/**
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

#ifndef LUMEX_CORE_UTILITY_PROCESS_HPP
#define LUMEX_CORE_UTILITY_PROCESS_HPP

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#ifdef LUMEX_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace process
{
/**
 * @brief Returns the OS process id of the calling process.
 * @details On Windows this is `GetCurrentProcessId ()` (a `DWORD`). On
 * POSIX it is `getpid ()` cast to `unsigned long`. The return type matches
 * the historical PeakExpertWeb helper so call sites can switch without
 * changing local storage types.
 * @return Current process id as `unsigned long`. Never throws.
 */
inline unsigned long
get_current_pid () LUMEX_NOEXCEPT
{
#ifdef LUMEX_OS_WINDOWS
  return GetCurrentProcessId ();
#else
  return static_cast<unsigned long> (getpid ());
#endif
}

} // namespace process
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_PROCESS_HPP

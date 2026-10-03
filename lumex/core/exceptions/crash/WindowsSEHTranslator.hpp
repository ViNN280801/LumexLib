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

/**
 * @file WindowsSEHTranslator.hpp
 * @brief The Windows structured exception translator `seh_translator()` and
 * the `SET_SEH_TRANSLATOR` macro that installs it.
 * @details On Windows `SET_SEH_TRANSLATOR` calls
 * `_set_se_translator(seh_translator)`, which applies to the calling thread
 * only. When a structured exception (an access violation, for example) then
 * occurs in that thread, the translator lets `LumexCrashHandler` write a
 * minidump and terminates the process with the exception code. MSVC expects
 * the code that installs a translator to be compiled with `/EHa` (warning
 * C4535 otherwise). `LUMEX_EXCEPTION_HANDLE_BEGIN` of `LumexException.hpp`
 * expands to this macro. On other platforms the macro is empty and the
 * function is not declared.
 */
#ifndef LUMEX_CORE_EXCEPTIONS_CRASH_WINDOWS_SEH_TRANSLATOR_HPP
#define LUMEX_CORE_EXCEPTIONS_CRASH_WINDOWS_SEH_TRANSLATOR_HPP

#include "lumex/LumexExport.hpp"

#if defined(_WIN32)
#include <Windows.h>
#include <eh.h>
#endif

#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#if defined(LUMEX_OS_WINDOWS)
/**
 * @brief SEH translator function for Windows.
 * @param code The exception code.
 * @param info The exception information.
 */
LUMEX_PUBLIC_API
void seh_translator (LUMEX_ATTRIBUTE_MAYBE_UNUSED unsigned int code,
                     _EXCEPTION_POINTERS *info);
#endif

#if defined(LUMEX_OS_WINDOWS)
#define SET_SEH_TRANSLATOR _set_se_translator (seh_translator);
#else
#define SET_SEH_TRANSLATOR
#endif

#endif // !LUMEX_CORE_EXCEPTIONS_CRASH_WINDOWS_SEH_TRANSLATOR_HPP

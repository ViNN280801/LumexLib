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
 * @file LumexLoggingMacro.hpp
 * @brief Short macros over `LumexLogging`: `lumDebug` to `lumCritical`, and
 * the `...FL` variants that prefix the source file and line.
 * @details Each macro takes a module name and the message parts and calls the
 * `LumexLogging` function of the same level. The header also provides
 * `LUMEX_FUNCTION_NAME` (the compiler's decorated name of the enclosing
 * function) by including `LumexMacros.hpp`, which defines it.
 */
#ifndef LUMEX_APPLIED_LOGGING_LOG_LOGGING_MACRO_HPP
#define LUMEX_APPLIED_LOGGING_LOG_LOGGING_MACRO_HPP

#include "LumexLogging.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"

#define lumDebug(moduleName, ...) LumexLogging::debug (moduleName, __VA_ARGS__)
#define lumInfo(moduleName, ...) LumexLogging::info (moduleName, __VA_ARGS__)
#define lumSuccess(moduleName, ...)                                           \
  LumexLogging::success (moduleName, __VA_ARGS__)
#define lumWarning(moduleName, ...)                                           \
  LumexLogging::warning (moduleName, __VA_ARGS__)
#define lumError(moduleName, ...) LumexLogging::error (moduleName, __VA_ARGS__)
#define lumCritical(moduleName, ...)                                          \
  LumexLogging::critical (moduleName, __VA_ARGS__)

#define lumDebugFL(moduleName, ...)                                           \
  LumexLogging::debug (moduleName, "[", __FILE__, ":", __LINE__, "] ",        \
                       __VA_ARGS__)
#define lumInfoFL(moduleName, ...)                                            \
  LumexLogging::info (moduleName, "[", __FILE__, ":", __LINE__, "] ",         \
                      __VA_ARGS__)
#define lumSuccessFL(moduleName, ...)                                         \
  LumexLogging::success (moduleName, "[", __FILE__, ":", __LINE__, "] ",      \
                         __VA_ARGS__)
#define lumWarningFL(moduleName, ...)                                         \
  LumexLogging::warning (moduleName, "[", __FILE__, ":", __LINE__, "] ",      \
                         __VA_ARGS__)
#define lumErrorFL(moduleName, ...)                                           \
  LumexLogging::error (moduleName, "[", __FILE__, ":", __LINE__, "] ",        \
                       __VA_ARGS__)
#define lumCriticalFL(moduleName, ...)                                        \
  LumexLogging::critical (moduleName, "[", __FILE__, ":", __LINE__, "] ",     \
                          __VA_ARGS__)

#endif // !LUMEX_APPLIED_LOGGING_LOG_LOGGING_MACRO_HPP

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
 * @file LumexInterprocessLockError.hpp
 * @brief Why an inter-process lock was not taken, or its payload not read:
 * `lock_error` and `lock_failure_t`.
 * @details The kinds are few on purpose: a caller decides between "another
 * process holds it" (`busy`), "this user may not use the file" (the directory
 * or the file is protected) and "something else". The number the operating
 * system gave travels with them for the log (`system_code`); its text does
 * not: on Windows it is in the ANSI code page, which a UTF-8 log cannot hold.
 */
#ifndef LUMEX_CORE_INTERPROCESS_LOCK_ERROR_HPP
#define LUMEX_CORE_INTERPROCESS_LOCK_ERROR_HPP

#if defined(__clang__)
#pragma clang diagnostic push
// lock_failure_t: a one-byte kind before an int.
#pragma clang diagnostic ignored "-Wpadded"
// LUMEX_DEFINE_REFLECTED_ENUM: to_string has a default label for values
// outside the enumeration, and the companion constants are marked maybe-unused
// although the macro itself uses them.
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#endif

#include <cstdint>

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex
{
namespace core
{
namespace interprocess
{
namespace lock
{
/**
 * @brief The kind of a failure of `interprocess_lock`.
 * @details `busy`: another open file holds the lock (another process, or
 * another `interprocess_lock` of this process on the same file).
 * `access_denied`: the user may not create or open the directory or the file.
 * `io_error`: anything else (no such file for `read_payload`, a full disk, a
 * path the system refuses). `to_string ()` returns the enumerator name
 * (`"busy"`); `lumex::core::fmt` formats it by name as well.
 */
LUMEX_DEFINE_REFLECTED_ENUM (lock_error, std::uint8_t, (busy), (access_denied),
                             (io_error))

/**
 * @brief A failure: its kind and the number of the operating system.
 */
struct lock_failure_t
{
  /// @brief The kind.
  lock_error kind{ lock_error::io_error };
  /// @brief `errno` (POSIX) or `GetLastError ()` (Windows) at the failure;
  /// 0 when there is none.
  int system_code{ 0 };
};
} // namespace lock
} // namespace interprocess
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_INTERPROCESS_LOCK_ERROR_HPP

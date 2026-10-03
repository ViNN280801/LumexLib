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
 * @file LumexUtilities.hpp
 * @brief `fd_to_ptr()` and `ptr_to_fd()`, which store a POSIX file descriptor
 * in a `void *` handle slot and get it back.
 * @details The conversion goes through `intptr_t`, so a non-negative
 * descriptor survives the round trip on 32-bit and 64-bit targets; it serves
 * code that keeps a Windows `HANDLE` and a POSIX descriptor in the same
 * member. Both functions are `static inline` at global scope, so every
 * translation unit gets its own copy.
 */
#ifndef LUMEX_CORE_UTILITY_UTIL_HPP
#define LUMEX_CORE_UTILITY_UTIL_HPP

#include <cstdint>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @brief Stores a POSIX file descriptor in a `void *` handle slot.
 * @details Windows keeps a native handle as a `HANDLE`, which is a `void *`;
 * POSIX keeps an `int` file descriptor. Code that keeps either one in the
 * same `void *` member converts a descriptor with this function and gets it
 * back with `ptr_to_fd()`.
 *
 * The descriptor is first widened to `intptr_t`, an integer as wide as a
 * pointer, and then converted to `void *`. Converting an integer to a pointer
 * is implementation-defined in C++, and on 64-bit targets an `int` is
 * narrower than a pointer; going through `intptr_t` keeps both sides of the
 * conversion the same width. On flat address spaces (x86, x86-64, ARM) the
 * pointer then carries the integer value unchanged, so `ptr_to_fd()` restores
 * it on 32-bit and 64-bit targets alike.
 *
 * The function is declared at global scope, not in a namespace, and is
 * `static inline`, so every translation unit gets its own copy.
 *
 * @param fileDescriptor POSIX file descriptor.
 * @return The descriptor as a `void *`. Descriptor 0 (standard input) gives
 * a null pointer.
 * @note Meant for POSIX file descriptors, which are non-negative. A negative
 * value such as -1 also survives the round trip, but its pointer is not
 * null. A Windows `HANDLE` is assigned to the `void *` member directly.
 *
 * @par Example
 * @code
 * #if LUMEX_OS_WINDOWS
 *   m_hInputFile = m_serialPort.native_handle(); // HANDLE is a void *
 * #else
 *   m_hInputFile = fd_to_ptr(m_serialPort.native_handle()); // int to void *
 * #endif
 *
 * #if !LUMEX_OS_WINDOWS
 *   ::close(ptr_to_fd(m_hInputFile));
 * #endif
 * @endcode
 */
static inline void *
fd_to_ptr (int fileDescriptor) LUMEX_NOEXCEPT
{
  return reinterpret_cast< // NOLINT(performance-no-int-to-ptr)
      void *> (static_cast<intptr_t> (fileDescriptor));
}

/**
 * @brief Gets back the POSIX file descriptor that `fd_to_ptr()` stored in a
 * `void *`.
 * @details Converts the pointer to `intptr_t` and truncates it to `int`. For
 * a pointer made by `fd_to_ptr()`, the bits that are dropped only repeat the
 * sign of the descriptor, so the original value comes back. Like
 * `fd_to_ptr()`, the function is declared at global scope, not in a
 * namespace, and is `static inline`.
 *
 * @param pointer A descriptor stored by `fd_to_ptr()`.
 * @return The POSIX file descriptor; 0 for a null pointer.
 *
 * @par Example
 * @code
 * void *stored_ptr = fd_to_ptr(5); // fd 5 as a void *
 * int fd = ptr_to_fd(stored_ptr);  // 5 again
 * assert(fd == 5);
 * @endcode
 */
inline static int
ptr_to_fd (void *pointer) LUMEX_NOEXCEPT
{
  return static_cast<int> (reinterpret_cast<intptr_t> (pointer));
}

#endif // !LUMEX_CORE_UTILITY_UTIL_HPP

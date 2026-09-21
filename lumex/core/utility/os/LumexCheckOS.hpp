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

#ifndef LUMEX_CORE_UTILITY_OS_HPP
#define LUMEX_CORE_UTILITY_OS_HPP

/**
 * @file LumexCheckOS.hpp
 * @brief Cross-platform OS macros
 * @details Provides LUMEX_OS_* macros for detecting operating systems
 *          Compatible with MSVC, GCC, and Clang compilers
 *
 * Info get from:
 * @link stackoverflow:
 * https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
 * @link Compiler macros: https://sourceforge.net/p/predef/wiki/Compilers/
 * @link Operating System macros:
 * https://sourceforge.net/p/predef/wiki/OperatingSystems/
 */

// Reset all OS macros first
#undef LUMEX_OS_WINDOWS
#undef LUMEX_OS_LINUX
#undef LUMEX_OS_MAC
#undef LUMEX_OS_IOS
#undef LUMEX_OS_ANDROID
#undef LUMEX_OS_UNIX
#undef LUMEX_OS_POSIX
#undef LUMEX_OS_UNKNOWN

// Windows (32-bit and 64-bit)
// Works with MSVC, GCC (MinGW), and Clang
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define LUMEX_OS_WINDOWS 1

// Windows 64-bit specific
#if defined(_WIN64) || defined(__WIN64__)
#define LUMEX_OS_WINDOWS_64 1
#else
#define LUMEX_OS_WINDOWS_32 1
#endif

// Windows compiler specific
#if defined(_MSC_VER)
#define LUMEX_OS_WINDOWS_MSVC 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
#define LUMEX_OS_WINDOWS_MINGW 1
#endif

// Apple Platforms
#elif defined(__APPLE__)
#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#if TARGET_IPHONE_SIMULATOR
#define LUMEX_OS_IOS_SIMULATOR 1
#else
#define LUMEX_OS_IOS 1
#endif
#elif TARGET_OS_MAC
#define LUMEX_OS_MAC 1
#define LUMEX_OS_MACOS 1
#elif TARGET_OS_TV
#define LUMEX_OS_TVOS 1
#elif TARGET_OS_WATCH
#define LUMEX_OS_WATCHOS 1
#else
#define LUMEX_OS_APPLE_UNKNOWN 1
#endif

// Common Apple macro
#define LUMEX_OS_APPLE 1

// Android (check before Linux since Android is Linux-based)
#elif defined(__ANDROID__)
#define LUMEX_OS_ANDROID 1
#define LUMEX_OS_LINUX_KERNEL 1 // Android uses Linux kernel

// Linux
#elif defined(__linux__) || defined(linux) || defined(__linux)
#define LUMEX_OS_LINUX 1
#define LUMEX_OS_UNIX 1

// Check for specific Linux distributions if needed
#if defined(__gnu_linux__)
#define LUMEX_OS_GNU_LINUX 1
#endif

// FreeBSD
#elif defined(__FreeBSD__)
#define LUMEX_OS_FREEBSD 1
#define LUMEX_OS_UNIX 1

// OpenBSD
#elif defined(__OpenBSD__)
#define LUMEX_OS_OPENBSD 1
#define LUMEX_OS_UNIX 1

// NetBSD
#elif defined(__NetBSD__)
#define LUMEX_OS_NETBSD 1
#define LUMEX_OS_UNIX 1

// DragonFly BSD
#elif defined(__DragonFly__)
#define LUMEX_OS_DRAGONFLY 1
#define LUMEX_OS_UNIX 1

// Solaris
#elif defined(__sun) || defined(sun)
#define LUMEX_OS_SOLARIS 1
#define LUMEX_OS_UNIX 1

// AIX
#elif defined(_AIX)
#define LUMEX_OS_AIX 1
#define LUMEX_OS_UNIX 1

// HP-UX
#elif defined(__hpux)
#define LUMEX_OS_HPUX 1
#define LUMEX_OS_UNIX 1

// Generic Unix
#elif defined(__unix__) || defined(__unix) || defined(unix)
#define LUMEX_OS_UNIX 1

// POSIX (fallback)
#elif defined(_POSIX_VERSION)
#define LUMEX_OS_POSIX 1

// Unknown OS
#else
#define LUMEX_OS_UNKNOWN 1
#if defined(__GNUC__) || defined(__clang__)
#warning "Unknown operating system detected"
#elif defined(_MSC_VER)
#pragma message("Unknown operating system detected")
#endif
#endif

// Architecture Helpers
#if defined(_M_X64) || defined(__x86_64__) || defined(__x86_64)               \
    || defined(__amd64__) || defined(__amd64)
#define LUMEX_ARCH_X64 1
#define LUMEX_ARCH_64BIT 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(i386)
#define LUMEX_ARCH_X86 1
#define LUMEX_ARCH_32BIT 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define LUMEX_ARCH_ARM64 1
#define LUMEX_ARCH_64BIT 1
#elif defined(_M_ARM) || defined(__arm__) || defined(__arm)
#define LUMEX_ARCH_ARM 1
#define LUMEX_ARCH_32BIT 1
#endif

// Compiler
#if defined(_MSC_VER)
#define LUMEX_COMPILER_MSVC 1
#define LUMEX_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define LUMEX_COMPILER_CLANG 1
#define LUMEX_COMPILER_VERSION                                                \
  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define LUMEX_COMPILER_GCC 1
#define LUMEX_COMPILER_VERSION                                                \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

// Convenience macros for common checks
#define LUMEX_OS_IS_WINDOWS() (defined (LUMEX_OS_WINDOWS))
#define LUMEX_OS_IS_LINUX()                                                   \
  (defined (LUMEX_OS_LINUX) || defined (LUMEX_OS_ANDROID))
#define LUMEX_OS_IS_ANDROID() (defined (LUMEX_OS_ANDROID))
#define LUMEX_OS_IS_MACOS()                                                   \
  (defined (LUMEX_OS_MAC) || defined (LUMEX_OS_MACOS))
#define LUMEX_OS_IS_IOS() (defined (LUMEX_OS_IOS))
#define LUMEX_OS_IS_ANDROID() (defined (LUMEX_OS_ANDROID))
#define LUMEX_OS_IS_APPLE() (defined (LUMEX_OS_APPLE))
#define LUMEX_OS_IS_UNIX()                                                    \
  (defined (LUMEX_OS_UNIX) || defined (LUMEX_OS_LINUX)                        \
   || defined (LUMEX_OS_APPLE))
#define LUMEX_OS_IS_POSIX() (defined (LUMEX_OS_POSIX) || LUMEX_OS_IS_UNIX ())

/* Usage example */
/*
#include "LumexCheckOS.hpp"

if (LUMEX_OS_IS_WINDOWS()) {
    // Windows specific code
} else if (LUMEX_OS_IS_LINUX()) {
    // Linux specific code
} else if (LUMEX_OS_IS_APPLE()) {
    // macOS specific code
}
*/

// Debug information macros (only in debug builds)
#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
#define LUMEX_OS_DEBUG_INFO()                                                 \
  do                                                                          \
    {                                                                         \
      static char const *os_name = LUMEX_OS_IS_WINDOWS ()   ? "Windows"       \
                                   : LUMEX_OS_IS_MACOS ()   ? "macOS"         \
                                   : LUMEX_OS_IS_IOS ()     ? "iOS"           \
                                   : LUMEX_OS_IS_ANDROID () ? "Android"       \
                                   : LUMEX_OS_IS_LINUX ()   ? "Linux"         \
                                   : LUMEX_OS_IS_UNIX ()    ? "Unix"          \
                                   : LUMEX_OS_IS_POSIX ()   ? "POSIX"         \
                                                            : "Unknown";        \
    }                                                                         \
  while (0)
#else
#define LUMEX_OS_DEBUG_INFO() ;
#endif

#if defined(LUMEX_OS_IS_WINDOWS) && LUMEX_OS_IS_WINDOWS
#define LUMEX_FUNC_NAME __FUNCSIG__
#elif defined(LUMEX_OS_IS_UNIX) && LUMEX_OS_IS_UNIX
#define LUMEX_FUNC_NAME __PRETTY_FUNCTION__
#else
#define LUMEX_FUNC_NAME __func__
#endif

#endif // !LUMEX_CORE_UTILITY_OS_HPP

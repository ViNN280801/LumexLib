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
 * @file LumexProtocolPlatformMacros.hpp
 * @brief The collision guard of the protocol tables: `is_platform_macro_name`
 * and `LUMEX_PROTOCOL_CHECK_IDENTIFIERS`.
 * @details A row identifier becomes the name of a type (`DELETE_t`) and of a
 * client method (`DELETE (...)`). The method name is a bare token, so a
 * platform header that defines a macro of that name (`<windows.h>` defines
 * `DELETE`, `ERROR`, `IN`, `OUT`, `min`, `max`; `<termios.h>` defines `ECHO`;
 * glibc defines `major` and `minor`) would break the table in some
 * translation units only. The guard rejects such identifiers on every
 * platform, whether the header is included or not: the row is written with
 * `X_AS (another_identifier, "DELETE", ...)`, which keeps the specification
 * name. `LUMEX_PROTOCOL_DEFINE` and `LUMEX_PROTOCOL_EXTEND` check every row;
 * `LUMEX_PROTOCOL_CHECK_IDENTIFIERS (LIST);` checks a list on its own.
 *
 * The list is curated, not complete: the macros of the Windows SDK headers
 * that `<windows.h>` includes (object-like names, the `A`/`W` function
 * names, serial and status codes) and of the usual POSIX and glibc headers
 * (`<unistd.h>`, `<fcntl.h>`, `<termios.h>`, `<sys/ioctl.h>`,
 * `<sys/param.h>`, `<sys/sysmacros.h>`, `<signal.h>`, `<errno.h>`,
 * `<math.h>`, `<stdio.h>`, `<stdlib.h>`) whose names a command table could
 * plausibly use. It is sorted by byte value for the binary search.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_PLATFORM_MACROS_HPP
#define LUMEX_CORE_PROTOCOL_DSL_PLATFORM_MACROS_HPP

#include <cstddef>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wdeprecated-redundant-constexpr-static-def")
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#endif
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
namespace detail
{
// A class template, so that the array can be defined in the header below
// C++17 (its elements are read in constant expressions).
template <typename Tag> struct platform_macro_table
{
  static LUMEX_CONSTEXPR char const *const names[]
      = { "ABSOLUTE",
          "ALTERNATE",
          "APIENTRY",
          "B0",
          "B110",
          "B115200",
          "B1200",
          "B134",
          "B150",
          "B1800",
          "B19200",
          "B200",
          "B230400",
          "B2400",
          "B300",
          "B38400",
          "B4800",
          "B50",
          "B57600",
          "B600",
          "B75",
          "B9600",
          "BIG_ENDIAN",
          "BRKINT",
          "BUFSIZ",
          "BYTE_ORDER",
          "CALLBACK",
          "CBRK",
          "CDECL",
          "CDISCARD",
          "CDSUSP",
          "CEOF",
          "CEOL",
          "CEOT",
          "CERASE",
          "CFLUSH",
          "CHAR_BIT",
          "CINTR",
          "CKILL",
          "CLNEXT",
          "CLOCAL",
          "CLOCKS_PER_SEC",
          "CLRBREAK",
          "CLRDTR",
          "CLRRTS",
          "CMIN",
          "CONST",
          "CQUIT",
          "CREAD",
          "CREATE_ALWAYS",
          "CREATE_NEW",
          "CREPRINT",
          "CRPRNT",
          "CRTSCTS",
          "CS5",
          "CS6",
          "CS7",
          "CS8",
          "CSIZE",
          "CSTART",
          "CSTATUS",
          "CSTOP",
          "CSTOPB",
          "CSUSP",
          "CTIME",
          "CTRL",
          "CWERASE",
          "CopyFile",
          "CreateDirectory",
          "CreateEvent",
          "CreateFile",
          "CreateMutex",
          "CreateProcess",
          "CreateSemaphore",
          "CreateService",
          "CreateWindow",
          "DELETE",
          "DEV_BSIZE",
          "DIFFERENCE",
          "DOMAIN",
          "DeleteFile",
          "DispatchMessage",
          "DrawText",
          "EACCES",
          "EAGAIN",
          "EALREADY",
          "EBADF",
          "EBUSY",
          "ECANCELED",
          "ECHO",
          "ECHOE",
          "ECHOK",
          "ECHONL",
          "ECONNREFUSED",
          "ECONNRESET",
          "EEXIST",
          "EINPROGRESS",
          "EINTR",
          "EINVAL",
          "EIO",
          "ENODEV",
          "ENOENT",
          "ENOMEM",
          "ENOSPC",
          "ENOTSUP",
          "ENXIO",
          "EOF",
          "EOVERFLOW",
          "EPERM",
          "EPIPE",
          "EPROTO",
          "ERANGE",
          "ERROR",
          "ETIMEDOUT",
          "EVENPARITY",
          "EWOULDBLOCK",
          "EXIT_FAILURE",
          "EXIT_SUCCESS",
          "FALSE",
          "FAR",
          "FD_CLOEXEC",
          "FILENAME_MAX",
          "FILE_BEGIN",
          "FILE_CURRENT",
          "FILE_END",
          "FIONBIO",
          "FIONREAD",
          "FOCUS_EVENT",
          "FP_INFINITE",
          "FP_NAN",
          "FP_NORMAL",
          "FP_SUBNORMAL",
          "FP_ZERO",
          "F_GETFD",
          "F_GETFL",
          "F_OK",
          "F_SETFD",
          "F_SETFL",
          "FindResource",
          "FindWindow",
          "GENERIC_ALL",
          "GENERIC_EXECUTE",
          "GENERIC_READ",
          "GENERIC_WRITE",
          "GetClassName",
          "GetCommandLine",
          "GetComputerName",
          "GetCurrentDirectory",
          "GetFileAttributes",
          "GetMessage",
          "GetModuleFileName",
          "GetModuleHandle",
          "GetObject",
          "GetTempPath",
          "GetUserName",
          "HIBYTE",
          "HIWORD",
          "HUGE_VAL",
          "HUPCL",
          "HZ",
          "ICANON",
          "ICRNL",
          "IDABORT",
          "IDCANCEL",
          "IDCLOSE",
          "IDHELP",
          "IDIGNORE",
          "IDNO",
          "IDOK",
          "IDRETRY",
          "IDYES",
          "IEXTEN",
          "IGNBRK",
          "IGNCR",
          "IGNORE",
          "IGNPAR",
          "IN",
          "INFINITE",
          "INFINITY",
          "INLCR",
          "INPCK",
          "INVALID_HANDLE_VALUE",
          "ISIG",
          "ISTRIP",
          "IXANY",
          "IXOFF",
          "IXON",
          "KEY_EVENT",
          "LITTLE_ENDIAN",
          "LOBYTE",
          "LOWORD",
          "LoadImage",
          "LoadLibrary",
          "LoadString",
          "MAKELONG",
          "MAKEWORD",
          "MARKPARITY",
          "MAX",
          "MAXBYTE",
          "MAXCHAR",
          "MAXDWORD",
          "MAXLONG",
          "MAXPATHLEN",
          "MAXSHORT",
          "MAXSYMLINKS",
          "MAXWORD",
          "MAX_PATH",
          "MB_CUR_MAX",
          "MB_LEN_MAX",
          "MENU_EVENT",
          "MIN",
          "MINCHAR",
          "MINLONG",
          "MINSHORT",
          "MOUSE_EVENT",
          "M_E",
          "M_PI",
          "MessageBox",
          "MoveFile",
          "NAME_MAX",
          "NAN",
          "NBBY",
          "NCARGS",
          "NEAR",
          "NGROUPS",
          "NOFILE",
          "NOFLSH",
          "NOPARITY",
          "NO_ERROR",
          "NULL",
          "OCRNL",
          "ODDPARITY",
          "OFILL",
          "ONE5STOPBITS",
          "ONESTOPBIT",
          "ONLCR",
          "ONLRET",
          "ONOCR",
          "OPAQUE",
          "OPEN_ALWAYS",
          "OPEN_EXISTING",
          "OPOST",
          "OPTIONAL",
          "OUT",
          "OVERFLOW",
          "O_APPEND",
          "O_CREAT",
          "O_EXCL",
          "O_NOCTTY",
          "O_NONBLOCK",
          "O_RDONLY",
          "O_RDWR",
          "O_SYNC",
          "O_TRUNC",
          "O_WRONLY",
          "OpenEvent",
          "OpenService",
          "PARENB",
          "PARMRK",
          "PARODD",
          "PASCAL",
          "PATH_MAX",
          "PDP_ENDIAN",
          "PIPE_BUF",
          "PLOSS",
          "PURE",
          "PeekMessage",
          "PlaySound",
          "PostMessage",
          "RAND_MAX",
          "READ_CONTROL",
          "RELATIVE",
          "RESETDEV",
          "R_OK",
          "RemoveDirectory",
          "ReportEvent",
          "SEEK_CUR",
          "SEEK_END",
          "SEEK_SET",
          "SETBREAK",
          "SETDTR",
          "SETRTS",
          "SETXOFF",
          "SETXON",
          "SIGABRT",
          "SIGALRM",
          "SIGBUS",
          "SIGCHLD",
          "SIGCONT",
          "SIGFPE",
          "SIGHUP",
          "SIGILL",
          "SIGINT",
          "SIGKILL",
          "SIGPIPE",
          "SIGQUIT",
          "SIGSEGV",
          "SIGSTOP",
          "SIGTERM",
          "SIGTRAP",
          "SIGUSR1",
          "SIGUSR2",
          "SIG_DFL",
          "SIG_ERR",
          "SIG_IGN",
          "SING",
          "SPACEPARITY",
          "STATUS_ACCESS_VIOLATION",
          "STATUS_CONTROL_C_EXIT",
          "STATUS_INTEGER_OVERFLOW",
          "STATUS_INVALID_HANDLE",
          "STATUS_INVALID_PARAMETER",
          "STATUS_NO_MEMORY",
          "STATUS_PENDING",
          "STATUS_STACK_OVERFLOW",
          "STATUS_TIMEOUT",
          "STATUS_WAIT_0",
          "STDERR_FILENO",
          "STDIN_FILENO",
          "STDOUT_FILENO",
          "STRICT",
          "SYNCHRONIZE",
          "SendMessage",
          "SetCurrentDirectory",
          "SetPort",
          "SetWindowText",
          "StartService",
          "TCFLSH",
          "TCGETS",
          "TCIFLUSH",
          "TCIOFF",
          "TCIOFLUSH",
          "TCION",
          "TCOFLUSH",
          "TCOOFF",
          "TCOON",
          "TCSADRAIN",
          "TCSAFLUSH",
          "TCSANOW",
          "TCSBRK",
          "TCSETS",
          "TCXONC",
          "TEXT",
          "THIS",
          "TIOCCBRK",
          "TIOCEXCL",
          "TIOCMBIC",
          "TIOCMBIS",
          "TIOCMGET",
          "TIOCMSET",
          "TIOCNXCL",
          "TIOCSBRK",
          "TLOSS",
          "TOSTOP",
          "TRANSPARENT",
          "TRUE",
          "TRUNCATE_EXISTING",
          "TWOSTOPBITS",
          "UNDERFLOW",
          "VEOF",
          "VEOL",
          "VERASE",
          "VINTR",
          "VKILL",
          "VMIN",
          "VOID",
          "VQUIT",
          "VSTART",
          "VSTOP",
          "VSUSP",
          "VTIME",
          "WAIT_ABANDONED",
          "WAIT_FAILED",
          "WAIT_OBJECT_0",
          "WAIT_TIMEOUT",
          "WINAPI",
          "WINDING",
          "WRITE_DAC",
          "WRITE_OWNER",
          "W_OK",
          "X_OK",
          "Yield",
          "assert",
          "cdecl",
          "clrbit",
          "errno",
          "far",
          "howmany",
          "hyper",
          "interface",
          "isclr",
          "isset",
          "linux",
          "major",
          "makedev",
          "max",
          "min",
          "minor",
          "near",
          "offsetof",
          "pascal",
          "powerof2",
          "roundup",
          "setbit",
          "small",
          "stderr",
          "stdin",
          "stdout",
          "unix" };
};

// Needed below C++17 (an element read at run time odr-uses the array),
// redundant from C++17 on, where the member is implicitly inline.
template <typename Tag>
LUMEX_CONSTEXPR char const *const platform_macro_table<Tag>::names[];

// strcmp as a constant expression: negative, zero or positive.
LUMEX_CONSTEXPR inline int
compare_names (char const *left, char const *right) LUMEX_NOEXCEPT
{
  return (*left != *right || *left == '\0')
             ? static_cast<int> (static_cast<unsigned char> (*left))
                   - static_cast<int> (static_cast<unsigned char> (*right))
             : compare_names (left + 1, right + 1);
}

// Binary search in [first, last): the recursion depth is the logarithm of
// the list size plus the length of the name.
LUMEX_CONSTEXPR inline bool
search_names (char const *name, std::size_t first,
              std::size_t last) LUMEX_NOEXCEPT
{
  return first >= last
             ? false
             : (compare_names (
                    name,
                    platform_macro_table<void>::names[first
                                                      + (last - first) / 2])
                        == 0
                    ? true
                    : (compare_names (
                           name, platform_macro_table<
                                     void>::names[first + (last - first) / 2])
                               < 0
                           ? search_names (name, first,
                                           first + (last - first) / 2)
                           : search_names (
                                 name, first + (last - first) / 2 + 1, last)));
}
} // namespace detail

/// @brief The number of names in the platform macro list.
LUMEX_CONSTEXPR inline std::size_t
platform_macro_count () LUMEX_NOEXCEPT
{
  return sizeof (detail::platform_macro_table<void>::names)
         / sizeof (detail::platform_macro_table<void>::names[0]);
}

/// @brief The name at `index` of the sorted platform macro list.
LUMEX_CONSTEXPR inline char const *
platform_macro_name (std::size_t index) LUMEX_NOEXCEPT
{
  return detail::platform_macro_table<void>::names[index];
}

/**
 * @brief Whether `name` is a macro of a platform header (see the file
 * description); usable in `static_assert`.
 */
LUMEX_CONSTEXPR inline bool
is_platform_macro_name (char const *name) LUMEX_NOEXCEPT
{
  return detail::search_names (name, 0, platform_macro_count ());
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/// @cond LUMEX_INTERNAL
#define LUMEX_PROTOCOL_DETAIL_CHECK_TEXT(id_text)                             \
  static_assert (                                                             \
      !::lumex::core::protocol::dsl::is_platform_macro_name (id_text),        \
      "LUMEX_PROTOCOL: the row identifier " id_text                           \
      " is the name of a platform macro (windows.h, POSIX): write the row "   \
      "as X_AS (another_identifier, \"" id_text "\", ...)")
#define LUMEX_PROTOCOL_DETAIL_CHECK(id_, key_, desc_, fields_, names_,        \
                                    reply_, options_)                         \
  LUMEX_PROTOCOL_DETAIL_CHECK_TEXT (#id_);
#define LUMEX_PROTOCOL_DETAIL_CHECK_AS(id_, spec_, key_, desc_, fields_,      \
                                       names_, reply_, options_)              \
  LUMEX_PROTOCOL_DETAIL_CHECK_TEXT (#id_);
/// @endcond

/**
 * @brief Checks the row identifiers of a table list against the platform
 * macro names: `LUMEX_PROTOCOL_CHECK_IDENTIFIERS (MY_ROWS);`
 * @details Expands to a struct named after the list with one
 * `static_assert` per row, so it is valid at namespace and at class scope.
 * The `X_AS` rows are checked by their identifier, not by their
 * specification name.
 */
#define LUMEX_PROTOCOL_CHECK_IDENTIFIERS(LIST_)                               \
  struct LIST_##_lumex_identifiers_checked                                    \
  {                                                                           \
    LIST_ (LUMEX_PROTOCOL_DETAIL_CHECK, LUMEX_PROTOCOL_DETAIL_CHECK_AS)       \
  }

#endif // !LUMEX_CORE_PROTOCOL_DSL_PLATFORM_MACROS_HPP

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

#ifndef LUMEX_CORE_UTILITY_DEBUG_HPP
#define LUMEX_CORE_UTILITY_DEBUG_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#if LUMEX_OS_WINDOWS
#include <Windows.h>

#include <DbgHelp.h>

#include <array>
#include <atomic>
#include <mutex>
#else
#include <array>
#include <unistd.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#endif
#if __has_include(<dlfcn.h>)
#include <dlfcn.h>
#endif
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif
#endif

#include "lumex/core/string/format/LumexStringify.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace debug
{
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-bounds-constant-array-index,
// cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
namespace Detail
{
// Format an address as uppercase hexadecimal digits, no leading zeros/padding
// (e.g. "1A2B3C"), matching the "[0x...]" style used throughout this file.
// NOTE: fmt is NOT a declared dependency anywhere else in LumexLib (no
// fmt::format usage/link in the rest of the tree), so this intentionally
// avoids relying on it - std::ostringstream + std::hex gives the exact same
// output without adding a new external dependency.
static inline std::string
formatHex (std::uintptr_t value)
{
  std::ostringstream oss;
  oss << std::hex << std::uppercase << value;
  return oss.str ();
}

#if (defined(__GNUC__) || defined(__clang__)) && defined(LUMEX_OS_LINUX)      \
    && LUMEX_OS_LINUX
// Resolve address to function name using addr2line (POSIX fallback)
static inline std::string
resolveAddressWithAddr2line (void *addr, char const *exe_path) LUMEX_NOEXCEPT
{
  try
    {
      // Create pipe for addr2line output
      std::array<char, 512> buffer{};
      std::string result;

      // Build command: addr2line -C -f -e <executable> <address>
      std::string cmd = lumex::core::string::format::stringify (
          "addr2line -C -f -e ", exe_path, " 0x",
          formatHex (reinterpret_cast<uintptr_t> (addr)), " 2>/dev/null");

      FILE *pipe = popen (cmd.c_str (),
                          "r"); // NOLINT(cppcoreguidelines-owning-memory)
      if (!pipe)
        return "";

      // Read function name (first line)
      if (fgets (buffer.data (), buffer.size (), pipe) != nullptr)
        {
          result = buffer.data ();
          // Remove newline
          if (!result.empty () && result.back () == '\n')
            result.pop_back ();

          // Read file:line (second line) and append
          if (fgets (buffer.data (), buffer.size (), pipe) != nullptr)
            {
              std::string location = buffer.data ();
              if (!location.empty () && location.back () == '\n')
                location.pop_back ();

              // Filter out "??:0" and "??:?"
              if (location != "??:0" && location != "??:?")
                result += " at " + location;
            }
        }

      pclose (pipe); // NOLINT(cppcoreguidelines-owning-memory)

      // Return empty if addr2line failed (contains "?" or "0x")
      if (result.find ("??") != std::string::npos || result.empty ())
        return "";

      return result;
    }
  catch (...)
    {
      return "";
    }
}

// Get executable path for addr2line
static inline std::string
getExecutablePath () LUMEX_NOEXCEPT
{
  try
    {
#if LUMEX_OS_LINUX
      std::array<char, 4096> buffer{};
      ssize_t len
          = readlink ("/proc/self/exe", buffer.data (), buffer.size () - 1);
      if (len > 0)
        {
          buffer[static_cast<std::size_t> (len)] = '\0';
          return std::string (buffer.data ());
        }
#endif
      return "";
    }
  catch (...)
    {
      return "";
    }
}
#endif

#if LUMEX_OS_WINDOWS
// Thread-safe DbgHelp mutex (Windows requires external synchronization)
static inline std::mutex &
getDbgHelpMutex () LUMEX_NOEXCEPT
{
  static std::mutex mtx;
  return mtx;
}

// Get executable directory for PDB search path.
// NOTE: intentionally self-contained (does not use
// lumex::core::filesystem::fs::lumex_filesystem::get_exe_path()):
// lumex/core/filesystem publicly depends on lumex/core/utility (see its
// CMakeLists.txt), so utility must not depend back on filesystem - that would
// create a circular module dependency in the build graph.
static inline std::string
getExeDirectory () LUMEX_NOEXCEPT
{
  try
    {
      std::array<char, 4096> buf{};
      DWORD const len = GetModuleFileNameA (nullptr, buf.data (),
                                            static_cast<DWORD> (buf.size ()));
      if (len == 0 || len >= buf.size ())
        return {};
      std::string path (buf.data (), len);
      std::size_t const lastSlash = path.find_last_of ("\\/");
      return (lastSlash != std::string::npos) ? path.substr (0, lastSlash + 1)
                                              : std::string{};
    }
  catch (...)
    {
      return {};
    }
}

// Lazy initialization for SymInitialize (amortize overhead)
static inline bool
ensureSymbolsInitialized (HANDLE process) LUMEX_NOEXCEPT
{
  static std::atomic_bool initialized{ false };
  static std::once_flag init_flag;

  if (initialized.load (std::memory_order_acquire))
    return true;

  std::call_once (init_flag,
                  [process] ()
                    {
                      std::lock_guard<std::mutex> lock (getDbgHelpMutex ());
                      // SymSetOptions MUST be called BEFORE SymInitialize
                      // (MSDN)
                      SymSetOptions (SYMOPT_LOAD_LINES | SYMOPT_UNDNAME
                                     | SYMOPT_DEFERRED_LOADS);
                      std::string const exeDir = getExeDirectory ();
                      char const *searchPath
                          = exeDir.empty () ? nullptr : exeDir.c_str ();
                      if (SymInitialize (process, searchPath, TRUE))
                        {
                          if (!exeDir.empty ())
                            SymSetSearchPath (process, exeDir.c_str ());
                          initialized.store (true, std::memory_order_release);
                        }
                    });

  return initialized.load (std::memory_order_acquire);
}
#elif defined(__GNUC__) || defined(__clang__)
// Demangle C++ symbols on POSIX systems
static inline std::string
demangleSymbol (char const *mangled) LUMEX_NOEXCEPT
{
  if (!mangled)
    return "??";

  int status = -1;
  char *demangled_ = nullptr;

#if __has_include(<cxxabi.h>)
  demangled_ = abi::__cxa_demangle (mangled, nullptr, nullptr,
                                    std::addressof (status));
#endif
  std::string result = (status == 0 && demangled_) ? demangled_ : mangled;

  if (demangled_)
    free (
        demangled_); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

  return result;
}
#endif
} // namespace Detail

/**
 * @brief Capture current call stack in a formatted string.
 *
 * @details
 * Windows (MSVC):
 * - Uses DbgHelp API (CaptureStackBackTrace + SymFromAddr)
 * - Requires PDB files for function names
 * - Thread-safe through internal mutex
 * - Lazy initialization to minimize overhead
 *
 * Linux/POSIX (GCC/Clang):
 * - Uses GNU backtrace() + backtrace_symbols() (not POSIX, but widely
 * available)
 * - Automatic demangling C++ symbols through __cxa_demangle
 * - Requires compilation with -g and linking with -rdynamic
 * - Fallback on dladdr() when backtrace is not available
 *
 * Performance:
 * - Debug mode: ~150-400 μs on 10 frames
 * - Release with symbols: ~100-250 μs
 * - Release without symbols: ~50-100 μs (only addresses)
 *
 * Graceful Degradation:
 * - Without debug symbols: returns addresses in hex format
 * - On errors: returns "Stack trace unavailable: <reason>"
 * - Thread-safe in all modes
 *
 * @param skip_frames Number of frames to skip (default 1 - itself
 * captureStackTrace)
 * @param max_frames Maximum number of frames to capture (default 16)
 *
 * @return std::string Formatted stack trace, each frame on a new line.
 *         Format: "  #N: function_name (file:line) [0xADDRESS]"
 *
 * @note
 * - Function noexcept - never throws exceptions
 * - Can allocate memory inside (NOT async-signal-safe!)
 * - Optimized for rare calls (logging errors)
 * - DO NOT use in signal handlers or hot paths
 *
 * @warning
 * - Windows: Requires DbgHelp.lib in linking
 * - Linux: Requires -rdynamic for symbol export
 * - Inline functions may be missing in trace at aggressive optimization
 *
 * @example
 * @code
 * void setConfigToDefault() {
 *   std::string trace = lumex::core::utility::debug::captureStackTrace(1, 5);
 *   lumWarning("Config reset. Call stack:\n", trace);
 *   // Output:
 *   //   #0: initChannelAmqpErrors() at ChromatographicController.cpp:140
 * [0x7FF6A2B41234]
 *   //   #1: CMChromatographicController::CMChromatographicController()
 * [0x7FF6A2B3F890]
 *   //   #2: ... etc
 * }
 * @endcode
 */
static inline std::string
captureStackTrace (int skip_frames = 1, int max_frames = 16) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR int kMaxStackFrames = 64;
  std::string result;

  try
    {
#if LUMEX_OS_WINDOWS
      // Windows: DbgHelp API for symbol information
      void *stack[kMaxStackFrames];

      HANDLE process = GetCurrentProcess ();

      // Thread-safe symbol initialization (lazy, once)
      if (!Detail::ensureSymbolsInitialized (process))
        {
          // Fallback: only addresses without symbols
          WORD frames = CaptureStackBackTrace (
              static_cast<DWORD> (skip_frames),
              static_cast<DWORD> (max_frames), stack, nullptr);

          for (WORD i = 0; i < frames; ++i)
            result += lumex::core::string::format::stringify (
                "  #", i, ": [0x",
                Detail::formatHex (reinterpret_cast<uintptr_t> (stack[i])),
                "]\n");
          return result.empty ()
                     ? "  Stack trace unavailable (SymInitialize failed)\n"
                     : result;
        }

      // Capture stack with full symbol resolution
      WORD frames = CaptureStackBackTrace (static_cast<DWORD> (skip_frames),
                                           static_cast<DWORD> (max_frames),
                                           stack, nullptr);

      if (frames == 0)
        return "  Stack trace empty (no frames captured)\n";

      // Allocate symbol buffer (variable size structure)
      LUMEX_CONSTEXPR std::size_t kSymbolBufferSize
          = sizeof (SYMBOL_INFO) + (MAX_SYM_NAME * sizeof (TCHAR));
      auto *symbol_buffer
          = static_cast<SYMBOL_INFO *> (malloc (kSymbolBufferSize));

      if (symbol_buffer == nullptr)
        return "  Stack trace unavailable (memory allocation failed)\n";

      symbol_buffer->MaxNameLen = MAX_SYM_NAME;
      symbol_buffer->SizeOfStruct = sizeof (SYMBOL_INFO);

      // Thread-safe symbol resolution
      std::lock_guard<std::mutex> lock (Detail::getDbgHelpMutex ());

      // Refresh module list to include DLLs loaded after SymInitialize (e.g.
      // plugins loaded at runtime). Ensures SymFromAddr can resolve addresses
      // in dynamically loaded modules.
      SymRefreshModuleList (process);

      for (WORD i = 0; i < frames; ++i)
        {
          DWORD64 address = reinterpret_cast<DWORD64> (stack[i]);

          // Get function name
          DWORD64 displacement = 0;
          if (SymFromAddr (process, address, &displacement, symbol_buffer))
            {
              // Try to get line number and file name
              IMAGEHLP_LINE64 line{};
              line.SizeOfStruct = sizeof (IMAGEHLP_LINE64);
              DWORD line_displacement = 0;

              if (SymGetLineFromAddr64 (process, address, &line_displacement,
                                        &line))
                {
                  // Full info: function + file:line + address
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", symbol_buffer->Name, " (", line.FileName,
                      ":", line.LineNumber, ") [0x",
                      Detail::formatHex (address), "]\n");
                }
              else
                {
                  // Only function name + address (no line info)
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", symbol_buffer->Name, " [0x",
                      Detail::formatHex (address), "]\n");
                }
            }
          else
            {
              // Fallback: try SymGetModuleInfo64 for module name (e.g.
              // ntdll.dll+0x1234)
              IMAGEHLP_MODULE64 moduleInfo{};
              moduleInfo.SizeOfStruct = sizeof (IMAGEHLP_MODULE64);
              if (SymGetModuleInfo64 (process, address, &moduleInfo))
                {
                  DWORD64 const offset = address - moduleInfo.BaseOfImage;
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", moduleInfo.ModuleName, "+0x",
                      Detail::formatHex (offset), " [0x",
                      Detail::formatHex (address), "]\n");
                }
              else
                {
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": [0x", Detail::formatHex (address), "]\n");
                }
            }
        }

      free (
          symbol_buffer); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

#elif defined(__GNUC__) || defined(__clang__)
      // Linux/POSIX: backtrace() + dladdr() for symbols
      // NOTE: backtrace() - GNU extension, not in POSIX.1-2024, but widely
      // available

#if __has_include(<execinfo.h>)
      void *addresses[kMaxStackFrames];

      int frame_count = backtrace (addresses, kMaxStackFrames);

      if (frame_count <= skip_frames)
        return "  Stack trace empty (insufficient frames)\n";

      int actual_frames = std::min (frame_count - skip_frames, max_frames);

      // Use backtrace_symbols for quick resolution (allocates memory)
      char **symbols
          = backtrace_symbols (addresses + skip_frames, actual_frames);

      if (!symbols)
        {
          // Fallback: dladdr() for each address
          for (int i = 0; i < actual_frames; ++i)
            {
              Dl_info info;
              if (dladdr (addresses[i + skip_frames], &info))
                {
                  std::string demangled
                      = Detail::demangleSymbol (info.dli_sname);
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", demangled, " [0x",
                      Detail::formatHex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
              else
                {
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": [0x",
                      Detail::formatHex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
            }
          return result;
        }

      // Get executable path for addr2line fallback
      static std::string const exe_path = Detail::getExecutablePath ();
      bool const use_addr2line_fallback = !exe_path.empty ();

      // Process each frame with demangling
      for (int i = 0; i < actual_frames; ++i)
        {
          Dl_info info;
          bool has_symbol
              = dladdr (addresses[i + skip_frames], &info) && info.dli_sname;
          if (has_symbol)
            {
              // Demangle C++ symbol
              std::string demangled = Detail::demangleSymbol (info.dli_sname);

              // Calculate offset within function
              ptrdiff_t offset
                  = reinterpret_cast<char *> (addresses[i + skip_frames])
                    - reinterpret_cast<char *> (info.dli_saddr);

              result += lumex::core::string::format::stringify (
                  "  #", i, ": ", demangled, " +", offset, " [0x",
                  Detail::formatHex (reinterpret_cast<uintptr_t> (
                      addresses[i + skip_frames])),
                  "]\n");
            }
          else
            {
              // Try addr2line fallback if no symbol info available
              std::string addr2line_result;
              if (use_addr2line_fallback)
                addr2line_result = Detail::resolveAddressWithAddr2line (
                    addresses[i + skip_frames], exe_path.c_str ());

              if (!addr2line_result.empty ())
                {
                  // addr2line succeeded - use its output
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", addr2line_result, " [0x",
                      Detail::formatHex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
              else
                {
                  // Complete fallback: raw backtrace_symbols output
                  result += lumex::core::string::format::stringify (
                      "  #", i, ": ", symbols[i], " [0x",
                      Detail::formatHex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
            }
        }

      free (
          symbols); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

#else
      // No backtrace support - return basic info
      result = "  Stack trace unavailable (<execinfo.h> not available)\n";
      result += lumex::core::string::format::stringify (
          "  Current function: ", LUMEX_FUNCTION_NAME, "\n");
#endif

#else
      // Unknown platform
      result = "  Stack trace not supported on this platform\n";
      result += lumex::core::string::format::stringify (
          "  Current function: ", LUMEX_FUNCTION_NAME, "\n");
#endif
    }
  catch (std::exception const &exc)
    {
      return lumex::core::string::format::stringify (
          "  Stack trace unavailable (exception: ", exc.what (), ")\n");
    }
  catch (...)
    {
      return "  Stack trace unavailable (unknown exception)\n";
    }

  return result.empty () ? "  Stack trace empty\n" : result;
}

/**
 * @brief Lightweight alternative: captures only the immediate caller.
 *
 * Calling through the LUMEX_CAPTURE_CALLER_INFO() macro ensures correctness on
 * all compilers, since the values are substituted at the call site (inside the
 * calling function).
 */
static inline std::string
captureCallerInfoImpl (char const *caller_function, char const *caller_file,
                       int caller_line) LUMEX_NOEXCEPT
{
  try
    {
      std::string file_name = caller_file;
      std::size_t last_slash = file_name.find_last_of ("/\\");
      if (last_slash != std::string::npos)
        file_name = file_name.substr (last_slash + 1);

      return lumex::core::string::format::stringify (
          caller_function ? caller_function : "<unknown>", "() at ", file_name,
          ":", caller_line);
    }
  catch (...)
    {
      return "Caller info unavailable";
    }
}

// Macro wrapper to capture caller context portably at the call site
#if defined(__clang__) || defined(__GNUC__)
#define LUMEX_CAPTURE_CALLER_INFO()                                           \
  ::lumex::core::utility::debug::captureCallerInfoImpl (                      \
      __builtin_FUNCTION (), __builtin_FILE (), __builtin_LINE ())
#elif defined(_MSC_VER)
#define LUMEX_CAPTURE_CALLER_INFO()                                           \
  ::lumex::core::utility::debug::captureCallerInfoImpl (__FUNCSIG__,          \
                                                        __FILE__, __LINE__)
#else
#define LUMEX_CAPTURE_CALLER_INFO()                                           \
  ::lumex::core::utility::debug::captureCallerInfoImpl ("<unknown>",          \
                                                        "<unknown>", 0)
#endif
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-bounds-constant-array-index,
// cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
} // namespace debug
} // namespace utility
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_DEBUG_HPP

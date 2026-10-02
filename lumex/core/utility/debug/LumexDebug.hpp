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
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
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
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>

// Platform headers are gated on compiler macros so they can precede the
// project headers (the code below still branches on LUMEX_OS_*).
#if defined(_WIN32)
#include <Windows.h>

#include <DbgHelp.h>
#else
#include <unistd.h>
#if defined(__linux__) && __has_include(<link.h>)
#include <link.h>
#endif
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

#include "lumex/core/string/utility/LumexStringify.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

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

#if defined(LUMEX_OS_WINDOWS)
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
#if !defined(LUMEX_OS_WINDOWS) && (defined(__GNUC__) || defined(__clang__))
// Where an address lies: the file name of the module (executable or shared
// library) that holds it and the address inside that module's image. The
// offset is what offline tools take with that file (addr2line -e <module>,
// gdb "info line *<offset>"), so a stack can be symbolized later from the
// module and its separate debug file. No process is started and no debug
// information is read here.
struct module_address_t
{
  std::string module;        ///< File name of the module, no directories.
  std::uintptr_t offset = 0; ///< Address inside the module's image.
  bool found = false;        ///< False when no loaded module holds it.
};

// The file name of the running executable, read once: the loader reports
// the main program without a name.
static inline std::string const &
executable_file_name () LUMEX_NOEXCEPT
{
  static std::string const name = [] () -> std::string
    {
      try
        {
#if defined(LUMEX_OS_LINUX)
          std::array<char, 4096> buffer{};
          ssize_t const length = readlink ("/proc/self/exe", buffer.data (),
                                           buffer.size () - 1);
          if (length > 0)
            return std::string (buffer.data (),
                                static_cast<std::size_t> (length));
#endif
          return std::string ();
        }
      catch (...)
        {
          return std::string ();
        }
    }();
  return name;
}

static inline std::string
file_name_of (std::string const &path)
{
  std::size_t const last_slash = path.find_last_of ('/');
  return last_slash == std::string::npos ? path : path.substr (last_slash + 1);
}

#if defined(LUMEX_OS_LINUX) && __has_include(<link.h>)
struct module_search_t
{
  std::uintptr_t address;
  module_address_t *result;
};

// dl_iterate_phdr callback: the module whose loadable segment holds the
// address. dlpi_addr is the load bias, 0 for an executable linked at a fixed
// address, so address - bias is the address in the ELF file for executables
// (PIE or not) and shared libraries alike.
static inline int
find_module_callback (dl_phdr_info *info, std::size_t,
                      void *data) LUMEX_NOEXCEPT
{
  try
    {
      auto *search = static_cast<module_search_t *> (data);
      for (ElfW (Half) i = 0; i < info->dlpi_phnum; ++i)
        {
          ElfW (Phdr) const &segment = info->dlpi_phdr[i];
          if (segment.p_type != PT_LOAD)
            continue;
          std::uintptr_t const start
              = static_cast<std::uintptr_t> (info->dlpi_addr)
                + static_cast<std::uintptr_t> (segment.p_vaddr);
          if (search->address < start
              || search->address
                     >= start + static_cast<std::uintptr_t> (segment.p_memsz))
            continue;
          search->result->found = true;
          search->result->offset
              = search->address
                - static_cast<std::uintptr_t> (info->dlpi_addr);
          char const *name = info->dlpi_name;
          search->result->module = file_name_of (
              name != nullptr && name[0] != '\0' ? std::string (name)
                                                 : executable_file_name ());
          return 1;
        }
      return 0;
    }
  catch (...)
    {
      return 1;
    }
}
#endif

// The module that holds address and the address inside it. Linux walks the
// loaded modules (dl_iterate_phdr); other systems fall back to dladdr and the
// module's base address, which is right for position-independent images.
static inline module_address_t
find_module_address (void const *address) LUMEX_NOEXCEPT
{
  module_address_t result;
  try
    {
#if defined(LUMEX_OS_LINUX) && __has_include(<link.h>)
      module_search_t search{ reinterpret_cast<std::uintptr_t> (address),
                              &result };
      dl_iterate_phdr (&find_module_callback, &search);
#elif __has_include(<dlfcn.h>)
      Dl_info info;
      if (dladdr (address, &info) != 0 && info.dli_fname != nullptr)
        {
          result.found = true;
          result.offset = reinterpret_cast<std::uintptr_t> (address)
                          - reinterpret_cast<std::uintptr_t> (info.dli_fbase);
          result.module = file_name_of (info.dli_fname);
        }
#endif
    }
  catch (...)
    {
      result = module_address_t ();
    }
  return result;
}

// "<module>+0x<offset>", or an empty string when no loaded module holds the
// address.
static inline std::string
describe_module_address (void const *address)
{
  module_address_t const where = find_module_address (address);
  if (!where.found)
    return std::string ();
  return lumex::core::string::utility::stringify (where.module, "+0x",
                                                  formatHex (where.offset));
}

// One frame of captureStackTrace: "  #N: <symbol> +<offset>
// (<module>+0x<offset in module>) [0x<address>]" for an exported symbol, " #N:
// <module>+0x<offset in module> [0x<address>]" otherwise.
static inline std::string
format_frame (int index, void *address)
{
  std::string const hex
      = formatHex (reinterpret_cast<std::uintptr_t> (address));
  std::string const location = describe_module_address (address);
#if __has_include(<dlfcn.h>)
  Dl_info info;
  if (dladdr (address, &info) != 0 && info.dli_sname != nullptr)
    {
      std::ptrdiff_t const offset = static_cast<char *> (address)
                                    - static_cast<char *> (info.dli_saddr);
      return lumex::core::string::utility::stringify (
          "  #", index, ": ", demangleSymbol (info.dli_sname), " +", offset,
          location.empty () ? std::string () : " (" + location + ")", " [0x",
          hex, "]\n");
    }
#endif
  if (!location.empty ())
    return lumex::core::string::utility::stringify (
        "  #", index, ": ", location, " [0x", hex, "]\n");
  return lumex::core::string::utility::stringify ("  #", index, ": [0x", hex,
                                                  "]\n");
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
 * - Uses GNU backtrace() (not POSIX, but widely available) for the
 * addresses
 * - Names a frame by its exported symbol (dladdr, demangled through
 * __cxa_demangle); a function the module does not export (most of an
 * executable not linked with -rdynamic) has no name here
 * - Every frame also carries its module and the address inside it
 * ("libfoo.so.1+0x1A2B", "app+0x401234"), found through dl_iterate_phdr on
 * Linux: the address addr2line, gdb or eu-addr2line take with that module or
 * its separate debug file, so the stack is symbolized offline, with source
 * lines
 * - Starts no process and reads no debug information
 *
 * Performance:
 * - Windows: DbgHelp resolves symbols and lines in-process
 * - POSIX: a few microseconds per frame, whatever debug information the
 * modules have (the frame needs only dladdr and the loaded module list)
 *
 * Graceful Degradation:
 * - Without a module for an address: returns the address in hex format
 * - On errors: returns "Stack trace unavailable: <reason>"
 * - Thread-safe in all modes
 *
 * @param skip_frames Number of frames to skip (default 1 - itself
 * captureStackTrace)
 * @param max_frames Maximum number of frames to capture (default 16)
 *
 * @return std::string Formatted stack trace, each frame on a new line.
 *         Windows: "  #N: function_name (file:line) [0xADDRESS]".
 *         POSIX: "  #N: symbol +offset (module+0xOFFSET) [0xADDRESS]" for
 *         an exported symbol, "  #N: module+0xOFFSET [0xADDRESS]" otherwise.
 *
 * @note
 * - Function noexcept - never throws exceptions
 * - Can allocate memory inside (NOT async-signal-safe!)
 * - Optimized for rare calls (logging errors)
 * - DO NOT use in signal handlers or hot paths
 *
 * @warning
 * - Windows: Requires DbgHelp.lib in linking
 * - POSIX: names only exported symbols; link with -rdynamic to export more,
 * or symbolize the module offsets offline
 * - Inline functions may be missing in trace at aggressive optimization
 *
 * @example
 * @code
 * void resetConfig() {
 *   std::string trace = lumex::core::utility::debug::captureStackTrace(1, 5);
 *   std::cerr << "Config reset. Call stack:\n" << trace;
 *   // Windows:
 *   //   #0: loadConfig (config.cpp:140) [0x7FF6A2B41234]
 *   // Linux (offsets for addr2line or gdb with app and libc.so.6):
 *   //   #0: app+0x4F7250 [0x8F7250]
 *   //   #1: __libc_start_main +235 (libc.so.6+0x2409B) [0x7F3E1E8E009B]
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
#if defined(LUMEX_OS_WINDOWS)
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
            result += lumex::core::string::utility::stringify (
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
                  result += lumex::core::string::utility::stringify (
                      "  #", i, ": ", symbol_buffer->Name, " (", line.FileName,
                      ":", line.LineNumber, ") [0x",
                      Detail::formatHex (address), "]\n");
                }
              else
                {
                  // Only function name + address (no line info)
                  result += lumex::core::string::utility::stringify (
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
                  result += lumex::core::string::utility::stringify (
                      "  #", i, ": ", moduleInfo.ModuleName, "+0x",
                      Detail::formatHex (offset), " [0x",
                      Detail::formatHex (address), "]\n");
                }
              else
                {
                  result += lumex::core::string::utility::stringify (
                      "  #", i, ": [0x", Detail::formatHex (address), "]\n");
                }
            }
        }

      free (
          symbol_buffer); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

#elif defined(__GNUC__) || defined(__clang__)
      // POSIX: backtrace () for the addresses, dladdr () for the exported
      // symbol names, the module and the address inside it for the rest.
      // No process is started and no debug information is read: a frame
      // costs microseconds, and the module offsets are symbolized offline
      // from the module's debug file.
      // NOTE: backtrace () is a GNU extension, not in POSIX.1-2024, but
      // widely available.

#if __has_include(<execinfo.h>)
      void *addresses[kMaxStackFrames];

      int frame_count = backtrace (addresses, kMaxStackFrames);

      if (frame_count <= skip_frames)
        return "  Stack trace empty (insufficient frames)\n";

      int actual_frames = std::min (frame_count - skip_frames, max_frames);

      for (int i = 0; i < actual_frames; ++i)
        result += Detail::format_frame (i, addresses[i + skip_frames]);

#else
      // No backtrace support - return basic info
      result = "  Stack trace unavailable (<execinfo.h> not available)\n";
      result += lumex::core::string::utility::stringify (
          "  Current function: ", LUMEX_FUNCTION_NAME, "\n");
#endif

#else
      // Unknown platform
      result = "  Stack trace not supported on this platform\n";
      result += lumex::core::string::utility::stringify (
          "  Current function: ", LUMEX_FUNCTION_NAME, "\n");
#endif
    }
  catch (std::exception const &exc)
    {
      return lumex::core::string::utility::stringify (
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

      return lumex::core::string::utility::stringify (
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

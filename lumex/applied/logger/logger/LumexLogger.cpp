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
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
#include <nlohmann/json.hpp>
#endif

// Platform headers are gated on compiler macros so they can precede the
// project headers (LOGGER_OS_WINDOWS comes from LumexLogger.hpp).
#if defined(_WIN32)
#include <windows.h>
#ifdef _MSC_VER
#include <dbghelp.h>
#endif
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
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
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__linux__)
#include <linux/limits.h>
#endif
#endif

#include "LumexLogger.hpp"
#include "lumex/applied/logger/config/LumexLoggerConfigFormat.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/util/LumexUtf8FileName.hpp"

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
#include "lumex/applied/settings/ini/LumexSettingsINI.hpp"
#endif
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_XML)
#include "lumex/xml/LumexXml"
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

// Bring the namespaced logger symbols into this translation unit unqualified.
// This is a
// `.cpp`-local convenience only (never leaks into consumers of
// LumexLogger.hpp) and mirrors how the rest of this file was ported
// line-for-line from DChannel's Logger.cpp.

namespace lumex
{
namespace applied
{
namespace logger
{
namespace logger
{

namespace
{
// ============= Utility Functions =============

// Every path of the logger is UTF-8, as everywhere in the library. On Windows
// a narrow name is read in the ANSI code page, so a path with Cyrillic (a user
// profile) reached another file or none: the streams, the Win32 calls and the
// removal below take the UTF-16 form of the path, and a path that is not valid
// UTF-8 is refused (the skipped bytes would name another file). The logger
// does not depend on lumex::filesystem; the header-only helper of
// lumex::utility does the conversion.

/**
 * @brief Opens a file stream by a UTF-8 path.
 * @return true when the stream is open.
 */
template <typename Stream>
bool
open_file_stream (Stream &stream, std::string const &path,
                  std::ios_base::openmode mode)
{
  return ::lumex::core::utility::utf8_file_name::open (stream, path, mode);
}

/**
 * @brief Removes a file by a UTF-8 path (the result of std::remove: 0 on
 * success).
 */
int
remove_file (std::string const &path) LUMEX_NOEXCEPT
{
#ifdef LOGGER_OS_WINDOWS
  std::wstring widePath;
  if (!::lumex::core::utility::utf8_file_name::to_wide (path, widePath))
    return -1;
  return DeleteFileW (widePath.c_str ()) != 0 ? 0 : -1;
#else
  return std::remove (path.c_str ());
#endif
}

/**
 * @brief Converts an integral value (an address) to a 0xHH string.
 */
template <typename T>
typename std::enable_if<std::is_integral<T>::value, std::string>::type
format_hex (T value) LUMEX_NOEXCEPT
{
  std::ostringstream oss;
  oss << "0x" << std::hex << std::uppercase << value;
  return oss.str ();
}

/**
 * @brief Returns the file size in bytes.
 */
int64_t
get_file_size (std::string const &filePath) LUMEX_NOEXCEPT
{
  try
    {
      if (filePath.empty ())
        return -1;

#ifdef LOGGER_OS_WINDOWS
      std::wstring widePath;
      if (!::lumex::core::utility::utf8_file_name::to_wide (filePath,
                                                            widePath))
        return -1;
      WIN32_FILE_ATTRIBUTE_DATA fileData{};
      if (GetFileAttributesExW (widePath.c_str (), GetFileExInfoStandard,
                                &fileData)
          == 0)
        return -1;

      if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        return -1;

      LARGE_INTEGER fileSize{};
      fileSize.LowPart = fileData.nFileSizeLow;
      fileSize.HighPart = static_cast<LONG> (fileData.nFileSizeHigh);
      return static_cast<int64_t> (fileSize.QuadPart);
#else
      struct stat fileStat{};
      if (stat (filePath.c_str (), &fileStat) != 0)
        return -1;

      if (!S_ISREG (fileStat.st_mode))
        return -1;

      return static_cast<int64_t> (fileStat.st_size);
#endif
    }
  catch (...)
    {
      return -1;
    }
}

/**
 * @brief Returns free disk space in bytes.
 */
int64_t
get_free_disk_space (std::string const &path) LUMEX_NOEXCEPT
{
  try
    {
      if (path.empty ())
        return -1;

#ifdef LOGGER_OS_WINDOWS
      ULARGE_INTEGER freeBytesAvailable{};
      ULARGE_INTEGER totalNumberOfBytes{};
      ULARGE_INTEGER totalNumberOfFreeBytes{};

      std::wstring widePath;
      if (!::lumex::core::utility::utf8_file_name::to_wide (path, widePath))
        return -1;

      // GetDiskFreeSpaceExW takes a directory and fails for a file name
      // (ERROR_DIRECTORY): the callers pass the log file, so ask for the
      // directory it is in.
      DWORD const attributes = GetFileAttributesW (widePath.c_str ());
      if (attributes != INVALID_FILE_ATTRIBUTES
          && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
          std::size_t const separator = widePath.find_last_of (L"\\/");
          widePath = separator == std::wstring::npos
                         ? std::wstring (L".")
                         : widePath.substr (0, separator + 1);
        }

      if (GetDiskFreeSpaceExW (widePath.c_str (), &freeBytesAvailable,
                               &totalNumberOfBytes, &totalNumberOfFreeBytes))
        return static_cast<int64_t> (freeBytesAvailable.QuadPart);
      return -1;
#else
      struct statvfs vfs{};
      if (statvfs (path.c_str (), &vfs) == 0)
        {
          int64_t freeSpace = static_cast<int64_t> (vfs.f_bavail)
                              * static_cast<int64_t> (vfs.f_frsize);
          return freeSpace;
        }
      else
        {
          return -1;
        }
#endif
    }
  catch (...)
    {
      return -1;
    }
}

/**
 * @brief Returns the total size of all files in a directory, in bytes.
 */
int64_t
get_directory_size (std::string const &directoryPath) LUMEX_NOEXCEPT
{
  try
    {
      if (directoryPath.empty ())
        return -1;

      int64_t totalSize = 0;

#ifdef LOGGER_OS_WINDOWS
      std::wstring wideDirectory;
      if (!::lumex::core::utility::utf8_file_name::to_wide (directoryPath,
                                                            wideDirectory))
        return -1;
      std::wstring const searchPath = wideDirectory + L"\\*";

      WIN32_FIND_DATAW findData{};
      HANDLE hFind = FindFirstFileW (searchPath.c_str (), &findData);

      if (hFind == INVALID_HANDLE_VALUE)
        return -1;

      do
        {
          if (wcscmp (findData.cFileName, L".") == 0
              || wcscmp (findData.cFileName, L"..") == 0)
            continue;

          std::string fullPath
              = directoryPath + "\\"
                + ::lumex::core::utility::utf8_file_name::to_utf8 (
                    findData.cFileName);

          if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
              int64_t subDirSize = get_directory_size (fullPath);
              if (subDirSize < 0)
                return -1;
              totalSize += subDirSize;
            }
          else
            {
              LARGE_INTEGER fileSize{};
              fileSize.LowPart = findData.nFileSizeLow;
              fileSize.HighPart = static_cast<LONG> (findData.nFileSizeHigh);
              totalSize += static_cast<int64_t> (fileSize.QuadPart);
            }
        }
      while (FindNextFileW (hFind, &findData) != 0);

      FindClose (hFind);

#else
      DIR *dir = opendir (directoryPath.c_str ());
      if (dir == nullptr)
        return -1;

      struct dirent *entry;
      while ((entry = readdir (dir)) != nullptr)
        {
          if (strcmp (entry->d_name, ".") == 0
              || strcmp (entry->d_name, "..") == 0)
            continue;

          std::string fullPath = directoryPath + "/" + entry->d_name;

          struct stat entryStat{};
          if (stat (fullPath.c_str (), &entryStat) != 0)
            {
              closedir (dir);
              return -1;
            }

          if (S_ISDIR (entryStat.st_mode))
            {
              int64_t subDirSize = get_directory_size (fullPath);
              if (subDirSize < 0)
                {
                  closedir (dir);
                  return -1;
                }
              totalSize += subDirSize;
            }
          else if (S_ISREG (entryStat.st_mode))
            {
              totalSize += static_cast<int64_t> (entryStat.st_size);
            }
        }

      closedir (dir);
#endif

      return totalSize;
    }
  catch (...)
    {
      return -1;
    }
}

/**
 * @brief Captures the current call stack as a formatted string.
 */
std::string
capture_stack_trace (int skip_frames = 1, int max_frames = 16) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR int kMaxStackFrames = 64;
  // A platform without a stack walker (MinGW) uses none of these.
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (skip_frames);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (max_frames);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (kMaxStackFrames);
  std::string result;

  try
    {
#if defined(LOGGER_OS_WINDOWS) && defined(_MSC_VER)
      void *stack[kMaxStackFrames];
      HANDLE process = GetCurrentProcess ();

      static std::atomic_bool initialized{ false };
      static std::once_flag init_flag;
      static std::mutex dbghelp_mutex;

      if (!initialized.load (std::memory_order_acquire))
        {
          std::call_once (
              init_flag,
              [process] ()
                {
                  std::lock_guard<std::mutex> lock (dbghelp_mutex);
                  if (SymInitialize (process, nullptr, TRUE))
                    {
                      SymSetOptions (SYMOPT_LOAD_LINES | SYMOPT_UNDNAME
                                     | SYMOPT_DEFERRED_LOADS);
                      initialized.store (true, std::memory_order_release);
                    }
                });
        }

      if (!initialized.load (std::memory_order_acquire))
        {
          WORD frames = CaptureStackBackTrace (
              static_cast<DWORD> (skip_frames),
              static_cast<DWORD> (max_frames), stack, nullptr);
          for (WORD i = 0; i < frames; ++i)
            result += logger_stringify (
                "  #", i, ": [",
                format_hex (reinterpret_cast<uintptr_t> (stack[i])), "]\n");
          return result.empty ()
                     ? "  Stack trace unavailable (SymInitialize failed)\n"
                     : result;
        }

      WORD frames = CaptureStackBackTrace (static_cast<DWORD> (skip_frames),
                                           static_cast<DWORD> (max_frames),
                                           stack, nullptr);

      if (frames == 0)
        return "  Stack trace empty (no frames captured)\n";

      LUMEX_CONSTEXPR std::size_t kSymbolBufferSize
          = sizeof (SYMBOL_INFO) + (MAX_SYM_NAME * sizeof (TCHAR));
      auto *symbol_buffer
          = static_cast<SYMBOL_INFO *> (malloc (kSymbolBufferSize));

      if (symbol_buffer == nullptr)
        return "  Stack trace unavailable (memory allocation failed)\n";

      symbol_buffer->MaxNameLen = MAX_SYM_NAME;
      symbol_buffer->SizeOfStruct = sizeof (SYMBOL_INFO);

      std::lock_guard<std::mutex> lock (dbghelp_mutex);

      for (WORD i = 0; i < frames; ++i)
        {
          DWORD64 address = reinterpret_cast<DWORD64> (stack[i]);

          DWORD64 displacement = 0;
          if (SymFromAddr (process, address, &displacement, symbol_buffer)
              != 0)
            {
              IMAGEHLP_LINE64 line{};
              line.SizeOfStruct = sizeof (IMAGEHLP_LINE64);
              DWORD line_displacement = 0;

              if (SymGetLineFromAddr64 (process, address, &line_displacement,
                                        &line)
                  != 0)
                {
                  result += logger_stringify (
                      "  #", i, ": ", symbol_buffer->Name, " (", line.FileName,
                      ":", line.LineNumber, ") [", format_hex (address),
                      "]\n");
                }
              else
                {
                  result
                      += logger_stringify ("  #", i, ": ", symbol_buffer->Name,
                                           " [", format_hex (address), "]\n");
                }
            }
          else
            {
              result += logger_stringify ("  #", i, ": [",
                                          format_hex (address), "]\n");
            }
        }

      free (symbol_buffer);

#elif defined(__GNUC__) || defined(__clang__)
#if __has_include(<execinfo.h>)
      void *addresses[kMaxStackFrames];

      int frame_count = backtrace (addresses, kMaxStackFrames);

      if (frame_count <= skip_frames)
        return "  Stack trace empty (insufficient frames)\n";

      int actual_frames = std::min (frame_count - skip_frames, max_frames);

      char **symbols
          = backtrace_symbols (addresses + skip_frames, actual_frames);

      if (!symbols)
        {
          for (int i = 0; i < actual_frames; ++i)
            {
              Dl_info info;
              if (dladdr (addresses[i + skip_frames], &info))
                {
                  std::string demangled
                      = info.dli_sname ? info.dli_sname : "??";
#if __has_include(<cxxabi.h>)
                  if (info.dli_sname)
                    {
                      int status = -1;
                      char *demangled_ = abi::__cxa_demangle (
                          info.dli_sname, nullptr, nullptr, &status);
                      if (status == 0 && demangled_)
                        {
                          demangled = demangled_;
                          free (demangled_);
                        }
                    }
#endif
                  result += logger_stringify (
                      "  #", i, ": ", demangled, " [",
                      format_hex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
              else
                {
                  result += logger_stringify (
                      "  #", i, ": [",
                      format_hex (reinterpret_cast<uintptr_t> (
                          addresses[i + skip_frames])),
                      "]\n");
                }
            }
          return result;
        }

      for (int i = 0; i < actual_frames; ++i)
        {
          Dl_info info;
          bool has_symbol
              = dladdr (addresses[i + skip_frames], &info) && info.dli_sname;
          if (has_symbol)
            {
              std::string demangled = info.dli_sname;
#if __has_include(<cxxabi.h>)
              int status = -1;
              char *demangled_ = abi::__cxa_demangle (info.dli_sname, nullptr,
                                                      nullptr, &status);
              if (status == 0 && demangled_)
                {
                  demangled = demangled_;
                  free (demangled_);
                }
#endif

              ptrdiff_t offset
                  = reinterpret_cast<char *> (addresses[i + skip_frames])
                    - reinterpret_cast<char *> (info.dli_saddr);

              result += logger_stringify (
                  "  #", i, ": ", demangled, " +", offset, " [",
                  format_hex (reinterpret_cast<uintptr_t> (
                      addresses[i + skip_frames])),
                  "]\n");
            }
          else
            {
              result += logger_stringify (
                  "  #", i, ": ", symbols[i], " [",
                  format_hex (reinterpret_cast<uintptr_t> (
                      addresses[i + skip_frames])),
                  "]\n");
            }
        }

      free (symbols);
#else
      result = "  Stack trace unavailable (<execinfo.h> not available)\n";
      result += logger_stringify ("  Current function: ", LOGGER_FUNCTION_NAME,
                                  "\n");
#endif

#else
      result = "  Stack trace not supported on this platform\n";
      result += logger_stringify ("  Current function: ", LOGGER_FUNCTION_NAME,
                                  "\n");
#endif
    }
  catch (std::exception const &exc)
    {
      return logger_stringify (
          "  Stack trace unavailable (exception: ", exc.what (), ")\n");
    }
  catch (...)
    {
      return "  Stack trace unavailable (unknown exception)\n";
    }

  return result.empty () ? "  Stack trace empty\n" : result;
}

/**
 * @brief Converts a string to uppercase and strips spaces.
 * @param str Source string.
 * @return Normalized uppercase string without spaces.
 * @note Does not throw.
 */
std::string
_normalize_string (std::string const &str) LUMEX_NOEXCEPT
{
  try
    {
      std::string result = str;

      // Convert to uppercase
#if LUMEX_HAS_STD_RANGES
      std::ranges::transform (
          result, result.begin (), [] (unsigned char ch)
            { return static_cast<char> (::toupper (ch)); });
#else
      std::transform (result.begin (), result.end (), result.begin (),
                      [] (unsigned char ch)
                        { return static_cast<char> (::toupper (ch)); });
#endif

      // Strip spaces
#if LUMEX_HAS_STD_RANGES
      auto iter = std::ranges::remove_if (result, ::isspace);
      result.erase (iter.begin (), result.end ());
#else
      result.erase (std::remove_if (result.begin (), result.end (), ::isspace),
                    result.end ());
#endif

      return result;
    }
  catch (...)
    {
      // On any error, return the original string
      return str;
    }
}

/**
 * @brief Creates the directory if it does not exist.
 * @param dirPath Directory path to create.
 * @return true if the directory was created or already exists, false
 * otherwise.
 * @note Does not throw.
 */
bool
_create_directory_if_not_exists (std::string const &dirPath) LUMEX_NOEXCEPT
{
  try
    {
      if (dirPath.empty ())
        return false;

#if defined(_WIN32) || defined(_WIN64)
      // Windows: use CreateDirectoryW (the path is UTF-8)
      std::wstring wideDirPath;
      if (!::lumex::core::utility::utf8_file_name::to_wide (dirPath,
                                                            wideDirPath))
        return false;
      BOOL result = CreateDirectoryW (wideDirPath.c_str (), nullptr);
      if (result != 0)
        {
          // Directory created successfully
          return true;
        }

      // Check why it failed
      DWORD error = GetLastError ();

      if (error == ERROR_ALREADY_EXISTS)
        {
          // Directory already exists; verify it really is a
          // directory
          DWORD attributes = GetFileAttributesW (wideDirPath.c_str ());
          bool isDir = (attributes != INVALID_FILE_ATTRIBUTES
                        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

          if (isDir)
            {
              // Check directory access by trying to create
              // a test file
              std::string testFile = dirPath + "\\test_write_access.tmp";
              std::ofstream testStream;
              if (open_file_stream (testStream, testFile, std::ios::out))
                {
                  testStream.close ();
                  remove_file (testFile);
                  return true;
                }

              return false;
            }

          return isDir;
        }

      // Other errors - failed to create the directory
      return false;
#else
      // Unix-like systems: use mkdir
      struct stat st{};
      if (stat (dirPath.c_str (), &st) == 0)
        {
          // Directory already exists
          return S_ISDIR (st.st_mode);
        }

      // Create the directory with mode 0755
      return mkdir (dirPath.c_str (), 0755) == 0;
#endif
    }
  catch (...)
    {
      // On any error, return false
      return false;
    }
}

/**
 * @brief Parses a prefixed configuration line "PREFIX=value".
 * @param line Line to parse.
 * @param prefix Prefix to search for (for example, "LEVEL").
 * @return Value after the equals sign, or an empty string if the prefix is not
 * found.
 * @note Does not throw.
 */
std::string
_parse_prefixed_value (std::string const &line,
                       std::string const &prefix) LUMEX_NOEXCEPT
{
  try
    {
      if (line.empty () || prefix.empty ())
        return "";

      // Look for the prefix at the start of the line (case-insensitive)
      std::string normalizedLine = _normalize_string (line);
      std::string normalizedPrefix = _normalize_string (prefix);

      if (normalizedLine.length () <= normalizedPrefix.length ())
        return "";

      // Check whether the line starts with the prefix
      if (normalizedLine.substr (0, normalizedPrefix.length ())
          != normalizedPrefix)
        return "";

      // Check whether an equals sign follows the prefix
      if (normalizedLine[normalizedPrefix.length ()] != '=')
        return "";

      // Extract the value after the equals sign
      std::string value = line.substr (prefix.length () + 1);

      // Strip leading and trailing spaces from the value
      value.erase (0, value.find_first_not_of (" \t\r\n"));
      value.erase (value.find_last_not_of (" \t\r\n") + 1);

      return value;
    }
  catch (...)
    {
      return "";
    }
}

/**
 * @brief Checks whether the line is a prefixed string of the form
 * "PREFIX=...".
 * @param line Line to check.
 * @param prefix Prefix to search for.
 * @return true if the line has the form "PREFIX=...", even if the value
 * after '=' is empty.
 * @note Does not throw.
 * @note The previous _parse_prefixed_value implementation returned false for
 * "PRESET=" (empty value), so the line fell through to the fallback
 * old format and overwrote log_level with LEVEL_INFO.
 */
bool
_has_prefix (std::string const &line, std::string const &prefix) LUMEX_NOEXCEPT
{
  try
    {
      if (line.empty () || prefix.empty ())
        return false;

      std::string normalizedLine = _normalize_string (line);
      std::string normalizedPrefix = _normalize_string (prefix);

      if (normalizedLine.length () < normalizedPrefix.length () + 1)
        return false;
      if (normalizedLine.substr (0, normalizedPrefix.length ())
          != normalizedPrefix)
        return false;
      return normalizedLine[normalizedPrefix.length ()] == '=';
    }
  catch (...)
    {
      return false;
    }
}

/**
 * @brief Returns the helper-file contents with the developer hint.
 * @return Hint text (bilingual: Russian and English).
 */
std::string
get_hint_file_content ()
{
  std::stringstream content;

  content << "\n";
  content << "=============================================================\n";
  content << "  Universal Logger - Подсказка для разработчиков / Hint for "
             "Developers\n";
  content << "=============================================================\n";
  content << "\n";

  // ========== RUSSIAN SECTION ==========
  content << "[RU] ВАЖНО: Создайте файл 'enable_logs' рядом с исполняемым "
             "файлом\n";
  content << "[RU]        (.exe/.so/.dll/.dylib) для включения логирования.\n";
  content << "\n";
  content
      << "[RU] Параметры конфигурации (case-insensitive, порядок не важен):\n";
  content << "\n";
  content << "[RU]   LEVEL=<уровень>          - Уровень логирования\n";
  content << "[RU]                            Значения: trace, debug, info, "
             "warning, error, fatal\n";
  content << "\n";
  content
      << "[RU]   FUNCNAME=<режим>        - Режим отображения имени функции\n";
  content << "[RU]                            Значения: none, short, normal, "
             "full (signature = full)\n";
  content
      << "[RU]                            - none: не показывать имя функции\n";
  content
      << "[RU]                            - short: короткое имя (__func__)\n";
  content << "[RU]                            - normal: "
             "ClassName::methodName() без типа возврата и аргументов\n";
  content << "[RU]                            - full: полная сигнатура "
             "(__FUNCSIG__/__PRETTY_FUNCTION__)\n";
  content << "\n";
  content
      << "[RU]   TIMESTAMPED=<значение>   - Использовать timestamped логи\n";
  content << "[RU]                            Значения: true/yes/1 или "
             "false/no/0\n";
  content << "\n";
  content << "[RU]   STACKTRACE=<значение>    - Показывать stacktrace (по "
             "умолчанию true для WARNING/ERROR)\n";
  content << "[RU]                            Значения: true/yes/1 или "
             "false/no/0\n";
  content << "[RU]                            ВАЖНО: Если STACKTRACE=true, то "
             "stacktrace записывается\n";
  content << "[RU]                            под абсолютно каждую функцию "
             "для ВСЕХ уровней логирования!\n";
  content << "\n";
  content << "[RU]   STACKTRACE_FRAMES=<num> - Максимальное количество "
             "фреймов в stacktrace\n";
  content << "[RU]                            Значение: число от 1 до 64 (по "
             "умолчанию 16)\n";
  content << "\n";
  content << "[RU]   HINT=<значение>          - Создавать ли файл-хелпер с "
             "подсказкой\n";
  content << "[RU]                            Значения: true/yes/1 "
             "(создавать) или false/no/0 (не создавать)\n";
  content << "\n";
  content
      << "[RU]   PRESET=<компоненты>      - Фильтрация логов по компонентам\n";
  content << "[RU]                            Список через запятую. "
             "Совпадение: короткое имя (WriteData),\n";
  content << "[RU]                            нормальное (Class::method), "
             "полная сигнатура или точное имя\n";
  content << "\n";
  content << "[RU]   BUFFERING=<значение>     - Включить/отключить "
             "буферизацию логов\n";
  content << "[RU]                            Значения: true/yes/1 (включить) "
             "или false/no/0 (отключить, по умолчанию)\n";
  content << "[RU]                            При включенной буферизации логи "
             "сохраняются в буфере\n";
  content << "[RU]                            и записываются при "
             "WARNING/ERROR/FATAL или при вызове flush()\n";
  content << "\n";
  content << "[RU]   DESCRIBE_FRAME=<значение> - Включить древовидный разбор "
             "фреймов пакетов в логах\n";
  content << "[RU]                            Значения: true/yes/1 (включить) "
             "или false/no/0 (отключить, по умолчанию)\n";
  content << "[RU]                            При включении после вывода "
             "пакета выводится дерево структуры\n";
  content << "[RU]                            (Header, Payload, CRC, флаги по "
             "битам и т.д.) для каждого протокола\n";
  content << "\n";
  content << "[RU]   BUFFER_SIZE=<число>      - Размер буфера логов "
             "(количество записей)\n";
  content << "[RU]                            Значение: положительное число "
             "(по умолчанию 100)\n";
  content << "[RU]                            Используется только при "
             "BUFFERING=true\n";
  content << "\n";
  content << "[RU]   BUFFERING_TRIGGER=<уровень> - Минимальный уровень для "
             "сброса буфера в файл\n";
  content << "[RU]                            Значения: TRACE, DEBUG, INFO, "
             "SUCCESS, WARNING, ERROR, FATAL "
             "(case-insensitive)\n";
  content << "[RU]                            При задании сброс только при "
             "этом уровне и выше; при выходе буфер не "
             "сбрасывается\n";
  content << "\n";
  content << "[RU] Примеры конфигурации:\n";
  content << "\n";
  content << "[RU] 1. Минимальная конфигурация (только уровень):\n";
  content << "   LEVEL=info\n";
  content << "\n";
  content << "[RU] 2. Полная конфигурация:\n";
  content << "   LEVEL=debug\n";
  content << "   FUNCNAME=short\n";
  content << "   TIMESTAMPED=true\n";
  content << "   STACKTRACE=true\n";
  content << "   STACKTRACE_FRAMES=10\n";
  content << "\n";
  content << "[RU] 3. Пример с произвольным порядком строк:\n";
  content << "   TIMESTAMPED=true\n";
  content << "   FUNCNAME=full\n";
  content << "   LEVEL=warning\n";
  content << "   STACKTRACE=false\n";
  content << "\n";
  content << "[RU] 4. Отключение имен функций:\n";
  content << "   LEVEL=trace\n";
  content << "   FUNCNAME=none\n";
  content << "\n";
  content << "[RU] 5. Отключение файла-хелпера:\n";
  content << "   LEVEL=info\n";
  content << "   HINT=false\n";
  content << "\n";
  content << "[RU] 6. Включение буферизации с настраиваемым размером:\n";
  content << "   LEVEL=debug\n";
  content << "   BUFFERING=true\n";
  content << "   BUFFER_SIZE=200\n";
  content << "\n";
  content << "[RU] 7. Пресет компонентов с буферизацией:\n";
  content << "   LEVEL=info\n";
  content << "   PRESET=NetworkManager,DatabaseConnection\n";
  content << "   BUFFERING=true\n";
  content << "   BUFFER_SIZE=150\n";
  content << "\n";
  content << "[RU] ПРИМЕЧАНИЕ: Все параметры case-insensitive (регистр не "
             "важен).\n";
  content << "[RU]            Можно писать: LEVEL=INFO, level=info, "
             "LeVeL=InFo - все равно работает.\n";
  content << "[RU]            Порядок строк в файле не важен.\n";
  content << "\n\n\n";

  // ========== ENGLISH SECTION ==========
  content
      << "[EN] IMPORTANT: Create 'enable_logs' file next to the executable\n";
  content << "[EN]            (.exe/.so/.dll/.dylib) to enable logging.\n";
  content << "\n";
  content << "[EN] Configuration parameters (case-insensitive, order doesn't "
             "matter):\n";
  content << "\n";
  content << "[EN]   LEVEL=<level>            - Logging level\n";
  content << "[EN]                            Values: trace, debug, info, "
             "warning, error, fatal\n";
  content << "\n";
  content << "[EN]   FUNCNAME=<mode>         - Function name display mode\n";
  content << "[EN]                            Values: none, short, normal, "
             "full (signature = full)\n";
  content
      << "[EN]                            - none: don't show function name\n";
  content
      << "[EN]                            - short: short name (__func__)\n";
  content << "[EN]                            - normal: "
             "ClassName::methodName() without return type/args\n";
  content << "[EN]                            - full: full signature "
             "(__FUNCSIG__/__PRETTY_FUNCTION__)\n";
  content << "\n";
  content << "[EN]   TIMESTAMPED=<value>      - Use timestamped logs\n";
  content
      << "[EN]                            Values: true/yes/1 or false/no/0\n";
  content << "\n";
  content << "[EN]   STACKTRACE=<value>       - Show stacktrace (default: "
             "true for WARNING/ERROR)\n";
  content
      << "[EN]                            Values: true/yes/1 or false/no/0\n";
  content << "[EN]                            IMPORTANT: If STACKTRACE=true, "
             "stacktrace is written\n";
  content << "[EN]                            under EVERY function for ALL "
             "log levels!\n";
  content << "\n";
  content << "[EN]   STACKTRACE_FRAMES=<num> - Maximum number of frames in "
             "stacktrace\n";
  content << "[EN]                            Value: number from 1 to 64 "
             "(default: 16)\n";
  content << "\n";
  content
      << "[EN]   HINT=<value>             - Create hint file for developers\n";
  content << "[EN]                            Values: true/yes/1 (create) or "
             "false/no/0 (don't create)\n";
  content << "\n";
  content << "[EN]   PRESET=<components>      - Filter logs by components\n";
  content << "[EN]                            Comma-separated. Match: short "
             "name (WriteData), normal (Class::method),\n";
  content << "[EN]                            full signature, or exact "
             "component name\n";
  content << "\n";
  content
      << "[EN]   BUFFERING=<value>        - Enable/disable log buffering\n";
  content << "[EN]                            Values: true/yes/1 (enable) or "
             "false/no/0 (disable, default)\n";
  content << "[EN]                            When enabled, logs are stored "
             "in buffer and written\n";
  content << "[EN]                            on WARNING/ERROR/FATAL or when "
             "flush() is called\n";
  content << "\n";
  content << "[EN]   DESCRIBE_FRAME=<value>   - Enable tree-like frame "
             "breakdown in packet logs\n";
  content << "[EN]                            Values: true/yes/1 (enable) or "
             "false/no/0 (disable, default)\n";
  content << "[EN]                            When enabled, after each packet "
             "a structure tree is logged\n";
  content << "[EN]                            (Header, Payload, CRC, flag "
             "bits, etc.) per protocol\n";
  content << "\n";
  content << "[EN]   BUFFER_SIZE=<number>     - Log buffer size (number of "
             "entries)\n";
  content << "[EN]                            Value: positive number "
             "(default: 100)\n";
  content << "[EN]                            Used only when BUFFERING=true\n";
  content << "\n";
  content << "[EN]   BUFFERING_TRIGGER=<level> - Minimum log level that "
             "triggers buffer flush to file\n";
  content << "[EN]                            Values: TRACE, DEBUG, INFO, "
             "SUCCESS, WARNING, ERROR, FATAL "
             "(case-insensitive)\n";
  content << "[EN]                            When set, buffer is flushed "
             "only on this level or higher; not on exit\n";
  content << "[EN]                            Preset does NOT affect flush: "
             "any message at trigger level flushes "
             "buffer\n";
  content << "\n";
  content << "[EN] Configuration examples:\n";
  content << "\n";
  content << "[EN] 1. Minimal configuration (level only):\n";
  content << "   LEVEL=info\n";
  content << "\n";
  content << "[EN] 2. Full configuration:\n";
  content << "   LEVEL=debug\n";
  content << "   FUNCNAME=short\n";
  content << "   TIMESTAMPED=true\n";
  content << "   STACKTRACE=true\n";
  content << "   STACKTRACE_FRAMES=10\n";
  content << "\n";
  content << "[EN] 3. Example with arbitrary line order:\n";
  content << "   TIMESTAMPED=true\n";
  content << "   FUNCNAME=full\n";
  content << "   LEVEL=warning\n";
  content << "   STACKTRACE=false\n";
  content << "\n";
  content << "[EN] 4. Disabling function names:\n";
  content << "   LEVEL=trace\n";
  content << "   FUNCNAME=none\n";
  content << "\n";
  content << "[EN] 5. Disabling hint file:\n";
  content << "   LEVEL=info\n";
  content << "   HINT=false\n";
  content << "\n";
  content << "[EN] 6. Enabling buffering with custom size:\n";
  content << "   LEVEL=debug\n";
  content << "   BUFFERING=true\n";
  content << "   BUFFER_SIZE=200\n";
  content << "\n";
  content << "[EN] 7. Component preset with buffering:\n";
  content << "   LEVEL=info\n";
  content << "   PRESET=NetworkManager,DatabaseConnection\n";
  content << "   BUFFERING=true\n";
  content << "   BUFFER_SIZE=150\n";
  content << "\n";
  content << "[EN] NOTE: All parameters are case-insensitive.\n";
  content << "[EN]            You can write: LEVEL=INFO, level=info, "
             "LeVeL=InFo - all work the same.\n";
  content << "[EN]            Line order in the file doesn't matter.\n";
  content << "\n";
  content << "=============================================================\n";
  content << "\n";

  return content.str ();
}

/**
 * @brief Prints a developer hint on how to use the logger.
 * @note Writes to stdout how to create the trigger file and example
 * configuration. Bilingual output: Russian and English.
 */
void
file_hint_about_logger ()
{
  std::cout << get_hint_file_content ();
}
} // anonymous namespaces for utility functions

// Ordinary definitions of the in-class constants (see LumexLogger.hpp): the
// library exports them in every standard.
std::size_t const logger_config_t::kBufferSize;
short const logger_config_t::kMaxStackTraceFrames;

namespace
{
/// The period of the lazy check of the trigger file: 2 s.
LUMEX_CONSTEXPR std::int64_t kTriggerCheckPeriodNs
    = static_cast<std::int64_t> (2000) * static_cast<std::int64_t> (1000000);

/// A modification time no file has: the trigger file was not read at start,
/// so the first check reads it again.
LUMEX_CONSTEXPR std::int64_t kUnreadTriggerTime
    = (std::numeric_limits<std::int64_t>::min) ();

/**
 * @brief The steady clock in nanoseconds, for the time of the next lazy
 * check.
 */
std::int64_t
steady_now_ns () LUMEX_NOEXCEPT
{
  return static_cast<std::int64_t> (
      std::chrono::duration_cast<std::chrono::nanoseconds> (
          std::chrono::steady_clock::now ().time_since_epoch ())
          .count ());
}

/**
 * @brief True in the thread that runs a check of the trigger file (it holds
 * reread_mutex): a level change callback that logs must not start the lazy
 * check, which would lock that mutex again.
 */
thread_local bool t_checking_trigger_file = false;

/**
 * @brief True in the thread that runs the last round of level change
 * callbacks for one change: a set_log_level () of a callback is ignored
 * there, so every callback last hears the final state.
 */
thread_local bool t_final_callback_round = false;

/**
 * @brief Marks the current thread as the one that checks.
 */
class checking_trigger_file_scope
{
public:
  checking_trigger_file_scope () LUMEX_NOEXCEPT
  {
    t_checking_trigger_file = true;
  }
  ~checking_trigger_file_scope () { t_checking_trigger_file = false; }
  checking_trigger_file_scope (checking_trigger_file_scope const &) = delete;
  checking_trigger_file_scope &operator= (checking_trigger_file_scope const &)
      = delete;
};

/**
 * @brief The name of a function-name mode in the log header.
 */
char const *
function_name_mode_name (FunctionNameMode mode) LUMEX_NOEXCEPT
{
  if (mode == FunctionNameMode::SHORT)
    return "SHORT";
  if (mode == FunctionNameMode::FULL)
    return "FULL";
  if (mode == FunctionNameMode::NORMAL)
    return "NORMAL";
  return "NONE";
}
} // namespace

LumexLogger::LumexLogger ()
    : current_log_level (LogLevel::LEVEL_INFO), logging_enabled (false),
      function_name_mode (FunctionNameMode::FULL),
      use_timestamped_logs (false), show_stack_trace (false),
      stack_trace_max_frames (logger_config_t::kMaxStackTraceFrames),
      hint_shown (false), trigger_file_name (kDefaultTriggerFileName),
      buffering_enabled (false), buffer_size (logger_config_t::kBufferSize),
      buffering_trigger_configured (false),
      buffering_trigger_level (LogLevel::LEVEL_WARNING),
      preset_enabled (false),
      next_trigger_check_ns (steady_now_ns () + kTriggerCheckPeriodNs),
      last_trigger_state (),
      last_file_settings (_file_settings_of (logger_config_t ())),
      reread_failure_reported (false), level_change_callbacks ()
{
  try
    {
      executable_directory = _get_executable_directory ();

      // The state the first check compares with, taken before the
      // configuration is read: a change in between is seen by that check.
      std::string const path = _trigger_file_path ();
      last_trigger_state = _stat_trigger_file (path);
      if (!last_trigger_state.present)
        {
          // Logging is disabled; do not create a log file
          return;
        }

      // Read configuration from the file (log level, function-name
      // mode, timestamped logs, stacktrace, preset). A file that cannot be
      // read starts the logger with the defaults, as before; the first check
      // reads it again.
      logger_config_t config;
      if (_try_read_config_from_path (path, config) != config_read_t::ok)
        last_trigger_state.mtime = kUnreadTriggerTime;
      _open_log_file (config);
      last_file_settings = _file_settings_of (config);
      logging_enabled.store (true);
    }
  catch (std::exception const &e)
    {
      // If the logger cannot be created, write the error to stderr
      std::cerr << "LumexLogger initialization error: " << e.what () << '\n';
      throw;
    }
}

LumexLogger::~LumexLogger ()
{
  try
    {
      std::lock_guard<std::mutex> lock (log_mutex);
      if (logging_enabled.load () && log_file.is_open ())
        _close_log_file ("=== End of the program ============");
    }
  catch (...)
    {
      std::cerr << "Warning: Unknown error during logger cleanup\n";
    }
}

LumexLogger::file_settings_t
LumexLogger::_file_settings_of (logger_config_t const &config)
{
  file_settings_t settings;
  settings.preset_components = config.preset_components;
  settings.buffer_size = config.buffer_size;
  settings.stack_trace_max_frames = config.stack_trace_max_frames;
  settings.log_level = config.log_level;
  settings.func_name_mode = config.func_name_mode;
  settings.buffering_trigger_level = config.buffering_trigger_level;
  settings.show_stack_trace = config.show_stack_trace;
  settings.buffering_enabled = config.buffering_enabled;
  settings.buffering_trigger_configured = config.buffering_trigger_configured;
  settings.describe_frame = config.describe_frame;
  return settings;
}

bool
LumexLogger::_apply_config (logger_config_t const &config,
                            file_settings_t const *previous)
{
  bool applied = false;
  if (previous == nullptr || config.log_level != previous->log_level)
    {
      current_log_level.store (config.log_level);
      applied = true;
    }
  if (previous == nullptr || config.func_name_mode != previous->func_name_mode)
    {
      function_name_mode.store (config.func_name_mode);
      applied = true;
    }
  if (previous == nullptr
      || config.show_stack_trace != previous->show_stack_trace)
    {
      show_stack_trace = config.show_stack_trace;
      applied = true;
    }
  if (previous == nullptr
      || config.stack_trace_max_frames != previous->stack_trace_max_frames)
    {
      stack_trace_max_frames = config.stack_trace_max_frames;
      applied = true;
    }

  // The preset of the file; a file without one switches it off
  if (previous == nullptr
      || config.preset_components != previous->preset_components)
    {
      preset_enabled = !config.preset_components.empty ();
      preset_components = config.preset_components;
      applied = true;
    }

  if (previous == nullptr || config.describe_frame != previous->describe_frame)
    {
      describe_frame_enabled.store (config.describe_frame);
      applied = true;
    }

  // Buffering as enable_buffering () and disable_buffering () set it (the
  // caller holds the lock they would take)
  if (previous == nullptr
      || config.buffering_enabled != previous->buffering_enabled
      || config.buffer_size != previous->buffer_size)
    {
      if (config.buffering_enabled)
        {
          buffering_enabled = true;
          buffer_size = (config.buffer_size > 0)
                            ? config.buffer_size
                            : logger_config_t::kBufferSize;
          if (log_buffer.size () > buffer_size)
            {
              std::size_t const elementsToRemove
                  = log_buffer.size () - buffer_size;
              log_buffer.erase (
                  log_buffer.begin (),
                  log_buffer.begin ()
                      + static_cast<ptrdiff_t> (elementsToRemove));
            }
        }
      else
        {
          if (!log_buffer.empty ())
            _flush_buffer ();
          log_buffer.clear ();
          buffering_enabled = false;
          buffer_size = logger_config_t::kBufferSize;
        }
      applied = true;
    }
  if (previous == nullptr
      || config.buffering_trigger_configured
             != previous->buffering_trigger_configured
      || config.buffering_trigger_level != previous->buffering_trigger_level)
    {
      buffering_trigger_configured = config.buffering_trigger_configured;
      buffering_trigger_level = config.buffering_trigger_level;
      applied = true;
    }
  return applied;
}

void
LumexLogger::_open_log_file (logger_config_t const &config)
{
  // The file first: when it cannot be opened, nothing changes
  std::string const path = _create_log_file_path (config.use_timestamped_logs);

  // Open the file for writing, overwriting existing contents
  if (!open_file_stream (log_file, path, std::ios::out | std::ios::trunc))
    {
      std::string errorMsg = "Cannot open log file: ";
      errorMsg.reserve (errorMsg.length () + path.length ());
      errorMsg += path;
      throw std::runtime_error (errorMsg);
    }
  log_file_path = path;
  use_timestamped_logs = config.use_timestamped_logs;
  (void)_apply_config (config, nullptr);

  // Helper-file management
  std::string hintFilePath = executable_directory;
  if (!hintFilePath.empty ()
      && hintFilePath[hintFilePath.length () - 1] != LOGGER_FILE_SEPARATOR)
    hintFilePath += LOGGER_FILE_SEPARATOR;
  hintFilePath += kHintFileName;

  if (config.create_hint_file)
    {
      // Create or update the helper file
      try
        {
          std::ofstream hintFile;
          if (open_file_stream (hintFile, hintFilePath,
                                std::ios::out | std::ios::trunc))
            {
              hintFile << get_hint_file_content ();
              hintFile.close ();
            }
        }
      catch (...) // NOLINT(bugprone-empty-catch)
        {
          // Ignore helper-file creation errors
        }
    }
  else
    {
      // Remove the helper file if it exists
      try
        {
          std::ifstream testFile;
          open_file_stream (testFile, hintFilePath, std::ios::in);
          if (testFile.good ())
            {
              testFile.close ();
              remove_file (hintFilePath);
            }
        }
      catch (...) // NOLINT(bugprone-empty-catch)
        {
          // Ignore helper-file deletion errors
        }
    }

  // Write the log header
  log_file << "=== Log start of the program ===\n";
  log_file_start_time = std::chrono::system_clock::now ();
  log_file << "Time start: " << _format_timestamp () << '\n';
  log_file << "Path to executable file: " << executable_directory << '\n';
  log_file << "Logging enabled by file: " << trigger_file_name << '\n';
  log_file << "Log level from trigger-file: "
           << log_level_to_string (current_log_level.load ()) << '\n';
  log_file << "Function name mode: "
           << function_name_mode_name (function_name_mode.load ()) << '\n';
  log_file << "Show stacktrace: " << (show_stack_trace ? "YES" : "NO") << '\n';
  log_file << "Stacktrace max frames: "
           << static_cast<int> (stack_trace_max_frames) << '\n';
  log_file << "Buffering enabled: " << (buffering_enabled ? "YES" : "NO")
           << '\n';
  log_file << "Describe frame (tree breakdown): "
           << (describe_frame_enabled.load () ? "YES" : "NO") << '\n';
  if (buffering_enabled)
    {
      log_file << "Buffer size: " << static_cast<int> (buffer_size)
               << " entries\n";
      if (buffering_trigger_configured)
        log_file << "Buffering trigger: "
                 << _level_to_string (buffering_trigger_level)
                 << " (dump only on this level or higher, no flush on exit)\n";
    }
  log_file << "Preset enabled: " << (preset_enabled ? "YES" : "NO") << '\n';
  if (preset_enabled && !preset_components.empty ())
    {
      log_file << "Preset components: ";
      bool first = true;
      for (auto const &component : preset_components)
        {
          if (!first)
            log_file << ", ";
          log_file << component;
          first = false;
        }
      log_file << '\n';
    }
  log_file << "==================================\n";
  log_file.flush ();
}

void
LumexLogger::_close_log_file (char const *title) LUMEX_NOEXCEPT
{
  try
    {
      // Flush the buffer on exit only if BUFFERING_TRIGGER is not set
      // (previous behavior)
      if (buffering_enabled && !log_buffer.empty ()
          && !buffering_trigger_configured)
        _flush_buffer ();
      log_buffer.clear ();

      log_file << title << '\n';
      log_file_end_time = std::chrono::system_clock::now ();
      log_file << "Time end: " << _format_timestamp () << '\n';

      auto activeTime = log_file_end_time - log_file_start_time;
      auto activeSeconds
          = std::chrono::duration_cast<std::chrono::seconds> (activeTime);
      auto activeMs
          = std::chrono::duration_cast<std::chrono::milliseconds> (activeTime)
            % std::chrono::seconds (1);
      log_file << "Active time: " << activeSeconds.count () << "."
               << std::setfill ('0') << std::setw (3) << activeMs.count ()
               << " seconds\n";
      log_file << "===================================" << '\n';
      log_file.flush ();
    }
  catch (std::exception const &e)
    {
      std::cerr << "Warning: LumexLogger cleanup error: " << e.what () << '\n';
    }
  catch (...)
    {
      std::cerr << "Warning: Unknown error during logger cleanup\n";
    }
  try
    {
      log_file.close ();
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // The stream is closed or unusable either way
    }
}

void
LumexLogger::_check_trigger_file_if_due () const LUMEX_NOEXCEPT
{
  std::int64_t const now = steady_now_ns ();
  std::int64_t due = next_trigger_check_ns.load ();
  if (now < due)
    return;

  // A level change callback that logs: its thread already checks.
  if (t_checking_trigger_file)
    return;

  // One thread wins the period and checks; the others go on at once.
  if (!next_trigger_check_ns.compare_exchange_strong (
          due, now + kTriggerCheckPeriodNs))
    return;

  // The singleton is never a const object (get_instance () returns a
  // non-const static); the const of is_level_enabled () promises the caller
  // that nothing it configured changes, which a check keeps.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  const_cast<LumexLogger *> (this)->_check_trigger_file (false);
}

void
LumexLogger::_check_trigger_file (bool wait) LUMEX_NOEXCEPT
{
  // A level change callback of this thread runs: it holds the check.
  if (t_checking_trigger_file)
    return;

  std::unique_lock<std::mutex> check (reread_mutex, std::defer_lock);
  std::string failure;
  try
    {
      if (wait)
        check.lock ();
      else if (!check.try_lock ())
        return; // another thread checks right now
      checking_trigger_file_scope const checking;

      std::string path;
      {
        std::lock_guard<std::mutex> lock (log_mutex);
        path = _trigger_file_path ();
      }

      trigger_state_t const state = _stat_trigger_file (path);
      if (state.present == last_trigger_state.present
          && state.mtime == last_trigger_state.mtime
          && state.size == last_trigger_state.size)
        return; // nothing changed: nothing changes

      // Read before the lock: the file I/O does not hold up the writers.
      // - A file that cannot be opened (held by an editor or a virus
      //   scanner) changes nothing; its state is not recorded, so the next
      //   check reads it again.
      // - A file that cannot be parsed (half written, malformed) changes
      //   nothing while logging is enabled, and is read again at the next
      //   check. While logging is disabled it enables logging with the
      //   defaults, as at start, and its state is recorded as unread, so the
      //   next check reads it again.
      logger_config_t config;
      config_read_t read = config_read_t::ok;
      if (state.present)
        read = _try_read_config_from_path (path, config);
      if (read == config_read_t::cannot_open)
        {
          failure = "cannot open " + path + "; trying again";
          throw std::runtime_error (failure);
        }
      // An empty file while logging is enabled is an editor between the
      // truncation and the text: like a half-written one. Empty means the
      // defaults only when logging is enabled by it (appearance or start).
      bool const caughtEmpty = state.present && state.size == 0
                               && read == config_read_t::ok
                               && logging_enabled.load ();
      if (read == config_read_t::unparsable || caughtEmpty)
        {
          if (logging_enabled.load ())
            {
              failure = (caughtEmpty ? "empty: " : "cannot parse ") + path
                        + "; the settings stay, trying again";
              throw std::runtime_error (failure);
            }
          config = logger_config_t ();
        }

      bool applied = false;
      {
        std::lock_guard<std::mutex> lock (log_mutex);
        bool const wasEnabled = logging_enabled.load ();
        if (!state.present)
          {
            if (wasEnabled)
              {
                // A log () that waits for the lock sees the flag and returns
                logging_enabled.store (false);
                if (log_file.is_open ())
                  _close_log_file ("=== Logging disabled: the trigger file "
                                   "was removed ===");
                log_file_path.clear ();
                // A removed trigger file is a choice: no hint on stdout
                hint_shown.store (true);
                applied = true;
              }
          }
        else if (!wasEnabled)
          {
            // Throws when the log cannot be opened: nothing is changed and
            // the state is not recorded, so the next check opens again.
            _open_log_file (config);
            logging_enabled.store (true);
            applied = true;
          }
        else
          {
            // Only what the file changed: a touch keeps the API's values.
            applied = _apply_config (config, &last_file_settings);
            if (applied && log_file.is_open ())
              {
                log_file << "[" << _format_timestamp () << "] "
                         << "=== Trigger file re-read, log level: "
                         << log_level_to_string (current_log_level.load ())
                         << " ===\n";
                log_file.flush ();
              }
          }
      }

      last_trigger_state = state;
      if (read == config_read_t::unparsable)
        last_trigger_state.mtime = kUnreadTriggerTime;
      if (state.present)
        last_file_settings = _file_settings_of (config);
      reread_failure_reported = false;
      if (applied)
        _call_level_change_callbacks (nullptr);
      return;
    }
  catch (std::exception const &e)
    {
      if (failure.empty ())
        failure = e.what ();
    }
  catch (...)
    {
      failure = "unknown error";
    }

  // One message per failure, not one per retry
  if (check.owns_lock () && !reread_failure_reported)
    {
      reread_failure_reported = true;
      std::cerr << "LumexLogger: trigger file re-read error: " << failure
                << '\n';
    }
}

void
LumexLogger::force_reread () LUMEX_NOEXCEPT
{
  _check_trigger_file (true);
}

void
LumexLogger::_call_level_change_callbacks (level_change_callback_t only)
{
  // A callback may change the state (set_log_level): the callbacks then hear
  // the final state too, in at most 4 rounds. In the last round a callback
  // can no longer change the level (two callbacks that disagree), so the
  // state every callback heard last is the final one.
  LUMEX_CONSTEXPR int kRounds = 4;
  for (int round = 0; round < kRounds; ++round)
    {
      bool const last = round == kRounds - 1;
      t_final_callback_round = last;
      bool const enabled = logging_enabled.load ();
      LogLevel const level = current_log_level.load ();
      for (std::size_t index = 0; index < kMaxLevelChangeCallbacks; ++index)
        {
          level_change_callback_t const callback
              = level_change_callbacks[index];
          if (callback == nullptr || (only != nullptr && callback != only))
            continue;
          try
            {
              callback (enabled, level);
            }
          catch (...) // NOLINT(bugprone-empty-catch)
            {
              // A throwing callback does not keep the others from the state
            }
        }
      t_final_callback_round = false;
      if (last
          || (enabled == logging_enabled.load ()
              && level == current_log_level.load ()))
        return;
      only = nullptr;
    }
}

void
LumexLogger::_push_level_change () LUMEX_NOEXCEPT
{
  // From a callback of this thread: the round that called it calls the
  // callbacks again with the new state.
  if (t_checking_trigger_file)
    return;
  try
    {
      std::lock_guard<std::mutex> check (reread_mutex);
      checking_trigger_file_scope const checking;
      _call_level_change_callbacks (nullptr);
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // The mutex failed: the callbacks hear the next change
    }
}

bool
LumexLogger::add_level_change_callback (level_change_callback_t callback)
    LUMEX_NOEXCEPT
{
  if (callback == nullptr || t_checking_trigger_file)
    return false;
  try
    {
      std::lock_guard<std::mutex> check (reread_mutex);
      checking_trigger_file_scope const checking;
      std::size_t freeIndex = kMaxLevelChangeCallbacks;
      bool present = false;
      for (std::size_t index = 0; index < kMaxLevelChangeCallbacks; ++index)
        {
          if (level_change_callbacks[index] == callback)
            present = true;
          else if (level_change_callbacks[index] == nullptr
                   && freeIndex == kMaxLevelChangeCallbacks)
            freeIndex = index;
        }
      if (!present)
        {
          if (freeIndex == kMaxLevelChangeCallbacks)
            return false; // every entry is taken
          level_change_callbacks[freeIndex] = callback;
        }
      _call_level_change_callbacks (callback);
      return true;
    }
  catch (...)
    {
      return false;
    }
}

bool
LumexLogger::remove_level_change_callback (level_change_callback_t callback)
    LUMEX_NOEXCEPT
{
  if (callback == nullptr || t_checking_trigger_file)
    return false;
  try
    {
      // Under the mutex: a round of calls in another thread ends first
      std::lock_guard<std::mutex> check (reread_mutex);
      for (std::size_t index = 0; index < kMaxLevelChangeCallbacks; ++index)
        if (level_change_callbacks[index] == callback)
          {
            level_change_callbacks[index] = nullptr;
            return true;
          }
      return false;
    }
  catch (...)
    {
      return false;
    }
}

LumexLogger &
LumexLogger::get_instance ()
{
  static LumexLogger instance;
  return instance;
}

void
LumexLogger::set_log_level (LogLevel level) LUMEX_NOEXCEPT
{
  // From a callback in the last round of a change: ignored (see
  // _call_level_change_callbacks)
  if (t_final_callback_round)
    return;
  {
    std::lock_guard<std::mutex> lock (log_mutex);
    current_log_level.store (level);
  }
  // After log_mutex: the callbacks may log
  _push_level_change ();
}

LogLevel
LumexLogger::get_log_level () const LUMEX_NOEXCEPT
{
  return current_log_level.load ();
}

void
LumexLogger::log (LogLevel level, std::string const &message)
{
  // Before the level filter, so a filtered call still checks
  _check_trigger_file_if_due ();

  if (!logging_enabled.load ())
    {
      // Show the hint once if logging is disabled but someone
      // tries to log
      if (!hint_shown.exchange (true))
        print_logger_hint ();
      return;
    }

  std::lock_guard<std::mutex> lock (log_mutex);
  // The trigger file may have disappeared while this thread waited
  if (!logging_enabled.load ())
    return;
  if (static_cast<uint8_t> (level)
      >= static_cast<uint8_t> (current_log_level.load ()))
    {
      // The buffer-flush trigger depends only on level; the preset does not
      // affect buffering
      bool isCriticalLevel
          = buffering_trigger_configured
                ? (static_cast<uint8_t> (level)
                   >= static_cast<uint8_t> (buffering_trigger_level))
                : (level == LogLevel::LEVEL_WARNING
                   || level == LogLevel::LEVEL_ERROR
                   || level == LogLevel::LEVEL_FATAL);

      // First flush the accumulated buffer (if there is a
      // trigger-level message)
      if (buffering_enabled && isCriticalLevel && !log_buffer.empty ())
        _flush_buffer ();

      // A trigger-level message is always written after the buffer flush
      // (the preset does NOT filter critical levels - that is the key
      // difference from the non-critical levels below, intentionally so that
      // WARNING/ERROR/FATAL from a component not in the preset are not dropped
      // and always flush the buffer)
      if (buffering_enabled && isCriticalLevel)
        {
          std::string fullMessage = message;
          bool shouldShowTrace = show_stack_trace;
          if (!shouldShowTrace
              && (level == LogLevel::LEVEL_WARNING
                  || level == LogLevel::LEVEL_ERROR))
            shouldShowTrace = true;
          if (shouldShowTrace)
            {
              try
                {
                  std::string stackTrace
                      = capture_stack_trace (1, stack_trace_max_frames);
                  if (!stackTrace.empty ())
                    fullMessage += "\n" + stackTrace;
                }
              catch (std::exception const &exc)
                {
                  std::cerr << "Warning: Error during capture stacktrace: "
                            << exc.what () << '\n';
                }
              catch (...)
                {
                  std::cerr
                      << "Warning: Unknown error during capture stacktrace\n";
                }
            }
          if (_can_write_log (fullMessage.length ()))
            _write_log (level, fullMessage);
          else
            {
              std::string warningMsg
                  = "LOG SIZE LIMIT EXCEEDED: Cannot write log message. "
                    "Current size: "
                    + _format_size (
                        use_timestamped_logs
                            ? get_directory_size (executable_directory
                                                  + LOGGER_FILE_SEPARATOR
                                                  + kLogDirectoryName)
                            : get_file_size (log_file_path))
                    + ", Free space: "
                    + _format_size (get_free_disk_space (log_file_path));
              _write_log (LogLevel::LEVEL_WARNING, warningMsg);
            }
          return;
        }

      // Non-critical level: the preset filters buffer insert and write
      if (!_should_log_by_preset (message))
        return;

      std::string fullMessage = message;
      bool shouldShowTrace = show_stack_trace;
      if (!shouldShowTrace
          && (level == LogLevel::LEVEL_WARNING
              || level == LogLevel::LEVEL_ERROR))
        shouldShowTrace = true;
      if (shouldShowTrace)
        {
          try
            {
              std::string stackTrace
                  = capture_stack_trace (1, stack_trace_max_frames);
              if (!stackTrace.empty ())
                fullMessage += "\n" + stackTrace;
            }
          catch (std::exception const &exc)
            {
              std::cerr << "Warning: Error during capture stacktrace: "
                        << exc.what () << '\n';
            }
          catch (...)
            {
              std::cerr
                  << "Warning: Unknown error during capture stacktrace\n";
            }
        }

      if (buffering_enabled)
        {
          log_buffer.emplace_back (level, fullMessage);
          if (log_buffer.size () > buffer_size)
            log_buffer.pop_front ();
          return;
        }

      if (_can_write_log (fullMessage.length ()))
        {
          _write_log (level, fullMessage);
        }
      else
        {
          std::string warningMsg
              = "LOG SIZE LIMIT EXCEEDED: Cannot write log message. Current "
                "size: "
                + _format_size (
                    use_timestamped_logs
                        ? get_directory_size (executable_directory
                                              + LOGGER_FILE_SEPARATOR
                                              + kLogDirectoryName)
                        : get_file_size (log_file_path))
                + ", Free space: "
                + _format_size (get_free_disk_space (log_file_path));
          _write_log (LogLevel::LEVEL_WARNING, warningMsg);
        }
    }
}

bool
LumexLogger::is_level_enabled (LogLevel level) const LUMEX_NOEXCEPT
{
  // Before the level filter, so a filtered call still checks
  _check_trigger_file_if_due ();

  if (!logging_enabled.load ())
    return false;
  return static_cast<uint8_t> (level)
         >= static_cast<uint8_t> (current_log_level.load ());
}

bool
LumexLogger::is_describe_frame_enabled () const LUMEX_NOEXCEPT
{
  return describe_frame_enabled.load ();
}

bool
LumexLogger::is_logging_enabled () const LUMEX_NOEXCEPT
{
  return logging_enabled.load ();
}

FunctionNameMode
LumexLogger::get_function_name_mode () const LUMEX_NOEXCEPT
{
  return function_name_mode.load ();
}

bool
LumexLogger::should_show_function_name () const LUMEX_NOEXCEPT
{
  return function_name_mode.load () != FunctionNameMode::NONE;
}

std::string
LumexLogger::get_log_file_path () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return log_file_path;
}

void
LumexLogger::
    print_logger_hint () // NOLINT(readability-convert-member-functions-to-static)
    const LUMEX_NOEXCEPT
{
  try
    {
      file_hint_about_logger ();
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // Ignore errors while printing the hint
    }
}

void
LumexLogger::flush ()
{
  if (!logging_enabled.load ())
    return;

  std::lock_guard<std::mutex> lock (log_mutex);
  if (buffering_enabled && !log_buffer.empty ())
    _flush_buffer ();
  if (log_file.is_open ())
    log_file.flush ();
}

void
LumexLogger::enable_buffering (std::size_t kBufferSize) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  buffering_enabled = true;
  buffer_size
      = (kBufferSize > 0)
            ? kBufferSize
            : logger_config_t::kBufferSize; // At least 1, by default
                                            // logger_config_t::kBufferSize
                                            // (100)

  // If the new size is smaller than the current buffer, drop older records
  if (log_buffer.size () > buffer_size)
    {
      std::size_t elementsToRemove = log_buffer.size () - buffer_size;
      log_buffer.erase (log_buffer.begin (),
                        log_buffer.begin ()
                            + static_cast<ptrdiff_t> (elementsToRemove));
    }
}

void
LumexLogger::disable_buffering ()
{
  std::lock_guard<std::mutex> lock (log_mutex);

  // Write every buffered record before disable
  if (!log_buffer.empty ())
    _flush_buffer ();

  buffering_enabled = false;
  log_buffer.clear ();
}

void
LumexLogger::set_buffer_size (std::size_t bufferSize) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  buffer_size
      = (bufferSize > 0)
            ? bufferSize
            : logger_config_t::kBufferSize; // At least 1, by default
                                            // logger_config_t::kBufferSize
                                            // (100)

  // If the new size is smaller than the current buffer, drop older records
  if (log_buffer.size () > buffer_size)
    {
      std::size_t elementsToRemove = log_buffer.size () - buffer_size;
      log_buffer.erase (log_buffer.begin (),
                        log_buffer.begin ()
                            + static_cast<ptrdiff_t> (elementsToRemove));
    }
}

bool
LumexLogger::is_buffering_enabled () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return buffering_enabled;
}

std::size_t
LumexLogger::get_buffer_size () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return buffer_size;
}

void
LumexLogger::enable_preset (std::unordered_set<std::string> const &components)
    LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  preset_enabled = true;
  preset_components = components;
}

void
LumexLogger::disable_preset () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  preset_enabled = false;
  preset_components.clear ();
}

void
LumexLogger::set_preset_components (
    std::unordered_set<std::string> const &components) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  preset_enabled = true; // Automatically enable the preset
  preset_components = components;
}

bool
LumexLogger::is_preset_enabled () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return preset_enabled;
}

std::unordered_set<std::string>
LumexLogger::get_preset_components () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return preset_components; // Return a copy
}

void
LumexLogger::set_trigger_file_name (std::string const &fileName) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  if (!fileName.empty ())
    trigger_file_name = fileName;
}

std::string
LumexLogger::get_trigger_file_name () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return trigger_file_name;
}

logger_applied_config_view_t
LumexLogger::get_applied_config_view () const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (log_mutex);
  return logger_applied_config_view_t{ use_timestamped_logs, show_stack_trace,
                                       stack_trace_max_frames,
                                       buffering_trigger_configured,
                                       buffering_trigger_level };
}

std::unordered_set<std::string>
LumexLogger::_parse_preset_components (std::string const &componentsStr)
    LUMEX_NOEXCEPT
{
  std::unordered_set<std::string> result;
  try
    {
      if (componentsStr.empty ())
        return result;

      std::stringstream ss (componentsStr);
      std::string component;

      // Split the string on commas
      while (std::getline (ss, component, ','))
        {
          // Strip leading and trailing spaces
          component.erase (0, component.find_first_not_of (" \t\r\n"));
          component.erase (component.find_last_not_of (" \t\r\n") + 1);

          // Add the component if it is not empty
          if (!component.empty ())
            result.insert (component);
        }
    }
  catch (...)
    {
      // On error, return an empty set
      return {};
    }

  return result;
}

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)

std::string
LumexLogger::_get_executable_directory () const
{
#if defined(_WIN32) || defined(_WIN64)
  // Windows: use GetModuleFileName to get the DLL/EXE path
  // The wide function: the narrow one gives the ANSI code page, and every
  // path of the logger is UTF-8.
  wchar_t path[LOGGER_MAX_PATH];
  HMODULE hModule = nullptr;

  // Try to get a handle to the current module
  if (GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                              | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR> (this), &hModule))
    {
      if (GetModuleFileNameW (hModule, path, LOGGER_MAX_PATH) > 0)
        return _extract_directory_from_path (
            ::lumex::core::utility::utf8_file_name::to_utf8 (path));
    }

  // Fallback: use GetModuleFileName with nullptr
  if (GetModuleFileNameW (nullptr, path, LOGGER_MAX_PATH) > 0)
    return _extract_directory_from_path (
        ::lumex::core::utility::utf8_file_name::to_utf8 (path));

  throw std::runtime_error ("Can't get executable directory on Windows");

#elif defined(__linux__)
  // Linux: read /proc/self/exe for the executable path
  char path[LOGGER_MAX_PATH];
  ssize_t len = readlink ("/proc/self/exe", path, sizeof (path) - 1);
  if (len != -1)
    {
      path[len] = '\0';
      return _extract_directory_from_path (path);
    }

  // Fallback: use getcwd
  char cwd[LOGGER_MAX_PATH];
  if (getcwd (cwd, sizeof (cwd)) != nullptr)
    return std::string (cwd);

  throw std::runtime_error ("Can't get executable directory on Linux");

#elif defined(__APPLE__)
  // macOS: use _NSGetExecutablePath
  char path[LOGGER_MAX_PATH];
  std::uint32_t size = sizeof (path);
  if (_NSGetExecutablePath (path, &size) == 0)
    return _extract_directory_from_path (path);

  // Fallback: use getcwd
  char cwd[LOGGER_MAX_PATH];
  if (getcwd (cwd, sizeof (cwd)) != nullptr)
    return std::string (cwd);

  throw std::runtime_error ("Can't get executable directory on macOS");

#else
  // Unknown platform: use the current working directory
  char cwd[LOGGER_MAX_PATH];
  if (getcwd (cwd, sizeof (cwd)) != nullptr)
    return std::string (cwd);

  throw std::runtime_error (
      "Unknown platform, can't get executable directory");
#endif
}

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)

std::string
LumexLogger::_create_log_file_path (bool timestamped) const
{
  if (timestamped)
    return _create_timestamped_log_file_path ();
  return _create_single_log_file_path ();
}

std::string
LumexLogger::_create_single_log_file_path () const
{
  std::string logPath = executable_directory;

  // Append a path separator if missing
  if (!logPath.empty ()
      && logPath[logPath.length () - 1] != LOGGER_FILE_SEPARATOR)
    logPath += LOGGER_FILE_SEPARATOR;

  logPath += kDefaultLogFileName;
  return logPath;
}

std::string
LumexLogger::_create_timestamped_log_file_path () const
{
  std::string logDir = executable_directory;

  // Append a path separator if missing
  if (!logDir.empty ()
      && logDir[logDir.length () - 1] != LOGGER_FILE_SEPARATOR)
    logDir += LOGGER_FILE_SEPARATOR;

  // Append the timestamped-log directory name
  logDir += kLogDirectoryName;

  // Create the directory if it does not exist
  if (!_create_directory_if_not_exists (logDir))
    {
      // If the directory cannot be created, return the ordinary log-file
      // path. Fallback when the directory cannot be created
      return _create_single_log_file_path ();
    }

  // Append a path separator to the directory
  logDir += LOGGER_FILE_SEPARATOR;

  // Generate a timestamped file name; a log of the same second that exists
  // already (the trigger file came back within it) keeps its name, the new
  // one takes the first free suffix _2, _3, ...
  std::string const stem = logDir + kDefaultLogFilePrefix + "_"
                           + _format_timestamp_for_filename ();
  std::string fullPath = stem + ".log";
  LUMEX_CONSTEXPR int kMaxSameSecondLogs = 1000;
  for (int index = 2; index <= kMaxSameSecondLogs; ++index)
    {
      std::ifstream existing;
      if (!open_file_stream (existing, fullPath, std::ios::in))
        break;
      existing.close ();
      fullPath = stem + "_" + std::to_string (index) + ".log";
    }

  // Extra check: try to create a test file to verify
  // access rights
  std::ofstream testFile;
  if (!open_file_stream (testFile, fullPath, std::ios::out | std::ios::app))
    {
      // If a file cannot be created in the timestamped directory, fall back to
      // the ordinary file
      testFile.close ();
      return _create_single_log_file_path ();
    }
  testFile.close ();

  // Delete the test file
  remove_file (fullPath);

  return fullPath;
}

std::string
LumexLogger::_format_timestamp_for_filename ()
{
  auto now = std::chrono::system_clock::now ();
  auto raw_time = std::chrono::system_clock::to_time_t (now);

  std::stringstream sstream;

  // Thread-safe localtime: localtime_s (Win) / localtime_r (POSIX)
  struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s (std::addressof (timeinfo), std::addressof (raw_time));
#else
  localtime_r (std::addressof (raw_time),
               std::addressof (timeinfo)); // POSIX thread-safe version
#endif

  // Format as DD.MM.YYYY-hh-mm-ss (no colons, for Windows)
  LUMEX_CONSTEXPR int YEAR_OFFSET = 1900;
  sstream << std::setfill ('0') << std::setw (2) << timeinfo.tm_mday << "."
          << std::setfill ('0') << std::setw (2) << (timeinfo.tm_mon + 1)
          << "." << std::setfill ('0') << std::setw (4)
          << (timeinfo.tm_year + YEAR_OFFSET) << "-" << std::setfill ('0')
          << std::setw (2) << timeinfo.tm_hour << "-" << std::setfill ('0')
          << std::setw (2) << timeinfo.tm_min << "-" << std::setfill ('0')
          << std::setw (2) << timeinfo.tm_sec;

  return sstream.str ();
}

std::string
LumexLogger::_format_timestamp ()
{
  auto now = std::chrono::system_clock::now ();
  // Rename the variable to avoid shadowing the global
  // time_t type from <time.h>. ISO C++: local names may shadow global
  // names, but -Wshadow warns for clarity
  auto raw_time = std::chrono::system_clock::to_time_t (now);

  auto ms_count = std::chrono::duration_cast<std::chrono::milliseconds> (
                      now.time_since_epoch ())
                  % std::chrono::milliseconds::period::den;

  std::stringstream sstream;

  // Thread-safe localtime: localtime_s (Win) / localtime_r (POSIX)
  // ISO C: localtime() uses static buffer - race condition in multi-threaded
  // code (a race in multithreaded code)
  struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s (std::addressof (timeinfo), std::addressof (raw_time));
#else
  localtime_r (std::addressof (raw_time),
               std::addressof (timeinfo)); // POSIX thread-safe version
#endif
  sstream << std::put_time (std::addressof (timeinfo), "%Y-%m-%d %H:%M:%S");
  sstream << '.' << std::setfill ('0') << std::setw (3) << ms_count.count ();

  return sstream.str ();
}

std::string
LumexLogger::_level_to_string (LogLevel level)
{
  switch (level)
    {
    case LogLevel::LEVEL_TRACE:
      return "TRACE";
    case LogLevel::LEVEL_DEBUG:
      return "DEBUG";
    case LogLevel::LEVEL_INFO:
      return "INFO ";
    case LogLevel::LEVEL_SUCCESS:
      return "SUCCESS";
    case LogLevel::LEVEL_WARNING:
      return "WARNING";
    case LogLevel::LEVEL_ERROR:
      return "ERROR";
    case LogLevel::LEVEL_FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
    }
}

void
LumexLogger::_write_log (LogLevel level, std::string const &message)
{
  // Lock already held by caller

  if (log_file.is_open ())
    {
      try
        {
          log_file << "[" << _format_timestamp () << "] "
                   << "[" << _level_to_string (level) << "] " << message
                   << '\n';

          // Flush only for critical levels because:
          // 1. ERROR and FATAL messages are critical and must be written
          // immediately
          // 2. Flushing after every log greatly reduces
          // performance
          // 3. For ordinary levels (TRACE, DEBUG, INFO, WARNING),
          // buffering is enough
          // 4. The OS will flush buffers to the file when
          // needed
          // 5. Forced flush() only for critical levels
          // keeps the balance
          //    between performance and reliable write of important messages
          if (level >= LogLevel::LEVEL_ERROR)
            log_file.flush ();
        }
      catch (std::ios_base::failure const &e)
        {
          // If the file write fails, write to stderr
          std::cerr << "Error writing to log: " << e.what () << '\n';
          throw;
        }
    }
}

std::string
LumexLogger::_extract_directory_from_path (std::string const &fullPath)
{
  // Check for an empty string
  if (fullPath.empty ())
    return ".";

  std::string directory = fullPath;

  // Find the last path separator
  std::size_t lastSlash = directory.find_last_of (LOGGER_FILE_SEPARATOR);
  if (lastSlash != std::string::npos && lastSlash < directory.length ())
    {
      directory = directory.substr (0, lastSlash);
    }
  else
    {
      // If there is no separator, return the current directory
      directory = ".";
    }

  return directory;
}

std::string
LumexLogger::_trigger_file_path () const
{
  std::string enableFilePath = executable_directory;

  // Append a path separator if missing
  if (!enableFilePath.empty ()
      && enableFilePath[enableFilePath.length () - 1] != LOGGER_FILE_SEPARATOR)
    enableFilePath += LOGGER_FILE_SEPARATOR;

  enableFilePath += trigger_file_name;
  return enableFilePath;
}

LumexLogger::trigger_state_t
LumexLogger::_stat_trigger_file (std::string const &path) LUMEX_NOEXCEPT
{
  trigger_state_t state = { 0, 0, false };
#if defined(_WIN32) || defined(_WIN64)
  // The write time in 100 ns units: a rewrite within the same second is seen
  std::wstring widePath;
  WIN32_FILE_ATTRIBUTE_DATA fileData{};
  if (::lumex::core::utility::utf8_file_name::to_wide (path, widePath)
      && GetFileAttributesExW (widePath.c_str (), GetFileExInfoStandard,
                               &fileData)
             != 0
      && (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
      state.present = true;
      state.mtime = static_cast<std::int64_t> (
          (static_cast<std::uint64_t> (fileData.ftLastWriteTime.dwHighDateTime)
           << 32U)
          | fileData.ftLastWriteTime.dwLowDateTime);
      state.size = static_cast<std::int64_t> (
          (static_cast<std::uint64_t> (fileData.nFileSizeHigh) << 32U)
          | fileData.nFileSizeLow);
    }
#else
  struct stat fileStat{};
  if (stat (path.c_str (), &fileStat) == 0 && !S_ISDIR (fileStat.st_mode))
    {
      LUMEX_CONSTEXPR std::int64_t kNsPerSecond = 1000000000;
      state.present = true;
#if defined(__APPLE__)
      state.mtime
          = static_cast<std::int64_t> (fileStat.st_mtimespec.tv_sec)
                * kNsPerSecond
            + static_cast<std::int64_t> (fileStat.st_mtimespec.tv_nsec);
#elif defined(__linux__)
      state.mtime
          = static_cast<std::int64_t> (fileStat.st_mtim.tv_sec) * kNsPerSecond
            + static_cast<std::int64_t> (fileStat.st_mtim.tv_nsec);
#else
      state.mtime
          = static_cast<std::int64_t> (fileStat.st_mtime) * kNsPerSecond;
#endif
      state.size = static_cast<std::int64_t> (fileStat.st_size);
    }
#endif
  return state;
}

logger_config_t
LumexLogger::read_config_from_path (std::string const &enableFilePath)
{
  logger_config_t config;
  if (_try_read_config_from_path (enableFilePath, config) != config_read_t::ok)
    return logger_config_t{};
  return config;
}

LumexLogger::config_read_t
LumexLogger::
    _try_read_config_from_path ( // NOLINT(readability-function-cognitive-complexity)
        std::string const &enableFilePath, logger_config_t &config)
{
  config = logger_config_t{};
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
  (void)enableFilePath;
  throw std::runtime_error (
      "logger config YAML is selected "
      "(LUMEX_LOGGER_CONFIG_FORMAT=YAML) but LumexSettingsYAML "
      "is not implemented yet");
#else
  try
    {
      // A file that cannot be opened (missing, held without sharing) is not
      // a configuration; one of size zero is the defaults in every format
      // (the hint says: create the trigger file).
      {
        std::ifstream probe;
        if (!open_file_stream (probe, enableFilePath,
                               std::ios::in | std::ios::binary))
          return config_read_t::cannot_open;
        if (probe.peek () == std::ifstream::traits_type::eof ())
          return probe.bad () ? config_read_t::unparsable : config_read_t::ok;
      }
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_XML)
      ::lumex::xml::document::XmlDocument document;
#if defined(LOGGER_OS_WINDOWS)
      // The wide overload: the narrow one reads the path in the ANSI code
      // page (the path was checked to be valid UTF-8 by the probe above).
      std::wstring wideConfigPath;
      ::lumex::core::utility::utf8_file_name::to_wide (enableFilePath,
                                                       wideConfigPath);
      ::lumex::xml::text::xml_parse_result_t const result
          = document.load_file (wideConfigPath.c_str ());
#else
      ::lumex::xml::text::xml_parse_result_t const result
          = document.load_file (enableFilePath.c_str ());
#endif
      if (!result)
        return config_read_t::unparsable;

      ::lumex::xml::node::XmlNode const root = document.child ("logger");
      if (!root)
        return config_read_t::unparsable;

      char const *const keys[]
          = { "LEVEL",       "FUNCNAME",          "TIMESTAMPED",
              "STACKTRACE",  "STACKTRACE_FRAMES", "HINT",
              "PRESET",      "BUFFERING",         "DESCRIBE_FRAME",
              "BUFFER_SIZE", "BUFFERING_TRIGGER" };
      for (char const *key : keys)
        {
          ::lumex::xml::node::XmlNode const node = root.child (key);
          if (!node)
            continue;

          char const *const value = node.text ().get ();
          if (value != nullptr && value[0] != '\0')
            _apply_logger_config_key (config, key, value);
        }
      return config_read_t::ok;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
      ::lumex::applied::settings::ini::LumexSettingsINI ini;
      if (!ini.load (enableFilePath))
        return config_read_t::unparsable;

      char const *const kLoggerSection = "logger";
      char const *const keys[]
          = { "LEVEL",       "FUNCNAME",          "TIMESTAMPED",
              "STACKTRACE",  "STACKTRACE_FRAMES", "HINT",
              "PRESET",      "BUFFERING",         "DESCRIBE_FRAME",
              "BUFFER_SIZE", "BUFFERING_TRIGGER" };
      for (char const *key : keys)
        {
          std::string const value = ini.get (kLoggerSection, key);
          if (!value.empty ())
            _apply_logger_config_key (config, key, value);
        }
      return config_read_t::ok;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
      std::ifstream file;
      if (!open_file_stream (file, enableFilePath, std::ios::in))
        return config_read_t::cannot_open;

      nlohmann::json root;
      file >> root;

      nlohmann::json const *obj = &root;
      if (root.is_object () && root.contains ("logger")
          && root["logger"].is_object ())
        obj = &root["logger"];

      if (!obj->is_object ())
        return config_read_t::unparsable;

      for (auto const &item : obj->items ())
        {
          if (!item.value ().is_string () && !item.value ().is_number ()
              && !item.value ().is_boolean ())
            continue;

          std::string value;
          if (item.value ().is_string ())
            value = item.value ().get<std::string> ();
          else if (item.value ().is_boolean ())
            value = item.value ().get<bool> () ? "true" : "false";
          else if (item.value ().is_number_integer ())
            value = std::to_string (item.value ().get<long long> ());
          else if (item.value ().is_number_unsigned ())
            value = std::to_string (item.value ().get<unsigned long long> ());
          else
            value = std::to_string (item.value ().get<double> ());

          _apply_logger_config_key (config, item.key (), value);
        }
      return config_read_t::ok;
#else
      std::ifstream file;
      if (!open_file_stream (file, enableFilePath, std::ios::in))
        return config_read_t::cannot_open;

      int oldFormatLineNumber = 0;
      std::string line;
      while (std::getline (file, line))
        {
          line.erase (0, line.find_first_not_of (" \t\r\n"));
          line.erase (line.find_last_not_of (" \t\r\n") + 1);

          if (line.empty () || line[0] == '#')
            continue;

          if (_has_prefix (line, "LEVEL"))
            {
              std::string value = _parse_prefixed_value (line, "LEVEL");
              if (!value.empty ())
                _apply_logger_config_key (config, "LEVEL", value);
            }
          else if (_has_prefix (line, "FUNCNAME"))
            {
              std::string value = _parse_prefixed_value (line, "FUNCNAME");
              if (!value.empty ())
                _apply_logger_config_key (config, "FUNCNAME", value);
            }
          else if (_has_prefix (line, "TIMESTAMPED"))
            {
              std::string value = _parse_prefixed_value (line, "TIMESTAMPED");
              if (!value.empty ())
                _apply_logger_config_key (config, "TIMESTAMPED", value);
            }
          else if (_has_prefix (line, "STACKTRACE_FRAMES"))
            {
              std::string value
                  = _parse_prefixed_value (line, "STACKTRACE_FRAMES");
              if (!value.empty ())
                _apply_logger_config_key (config, "STACKTRACE_FRAMES", value);
            }
          else if (_has_prefix (line, "STACKTRACE"))
            {
              std::string value = _parse_prefixed_value (line, "STACKTRACE");
              if (!value.empty ())
                _apply_logger_config_key (config, "STACKTRACE", value);
            }
          else if (_has_prefix (line, "HINT"))
            {
              std::string value = _parse_prefixed_value (line, "HINT");
              if (!value.empty ())
                _apply_logger_config_key (config, "HINT", value);
            }
          else if (_has_prefix (line, "PRESET"))
            {
              std::string value = _parse_prefixed_value (line, "PRESET");
              if (!value.empty ())
                _apply_logger_config_key (config, "PRESET", value);
            }
          else if (_has_prefix (line, "BUFFERING_TRIGGER"))
            {
              std::string value
                  = _parse_prefixed_value (line, "BUFFERING_TRIGGER");
              if (!value.empty ())
                _apply_logger_config_key (config, "BUFFERING_TRIGGER", value);
            }
          else if (_has_prefix (line, "BUFFERING"))
            {
              std::string value = _parse_prefixed_value (line, "BUFFERING");
              if (!value.empty ())
                _apply_logger_config_key (config, "BUFFERING", value);
            }
          else if (_has_prefix (line, "DESCRIBE_FRAME"))
            {
              std::string value
                  = _parse_prefixed_value (line, "DESCRIBE_FRAME");
              if (!value.empty ())
                _apply_logger_config_key (config, "DESCRIBE_FRAME", value);
            }
          else if (_has_prefix (line, "BUFFER_SIZE"))
            {
              std::string value = _parse_prefixed_value (line, "BUFFER_SIZE");
              if (!value.empty ())
                _apply_logger_config_key (config, "BUFFER_SIZE", value);
            }
          else
            {
              // Unknown KEY=value is ignored (same as INI/JSON/XML).
              // Legacy positional format is lines without '='.
              if (line.find ('=') != std::string::npos)
                continue;

              oldFormatLineNumber++;

              if (oldFormatLineNumber == 1)
                _apply_logger_config_key (config, "LEVEL", line);
              else if (oldFormatLineNumber == 2)
                _apply_logger_config_key (config, "FUNCNAME", line);
              else if (oldFormatLineNumber == 3)
                _apply_logger_config_key (config, "TIMESTAMPED", line);
            }
        }

      // A read error (not the end of the file) is a failure too
      return file.bad () ? config_read_t::unparsable : config_read_t::ok;
#endif
    }
  catch (...)
    {
      config = logger_config_t{};
      return config_read_t::unparsable;
    }
#endif
}

void
LumexLogger::_apply_logger_config_key (logger_config_t &config,
                                       std::string const &key,
                                       std::string const &value) LUMEX_NOEXCEPT
{
  try
    {
      std::string const normalized = _normalize_string (key);
      if (normalized == "LEVEL")
        config.log_level = _string_to_log_level (value);
      else if (normalized == "FUNCNAME")
        config.func_name_mode = _parse_function_name_mode (value);
      else if (normalized == "TIMESTAMPED")
        config.use_timestamped_logs = _should_use_timestamped_logs (value);
      else if (normalized == "STACKTRACE_FRAMES")
        config.stack_trace_max_frames = _parse_stack_trace_frames (
            value, logger_config_t ().kMaxStackTraceFrames);
      else if (normalized == "STACKTRACE")
        config.show_stack_trace = _should_show_stack_trace (value);
      else if (normalized == "HINT")
        config.create_hint_file = _should_show_stack_trace (value);
      else if (normalized == "PRESET")
        config.preset_components = _parse_preset_components (value);
      else if (normalized == "BUFFERING_TRIGGER")
        {
          config.buffering_trigger_configured = true;
          config.buffering_trigger_level = _string_to_log_level (value);
        }
      else if (normalized == "BUFFERING")
        config.buffering_enabled = _should_show_stack_trace (value);
      else if (normalized == "DESCRIBE_FRAME")
        config.describe_frame = _should_show_stack_trace (value);
      else if (normalized == "BUFFER_SIZE")
        {
          try
            {
              std::size_t bufferSize = std::stoull (value);
              config.buffer_size = (bufferSize > 0)
                                       ? bufferSize
                                       : logger_config_t ().kBufferSize;
            }
          catch (...)
            {
              config.buffer_size = logger_config_t ().kBufferSize;
            }
        }
    }
  catch (...)
    {
      // On any error, leave the caller's config unchanged for this key.
    }
}

LogLevel
LumexLogger::_string_to_log_level (std::string const &levelStr) LUMEX_NOEXCEPT
{
  try
    {
      // Normalize the string (uppercase + strip spaces)
      std::string normalizedStr = _normalize_string (levelStr);

      // Compare with the known levels
      if (normalizedStr == "TRACE")
        return LogLevel::LEVEL_TRACE;
      if (normalizedStr == "DEBUG")
        return LogLevel::LEVEL_DEBUG;
      if (normalizedStr == "INFO")
        return LogLevel::LEVEL_INFO;
      if (normalizedStr == "SUCCESS")
        return LogLevel::LEVEL_SUCCESS;
      if (normalizedStr == "WARNING")
        return LogLevel::LEVEL_WARNING;
      if (normalizedStr == "ERROR")
        return LogLevel::LEVEL_ERROR;
      if (normalizedStr == "FATAL")
        return LogLevel::LEVEL_FATAL;

      return LogLevel::LEVEL_INFO; // Default is INFO
    }
  catch (...)
    {
      // On any error, use INFO by default
      return LogLevel::LEVEL_INFO;
    }
}

FunctionNameMode
LumexLogger::_parse_function_name_mode (std::string const &funcNameStr)
    LUMEX_NOEXCEPT
{
  try
    {
      // Normalize the string (uppercase + strip spaces)
      std::string normalizedStr = _normalize_string (funcNameStr);

      // Parse the variants (case-insensitive). signature -> FULL for
      // backward compatibility
      if (normalizedStr == "NONE" || normalizedStr == "NO_FUNCNAME")
        return FunctionNameMode::NONE;
      if (normalizedStr == "SHORT")
        return FunctionNameMode::SHORT;
      if (normalizedStr == "FULL" || normalizedStr == "SIGNATURE")
        return FunctionNameMode::FULL;
      if (normalizedStr == "NORMAL")
        return FunctionNameMode::NORMAL;

      // By default return NORMAL for backward compatibility
      return FunctionNameMode::NORMAL;
    }
  catch (...)
    {
      // On any error, return NORMAL by default
      return FunctionNameMode::NORMAL;
    }
}

bool
LumexLogger::_should_show_function_name (std::string const &funcNameStr)
    LUMEX_NOEXCEPT
{
  return _parse_function_name_mode (funcNameStr) != FunctionNameMode::NONE;
}

bool
LumexLogger::_should_show_stack_trace (std::string const &stackTraceStr)
    LUMEX_NOEXCEPT
{
  try
    {
      // Normalize the string (uppercase + strip spaces)
      std::string normalizedStr = _normalize_string (stackTraceStr);

      // Check the various "true" spellings
      return (normalizedStr == "TRUE" || normalizedStr == "YES"
              || normalizedStr == "1");
    }
  catch (...)
    {
      // On any error, return false
      return false;
    }
}

short
LumexLogger::_parse_stack_trace_frames (std::string const &framesStr,
                                        short defaultFrames) LUMEX_NOEXCEPT
{
  try
    {
      if (framesStr.empty ())
        return defaultFrames;

      // Try to convert the string to a number
      int frames = std::stoi (framesStr);

      // Clamp the range from 1 to the maximum (64 frames)
      LUMEX_CONSTEXPR int kMaxStackTraceFrames = 64;
      if (frames < 1)
        return 1;
      if (frames > kMaxStackTraceFrames)
        return static_cast<short> (kMaxStackTraceFrames);

      return static_cast<short> (frames);
    }
  catch (std::exception const &exc)
    {
      // On any error, return the default value
      std::cerr << "Warning: Error during parse stacktrace frames: "
                << exc.what ()
                << ". Returning default frames: " << defaultFrames << '\n';
      return defaultFrames;
    }
  catch (...)
    {
      std::cerr << "Warning: Unknown error during parse stacktrace frames. "
                   "Returning default frames: "
                << defaultFrames << '\n';
      return defaultFrames;
    }
}

bool
LumexLogger::_should_use_timestamped_logs (std::string const &timestampedStr)
    LUMEX_NOEXCEPT
{
  try
    {
      // Normalize the string (uppercase + strip spaces)
      std::string normalizedStr = _normalize_string (timestampedStr);

      // Check the various "true" spellings
      return (normalizedStr == "TIMESTAMPED" || normalizedStr == "TRUE"
              || normalizedStr == "YES" || normalizedStr == "1");
    }
  catch (...)
    {
      // On any error, do not use timestamped logs
      return false;
    }
}

bool
LumexLogger::_check_log_size_limits () const LUMEX_NOEXCEPT
{
  try
    {
      if (!logging_enabled.load () || log_file_path.empty ())
        return true;

      // Get free disk space
      int64_t freeSpace = get_free_disk_space (log_file_path);
      if (freeSpace < 0)
        return true; // If the information cannot be obtained, allow the write

      // Decide whether we are dealing with a file or a folder
      bool isDirectory = use_timestamped_logs;

      // Compute the maximum allowed size
      int64_t maxAllowedSize
          = _calculate_max_allowed_size (freeSpace, isDirectory);

      // Get the current size
      int64_t currentSize = 0;
      if (isDirectory)
        {
          // For a folder, get the total size of all files
          std::string logDir = executable_directory;
          if (!logDir.empty ()
              && logDir[logDir.length () - 1] != LOGGER_FILE_SEPARATOR)
            logDir += LOGGER_FILE_SEPARATOR;
          logDir += kLogDirectoryName;

          currentSize = get_directory_size (logDir);
        }
      else
        {
          // For a file, get the file size
          currentSize = get_file_size (log_file_path);
        }

      if (currentSize < 0)
        return true; // If the size cannot be obtained, allow the write

      // Check whether the limit is exceeded
      if (currentSize > maxAllowedSize)
        {
          // Try to clean up old logs
          std::string logPath
              = isDirectory ? (executable_directory + LOGGER_FILE_SEPARATOR
                               + kLogDirectoryName)
                            : log_file_path;

          LUMEX_CONSTEXPR double kCleanupRatio = 0.8; // Leave 80% of the limit
          int64_t targetSize = static_cast<int64_t> (
              static_cast<double> (maxAllowedSize) * kCleanupRatio);
          return _cleanup_old_logs (logPath, targetSize);
        }

      return true;
    }
  catch (...)
    {
      // On any error, allow the write
      return true;
    }
}

int64_t
LumexLogger::_calculate_max_allowed_size (int64_t freeSpace,
                                          bool isDirectory) LUMEX_NOEXCEPT
{
  try
    {
      // Base limits
      int64_t baseLimit
          = isDirectory ? kMaxLogDirectorySizeBytes : kMaxLogFileSizeBytes;

      // Compute the limit from free space
      auto freeSpaceLimit = static_cast<int64_t> (
          static_cast<double> (freeSpace) * kMinFreeSpaceRatio);

      // Honor the minimum free space
      int64_t adjustedFreeSpace = std::max (static_cast<int64_t> (0),
                                            freeSpace - kMinFreeSpaceBytes);

      auto adjustedFreeSpaceLimit = static_cast<int64_t> (
          static_cast<double> (adjustedFreeSpace) * kMinFreeSpaceRatio);

      // Take the smallest of all limits
      int64_t finalLimit = std::min (baseLimit, freeSpaceLimit);
      finalLimit = std::min (finalLimit, adjustedFreeSpaceLimit);

      // Do not let the limit drop below 10 MB
      LUMEX_CONSTEXPR int64_t kMinLogSize = static_cast<int64_t> (10)
                                            * static_cast<int64_t> (1024)
                                            * static_cast<int64_t> (1024);
      finalLimit = std::max (finalLimit, kMinLogSize);

      return finalLimit;
    }
  catch (...)
    {
      // On error, return the base limit
      return isDirectory ? kMaxLogDirectorySizeBytes : kMaxLogFileSizeBytes;
    }
}

bool
LumexLogger::_cleanup_old_logs (std::string const &logPath,
                                int64_t targetSize) LUMEX_NOEXCEPT
{
  try
    {
      if (logPath.empty ())
        return false;

      // Check whether this is a file or a folder
      int64_t currentSize = get_file_size (logPath);
      if (currentSize >= 0)
        {
          // This is a file - just clear it
          if (currentSize > targetSize)
            {
              std::ofstream file;
              open_file_stream (file, logPath,
                                std::ios::out | std::ios::trunc);
              return file.good ();
            }
          return true;
        }

      // This is a folder - list files and delete the oldest
      std::vector<std::pair<std::string, std::time_t>> logFiles
          = _get_log_files_sorted_by_time (logPath);

      int64_t totalSize = get_directory_size (logPath);
      if (totalSize < 0)
        return false;

      for (auto const &filePair : logFiles)
        {
          if (totalSize <= targetSize)
            break;

          int64_t fileSize = get_file_size (filePair.first);
          if (fileSize > 0)
            {
              // Delete the file
              if (remove_file (filePair.first) == 0)
                totalSize -= fileSize;
            }
        }

      return totalSize <= targetSize;
    }
  catch (...)
    {
      return false;
    }
}

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay)
std::vector<std::pair<std::string, std::time_t>>
LumexLogger::_get_log_files_sorted_by_time (std::string const &directoryPath)
    LUMEX_NOEXCEPT
{
  std::vector<std::pair<std::string, std::time_t>> result;

  try
    {
      if (directoryPath.empty ())
        return result;

#if defined(_WIN32) || defined(_WIN64)
      // Windows: use FindFirstFile/FindNextFile
      std::wstring wideDirectory;
      std::wstring const searchPath
          = ::lumex::core::utility::utf8_file_name::to_wide (directoryPath,
                                                             wideDirectory)
                ? wideDirectory + L"\\*"
                : std::wstring ();

      WIN32_FIND_DATAW findData{};
      HANDLE hFind = searchPath.empty ()
                         ? INVALID_HANDLE_VALUE
                         : FindFirstFileW (searchPath.c_str (), &findData);

      if (hFind != INVALID_HANDLE_VALUE)
        {
          do
            {
              if (wcscmp (findData.cFileName, L".") != 0
                  && wcscmp (findData.cFileName, L"..") != 0)
                {
                  if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                      == 0U)
                    {
                      std::string fullPath
                          = directoryPath + "\\"
                            + ::lumex::core::utility::utf8_file_name::to_utf8 (
                                findData.cFileName);

                      // Get the file creation time
                      FILETIME creationTime = findData.ftCreationTime;
                      ULARGE_INTEGER timeValue{};
                      timeValue.LowPart = creationTime.dwLowDateTime;
                      timeValue.HighPart = creationTime.dwHighDateTime;

                      // Constants for converting FILETIME to time_t
                      LUMEX_CONSTEXPR uint64_t kFileTimeToSeconds
                          = 10000000ULL; // 100-nanosecond intervals to seconds
                      LUMEX_CONSTEXPR uint64_t kEpochOffset
                          = 11644473600ULL; // Seconds between 1601-01-01 and
                                            // 1970-01-01

                      auto fileTime = static_cast<std::time_t> (
                          (timeValue.QuadPart / kFileTimeToSeconds)
                          - kEpochOffset);
                      result.emplace_back (fullPath, fileTime);
                    }
                }
            }
          while (FindNextFileW (hFind, &findData) != 0);

          FindClose (hFind);
        }

#else
      // POSIX: use opendir/readdir
      DIR *dir = opendir (directoryPath.c_str ());
      if (dir != nullptr)
        {
          struct dirent *entry;
          while ((entry = readdir (dir)) != nullptr)
            {
              if (strcmp (entry->d_name, ".") != 0
                  && strcmp (entry->d_name, "..") != 0)
                {
                  std::string fullPath = directoryPath + "/" + entry->d_name;

                  struct stat fileStat{};
                  if (stat (fullPath.c_str (), &fileStat) == 0
                      && S_ISREG (fileStat.st_mode))
                    result.emplace_back (fullPath, fileStat.st_mtime);
                }
            }
          closedir (dir);
        }
#endif

      // Sort by creation time (oldest first)
      std::sort (result.begin (), result.end (),
                 [] (std::pair<std::string, std::time_t> const &firstFile,
                     std::pair<std::string, std::time_t> const &secondFile)
                   { return firstFile.second < secondFile.second; });
    }
  catch (...)
    {
      // On error, return an empty list
      return {};
    }

  return result;
}
// NOLINTEND(cppcoreguidelines-avoid-do-while,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay)

bool
LumexLogger::_can_write_log (std::size_t messageSize) const LUMEX_NOEXCEPT
{
  try
    {
      if (!logging_enabled.load ())
        return false;

      // Check the current limits
      if (!_check_log_size_limits ())
        return false;

      // Extra check: if the message is very large (> 1 MB),
      // check that enough space remains
      LUMEX_CONSTEXPR std::size_t kLargeMessageThreshold
          = static_cast<std::size_t> (1024)
            * static_cast<std::size_t> (1024); // 1 MB
      LUMEX_CONSTEXPR double kMaxMessageToFreeSpaceRatio
          = 0.1; // No more than 10% of free space

      if (messageSize > kLargeMessageThreshold)
        {
          int64_t freeSpace = get_free_disk_space (log_file_path);
          if (freeSpace >= 0
              && static_cast<int64_t> (messageSize)
                     > static_cast<int64_t> (static_cast<double> (freeSpace)
                                             * kMaxMessageToFreeSpaceRatio))
            {
              return false;
            }
        }

      return true;
    }
  catch (...)
    {
      return false;
    }
}

std::string
LumexLogger::_format_size (int64_t sizeInBytes) LUMEX_NOEXCEPT
{
  try
    {
      LUMEX_CONSTEXPR int64_t kKB = static_cast<int64_t> (1024);
      LUMEX_CONSTEXPR int64_t kMB = kKB * static_cast<int64_t> (1024);
      LUMEX_CONSTEXPR int64_t kGB = kMB * static_cast<int64_t> (1024);
      LUMEX_CONSTEXPR int64_t kTB = kGB * static_cast<int64_t> (1024);

      std::stringstream stringStream;
      stringStream << std::fixed << std::setprecision (2);

      if (sizeInBytes >= kTB)
        stringStream << (static_cast<double> (sizeInBytes) / kTB) << " TB";
      else if (sizeInBytes >= kGB)
        stringStream << (static_cast<double> (sizeInBytes) / kGB) << " GB";
      else if (sizeInBytes >= kMB)
        stringStream << (static_cast<double> (sizeInBytes) / kMB) << " MB";
      else if (sizeInBytes >= kKB)
        stringStream << (static_cast<double> (sizeInBytes) / kKB) << " KB";
      else
        stringStream << sizeInBytes << " bytes";

      return stringStream.str ();
    }
  catch (...)
    {
      return "Unknown size";
    }
}

void
LumexLogger::_flush_buffer ()
{
  // Lock already held by caller

  if (!log_file.is_open () || log_buffer.empty ())
    return;

  try
    {
      std::size_t const count = log_buffer.size ();

      // Write every buffered record to the file
      for (auto const &entry : log_buffer)
        {
          // Use the timestamp stored in the buffer
          auto raw_time
              = std::chrono::system_clock::to_time_t (entry.timestamp);
          auto ms_count
              = std::chrono::duration_cast<std::chrono::milliseconds> (
                    entry.timestamp.time_since_epoch ())
                % std::chrono::milliseconds::period::den;

          std::stringstream sstream;
          struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
          localtime_s (std::addressof (timeinfo), std::addressof (raw_time));
#else
          localtime_r (std::addressof (raw_time), std::addressof (timeinfo));
#endif
          sstream << std::put_time (std::addressof (timeinfo),
                                    "%Y-%m-%d %H:%M:%S");
          sstream << '.' << std::setfill ('0') << std::setw (3)
                  << ms_count.count ();

          log_file << "[" << sstream.str () << "] "
                   << "[" << _level_to_string (entry.level) << "] "
                   << entry.message << '\n';
        }

      log_file << "=======>>> Dumped " << count << " logs\n";

      // Clear the buffer after writing
      log_buffer.clear ();

      // Force-flush buffers to the file
      log_file.flush ();
    }
  catch (std::ios_base::failure const &e)
    {
      std::cerr << "Error writing buffer to log: " << e.what () << '\n';
      throw;
    }
}

std::string
LumexLogger::_extract_component_name (std::string const &message)
    LUMEX_NOEXCEPT
{
  try
    {
      if (message.empty ())
        return "";

      // Support "[ComponentName]: message" and "[ComponentName]
      // message" (logger macros emit "[FunctionName] text", without
      // a colon)
      std::size_t openBracket = message.find ('[');
      if (openBracket == std::string::npos)
        return "";

      std::size_t closeBracket = message.find (']', openBracket + 1);
      if (closeBracket == std::string::npos)
        return "";

      // After ] accept either ": " ("[Name]: text") or " " (form
      // "[Name] text")
      if (closeBracket + 1 >= message.length ())
        return "";
      char afterBracket = message[closeBracket + 1];
      if (afterBracket == ':')
        {
          if (closeBracket + 2 >= message.length ()
              || message[closeBracket + 2] != ' ')
            return "";
        }
      else if (afterBracket != ' ')
        {
          return "";
        }

      std::size_t componentLength = closeBracket - openBracket - 1;
      if (componentLength == 0)
        return "";

      return message.substr (openBracket + 1, componentLength);
    }
  catch (...)
    {
      return "";
    }
}

std::string
LumexLogger::_get_short_form_from_component (std::string const &component)
    LUMEX_NOEXCEPT
{
  try
    {
      if (component.empty ())
        return "";

      std::string s = component;
      // Strip trailing "()" for "Class::method()"
      if (s.size () >= 2 && s[s.size () - 2] == '(' && s[s.size () - 1] == ')')
        s = s.substr (0, s.size () - 2);

      std::size_t lastColonColon = s.rfind ("::");
      if (lastColonColon != std::string::npos
          && lastColonColon + 2 < s.size ())
        s = s.substr (lastColonColon + 2);

      // For a full signature, after "::" comes "WriteData(class ... &)" -
      // keep only the method name
      std::size_t openParen = s.find ('(');
      if (openParen != std::string::npos)
        s = s.substr (0, openParen);

      return s;
    }
  catch (...)
    {
      return "";
    }
}

std::string
LumexLogger::_get_normal_form_from_component (std::string const &component)
    LUMEX_NOEXCEPT
{
  try
    {
      if (component.empty ())
        return "";

      // Full signature: parentheses (arguments), namespace/class, and
      // a space (return type or __cdecl)
      bool looksLikeFullSignature
          = (component.find ('(') != std::string::npos)
            && (component.find ("::") != std::string::npos)
            && (component.find (' ') != std::string::npos);
      if (looksLikeFullSignature)
        return logger_format_function_name_clean (component.c_str ());

      return component;
    }
  catch (...)
    {
      return "";
    }
}

bool
LumexLogger::_preset_matches_component (
    std::string const &preset, std::string const &component) LUMEX_NOEXCEPT
{
  try
    {
      if (preset.empty () || component.empty ())
        return false;

      if (component == preset)
        return true;

      std::string const shortComp = _get_short_form_from_component (component);
      std::string const normalComp
          = _get_normal_form_from_component (component);

      std::string presetTrim = preset;
      if (presetTrim.size () >= 2 && presetTrim[presetTrim.size () - 2] == '('
          && presetTrim[presetTrim.size () - 1] == ')')
        presetTrim = presetTrim.substr (0, presetTrim.size () - 2);

      std::string normalTrim = normalComp;
      if (normalTrim.size () >= 2 && normalTrim[normalTrim.size () - 2] == '('
          && normalTrim[normalTrim.size () - 1] == ')')
        normalTrim = normalTrim.substr (0, normalTrim.size () - 2);

      if (shortComp == preset || shortComp == presetTrim)
        return true;
      if (normalComp == preset || normalTrim == presetTrim)
        return true;
      if (normalComp == preset + "()" || normalTrim == presetTrim + "()")
        return true;

      return false;
    }
  catch (...)
    {
      return false;
    }
}

bool
LumexLogger::_should_log_by_preset (std::string const &message) const
    LUMEX_NOEXCEPT
{
  try
    {
      if (!preset_enabled)
        return true;
      if (preset_components.empty ())
        return false;

      std::string componentName = _extract_component_name (message);
      if (componentName.empty ())
        return false;

      for (auto const &preset : preset_components)
        if (_preset_matches_component (preset, componentName))
          return true;
      return false;
    }
  catch (...)
    {
      return true;
    }
}
} // namespace logger
} // namespace logger
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

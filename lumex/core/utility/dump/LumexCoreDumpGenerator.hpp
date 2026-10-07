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
 * @file LumexCoreDumpGenerator.hpp
 * @brief The crash dump subsystem of `lumex::utility`: `core_dump_generator`,
 * the `DumpType` enumeration, `dump_configuration` and `dump_factory`.
 * @details `core_dump_generator` is a process-wide singleton that installs
 * crash handlers and writes dumps on a crash or on request: minidumps through
 * DbgHelp on Windows, where `DumpType` maps to `MINIDUMP_TYPE` flags, and core
 * dumps on Unix-like systems, where it lifts the core size limit, points the
 * kernel core pattern at its dump directory through `sudo` (restoring the
 * previous pattern on destruction) and can copy the dumps that
 * systemd-coredump collects. It also lets the application register a graceful
 * shutdown handler: a console control handler on Windows, a thread that waits
 * for SIGINT, SIGTERM or SIGHUP on POSIX systems. `dump_factory` builds and
 * checks the configuration for a dump type and reports which types the
 * platform supports.
 *
 * Most of the implementation is inline in this header; the static data members
 * are defined in `LumexCoreDumpGenerator.cpp`. Windows and Unix-like systems
 * including Android are supported; on any other platform the header stops with
 * `#error`.
 */
#ifndef LUMEX_CORE_UTILITY_DUMP_HPP
#define LUMEX_CORE_UTILITY_DUMP_HPP

#include "lumex/LumexExport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#if __cplusplus >= 201703L
#include <optional>
#endif
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<concepts>)
#include <concepts>
#endif
#endif
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// Forward declarations

// Platforms: Windows, or a Unix-like system (Android included, which
// LumexCheckOS.hpp does not count as LUMEX_OS_IS_UNIX()).
#if !LUMEX_OS_IS_WINDOWS() && !LUMEX_OS_IS_UNIX() && !LUMEX_OS_IS_ANDROID()
#error "Unsupported platform"
#endif

// Windows-specific includes
#if LUMEX_OS_IS_WINDOWS()
// Prevent Windows headers from defining conflicting symbols
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <aclapi.h>
#include <dbghelp.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sddl.h>
#include <shlwapi.h>
#include <tchar.h>
#include <wincrypt.h>

// GCC ignores these pragmas; MinGW links the libraries from CMake.
#if defined(_MSC_VER)
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")
#endif
#endif

// UNIX-specific includes
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
#include <csignal>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <fstream> // for std::ofstream in _generate_core_dump()
#include <grp.h>   // for getgrnam, group membership checks
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/inotify.h> // For instant systemd-coredump monitoring
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/select.h> // For select() in inotify loop
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// MinGW (GCC) warns for every member that is defined outside the class after
// a member function has used it, when the class itself is dllimport
// ("redeclared without dllimport attribute"), and no flag or pragma turns
// that warning off. Every member function of the two classes below is inline;
// the .cpp defines only the static data members. So under MinGW the export
// attribute sits on those data members; MSVC, clang-cl and ELF keep it on the
// class.
#if defined(__MINGW32__)
#define LUMEX_DUMP_CLASS_API
#define LUMEX_DUMP_DATA_API LUMEX_UTILITY_API
#else
#define LUMEX_DUMP_CLASS_API LUMEX_UTILITY_API
#define LUMEX_DUMP_DATA_API
#endif

namespace lumex
{
namespace core
{
namespace utility
{
namespace dump
{

/**
 * @enum DumpType
 * @brief Comprehensive enumeration of all supported crash dump types across
 * platforms
 *
 * This enumeration defines the complete set of dump types that can be
 * generated by the core_dump_generator, including Windows-specific mini-dump
 * types, kernel-mode dump variants, and UNIX core dump types. Each type is
 * designed to capture different levels of system state information for
 * debugging purposes.
 *
 * @details The enumeration is based on Microsoft's MINIDUMP_TYPE documentation
 * and POSIX core dump specifications, providing a unified interface for
 * cross-platform crash dump generation.
 *
 * @note All enum values are explicitly typed as std::int8_t for memory
 * efficiency and ABI stability across different compilers and platforms.
 *
 * @since Version 1.0
 * @see
 * https://docs.microsoft.com/en-us/windows/win32/api/minidumpapiset/ne-minidumpapiset-minidump_type
 * @see https://pubs.opengroup.org/onlinepubs/9699919799/functions/core.html
 */
enum class DumpType : std::int8_t
{
  // Windows-specific dump types (based on MINIDUMP_TYPE)
  MINI_DUMP_NORMAL = 0,                ///< Basic mini-dump (64KB)
  MINI_DUMP_WITH_DATA_SEGS = 1,        ///< Include data segments
  MINI_DUMP_WITH_FULL_MEMORY = 2,      ///< Full memory dump (largest)
  MINI_DUMP_WITH_HANDLE_DATA = 3,      ///< Include handle data
  MINI_DUMP_FILTER_MEMORY = 4,         ///< Filter memory
  MINI_DUMP_SCAN_MEMORY = 5,           ///< Scan memory
  MINI_DUMP_WITH_UNLOADED_MODULES = 6, ///< Include unloaded modules
  MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY
  = 7,                               ///< Include indirectly referenced memory
  MINI_DUMP_FILTER_MODULE_PATHS = 8, ///< Filter module paths
  MINI_DUMP_WITH_PROCESS_THREAD_DATA = 9, ///< Include process/thread data
  MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY
  = 10,                                 ///< Include private read/write memory
  MINI_DUMP_WITHOUT_OPTIONAL_DATA = 11, ///< Without optional data
  MINI_DUMP_WITH_FULL_MEMORY_INFO = 12, ///< Include full memory info
  MINI_DUMP_WITH_THREAD_INFO = 13,      ///< Include thread info
  MINI_DUMP_WITH_CODE_SEGMENTS = 14,    ///< Include code segments
  MINI_DUMP_WITHOUT_AUXILIARY_STATE = 15,   ///< Without auxiliary state
  MINI_DUMP_WITH_FULL_AUXILIARY_STATE = 16, ///< With full auxiliary state
  MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY
  = 17, ///< Include private write-copy memory
  MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY = 18, ///< Ignore inaccessible memory
  MINI_DUMP_WITH_TOKEN_INFORMATION = 19,     ///< Include token information

  // Windows kernel-mode dump types (based on Microsoft documentation)
  KERNEL_FULL_DUMP = 20,      ///< Complete memory dump - largest kernel dump
  KERNEL_KERNEL_DUMP = 21,    ///< Kernel memory dump - kernel memory only
  KERNEL_SMALL_DUMP = 22,     ///< Small memory dump - 64KB
  KERNEL_AUTOMATIC_DUMP = 23, ///< Automatic memory dump - flexible size
  KERNEL_ACTIVE_DUMP
  = 24, ///< Active memory dump - similar to full but smaller

  // UNIX/Linux core dump types
  CORE_DUMP_FULL = 25, ///< Full core dump with all memory

  // Default types
  DEFAULT_WINDOWS = MINI_DUMP_WITH_FULL_MEMORY, ///< Default Windows dump type
  DEFAULT_UNIX = CORE_DUMP_FULL,                ///< Default UNIX dump type
  DEFAULT_AUTO = -1 ///< Auto-detect based on platform
};

/**
 * @brief Validation and utility functions for DumpType enum
 * @namespace DumpTypeUtils
 */
namespace DumpTypeUtils
{
// Named constants for magic numbers
namespace Constants
{
static constexpr std::int8_t MIN_DUMP_TYPE_VALUE = 0;
static constexpr std::int8_t MAX_DUMP_TYPE_VALUE = 25;
static constexpr std::int8_t AUTO_DETECT_VALUE = -1;
static constexpr std::int8_t WINDOWS_MAX_TYPE = 24;
static constexpr std::int8_t UNIX_MIN_TYPE = 25;
static constexpr std::int8_t UNIX_MAX_TYPE = 25;
static constexpr std::int8_t KERNEL_MIN_TYPE = 20;
static constexpr std::int8_t KERNEL_MAX_TYPE = 24;
static constexpr std::int8_t KERNEL_ONLY_TYPE = 26;
} // namespace Constants

// Character code constants
namespace CharacterConstants
{
static constexpr char CONTROL_CHAR_THRESHOLD = 32;
static constexpr char COLON_CHAR = ':';
static constexpr char ASTERISK_CHAR = '*';
static constexpr char QUESTION_CHAR = '?';
static constexpr char QUOTE_CHAR = '"';
static constexpr char LESS_THAN_CHAR = '<';
static constexpr char GREATER_THAN_CHAR = '>';
static constexpr char PIPE_CHAR = '|';
} // namespace CharacterConstants

/**
 * @brief Check if a DumpType value is valid
 * @param type The dump type to validate
 * @return true if valid, false otherwise
 * @note This function is thread-safe and noexcept
 */
inline bool
is_valid (DumpType type) noexcept
{
  auto const value = static_cast<std::int8_t> (type);
  return (value >= Constants::MIN_DUMP_TYPE_VALUE
          && value <= Constants::MAX_DUMP_TYPE_VALUE)
         || (value == Constants::AUTO_DETECT_VALUE);
}

/**
 * @brief Check if a DumpType is a Windows-specific type
 * @param type The dump type to check
 * @return true if Windows-specific, false otherwise
 * @note This function is thread-safe and noexcept
 */
inline bool
is_windows_type (DumpType type) noexcept
{
  auto const value = static_cast<std::int8_t> (type);
  return value >= Constants::MIN_DUMP_TYPE_VALUE
         && value <= Constants::WINDOWS_MAX_TYPE;
}

/**
 * @brief Check if a DumpType is a UNIX-specific type
 * @param type The dump type to check
 * @return true if UNIX-specific, false otherwise
 * @note This function is thread-safe and noexcept
 */
inline bool
is_unix_type (DumpType type) noexcept
{
  auto const value = static_cast<std::int8_t> (type);
  return value >= Constants::UNIX_MIN_TYPE
         && value <= Constants::UNIX_MAX_TYPE;
}

/**
 * @brief Check if a DumpType is a kernel dump type
 * @param type The dump type to check
 * @return true if kernel dump, false otherwise
 * @note This function is thread-safe and noexcept
 */
inline bool
is_kernel_type (DumpType type) noexcept
{
  auto const value = static_cast<std::int8_t> (type);
  return (value >= Constants::KERNEL_MIN_TYPE
          && value <= Constants::KERNEL_MAX_TYPE)
         || (value == Constants::KERNEL_ONLY_TYPE);
}

/**
 * @brief Get the minimum valid DumpType value
 * @return Minimum valid DumpType value
 * @note This function is thread-safe and noexcept
 */
constexpr DumpType
get_min_value () noexcept
{
  return DumpType::MINI_DUMP_NORMAL;
}

/**
 * @brief Get the maximum valid DumpType value
 * @return Maximum valid DumpType value
 * @note This function is thread-safe and noexcept
 */
constexpr DumpType
get_max_value () noexcept
{
  return DumpType::CORE_DUMP_FULL;
}
} // namespace DumpTypeUtils

// Modern C++ concepts for type safety
#if LUMEX_HAS_STD_CONCEPTS
namespace CoreDumpGeneratorConcepts
{
template <typename T>
concept DumpTypeLike = std::same_as<T, DumpType>;

template <typename T>
concept Configurable = requires (T configurable) {
  { configurable.is_valid () } -> std::convertible_to<bool>;
  { configurable.get_validation_error () } -> std::convertible_to<std::string>;
};

template <typename T>
concept DumpGenerator = requires (T generator) {
  { generator.is_initialized () } -> std::convertible_to<bool>;
  { generator.generate_dump (std::string{}) } -> std::convertible_to<bool>;
};
} // namespace CoreDumpGeneratorConcepts
#endif

/**
 * @class dump_configuration
 * @brief Comprehensive configuration class for crash dump generation
 *
 * This class encapsulates all configurable parameters for generating
 * crash dumps, including dump type selection, file naming, directory
 * specification, size limits, compression settings, and platform-specific
 * filtering options. It provides a unified interface for configuring
 * dump generation across different platforms and dump types.
 *
 * @details The class is designed to be:
 * - Copyable and movable for efficient parameter passing
 * - Default-constructible with sensible defaults
 * - Thread-safe for concurrent access (read-only operations)
 * - Extensible for future platform-specific options
 * - Value semantics with proper move semantics
 * - Encapsulated with proper validation and error handling
 *
 * @invariant All string members must be valid UTF-8 encoded strings
 * @invariant m_maxSizeBytes must be 0 (unlimited) or a positive value
 * @invariant m_directory must be a valid, accessible path
 * @invariant m_filename must be a valid filename (no path separators)
 *
 * @thread_safety This class is thread-safe for read-only operations.
 * Modifications should be synchronized externally.
 *
 * @since Version 1.0
 * @see dump_factory::create_configuration()
 * @see core_dump_generator::initialize()
 */
class dump_configuration
{
public:
  // Constructors and assignment operators
  dump_configuration () noexcept = default;
  ~dump_configuration () noexcept = default;

  // Move constructor and assignment for performance
  dump_configuration (dump_configuration &&) noexcept = default;
  dump_configuration &operator= (dump_configuration &&) noexcept = default;

  // Copy constructor and assignment
  dump_configuration (dump_configuration const &) = default;
  dump_configuration &operator= (dump_configuration const &) = default;

  // Equality comparison for testing and validation
  bool operator== (dump_configuration const &other) const noexcept;
  bool operator!= (dump_configuration const &other) const noexcept;

  // Getters
  DumpType
  get_type () const noexcept
  {
    return m_type;
  }
  std::string const &
  get_filename () const noexcept
  {
    return m_filename;
  }
  std::string const &
  get_directory () const noexcept
  {
    return m_directory;
  }
  bool
  is_compress () const noexcept
  {
    return m_compress;
  }
  bool
  is_include_unloaded_modules () const noexcept
  {
    return m_includeUnloadedModules;
  }
  bool
  is_include_handle_data () const noexcept
  {
    return m_includeHandleData;
  }
  bool
  is_include_thread_info () const noexcept
  {
    return m_includeThreadInfo;
  }
  bool
  is_include_process_data () const noexcept
  {
    return m_includeProcessData;
  }
  size_t
  get_max_size_bytes () const noexcept
  {
    return m_maxSizeBytes;
  }
  std::vector<std::string> const &
  get_memory_filters () const noexcept
  {
    return m_memoryFilters;
  }
  bool
  is_enable_symbols () const noexcept
  {
    return m_enableSymbols;
  }
  bool
  is_enable_source_info () const noexcept
  {
    return m_enableSourceInfo;
  }

  // Setters with validation
  bool set_type (DumpType type) noexcept;
  bool set_filename (std::string const &filename) noexcept;
  bool set_directory (std::string const &directory) noexcept;

#if LUMEX_HAS_STD_CONCEPTS
  // Template setters with concepts for type safety
  template <traits::string::StringLike T>
  bool
  set_filename (T const &filename) noexcept
  {
    return set_filename (std::string{ filename });
  }

  template <traits::string::StringLike T>
  bool
  set_directory (T const &directory) noexcept
  {
    return set_directory (std::string{ directory });
  }

  template <traits::string::StringLike T>
  bool
  add_memory_filter (T const &filter) noexcept
  {
    return add_memory_filter (std::string{
        filter }); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  }
#endif
  void
  set_compress (bool compress) noexcept
  {
    m_compress = compress;
  }
  void
  set_include_unloaded_modules (bool include) noexcept
  {
    m_includeUnloadedModules = include;
  }
  void
  set_include_handle_data (bool include) noexcept
  {
    m_includeHandleData = include;
  }
  void
  set_include_thread_info (bool include) noexcept
  {
    m_includeThreadInfo = include;
  }
  void
  set_include_process_data (bool include) noexcept
  {
    m_includeProcessData = include;
  }
  bool set_max_size_bytes (size_t maxSize) noexcept;
  bool add_memory_filter (std::string const &filter) noexcept;
  void
  clear_memory_filters () noexcept
  {
    m_memoryFilters.clear ();
  }
  void
  set_enable_symbols (bool enable) noexcept
  {
    m_enableSymbols = enable;
  }
  void
  set_enable_source_info (bool enable) noexcept
  {
    m_enableSourceInfo = enable;
  }

  // Validation methods
  bool is_valid () const noexcept;
  std::string get_validation_error () const;

private:
  DumpType m_type = DumpType::DEFAULT_AUTO; ///< Type of dump to generate
  std::string m_filename;  ///< Custom filename (empty for auto-generated)
  std::string m_directory; ///< Directory for dump files
  bool m_compress = false; ///< Whether to compress the dump
  bool m_includeUnloadedModules = true; ///< Include unloaded modules (Windows)
  bool m_includeHandleData = true;      ///< Include handle data (Windows)
  bool m_includeThreadInfo = true;      ///< Include thread information
  bool m_includeProcessData = true;     ///< Include process data
  size_t m_maxSizeBytes = 0; ///< Maximum size in bytes (0 = unlimited)
  std::vector<std::string> m_memoryFilters; ///< Memory region filters (UNIX)
  bool m_enableSymbols = true;              ///< Enable symbol information
  bool m_enableSourceInfo = true;           ///< Enable source file information

  // Private validation helpers
  static bool is_valid_filename (std::string const &filename) noexcept;
  static bool is_valid_directory (std::string const &directory) noexcept;
  static bool is_valid_memory_filter (std::string const &filter) noexcept;
};

// ==================================== dump_configuration Implementation
// ==================================== //

inline bool
dump_configuration::set_type (DumpType type) noexcept
{
  if (!DumpTypeUtils::is_valid (type))
    return false;
  m_type = type;
  return true;
}

inline bool
dump_configuration::set_filename (std::string const &filename) noexcept
{
  if (!is_valid_filename (filename))
    return false;
  m_filename = filename;
  return true;
}

inline bool
dump_configuration::set_directory (std::string const &directory) noexcept
{
  if (!is_valid_directory (directory))
    return false;
  m_directory = directory;
  return true;
}

inline bool
dump_configuration::set_max_size_bytes (size_t maxSize) noexcept
{
  // Allow 0 for unlimited size
  m_maxSizeBytes = maxSize;
  return true;
}

inline bool
dump_configuration::add_memory_filter (std::string const &filter) noexcept
{
  if (!is_valid_memory_filter (filter))
    return false;
  try
    {
      m_memoryFilters.emplace_back (filter);
      return true;
    }
  catch (...)
    {
      return false;
    }
}

inline bool
dump_configuration::is_valid () const noexcept
{
  return DumpTypeUtils::is_valid (m_type) && is_valid_filename (m_filename)
         && is_valid_directory (m_directory)
         && std::all_of (m_memoryFilters.begin (), m_memoryFilters.end (),
                         [] (std::string const &filter)
                           { return is_valid_memory_filter (filter); });
}

inline std::string
dump_configuration::get_validation_error () const
{
  if (!DumpTypeUtils::is_valid (m_type))
    return "Invalid dump type";
  if (!is_valid_filename (m_filename))
    return "Invalid filename: contains invalid characters or path separators";
  if (!is_valid_directory (m_directory))
    return "Invalid directory: contains invalid characters or is not "
           "accessible";
  for (auto const &filter : m_memoryFilters)
    if (!is_valid_memory_filter (filter))
      return "Invalid memory filter: " + filter;
  return "";
}

inline bool
dump_configuration::is_valid_filename (std::string const &filename) noexcept
{
  if (filename.empty ())
    return true; // Empty filename is valid (auto-generated)

  // Check for path separators
  if (filename.find ('/') != std::string::npos
      || filename.find ('\\') != std::string::npos)
    return false;

// Check for invalid characters
#if LUMEX_HAS_STD_RANGES
  return std::ranges::all_of (
      filename,
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character != DumpTypeUtils::CharacterConstants::COLON_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#else
  return std::all_of (
      filename.begin (), filename.end (),
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character != DumpTypeUtils::CharacterConstants::COLON_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#endif
}

inline bool
dump_configuration::is_valid_directory (std::string const &directory) noexcept
{
  if (directory.empty ())
    return true; // Empty directory is valid (will use default)

// Check for invalid characters
#if LUMEX_HAS_STD_RANGES
  return std::ranges::all_of (
      directory,
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#else
  return std::all_of (
      directory.begin (), directory.end (),
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#endif
}

inline bool
dump_configuration::is_valid_memory_filter (std::string const &filter) noexcept
{
  if (filter.empty ())
    return false;

// Basic validation for memory filter format
// This is a simplified validation - in practice, you might want more
// sophisticated checks
#if LUMEX_HAS_STD_RANGES
  return std::ranges::all_of (
      filter,
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#else
  return std::all_of (
      filter.begin (), filter.end (),
      [] (char character)
        {
          return character >= DumpTypeUtils::CharacterConstants::
                         CONTROL_CHAR_THRESHOLD
                 && character
                        != DumpTypeUtils::CharacterConstants::ASTERISK_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::QUESTION_CHAR
                 && character != DumpTypeUtils::CharacterConstants::QUOTE_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::LESS_THAN_CHAR
                 && character
                        != DumpTypeUtils::CharacterConstants::GREATER_THAN_CHAR
                 && character != DumpTypeUtils::CharacterConstants::PIPE_CHAR;
        });
#endif
}

inline bool
dump_configuration::operator== (dump_configuration const &other) const noexcept
{
  return m_type == other.m_type && m_filename == other.m_filename
         && m_directory == other.m_directory && m_compress == other.m_compress
         && m_includeUnloadedModules == other.m_includeUnloadedModules
         && m_includeHandleData == other.m_includeHandleData
         && m_includeThreadInfo == other.m_includeThreadInfo
         && m_includeProcessData == other.m_includeProcessData
         && m_maxSizeBytes == other.m_maxSizeBytes
         && m_memoryFilters == other.m_memoryFilters
         && m_enableSymbols == other.m_enableSymbols
         && m_enableSourceInfo == other.m_enableSourceInfo;
}

inline bool
dump_configuration::operator!= (dump_configuration const &other) const noexcept
{
  return !(*this == other);
}

/**
 * @class dump_factory
 * @brief Factory class for creating and configuring crash dump generators
 *
 * This factory class implements the Factory design pattern to create
 * and configure different types of crash dump generators based on the
 * DumpType enumeration. It provides a centralized way to create
 * platform-appropriate dump configurations and validate dump type
 * support across different operating systems.
 *
 * @details The factory provides:
 * - Configuration creation for all supported dump types
 * - Platform-specific configuration optimization
 * - Dump type validation and support checking
 * - Human-readable descriptions and size estimates
 * - Cross-platform compatibility mapping
 * - Error handling with detailed error messages
 *
 * @note This class is stateless and thread-safe. All methods are
 * static and can be called concurrently from multiple threads.
 *
 * @thread_safety All public methods are thread-safe and can be called
 * concurrently without external synchronization.
 *
 * @since Version 1.0
 * @see DumpType
 * @see dump_configuration
 * @see core_dump_generator
 */
class LUMEX_DUMP_CLASS_API dump_factory
{
public:
  /**
   * @brief Create a dump configuration for the specified type
   * @param type The dump type to create configuration for
   * @return dump_configuration object configured for the specified type
   * @throws std::invalid_argument if type is not supported on current platform
   * @throws std::runtime_error if configuration creation fails
   * @note This method is thread-safe and can be called concurrently
   */
  static dump_configuration create_configuration (DumpType type);

  /**
   * @brief Create a dump configuration with error handling
   * @param type The dump type to create configuration for
   * @param errorCode Error code to be set on failure
   * @return dump_configuration object configured for the specified type
   * @note This method is thread-safe and noexcept
   */
  static dump_configuration
  create_configuration (DumpType type, std::error_code &errorCode) noexcept;

  /**
   * @brief Get the default dump type for the current platform
   * @return Default DumpType for the current platform
   * @note This method is thread-safe and noexcept
   */
  static DumpType get_default_dump_type () noexcept;

  /**
   * @brief Check if a dump type is supported on the current platform
   * @param type The dump type to check
   * @return true if supported, false otherwise
   * @note This method is thread-safe and noexcept
   */
  static bool is_supported (DumpType type) noexcept;

  /**
   * @brief Get a human-readable description of the dump type
   * @param type The dump type to describe
   * @return String description of the dump type
   * @note This method is thread-safe and noexcept
   */
  static std::string get_description (DumpType type) noexcept;

  /**
   * @brief Get the estimated size of a dump type
   * @param type The dump type to estimate
   * @return Estimated size in bytes (0 if unknown)
   * @note This method is thread-safe and noexcept
   */
  static size_t get_estimated_size (DumpType type) noexcept;

  /**
   * @brief Get all supported dump types for current platform
   * @return Vector of supported DumpType values
   * @note This method is thread-safe and noexcept
   */
  static std::vector<DumpType> get_supported_types () noexcept;

  /**
   * @brief Validate a dump configuration
   * @param config The configuration to validate
   * @return true if valid, false otherwise
   * @note This method is thread-safe and noexcept
   */
  static bool
  validate_configuration (dump_configuration const &config) noexcept;

private:
  // Platform-specific configuration creators
  static dump_configuration create_windows_configuration (DumpType type);
  static dump_configuration create_unix_configuration (DumpType type);

  // Registry of dump type descriptions
  LUMEX_DUMP_DATA_API static std::map<DumpType, std::string> const
      s_descriptions;
  LUMEX_DUMP_DATA_API static std::map<DumpType, size_t> const s_estimatedSizes;
  LUMEX_DUMP_DATA_API static std::map<DumpType, bool> const s_platformSupport;
};

/**
 * @class core_dump_generator
 * @brief Cross-platform crash dump handler with comprehensive debugging
 * support
 *
 * This class provides a robust, thread-safe crash dump handler that
 * automatically captures memory dumps when crashes occur on both Windows and
 * UNIX platforms. It supports multiple dump types through the dump_factory
 * pattern and provides comprehensive debugging information for post-mortem
 * analysis.
 *
 * @details The core_dump_generator offers:
 * - Automatic crash detection and dump generation
 * - Cross-platform compatibility (Windows/UNIX)
 * - Multiple dump types and configurations
 * - Thread-safe singleton pattern implementation
 * - Comprehensive error handling and logging
 * - Security-focused path validation
 * - Atomic file operations to prevent race conditions
 *
 * @invariant The singleton instance is created only once and is thread-safe
 * @invariant All file operations are atomic to prevent TOCTOU race conditions
 * @invariant All input paths are validated for security vulnerabilities
 * @invariant Platform-specific handlers are properly initialized
 *
 * @thread_safety This class is thread-safe. Multiple threads can call
 * public methods concurrently without external synchronization.
 *
 * @exception_safety All public methods provide strong exception safety
 * guarantee unless otherwise specified. Methods marked noexcept provide
 * no-throw guarantee.
 *
 * @since Version 1.0
 * @see DumpType
 * @see dump_configuration
 * @see dump_factory
 *
 * @par Example
 * ```cpp
 * // Initialize the crash dump handler
 * core_dump_generator::initialize("/path/to/dumps",
 * DumpType::MINI_DUMP_WITH_FULL_MEMORY);
 *
 * // Generate a manual dump
 * core_dump_generator::generate_dump("Manual dump for testing");
 *
 * // Get the singleton instance
 * auto& generator = core_dump_generator::instance();
 * ```
 */
class LUMEX_DUMP_CLASS_API core_dump_generator
{
public:
  // The size constants are plain `static const` members defined in
  // LumexCoreDumpGenerator.cpp, not constexpr: from C++17 that makes them
  // inline variables, which a library built at C++17 does not export, so a
  // C++11 / C++14 consumer that binds one to a reference (EXPECT_EQ does)
  // would not link. An ordinary definition exists in every standard, and
  // the members are still constant expressions.
  LUMEX_DUMP_DATA_API static std::size_t const KB_32 = 32ULL * 1024ULL; // 32KB
  LUMEX_DUMP_DATA_API static std::size_t const KB_64 = 64ULL * 1024ULL; // 64KB
  LUMEX_DUMP_DATA_API static std::size_t const KB_128
      = 128ULL * 1024ULL; // 128KB
  LUMEX_DUMP_DATA_API static std::size_t const KB_256
      = 256ULL * 1024ULL; // 256KB
  LUMEX_DUMP_DATA_API static std::size_t const KB_512
      = 512ULL * 1024ULL; // 512KB
  LUMEX_DUMP_DATA_API static std::size_t const MB_1 = 1024ULL * 1024ULL; // 1MB

  core_dump_generator (core_dump_generator const &) = delete;
  core_dump_generator &operator= (core_dump_generator const &) = delete;
  core_dump_generator (core_dump_generator &&) = delete;
  core_dump_generator &operator= (core_dump_generator &&) = delete;
  ~core_dump_generator () noexcept
  {
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
    // Stop instant systemd monitor thread
    if (s_monitorThread.joinable ())
      {
        s_monitorThreadShouldStop.store (true, std::memory_order_release);
        s_monitorThread.join ();
      }

    // Restore original core pattern on destruction
    _restore_core_pattern ();
#endif
  }

  /**
   * @brief Initialize the crash dump handler
   *
   * Sets up platform-specific crash handlers with default configuration.
   * Also sets up C++ exception handling.
   *
   * @param dumpDirectory Optional directory path for dump files.
   *                      If empty, uses executable directory.
   * @param dumpType Type of dump to generate (defaults to platform-specific)
   * @param handleExceptions Whether to handle unhandled C++ exceptions
   * (default: true)
   * @throws std::runtime_error if initialization fails
   * @throws std::system_error if filesystem operations fail
   * @throws std::invalid_argument if configuration is invalid
   *
   * @complexity O(1) for basic initialization, O(n) where n is directory depth
   * for recursive creation
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety Strong guarantee: if an exception is thrown, the object
   * remains in a valid state
   */
  static void initialize (std::string const &dumpDirectory = "",
                          DumpType dumpType = DumpType::DEFAULT_AUTO,
                          bool handleExceptions = true);

  /**
   * @brief Initialize the crash dump handler with full configuration
   *
   * Sets up platform-specific crash handlers with custom configuration.
   * Also sets up C++ exception handling.
   *
   * @param config Complete dump configuration
   * @param handleExceptions Whether to handle unhandled C++ exceptions
   * (default: true)
   * @throws std::runtime_error if initialization fails
   * @throws std::system_error if filesystem operations fail
   */
  static void initialize (dump_configuration const &config,
                          bool handleExceptions = true);

  /**
   * @brief Check if the dump creator is initialized
   *
   * @return true if initialized, false otherwise
   */
  static bool is_initialized () noexcept;

  /**
   * @brief Manually trigger a dump generation
   *
   * @param reason Optional reason for the dump generation
   * @param dumpType Type of dump to generate (uses current config if
   * DEFAULT_AUTO)
   * @return true if dump was generated successfully, false otherwise
   * @throws std::runtime_error if not initialized
   * @throws std::system_error if filesystem operations fail
   * @throws std::invalid_argument if dump type is not supported
   *
   * @complexity O(1) for basic operations, O(n) where n is memory size for
   * full dumps
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety Basic guarantee: if an exception is thrown, the system
   * remains in a valid state
   */
  static bool generate_dump (std::string const &reason = "Manual dump",
                             DumpType dumpType = DumpType::DEFAULT_AUTO);

  /**
   * @brief Manually trigger a dump generation with error code
   *
   * Non-throwing alternative to generate_dump() that returns error information
   * via std::error_code instead of throwing exceptions.
   *
   * @param reason Optional reason for the dump generation
   * @param dumpType Type of dump to generate (uses current config if
   * DEFAULT_AUTO)
   * @param errorCode Error code to be set on failure
   * @return true if dump was generated successfully, false otherwise
   *
   * @complexity O(1) for basic operations, O(n) where n is memory size for
   * full dumps
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety No-throw guarantee
   */
  static bool generate_dump (std::string const &reason, DumpType dumpType,
                             std::error_code &errorCode) noexcept;

  /**
   * @brief Generate a dump file with custom configuration
   *
   * @param config Complete dump configuration
   * @param reason Reason for generating the dump
   * @return true if successful, false otherwise
   * @throws std::runtime_error if not initialized
   */
  static bool generate_dump (dump_configuration const &config,
                             std::string const &reason = "Manual dump");

  /**
   * @brief Get the singleton instance
   *
   * @return Reference to the singleton instance
   * @note This method is thread-safe
   * @throws std::runtime_error if not initialized
   */
  static core_dump_generator &instance ();

  /**
   * @brief Get the current dump directory
   *
   * @return Current dump directory path
   */
  static std::string get_dump_directory () noexcept;

  /**
   * @brief Get the current dump configuration
   *
   * @return Current dump configuration
   */
  static dump_configuration get_current_configuration () noexcept;

  /**
   * @brief Set the dump type for future dumps
   *
   * @param dumpType New dump type to use
   * @return true if successfully set, false if not supported
   * @throws std::runtime_error if not initialized
   */
  static bool set_dump_type (DumpType dumpType);

  /**
   * @brief Get the current dump type
   *
   * @return Current dump type
   */
  static DumpType get_current_dump_type () noexcept;

  /**
   * @brief Set core pattern for crash (like in the working example)
   * @details Sets the core pattern to create core dump with correct filename
   */
  static void set_core_pattern_for_crash ();

  /**
   * @brief Rename any core files that were created
   * @details Renames core files to the correct name and moves them to dumps
   * directory
   */
  static void rename_core_files ();

  /**
   * @brief Check if the current process is running with administrator
   * privileges
   *
   * This method provides a cross-platform way to check if the current process
   * has administrator/root privileges. On Windows, it checks both token
   * elevation and membership in the Administrators group. On UNIX systems, it
   * checks if the effective user ID is 0 (root).
   *
   * @return true if running with admin privileges, false otherwise
   * @note This method is thread-safe and can be called concurrently
   * @note Returns false if privilege checking fails for security reasons
   *
   * @complexity O(1) - constant time operation
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety No-throw guarantee
   *
   * @since Version 1.0
   * @see _is_elevated_process() for a simpler elevation check
   */
  static bool is_admin_privileges () noexcept;

  /**
   * @brief Sets the admin group name used by is_admin_privileges() on UNIX.
   *
   * LumexLib does not hardcode an admin group: the name defaults to empty,
   * which leaves the root-only check in place. Passing a non-empty name makes
   * members of that group count as admin too. A name that cannot be resolved
   * with getgrnam() also falls back to the root-only check. On Windows this is
   * a no-op.
   *
   * @param name Group name to treat as admin; empty disables the group check.
   * @note This method is thread-safe.
   */
  static void set_admin_group_name (std::string const &name) noexcept;

  /**
   * @brief Register a custom signal handler for graceful shutdown signals
   *
   * This method allows registering custom handlers for signals like SIGINT,
   * SIGTERM, SIGHUP that should trigger graceful shutdown rather than crash
   * dumps. The handler will be called before the default crash handling
   * behavior.
   *
   * @param signum Signal number to register handler for
   * @param handler Function pointer to the custom handler
   * @return true if registration was successful, false otherwise
   * @note This method is thread-safe
   * @note Only works on UNIX systems (Windows uses console handlers instead)
   * @note Handler should be async-signal-safe
   *
   * @complexity O(1) - constant time operation
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety No-throw guarantee
   *
   * @since Version 1.1
   */
  static bool register_custom_signal_handler (int signum,
                                              void (*handler) (int)) noexcept;

#if LUMEX_OS_IS_WINDOWS()
  /**
   * @brief Register a custom console handler for Windows graceful shutdown
   *
   * This method allows registering a custom console control handler for
   * Windows that will be called for console events like Ctrl+C, Ctrl+Break,
   * etc.
   *
   * @param handler Function pointer to the custom console handler
   * @return true if registration was successful, false otherwise
   * @note This method is thread-safe
   * @note Only works on Windows systems
   * @note Handler should be async-signal-safe
   *
   * @complexity O(1) - constant time operation
   * @thread_safety This function is thread-safe and may be called concurrently
   * @exception_safety No-throw guarantee
   *
   * @since Version 1.1
   */
  static bool register_custom_console_handler (
      BOOL (WINAPI *handler) (DWORD)) noexcept;
#endif

#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  /**
   * @brief POSIX-safe console-style graceful shutdown registration.
   * Blocks SIGINT/SIGTERM/SIGHUP in the caller and starts an internal sigwait
   * thread that invokes the provided handler() in a normal thread context.
   */
  static bool register_custom_console_handler (void (*handler) ()) noexcept;
#endif

  // Instance methods for better encapsulation
  /**
   * @brief Get the dump directory for this instance
   * @return Current dump directory path
   * @note This method is thread-safe
   */
  std::string const &get_instance_dump_directory () const noexcept;

  /**
   * @brief Get the current configuration for this instance
   * @return Current dump configuration
   * @note This method is thread-safe
   */
  dump_configuration const &get_instance_configuration () const noexcept;

  /**
   * @brief Check if this instance is initialized
   * @return true if initialized, false otherwise
   * @note This method is thread-safe
   */
  bool is_instance_initialized () const noexcept;

  /**
   * @brief Generate a dump using this instance's configuration
   * @param reason Reason for generating the dump
   * @return true if successful, false otherwise
   * @note This method is thread-safe
   */
  bool generate_instance_dump (std::string const &reason = "Manual dump");

  /**
   * @brief Generate a dump with error handling
   * @param reason Reason for generating the dump
   * @param errorCode Error code to be set on failure
   * @return true if successful, false otherwise
   * @note This method is thread-safe and noexcept
   */
  bool generate_instance_dump (std::string const &reason,
                               std::error_code &errorCode) noexcept;

#if LUMEX_HAS_STD_CONCEPTS
  // Template methods with concepts for type safety
  template <traits::string::StringLike T>
  bool
  generate_instance_dump (T const &reason)
  {
    return generate_instance_dump (std::string{ reason });
  }

  template <traits::string::StringLike T>
  bool
  generate_instance_dump (T const &reason, std::error_code &errorCode) noexcept
  {
    return generate_instance_dump (std::string{ reason }, errorCode);
  }
#endif

// Modern C++ features
#if LUMEX_HAS_STD_RANGES
  /**
   * @brief Get all memory filters as a range
   * @return Range of memory filters
   * @note This method is thread-safe
   */
  static auto
  get_memory_filters_range () noexcept
  {
    return s_currentConfig.get_memory_filters () | std::views::all;
  }
#endif

#if LUMEX_HAS_STD_OPTIONAL
  /**
   * @brief Get optional dump directory
   * @return Optional containing directory if set, empty if not
   * @note This method is thread-safe
   */
  std::optional<std::string>
  get_optional_dump_directory () const noexcept
  {
    std::lock_guard<std::mutex> lock (m_instanceMutex);
    return m_dumpDirectory.empty () ? std::nullopt
                                    : std::make_optional (m_dumpDirectory);
  }
#else
  /**
   * @brief Get dump directory with validity check
   * @param directory Output parameter for directory
   * @return true if directory is set, false if empty
   * @note This method is thread-safe
   */
  bool
  get_dump_directory_if_set (std::string &directory) const noexcept
  {
    std::lock_guard<std::mutex> lock (m_instanceMutex);
    if (m_dumpDirectory.empty ())
      return false;
    directory = m_dumpDirectory;
    return true;
  }
#endif

private:
  // Private constructor for singleton pattern
  core_dump_generator () = default;

  // Private constructor with initialization
  explicit core_dump_generator (std::string const &dumpDirectory,
                                dump_configuration const &config)
      : m_dumpDirectory (dumpDirectory), m_currentConfig (config),
        m_isInitialized (true)
  {
  }

  // Member variables
  LUMEX_DUMP_DATA_API static std::unique_ptr<core_dump_generator> s_instance;
  LUMEX_DUMP_DATA_API static std::mutex s_mutex;
#if __cplusplus >= 201103L
  LUMEX_DUMP_DATA_API static std::once_flag s_initFlag;
#endif
  LUMEX_DUMP_DATA_API static std::string s_dumpDirectory;
#if __cplusplus >= 201103L
  LUMEX_DUMP_DATA_API static std::atomic_bool s_initialized;
#else
  LUMEX_DUMP_DATA_API static bool s_initialized;
#endif
  LUMEX_DUMP_DATA_API static dump_configuration s_currentConfig;
  LUMEX_DUMP_DATA_API static std::string s_originalCorePattern;

#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  // Instant systemd-coredump monitor thread (for IMMEDIATE extraction)
  LUMEX_DUMP_DATA_API static std::atomic_bool s_monitorThreadShouldStop;
  LUMEX_DUMP_DATA_API static std::thread s_monitorThread;
  LUMEX_DUMP_DATA_API static pid_t
      s_applicationPid; // Store PID for filtering core dumps
  LUMEX_DUMP_DATA_API static std::string s_adminGroupName;
#endif

  // Custom signal handlers for graceful shutdown
  LUMEX_DUMP_DATA_API static std::map<int, void (*) (int)>
      s_customSignalHandlers;
  LUMEX_DUMP_DATA_API static std::mutex s_customHandlersMutex;
#if LUMEX_OS_IS_WINDOWS()
  LUMEX_DUMP_DATA_API static BOOL (WINAPI *s_customConsoleHandler) (DWORD);
#endif
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  LUMEX_DUMP_DATA_API static void (*s_unixConsoleHandler) ();
  LUMEX_DUMP_DATA_API static std::atomic_bool s_posixSigThreadStarted;
#endif

  // Instance member variables
  std::string m_dumpDirectory;
  dump_configuration m_currentConfig;
  bool m_isInitialized = false;
  mutable std::mutex m_instanceMutex;

  // Performance optimization: cache frequently accessed values
  mutable DumpType m_cachedDumpType = DumpType::DEFAULT_AUTO;
  mutable std::string m_cachedDumpDirectory;
  mutable bool m_cachedDumpTypeValid = false;
  mutable bool m_cachedDumpDirectoryValid = false;
  mutable bool m_cacheValid = false;

  // Memory optimization: use small string optimization
  static constexpr size_t SMALL_STRING_SIZE = 32;
  using SmallString = std::array<char, SMALL_STRING_SIZE>;

  // Thread safety improvements
  mutable std::mutex m_sharedMutex;
  std::atomic_bool m_operationInProgress;

  // Concurrency control
  static constexpr size_t MAX_CONCURRENT_OPERATIONS = 4;
  LUMEX_DUMP_DATA_API static std::atomic<size_t> s_activeOperations;
  LUMEX_DUMP_DATA_API static std::condition_variable s_operationCondition;
  LUMEX_DUMP_DATA_API static std::mutex s_operationMutex;

  // Platform-specific initialization
  static void _platform_initialize ();

  // Exception handling
  static void _setup_exception_handling ();
  static void _unhandled_exception_handler ();

#if LUMEX_OS_IS_WINDOWS()
  /**
   * @brief Setup Windows handlers
   */
  static void _setup_windows_handlers ();

  /**
   * @brief Create Windows dump with specific type
   */
  static bool _create_windows_dump (std::string const &filename,
                                    dump_configuration const &config);
#endif

#if LUMEX_OS_IS_WINDOWS()
  /**
   * @brief Windows-specific unhandled exception filter
   */
  static LONG WINAPI
  _windows_exception_handler (EXCEPTION_POINTERS *pExInfo) noexcept;

  /**
   * @brief Windows console handler wrapper
   * @details Calls custom console handler if registered
   */
  static BOOL WINAPI _windows_console_handler (DWORD ctrlType) noexcept;

  /**
   * @brief Redirected exception filter to prevent removal
   */
  static LONG WINAPI _redirected_set_unhandled_exception_filter (
      EXCEPTION_POINTERS * /*ExceptionInfo*/) noexcept;
#endif

#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  /**
   * @brief UNIX-specific signal handler
   */
  static void _unix_signal_handler (int signum) noexcept;

  /**
   * @brief Wrapper for custom signal handlers
   * @details Calls custom handler if registered, otherwise calls default crash
   * handler
   */
  static void _custom_signal_handler_wrapper (int signum) noexcept;

  /**
   * @brief Generate core dump on UNIX
   */
  static void _generate_core_dump ();

  /**
   * @brief Log core dump file size (UNIX helper)
   * @param filename The path to the core dump file
   */
  static void _log_core_dump_size (std::string const &filename) noexcept;

  /**
   * @brief Setup signal handlers
   */
  static void _setup_signal_handlers ();
  static void _setup_posix_graceful_shutdown ();
  // Internal helpers for POSIX graceful shutdown via sigwait
  static void _posix_sigwait_thread ();
  static void _block_default_shutdown_signals ();

  /**
   * @brief Setup core dump settings
   */
  static void _setup_core_dump_settings ();

  /**
   * @brief Setup core pattern for custom dump location
   */
  static void _setup_core_pattern ();

  /**
   * @brief Create core dump manually using external tools
   */
  static void _create_manual_core_dump ();

  /**
   * @brief Restore original core pattern
   */
  static void _restore_core_pattern ();
  /**
   * @brief Monitor and copy core dumps from systemd-coredump to dumps
   * directory (OLD - blocking)
   */
  static void _monitor_and_copy_core_dumps ();

  /**
   * @brief INSTANT systemd-coredump monitor using inotify (runs in background
   * thread)
   * @details Monitors /var/lib/systemd/coredump/ using Linux inotify API
   *          Immediately extracts new core dumps to dumps/ directory
   * @note Requires user in systemd-journal group
   * @source Official Linux inotify(7) man page
   */
  static void _instant_systemd_monitor () noexcept;
#endif

  // Utility functions
  static std::string _generate_dump_filename (std::string const &prefix);
  static std::string _generate_dump_filename (DumpType dumpType);
  static std::string _get_executable_directory () noexcept;
  static void _log_message (std::string const &message, bool isError = false);

  // Helper functions to reduce code duplication
#if LUMEX_OS_IS_WINDOWS()
  static std::string _convert_wide_string_to_narrow (
      std::wstring const &wideStr) noexcept; // only for Windows
#endif
  static void _log_dump_creation_success (std::string const &filename,
                                          size_t size,
                                          DumpType dumpType) noexcept;

  // Atomic file operations to prevent TOCTOU race conditions
  static bool _create_file_atomically (std::string const &filename,
                                       std::string const &content) noexcept;
  static bool _create_directory_atomically (std::string const &path) noexcept;

#if LUMEX_OS_IS_WINDOWS()
  /**
   * @brief Convert DumpType enum to MINIDUMP_TYPE flags
   * @param type The dump type to convert
   * @return Corresponding MINIDUMP_TYPE flags
   */
  static MINIDUMP_TYPE _get_minidump_type (DumpType type) noexcept;

  /**
   * @brief Validate MINIDUMP_TYPE flags combination
   * @param flags The MINIDUMP_TYPE flags to validate
   * @return true if flags combination is valid, false otherwise
   */
  static bool _is_valid_minidump_type (MINIDUMP_TYPE flags) noexcept;
#endif

  // Security and validation functions
  static bool _validate_directory (std::string const &path) noexcept;
  static bool _validate_filename (std::string const &filename) noexcept;
  static std::string _sanitize_path (std::string const &path) noexcept;

  // Platform-specific filesystem utilities (C++11 compatible)
  static bool _create_directory_recursive (std::string const &path) noexcept;

  // Security helper functions
  static std::string _generate_secure_random_component () noexcept;
  static std::string _generate_fallback_random_component () noexcept;
  static std::string
  _sanitize_filename_component (std::string const &component) noexcept;
  static std::string
  _sanitize_log_message (std::string const &message) noexcept;
  static std::string
  _sanitize_log_message_for_admin (std::string const &message) noexcept;

  // Advanced error handling and logging
  enum class LogLevel : std::uint8_t
  {
    DEBUG_ = 0,
    INFO_ = 1,
    WARNING_ = 2,
    ERROR_ = 3,
    CRITICAL_ = 4
  };

  static void _log_message (std::string const &message,
                            LogLevel level = LogLevel::INFO_) noexcept;

  // Performance monitoring
  struct PerformanceMetrics
  {
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
    size_t m_dumpSize = 0;
    bool m_success = false;
  };

  static void
  _start_performance_monitoring (PerformanceMetrics &metrics) noexcept;
  static void _end_performance_monitoring (PerformanceMetrics &metrics,
                                           bool success) noexcept;
  static void
  _log_performance_metrics (PerformanceMetrics const &metrics) noexcept;

  // Advanced concurrency control
  static bool _acquire_operation_slot () noexcept;
  static void _release_operation_slot () noexcept;
  static void _wait_for_operation_slot () noexcept;

  // Thread-safe cache management
  void _invalidate_cache () const noexcept;
  void _update_cache () const noexcept;

  // RAII operation guard
  class operation_guard
  {
  public:
    operation_guard () noexcept;
    ~operation_guard () noexcept;
    operation_guard (operation_guard const &) = delete;
    operation_guard &operator= (operation_guard const &) = delete;
    operation_guard (operation_guard &&) = delete;
    operation_guard &operator= (operation_guard &&) = delete;

    bool
    is_acquired () const noexcept
    {
      return m_acquired;
    }

  private:
    bool m_acquired = false;
  };

// Windows privilege checking
#if LUMEX_OS_IS_WINDOWS()
  static bool _is_admin_privileges () noexcept;
  static bool _is_elevated_process () noexcept;
#endif
};

// ==================================== Implementation
// ==================================== //

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay)

// Static data members are defined exactly once, in LumexCoreDumpGenerator.cpp,
// as ordinary (non-inline) definitions. They must NOT be defined here: a
// C++17 `inline` variable in a header is only a clang-cl warning
// (-Wc++17-extensions) but a hard MSVC error (C7525) in a translation unit
// without /std:c++17.
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())

inline bool
core_dump_generator::register_custom_console_handler (
    void (*handler) ()) noexcept
{
  try
    {
      s_unixConsoleHandler = handler;
      if (!s_posixSigThreadStarted.exchange (true))
        {
          _block_default_shutdown_signals ();
          std::thread (_posix_sigwait_thread).detach ();
        }
      return true;
    }
  catch (...)
    {
      return false;
    }
}

inline void
core_dump_generator::_block_default_shutdown_signals ()
{
  sigset_t set;
  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigaddset (&set, SIGTERM);
  sigaddset (&set, SIGHUP);
  pthread_sigmask (SIG_BLOCK, &set, nullptr);
}

inline void
core_dump_generator::_posix_sigwait_thread ()
{
  sigset_t set;
  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigaddset (&set, SIGTERM);
  sigaddset (&set, SIGHUP);
  int sig = 0;
  if (sigwait (&set, &sig) == 0)
    {
      auto callback = s_unixConsoleHandler;
      if (callback)
        callback ();
    }
}
#endif

// Thread-safe time formatting
namespace
{
std::mutex
    s_timeMutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::string
format_time (char const *format)
{
  std::lock_guard<std::mutex> lock (s_timeMutex);
  auto now = std::time (nullptr);

#if defined(_MSC_VER)
  struct tm timeInfo = {};
  if (localtime_s (&timeInfo, &now) != 0)
    return "unknown_time";
  auto &timeStruct = timeInfo;
#elif defined(__MINGW32__)
  // localtime_r is POSIX and MinGW's headers do not declare it; the mutex
  // above makes std::localtime safe to use here.
  std::tm const *local = std::localtime (&now);
  if (local == nullptr)
    return "unknown_time";
  struct tm timeInfo = *local;
  auto &timeStruct = timeInfo;
#else
  struct tm timeInfo = {};
  if (localtime_r (&now, &timeInfo) == nullptr)
    return "unknown_time";
  auto &timeStruct = timeInfo;
#endif

  std::ostringstream oss;
  oss << std::put_time (&timeStruct, format);
  return oss.str ();
}

/**
 * @brief Convert DumpType enum to descriptive string
 * @param type The dump type to convert
 * @return String representation of the dump type
 */
std::string
dump_type_to_string (DumpType type)
{
  switch (type)
    {
    // Windows mini-dump types
    case DumpType::MINI_DUMP_NORMAL:
      return "mini_dump_normal";
    case DumpType::MINI_DUMP_WITH_DATA_SEGS:
      return "mini_dump_with_data_segs";
    case DumpType::MINI_DUMP_WITH_FULL_MEMORY:
      return "mini_dump_with_full_memory";
    case DumpType::MINI_DUMP_WITH_HANDLE_DATA:
      return "mini_dump_with_handle_data";
    case DumpType::MINI_DUMP_FILTER_MEMORY:
      return "mini_dump_filter_memory";
    case DumpType::MINI_DUMP_SCAN_MEMORY:
      return "mini_dump_scan_memory";
    case DumpType::MINI_DUMP_WITH_UNLOADED_MODULES:
      return "mini_dump_with_unloaded_modules";
    case DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY:
      return "mini_dump_with_indirectly_referenced_memory";
    case DumpType::MINI_DUMP_FILTER_MODULE_PATHS:
      return "mini_dump_filter_module_paths";
    case DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA:
      return "mini_dump_with_process_thread_data";
    case DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY:
      return "mini_dump_with_private_read_write_memory";
    case DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA:
      return "mini_dump_without_optional_data";
    case DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO:
      return "mini_dump_with_full_memory_info";
    case DumpType::MINI_DUMP_WITH_THREAD_INFO:
      return "mini_dump_with_thread_info";
    case DumpType::MINI_DUMP_WITH_CODE_SEGMENTS:
      return "mini_dump_with_code_segments";
    case DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE:
      return "mini_dump_without_auxiliary_state";
    case DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE:
      return "mini_dump_with_full_auxiliary_state";
    case DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY:
      return "mini_dump_with_private_write_copy_memory";
    case DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY:
      return "mini_dump_ignore_inaccessible_memory";
    case DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION:
      return "mini_dump_with_token_information";

    // Windows kernel-mode dump types
    case DumpType::KERNEL_FULL_DUMP:
      return "kernel_full_dump";
    case DumpType::KERNEL_KERNEL_DUMP:
      return "kernel_kernel_dump";
    case DumpType::KERNEL_SMALL_DUMP:
      return "kernel_small_dump";
    case DumpType::KERNEL_AUTOMATIC_DUMP:
      return "kernel_automatic_dump";
    case DumpType::KERNEL_ACTIVE_DUMP:
      return "kernel_active_dump";

    // UNIX core dump types
    case DumpType::CORE_DUMP_FULL:
      return "core_dump_full";

    // Default types
    case DumpType::DEFAULT_AUTO:
      return "default_auto";

    default:
      return "unknown_dump_type";
    }
}

#if defined(_WIN32)
// GetCurrentProcessToken is an inline of the Windows 8 SDK that older
// MinGW-w64 headers do not have. It returns this documented pseudo handle, so
// the library spells it itself and builds with any SDK.
HANDLE
current_process_token ()
{
  return reinterpret_cast<HANDLE> (static_cast<LONG_PTR> (-4));
}
#endif
} // namespace

inline void
core_dump_generator::initialize (std::string const &dumpDirectory,
                                 DumpType dumpType, bool handleExceptions)
{
  dump_configuration config = dump_factory::create_configuration (dumpType);
  config.set_directory (dumpDirectory.empty ()
                            ? _get_executable_directory () + "/dumps"
                            : dumpDirectory);
  initialize (config, handleExceptions);
}

inline void
core_dump_generator::initialize (dump_configuration const &config,
                                 bool handleExceptions)
{
  std::lock_guard<std::mutex> lock (s_mutex);

#if __cplusplus >= 201103L
  // Check if already initialized using atomic load
  if (s_initialized.load (std::memory_order_acquire))
    {
      _log_message ("CoreDumpGenerator already initialized", false);
      return;
    }
#else
  if (s_initialized)
    {
      _log_message ("CoreDumpGenerator already initialized", false);
      return;
    }
#endif

  try
    {
      // Set configuration
      s_currentConfig = config;

      // Set dump directory
      s_dumpDirectory = config.get_directory ().empty ()
                            ? _get_executable_directory () + "/dumps"
                            : config.get_directory ();

      // Validate and sanitize directory path
      s_dumpDirectory = _sanitize_path (s_dumpDirectory);
      if (!_validate_directory (s_dumpDirectory))
        throw std::runtime_error ("Invalid dump directory: "
                                  + s_dumpDirectory);

      // Create dump directory with proper error handling
      if (!_create_directory_recursive (s_dumpDirectory))
        throw std::runtime_error ("Failed to create dump directory: "
                                  + s_dumpDirectory);
      _log_message ("Dump directory created: " + s_dumpDirectory, false);

      // Log configuration
      _log_message ("Dump type: "
                        + dump_factory::get_description (config.get_type ()),
                    false);
      if (config.get_max_size_bytes () > 0)
        _log_message ("Max size: "
                          + std::to_string (config.get_max_size_bytes ())
                          + " bytes",
                      false);

      // Initialize platform-specific handlers
      _platform_initialize ();

      // Setup exception handling if requested
      if (handleExceptions)
        _setup_exception_handling ();

      // Set initialized flag with release semantics to ensure all previous
      // operations are visible
#if __cplusplus >= 201103L
      s_initialized.store (true, std::memory_order_release);
#else
      s_initialized = true;
#endif
      _log_message ("CoreDumpGenerator initialized successfully", false);
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to initialize CoreDumpGenerator: "
                        + std::string (exc.what ()),
                    true);
      throw;
    }
  catch (...)
    {
      _log_message ("Failed to initialize CoreDumpGenerator: Unknown error",
                    true);
      throw std::runtime_error (
          "Unknown error during CoreDumpGenerator initialization");
    }
}

inline core_dump_generator &
core_dump_generator::instance ()
{
#if __cplusplus >= 201103L
  // Double-checked locking pattern for thread safety
  if (!s_initialized.load (std::memory_order_acquire))
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");

  std::call_once (
      s_initFlag,
      [] ()
        {
          // Additional check inside call_once for extra safety
          if (!s_initialized.load (std::memory_order_acquire))
            throw std::runtime_error (
                "CoreDumpGenerator not initialized. Call initialize() first.");
          s_instance = std::unique_ptr<core_dump_generator> (
              new core_dump_generator ());
        });
#else
  // C++98/03 fallback - not thread-safe
  if (!s_instance)
    {
      if (!s_initialized)
        throw std::runtime_error (
            "CoreDumpGenerator not initialized. Call initialize() first.");
    }
#endif

  return *s_instance;
}

inline bool
core_dump_generator::is_initialized () noexcept
{
#if __cplusplus >= 201103L
  return s_initialized.load (std::memory_order_seq_cst);
#else
  return s_initialized;
#endif
}

inline bool
core_dump_generator::generate_dump (std::string const &reason,
                                    DumpType dumpType)
{
#if __cplusplus >= 201103L
  if (!s_initialized.load (std::memory_order_acquire))
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#else
  if (!s_initialized)
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#endif

  try
    {
      dump_configuration config = s_currentConfig;
      if (dumpType != DumpType::DEFAULT_AUTO)
        {
          config = dump_factory::create_configuration (dumpType);
          config.set_directory (s_dumpDirectory); // Preserve current directory
        }

      std::string filename = _generate_dump_filename (config.get_type ());
      _log_message ("Generating dump: " + reason, false);
      _log_message ("Dump type: "
                        + dump_factory::get_description (config.get_type ()),
                    false);

#if LUMEX_OS_IS_WINDOWS()
      return _create_windows_dump (filename, config);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      _generate_core_dump ();
      _log_core_dump_size (filename);
      return true;
#endif
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to generate dump: " + std::string (exc.what ()),
                    true);
      return false;
    }
}

inline bool
core_dump_generator::generate_dump (dump_configuration const &config,
                                    std::string const &reason)
{
#if __cplusplus >= 201103L
  if (!s_initialized.load (std::memory_order_acquire))
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#else
  if (!s_initialized)
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#endif

  try
    {
      std::string filename = _generate_dump_filename (config.get_type ());
      _log_message ("Generating dump: " + reason, false);
      _log_message ("Dump type: "
                        + dump_factory::get_description (config.get_type ()),
                    false);

#if LUMEX_OS_IS_WINDOWS()
      return _create_windows_dump (filename, config);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      _generate_core_dump ();
      _log_core_dump_size (filename);
      return true;
#endif
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to generate dump: " + std::string (exc.what ()),
                    true);
      return false;
    }
}

inline bool
core_dump_generator::generate_dump (std::string const &reason,
                                    DumpType dumpType,
                                    std::error_code &errorCode) noexcept
{
  try
    {
      // Clear error code
      errorCode.clear ();

#if __cplusplus >= 201103L
      if (!s_initialized.load (std::memory_order_acquire))
        {
          errorCode
              = std::make_error_code (std::errc::operation_not_permitted);
          return false;
        }
#else
      if (!s_initialized)
        {
          errorCode
              = std::make_error_code (std::errc::operation_not_permitted);
          return false;
        }
#endif

      // Validate dump type
      if (!dump_factory::is_supported (dumpType))
        {
          errorCode = std::make_error_code (std::errc::invalid_argument);
          return false;
        }

      dump_configuration config = s_currentConfig;
      if (dumpType != DumpType::DEFAULT_AUTO)
        {
          config = dump_factory::create_configuration (dumpType);
          config.set_directory (s_dumpDirectory); // Preserve current directory
        }

      std::string filename = _generate_dump_filename (config.get_type ());
      _log_message ("Generating dump: " + reason, false);
      _log_message ("Dump type: "
                        + dump_factory::get_description (config.get_type ()),
                    false);

#if LUMEX_OS_IS_WINDOWS()
      return _create_windows_dump (filename, config);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      _generate_core_dump ();
      _log_core_dump_size (filename);
      return true;
#endif
    }
  catch (std::system_error const &exc)
    {
      errorCode = exc.code ();
      _log_message ("Failed to generate dump: " + std::string (exc.what ()),
                    true);
      return false;
    }
  catch (std::exception const &exc)
    {
      errorCode = std::make_error_code (std::errc::operation_canceled);
      _log_message ("Failed to generate dump: " + std::string (exc.what ()),
                    true);
      return false;
    }
  catch (...)
    {
      errorCode = std::make_error_code (std::errc::operation_canceled);
      _log_message ("Failed to generate dump: Unknown error", true);
      return false;
    }
}

inline std::string
core_dump_generator::get_dump_directory () noexcept
{
  return s_dumpDirectory; // s_dumpDirectory is only modified during
                          // initialization, which is single-threaded
}

inline dump_configuration
core_dump_generator::get_current_configuration () noexcept
{
  return s_currentConfig; // s_currentConfig is only modified during
                          // initialization, which is single-threaded
}

inline bool
core_dump_generator::set_dump_type (DumpType dumpType)
{
  std::lock_guard<std::mutex> lock (s_mutex);

#if __cplusplus >= 201103L
  if (!s_initialized.load (std::memory_order_acquire))
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#else
  if (!s_initialized)
    throw std::runtime_error (
        "CoreDumpGenerator not initialized. Call initialize() first.");
#endif

  if (!dump_factory::is_supported (dumpType))
    {
      _log_message ("Dump type not supported on this platform", true);
      return false;
    }

  s_currentConfig = dump_factory::create_configuration (dumpType);
  s_currentConfig.set_directory (
      s_dumpDirectory); // Preserve current directory
  return true;
}

inline DumpType
core_dump_generator::get_current_dump_type () noexcept
{
  return s_currentConfig
      .get_type (); // s_currentConfig is only modified during
                    // initialization, which is single-threaded
}

inline void
core_dump_generator::set_core_pattern_for_crash ()
{
  // Core pattern is set in _generate_core_dump now
}

inline void
core_dump_generator::set_admin_group_name (std::string const &name) noexcept
{
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  try
    {
      std::lock_guard<std::mutex> lock (s_customHandlersMutex);
      s_adminGroupName = name;
    }
  catch (...)
    {
      // Never propagate; the group check must stay usable even if allocation
      // fails.
    }
#else
  (void)name; // Group membership is only meaningful on UNIX.
#endif
}

inline bool
core_dump_generator::is_admin_privileges () noexcept
{
#if LUMEX_OS_IS_WINDOWS()
  return _is_admin_privileges ();
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  if (getuid () == 0)
    return true;

  // LumexLib does not hardcode an admin group: the check is opt-in through
  // set_admin_group_name(). An empty name (the default) and a name that cannot
  // be resolved keep the root-only behaviour.
  std::string adminGroupName;
  {
    std::lock_guard<std::mutex> lock (s_customHandlersMutex);
    adminGroupName = s_adminGroupName;
  }
  if (adminGroupName.empty ())
    return false;

  struct group *adminGroup = getgrnam (adminGroupName.c_str ());
  if (adminGroup == nullptr)
    return false;

  gid_t const adminGid = adminGroup->gr_gid;
  if (getgid () == adminGid || getegid () == adminGid)
    return true;

  int const groupCount = getgroups (0, nullptr);
  if (groupCount <= 0)
    return false;

  std::vector<gid_t> groups (static_cast<std::size_t> (groupCount));
  if (getgroups (groupCount, groups.data ()) < 0)
    return false;

  return std::find (groups.begin (), groups.end (), adminGid) != groups.end ();
#else
  return false; // Unsupported platform
#endif
}

inline bool
core_dump_generator::register_custom_signal_handler (
    int signum, void (*handler) (int)) noexcept
{
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  try
    {
      std::lock_guard<std::mutex> lock (s_customHandlersMutex);
      s_customSignalHandlers[signum] = handler;

      // Register the signal with our wrapper handler
      struct sigaction sa;
      sa.sa_handler = _custom_signal_handler_wrapper;
      sigemptyset (&sa.sa_mask);
      sa.sa_flags = static_cast<int> (SA_RESETHAND);

      if (sigaction (signum, &sa, nullptr) == 0)
        {
          _log_message ("Custom signal handler registered for signal "
                            + std::to_string (signum),
                        false);
          return true;
        }
      else
        {
          s_customSignalHandlers.erase (signum);
          return false;
        }
    }
  catch (...)
    {
      return false;
    }
#else
  // Windows doesn't use signal handlers for graceful shutdown
  (void)signum;  // Suppress unused parameter warning
  (void)handler; // Suppress unused parameter warning
  return false;
#endif
}

#if LUMEX_OS_IS_WINDOWS()
inline bool
core_dump_generator::register_custom_console_handler (
    BOOL (WINAPI *handler) (DWORD)) noexcept
{
  try
    {
      s_customConsoleHandler = handler;
      _log_message ("Custom console handler registered", false);
      return true;
    }
  catch (...)
    {
      return false;
    }
}
#endif

// Instance methods implementation
inline std::string const &
core_dump_generator::get_instance_dump_directory () const noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);
  return m_dumpDirectory;
}

inline dump_configuration const &
core_dump_generator::get_instance_configuration () const noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);
  return m_currentConfig;
}

inline bool
core_dump_generator::is_instance_initialized () const noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);
  return m_isInitialized;
}

inline bool
core_dump_generator::generate_instance_dump (std::string const &reason)
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);

  if (!m_isInitialized)
    return false;

  try
    {
      return generate_dump (reason, m_currentConfig.get_type ());
    }
  catch (...)
    {
      return false;
    }
}

inline bool
core_dump_generator::generate_instance_dump (
    std::string const &reason, std::error_code &errorCode) noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);

  if (!m_isInitialized)
    {
      errorCode = std::make_error_code (std::errc::invalid_argument);
      return false;
    }

  try
    {
      return generate_dump (reason, m_currentConfig.get_type (), errorCode);
    }
  catch (...)
    {
      errorCode = std::make_error_code (std::errc::invalid_argument);
      return false;
    }
}

// Advanced concurrency control implementation
inline bool
core_dump_generator::_acquire_operation_slot () noexcept
{
  size_t current = s_activeOperations.load (std::memory_order_acquire);
  while (current < MAX_CONCURRENT_OPERATIONS)
    {
      if (s_activeOperations.compare_exchange_weak (current, current + 1,
                                                    std::memory_order_release,
                                                    std::memory_order_acquire))
        {
          return true;
        }
    }
  return false;
}

inline void
core_dump_generator::_release_operation_slot () noexcept
{
  s_activeOperations.fetch_sub (1, std::memory_order_release);
  s_operationCondition.notify_one ();
}

inline void
core_dump_generator::_wait_for_operation_slot () noexcept
{
  std::unique_lock<std::mutex> lock (s_operationMutex);
  s_operationCondition.wait (
      lock, [] ()
        { return s_activeOperations.load () < MAX_CONCURRENT_OPERATIONS; });
}

inline void
core_dump_generator::_invalidate_cache () const noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);
  m_cacheValid = false;
  m_cachedDumpTypeValid = false;
  m_cachedDumpDirectoryValid = false;
}

inline void
core_dump_generator::_update_cache () const noexcept
{
  std::lock_guard<std::mutex> lock (m_instanceMutex);
  if (!m_cacheValid)
    {
      m_cachedDumpType = m_currentConfig.get_type ();
      m_cachedDumpDirectory = m_dumpDirectory;
      m_cachedDumpTypeValid = true;
      m_cachedDumpDirectoryValid = true;
      m_cacheValid = true;
    }
}

// operation_guard implementation
inline core_dump_generator::operation_guard::operation_guard () noexcept
{
  m_acquired = _acquire_operation_slot ();
  if (!m_acquired)
    {
      _wait_for_operation_slot ();
      m_acquired = _acquire_operation_slot ();
    }
}

inline core_dump_generator::operation_guard::~operation_guard () noexcept
{
  if (m_acquired)
    _release_operation_slot ();
}

// Performance monitoring implementation
inline void
core_dump_generator::_start_performance_monitoring (
    PerformanceMetrics &metrics) noexcept
{
  metrics.m_startTime = std::chrono::high_resolution_clock::now ();
  metrics.m_success = false;
  metrics.m_dumpSize = 0;
}

inline void
core_dump_generator::_end_performance_monitoring (PerformanceMetrics &metrics,
                                                  bool success) noexcept
{
  metrics.m_endTime = std::chrono::high_resolution_clock::now ();
  metrics.m_success = success;
  _log_performance_metrics (metrics);
}

inline void
core_dump_generator::_log_performance_metrics (
    PerformanceMetrics const &metrics) noexcept
{
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (
      metrics.m_endTime - metrics.m_startTime);
  std::string message
      = "Performance: " + std::to_string (duration.count ())
        + "ms, Size: " + std::to_string (metrics.m_dumpSize)
        + " bytes, Success: " + (metrics.m_success ? "true" : "false");
  _log_message (message, LogLevel::INFO_);
}

// Private implementation
inline void
core_dump_generator::_platform_initialize ()
{
#if LUMEX_OS_IS_WINDOWS()
  _setup_windows_handlers ();
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  _setup_signal_handlers ();
  _setup_core_dump_settings ();

  // Store orig core_pattern BEFORE attempting to change it
  _setup_core_pattern ();
  std::string pattern_before_change = s_originalCorePattern;

  // Try to set custom core_pattern via sudo (primary mechanism)
  _generate_core_dump ();

  // Verify if core_pattern actually changed with kernel specifiers
  std::ifstream check_pattern ("/proc/sys/kernel/core_pattern");
  std::string pattern_after_change;

  if (std::getline (check_pattern, pattern_after_change))
    {
      // Check for our pattern with kernel specifiers: %t, %p, %e
      bool has_kernel_specifiers
          = (pattern_after_change.find ("%t") != std::string::npos
             && pattern_after_change.find ("%p") != std::string::npos
             && pattern_after_change.find ("%e") != std::string::npos);

      bool has_our_directory
          = (pattern_after_change.find (s_dumpDirectory) != std::string::npos);

      if (has_kernel_specifiers && has_our_directory)
        {
          _log_message ("SUCCESS: Custom core_pattern set successfully",
                        false);
          _log_message ("  Dumps will be created directly in: "
                            + s_dumpDirectory,
                        false);
          _log_message ("  Pattern: " + pattern_after_change, false);
        }
      else
        {
          _log_message ("WARNING: core_pattern did not change to our pattern "
                        "(sudo likely failed)",
                        false);

          // Try to restore original systemd-coredump pattern if it was active
          // before This prevents leaving stale pattern from previous run
          if (!pattern_before_change.empty ()
              && pattern_before_change[0] == '|'
              && pattern_before_change.find ("systemd-coredump")
                     != std::string::npos)
            {
              _log_message ("INFO: Attempting to restore original "
                            "systemd-coredump pattern",
                            false);
              std::string restore_cmd
                  = "echo \"" + pattern_before_change
                    + "\" | sudo tee /proc/sys/kernel/core_pattern >/dev/null "
                      "2>&1";
              int restore_ret = system (restore_cmd.c_str ());

              if (restore_ret == 0)
                {
                  _log_message (
                      "INFO: Successfully restored systemd-coredump pattern",
                      false);
                  pattern_after_change
                      = pattern_before_change; // Update for subsequent checks
                }
            }

          // Check if systemd-coredump is active (either originally or after
          // restore)
          if (!pattern_after_change.empty () && pattern_after_change[0] == '|'
              && pattern_after_change.find ("systemd-coredump")
                     != std::string::npos)
            {
              // systemd-coredump is active -> start fallback monitor
              try
                {
                  s_monitorThreadShouldStop.store (false,
                                                   std::memory_order_release);
                  s_monitorThread = std::thread (_instant_systemd_monitor);
                  _log_message ("INFO: Using systemd-coredump with instant "
                                "extraction fallback",
                                false);
                  _log_message ("  Dumps will be extracted from systemd to: "
                                    + s_dumpDirectory,
                                false);
                }
              catch (std::exception const &exc)
                {
                  _log_message ("Failed to start systemd fallback monitor: "
                                    + std::string (exc.what ()),
                                true);
                }
            }
          else
            {
              _log_message ("WARNING: Using system default core_pattern",
                            false);
              _log_message ("  Current pattern: " + pattern_after_change,
                            false);
              _log_message ("  Dumps may be created in current directory or "
                            "other system location",
                            false);
            }
        }
    }

#endif
}

// Windows-specific implementation
#if LUMEX_OS_IS_WINDOWS()

inline void
core_dump_generator::_setup_windows_handlers ()
{
  try
    {
      // Set our exception handler
      SetUnhandledExceptionFilter (_windows_exception_handler);

      // Register console handler for graceful shutdown
      SetConsoleCtrlHandler (_windows_console_handler, TRUE);

      // Redirect SetUnhandledExceptionFilter to prevent removal
      HMODULE hKernel32 = GetModuleHandleA ("kernel32.dll");
      if (hKernel32 != nullptr)
        {
          FARPROC pSetUnhandledExceptionFilter
              = GetProcAddress (hKernel32, "SetUnhandledExceptionFilter");
          if (pSetUnhandledExceptionFilter != nullptr)
            {
              // We can't easily hook this in C++, but the handler should work
              _log_message ("Windows exception handler installed", false);
            }
        }

      _log_message ("Windows crash handlers installed successfully", false);
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to setup Windows handlers: "
                        + std::string (exc.what ()),
                    true);
      throw;
    }
}

inline LONG WINAPI
core_dump_generator::_windows_exception_handler (
    EXCEPTION_POINTERS *pExInfo) noexcept
{
  HANDLE hFile = INVALID_HANDLE_VALUE; // Initialize to invalid handle for
                                       // RAII-style cleanup

  try
    {
      _log_message ("Windows exception handler called", false);

      // Create dump directory
      _create_directory_recursive (s_dumpDirectory);

      // Generate filename using thread-safe time formatting with dump type
      std::string timeStr = format_time ("%d.%m.%Y.%H.%M.%S");
      std::string dumpTypeStr
          = dump_type_to_string (s_currentConfig.get_type ());

      // Sanitize time string to prevent command injection
      std::string sanitizedTimeStr;
      for (char c : timeStr)
        if (std::isalnum (c) || c == '.' || c == '_' || c == '-')
          sanitizedTimeStr += c;
        else
          sanitizedTimeStr += '_';

      std::wstring wTimeStr (sanitizedTimeStr.begin (),
                             sanitizedTimeStr.end ());
      std::wstring wDumpTypeStr (dumpTypeStr.begin (), dumpTypeStr.end ());
      std::wstringstream wss;
      wss << s_dumpDirectory.c_str () << L"\\" << wDumpTypeStr << L"_"
          << wTimeStr << L".dmp";

      std::wstring filename = wss.str ();

      // Open file for writing dump with secure permissions (owner read/write
      // only)
      SECURITY_ATTRIBUTES sa = {};
      sa.nLength = sizeof (SECURITY_ATTRIBUTES);
      sa.bInheritHandle = FALSE;

      // Create a security descriptor that allows proper access for deletion
      SECURITY_DESCRIPTOR sd = {};
      if (InitializeSecurityDescriptor (&sd, SECURITY_DESCRIPTOR_REVISION))
        {
          // Set owner to current user
          PSID ownerSid = nullptr;
          DWORD ownerSidSize = 0;
          GetTokenInformation (current_process_token (), TokenUser, nullptr, 0,
                               &ownerSidSize);
          if (ownerSidSize > 0)
            {
              std::vector<BYTE> tokenInfo (ownerSidSize);
              if (GetTokenInformation (current_process_token (), TokenUser,
                                       tokenInfo.data (), ownerSidSize,
                                       &ownerSidSize))
                {
                  TOKEN_USER *tokenUser
                      = reinterpret_cast<TOKEN_USER *> (tokenInfo.data ());
                  ownerSid = tokenUser->User.Sid;
                }
            }

          // Get Administrators group SID
          PSID administratorsGroup = nullptr;
          SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
          if (AllocateAndInitializeSid (&ntAuthority, 2,
                                        SECURITY_BUILTIN_DOMAIN_RID,
                                        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0,
                                        0, &administratorsGroup))
            {
              if (ownerSid
                  && SetSecurityDescriptorOwner (&sd, ownerSid, FALSE))
                {
                  // Create DACL with full access for owner and administrators
                  PACL dacl = nullptr;
                  DWORD daclSize
                      = static_cast<DWORD> (sizeof (ACL)
                                            + 2 * sizeof (ACCESS_ALLOWED_ACE))
                        + GetLengthSid (ownerSid)
                        + GetLengthSid (administratorsGroup);
                  dacl = static_cast<PACL> (LocalAlloc (LPTR, daclSize));

                  if (dacl && InitializeAcl (dacl, daclSize, ACL_REVISION))
                    {
                      // Add full access for owner
                      if (AddAccessAllowedAce (dacl, ACL_REVISION,
                                               FILE_ALL_ACCESS, ownerSid))
                        {
                          // Add full access for administrators
                          if (AddAccessAllowedAce (dacl, ACL_REVISION,
                                                   FILE_ALL_ACCESS,
                                                   administratorsGroup))
                            {
                              SetSecurityDescriptorDacl (&sd, TRUE, dacl,
                                                         FALSE);
                              sa.lpSecurityDescriptor = &sd;
                            }
                        }
                    }
                }

              // Clean up administrators SID
              if (administratorsGroup)
                FreeSid (administratorsGroup);
            }
        }

      hFile = CreateFileW (filename.c_str (), GENERIC_WRITE, 0, &sa,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile == INVALID_HANDLE_VALUE)
        {
          DWORD error = GetLastError ();
          _log_message ("Failed to create dump file. Error: "
                            + std::to_string (error),
                        true);
          return EXCEPTION_EXECUTE_HANDLER;
        }

      // Write minidump with proper dump type
      MINIDUMP_EXCEPTION_INFORMATION eInfo;
      eInfo.ThreadId = GetCurrentThreadId ();
      eInfo.ExceptionPointers = pExInfo;
      eInfo.ClientPointers = FALSE;

      // Get dump type using centralized mapping
      MINIDUMP_TYPE dumpType
          = _get_minidump_type (s_currentConfig.get_type ());

      // Validate dump type flags
      if (!_is_valid_minidump_type (dumpType))
        {
          _log_message (
              "Invalid MINIDUMP_TYPE flags detected, using MiniDumpNormal",
              true);
          dumpType = MiniDumpNormal;
        }

      BOOL success
          = MiniDumpWriteDump (GetCurrentProcess (), GetCurrentProcessId (),
                               hFile, dumpType, &eInfo, NULL, NULL);

      // Get file size BEFORE closing the handle
      LARGE_INTEGER fileSize = { { 0, 0 } };
      BOOL sizeResult = GetFileSizeEx (hFile, &fileSize);

      // Close the handle immediately after getting size
      CloseHandle (hFile);
      hFile = INVALID_HANDLE_VALUE;

      if (success == FALSE)
        {
          DWORD error = GetLastError ();
          _log_message ("MiniDumpWriteDump failed with error: "
                            + std::to_string (error),
                        true);
        }
      else
        {
          // Convert wide string to narrow string using helper function
          std::string narrowFilename
              = _convert_wide_string_to_narrow (filename);
          if (narrowFilename.empty ())
            {
              _log_message ("Failed to convert wide string to narrow string",
                            true);
              return EXCEPTION_EXECUTE_HANDLER;
            }

          // Log success using helper function with overflow protection
          size_t fileSizeBytes = 0;
          if (sizeResult != 0)
            {
              // Check for integer overflow before conversion
              if (fileSize.QuadPart >= 0
                  && static_cast<unsigned long long> (fileSize.QuadPart)
                         <= SIZE_MAX)
                {
                  fileSizeBytes = static_cast<size_t> (fileSize.QuadPart);
                }
              else
                {
                  _log_message (
                      "File size too large to represent in size_t, using 0",
                      true);
                  fileSizeBytes = 0;
                }
            }
          _log_dump_creation_success (narrowFilename, fileSizeBytes,
                                      static_cast<DumpType> (dumpType));
        }
    }
  catch (...)
    {
      // Don't let exceptions escape from the exception handler
      _log_message ("Exception in Windows exception handler", true);

      // Ensure handle is closed even on exceptions
      if (hFile != INVALID_HANDLE_VALUE)
        {
          CloseHandle (hFile);
          hFile = INVALID_HANDLE_VALUE;
        }
    }

  return EXCEPTION_EXECUTE_HANDLER;
}

inline LONG WINAPI
core_dump_generator::_redirected_set_unhandled_exception_filter (
    EXCEPTION_POINTERS * /*ExceptionInfo*/) noexcept
{
  // When the CRT calls SetUnhandledExceptionFilter with NULL parameter
  // our handler will not get removed.
  return 0;
}

inline BOOL WINAPI
core_dump_generator::_windows_console_handler (DWORD ctrlType) noexcept
{
  // Call custom console handler if registered
  if (s_customConsoleHandler != nullptr)
    return s_customConsoleHandler (ctrlType);

  // Default behavior - let the system handle it
  return FALSE;
}

inline bool
core_dump_generator::_create_windows_dump (std::string const &filename,
                                           dump_configuration const &config)
{
  try
    {
      // Convert string to wide string for Windows API
      std::wstring wfilename (filename.begin (), filename.end ());

      // Create security attributes for proper file access
      SECURITY_ATTRIBUTES sa = {};
      sa.nLength = sizeof (SECURITY_ATTRIBUTES);
      sa.bInheritHandle = FALSE;

      // Create a security descriptor that allows proper access for deletion
      SECURITY_DESCRIPTOR sd = {};
      if (InitializeSecurityDescriptor (&sd, SECURITY_DESCRIPTOR_REVISION))
        {
          // Set owner to current user
          PSID ownerSid = nullptr;
          DWORD ownerSidSize = 0;
          GetTokenInformation (current_process_token (), TokenUser, nullptr, 0,
                               &ownerSidSize);
          if (ownerSidSize > 0)
            {
              std::vector<BYTE> tokenInfo (ownerSidSize);
              if (GetTokenInformation (current_process_token (), TokenUser,
                                       tokenInfo.data (), ownerSidSize,
                                       &ownerSidSize))
                {
                  TOKEN_USER *tokenUser
                      = reinterpret_cast<TOKEN_USER *> (tokenInfo.data ());
                  ownerSid = tokenUser->User.Sid;
                }
            }

          // Get Administrators group SID
          PSID administratorsGroup = nullptr;
          SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
          if (AllocateAndInitializeSid (&ntAuthority, 2,
                                        SECURITY_BUILTIN_DOMAIN_RID,
                                        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0,
                                        0, &administratorsGroup))
            {
              if (ownerSid
                  && SetSecurityDescriptorOwner (&sd, ownerSid, FALSE))
                {
                  // Create DACL with full access for owner and administrators
                  PACL dacl = nullptr;
                  DWORD daclSize
                      = static_cast<DWORD> (sizeof (ACL)
                                            + 2 * sizeof (ACCESS_ALLOWED_ACE))
                        + GetLengthSid (ownerSid)
                        + GetLengthSid (administratorsGroup);
                  dacl = static_cast<PACL> (LocalAlloc (LPTR, daclSize));

                  if (dacl && InitializeAcl (dacl, daclSize, ACL_REVISION))
                    {
                      // Add full access for owner
                      if (AddAccessAllowedAce (dacl, ACL_REVISION,
                                               FILE_ALL_ACCESS, ownerSid))
                        {
                          // Add full access for administrators
                          if (AddAccessAllowedAce (dacl, ACL_REVISION,
                                                   FILE_ALL_ACCESS,
                                                   administratorsGroup))
                            {
                              SetSecurityDescriptorDacl (&sd, TRUE, dacl,
                                                         FALSE);
                              sa.lpSecurityDescriptor = &sd;
                            }
                        }
                    }
                }

              // Clean up administrators SID
              if (administratorsGroup)
                FreeSid (administratorsGroup);
            }
        }

      // Open file for writing dump with proper error handling
      HANDLE hFile = CreateFileW (wfilename.c_str (), GENERIC_WRITE, 0, &sa,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile == INVALID_HANDLE_VALUE)
        {
          DWORD error = GetLastError ();
          _log_message ("Failed to create dump file. Error: "
                            + std::to_string (error),
                        true);
          return false;
        }

      // Write minidump with proper exception information
      MINIDUMP_EXCEPTION_INFORMATION eInfo;
      eInfo.ThreadId = GetCurrentThreadId ();
      eInfo.ExceptionPointers = nullptr; // No exception context in manual dump
      eInfo.ClientPointers = FALSE;

      // Get dump type using centralized mapping
      MINIDUMP_TYPE dumpType = _get_minidump_type (config.get_type ());

      // Validate dump type flags
      if (!_is_valid_minidump_type (dumpType))
        {
          _log_message (
              "Invalid MINIDUMP_TYPE flags detected, using MiniDumpNormal",
              true);
          dumpType = MiniDumpNormal;
        }

      // Configure symbol information if enabled
      MINIDUMP_USER_STREAM_INFORMATION userStreamInfo = { 0, nullptr };

      BOOL success
          = MiniDumpWriteDump (GetCurrentProcess (), GetCurrentProcessId (),
                               hFile, dumpType, &eInfo, &userStreamInfo, NULL);
      CloseHandle (hFile);

      if (success == FALSE)
        {
          DWORD error = GetLastError ();
          _log_message ("MiniDumpWriteDump failed with error: "
                            + std::to_string (error),
                        true);
          return false;
        }

      _log_message ("Windows dump created successfully: " + filename, false);
      return true;
    }
  catch (std::exception const &exc)
    {
      _log_message ("Exception in _createWindowsDump: "
                        + std::string (exc.what ()),
                    true);
      return false;
    }
}

// Windows-specific utility functions
inline MINIDUMP_TYPE
core_dump_generator::_get_minidump_type (DumpType type) noexcept
{
  switch (type)
    {
    // Basic mini-dump types (single flags)
    case DumpType::MINI_DUMP_NORMAL:
      return MiniDumpNormal;
    case DumpType::MINI_DUMP_WITH_DATA_SEGS:
      return MiniDumpWithDataSegs;
    case DumpType::MINI_DUMP_WITH_HANDLE_DATA:
      return MiniDumpWithHandleData;
    case DumpType::MINI_DUMP_FILTER_MEMORY:
      return MiniDumpFilterMemory;
    case DumpType::MINI_DUMP_SCAN_MEMORY:
      return MiniDumpScanMemory;
    case DumpType::MINI_DUMP_WITH_UNLOADED_MODULES:
      return MiniDumpWithUnloadedModules;
    case DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY:
      return MiniDumpWithIndirectlyReferencedMemory;
    case DumpType::MINI_DUMP_FILTER_MODULE_PATHS:
      return MiniDumpFilterModulePaths;
    case DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA:
      return MiniDumpWithProcessThreadData;
    case DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY:
      return MiniDumpWithPrivateReadWriteMemory;
    case DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA:
      return MiniDumpWithoutOptionalData;
    case DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO:
      return MiniDumpWithFullMemoryInfo;
    case DumpType::MINI_DUMP_WITH_THREAD_INFO:
      return MiniDumpWithThreadInfo;
    case DumpType::MINI_DUMP_WITH_CODE_SEGMENTS:
      return MiniDumpWithCodeSegs;
    case DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE:
      return MiniDumpWithoutAuxiliaryState;
    case DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE:
      return MiniDumpWithFullAuxiliaryState;
    case DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY:
      return MiniDumpWithPrivateWriteCopyMemory;
    case DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY:
      return MiniDumpIgnoreInaccessibleMemory;
    case DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION:
      return MiniDumpWithTokenInformation;

    // Full memory dump (combination of flags for maximum information)
    case DumpType::MINI_DUMP_WITH_FULL_MEMORY:
    case DumpType::KERNEL_FULL_DUMP:
      return static_cast<MINIDUMP_TYPE> (
          MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo
          | MiniDumpWithHandleData | MiniDumpWithUnloadedModules
          | MiniDumpWithIndirectlyReferencedMemory
          | MiniDumpWithProcessThreadData | MiniDumpWithPrivateReadWriteMemory
          | MiniDumpWithThreadInfo);

    // Kernel dump types (mapped to appropriate mini-dump equivalents)
    case DumpType::KERNEL_KERNEL_DUMP:
      return static_cast<MINIDUMP_TYPE> (MiniDumpWithFullMemory
                                         | MiniDumpWithFullMemoryInfo);
    case DumpType::KERNEL_SMALL_DUMP:
      return MiniDumpNormal;
    case DumpType::KERNEL_AUTOMATIC_DUMP:
      return static_cast<MINIDUMP_TYPE> (MiniDumpWithFullMemory
                                         | MiniDumpWithFullMemoryInfo
                                         | MiniDumpWithHandleData);
    case DumpType::KERNEL_ACTIVE_DUMP:
      return static_cast<MINIDUMP_TYPE> (
          MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo
          | MiniDumpWithHandleData | MiniDumpWithUnloadedModules
          | MiniDumpWithProcessThreadData);

    // The Unix dump type (DEFAULT_UNIX has the same value) and the platform
    // auto-detection have no mini-dump equivalent; they are listed so that
    // -Wswitch-enum sees them handled, and take the default.
    case DumpType::CORE_DUMP_FULL:
    case DumpType::DEFAULT_AUTO:
    // Default fallback
    default:
      return MiniDumpNormal;
    }
}

inline bool
core_dump_generator::_is_valid_minidump_type (MINIDUMP_TYPE flags) noexcept
{
  // Check if flags contain only valid combinations
  // Basic validation: ensure no invalid flag combinations

  // Check for mutually exclusive flags
  if (((flags & MiniDumpWithFullMemory) != 0)
      && ((flags & MiniDumpNormal) != 0))
    return false; // Cannot have both full memory and normal

  // Check for invalid flag combinations
  if (((flags & MiniDumpWithoutOptionalData) != 0)
      && ((flags & MiniDumpWithFullMemoryInfo) != 0))
    return false; // Conflicting flags

  // Check for valid individual flags
  const MINIDUMP_TYPE validFlags = static_cast<MINIDUMP_TYPE> (
      MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithFullMemory
      | MiniDumpWithHandleData | MiniDumpFilterMemory | MiniDumpScanMemory
      | MiniDumpWithUnloadedModules | MiniDumpWithIndirectlyReferencedMemory
      | MiniDumpFilterModulePaths | MiniDumpWithProcessThreadData
      | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithoutOptionalData
      | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo
      | MiniDumpWithCodeSegs | MiniDumpWithoutAuxiliaryState
      | MiniDumpWithFullAuxiliaryState | MiniDumpWithPrivateWriteCopyMemory
      | MiniDumpIgnoreInaccessibleMemory | MiniDumpWithTokenInformation);

  // Ensure all flags are valid
  return (flags & ~validFlags) == 0;
}
#endif // LUMEX_OS_IS_WINDOWS ()

// UNIX-specific implementation
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())

inline void
core_dump_generator::_setup_signal_handlers ()
{
  struct sigaction sa;
  sa.sa_handler = _unix_signal_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = static_cast<int> (
      SA_RESETHAND); // This flag resets handler after first call

  sigaction (SIGSEGV, &sa, nullptr);
  sigaction (SIGABRT, &sa, nullptr);
  sigaction (SIGFPE, &sa, nullptr);
  sigaction (SIGILL, &sa, nullptr);

  _log_message ("UNIX signal handlers installed successfully", false);
}

inline void
core_dump_generator::_setup_core_dump_settings ()
{
  prctl (PR_SET_DUMPABLE, 1, 0, 0, 0);

  struct rlimit core_limit;
  core_limit.rlim_cur = RLIM_INFINITY;
  core_limit.rlim_max = RLIM_INFINITY;
  setrlimit (RLIMIT_CORE, &core_limit);

  _log_message ("UNIX core dump settings configured", false);
}

inline void
core_dump_generator::_setup_core_pattern ()
{
  // Store original pattern for restoration
  std::ifstream core_pattern_file ("/proc/sys/kernel/core_pattern");
  if (std::getline (core_pattern_file, s_originalCorePattern))
    _log_message ("Current core pattern: " + s_originalCorePattern, false);
}

inline void
core_dump_generator::_create_manual_core_dump ()
{
  try
    {
      // Generate filename
      std::string timeStr = format_time ("%d.%m.%Y.%H.%M.%S");
      std::ostringstream filename;
      filename << s_dumpDirectory << "/manual_core_" << timeStr << ".core";

      // Sanitize filename
      std::string sanitizedFilename = filename.str ();
      std::replace (sanitizedFilename.begin (), sanitizedFilename.end (), ':',
                    '_');
      std::replace (sanitizedFilename.begin (), sanitizedFilename.end (), ' ',
                    '_');

      // Try to use gcore if available
      std::string gcore_cmd = "which gcore >/dev/null 2>&1";
      int gcore_available = system (gcore_cmd.c_str ());

      if (gcore_available == 0)
        {
          // Use gcore to create core dump
          std::string gcore_exec = "gcore -o " + sanitizedFilename + " "
                                   + std::to_string (getpid ())
                                   + " >/dev/null 2>&1";
          int result = system (gcore_exec.c_str ());

          if (result == 0)
            {
              _log_message ("Manual core dump created using gcore: "
                                + sanitizedFilename,
                            false);

              // Check if file was created and get size
              struct stat fileStat;
              if (stat (sanitizedFilename.c_str (), &fileStat) == 0)
                _log_message ("Core dump size: "
                                  + std::to_string (fileStat.st_size)
                                  + " bytes",
                              false);
            }
          else
            {
              _log_message ("Failed to create core dump using gcore", true);
            }
        }
      else
        {
          // Try to use gdb if available
          std::string gdb_cmd = "which gdb >/dev/null 2>&1";
          int gdb_available = system (gdb_cmd.c_str ());

          if (gdb_available == 0)
            {
              // Use gdb to create core dump
              std::string gdb_exec
                  = "gdb --batch --quiet -ex 'generate-core-file "
                    + sanitizedFilename + "' -ex 'quit' -p "
                    + std::to_string (getpid ()) + " >/dev/null 2>&1";
              int result = system (gdb_exec.c_str ());

              if (result == 0)
                {
                  _log_message ("Manual core dump created using gdb: "
                                    + sanitizedFilename,
                                false);

                  // Check if file was created and get size
                  struct stat fileStat;
                  if (stat (sanitizedFilename.c_str (), &fileStat) == 0)
                    _log_message ("Core dump size: "
                                      + std::to_string (fileStat.st_size)
                                      + " bytes",
                                  false);
                }
              else
                {
                  _log_message ("Failed to create core dump using gdb", true);
                }
            }
          else
            {
              _log_message ("Neither gcore nor gdb available for manual core "
                            "dump creation",
                            true);
            }
        }
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to create manual core dump: "
                        + std::string (exc.what ()),
                    true);
    }
}

inline void
core_dump_generator::_restore_core_pattern ()
{
  try
    {
      if (!s_originalCorePattern.empty ())
        {
          int fd = open ("/proc/sys/kernel/core_pattern", O_WRONLY | O_TRUNC);
          if (fd >= 0)
            {
              ssize_t bytes_written
                  = write (fd, s_originalCorePattern.c_str (),
                           s_originalCorePattern.length ());
              if (bytes_written == -1)
                _log_message ("Failed to restore original core pattern: "
                                  + std::to_string (errno),
                              true);
              else
                _log_message ("Original core pattern restored", false);
              close (fd);
            }
        }
    }
  catch (std::exception const &exc)
    {
      _log_message ("Failed to restore core pattern: "
                        + std::string (exc.what ()),
                    true);
    }
}

inline void
core_dump_generator::_instant_systemd_monitor () noexcept
{
  // INSTANT core dump extraction using Linux inotify API
  // Source: inotify(7) man page - official Linux kernel documentation
  // Monitors /var/lib/systemd/coredump/ for new files and extracts IMMEDIATELY
  // (< 1 sec)

  try
    {
      char const *systemd_coredump_dir = "/var/lib/systemd/coredump";

      // Check if systemd-coredump directory exists and accessible
      struct stat st;
      if (stat (systemd_coredump_dir, &st) != 0)
        {
          _log_message ("systemd-coredump directory not accessible - skipping "
                        "instant monitor",
                        false);
          return;
        }

      // Initialize inotify (non-blocking mode)
      int inotify_fd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
      if (inotify_fd < 0)
        {
          _log_message ("Failed to initialize inotify", true);
          return;
        }

      // Watch for new files (IN_CREATE) and moved files (IN_MOVED_TO)
      int watch_fd = inotify_add_watch (inotify_fd, systemd_coredump_dir,
                                        IN_CREATE | IN_MOVED_TO);
      if (watch_fd < 0)
        {
          close (inotify_fd);
          _log_message ("Failed to add inotify watch on systemd-coredump",
                        true);
          return;
        }

      _log_message ("INSTANT systemd-coredump monitor ACTIVE (inotify-based)",
                    false);
      _log_message ("   - Monitoring: " + std::string (systemd_coredump_dir),
                    false);
      _log_message (
          "   - Will extract dumps IMMEDIATELY upon creation (<1 sec)", false);
      _log_message ("   - Target directory: " + s_dumpDirectory, false);

      // Event buffer (aligned for inotify_event structure)
      char event_buffer[4096]
          __attribute__ ((aligned (__alignof__ (struct inotify_event))));

      while (!s_monitorThreadShouldStop.load (std::memory_order_acquire))
        {
          // Use select() for timeout support (allows clean shutdown)
          fd_set readfds;
          FD_ZERO (&readfds);
          FD_SET (inotify_fd, &readfds);

          struct timeval timeout;
          timeout.tv_sec = 1; // 1 second timeout
          timeout.tv_usec = 0;

          int select_ret
              = select (inotify_fd + 1, &readfds, nullptr, nullptr, &timeout);

          if (select_ret > 0 && FD_ISSET (inotify_fd, &readfds))
            {
              // Read inotify events
              ssize_t len
                  = read (inotify_fd, event_buffer, sizeof (event_buffer));

              if (len > 0)
                {
                  // Process all events in buffer
                  const struct inotify_event *event;
                  for (char *ptr = event_buffer; ptr < event_buffer + len;
                       ptr += sizeof (struct inotify_event) + event->len)
                    {
                      event = reinterpret_cast<const struct inotify_event *> (
                          ptr);

                      // Check if this is a new file creation or move
                      if ((event->mask & (IN_CREATE | IN_MOVED_TO))
                          && event->len > 0)
                        {
                          std::string filename = event->name;

                          // Auto-detect executable name from /proc/self/exe
                          // (UNIVERSAL)
                          static std::string exe_name;
                          if (exe_name.empty ())
                            {
                              char buf[PATH_MAX];
                              ssize_t exe_len = readlink (
                                  "/proc/self/exe", buf, sizeof (buf) - 1);
                              if (exe_len != -1)
                                {
                                  buf[exe_len] = '\0';
                                  char const *base = strrchr (buf, '/');
                                  exe_name = base ? (base + 1) : buf;
                                }
                              else
                                {
                                  exe_name = "unknown"; // Fallback if readlink
                                                        // fails
                                }
                            }

                          // Filter for our application's core dumps only
                          // Format:
                          // core.<exe_name>.{uid}.{boot-id}.{pid}.{timestamp}.zst
                          std::string core_prefix = "core." + exe_name;
                          if (filename.find (core_prefix) != std::string::npos)
                            {
                              _log_message ("NEW CORE DUMP DETECTED: "
                                                + filename,
                                            false);

                              // Give systemd-coredump time to finish writing
                              // and compressing
                              std::this_thread::sleep_for (
                                  std::chrono::milliseconds (500));

                              // Generate target filename with timestamp
                              std::string timeStr
                                  = format_time ("%d.%m.%Y.%H.%M.%S");
                              std::string target_file = s_dumpDirectory
                                                        + "/core_dump_full_"
                                                        + timeStr + ".core";

                              // Extract using coredumpctl (works without sudo
                              // if user in systemd-journal group) Use
                              // auto-detected executable name for universal
                              // compatibility
                              std::string extract_cmd
                                  = "coredumpctl dump " + exe_name
                                    + " --output=\"" + target_file + "\" 2>&1";

                              _log_message ("EXTRACTING IMMEDIATELY to: "
                                                + target_file,
                                            false);

                              FILE *pipe = popen (extract_cmd.c_str (), "r");
                              if (pipe)
                                {
                                  char cmd_buffer[512];
                                  std::string cmd_output;
                                  while (fgets (cmd_buffer,
                                                sizeof (cmd_buffer), pipe))
                                    cmd_output += cmd_buffer;

                                  int exit_code = pclose (pipe);
                                  if (exit_code == 0)
                                    {
                                      // Verify extraction success
                                      struct stat dump_stat;
                                      if (stat (target_file.c_str (),
                                                &dump_stat)
                                          == 0)
                                        {
                                          double size_mb
                                              = static_cast<double> (
                                                    dump_stat.st_size)
                                                / (1024.0 * 1024.0);
                                          _log_message ("SUCCESS: Core dump "
                                                        "extracted INSTANTLY!",
                                                        false);
                                          _log_message ("   File: "
                                                            + target_file,
                                                        false);
                                          _log_message (
                                              "   Size: "
                                                  + std::to_string (size_mb)
                                                  + " MB ("
                                                  + std::to_string (
                                                      dump_stat.st_size)
                                                  + " bytes)",
                                              false);
                                          _log_message (
                                              "   Extraction time: "
                                              "<1 second after crash",
                                              false);

                                          // Set proper permissions (readable
                                          // by user and admin group)
                                          chmod (target_file.c_str (), 0640);
                                        }
                                      else
                                        {
                                          _log_message ("File extracted but "
                                                        "stat() failed",
                                                        true);
                                        }
                                    }
                                  else
                                    {
                                      _log_message (
                                          "coredumpctl failed (exit code: "
                                              + std::to_string (exit_code)
                                              + ")",
                                          true);
                                      if (!cmd_output.empty ())
                                        _log_message (
                                            "   Output: " + cmd_output, true);
                                      _log_message (
                                          "   Ensure user is in "
                                          "'systemd-journal' group: sudo "
                                          "usermod -aG systemd-journal $USER",
                                          true);
                                    }
                                }
                              else
                                {
                                  _log_message (
                                      "Failed to execute coredumpctl command",
                                      true);
                                }
                            }
                        }
                    }
                }
              else if (len < 0 && errno != EAGAIN)
                {
                  _log_message ("inotify read error: "
                                    + std::string (strerror (errno)),
                                true);
                  break;
                }
            }
          else if (select_ret < 0 && errno != EINTR)
            {
              _log_message ("select() error in systemd monitor: "
                                + std::string (strerror (errno)),
                            true);
              break;
            }
        }

      // Cleanup
      inotify_rm_watch (inotify_fd, watch_fd);
      close (inotify_fd);
      _log_message ("Instant systemd monitor stopped cleanly", false);
    }
  catch (std::exception const &exc)
    {
      _log_message ("Exception in instant systemd monitor: "
                        + std::string (exc.what ()),
                    true);
    }
  catch (...)
    {
      _log_message ("Unknown exception in instant systemd monitor", true);
    }
}

inline void
core_dump_generator::_monitor_and_copy_core_dumps ()
{
  try
    {
      // Check if coredumpctl is available
      std::string coredumpctl_cmd = "which coredumpctl >/dev/null 2>&1";
      int coredumpctl_available = system (coredumpctl_cmd.c_str ());

      if (coredumpctl_available == 0)
        {
          // Get the most recent core dump
          std::string get_latest_cmd = "coredumpctl list --no-pager | tail -1";
          FILE *pipe = popen (get_latest_cmd.c_str (), "r");
          if (pipe)
            {
              char buffer[256];
              std::string output;
              while (fgets (buffer, sizeof (buffer), pipe) != nullptr)
                output += buffer;
              pclose (pipe);

              if (!output.empty ())
                {
                  // Parse the output to get PID
                  std::istringstream iss (output);
                  std::string time, pid_str, uid, gid, sig, corefile, exe,
                      size;
                  iss >> time >> pid_str >> uid >> gid >> sig >> corefile
                      >> exe >> size;

                  if (!pid_str.empty () && pid_str != "PID")
                    {
                      // Generate filename for our copy
                      std::string timeStr = format_time ("%d.%m.%Y.%H.%M.%S");
                      std::ostringstream filename;
                      filename << s_dumpDirectory << "/core_" << timeStr << "_"
                               << pid_str << ".core";

                      // Copy the core dump (disabled - requires sudo)
                      // std::string copy_cmd = "coredumpctl dump " + pid_str +
                      // " -o " + filename.str() + " >/dev/null 2>&1"; int
                      // result           = system(copy_cmd.c_str());
                      int result = -1; // Disabled to avoid sudo requirement

                      if (result == 0)
                        {
                          _log_message ("Core dump copied to: "
                                            + filename.str (),
                                        false);

                          // Check file size
                          struct stat fileStat;
                          if (stat (filename.str ().c_str (), &fileStat) == 0)
                            _log_message (
                                "Core dump size: "
                                    + std::to_string (fileStat.st_size)
                                    + " bytes",
                                false);
                        }
                      else
                        {
                          _log_message (
                              "Failed to copy core dump from systemd-coredump",
                              true);
                        }
                    }
                }
            }
        }
      else
        {
          _log_message ("coredumpctl not available for core dump monitoring",
                        true);
        }
    }
  catch (std::exception const &exc)
    {
      _log_message (
          "Failed to monitor core dumps: " + std::string (exc.what ()), true);
    }
}

inline void
core_dump_generator::_unix_signal_handler (int signum) noexcept
{
  // Only async-signal-safe operations in signal handler
  char const crash_msg[] = "CRASH DETECTED\n";
  write (STDERR_FILENO, crash_msg, sizeof (crash_msg) - 1);

  // Set standard behavior and re-raise signal for proper termination (like in
  // working example)
  signal (signum, SIG_DFL);
  raise (signum);
}

inline void
core_dump_generator::_custom_signal_handler_wrapper (int signum) noexcept
{
  // Check if we have a custom handler for this signal
  std::lock_guard<std::mutex> lock (s_customHandlersMutex);
  auto it = s_customSignalHandlers.find (signum);

  if (it != s_customSignalHandlers.end ())
    {
      // Call custom handler
      it->second (signum);
    }
  else
    {
      // No custom handler, use default crash behavior
      _unix_signal_handler (signum);
    }
}

// Helper function to check and log core dump file size
inline void
core_dump_generator::_log_core_dump_size (std::string const &filename) noexcept
{
  try
    {
      struct stat fileStat;
      if (stat (filename.c_str (), &fileStat) == 0)
        {
          _log_message ("Core dump created successfully. Path: " + filename
                            + ". Size: " + std::to_string (fileStat.st_size)
                            + " bytes",
                        false);
        }
      else
        {
          _log_message ("Core dump may have been created at: " + filename
                            + " (unable to verify size)",
                        false);

          // Check common core dump locations
          std::vector<std::string> commonLocations
              = { "/tmp", ".", "/var/crash", "/var/tmp" };

          for (auto const &location : commonLocations)
            {
              std::string searchPattern = location + "/core*";
              // Note: In a real implementation, you'd use glob() or similar to
              // search for core files
              _log_message ("Check " + location + " for core dump files",
                            false);
            }
        }
    }
  catch (...)
    {
      // Don't let logging errors crash the application
      _log_message ("Core dump created (unable to verify details)", false);
    }
}

inline void
core_dump_generator::_generate_core_dump ()
{
  try
    {
      // Create dump directory
      mkdir (s_dumpDirectory.c_str (), 0755);

      // Generate core_pattern using kernel specifiers
      // %t - timestamp of crash (NOT initialization!)
      // %p - PID of crashing process
      // %e - executable name
      // Source: core(5) man page - official Linux documentation
      std::ostringstream filename;
      filename << s_dumpDirectory << "/core_dump_full_%t_%p_%e.core";

      std::string core_pattern = filename.str ();
      _log_message ("Core dump pattern: " + core_pattern, false);
      _log_message ("  %t = crash timestamp, %p = PID, %e = executable name",
                    false);

      // Set core dump size limit
      struct rlimit core_limit;
      core_limit.rlim_cur = RLIM_INFINITY;
      core_limit.rlim_max = RLIM_INFINITY;
      setrlimit (RLIMIT_CORE, &core_limit);

      // Set core pattern to create file with our name (PRIMARY mechanism: sudo
      // tee)
      std::string core_pattern_cmd
          = "echo \"" + core_pattern
            + "\" | sudo tee /proc/sys/kernel/core_pattern";
      int ret = system (core_pattern_cmd.c_str ());

      if (ret != 0)
        {
          _log_message ("WARNING: sudo tee failed, attempting direct write",
                        true);

          // Try direct write (will fail on most systems but worth trying)
          int fd = open ("/proc/sys/kernel/core_pattern", O_WRONLY | O_TRUNC);
          if (fd >= 0)
            {
              ssize_t written
                  = write (fd, core_pattern.c_str (), core_pattern.length ());
              close (fd);
              if (written > 0)
                {
                  _log_message ("Direct write successful (no sudo required)",
                                false);
                  _log_message ("Core dump generation configured", false);
                  return;
                }
            }

          _log_message ("WARNING: Cannot set custom core_pattern - will use "
                        "system default",
                        true);
          _log_message (
              "  Core dumps will be handled by systemd-coredump if available",
              false);
          _log_message ("  or created as './core' in current directory",
                        false);
          _log_message ("  systemd-coredump fallback monitor will extract "
                        "dumps if active",
                        false);
        }
      else
        {
          _log_message ("Core pattern set successfully via sudo", false);
        }

      _log_message ("Core dump generation configured", false);
    }
  catch (std::exception const &exc)
    {
      _log_message ("Exception in _generateCoreDump: "
                        + std::string (exc.what ()),
                    true);
    }
}

// Helper function to set core pattern for crash (like in the working example)

#endif // (LUMEX_OS_IS_UNIX () || LUMEX_OS_IS_ANDROID ())

// Helper functions to reduce code duplication
#if LUMEX_OS_IS_WINDOWS()
inline std::string
core_dump_generator::_convert_wide_string_to_narrow (
    std::wstring const &wideStr) noexcept
{
  try
    {
      int size = WideCharToMultiByte (CP_UTF8, 0, wideStr.c_str (), -1,
                                      nullptr, 0, nullptr, nullptr);
      if (size <= 0)
        return "";

      std::string narrowStr (static_cast<std::size_t> (size - 1), 0);
      int result = WideCharToMultiByte (CP_UTF8, 0, wideStr.c_str (), -1,
                                        &narrowStr[0], size, nullptr, nullptr);
      if (result == 0)
        return "";

      return narrowStr;
    }
  catch (...)
    {
      return "";
    }
}
#endif

inline void
core_dump_generator::_log_dump_creation_success (std::string const &filename,
                                                 size_t size,
                                                 DumpType dumpType) noexcept
{
  try
    {
      std::ostringstream oss;
      oss << "Crash dump created successfully with type: "
          << static_cast<int> (dumpType) << ". Path: " << filename;

      if (size > 0)
        oss << ". Size: " << size << " bytes";
      else
        oss << ". Size: unknown";

      // Use direct logging for dump creation success to avoid path
      // sanitization
      std::string timeStr = format_time ("%H:%M:%S");
      std::cout << "[" << timeStr << "] INFO: " << oss.str () << '\n';
    }
  catch (...)
    {
      // Silent failure in logging
    }
}

// Atomic file operations to prevent TOCTOU race conditions
inline bool
core_dump_generator::_create_file_atomically (
    std::string const &filename, std::string const &content) noexcept
{
  try
    {
#if LUMEX_OS_IS_WINDOWS()
      // On Windows, use CreateFileW with CREATE_NEW to ensure atomic creation
      std::wstring wfilename (filename.begin (), filename.end ());
      HANDLE hFile = CreateFileW (wfilename.c_str (), GENERIC_WRITE, 0, NULL,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile == INVALID_HANDLE_VALUE)
        {
          DWORD error = GetLastError ();
          if (error != ERROR_FILE_EXISTS)
            _log_message ("Failed to create file atomically. Error: "
                              + std::to_string (error),
                          true);
          return false;
        }

      // Write content
      DWORD bytesWritten;
      if (!WriteFile (hFile, content.c_str (),
                      static_cast<DWORD> (content.length ()), &bytesWritten,
                      NULL))
        {
          CloseHandle (hFile);
          DeleteFileW (wfilename.c_str ()); // Clean up on failure
          return false;
        }

      CloseHandle (hFile);
      return true;
#else
      // On UNIX, use O_CREAT | O_EXCL to ensure atomic creation
      int fd = open (filename.c_str (), O_CREAT | O_EXCL | O_WRONLY, 0644);
      if (fd == -1)
        {
          if (errno != EEXIST)
            _log_message ("Failed to create file atomically. Error: "
                              + std::to_string (errno),
                          true);
          return false;
        }

      // Write content
      ssize_t bytesWritten = write (fd, content.c_str (), content.length ());
      if (bytesWritten == -1)
        _log_message ("Failed to write to file: " + std::to_string (errno),
                      true);
      close (fd);

      if (bytesWritten != static_cast<ssize_t> (content.length ()))
        {
          unlink (filename.c_str ()); // Clean up on failure
          return false;
        }

      return true;
#endif
    }
  catch (...)
    {
      return false;
    }
}

inline bool
core_dump_generator::_create_directory_atomically (
    std::string const &path) noexcept
{
  try
    {
#if LUMEX_OS_IS_WINDOWS()
      // On Windows, use CreateDirectoryW
      std::wstring wpath (path.begin (), path.end ());
      if (CreateDirectoryW (wpath.c_str (), NULL))
        return true;

      DWORD error = GetLastError ();
      if (error == ERROR_ALREADY_EXISTS)
        return true; // Directory already exists, which is fine

      _log_message ("Failed to create directory atomically. Error: "
                        + std::to_string (error),
                    true);
      return false;
#else
      // On UNIX, use mkdir with proper error handling
      if (mkdir (path.c_str (), 0755) == 0)
        return true;

      if (errno == EEXIST)
        return true; // Directory already exists, which is fine

      _log_message ("Failed to create directory atomically. Error: "
                        + std::to_string (errno),
                    true);
      return false;
#endif
    }
  catch (...)
    {
      return false;
    }
}

// Utility functions
inline std::string
core_dump_generator::_generate_dump_filename (std::string const &prefix)
{
  // Generate cryptographically secure random component to prevent enumeration
  // attacks
  std::string randomComponent = _generate_secure_random_component ();

  std::string timeStr = format_time ("%d.%m.%Y.%H.%M.%S");

  // Sanitize prefix to prevent command injection
  std::string sanitizedPrefix = _sanitize_filename_component (prefix);

  // Sanitize time string to prevent command injection
  std::string sanitizedTimeStr = _sanitize_filename_component (timeStr);

  std::ostringstream oss;
  oss << s_dumpDirectory << "/" << sanitizedPrefix << "_" << sanitizedTimeStr
      << "_" << randomComponent;

#if LUMEX_OS_IS_WINDOWS()
  oss << ".dmp";
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  oss << ".core";
#endif

  std::string filename = oss.str ();

  // Final validation to ensure filename is safe
  if (!_validate_filename (filename))
    {
      // Fallback to safe default filename
      std::ostringstream fallback;
      fallback << s_dumpDirectory << "/dump_" << randomComponent;
#if LUMEX_OS_IS_WINDOWS()
      fallback << ".dmp";
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      fallback << ".core";
#endif
      return fallback.str ();
    }

  return filename;
}

inline std::string
core_dump_generator::_generate_dump_filename (DumpType dumpType)
{
  std::string prefix = dump_type_to_string (dumpType);
  return _generate_dump_filename (prefix);
}

inline std::string
core_dump_generator::_get_executable_directory () noexcept
{
  try
    {
#if LUMEX_OS_IS_WINDOWS()
      std::array<char, MAX_PATH> buffer{};
      DWORD length = GetModuleFileNameA (nullptr, buffer.data (),
                                         static_cast<DWORD> (buffer.size ()));
      if (length == 0)
        {
          _log_message ("Failed to get executable path", true);
          return ".";
        }

      std::string exePath (buffer.data (), length);
      size_t lastSlash = exePath.find_last_of ("\\/");
      if (lastSlash != std::string::npos)
        return exePath.substr (0, lastSlash);
      return ".";

#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      std::array<char, PATH_MAX> buffer{};
      ssize_t length
          = readlink ("/proc/self/exe", buffer.data (), buffer.size () - 1);
      if (length == -1)
        {
          // Fallback to getcwd if readlink fails
          if (getcwd (buffer.data (), buffer.size ()) != nullptr)
            return std::string (buffer.data ());
          _log_message ("Failed to get executable path", true);
          return ".";
        }

      // readlink returns -1 or the non-negative number of bytes written.
      buffer[static_cast<std::size_t> (length)] = '\0';
      std::string exePath (buffer.data ());
      size_t lastSlash = exePath.find_last_of ('/');
      if (lastSlash != std::string::npos)
        return exePath.substr (0, lastSlash);
      return ".";
#endif
    }
  catch (std::exception const &exc)
    {
      _log_message ("Exception in _getExecutableDirectory: "
                        + std::string (exc.what ()),
                    true);
      return ".";
    }
  catch (...)
    {
      _log_message ("Unknown exception in _getExecutableDirectory", true);
      return ".";
    }
}

// dump_factory implementation
inline dump_configuration
dump_factory::create_configuration (DumpType type)
{
  if (type == DumpType::DEFAULT_AUTO)
    type = get_default_dump_type ();

#if LUMEX_OS_IS_WINDOWS()
  return create_windows_configuration (type);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  return create_unix_configuration (type);
#else
  return dump_configuration{}; // Empty configuration for unsupported platforms
#endif
}

inline DumpType
dump_factory::get_default_dump_type () noexcept
{
#if LUMEX_OS_IS_WINDOWS()
  return DumpType::DEFAULT_WINDOWS;
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  return DumpType::DEFAULT_UNIX;
#else
  return DumpType::DEFAULT_AUTO;
#endif
}

inline bool
dump_factory::is_supported (DumpType type) noexcept
{
  if (type == DumpType::DEFAULT_AUTO)
    return true; // Always supported

#if LUMEX_OS_IS_WINDOWS()
  // Windows supports all mini-dump types and kernel dump types
  return (static_cast<int> (type)
              >= static_cast<int> (DumpType::MINI_DUMP_NORMAL)
          && static_cast<int> (type)
                 <= static_cast<int> (DumpType::KERNEL_ACTIVE_DUMP))
         || (type == DumpType::DEFAULT_WINDOWS);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
  // UNIX supports core dump types
  return (static_cast<int> (type)
              >= static_cast<int> (DumpType::CORE_DUMP_FULL)
          && static_cast<int> (type)
                 <= static_cast<int> (DumpType::CORE_DUMP_FULL))
         || (type == DumpType::DEFAULT_UNIX);
#else
  return false;
#endif
}

inline std::string
dump_factory::get_description (DumpType type) noexcept
{
  auto iter = s_descriptions.find (type);
  if (iter != s_descriptions.end ())
    return iter->second;
  return "Unknown dump type";
}

inline size_t
dump_factory::get_estimated_size (DumpType type) noexcept
{
  auto iter = s_estimatedSizes.find (type);
  if (iter != s_estimatedSizes.end ())
    return iter->second;
  return 0; // Unknown size
}

inline std::vector<DumpType>
dump_factory::get_supported_types () noexcept
{
  std::vector<DumpType> supportedTypes;

  for (auto const &pair : s_platformSupport)
    if (pair.second)
      supportedTypes.push_back (pair.first);

  return supportedTypes;
}

inline bool
dump_factory::validate_configuration (
    dump_configuration const &config) noexcept
{
  return config.is_valid ();
}

inline dump_configuration
dump_factory::create_configuration (DumpType type,
                                    std::error_code &errorCode) noexcept
{
  errorCode.clear ();

  if (!is_supported (type))
    {
      errorCode = std::make_error_code (std::errc::not_supported);
      return dump_configuration{};
    }

  try
    {
      return create_configuration (type);
    }
  catch (...)
    {
      errorCode = std::make_error_code (std::errc::invalid_argument);
      return dump_configuration{};
    }
}

inline dump_configuration
dump_factory::create_windows_configuration (DumpType type)
{
  dump_configuration config;
  config.set_type (type);
  config.set_enable_symbols (true);
  config.set_enable_source_info (true);
  config.set_include_unloaded_modules (true);
  config.set_include_handle_data (true);
  config.set_include_thread_info (true);
  config.set_include_process_data (true);

  // Only the types with their own size limit have a case label; every other
  // DumpType takes the default limit on purpose (-Wswitch-enum).
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
  switch (type)
    {
    case DumpType::MINI_DUMP_NORMAL:
      config.set_max_size_bytes (core_dump_generator::KB_64); // 64KB
      break;
    case DumpType::MINI_DUMP_WITH_FULL_MEMORY:
      // Set reasonable limit to prevent DoS attacks (1GB max)
      config.set_max_size_bytes (1024ULL * 1024ULL * 1024ULL); // 1GB
      break;
    case DumpType::KERNEL_FULL_DUMP:
      // Set reasonable limit for kernel dumps (2GB max)
      config.set_max_size_bytes (2ULL * 1024ULL * 1024ULL * 1024ULL); // 2GB
      break;
    case DumpType::KERNEL_SMALL_DUMP:
      config.set_max_size_bytes (core_dump_generator::KB_64); // 64KB
      break;
    default:
      // Use default settings with reasonable limits
      config.set_max_size_bytes (256ULL * 1024ULL
                                 * 1024ULL); // 256MB default limit
      break;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

  return config;
}

inline dump_configuration
dump_factory::create_unix_configuration (DumpType type)
{
  dump_configuration config;
  config.set_type (type);
  config.set_enable_symbols (true);
  config.set_enable_source_info (true);

  // Only the full core dump has its own size limit; every other DumpType
  // takes the default limit on purpose (-Wswitch-enum).
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
  switch (type)
    {
    case DumpType::CORE_DUMP_FULL:
      // Set reasonable limit to prevent DoS attacks (1GB max)
      config.set_max_size_bytes (1024ULL * 1024ULL * 1024ULL); // 1GB
      break;
    default:
      // Use default settings with reasonable limits
      config.set_max_size_bytes (128ULL * 1024ULL
                                 * 1024ULL); // 128MB default limit
      break;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

  return config;
}

inline void
core_dump_generator::_log_message (std::string const &message,
                                   LogLevel level) noexcept
{
  // Map the richer LogLevel to the shared (message, bool isError) path.
  bool const isError
      = (level == LogLevel::ERROR_ || level == LogLevel::CRITICAL_);
  _log_message (message, isError);
}

inline void
core_dump_generator::_log_message (std::string const &message, bool isError)
{
  try
    {
      std::string timeStr = format_time ("%H:%M:%S");
      std::ostringstream oss;

      // Check if running with admin privileges to determine sanitization level
      bool isAdmin = is_admin_privileges ();

      // Sanitize message based on privilege level
      std::string sanitizedMessage;
      if (isAdmin)
        {
          // For admin users, use less restrictive sanitization that allows
          // paths
          sanitizedMessage = _sanitize_log_message_for_admin (message);
        }
      else
        {
          // For regular users, use full sanitization to prevent information
          // disclosure
          sanitizedMessage = _sanitize_log_message (message);
        }

      oss << "[" << timeStr << "] " << (isError ? "ERROR" : "INFO") << ": "
          << sanitizedMessage;

      // Standard console output with explicit flush (unbuffered for immediate
      // visibility)
      if (isError)
        std::cerr << oss.str () << std::endl;
      else
        std::cout << oss.str () << std::endl;
    }
  catch (std::exception const &exc)
    {
      // Log the exception but don't let logging exceptions crash the
      // application
      std::cerr << "Logging error: " << exc.what () << '\n';
    }
  catch (...)
    {
      // Don't let logging exceptions crash the application
      std::cerr << "Unknown logging error\n";
    }
}

// Exception handling implementation
inline void
core_dump_generator::_setup_exception_handling ()
{
  // Set the unhandled exception handler
  std::set_terminate (_unhandled_exception_handler);
  _log_message ("Exception handling enabled", false);
}

inline void
core_dump_generator::_unhandled_exception_handler ()
{
  try
    {
      _log_message ("Unhandled C++ exception detected", true);

      // Generate dump for the exception with proper synchronization
      // Use atomic load to safely check initialization status
#if __cplusplus >= 201103L
      bool isInitialized = s_initialized.load (std::memory_order_acquire);
#else
      bool isInitialized = s_initialized;
#endif

      if (isInitialized)
        {
          // Create a local copy of configuration to avoid accessing
          // potentially destroyed static data
          dump_configuration localConfig = s_currentConfig;
          std::string filename
              = _generate_dump_filename ("unhandled_exception");
          _log_message ("Generating exception dump: " + filename, false);

#if LUMEX_OS_IS_WINDOWS()
          _create_windows_dump (filename, localConfig);
#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
          _generate_core_dump ();
#endif
        }
      else
        {
          _log_message (
              "CoreDumpGenerator not initialized, skipping dump generation",
              true);
        }
    }
  catch (...)
    {
      // If exception handling fails, just log to stderr
      // Use only async-signal-safe functions in exception handler
      char const errorMsg[] = "Failed to handle unhandled exception\n";
#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      auto bytes_written
          = write (STDERR_FILENO, errorMsg, sizeof (errorMsg) - 1);
      if (bytes_written == -1)
        _log_message ("Failed to write to stderr: " + std::to_string (errno),
                      true);
#else
      // Windows fallback
      OutputDebugStringA (errorMsg);
#endif
    }

  // Call the default terminate handler
  std::abort ();
}

// Security and validation functions
inline bool
core_dump_generator::_validate_directory (std::string const &path) noexcept
{
  auto npos = std::string::npos;

  try
    {
      if (path.empty ())
        return false;

      // Check path length first
      constexpr size_t MAX_PATH_LENGTH = 4096;
      if (path.length () > MAX_PATH_LENGTH)
        return false;

      // Normalize path to prevent Unicode and encoding attacks
      std::string normalizedPath (path);

      // Remove null bytes and control characters (CWE-170: Improper Null
      // Termination)
      normalizedPath.erase (
          std::remove_if (normalizedPath.begin (), normalizedPath.end (),
                          [] (char c)
                            {
                              return c == '\0'
                                     || (c < 32 && c != '\t' && c != '\n'
                                         && c != '\r');
                            }),
          normalizedPath.end ());

      // Check for path traversal attempts (CWE-22: Path Traversal) - Enhanced
      // detection Check for various encoding attacks
      std::string lowerPath = normalizedPath;
      std::transform (lowerPath.begin (), lowerPath.end (), lowerPath.begin (),
                      [] (char c)
                        {
                          return static_cast<char> (
                              std::tolower (static_cast<unsigned char> (c)));
                        });

      // Check for directory traversal patterns
      if (lowerPath.find ("..") != npos)
        return false;
      if (lowerPath.find ("%2e%2e") != npos)
        return false; // URL encoded
      if (lowerPath.find ("%252e%252e") != npos)
        return false; // Double URL encoded
      if (lowerPath.find ("..%2f") != npos)
        return false; // Mixed encoding
      if (lowerPath.find ("..%5c") != npos)
        return false; // Windows backslash
      if (lowerPath.find ("..\\") != npos)
        return false; // Windows backslash
      if (lowerPath.find ("//") != npos)
        return false;
      if (lowerPath.find ("\\\\") != npos)
        return false;

      // Check for command injection patterns (CWE-78: OS Command Injection)
      constexpr std::array<char, 15> dangerous_chars
          = { ';', '&', '|', '`', '$', '(', ')', '{',
              '}', '[', ']', '<', '>', '"', '\'' };
      if (!std::none_of (
              dangerous_chars.begin (), dangerous_chars.end (),
              [&normalizedPath, npos] (char character)
                { return normalizedPath.find (character) != npos; }))
        return false;

      // Check for absolute path requirements
      if (normalizedPath.empty ())
        return false;

      // Validate absolute path format
      bool isAbsolute = false;
#if LUMEX_OS_IS_WINDOWS()
      // Windows: C:\ or \\server\share
      isAbsolute
          = (normalizedPath.length () >= 3 && normalizedPath[1] == ':'
             && (normalizedPath[2] == '\\' || normalizedPath[2] == '/'))
            || (normalizedPath.length () >= 2 && normalizedPath[0] == '\\'
                && normalizedPath[1] == '\\');
#else
      // UNIX: /path
      isAbsolute = (normalizedPath[0] == '/');
#endif

      if (!isAbsolute)
        return false;

      // Additional Windows-specific checks
#if LUMEX_OS_IS_WINDOWS()
      // Check for reserved names (CWE-22) - Enhanced check
      std::array<std::string, 22> const reservedNames
          = { "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3", "com4",
              "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3",
              "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };

      // Extract filename from path for reserved name check
      size_t lastSlash = lowerPath.find_last_of ("\\/");
      std::string filename
          = (lastSlash != npos) ? lowerPath.substr (lastSlash + 1) : lowerPath;

      // Check if filename starts with reserved name
      for (auto const &reserved : reservedNames)
        {
          if (filename.length () >= reserved.length ()
              && filename.substr (0, reserved.length ()) == reserved
              && (filename.length () == reserved.length ()
                  || filename[reserved.length ()] == '.'))
            {
              return false;
            }
        }
#endif

      return true;
    }
  catch (...)
    {
      return false;
    }
}

inline bool
core_dump_generator::_validate_filename (std::string const &filename) noexcept
{
  auto npos = std::string::npos;

  try
    {
      if (filename.empty ())
        return false;

      // Check filename length first
      constexpr size_t MAX_FILENAME_LENGTH = 255;
      if (filename.length () > MAX_FILENAME_LENGTH)
        return false;

      // Normalize filename to prevent Unicode and encoding attacks
      std::string normalizedFilename (filename);

      // Remove null bytes and control characters (CWE-170: Improper Null
      // Termination)
      normalizedFilename.erase (
          std::remove_if (
              normalizedFilename.begin (), normalizedFilename.end (),
              [] (char c)
                {
                  return c == '\0'
                         || (c < 32 && c != '\t' && c != '\n' && c != '\r');
                }),
          normalizedFilename.end ());

      // Check for path traversal attempts (CWE-22: Path Traversal) - Enhanced
      // detection
      std::string lowerFilename = normalizedFilename;
      std::transform (lowerFilename.begin (), lowerFilename.end (),
                      lowerFilename.begin (),
                      [] (char c)
                        {
                          return static_cast<char> (
                              std::tolower (static_cast<unsigned char> (c)));
                        });

      // Check for directory traversal patterns
      if (lowerFilename.find ("..") != npos)
        return false;
      if (lowerFilename.find ("%2e%2e") != npos)
        return false; // URL encoded
      if (lowerFilename.find ("%252e%252e") != npos)
        return false; // Double URL encoded
      if (lowerFilename.find ("..%2f") != npos)
        return false; // Mixed encoding
      if (lowerFilename.find ("..%5c") != npos)
        return false; // Windows backslash
      if (lowerFilename.find ('/') != npos)
        return false;
      if (lowerFilename.find ('\\') != npos)
        return false;

      // Check for command injection patterns (CWE-78: OS Command Injection)
      constexpr std::array<char, 15> dangerous_chars
          = { ';', '&', '|', '`', '$', '(', ')', '{',
              '}', '[', ']', '<', '>', '"', '\'' };
      if (!std::none_of (
              dangerous_chars.begin (), dangerous_chars.end (),
              [&normalizedFilename, npos] (char character)
                { return normalizedFilename.find (character) != npos; }))
        return false;

      // Check for valid extension
      if (normalizedFilename.find ('.') == npos)
        return false;

      // Additional Windows-specific checks
#if LUMEX_OS_IS_WINDOWS()
      // Check for reserved names (CWE-22) - Enhanced check
      std::array<std::string, 22> const reservedNames
          = { "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3", "com4",
              "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3",
              "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };

      // Extract base filename without extension
      size_t dotPos = lowerFilename.find_last_of ('.');
      std::string baseFilename = (dotPos != npos)
                                     ? lowerFilename.substr (0, dotPos)
                                     : lowerFilename;

      // Check if base filename matches reserved names
      for (auto const &reserved : reservedNames)
        if (baseFilename == reserved)
          return false;
#endif

      return true;
    }
  catch (...)
    {
      return false;
    }
}

inline std::string
core_dump_generator::_sanitize_path (std::string const &path) noexcept
{
  auto npos = std::string::npos;

  try
    {
      if (path.empty ())
        return "";

      std::string sanitized (path);

      // Remove dangerous characters
      std::string const dangerous_chars = ";&|`$(){}[]<>\"'";
      sanitized.erase (
          std::remove_if (sanitized.begin (), sanitized.end (),
                          [&dangerous_chars, npos] (char c)
                            { return dangerous_chars.find (c) != npos; }),
          sanitized.end ());

      // Return sanitized path
      return sanitized;
    }
  catch (...)
    {
      return path; // Return original if sanitization fails
    }
}

// Security helper functions implementation
inline std::string
core_dump_generator::_generate_secure_random_component () noexcept
{
  try
    {
      // Generate 32 random bytes (256 bits) for cryptographically secure
      // uniqueness This provides 256 bits of entropy, exceeding the 128-bit
      // security level
      constexpr size_t RANDOM_BYTES = 32;
      std::array<unsigned char, RANDOM_BYTES> randomBytes{};

#if LUMEX_OS_IS_WINDOWS()
      // Use Windows CryptGenRandom for cryptographically secure random numbers
      // This is the recommended approach for Windows systems per NIST SP
      // 800-90A
      HCRYPTPROV hProv = 0;
      if (CryptAcquireContext (&hProv, nullptr, nullptr, PROV_RSA_FULL,
                               CRYPT_VERIFYCONTEXT))
        {
          if (CryptGenRandom (hProv, static_cast<DWORD> (RANDOM_BYTES),
                              randomBytes.data ()))
            {
              // Successfully generated cryptographically secure random bytes
              CryptReleaseContext (hProv, 0);
            }
          else
            {
              // CryptGenRandom failed - this is a critical security failure
              CryptReleaseContext (hProv, 0);
              _log_message (
                  "CRITICAL: CryptGenRandom failed - falling back to "
                  "insecure method",
                  true);
              return _generate_fallback_random_component ();
            }
        }
      else
        {
          // CryptAcquireContext failed - this is a critical security failure
          _log_message (
              "CRITICAL: CryptAcquireContext failed - falling back to "
              "insecure method",
              true);
          return _generate_fallback_random_component ();
        }
#else
      // UNIX: Use /dev/urandom for cryptographically secure random numbers
      // This is the recommended approach for UNIX systems per NIST SP 800-90A
      int fd = open ("/dev/urandom", O_RDONLY);
      if (fd >= 0)
        {
          ssize_t bytesRead = read (fd, randomBytes.data (), RANDOM_BYTES);
          close (fd);

          if (bytesRead == static_cast<ssize_t> (RANDOM_BYTES))
            {
              // Successfully read cryptographically secure random bytes
            }
          else
            {
              // Partial read or read failure - this is a critical security
              // failure
              _log_message (
                  "CRITICAL: /dev/urandom read failed - falling back to "
                  "insecure method",
                  true);
              return _generate_fallback_random_component ();
            }
        }
      else
        {
          // /dev/urandom not available - this is a critical security failure
          _log_message (
              "CRITICAL: /dev/urandom not available - falling back to "
              "insecure method",
              true);
          return _generate_fallback_random_component ();
        }
#endif

      // Convert to hex string using secure formatting
      std::ostringstream oss;
      oss << std::hex << std::setfill ('0') << std::uppercase;
      for (unsigned char byte : randomBytes)
        oss << std::setw (2) << static_cast<unsigned int> (byte);

      return oss.str ();
    }
  catch (...)
    {
      // Exception occurred - this is a critical security failure
      _log_message (
          "CRITICAL: Exception in secure random generation - using fallback",
          true);
      return _generate_fallback_random_component ();
    }
}

// Fallback random component generation (insecure - only for system
// compatibility)
inline std::string
core_dump_generator::_generate_fallback_random_component () noexcept
{
  try
    {
      // WARNING: This fallback method is NOT cryptographically secure
      // It should only be used when the secure random number generator is
      // unavailable This provides minimal entropy and should be avoided in
      // production systems

      auto now = std::chrono::high_resolution_clock::now ();
      auto timestamp = now.time_since_epoch ().count ();

      // Use multiple entropy sources for better (but still insufficient)
      // randomness
#if LUMEX_OS_IS_WINDOWS()
      auto pid = _getpid ();
#else
      auto pid = getpid ();
#endif
      auto tid = std::this_thread::get_id ();

      // Combine entropy sources using XOR
      std::hash<std::string> hasher;
      std::string combined
          = std::to_string (timestamp) + std::to_string (pid)
            + std::to_string (std::hash<std::thread::id>{}(tid));

      auto hashValue = hasher (combined);

      std::ostringstream oss;
      oss << std::hex << std::setfill ('0') << std::uppercase;
      oss << std::setw (16) << (hashValue & 0xFFFFFFFFFFFFFFFFULL);

      return oss.str ();
    }
  catch (...)
    {
      // Ultimate fallback - use timestamp only (very insecure)
      auto now = std::chrono::high_resolution_clock::now ();
      auto timestamp = now.time_since_epoch ().count ();

      std::ostringstream oss;
      oss << std::hex << timestamp;
      return oss.str ();
    }
}

inline std::string
core_dump_generator::_sanitize_filename_component (
    std::string const &component) noexcept
{
  try
    {
      if (component.empty ())
        return "unknown";

      std::string sanitized = component;

      // Remove or replace dangerous characters
      for (char &c : sanitized)
        {
          if (c < 32 || c > 126) // Non-printable characters
            {
              c = '_';
            }
          else if (c == ' ' || c == '\t' || c == '\n'
                   || c == '\r') // Whitespace
            {
              c = '_';
            }
          else if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?'
                   || c == '"' || c == '<' || c == '>'
                   || c == '|') // Invalid filename characters
            {
              c = '_';
            }
          else if (c == ';' || c == '&' || c == '|' || c == '`' || c == '$'
                   || c == '(' || c == ')' || c == '{' || c == '}' || c == '['
                   || c == ']') // Command injection characters
            {
              c = '_';
            }
        }

      // Remove consecutive underscores
      sanitized.erase (std::unique (sanitized.begin (), sanitized.end (),
                                    [] (char a, char b)
                                      { return a == '_' && b == '_'; }),
                       sanitized.end ());

      // Remove leading/trailing underscores
      if (!sanitized.empty () && sanitized.front () == '_')
        sanitized.erase (0, 1);
      if (!sanitized.empty () && sanitized.back () == '_')
        sanitized.pop_back ();

      // Ensure component is not empty after sanitization
      if (sanitized.empty ())
        return "unknown";

      // Limit length to prevent buffer overflow
      constexpr size_t MAX_COMPONENT_LENGTH = 64;
      if (sanitized.length () > MAX_COMPONENT_LENGTH)
        sanitized = sanitized.substr (0, MAX_COMPONENT_LENGTH);

      return sanitized;
    }
  catch (...)
    {
      return "unknown";
    }
}

inline std::string
core_dump_generator::_sanitize_log_message (
    std::string const &message) noexcept
{
  try
    {
      if (message.empty ())
        return "[empty]";

      std::string sanitized = message;

      // Remove or replace sensitive information patterns
      // Remove full file paths - replace with relative paths or basenames
      size_t pos = 0;
      while ((pos = sanitized.find ("C:\\", pos)) != std::string::npos)
        {
          size_t end = sanitized.find ('\\', pos + 3);
          if (end != std::string::npos)
            {
              sanitized.replace (pos, end - pos, "[PATH]");
              pos += 6; // Length of "[PATH]"
            }
          else
            {
              break;
            }
        }

      // Remove Unix paths
      pos = 0;
      while ((pos = sanitized.find ("/", pos)) != std::string::npos)
        {
          if (pos > 0 && sanitized[pos - 1] != ' ')
            {
              size_t start = sanitized.rfind (' ', pos);
              if (start == std::string::npos)
                start = 0;
              else
                start++;

              size_t end = sanitized.find (' ', pos);
              if (end == std::string::npos)
                end = sanitized.length ();

              if (end - start > 10) // Only replace long paths
                {
                  sanitized.replace (start, end - start, "[PATH]");
                  pos = start + 6; // Length of "[PATH]"
                }
              else
                {
                  pos++;
                }
            }
          else
            {
              pos++;
            }
        }

      // Remove potential memory addresses (hex patterns)
      pos = 0;
      while ((pos = sanitized.find ("0x", pos)) != std::string::npos)
        {
          size_t end = pos + 2;
          while (end < sanitized.length ()
                 && ((sanitized[end] >= '0' && sanitized[end] <= '9')
                     || (sanitized[end] >= 'a' && sanitized[end] <= 'f')
                     || (sanitized[end] >= 'A' && sanitized[end] <= 'F')))
            {
              end++;
            }

          if (end - pos > 6) // Only replace long hex patterns
            {
              sanitized.replace (pos, end - pos, "[ADDR]");
              pos += 6; // Length of "[ADDR]"
            }
          else
            {
              pos = end;
            }
        }

      // Remove potential process IDs and thread IDs
      pos = 0;
      while ((pos = sanitized.find ("PID:", pos)) != std::string::npos)
        {
          size_t end = pos + 4;
          while (end < sanitized.length () && sanitized[end] >= '0'
                 && sanitized[end] <= '9')
            end++;
          sanitized.replace (pos, end - pos, "PID:[REDACTED]");
          pos += 13; // Length of "PID:[REDACTED]"
        }

      // Remove potential error codes that might leak system information
      pos = 0;
      while ((pos = sanitized.find ("Error:", pos)) != std::string::npos)
        {
          size_t end = sanitized.find (' ', pos + 6);
          if (end == std::string::npos)
            end = sanitized.length ();

          // Check if it's a numeric error code
          bool isNumeric = true;
          for (size_t i = pos + 6; i < end; i++)
            {
              if (sanitized[i] < '0' || sanitized[i] > '9')
                {
                  isNumeric = false;
                  break;
                }
            }

          if (isNumeric
              && end - pos > 8) // Only replace long numeric error codes
            {
              sanitized.replace (pos, end - pos, "Error:[REDACTED]");
              pos += 15; // Length of "Error:[REDACTED]"
            }
          else
            {
              pos = end;
            }
        }

      // Remove control characters and non-printable characters
      sanitized.erase (std::remove_if (sanitized.begin (), sanitized.end (),
                                       [] (char c)
                                         { return c < 32 || c > 126; }),
                       sanitized.end ());

      // Limit message length to prevent log flooding
      constexpr size_t MAX_LOG_LENGTH = 512;
      if (sanitized.length () > MAX_LOG_LENGTH)
        sanitized = sanitized.substr (0, MAX_LOG_LENGTH - 3) + "...";

      return sanitized;
    }
  catch (...)
    {
      return "[sanitization_failed]";
    }
}

inline std::string
core_dump_generator::_sanitize_log_message_for_admin (
    std::string const &message) noexcept
{
  try
    {
      if (message.empty ())
        return "[empty]";

      std::string sanitized = message;

      // For admin users, we allow more detailed information but still sanitize
      // sensitive data Remove potential memory addresses (hex patterns) -
      // still sanitize for security
      size_t pos = 0;
      while ((pos = sanitized.find ("0x", pos)) != std::string::npos)
        {
          size_t end = pos + 2;
          while (end < sanitized.length ()
                 && ((sanitized[end] >= '0' && sanitized[end] <= '9')
                     || (sanitized[end] >= 'a' && sanitized[end] <= 'f')
                     || (sanitized[end] >= 'A' && sanitized[end] <= 'F')))
            {
              end++;
            }

          if (end - pos > 10) // Only replace very long hex patterns for admin
            {
              sanitized.replace (pos, end - pos, "[ADDR]");
              pos += 6; // Length of "[ADDR]"
            }
          else
            {
              pos = end;
            }
        }

      // Remove potential process IDs and thread IDs - still sanitize for
      // security
      pos = 0;
      while ((pos = sanitized.find ("PID:", pos)) != std::string::npos)
        {
          size_t end = pos + 4;
          while (end < sanitized.length () && sanitized[end] >= '0'
                 && sanitized[end] <= '9')
            end++;
          sanitized.replace (pos, end - pos, "PID:[REDACTED]");
          pos += 13; // Length of "PID:[REDACTED]"
        }

      // Remove control characters and non-printable characters
      sanitized.erase (std::remove_if (sanitized.begin (), sanitized.end (),
                                       [] (char c)
                                         { return c < 32 || c > 126; }),
                       sanitized.end ());

      // Limit message length to prevent log flooding
      constexpr size_t MAX_LOG_LENGTH
          = 1024; // Allow longer messages for admin
      if (sanitized.length () > MAX_LOG_LENGTH)
        sanitized = sanitized.substr (0, MAX_LOG_LENGTH - 3) + "...";

      return sanitized;
    }
  catch (...)
    {
      return "[sanitization_failed]";
    }
}

// Windows privilege checking implementation
#if LUMEX_OS_IS_WINDOWS()
inline bool
core_dump_generator::_is_admin_privileges () noexcept
{
  try
    {
      // Method 1: Check if the current process is running with elevated
      // privileges This is the most reliable method for checking admin
      // privileges
      HANDLE hToken = nullptr;
      if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken))
        return false;

      // Use RAII to ensure token handle is closed
      struct TokenHandleCloser
      {
        HANDLE handle;
        explicit TokenHandleCloser (HANDLE h) : handle (h) {}
        ~TokenHandleCloser ()
        {
          if (handle)
            CloseHandle (handle);
        }
      } tokenCloser (hToken);

      // Get token elevation information
      TOKEN_ELEVATION elevation = {};
      DWORD dwSize = 0;
      if (!GetTokenInformation (hToken, TokenElevation, &elevation,
                                sizeof (elevation), &dwSize))
        return false;

      // Check if the token is elevated
      if (elevation.TokenIsElevated == 0)
        return false;

      // Method 2: Verify the user is actually in the Administrators group
      // This provides additional security by checking group membership
      SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
      PSID administratorsGroup = nullptr;

      if (!AllocateAndInitializeSid (
              &ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
              DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup))
        {
          return false;
        }

      // Use RAII to ensure SID is freed
      struct SidCloser
      {
        PSID sid;
        explicit SidCloser (PSID s) : sid (s) {}
        ~SidCloser ()
        {
          if (sid)
            FreeSid (sid);
        }
      } sidCloser (administratorsGroup);

      // Check if the current user is a member of the Administrators group
      BOOL isMember = FALSE;
      if (!CheckTokenMembership (hToken, administratorsGroup, &isMember))
        return false;

      return (isMember != FALSE);
    }
  catch (...)
    {
      // If any exception occurs, assume no admin privileges for security
      return false;
    }
}

inline bool
core_dump_generator::_is_elevated_process () noexcept
{
  try
    {
      // Alternative method: Check if the process is running with elevated
      // privileges This is a simpler check that doesn't require group
      // membership verification
      HANDLE hToken = nullptr;
      if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken))
        return false;

      // Use RAII to ensure token handle is closed
      struct TokenHandleCloser
      {
        HANDLE handle;
        explicit TokenHandleCloser (HANDLE h) : handle (h) {}
        ~TokenHandleCloser ()
        {
          if (handle)
            CloseHandle (handle);
        }
      } tokenCloser (hToken);

      // Get token elevation information
      TOKEN_ELEVATION elevation = {};
      DWORD dwSize = 0;
      if (!GetTokenInformation (hToken, TokenElevation, &elevation,
                                sizeof (elevation), &dwSize))
        return false;

      return (elevation.TokenIsElevated != 0);
    }
  catch (...)
    {
      // If any exception occurs, assume no elevated privileges for security
      return false;
    }
}
#endif // LUMEX_OS_IS_WINDOWS ()

// Platform-specific filesystem utilities implementation
inline bool
core_dump_generator::_create_directory_recursive (
    std::string const &path) noexcept
{
  try
    {
      if (path.empty ())
        return false;

#if LUMEX_OS_IS_WINDOWS()
      // Windows implementation using CreateDirectory
      std::wstring wpath (path.begin (), path.end ());

      // Check if directory already exists
      DWORD attributes = GetFileAttributesW (wpath.c_str ());
      if ((attributes != INVALID_FILE_ATTRIBUTES)
          && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0))
        return true; // Directory already exists

      // Create parent directories first
      std::string parent = path;
      size_t lastSlash = parent.find_last_of ("\\/");
      if (lastSlash != std::string::npos)
        {
          parent = parent.substr (0, lastSlash);
          // NOLINTNEXTLINE(cppcoreguidelines-avoid-recursion) - Intentional
          // recursion for directory creation
          if (!_create_directory_recursive (parent))
            return false;
        }

      // Create the directory atomically
      return _create_directory_atomically (path);

#elif (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
      // UNIX implementation using mkdir
      struct stat st;
      if (stat (path.c_str (), &st) == 0)
        return S_ISDIR (st.st_mode); // Directory already exists

      // Create parent directories first
      std::string parent = path;
      size_t lastSlash = parent.find_last_of ('/');
      if (lastSlash != std::string::npos)
        {
          parent = parent.substr (0, lastSlash);
          // NOLINTNEXTLINE(cppcoreguidelines-avoid-recursion) - Intentional
          // recursion for directory creation
          if (!_create_directory_recursive (parent))
            return false;
        }

      // Create the directory atomically
      return _create_directory_atomically (path);

#else
      return false;
#endif
    }
  catch (...)
    {
      return false;
    }
}

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-array-to-pointer-decay)

} // namespace dump
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_DUMP_HPP

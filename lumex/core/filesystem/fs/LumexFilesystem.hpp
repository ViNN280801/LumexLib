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
 * @file LumexFilesystem.hpp
 * @brief Cross-platform, header-only replacement for <filesystem> (C++17)
 * written in pure C++11.
 *
 * The design goals are:
 *   1. 100 % C++11-conformance – no compiler extensions, RTTI, or exceptions
 * required.
 *   2. No dynamic memory allocation inside core operations unless unavoidable.
 *   3. Zero-exception guarantee – all APIs return `filesystem_result<T>`
 * instead of throwing.
 *   4. SOLID-compliant, header-only implementation with clean separation of
 * concerns.
 *   5. Thread-safety for read-only operations (stat, exists, etc.).
 *
 *     @code
 *     using lumex::path;
 *     using lumex::filesystem;
 *
 *     // create tree recursively
 *     filesystem::create_directories(path("sandbox/dir/sub"));
 *
 *     // iterate
 *     for (lumex::directory_entry const& e :
 * filesystem::directory_contents(path("sandbox"))) { std::cout <<
 * e.path().string() << std::endl;
 *     }
 *     @endcode
 */
#ifndef LUMEX_CORE_FILESYSTEM_FS_HPP
#define LUMEX_CORE_FILESYSTEM_FS_HPP

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

#include "lumex/LumexExport.hpp"

#include <cstdint> // std::uintmax_t, std::int8_t, std::uint16_t
#include <ctime>   // std::time_t
#include <memory>  // std::shared_ptr
#include <ostream> // std::ostream
#include <string>  // std::string, std::wstring
#include <vector>  // std::vector

#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// C++11 compatible nested namespaces
namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Contains classes and utilities for cross-platform filesystem
 * operations.
 * @details This namespace provides a comprehensive set of tools for managing
 *          files and directories, designed to be a C++11 compatible
 * alternative to `std::filesystem`. It emphasizes exception-safety and clear
 *          error reporting through `filesystem_result`.
 */
namespace filesystem
{
namespace fs
{

// 116444736000000000ULL - count of intervals between 1601 and 1970
// If subtract this number from ulInt.QuadPart (which contains the number of
// 100-nanosecond intervals since 1601) we get the number of 100-nanosecond
// intervals since 1970.

// 10000000ULL - count of 100-nanosecond interval in 1 second
// Since std::time_t is measured in seconds, and ulInt.QuadPart (after
// subtraction) contains the number of 100-nanosecond intervals, dividing by
// 10000000ULL converts these 100-nanosecond intervals into seconds.
/// @brief Constant representing the number of 100-nanosecond intervals between
/// January 1, 1601 (Windows FILETIME epoch) and January 1, 1970 (Unix epoch).
LUMEX_CONSTEXPR unsigned long long
    KDEFAULT_WINDOWS_FILETIME_TO_UNIX_EPOCH_INTERVALS
    = 116444736000000000ULL;
/// @brief Constant representing the number of 100-nanosecond intervals in one
/// second.
LUMEX_CONSTEXPR unsigned long long KDEFAULT_HUNDRED_NANOSECONDS_PER_SECOND
    = 10000000ULL;

// Forward declarations
class path;
class directory_entry;
class directory_iterator;
class file_status;
class space_info;

/**
 * @brief Enumeration of common file types, compatible with `std::filesystem`.
 * @details This enum provides a portable way to categorize file system
 * entities, including regular files, directories, symbolic links, and various
 *          device or special files.
 */
enum class file_type : std::int8_t
{
  not_found = -1, ///< The file system entry does not exist.
  none = 0,       ///< No file type is specified (default constructed state).
  regular = 1,    ///< A regular file.
  directory = 2,  ///< A directory.
  symlink = 3,    ///< A symbolic link.
  block = 4,      ///< A block device file.
  character = 5,  ///< A character device file.
  fifo = 6,       ///< A FIFO (named pipe) file.
  socket = 7,     ///< A socket file.
  unknown = 8     ///< The file type is unknown or could not be determined.
};

/**
 * @brief Enumeration of file permissions, compatible with POSIX `chmod` masks.
 * @details These flags can be combined using bitwise operators to represent
 *          various access rights for the owner, group, and others. Special
 *          bits like Set-UID, Set-GID, and Sticky are also included.
 */
enum class perms : std::uint16_t
{
  none = 0,           ///< No permissions.
  owner_read = 0400,  ///< Read permission for the file owner.
  owner_write = 0200, ///< Write permission for the file owner.
  owner_exec = 0100,  ///< Execute permission for the file owner.
  owner_all
  = 0700, ///< All permissions for the file owner (read, write, execute).
  group_read = 040,   ///< Read permission for the file's group.
  group_write = 020,  ///< Write permission for the file's group.
  group_exec = 010,   ///< Execute permission for the file's group.
  group_all = 070,    ///< All permissions for the file's group.
  others_read = 04,   ///< Read permission for others.
  others_write = 02,  ///< Write permission for others.
  others_exec = 01,   ///< Execute permission for others.
  others_all = 07,    ///< All permissions for others.
  all = 0777,         ///< All standard permissions (owner, group, others).
  set_uid = 04000,    ///< Set-User-ID bit.
  set_gid = 02000,    ///< Set-Group-ID bit.
  sticky_bit = 01000, ///< Sticky bit.
  mask = 07777,       ///< Mask for all standard permission bits.
  unknown = 0xFFFF    ///< Permissions are unknown or could not be determined.
};

/**
 * @brief Bitwise OR operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand.
 * @param rhs The right-hand side `perms` operand.
 * @return A `perms` value with bits set from both operands.
 */
inline perms
operator| (perms lhs, perms rhs)
{
  return static_cast<perms> (static_cast<int> (lhs) | static_cast<int> (rhs));
}

/**
 * @brief Bitwise AND operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand.
 * @param rhs The right-hand side `perms` operand.
 * @return A `perms` value with only common bits set from both operands.
 */
inline perms
operator& (perms lhs, perms rhs)
{
  return static_cast<perms> (static_cast<int> (lhs) & static_cast<int> (rhs));
}

/**
 * @brief Bitwise XOR operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand.
 * @param rhs The right-hand side `perms` operand.
 * @return A `perms` value with bits set if they are set in one, but not both,
 * operands.
 */
inline perms
operator^ (perms lhs, perms rhs)
{
  return static_cast<perms> (static_cast<int> (lhs) ^ static_cast<int> (rhs));
}

/**
 * @brief Bitwise NOT operator for `perms` enum.
 * @param rhs The `perms` operand.
 * @return A `perms` value with all bits inverted.
 */
inline perms
operator~(perms rhs)
{
  return static_cast<perms> (~static_cast<int> (rhs));
}

/**
 * @brief Bitwise OR assignment operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand (modified in place).
 * @param rhs The right-hand side `perms` operand.
 * @return A reference to `lhs` after the operation.
 */
inline perms &
operator|= (perms &lhs, perms rhs)
{
  return lhs = lhs | rhs;
}

/**
 * @brief Bitwise AND assignment operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand (modified in place).
 * @param rhs The right-hand side `perms` operand.
 * @return A reference to `lhs` after the operation.
 */
inline perms &
operator&= (perms &lhs, perms rhs)
{
  return lhs = lhs & rhs;
}

/**
 * @brief Bitwise XOR assignment operator for `perms` enum.
 * @param lhs The left-hand side `perms` operand (modified in place).
 * @param rhs The right-hand side `perms` operand.
 * @return A reference to `lhs` after the operation.
 */
inline perms &
operator^= (perms &lhs, perms rhs)
{
  return lhs = lhs ^ rhs;
}

/**
 * @brief A generic result type for filesystem operations that encapsulates a
 * value or an error.
 * @details This class is designed to provide exception-safe error handling for
 *          filesystem operations. Instead of throwing exceptions on failure,
 *          functions return `filesystem_result<T>` which explicitly indicates
 *          success or failure and holds either the successful value or an
 * error code.
 * @tparam T The type of the value returned on success.
 */
template <typename T> class filesystem_result
{
public:
  /**
   * @brief Default constructor. Creates a successful result with a
   * default-constructed value and no error.
   */
  filesystem_result () : m_value (), m_success (true), m_error_code (0) {}

  /**
   * @brief Factory method for creating a successful `filesystem_result`.
   * @param value The value to encapsulate in the successful result.
   * @return A `filesystem_result<T>` object indicating success.
   * @note The value is moved to prevent unnecessary copies.
   */
  static filesystem_result<T>
  ok (T value)
  {
    return filesystem_result<T> (std::move (value), /*success=*/true,
                                 /*err=*/0);
  }

  /**
   * @brief Factory method for creating an unsuccessful `filesystem_result`.
   * @param error_code The system-specific error code indicating the reason for
   * failure.
   * @param default_value An optional default-constructed value to hold in case
   * of error.
   * @return A `filesystem_result<T>` object indicating failure.
   * @note The default value is moved if provided.
   */
  static filesystem_result<T>
  err (int error_code, T default_value = T{})
  {
    return filesystem_result<T> (std::move (default_value),
                                 /*success=*/false,
                                 /*err=*/error_code);
  }

  /**
   * @brief Retrieves the encapsulated value (const version).
   * @return A constant reference to the encapsulated value.
   * @warning Accessing the value of an unsuccessful result leads to undefined
   * behavior.
   */
  T const &
  value () const
  {
    return m_value;
  }
  /**
   * @brief Retrieves the encapsulated value (non-const version).
   * @return A non-constant reference to the encapsulated value.
   * @warning Accessing the value of an unsuccessful result leads to undefined
   * behavior.
   */
  T &
  value ()
  {
    return m_value;
  }
  /**
   * @brief Checks if the operation was successful.
   * @return True if the result represents success, false otherwise.
   */
  bool
  success () const
  {
    return m_success;
  }
  /**
   * @brief Retrieves the error code if the operation was unsuccessful.
   * @return The system-specific error code. Returns 0 if the operation was
   * successful.
   */
  int
  error_code () const
  {
    return m_error_code;
  }

  /**
   * @brief Implicit conversion to `bool` to easily check success status.
   * @return True if the operation was successful, false otherwise.
   * @note Enables usage like `if (result) { ... }`.
   */
  operator bool () const { return m_success; }

  /**
   * @brief Retrieves the encapsulated value if successful, or a provided
   * default value if unsuccessful.
   * @param default_value The value to return if the operation failed.
   * @return The actual value if `success()` is true, otherwise
   * `default_value`.
   */
  T
  value_or (T default_value) const
  {
    return m_success ? m_value : default_value;
  }

private:
  /**
   * @brief Private constructor used by the `ok()` and `err()` factory methods.
   * @param value The value to store.
   * @param success The success status.
   * @param error_code The error code.
   */
  filesystem_result (T value, bool success, int error_code)
      : m_value (std::move (value)), m_success (success),
        m_error_code (error_code)
  {
  }

  T m_value; ///< The encapsulated value. Valid only if `m_success` is true.
  bool m_success;   ///< Flag indicating whether the operation was successful.
  int m_error_code; ///< System-specific error code. 0 for success.
};

/**
 * @brief Specialization of `filesystem_result` for `void` return types.
 * @details This specialization is used for filesystem operations that do not
 *          return a specific value on success, but only indicate success or
 * failure.
 */
template <> class filesystem_result<void>
{
public:
  /**
   * @brief Default constructor. Creates a successful result with no error.
   */
  filesystem_result () : m_success (true), m_error_code (0) {}

  /**
   * @brief Factory method for creating a successful `filesystem_result<void>`.
   * @return A `filesystem_result<void>` object indicating success.
   */
  static filesystem_result<void>
  ok ()
  {
    return filesystem_result<void> (/*success=*/true, /*error_code=*/0);
  }

  /**
   * @brief Factory method for creating an unsuccessful
   * `filesystem_result<void>`.
   * @param error_code The system-specific error code indicating the reason for
   * failure.
   * @return A `filesystem_result<void>` object indicating failure.
   */
  static filesystem_result<void>
  err (int error_code)
  {
    return filesystem_result<void> (/*success=*/false,
                                    /*error_code=*/error_code);
  }

  /**
   * @brief Checks if the operation was successful.
   * @return True if the result represents success, false otherwise.
   */
  bool
  success () const
  {
    return m_success;
  }
  /**
   * @brief Retrieves the error code if the operation was unsuccessful.
   * @return The system-specific error code. Returns 0 if the operation was
   * successful.
   */
  int
  error_code () const
  {
    return m_error_code;
  }
  /**
   * @brief Implicit conversion to `bool` to easily check success status.
   * @return True if the operation was successful, false otherwise.
   * @note Enables usage like `if (result) { ... }`.
   */
  operator bool () const { return m_success; }

private:
  /**
   * @brief Private constructor used by the `ok()` and `err()` factory methods.
   * @param success The success status.
   * @param error_code The error code.
   */
  filesystem_result (bool success, int error_code)
      : m_success (success), m_error_code (error_code)
  {
  }

  bool m_success;   ///< Flag indicating whether the operation was successful.
  int m_error_code; ///< System-specific error code. 0 for success.
};

/**
 * @brief Cross-platform path representation and manipulation.
 * @details This class provides a robust and portable way to represent and
 * manipulate file system paths. It handles platform-specific path formats and
 *          character encodings internally while presenting a unified API
 *          inspired by C++17 `std::filesystem::path`.
 */
class LUMEX_API path
{
public:
  /// @brief Type alias for the internal string representation of the path,
  /// typically `std::string`.
  using string_type = std::string;

  /// @brief Type alias for a single character in the path string, typically
  /// `char`.
  using value_type = char;

  /// @brief The preferred directory separator character for the current
  /// operating system.
  static value_type const preferred_separator =
#if LUMEX_OS_WINDOWS
      '\\';
#else
      '/';
#endif

  /**
   * @brief Default constructor. Constructs an empty path.
   */
  path () = default;

  /**
   * @brief Constructs a path object from a `std::string`.
   * @param source The `std::string` containing the path.
   * @note The string is moved into the path object for efficiency.
   */
  path (string_type source);

  /**
   * @brief Constructs a path object from a C-style string.
   * @param source The null-terminated C-style string containing the path.
   * @note Handles `nullptr` input by initializing with an empty string.
   */
  path (char const *source)
      : m_path (source != nullptr ? source : "") {
  } // FIX(Test:
    // LumexSettingsINITest.GivenNullFilePath_WhenIsIniValid_ThenReturnsFalse):
    // Handle nullptr input

  /**
   * @brief Copy constructor. Performs a deep copy of the path string.
   * @param other The `path` object to copy from.
   */
  path (path const &other) = default;

  /**
   * @brief Copy assignment operator. Assigns the contents of another path
   * object via deep copy.
   * @param other The `path` object to assign from.
   * @return A reference to `*this` after assignment.
   */
  path &operator= (path const &other) = default;

  /**
   * @brief Default destructor. Cleans up the internal path string.
   */
  ~path () = default;

  // =================== C++11 move semantics ===================
  /**
   * @brief Move constructor. Efficiently moves the resources from another
   * `path` object.
   * @param other The `path` object to move from.
   * @note The moved-from object (`other`) is left in a valid, but unspecified,
   * state, typically set to `"."` if it was empty, to ensure validity.
   */
  path (path &&other) LUMEX_NOEXCEPT : m_path (std::move (other.m_path))
  {
    if (other.m_path.empty ())
      other.m_path = "."; // Ensure moved-from object is valid
  }
  /**
   * @brief Move assignment operator. Efficiently moves the resources from
   * another `path` object.
   * @param other The `path` object to move from.
   * @return A reference to `*this` after assignment.
   * @note The moved-from object (`other`) is left in a valid, but unspecified,
   * state, typically set to `"."` if it was empty, to ensure validity.
   */
  path &
  operator= (path &&other) LUMEX_NOEXCEPT
  {
    if (this != std::addressof (other))
      {
        m_path = std::move (other.m_path);
        if (other.m_path.empty ())
          other.m_path = "."; // Ensure moved-from object is valid
      }
    return *this;
  }

  // =================== Concatenation ===================
  /**
   * @brief Appends another `path` object as a sub-path using the path
   * concatenation operator (`/=`).
   * @param path_arg The `path_arg` object to append.
   * @return A reference to the modified `*this` object.
   */
  path &operator/= (path const &);
  /**
   * @brief Appends a `std::string` as a sub-path_arg using the path_arg
   * concatenation operator (`/=`).
   * @param path_arg The `std::string` representing the path_arg component to
   * append.
   * @return A reference to the modified `*this` object.
   */
  path &operator/= (string_type const &);
  /**
   * @brief Appends a C-style string as a sub-path_arg using the path_arg
   * concatenation operator (`/=`).
   * @param path_arg The null-terminated C-string representing the path_arg
   * component to append.
   * @return A reference to the modified `*this` object.
   */
  path &operator/= (char const *);

  /**
   * @brief Concatenates another `path_arg` object directly to the end of the
   * current path_arg string.
   * @param path_arg The `path_arg` object whose string representation will be
   * appended.
   * @return A reference to the modified `*this` object.
   */
  path &operator+= (path const &);
  /**
   * @brief Concatenates a `std::string` directly to the end of the current
   * path_arg string.
   * @param path_arg The `std::string` to append.
   * @return A reference to the modified `*this` object.
   */
  path &operator+= (string_type const &);
  /**
   * @brief Concatenates a C-style string directly to the end of the current
   * path_arg string.
   * @param path_arg The null-terminated C-string to append.
   * @return A reference to the modified `*this` object.
   */
  path &operator+= (char const *);
  /**
   * @brief Concatenates a single character directly to the end of the current
   * path_arg string.
   * @param chr The character to append.
   * @return A reference to the modified `*this` object.
   */
  path &operator+= (value_type);

  // =================== Modifiers ===================
  /**
   * @brief Clears the path_arg, making it empty.
   */
  void
  clear ()
  {
    m_path.clear ();
  }

  /**
   * @brief Converts all directory separators to the preferred separator for
   * the current OS.
   * @return A reference to the modified `*this` object.
   */
  path &make_preferred ();

  /**
   * @brief Removes the filename component from the path_arg, leaving only the
   * parent path_arg.
   * @return A reference to the modified `*this` object.
   */
  path &remove_filename ();

  /**
   * @brief Replaces the filename component of the path_arg with a new C-style
   * string filename.
   * @param filename The null-terminated C-string representing the new
   * filename.
   * @return A reference to the modified `*this` object.
   */
  path &replace_filename (char const *filename);

  /**
   * @brief Replaces the filename component of the path_arg with a new
   * `std::string` filename.
   * @param filename The `std::string` representing the new filename.
   * @return A reference to the modified `*this` object.
   */
  path &replace_filename (std::string const &filename);

  /**
   * @brief Replaces the filename component of the path with a new `path`
   * filename.
   * @param replacement The `path` object representing the new filename.
   * @return A reference to the modified `*this` object.
   */
  path &replace_filename (path const &replacement);

  /**
   * @brief Replaces the extension of the path with a new C-style string
   * extension.
   * @param ext The null-terminated C-string representing the new extension.
   * @return A reference to the modified `*this` object.
   */
  path &replace_extension (char const *ext);

  /**
   * @brief Replaces the extension of the path with a new `std::string`
   * extension.
   * @param ext The `std::string` representing the new extension.
   * @return A reference to the modified `*this` object.
   */
  path &replace_extension (std::string const &ext);
  /**
   * @brief Replaces the extension of the path with a new `path` extension.
   * @param ext The `path` object representing the new extension. Defaults to
   * an empty `path` to remove the existing extension.
   * @return A reference to the modified `*this` object.
   */
  path &replace_extension (path const &ext = path ());
  /**
   * @brief Swaps the contents of this path object with another path object.
   * @param other The other `path` object to swap with.
   * @note This operation is `noexcept` because `std::string::swap` is
   * `noexcept`.
   */
  void
  swap (path &other) LUMEX_NOEXCEPT
  {
    m_path.swap (other.m_path);
  }

  // Native format observers
  /**
   * @brief Returns the native string representation of the path.
   * @return A constant reference to the internal `std::string` representing
   * the path.
   */
  string_type const &
  native () const
  {
    return m_path;
  }
  /**
   * @brief Returns a pointer to the null-terminated C-style string
   * representation of the path.
   * @return A `const char*` pointer to the internal path string.
   */
  value_type const *
  c_str () const
  {
    return m_path.c_str ();
  }
  /**
   * @brief Implicit conversion to `std::string`.
   * @return A copy of the internal path string.
   */
  operator string_type () const { return m_path; }

  // Generic format observers
  /**
   * @brief Returns a copy of the path as a `std::string`.
   * @return A `std::string` representation of the path.
   */
  string_type
  string () const
  {
    return m_path;
  }
  /**
   * @brief Converts and returns the path as a `std::wstring`.
   * @return A `std::wstring` representation of the path.
   */
  std::wstring wstring () const;
  /**
   * @brief Returns a copy of the path as a UTF-8 encoded `std::string`.
   * @return A `std::string` (UTF-8) representation of the path.
   */
  string_type
  u8string () const
  {
    return string ();
  }

  // Decomposition
  /**
   * @brief Extracts the root name component from the path.
   * @return A `path` object representing the root name (e.g., drive letter on
   * Windows, empty on POSIX).
   */
  path root_name () const;
  /**
   * @brief Extracts the root directory component from the path.
   * @return A `path` object representing the root directory (e.g., "/" on
   * POSIX, "\" on Windows).
   */
  path root_directory () const;
  /**
   * @brief Extracts the root path component from the path (root name + root
   * directory).
   * @return A `path` object representing the root path.
   */
  path root_path () const;
  /**
   * @brief Extracts the relative path component from the path (after the root
   * path).
   * @return A `path` object representing the relative path.
   */
  path relative_path () const;
  /**
   * @brief Extracts the parent path component from the path.
   * @return A `path` object representing the parent directory.
   */
  path parent_path () const;
  /**
   * @brief Extracts the filename component from the path.
   * @return A `path` object representing the filename (including extension).
   */
  path filename () const;
  /**
   * @brief Extracts the stem component from the path (filename without
   * extension).
   * @return A `path` object representing the filename stem.
   */
  path stem () const;
  /**
   * @brief Extracts the extension component from the path.
   * @return A `path` object representing the extension (including the leading
   * dot).
   */
  path extension () const;

  // Queries
  /**
   * @brief Checks if the path is empty.
   * @return True if the path string is empty, false otherwise.
   */
  bool
  empty () const
  {
    return m_path.empty ();
  }
  /**
   * @brief Checks if the path has a root name component.
   * @return True if the path has a root name, false otherwise.
   */
  bool has_root_name () const;
  /**
   * @brief Checks if the path has a root directory component.
   * @return True if the path has a root directory, false otherwise.
   */
  bool has_root_directory () const;
  /**
   * @brief Checks if the path has a root path component.
   * @return True if the path has a root path, false otherwise.
   */
  bool has_root_path () const;
  /**
   * @brief Checks if the path has a relative path component.
   * @return True if the path has a relative path, false otherwise.
   */
  bool has_relative_path () const;
  /**
   * @brief Checks if the path has a parent path component.
   * @return True if the path has a parent path, false otherwise.
   */
  bool has_parent_path () const;
  /**
   * @brief Checks if the path has a filename component.
   * @return True if the path has a filename, false otherwise.
   */
  bool has_filename () const;
  /**
   * @brief Checks if the path has a stem component (filename without
   * extension).
   * @return True if the path has a stem, false otherwise.
   */
  bool has_stem () const;
  /**
   * @brief Checks if the path has an extension component.
   * @return True if the path has an extension, false otherwise.
   */
  bool has_extension () const;
  /**
   * @brief Checks if the path is an absolute path.
   * @return True if the path is absolute, false otherwise.
   */
  bool is_absolute () const;
  /**
   * @brief Checks if the path is a relative path.
   * @return True if the path is relative, false otherwise.
   */
  bool
  is_relative () const
  {
    return !is_absolute ();
  }

  // Iterators (not implemented in this header, but part of the conceptual
  // interface)
  class iterator; ///< Forward declaration for path iterator.
  iterator
  begin () const; ///< Returns an iterator to the first component of the path.
  iterator end () const; ///< Returns an iterator to the past-the-end component
                         ///< of the path.

  // Non-member operators (declared as friends for direct access to m_path)
  friend bool operator== (path const &lhs,
                          path const &rhs); ///< Equality comparison.
  friend bool operator!= (path const &lhs,
                          path const &rhs); ///< Inequality comparison.
  friend bool
  operator< (path const &lhs,
             path const &rhs); ///< Less-than comparison (lexicographical).
  friend bool operator<= (path const &lhs,
                          path const &rhs); ///< Less-than-or-equal comparison.
  friend bool operator> (path const &lhs,
                         path const &rhs); ///< Greater-than comparison.
  friend bool
  operator>= (path const &lhs,
              path const &rhs); ///< Greater-than-or-equal comparison.
  friend path operator/ (path const &lhs,
                         path const &rhs); ///< path concatenation.

private:
#ifdef _WIN32
#pragma warning(push)
#pragma warning(                                                              \
    disable : 4251) // Suppress C4251 for STL members in DLL interface
#endif
  std::string m_path; ///< The internal string storing the path.
#ifdef _WIN32
#pragma warning(pop)
#endif

  // Helper methods
  void append_separator_if_needed (); ///< Appends a separator if necessary.
  static bool is_separator (
      value_type); ///< Checks if a character is a path separator.
  std::size_t
  find_filename_pos () const; ///< Finds the starting position of the filename.
  std::size_t find_extension_pos ()
      const; ///< Finds the starting position of the extension.
};

/**
 * @brief File status information.
 * @details This class encapsulates information about a file system entry,
 *          including its type (e.g., regular file, directory) and permissions.
 */
class LUMEX_API file_status
{
public:
  /**
   * @brief Default constructor. Initializes with `file_type::none` and
   * `perms::unknown`.
   */
  file_status () : m_type (file_type::none), m_perms (perms::unknown) {}
  /**
   * @brief Constructs a `file_status` object with a specified file type and
   * optional permissions.
   * @param type The `file_type` of the entry.
   * @param perms The `perms` of the entry. Defaults to `perms::unknown`.
   */
  explicit file_status (file_type type, perms p = perms::unknown)
      : m_type (type), m_perms (p)
  {
  }

  /**
   * @brief Retrieves the file type.
   * @return The `file_type` of the entry.
   */
  file_type
  type () const
  {
    return m_type;
  }
  /**
   * @brief Retrieves the file permissions.
   * @return The `perms` of the entry.
   */
  perms
  permissions () const
  {
    return m_perms;
  }

  /**
   * @brief Sets the file type.
   * @param type The new `file_type`.
   */
  void
  type (file_type type)
  {
    m_type = type;
  }
  /**
   * @brief Sets the file permissions.
   * @param p The new `perms`.
   */
  void
  permissions (perms p)
  {
    m_perms = p;
  }

private:
  file_type m_type; ///< The type of the file system entry.
  perms m_perms;    ///< The permissions of the file system entry.
};

/**
 * @brief Space information for a filesystem.
 * @details This class provides details about the disk space, including
 *          total capacity, free space, and available space.
 */
class LUMEX_API space_info
{
public:
  std::uintmax_t capacity{}; ///< Total capacity of the filesystem in bytes.
  std::uintmax_t free{};     ///< Total free space on the filesystem in bytes.
  std::uintmax_t
      available{}; ///< Available free space for unprivileged users in bytes.
};

/**
 * @brief Directory entry representation.
 * @details This class represents a single entry (file or subdirectory) found
 *          within a directory. It stores the path to the entry and provides
 *          methods to query its status (type, size, permissions).
 */
class LUMEX_API directory_entry
{
public:
  /**
   * @brief Default constructor. Creates an empty directory entry.
   */
  directory_entry () = default;

  /**
   * @brief Constructs a `directory_entry` from a `path`.
   * @param path_arg The path object representing the directory entry.
   */
  explicit directory_entry (fs::path const &);

  /**
   * @brief Retrieves the path of this directory entry.
   * @return A constant reference to the `path` object.
   */
  fs::path const &
  path () const
  {
    return m_path;
  }

  /**
   * @brief Checks if the file system entry exists.
   * @return True if the entry exists, false otherwise.
   */
  bool exists () const;

  /**
   * @brief Checks if the entry represents a regular file.
   * @return True if it's a regular file, false otherwise.
   */
  bool is_regular_file () const;

  /**
   * @brief Checks if the entry represents a directory.
   * @return True if it's a directory, false otherwise.
   */
  bool is_directory () const;

  /**
   * @brief Checks if the entry represents a symbolic link.
   * @return True if it's a symbolic link, false otherwise.
   */
  bool is_symlink () const;

  /**
   * @brief Checks if the entry represents a block device file.
   * @return True if it's a block device, false otherwise.
   */
  bool is_block_file () const;

  /**
   * @brief Checks if the entry represents a character device file.
   * @return True if it's a character device, false otherwise.
   */
  bool is_character_file () const;

  /**
   * @brief Checks if the entry represents a FIFO (named pipe) file.
   * @return True if it's a FIFO, false otherwise.
   */
  bool is_fifo () const;

  /**
   * @brief Checks if the entry represents a socket file.
   * @return True if it's a socket, false otherwise.
   */
  bool is_socket () const;

  /**
   * @brief Checks if the entry represents a file type other than regular file
   * or directory.
   * @return True if it's an "other" type, false otherwise.
   */
  bool is_other () const;

  /**
   * @brief Retrieves the size of the file represented by this entry.
   * @return The size of the file in bytes. Returns 0 if the entry is not a
   * regular file or an error occurs.
   */
  std::uintmax_t file_size () const;

  /**
   * @brief Retrieves the status of the file system entry, following symbolic
   * links.
   * @return A `file_status` object containing the file type and permissions.
   */
  file_status status () const;

  /**
   * @brief Retrieves the status of the file system entry, without following
   * symbolic links.
   * @return A `file_status` object containing the file type and permissions of
   * the link itself.
   */
  file_status symlink_status () const;

  /**
   * @brief Equality comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the paths are equal, false otherwise.
   */
  bool operator== (directory_entry const &rhs) const;

  /**
   * @brief Inequality comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the paths are not equal, false otherwise.
   */
  bool operator!= (directory_entry const &rhs) const;

  /**
   * @brief Less-than comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the left-hand side path is lexicographically less than the
   * right-hand side path, false otherwise.
   */
  bool operator< (directory_entry const &rhs) const;

  /**
   * @brief Less-than-or-equal-to comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the left-hand side path is lexicographically less than or
   * equal to the right-hand side path, false otherwise.
   */
  bool operator<= (directory_entry const &rhs) const;

  /**
   * @brief Greater-than comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the left-hand side path is lexicographically greater than
   * the right-hand side path, false otherwise.
   */
  bool operator> (directory_entry const &rhs) const;

  /**
   * @brief Greater-than-or-equal-to comparison operator for `directory_entry`.
   * @param rhs The right-hand side `directory_entry` object.
   * @return True if the left-hand side path is lexicographically greater than
   * or equal to the right-hand side path, false otherwise.
   */
  bool operator>= (directory_entry const &rhs) const;

private:
  // Qualified: member function path() shadows the namespace-scope class name.
  fs::path m_path;              ///< The path of this directory entry.
  mutable file_status m_status; ///< Cached file status for this entry. Marked
                                ///< `mutable` for lazy evaluation.
  mutable bool
      m_status_known{}; ///< Flag indicating if `m_status` has been populated.

  void
  refresh_status () const; ///< Refreshes the cached file status information.
};

/**
 * @brief Directory iterator for traversing directory contents.
 * @details This class provides an input iterator interface for iterating
 *          through the entries (files and subdirectories) within a given
 * directory. It hides the platform-specific directory enumeration mechanisms.
 */
class LUMEX_API directory_iterator
{
public:
  // Iterator traits
  using value_type = directory_entry; ///< The type of elements returned by
                                      ///< dereferencing the iterator.
  using difference_type
      = std::ptrdiff_t; ///< The type used for differences between iterators.
  using pointer = directory_entry const *;   ///< Pointer to the value type.
  using reference = directory_entry const &; ///< Reference to the value type.
  using iterator_category
      = std::input_iterator_tag; ///< The category of this iterator (input
                                 ///< iterator).

  // =================== Constructors ===================
  /**
   * @brief Default constructor. Creates an end iterator.
   */
  directory_iterator () = default;
  /**
   * @brief Constructs a `directory_iterator` for the specified path.
   * @param path_arg The `path_arg` object representing the directory to
   * iterate.
   * @note Initializes the iterator to point to the first entry. If the
   * directory cannot be opened, it becomes an end iterator.
   */
  explicit directory_iterator (path const &);
  /**
   * @brief Copy constructor. Copies the underlying shared state.
   * @param other The `directory_iterator` object to copy from.
   */
  directory_iterator (directory_iterator const &);
  /**
   * @brief Move constructor. Moves the underlying shared state.
   * @param other The `directory_iterator` object to move from.
   * @note The moved-from object is left in a valid, but unspecified, state.
   */
  directory_iterator (directory_iterator &&) LUMEX_NOEXCEPT;
  /**
   * @brief Default destructor. Closes the directory handle/stream if open.
   */
  ~directory_iterator () = default;

  /**
   * @brief Copy assignment operator. Copies the underlying shared state.
   * @param other The `directory_iterator` object to copy from.
   * @return A reference to `*this` after assignment.
   */
  directory_iterator &operator= (directory_iterator const &);
  /**
   * @brief Move assignment operator. Moves the underlying shared state.
   * @param other The `directory_iterator` object to move from.
   * @return A reference to `*this` after assignment.
   * @note The moved-from object is left in a valid, but unspecified, state.
   */
  directory_iterator &operator= (directory_iterator &&) LUMEX_NOEXCEPT;

  // =================== Iterator operations ===================
  /**
   * @brief Dereferences the iterator to access the current `directory_entry`.
   * @return A constant reference to the `directory_entry` pointed to by the
   * iterator.
   */
  reference operator* () const;
  /**
   * @brief Dereferences the iterator to access members of the current
   * `directory_entry`.
   * @return A constant pointer to the `directory_entry` pointed to by the
   * iterator.
   */
  pointer operator->() const;
  /**
   * @brief Pre-increments the iterator to move to the next entry in the
   * directory.
   * @return A reference to the incremented `*this` object.
   */
  directory_iterator &operator++ ();
  /**
   * @brief Post-increments the iterator to move to the next entry in the
   * directory.
   * @return A copy of the iterator before it was incremented.
   */
  directory_iterator operator++ (int);

  /**
   * @brief Compares two `directory_iterator` objects for equality.
   * @param rhs The right-hand side `directory_iterator` object.
   * @return True if the iterators are equal (point to the same state), false
   * otherwise.
   */
  bool operator== (directory_iterator const &) const;
  /**
   * @brief Compares two `directory_iterator` objects for inequality.
   * @param rhs The right-hand side `directory_iterator` object.
   * @return True if the iterators are not equal, false otherwise.
   */
  bool operator!= (directory_iterator const &) const;

private:
  class Impl; ///< Forward declaration for the private implementation details.
              /*
                Warning C4251
                'lumex::core::filesystem::fs::lumex_filesystem::directory_iterator::m_impl':
                'std::shared_ptr<lumex::core::filesystem::fs::lumex_filesystem::directory_iterator::Impl>'
                needs to have dll-interface to be used by clients of
                'lumex::core::filesystem::fs::lumex_filesystem::directory_iterator' appears
                on MSVC,             because directory_iterator is exported from DLL
                (LUMEX_API),             and             its private member m_impl is an
                instance of             std::shared_ptr,             which itself             does not
                have an explicit DLL             interface.
            
                Although std::shared_ptr is a standard library and is usually safely
                used across DLL boundaries (assuming the compiler and settings are
                the same), MSVC issues this warning due to potential ODR (One
                Definition Rule) issues if the std::shared_ptr implementation differs
                between DLL and client code.
              */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::shared_ptr<Impl>
      m_impl; ///< Pointer to the private implementation details (PIMPL idiom).
#ifdef _WIN32
#pragma warning(pop)
#endif
};

/**
 * @brief Main filesystem operations class.
 * @details This class provides a collection of static methods for performing
 *          common file system operations in a cross-platform and
 * exception-safe manner. All methods return `filesystem_result` objects to
 * indicate success or failure.
 */
class LUMEX_API lumex_filesystem
{
public:
  // =================== File type tests ===================
  /**
   * @brief Checks if a file or directory exists at the specified path.
   * @param path_arg The `path_arg` object representing the file or directory.
   * @return True if the file or directory exists, false otherwise.
   */
  static bool exists (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a regular file.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg exists and refers to a regular file, false
   * otherwise.
   */
  static bool is_regular_file (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a directory.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg exists and refers to a directory, false
   * otherwise.
   */
  static bool is_directory (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a symbolic link.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg refers to a symbolic link, false otherwise.
   */
  static bool is_symlink (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a block device file.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg refers to a block device file, false
   * otherwise.
   */
  static bool is_block_file (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a character device file.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg refers to a character device file, false
   * otherwise.
   */
  static bool is_character_file (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a FIFO (named pipe)
   * file.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg refers to a FIFO file, false otherwise.
   */
  static bool is_fifo (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a socket file.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg refers to a socket file, false otherwise.
   */
  static bool is_socket (path const &);
  /**
   * @brief Checks if the specified path_arg refers to a file type other than
   * regular file or directory.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return True if the path_arg exists and is neither a regular file nor a
   * directory, false otherwise.
   */
  static bool is_other (path const &);
  /**
   * @brief Checks if a file is empty or a directory is empty.
   * @param path_arg The `path_arg` object representing the file or directory.
   * @return True if the path_arg exists and refers to an empty file (size 0)
   * or an empty directory, false otherwise.
   */
  static bool is_empty (path const &);

  // =================== File operations ===================
  /**
   * @brief Recursively copies a file or a directory (and its contents) from
   * one location to another.
   * @param from The `path_arg` object representing the source file or
   * directory.
   * @param to_ The `path_arg` object representing the destination path_arg.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> copy (path const &, path const &);

  /**
   * @brief Copies a single file from one location to another.
   * @param from The `path_arg` object representing the source file.
   * @param to_ The `path_arg` object representing the destination file.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> copy_file (path const &, path const &);

  /**
   * @brief Creates a new symbolic link `to_` that points to `from`.
   * @param from The `path_arg` object representing the target of the symbolic
   * link.
   * @param to_ The `path_arg` object representing the new symbolic link to
   * create.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> copy_symlink (path const &, path const &);

  /**
   * @brief Creates a new directory at the specified path.
   * @param path_arg The `path_arg` object representing the directory to
   * create.
   * @return A `filesystem_result<bool>`: `ok(true)` if created, `ok(false)` if
   * already exists, `err` on failure.
   */
  static filesystem_result<bool> create_directory (path const &);

  /**
   * @brief Creates all directories in the specified path_arg, including any
   * missing parent directories.
   * @param path_arg The `path_arg` object representing the directory path_arg
   * to create.
   * @return A `filesystem_result<bool>`: `ok(true)` if created, `ok(false)` if
   * all existed, `err` on failure.
   */
  static filesystem_result<bool> create_directories (path const &);

  /**
   * @brief Creates a new symbolic link `link` that points to `target`.
   * @param target The `path_arg` object representing the existing target file
   * or directory.
   * @param link The `path_arg` object representing the new symbolic link to
   * create.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> create_symlink (path const &, path const &);

  /**
   * @brief Creates a new symbolic link `link` that points to a directory
   * `target`.
   * @param target The `path_arg` object representing the existing target
   * directory.
   * @param link The `path_arg` object representing the new symbolic link to
   * create.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> create_directory_symlink (path const &,
                                                           path const &);

  /**
   * @brief Retrieves the current working directory.
   * @return A `filesystem_result<path>`: `ok(current_path)` on success, `err`
   * on failure.
   */
  static filesystem_result<path> current_path ();

  /**
   * @brief Sets the current working directory to the specified path_arg.
   * @param path_arg The `path_arg` object representing the new current
   * directory.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> current_path (path const &);

  /**
   * @brief Checks if two paths refer to the same file system entity.
   * @param lhs The first `path_arg` object.
   * @param rhs The second `path_arg` object.
   * @return True if the file types and sizes are equal, false otherwise.
   */
  static bool equivalent (path const &, path const &);

  /**
   * @brief Retrieves the size of a regular file in bytes.
   * @param path_arg The `path_arg` object representing the file.
   * @return A `filesystem_result<std::uintmax_t>`: `ok(size)` on success,
   * `err` on failure.
   */
  static filesystem_result<std::uintmax_t> file_size (path const &);

  /**
   * @brief Retrieves the last write time of a file or directory.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @return A `filesystem_result<std::time_t>`: `ok(time)` on success, `err`
   * on failure.
   */
  static filesystem_result<std::time_t> last_write_time (path const &);

  /**
   * @brief Sets the last write time of a file or directory.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @param new_time The new `std::time_t` value to set as the last write time.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> last_write_time (path const &, std::time_t);

  /**
   * @brief Sets the permissions of a file or directory.
   * @param path_arg The `path_arg` object representing the file system entry.
   * @param prms The `perms` flags representing the new permissions to apply.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */

  static filesystem_result<void> permissions (path const &, perms);
  /**
   * @brief Reads the target of a symbolic link.
   * @param path_arg The `path_arg` object representing the symbolic link.
   * @return A `filesystem_result<path>`: `ok(target_path)` on success, `err`
   * on failure.
   */

  static filesystem_result<path> read_symlink (path const &);

  /**
   * @brief Removes a file or an empty directory at the specified path_arg.
   * @param path_arg The `path_arg` object representing the file or directory
   * to remove.
   * @return A `filesystem_result<bool>`: `ok(true)` if removed, `ok(false)` if
   * not found, `err` on failure.
   */
  static filesystem_result<bool> remove (path const &);

  /**
   * @brief Recursively removes a file or a directory and all its contents.
   * @param path_arg The `path_arg` object representing the file or directory
   * to remove.
   * @return A `filesystem_result<std::uintmax_t>`: `ok(count)` of removed
   * items, `err` on failure.
   */
  static filesystem_result<std::uintmax_t> remove_all (path const &);

  /**
   * @brief Renames or moves a file or directory.
   * @param from The `path_arg` object representing the source file or
   * directory.
   * @param to_ The `path_arg` object representing the new name/location.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> rename (path const &, path const &);

  /**
   * @brief Changes the size of a regular file.
   * @param path_arg The `path_arg` object representing the file to resize.
   * @param new_size The desired new size of the file in bytes.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> resize_file (path const &, std::uintmax_t);

  /**
   * @brief Moves a file from one location to another.
   * @param from The `path_arg` object representing the source file.
   * @param to_path The `path_arg` object representing the destination path_arg
   * for the file.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> move_file (path const &from,
                                            path const &to_path);

  /**
   * @brief Moves a directory and its contents from one location to another.
   * @param from The `path_arg` object representing the source directory.
   * @param to_path The `path_arg` object representing the destination path_arg
   * for the directory.
   * @return A `filesystem_result<void>`: `ok()` on success, `err(error_code)`
   * on failure.
   */
  static filesystem_result<void> move_directory (path const &from,
                                                 path const &to_path);

  /**
   * @brief Retrieves information about the free and total space on the
   * filesystem.
   * @param path_arg The `path_arg` object representing a file or directory
   * within the target filesystem.
   * @return A `filesystem_result<space_info>`: `ok(space_info)` on success,
   * `err` on failure.
   */
  static filesystem_result<space_info> space (path const &);

  /**
   * @brief Returns the status of the file or directory at the specified
   * path_arg, following symbolic links.
   * @param path_arg The `path_arg` object of the file or directory.
   * @return A `filesystem_result<file_status>` containing the file status.
   */
  static filesystem_result<file_status> status (path const &);

  /**
   * @brief Returns the status of the file or directory at the specified
   * path_arg, without following symbolic links.
   * @param path_arg The `path_arg` object of the file or directory.
   * @return A `filesystem_result<file_status>` containing the file status.
   */
  static filesystem_result<file_status> symlink_status (path const &);

  /**
   * @brief Retrieves the path_arg to the system's temporary directory.
   * @return A `filesystem_result<path>`: `ok(temp_path)` on success, `err` on
   * failure.
   */
  static filesystem_result<path> temp_directory_path ();

  /**
   * @brief Converts a `std::string` (UTF-8 encoded) to a `std::wstring`.
   * @param str The `std::string` to convert.
   * @return A `std::wstring` representation.
   */
  static std::wstring to_wide_string (std::string const &);

  /**
   * @brief Converts a `std::wstring` to a `std::string` (UTF-8 encoded).
   * @param wstr The `std::wstring` to convert.
   * @return A `std::string` representation.
   */
  static std::string from_wide_string (std::wstring const &);

  // =================== path operations ===================
  /**
   * @brief Converts a path_arg to its absolute form.
   * @param path_arg The `path_arg` object to convert.
   * @return An absolute `path_arg` object.
   */
  static path absolute (path const &);

  /**
   * @brief Returns the canonical (absolute, resolved) form of a path_arg.
   * @param path_arg The `path_arg` object to canonicalize.
   * @return A canonical `path_arg` object.
   */
  static path canonical (path const &);

  /**
   * @brief Returns the weakly canonical form of a path_arg.
   * @param path_arg The `path_arg` object to process.
   * @return A weakly canonical `path_arg` object.
   */
  static path weakly_canonical (path const &);

  /**
   * @brief Constructs a relative path_arg from `path_arg` to `base`.
   * @param path_arg The `path_arg` object to make relative.
   * @param base The `path_arg` object to make `path_arg` relative to.
   * @return A `path_arg` object representing the relative path_arg.
   */
  static path relative (path const &, path const &);

  /**
   * @brief Constructs a proximate path_arg from `path_arg` to `base`.
   * @param path_arg The `path_arg` object to make proximate.
   * @param base The `path_arg` object to make `path_arg` proximate to.
   * @return A `path_arg` object representing the proximate path_arg.
   */
  static path proximate (path const &, path const &);

  // =================== Convenience directory iteration ===================
  /**
   * @brief Retrieves a vector of all `directory_entry` objects within the
   * specified directory.
   * @param path_arg The `path_arg` object representing the directory.
   * @return A `std::vector` of `directory_entry` objects.
   */
  static std::vector<directory_entry> directory_contents (path const &);

  /**
   * @brief Retrieves a vector of all `path_arg` objects within the specified
   * directory.
   * @param path_arg The `path_arg` object representing the directory.
   * @return A `filesystem_result<std::vector<path>>` containing paths of all
   * entries, or an error.
   */
  static filesystem_result<std::vector<path>> directory_paths (path const &);

  // =================== Custom own methods (not from standard)
  // ===================
  /**
   * @brief Retrieve the absolute filesystem path_arg of the running
   * executable.
   *
   * On Windows, uses GetModuleFileNameW;
   * on Unix‑like systems, reads `/proc/self/exe`.
   *
   * @return lumex_filesystem::path Full path_arg to the current executable.
   */
  static path get_exe_path ();

  /**
   * @brief Acquire an exclusive lock on a directory to prevent its deletion.
   *
   * On Windows, opens the directory handle without FILE_SHARE_DELETE;
   * on POSIX, creates a hidden ".lock" file inside the directory and
   * applies a write lock via `fcntl`.
   *
   * @param path_arg filesystem path_arg to the directory to lock.
   */
  static void lock_directory (path const &path_arg);

  /**
   * @brief Check if a file is readable by the current process.
   * @param path_arg The filesystem path_arg to check.
   * @return `true` if the file exists, has read permissions, and can be opened
   * for reading; `false` otherwise.
   * @note Logs warnings to `std::cerr` for failures (non-existent file,
   * missing permissions, or open errors).
   */
  static bool is_readable (path const &path_arg);

  /**
   * @brief Overload of `is_readable` that accepts a C-style string path_arg.
   * @param path_str The null-terminated C-string filesystem path_arg to check.
   * @return `true` if the file is readable, `false` otherwise.
   */
  static bool
  is_readable (char const *path_str)
  {
    return is_readable (path (path_str));
  }

  /**
   * @brief Overload of `is_readable` that accepts a `std::string` path_arg.
   * @param path_str The `std::string` filesystem path_arg to check.
   * @return `true` if the file is readable, `false` otherwise.
   */
  static bool
  is_readable (std::string const &path_str)
  {
    return is_readable (path (path_str));
  }

  /**
   * @brief Check if a file is writable by the current process.
   * @param path_arg The filesystem path_arg to check.
   * @return `true` if the file exists, has write permissions, and can be
   * opened for writing; `false` otherwise.
   * @note Logs warnings to `std::cerr` for failures (non-existent file,
   * missing permissions, or open errors).
   */
  static bool is_writable (path const &path_arg);

  /**
   * @brief Overload of `is_writable` that accepts a C-style string path_arg.
   * @param path_str The null-terminated C-string filesystem path_arg to check.
   * @return `true` if the file is writable, `false` otherwise.
   */
  static bool
  is_writable (char const *path_str)
  {
    return is_writable (path (path_str));
  }

  /**
   * @brief Overload of `is_writable` that accepts a `std::string` path_arg.
   * @param path_str The `std::string` filesystem path_arg to check.
   * @return `true` if the file is writable, `false` otherwise.
   */
  static bool
  is_writable (std::string const &path_str)
  {
    return is_writable (path (path_str));
  }

  /**
   * @brief Check if a file is both readable and writable by the current
   * process.
   * @param path_arg The filesystem path_arg to check.
   * @return `true` if `isReadable(path_arg) && isWritable(path_arg)`; `false`
   * otherwise.
   */
  static bool is_accessible (path const &path_arg);

  /**
   * @brief Overload of `is_accessible` that accepts a C-style string path_arg.
   * @param path_str The null-terminated C-string filesystem path_arg to check.
   * @return `true` if the file is accessible, `false` otherwise.
   */
  static bool
  is_accessible (char const *path_str)
  {
    return is_accessible (path (path_str));
  }

  /**
   * @brief Overload of `is_accessible` that accepts a `std::string` path_arg.
   * @param path_str The `std::string` filesystem path_arg to check.
   * @return `true` if the file is accessible, `false` otherwise.
   */
  static bool
  is_accessible (std::string const &path_str)
  {
    return is_accessible (path (path_str));
  }

private:
  // =================== OS-specific implementations ===================
#if LUMEX_OS_WINDOWS
  static filesystem_result<file_status>
  get_file_status_windows (path const &,
                           bool); ///< Internal helper for Windows file status.
#else
  static filesystem_result<file_status>
  get_file_status_posix (path const &,
                         bool); ///< Internal helper for POSIX file status.
#endif
};

// =================== path_arg non-member operators implementation
// ===================
/**
 * @brief Equality comparison operator for `path_arg`.
 * @param lhs The left-hand side `path_arg` object.
 * @param rhs The right-hand side `path_arg` object.
 * @return True if the paths are equal, false otherwise.
 */
inline bool
operator== (path const &lhs, path const &rhs)
{
  return lhs.m_path == rhs.m_path;
}

/**
 * @brief Inequality comparison operator for `path`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return True if the paths are not equal, false otherwise.
 */
inline bool
operator!= (path const &lhs, path const &rhs)
{
  return !(lhs == rhs);
}

/**
 * @brief Less-than comparison operator for `path`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return True if the left-hand side path is lexicographically less than the
 * right-hand side path, false otherwise.
 */
inline bool
operator< (path const &lhs, path const &rhs)
{
  return lhs.m_path < rhs.m_path;
}

/**
 * @brief Less-than-or-equal comparison operator for `path`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return True if the left-hand side path is lexicographically less than or
 * equal to the right-hand side path, false otherwise.
 */
inline bool
operator<= (path const &lhs, path const &rhs)
{
  return !(rhs < lhs);
}

/**
 * @brief Greater-than comparison operator for `path`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return True if the left-hand side path is lexicographically greater than
 * the right-hand side path, false otherwise.
 */
inline bool
operator> (path const &lhs, path const &rhs)
{
  return rhs < lhs;
}

/**
 * @brief Greater-than-or-equal comparison operator for `path`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return True if the left-hand side path is lexicographically greater than or
 * equal to the right-hand side path, false otherwise.
 */
inline bool
operator>= (path const &lhs, path const &rhs)
{
  return !(lhs < rhs);
}

/**
 * @brief path concatenation operator.
 * @param lhs The left-hand side `path` object.
 * @param rhs The right-hand side `path` object.
 * @return A new `path` object representing the concatenation of `lhs` and
 * `rhs`.
 */
inline path
operator/ (path const &lhs, path const &rhs)
{
  path result = lhs;
  result /= rhs;
  return result;
}

/**
 * @brief path concatenation operator with a `std::string`.
 * @param lhs The left-hand side `path` object.
 * @param rhs The `std::string` to append.
 * @return A new `path` object representing the concatenation.
 */
inline path
operator/ (path const &lhs, std::string const &rhs)
{
  path result = lhs;
  result /= rhs;
  return result;
}

/**
 * @brief path concatenation operator with a C-style string.
 * @param lhs The left-hand side `path` object.
 * @param rhs The null-terminated C-string to append.
 * @return A new `path` object representing the concatenation.
 */
inline path
operator/ (path const &lhs, char const *rhs)
{
  path result = lhs;
  result /= rhs;
  return result;
}

/**
 * @brief path concatenation operator with `std::string` as left operand.
 * @param lhs The `std::string` left-hand side.
 * @param rhs The right-hand side `path` object.
 * @return A new `path` object representing the concatenation.
 */
inline path
operator/ (std::string const &lhs, path const &rhs)
{
  return path (lhs) / rhs;
}

/**
 * @brief path concatenation operator with C-style string as left operand.
 * @param lhs The null-terminated C-string left-hand side.
 * @param rhs The right-hand side `path` object.
 * @return A new `path` object representing the concatenation.
 */
inline path
operator/ (char const *lhs, path const &rhs)
{
  return path (lhs) / rhs;
}

/**
 * @brief Checks if a file exists at the specified path
 * @param path path_arg to the file
 * @return True if the file exists, false otherwise
 */
LUMEX_PUBLIC_API
bool isFileExists (std::string const &path_arg);

/**
 * @brief Checks if a file or directory name is valid
 * @param name Name to check
 * @throws std::invalid_argument if the name contains invalid characters or is
 * reserved
 */
LUMEX_PUBLIC_API void checkName (std::string const &name);

/**
 * @brief Filters name, replacing invalid characters with '_' (noexcept
 * wrapper)
 * @param name Original name
 * @param defaultValue Returned if name is empty or completely invalid
 * @return Sanitized name or defaultValue
 */
LUMEX_PUBLIC_API std::string sanitizeName (
    std::string const &name, // NOLINT(bugprone-easily-swappable-parameters)
    std::string const &defaultValue = "unnamed") LUMEX_NOEXCEPT;
} // namespace fs
} // namespace filesystem
} // namespace core
} // namespace lumex

// Convenience type aliases
namespace lumex
{
/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::path`.
 * @details This allows `path` to be used without full namespace qualification.
 */
using path = core::filesystem::fs::path;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::directory_entry`.
 * @details This allows `directory_entry` to be used without full namespace
 * qualification.
 */
using directory_entry = core::filesystem::fs::directory_entry;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::directory_iterator`.
 * @details This allows `directory_iterator` to be used without full namespace
 * qualification.
 */
using directory_iterator = core::filesystem::fs::directory_iterator;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::file_type`.
 * @details This allows `file_type` to be used without full namespace
 * qualification.
 */
using file_type = core::filesystem::fs::file_type;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::file_status`.
 * @details This allows `file_status` to be used without full namespace
 * qualification.
 */
using file_status = core::filesystem::fs::file_status;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::perms`.
 * @details This allows `perms` to be used without full namespace
 * qualification.
 */
using perms = core::filesystem::fs::perms;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::space_info`.
 * @details This allows `space_info` to be used without full namespace
 * qualification.
 */
using space_info = core::filesystem::fs::space_info;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::lumex_filesystem`.
 * @details This allows `filesystem` to be used without full namespace
 * qualification.
 */
using filesystem = core::filesystem::fs::lumex_filesystem;

/**
 * @brief Global alias for
 * `lumex::core::filesystem::fs::lumex_filesystem::filesystem_result`.
 * @details This allows `filesystem_result` to be used without full namespace
 * qualification.
 * @tparam T The type of the value held by the result.
 */
template <typename T>
using filesystem_result = core::filesystem::fs::filesystem_result<T>;
} // namespace lumex

/**
 * @brief Stream insertion operator for `lumex::path`.
 * @details Allows printing `path` objects directly to `std::ostream` (e.g.,
 * `std::cout`).
 * @param out The output stream.
 * @param path_arg The `lumex::path` object to print.
 * @return A reference to the output stream.
 */
inline std::ostream &
operator<< (std::ostream &out, lumex::path const &path_arg)
{
  out << path_arg.string ();
  return out;
}

/**
 * @brief Stream insertion operator for `lumex::directory_entry`.
 * @details Allows printing `directory_entry` objects directly to
 * `std::ostream` (e.g., `std::cout`). It prints the path_arg string of the
 * entry.
 * @param out The output stream.
 * @param entry The `lumex::directory_entry` object to print.
 * @return A reference to the output stream.
 */
inline std::ostream &
operator<< (std::ostream &out, lumex::directory_entry const &entry)
{
  out << entry.path ().string ();
  return out;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FILESYSTEM_FS_HPP

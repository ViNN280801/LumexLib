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

#ifndef LUMEX_CORE_TEMPORARY_TMP_HPP
#define LUMEX_CORE_TEMPORARY_TMP_HPP

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

#include <string>

#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace temporary
{
namespace tmp
{
/**
 * @brief RAII wrapper for temporary directory management
 * @details Automatically removes temporary directory when destroyed
 */
class LUMEX_API TemporaryDirectory
{
public:
  TemporaryDirectory () : m_valid (false) {}
  explicit TemporaryDirectory (lumex::path const &path);
  ~TemporaryDirectory ();

  // Non-copyable, movable
  TemporaryDirectory (TemporaryDirectory const &) = delete;
  TemporaryDirectory &operator= (TemporaryDirectory const &) = delete;

  TemporaryDirectory (TemporaryDirectory &&other) LUMEX_NOEXCEPT;
  TemporaryDirectory &operator= (TemporaryDirectory &&other) LUMEX_NOEXCEPT;

  lumex::path const &
  path () const
  {
    return m_path;
  }
  bool
  is_valid () const
  {
    return m_valid;
  }
  void release (); // Don't auto-remove on destruction

private:
  lumex::path m_path; ///< Path to the temporary directory.
  bool m_valid;       ///< Flag indicating if the temporary directory is valid.
};

/**
 * @brief RAII wrapper for temporary file management
 * @details Automatically removes temporary file when destroyed
 */
class LUMEX_API TemporaryFile
{
public:
  TemporaryFile () : m_valid (false) {}
  explicit TemporaryFile (lumex::path const &path);
  ~TemporaryFile ();

  // Non-copyable, movable
  TemporaryFile (TemporaryFile const &) = delete;
  TemporaryFile &operator= (TemporaryFile const &) = delete;

  TemporaryFile (TemporaryFile &&other) LUMEX_NOEXCEPT;
  TemporaryFile &operator= (TemporaryFile &&other) LUMEX_NOEXCEPT;

  lumex::path const &
  path () const
  {
    return m_path;
  }
  bool
  is_valid () const
  {
    return m_valid;
  }
  void release (); // Don't auto-remove on destruction

private:
  lumex::path m_path;
  bool m_valid;
};

class LUMEX_API LumexTemporary
{
public:
  /**
   * @brief Returns the path to the temporary directory.
   * @return The path to the temporary directory.
   */
  static lumex::path get_temp_directory_path ();

  /**
   * @brief Creates a temporary directory with optional name prefix
   * @param name Prefix for directory name (can be empty)
   * @return filesystem_result containing TemporaryDirectory on success
   */
  static lumex::filesystem_result<TemporaryDirectory>
  create_temp_directory (std::string const &name = std::string ());

  /**
   * @brief Removes a temporary directory
   * @param path Path to directory to remove
   * @return filesystem_result indicating success/failure
   */
  static lumex::filesystem_result<void>
  remove_temp_directory (lumex::path const &path);

  /**
   * @brief Creates a temporary file with optional name prefix
   * @param name Prefix for file name (can be empty)
   * @return filesystem_result containing TemporaryFile on success
   */
  static lumex::filesystem_result<TemporaryFile>
  create_temp_file (std::string const &name = std::string ());

  /**
   * @brief Removes a temporary file
   * @param path Path to file to remove
   * @return filesystem_result indicating success/failure
   */
  static lumex::filesystem_result<void>
  remove_temp_file (lumex::path const &path);

  /**
   * @brief Generate unique temporary name with prefix
   * @param prefix Optional prefix for the name
   * @return Unique string suitable for temporary files/directories
   */
  static std::string generate_temp_name (std::string const &prefix
                                         = std::string ());

private:
  // Helper methods
  static std::string _generate_random_suffix ();
  static bool _ensure_temp_directory_exists (lumex::path const &temp_dir);
};
} // namespace core
} // namespace tmp
} // namespace temporary
} // namespace lumex

using TemporaryFile = lumex::core::temporary::tmp::TemporaryFile;
using TemporaryDirectory = lumex::core::temporary::tmp::TemporaryDirectory;

using LumexTemporary = lumex::core::temporary::tmp::LumexTemporary;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_TEMPORARY_TMP_HPP

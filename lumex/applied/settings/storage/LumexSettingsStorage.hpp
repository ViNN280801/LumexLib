/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexSettingsStorage.hpp
 * @brief `replace_file_content`, the write through a temporary file that the
 * INI, JSON and XML settings implementations save with.
 * @details The content is written to `<path>.tmp` in the same directory and
 * that file is renamed over `<path>` only when it is complete, so a failed
 * write leaves the previous file as it was instead of truncated or missing.
 */
#ifndef LUMEX_APPLIED_SETTINGS_STORAGE_HPP
#define LUMEX_APPLIED_SETTINGS_STORAGE_HPP

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <string>

#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace settings
{
namespace storage
{
/// Suffix appended to the target path to name the temporary file that
/// `replace_file_content` writes first.
LUMEX_CONST_STR TEMPORARY_FILE_SUFFIX = ".tmp";

/// How `replace_file_content` opens the temporary file.
enum class LumexWriteMode : std::uint8_t
{
  text,  ///< Text mode: on Windows every `\n` becomes `\r\n`.
  binary ///< Binary mode: the bytes are written unchanged.
};

/**
 * @brief Replaces the content of a file without ever leaving it truncated.
 * @details Creates the parent directory when it does not exist, writes
 * `content` to `path` + `TEMPORARY_FILE_SUFFIX`, closes it, and renames it
 * over `path` (replacing an existing file on Windows too). If opening,
 * writing, closing or renaming fails, the temporary file is removed when
 * this call created it, `path` keeps its previous content (or stays
 * absent), and the call returns `false`.
 * @note The renamed file is a new file: it gets the default permissions of
 * a new file, and a symbolic link at `path` is replaced by a regular file
 * instead of being followed. An existing directory or a file that cannot be
 * written at the temporary path makes the call fail.
 * @param path The file to replace.
 * @param content The complete new content.
 * @param mode Whether the temporary file is opened in text or binary mode.
 * @return `true` if `path` holds `content` after the call, `false`
 * otherwise. Does not throw.
 */
LUMEX_API bool replace_file_content (std::string const &path,
                                     std::string const &content,
                                     LumexWriteMode mode) LUMEX_NOEXCEPT;
} // namespace storage
} // namespace settings
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_SETTINGS_STORAGE_HPP

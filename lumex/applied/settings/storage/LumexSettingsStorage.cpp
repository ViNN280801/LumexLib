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

#define LUMEX_IMPLEMENTATION
#include <fstream>
#include <ios>
#include <string>

#include "LumexSettingsStorage.hpp"
#include "lumex/core/filesystem/LumexFilesystem"

namespace lumex
{
namespace applied
{
namespace settings
{
namespace storage
{
namespace
{
/// Removes a temporary file this module created; failures are ignored.
void
remove_temporary (std::string const &temporary) LUMEX_NOEXCEPT
{
  try
    {
      lumex::core::filesystem::fs::lumex_filesystem::remove (
          lumex::path (temporary));
    }
  catch (...)
    {
    }
}
} // namespace

LUMEX_PUBLIC_API
bool
replace_file_content (std::string const &path, std::string const &content,
                      LumexWriteMode mode) LUMEX_NOEXCEPT
{
  if (path.empty ())
    return false;

  std::string temporary;
  bool created = false;
  try
    {
      lumex::path const target (path);
      lumex::path const parent = target.parent_path ();
      if (!parent.empty ()
          && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
        lumex::core::filesystem::fs::lumex_filesystem::create_directories (
            parent);

      temporary = path + TEMPORARY_FILE_SUFFIX;
      std::ios_base::openmode const open_mode
          = mode == LumexWriteMode::binary
                ? std::ios_base::out | std::ios_base::trunc
                      | std::ios_base::binary
                : std::ios_base::out | std::ios_base::trunc;

      std::ofstream out (temporary.c_str (), open_mode);
      if (!out.is_open ())
        return false;
      created = true;

      out.write (content.data (),
                 static_cast<std::streamsize> (content.size ()));
      out.close ();
      if (!out.good ())
        {
          remove_temporary (temporary);
          return false;
        }

      if (!lumex::core::filesystem::fs::lumex_filesystem::rename (
               lumex::path (temporary), target)
               .success ())
        {
          remove_temporary (temporary);
          return false;
        }
      return true;
    }
  catch (...)
    {
      if (created)
        remove_temporary (temporary);
      return false;
    }
}
} // namespace storage
} // namespace settings
} // namespace applied
} // namespace lumex

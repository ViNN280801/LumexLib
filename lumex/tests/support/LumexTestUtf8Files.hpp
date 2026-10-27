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
 * @file LumexTestUtf8Files.hpp
 * @brief Test helpers that make, read and remove files and directories by a
 * UTF-8 path through the system itself (the wide `...W` functions on Windows,
 * the bytes on POSIX).
 * @details The tests of the modules that open files by a UTF-8 path (a
 * Cyrillic user profile on Windows) ask the system through these helpers, so
 * a check does not depend on the code under test: a file that the code wrote
 * under a garbled name is not found here. The helpers know nothing of
 * LumexLib, so a module that must not depend on `lumex::filesystem` can use
 * them.
 */
#ifndef LUMEX_TESTS_SUPPORT_UTF8_FILES_HPP
#define LUMEX_TESTS_SUPPORT_UTF8_FILES_HPP

#include <fstream>
#include <iterator>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lumex_test
{
namespace utf8_files
{
/// "Привет" (Russian) in UTF-8: a name an ANSI code page other than 1251 (and
/// the UTF-8 bytes read as ANSI everywhere) cannot give back.
inline std::string
cyrillic (std::string const &suffix = std::string ())
{
  return std::string ("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82")
         + suffix;
}

#if defined(_WIN32)
/// The UTF-16 form of a UTF-8 path (the system's own conversion).
inline std::wstring
wide (std::string const &utf8)
{
  if (utf8.empty ())
    return std::wstring ();
  int const size = MultiByteToWideChar (
      CP_UTF8, 0, utf8.data (), static_cast<int> (utf8.size ()), nullptr, 0);
  std::wstring result (static_cast<std::size_t> (size > 0 ? size : 0), L'\0');
  if (size > 0)
    MultiByteToWideChar (CP_UTF8, 0, utf8.data (),
                         static_cast<int> (utf8.size ()), &result[0], size);
  return result;
}
#endif

/// Creates the directory `path` (one level); true on success.
inline bool
make_directory (std::string const &path)
{
#if defined(_WIN32)
  return CreateDirectoryW (wide (path).c_str (), nullptr) != 0;
#else
  return mkdir (path.c_str (), 0755) == 0;
#endif
}

/// Removes the empty directory `path`; true on success.
inline bool
remove_directory (std::string const &path)
{
#if defined(_WIN32)
  return RemoveDirectoryW (wide (path).c_str ()) != 0;
#else
  return rmdir (path.c_str ()) == 0;
#endif
}

/// Whether anything is under the name `path`.
inline bool
exists (std::string const &path)
{
#if defined(_WIN32)
  return GetFileAttributesW (wide (path).c_str ()) != INVALID_FILE_ATTRIBUTES;
#else
  struct stat status;
  return lstat (path.c_str (), &status) == 0;
#endif
}

/// The size the system reports for the file `path`; -1 when there is none.
inline long long
file_size (std::string const &path)
{
#if defined(_WIN32)
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExW (wide (path).c_str (), GetFileExInfoStandard, &data)
      == 0)
    return -1;
  return (static_cast<long long> (data.nFileSizeHigh) << 32)
         | static_cast<long long> (data.nFileSizeLow);
#else
  struct stat status;
  if (stat (path.c_str (), &status) != 0)
    return -1;
  return static_cast<long long> (status.st_size);
#endif
}

/// Removes the file `path`; true when it is gone (or was never there).
inline bool
remove_file (std::string const &path)
{
#if defined(_WIN32)
  std::wstring const name = wide (path);
  SetFileAttributesW (name.c_str (), FILE_ATTRIBUTE_NORMAL);
  return DeleteFileW (name.c_str ()) != 0
         || GetLastError () == ERROR_FILE_NOT_FOUND
         || GetLastError () == ERROR_PATH_NOT_FOUND;
#else
  return unlink (path.c_str ()) == 0 || !exists (path);
#endif
}

/// Writes `content` to the file `path` (replaced); true on success.
inline bool
write_file (std::string const &path, std::string const &content)
{
#if defined(_WIN32)
  HANDLE const file
      = CreateFileW (wide (path).c_str (), GENERIC_WRITE, 0, nullptr,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  DWORD written = 0;
  bool const ok
      = content.empty ()
        || (WriteFile (file, content.data (),
                       static_cast<DWORD> (content.size ()), &written, nullptr)
                != 0
            && written == content.size ());
  CloseHandle (file);
  return ok;
#else
  int const descriptor
      = open (path.c_str (), O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (descriptor < 0)
    return false;
  bool const ok = content.empty ()
                  || write (descriptor, content.data (), content.size ())
                         == static_cast<ssize_t> (content.size ());
  close (descriptor);
  return ok;
#endif
}

/// Reads the whole file `path` into `content`; false when it cannot be read.
inline bool
read_file (std::string const &path, std::string &content)
{
  content.clear ();
#if defined(_WIN32)
  HANDLE const file = CreateFileW (
      wide (path).c_str (), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  char buffer[4096];
  DWORD read = 0;
  while (ReadFile (file, buffer, sizeof (buffer), &read, nullptr) != 0
         && read > 0)
    content.append (buffer, read);
  CloseHandle (file);
  return true;
#else
  int const descriptor = open (path.c_str (), O_RDONLY);
  if (descriptor < 0)
    return false;
  char buffer[4096];
  ssize_t count = 0;
  while ((count = read (descriptor, buffer, sizeof (buffer))) > 0)
    content.append (buffer, static_cast<std::size_t> (count));
  close (descriptor);
  return true;
#endif
}
} // namespace utf8_files
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_UTF8_FILES_HPP

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
 * @file LumexTestCyrillicCopy.hpp
 * @brief Windows test helper: runs a test of the current test executable from
 * a copy of the executable (and of its DLLs) in a directory with a Cyrillic
 * name, as when the program is installed under a Cyrillic user profile.
 * @details A module that finds its own directory by itself (the logger and the
 * crash report write next to the executable) cannot be moved into such a
 * directory from inside the process. The parent test calls `make_copy`, puts
 * the files the child needs in the directory, calls `run` and reads what the
 * child left; the child is a test of the same executable that does nothing
 * unless `is_child ()` (the environment variable is set by `run`). Everything
 * here goes through the wide Win32 functions. On other systems the header
 * declares nothing: the tests that use it are `#if defined(_WIN32)`.
 */
#ifndef LUMEX_TESTS_SUPPORT_CYRILLIC_COPY_HPP
#define LUMEX_TESTS_SUPPORT_CYRILLIC_COPY_HPP

#if defined(_WIN32)
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>

#include "lumex/tests/support/LumexTestUtf8Files.hpp"

namespace lumex_test
{
namespace cyrillic_copy
{
/// Name of the environment variable that marks the child process.
inline wchar_t const *
child_variable ()
{
  return L"LUMEX_TEST_CYRILLIC_CHILD";
}

/// True in the process started by `run`.
inline bool
is_child ()
{
  return GetEnvironmentVariableW (child_variable (), nullptr, 0) != 0;
}

inline std::string
to_utf8 (std::wstring const &wide)
{
  if (wide.empty ())
    return std::string ();
  int const size = WideCharToMultiByte (CP_UTF8, 0, wide.data (),
                                        static_cast<int> (wide.size ()),
                                        nullptr, 0, nullptr, nullptr);
  std::string result (static_cast<std::size_t> (size > 0 ? size : 0), '\0');
  if (size > 0)
    WideCharToMultiByte (CP_UTF8, 0, wide.data (),
                         static_cast<int> (wide.size ()), &result[0], size,
                         nullptr, nullptr);
  return result;
}

/// The full path of the running executable (wide).
inline std::wstring
self_path ()
{
  std::wstring path (32768, L'\0');
  DWORD const length = GetModuleFileNameW (nullptr, &path[0],
                                           static_cast<DWORD> (path.size ()));
  path.resize (length);
  return path;
}

/// The directory of the running executable (UTF-8, no trailing separator).
inline std::string
self_directory ()
{
  std::wstring const path = self_path ();
  return to_utf8 (path.substr (0, path.find_last_of (L"\\/")));
}

/// The names (UTF-8) of the files and directories in `directory`, without
/// `.` and `..`.
inline std::vector<std::string>
entries (std::string const &directory)
{
  std::vector<std::string> names;
  WIN32_FIND_DATAW data;
  HANDLE const find = FindFirstFileW (
      (utf8_files::wide (directory) + L"\\*").c_str (), &data);
  if (find == INVALID_HANDLE_VALUE)
    return names;
  do
    {
      std::wstring const name = data.cFileName;
      if (name != L"." && name != L"..")
        names.push_back (to_utf8 (name));
    }
  while (FindNextFileW (find, &data) != 0);
  FindClose (find);
  return names;
}

/// Removes the directory `wide_directory` (UTF-16, so an entry whose name is
/// not valid Unicode goes too) with everything in it; the files are made
/// writable.
inline void
remove_tree_wide (std::wstring const &wide_directory)
{
  WIN32_FIND_DATAW data;
  HANDLE const find
      = FindFirstFileW ((wide_directory + L"\\*").c_str (), &data);
  if (find != INVALID_HANDLE_VALUE)
    {
      do
        {
          std::wstring const name = data.cFileName;
          if (name == L"." || name == L"..")
            continue;
          std::wstring const full = wide_directory + L"\\" + name;
          if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            remove_tree_wide (full);
          else
            {
              SetFileAttributesW (full.c_str (), FILE_ATTRIBUTE_NORMAL);
              DeleteFileW (full.c_str ());
            }
        }
      while (FindNextFileW (find, &data) != 0);
      FindClose (find);
    }
  RemoveDirectoryW (wide_directory.c_str ());
}

/// Removes `directory` (UTF-8) with everything in it.
inline void
remove_tree (std::string const &directory)
{
  remove_tree_wide (utf8_files::wide (directory));
}

/**
 * @brief Copies the running executable and every DLL beside it into a new
 * directory `<executable directory>/<Cyrillic><suffix>`.
 * @param suffix Added to the Cyrillic name, so tests of one binary do not
 * share a directory.
 * @param directory Receives the directory (UTF-8); a leftover of an earlier
 * run is removed first.
 * @param exe_name The file name (UTF-8) of the copied executable; empty keeps
 * the name of the running one.
 * @return true when every file was copied.
 */
inline bool
make_copy (std::string const &suffix, std::string &directory,
           std::string const &exe_name = std::string ())
{
  directory = self_directory () + "/" + utf8_files::cyrillic (suffix);
  remove_tree (directory);
  if (!utf8_files::make_directory (directory))
    return false;
  std::wstring const self = self_path ();
  std::wstring const self_dir = self.substr (0, self.find_last_of (L"\\/"));
  std::wstring const target = utf8_files::wide (directory);
  std::wstring const leaf = exe_name.empty ()
                                ? self.substr (self.find_last_of (L"\\/") + 1)
                                : utf8_files::wide (exe_name);
  bool ok = CopyFileW (self.c_str (), (target + L"\\" + leaf).c_str (), FALSE)
            != 0;
  WIN32_FIND_DATAW data;
  HANDLE const find = FindFirstFileW ((self_dir + L"\\*.dll").c_str (), &data);
  if (find != INVALID_HANDLE_VALUE)
    {
      do
        ok = CopyFileW ((self_dir + L"\\" + data.cFileName).c_str (),
                        (target + L"\\" + data.cFileName).c_str (), FALSE)
                 != 0
             && ok;
      while (FindNextFileW (find, &data) != 0);
      FindClose (find);
    }
  return ok;
}

/**
 * @brief Runs the copy made by `make_copy` with `--gtest_filter=<filter>` and
 * the child variable set, in the copy's directory, and waits for it.
 * @param arguments More command-line arguments of the child (UTF-8).
 * @param exe_name The name given to `make_copy`; empty for the default.
 * @return the exit code of the child (0 when its tests passed), -1 when it
 * could not be started.
 */
inline int
run (std::string const &directory, std::string const &filter,
     std::string const &arguments = std::string (),
     std::string const &exe_name = std::string ())
{
  std::wstring const self = self_path ();
  std::wstring const wide_directory = utf8_files::wide (directory);
  std::wstring const exe
      = wide_directory + L"\\"
        + (exe_name.empty () ? self.substr (self.find_last_of (L"\\/") + 1)
                             : utf8_files::wide (exe_name));
  std::wstring command
      = L"\"" + exe + L"\" --gtest_filter=" + utf8_files::wide (filter);
  if (!arguments.empty ())
    command += L" " + utf8_files::wide (arguments);
  SetEnvironmentVariableW (child_variable (), L"1");
  STARTUPINFOW startup;
  ZeroMemory (&startup, sizeof (startup));
  startup.cb = sizeof (startup);
  PROCESS_INFORMATION process;
  ZeroMemory (&process, sizeof (process));
  BOOL const started
      = CreateProcessW (exe.c_str (), &command[0], nullptr, nullptr, TRUE, 0,
                        nullptr, wide_directory.c_str (), &startup, &process);
  SetEnvironmentVariableW (child_variable (), nullptr);
  if (started == 0)
    return -1;
  WaitForSingleObject (process.hProcess, 120000);
  DWORD code = static_cast<DWORD> (-1);
  GetExitCodeProcess (process.hProcess, &code);
  CloseHandle (process.hProcess);
  CloseHandle (process.hThread);
  return static_cast<int> (code);
}

/// Makes the file `path` (UTF-16, so a name that is not valid Unicode can be
/// used) `size` bytes long without writing them: a sparse file where the
/// volume supports it. true on success.
inline bool
make_sparse_file_wide (std::wstring const &path, long long size)
{
  HANDLE const file
      = CreateFileW (path.c_str (), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  DWORD returned = 0;
  DeviceIoControl (file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned,
                   nullptr);
  LARGE_INTEGER position;
  position.QuadPart = size;
  bool const ok = SetFilePointerEx (file, position, nullptr, FILE_BEGIN) != 0
                  && SetEndOfFile (file) != 0;
  CloseHandle (file);
  return ok;
}

/// Makes the file `path` (UTF-8) `size` bytes long without writing them: a
/// sparse file where the volume supports it. true on success.
inline bool
make_sparse_file (std::string const &path, long long size)
{
  return make_sparse_file_wide (utf8_files::wide (path), size);
}
} // namespace cyrillic_copy
} // namespace lumex_test

#endif // defined(_WIN32)

#endif // !LUMEX_TESTS_SUPPORT_CYRILLIC_COPY_HPP

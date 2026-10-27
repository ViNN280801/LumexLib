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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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
 * @file LumexTestNonAsciiDirectory.hpp
 * @brief Windows test helper: a directory with a Cyrillic name, a copy of the
 * running test binary in it, and a child process started from there.
 * @details The ANSI path functions (GetModuleFileNameA and friends) return
 * the text in the ANSI code page of the process, so a test that runs from
 * ASCII folders never sees them fail. A test that has to prove a module
 * handles a non-ASCII executable directory copies its own binary and the DLLs
 * beside it into `workspace::directory ()` and runs one DISABLED_ test of that
 * copy (`--gtest_also_run_disabled_tests`); the copy reports through the file
 * named by the `LUMEX_ANSI_RESULT` variable (`child_result_path ()`,
 * `write_child_result ()`), the parent reads it with `read_result ()`.
 *
 * Every name here is made with the wide Win32 API and UTF-16 literals, never
 * with a converter of LumexLib, so a broken converter cannot hide itself.
 * Nothing depends on LumexLib.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_NON_ASCII_DIRECTORY_HPP
#define LUMEX_TESTS_SUPPORT_TEST_NON_ASCII_DIRECTORY_HPP

#if defined(_WIN32)

#include <cstddef>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace lumex_test
{
namespace non_ascii
{
/// The name of the variable that carries the result file to the child.
inline wchar_t const *
result_variable ()
{
  return L"LUMEX_ANSI_RESULT";
}

/// A Cyrillic name as UTF-16 code units.
inline wchar_t const *
cyrillic_name ()
{
  return L"\u041A\u0430\u0442\u0430\u043B\u043E\u0433";
}

/// The path of the running executable (empty on failure).
inline std::wstring
own_binary_path ()
{
  std::wstring path (MAX_PATH, L'\0');
  for (;;)
    {
      DWORD const length = GetModuleFileNameW (
          nullptr, &path[0], static_cast<DWORD> (path.size ()));
      if (length == 0)
        return std::wstring ();
      if (length < path.size ())
        {
          path.resize (length);
          return path;
        }
      path.resize (path.size () * 2);
    }
}

/// The part of `path` before the last separator.
inline std::wstring
parent_of (std::wstring const &path)
{
  std::wstring::size_type const slash = path.find_last_of (L"\\/");
  return slash == std::wstring::npos ? std::wstring ()
                                     : path.substr (0, slash);
}

/// The part of `path` after the last separator.
inline std::wstring
leaf_of (std::wstring const &path)
{
  std::wstring::size_type const slash = path.find_last_of (L"\\/");
  return slash == std::wstring::npos ? path : path.substr (slash + 1);
}

/// UTF-16 to UTF-8 (the Win32 call; for the expected text of a check).
inline std::string
utf8_of (std::wstring const &text)
{
  if (text.empty ())
    return std::string ();
  int const size = WideCharToMultiByte (
      CP_UTF8, WC_ERR_INVALID_CHARS, text.data (),
      static_cast<int> (text.size ()), nullptr, 0, nullptr, nullptr);
  if (size <= 0)
    return std::string ();
  std::string utf8 (static_cast<std::size_t> (size), '\0');
  WideCharToMultiByte (CP_UTF8, WC_ERR_INVALID_CHARS, text.data (),
                       static_cast<int> (text.size ()), &utf8[0], size,
                       nullptr, nullptr);
  return utf8;
}

/// UTF-8 to UTF-16 (the Win32 call; for the path a child reports).
inline std::wstring
wide_of (std::string const &text)
{
  if (text.empty ())
    return std::wstring ();
  int const size
      = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, text.data (),
                             static_cast<int> (text.size ()), nullptr, 0);
  if (size <= 0)
    return std::wstring ();
  std::wstring wide (static_cast<std::size_t> (size), L'\0');
  MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, text.data (),
                       static_cast<int> (text.size ()), &wide[0], size);
  return wide;
}

/// The whole content of the file `path`; false when it cannot be read.
inline bool
read_bytes (std::wstring const &path, std::string &bytes)
{
  bytes.clear ();
  HANDLE const file = CreateFileW (
      path.c_str (), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  char buffer[4096];
  DWORD got = 0;
  while (ReadFile (file, buffer, sizeof (buffer), &got, nullptr) && got > 0)
    bytes.append (buffer, got);
  CloseHandle (file);
  return true;
}

/// Writes `bytes` to the file `path` (replaced).
inline bool
write_bytes (std::wstring const &path, void const *bytes, std::size_t size)
{
  HANDLE const file
      = CreateFileW (path.c_str (), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  DWORD written = 0;
  BOOL const ok = size == 0
                  || WriteFile (file, bytes, static_cast<DWORD> (size),
                                &written, nullptr);
  CloseHandle (file);
  return ok != 0 && written == size;
}

/// Child side: the result file the parent asked for (empty when the test
/// runs by itself).
inline std::wstring
child_result_path ()
{
  wchar_t buffer[MAX_PATH * 4] = {};
  DWORD const length = GetEnvironmentVariableW (
      result_variable (), buffer,
      static_cast<DWORD> (sizeof (buffer) / sizeof (buffer[0])));
  if (length == 0 || length >= sizeof (buffer) / sizeof (buffer[0]))
    return std::wstring ();
  return std::wstring (buffer, length);
}

/// Child side: stores `bytes` for the parent; false when no file was asked
/// for or it cannot be written.
inline bool
write_child_result (void const *bytes, std::size_t size)
{
  std::wstring const path = child_result_path ();
  return !path.empty () && write_bytes (path, bytes, size);
}

/// Removes the files of `directory` and then the directory (best effort).
inline void
remove_tree (std::wstring const &directory)
{
  WIN32_FIND_DATAW data = {};
  HANDLE const find = FindFirstFileW ((directory + L"\\*").c_str (), &data);
  if (find != INVALID_HANDLE_VALUE)
    {
      do
        {
          std::wstring const name = data.cFileName;
          if (name == L"." || name == L"..")
            continue;
          std::wstring const full = directory + L"\\" + name;
          if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            remove_tree (full);
          else
            {
              SetFileAttributesW (full.c_str (), FILE_ATTRIBUTE_NORMAL);
              DeleteFileW (full.c_str ());
            }
        }
      while (FindNextFileW (find, &data) != 0);
      FindClose (find);
    }
  RemoveDirectoryW (directory.c_str ());
}

/// True when a file whose name matches `pattern` (wildcards) is in
/// `directory`.
inline bool
has_file (std::wstring const &directory, wchar_t const *pattern)
{
  WIN32_FIND_DATAW data = {};
  HANDLE const find
      = FindFirstFileW ((directory + L"\\" + pattern).c_str (), &data);
  if (find == INVALID_HANDLE_VALUE)
    return false;
  FindClose (find);
  return true;
}

/**
 * @brief A temporary folder `<temp>\lumex_ansi_<pid>_<tick>` that holds a
 * subdirectory with a Cyrillic name; removed with everything in it.
 */
class workspace
{
public:
  workspace ()
  {
    wchar_t temp[MAX_PATH + 1] = {};
    DWORD const length = GetTempPathW (MAX_PATH, temp);
    if (length == 0 || length > MAX_PATH)
      return;
    std::wstring base (temp, length);
    while (!base.empty () && (base.back () == L'\\' || base.back () == L'/'))
      base.pop_back ();
    m_root = base + L"\\lumex_ansi_" + std::to_wstring (GetCurrentProcessId ())
             + L"_" + std::to_wstring (GetTickCount64 ());
    m_directory = m_root + L"\\" + cyrillic_name ();
    m_ok = CreateDirectoryW (m_root.c_str (), nullptr) != 0
           && CreateDirectoryW (m_directory.c_str (), nullptr) != 0;
  }

  workspace (workspace const &) = delete;
  workspace &operator= (workspace const &) = delete;

  ~workspace ()
  {
    if (!m_root.empty ())
      remove_tree (m_root);
  }

  bool
  ok () const
  {
    return m_ok;
  }

  /// The folder with an ASCII-only part of the name (unless the temp folder
  /// of the user is not ASCII): the result file lives here.
  std::wstring const &
  root () const
  {
    return m_root;
  }

  /// The folder with the Cyrillic name.
  std::wstring const &
  directory () const
  {
    return m_directory;
  }

  /// The result file of the child.
  std::wstring
  result_file () const
  {
    return m_root + L"\\result.bin";
  }

  /// The standard output and error of the child.
  std::wstring
  log_file () const
  {
    return m_root + L"\\child.log";
  }

  /// The text the child printed (for a failure message).
  std::string
  child_log () const
  {
    std::string text;
    read_bytes (log_file (), text);
    return text;
  }

  /// Copies the running test binary and every DLL beside it into
  /// `directory ()`; returns the path of the copy (empty on failure). The
  /// copy keeps the file name unless `file_name` gives another one.
  std::wstring
  copy_binary (std::wstring const &file_name = std::wstring ()) const
  {
    std::wstring const own = own_binary_path ();
    if (own.empty () || !m_ok)
      return std::wstring ();
    std::wstring const source = parent_of (own);
    std::wstring const copy
        = m_directory + L"\\"
          + (file_name.empty () ? leaf_of (own) : file_name);
    if (CopyFileW (own.c_str (), copy.c_str (), FALSE) == 0)
      return std::wstring ();
    WIN32_FIND_DATAW data = {};
    HANDLE const find = FindFirstFileW ((source + L"\\*.dll").c_str (), &data);
    if (find != INVALID_HANDLE_VALUE)
      {
        do
          CopyFileW ((source + L"\\" + data.cFileName).c_str (),
                     (m_directory + L"\\" + data.cFileName).c_str (), FALSE);
        while (FindNextFileW (find, &data) != 0);
        FindClose (find);
      }
    return copy;
  }

  /**
   * @brief Runs `binary` with `arguments`, the working directory
   * `directory ()` and the result file in the environment; waits for it.
   * @return The exit code of the child, or `0xFFFFFFFF` when it could not be
   * started or did not end within `timeout_ms` (it is then killed).
   */
  DWORD
  run (std::wstring const &binary, std::wstring const &arguments,
       DWORD timeout_ms = 60000) const
  {
    std::wstring command = L"\"" + binary + L"\" " + arguments;
    SetEnvironmentVariableW (result_variable (), result_file ().c_str ());
    // The output of the child goes to a file (log_file ()), so a failing
    // child can be read in the failure message of the parent.
    SECURITY_ATTRIBUTES inherit = {};
    inherit.nLength = sizeof (inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE const log = CreateFileW (log_file ().c_str (), GENERIC_WRITE,
                                    FILE_SHARE_READ, &inherit, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW startup = {};
    startup.cb = sizeof (startup);
    if (log != INVALID_HANDLE_VALUE)
      {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = log;
        startup.hStdError = log;
        startup.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
      }
    PROCESS_INFORMATION process = {};
    BOOL const started = CreateProcessW (
        binary.c_str (), &command[0], nullptr, nullptr,
        log != INVALID_HANDLE_VALUE ? TRUE : FALSE, CREATE_NO_WINDOW, nullptr,
        m_directory.c_str (), &startup, &process);
    SetEnvironmentVariableW (result_variable (), nullptr);
    if (log != INVALID_HANDLE_VALUE)
      CloseHandle (log);
    if (started == 0)
      return 0xFFFFFFFFu;
    CloseHandle (process.hThread);
    DWORD code = 0xFFFFFFFFu;
    if (WaitForSingleObject (process.hProcess, timeout_ms) == WAIT_OBJECT_0)
      GetExitCodeProcess (process.hProcess, &code);
    else
      {
        TerminateProcess (process.hProcess, 9);
        WaitForSingleObject (process.hProcess, INFINITE);
      }
    CloseHandle (process.hProcess);
    return code;
  }

private:
  std::wstring m_root;
  std::wstring m_directory;
  bool m_ok = false;
};
} // namespace non_ascii
} // namespace lumex_test

#endif // defined(_WIN32)

#endif // !LUMEX_TESTS_SUPPORT_TEST_NON_ASCII_DIRECTORY_HPP

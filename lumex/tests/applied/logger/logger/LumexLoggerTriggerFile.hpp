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
 * @file LumexLoggerTriggerFile.hpp
 * @brief Test helpers that drive the `LumexLogger` singleton through its
 * trigger file: the directory the logger reads it from, the file text in the
 * compiled config format, and small file helpers.
 * @details Used by the re-read tests of the logger
 * (LumexLoggerReread.cxx11.tests.cpp) and by the tests of the transport
 * adapter (lumex/tests/applied/transport/log/adapter). The tests that use it
 * share the log file next to the logger, so their CMakeLists.txt give them
 * the CTest RESOURCE_LOCK `lumex_logger_trigger_file`; each test sets a
 * trigger file name of its own, so the `enable_logs` of a developer machine
 * is never touched.
 */
#ifndef LUMEX_TESTS_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_HPP
#define LUMEX_TESTS_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_HPP

#include <cstdio>
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
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <cstdint>
#include <mach-o/dyld.h>
#endif
#endif

#include "lumex/applied/logger/LumexLogger"

namespace lumex_test
{
namespace logger_trigger
{
/// True when the compiled config format can be written by `config_text`
/// (every format but the reserved YAML).
inline bool
format_supported ()
{
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
  return false;
#else
  return true;
#endif
}

/// The directory of a path, without the last separator.
inline std::string
directory_of (std::string const &path)
{
  std::string::size_type const slash = path.find_last_of ("/\\");
  return slash == std::string::npos ? std::string (".")
                                    : path.substr (0, slash);
}

/**
 * @brief The directory the logger reads its trigger file from and writes
 * its log to: the module that holds the singleton on Windows (the logger
 * DLL, or the executable when it is linked statically), the executable
 * elsewhere (LumexLogger::_get_executable_directory).
 */
inline std::string
logger_directory ()
{
#if defined(_WIN32)
  HMODULE module = nullptr;
  char path[MAX_PATH] = { 0 };
  if (GetModuleHandleExA (
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
              | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCSTR> (&::LumexLogger::get_instance ()), &module)
      && GetModuleFileNameA (module, path, MAX_PATH) > 0)
    return directory_of (path);
  if (GetModuleFileNameA (nullptr, path, MAX_PATH) > 0)
    return directory_of (path);
  return ".";
#elif defined(__APPLE__)
  char path[4096] = { 0 };
  std::uint32_t size = sizeof (path);
  if (_NSGetExecutablePath (path, &size) == 0)
    return directory_of (path);
  return ".";
#elif defined(__linux__)
  char path[4096] = { 0 };
  ssize_t const length = readlink ("/proc/self/exe", path, sizeof (path) - 1);
  if (length > 0)
    return directory_of (
        std::string (path, static_cast<std::size_t> (length)));
  return ".";
#else
  return ".";
#endif
}

/// `name` in the logger's directory.
inline std::string
path_in_logger_directory (std::string const &name)
{
#if defined(_WIN32)
  return logger_directory () + "\\" + name;
#else
  return logger_directory () + "/" + name;
#endif
}

/**
 * @brief The text of a trigger file in the compiled format with LEVEL set
 * to `level` and, when not empty, `extra` (KEY=value lines of the plain
 * text format, one per line, converted to the compiled format).
 */
inline std::string
config_text (std::string const &level, std::string const &extra = "")
{
  std::string lines = "LEVEL=" + level + "\n" + extra;
  if (!lines.empty () && lines[lines.size () - 1] != '\n')
    lines += '\n';
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
  return lines;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
  return "[logger]\n" + lines;
#else
  std::string body;
  std::string::size_type start = 0;
  while (start < lines.size ())
    {
      std::string::size_type const end = lines.find ('\n', start);
      std::string const line = lines.substr (start, end - start);
      start = end + 1;
      std::string::size_type const equal = line.find ('=');
      if (equal == std::string::npos)
        continue;
      std::string const key = line.substr (0, equal);
      std::string const value = line.substr (equal + 1);
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
      body += (body.empty () ? "" : ",\n") + std::string ("    \"") + key
              + "\": \"" + value + "\"";
#else
      body += "  <" + key + ">" + value + "</" + key + ">\n";
#endif
    }
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
  return "{\n  \"logger\": {\n" + body + "\n  }\n}\n";
#else
  return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<logger>\n" + body
         + "</logger>\n";
#endif
#endif
}

/// Writes `content` to `path` (replaced); false when it cannot be written.
inline bool
write_file (std::string const &path, std::string const &content)
{
  std::ofstream out (path.c_str (),
                     std::ios::out | std::ios::trunc | std::ios::binary);
  if (!out.is_open ())
    return false;
  out << content;
  out.close ();
  return !out.fail ();
}

/// The whole content of `path`; empty when it cannot be read.
inline std::string
read_file (std::string const &path)
{
  std::ifstream in (path.c_str (), std::ios::in | std::ios::binary);
  if (!in.is_open ())
    return std::string ();
  return std::string (std::istreambuf_iterator<char> (in),
                      std::istreambuf_iterator<char> ());
}

/// True when `path` exists and can be opened for reading.
inline bool
file_exists (std::string const &path)
{
  std::ifstream in (path.c_str ());
  return in.is_open ();
}

/// Removes `path`; true when it is gone (or was never there).
inline bool
remove_file (std::string const &path)
{
  std::remove (path.c_str ());
  return !file_exists (path);
}

/// Creates the directory `path`; true on success.
inline bool
make_directory (std::string const &path)
{
#if defined(_WIN32)
  return CreateDirectoryA (path.c_str (), nullptr) != 0;
#else
  return mkdir (path.c_str (), 0755) == 0;
#endif
}

/// Removes the empty directory `path`; true on success.
inline bool
remove_directory (std::string const &path)
{
#if defined(_WIN32)
  return RemoveDirectoryA (path.c_str ()) != 0;
#else
  return rmdir (path.c_str ()) == 0;
#endif
}
} // namespace logger_trigger
} // namespace lumex_test

#endif // !LUMEX_TESTS_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_HPP

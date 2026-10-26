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
 * @file LumexInterprocessLockWindows.cpp
 * @brief `interprocess_lock` on Windows: `CreateFileW` and `LockFileEx`.
 * @details CMake compiles this file on Windows only, and
 * `LumexInterprocessLockPosix.cpp` elsewhere.
 */
#if !defined(_WIN32)
#error                                                                        \
    "LumexInterprocessLockWindows.cpp is the Windows implementation; CMake compiles LumexInterprocessLockPosix.cpp on other systems"
#endif

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include <windows.h>

#include "lumex/core/interprocess/lock/LumexInterprocessLock.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything. impl holds a handle
// and a flag, so it is padded.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace interprocess
{
namespace lock
{
namespace
{
// The locked byte lies far behind any payload: a range locked through one
// handle cannot be read through another, and read_payload must read the
// payload while the lock is held. LockFileEx accepts a range behind the end of
// the file, and the file never grows that far.
DWORD const locked_offset_high = 0x7FFFFFFF;

// read_payload reads at most this much: the payload is a hint for a message,
// and any program may write into the file.
std::size_t const max_payload_read = 64 * 1024;

OVERLAPPED
locked_range () LUMEX_NOEXCEPT
{
  OVERLAPPED range = {};
  range.Offset = 0;
  range.OffsetHigh = locked_offset_high;
  return range;
}

lock_failure_t
failure (DWORD code) LUMEX_NOEXCEPT
{
  lock_failure_t result;
  result.system_code = static_cast<int> (code);
  switch (code)
    {
    case ERROR_LOCK_VIOLATION:
    // Another program opened the file without sharing it.
    case ERROR_SHARING_VIOLATION:
      result.kind = lock_error::busy;
      break;
    case ERROR_ACCESS_DENIED:
      result.kind = lock_error::access_denied;
      break;
    default:
      result.kind = lock_error::io_error;
      break;
    }
  return result;
}

// Writes the payload from the start of the file and cuts the file after it.
// Returns ERROR_SUCCESS or the error.
DWORD
write_payload (HANDLE file, std::string const &payload) LUMEX_NOEXCEPT
{
  if (payload.size () > MAXDWORD)
    return ERROR_INVALID_PARAMETER;
  LARGE_INTEGER start;
  start.QuadPart = 0;
  if (SetFilePointerEx (file, start, nullptr, FILE_BEGIN) == 0)
    return GetLastError ();
  DWORD written = 0;
  if (!payload.empty ()
      && (WriteFile (file, payload.data (),
                     static_cast<DWORD> (payload.size ()), &written, nullptr)
              == 0
          || written != payload.size ()))
    return GetLastError ();
  if (SetEndOfFile (file) == 0)
    return GetLastError ();
  return ERROR_SUCCESS;
}

// Closes a handle of read_payload on every way out.
class scoped_handle
{
public:
  explicit scoped_handle (HANDLE handle) LUMEX_NOEXCEPT : m_handle (handle) {}
  ~scoped_handle () { CloseHandle (m_handle); }
  scoped_handle (scoped_handle const &) = delete;
  scoped_handle &operator= (scoped_handle const &) = delete;

private:
  HANDLE m_handle;
};
} // namespace

// What a lock owns. It is made before anything is opened and releases what it
// got, so no way out of try_acquire leaks the handle or the lock.
struct interprocess_lock::impl
{
  impl () = default;
  impl (impl const &) = delete;
  impl &operator= (impl const &) = delete;

  ~impl ()
  {
    if (handle == INVALID_HANDLE_VALUE)
      return;
    if (locked)
      {
        // Closing the handle releases the lock too, but the system may do it
        // later; unlocking first frees it at once.
        OVERLAPPED range = locked_range ();
        UnlockFileEx (handle, 0, 1, 0, &range);
      }
    CloseHandle (handle);
  }

  HANDLE handle = INVALID_HANDLE_VALUE;
  bool locked = false;
};

interprocess_lock::acquire_result_t
interprocess_lock::try_acquire (path_t const &file, std::string const &payload)
{
  using lumex::core::expected::result::unexpect;
  using lumex::core::filesystem::fs::lumex_filesystem;

  path_t const parent = file.parent_path ();
  if (!parent.empty ())
    {
      auto const created = lumex_filesystem::create_directories (parent);
      if (!created.success ())
        return acquire_result_t (
            unexpect, failure (static_cast<DWORD> (created.error_code ())));
    }

  std::unique_ptr<impl> state (new impl ());
  // Read and write sharing: another try_acquire must reach LockFileEx (and get
  // busy), and read_payload must open the file while the lock is held.
  state->handle
      = CreateFileW (file.wstring ().c_str (), GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
  if (state->handle == INVALID_HANDLE_VALUE)
    return acquire_result_t (unexpect, failure (GetLastError ()));

  OVERLAPPED range = locked_range ();
  if (LockFileEx (state->handle,
                  LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                  &range)
      == 0)
    return acquire_result_t (unexpect, failure (GetLastError ()));
  state->locked = true;

  DWORD const written = write_payload (state->handle, payload);
  if (written != ERROR_SUCCESS)
    return acquire_result_t (unexpect, failure (written));

  return acquire_result_t (interprocess_lock (std::move (state)));
}

interprocess_lock::payload_result_t
interprocess_lock::read_payload (path_t const &file)
{
  using lumex::core::expected::result::unexpect;

  HANDLE const handle
      = CreateFileW (file.wstring ().c_str (), GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return payload_result_t (unexpect, failure (GetLastError ()));
  scoped_handle const closer (handle);

  std::string text;
  std::array<char, 4096> buffer;
  while (text.size () < max_payload_read)
    {
      DWORD got = 0;
      if (ReadFile (handle, buffer.data (),
                    static_cast<DWORD> (buffer.size ()), &got, nullptr)
          == 0)
        return payload_result_t (unexpect, failure (GetLastError ()));
      if (got == 0)
        break;
      text.append (buffer.data (), got);
    }
  if (text.size () > max_payload_read)
    text.resize (max_payload_read);
  return payload_result_t (std::move (text));
}

interprocess_lock::interprocess_lock (std::unique_ptr<impl> state)
    LUMEX_NOEXCEPT : m_impl (std::move (state))
{
}

interprocess_lock::interprocess_lock (interprocess_lock &&other) LUMEX_NOEXCEPT
    = default;

interprocess_lock &
interprocess_lock::operator= (interprocess_lock &&other) LUMEX_NOEXCEPT
    = default;

interprocess_lock::~interprocess_lock () = default;

void
interprocess_lock::release () LUMEX_NOEXCEPT
{
  m_impl.reset ();
}
} // namespace lock
} // namespace interprocess
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

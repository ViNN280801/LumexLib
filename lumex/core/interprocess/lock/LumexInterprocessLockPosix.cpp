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
 * @file LumexInterprocessLockPosix.cpp
 * @brief `interprocess_lock` on POSIX: `open` and `flock`.
 * @details CMake compiles this file everywhere but Windows, and
 * `LumexInterprocessLockWindows.cpp` there.
 */
#if defined(_WIN32)
#error                                                                        \
    "LumexInterprocessLockPosix.cpp is the POSIX implementation; CMake compiles LumexInterprocessLockWindows.cpp on Windows"
#endif

#include <array>
#include <cerrno>
#include <cstddef>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
// read_payload reads at most this much: the payload is a hint for a message,
// and any program may write into the file.
std::size_t const max_payload_read = 64 * 1024;

lock_failure_t
failure (int code) LUMEX_NOEXCEPT
{
  lock_failure_t result;
  result.system_code = code;
  // EWOULDBLOCK and EAGAIN are the same number on most systems: two case
  // labels would not compile, and GCC warns about `x == a || x == a`
  // (-Wlogical-op).
  if (code == EWOULDBLOCK
#if EAGAIN != EWOULDBLOCK
      || code == EAGAIN
#endif
  )
    result.kind = lock_error::busy;
  else if (code == EACCES || code == EPERM || code == EROFS)
    result.kind = lock_error::access_denied;
  else
    result.kind = lock_error::io_error;
  return result;
}

// Writes the payload from the start of the file and cuts the file after it.
// Returns 0 or the errno.
int
write_payload (int descriptor, std::string const &payload) LUMEX_NOEXCEPT
{
  std::size_t done = 0;
  while (done < payload.size ())
    {
      ssize_t const count
          = pwrite (descriptor, payload.data () + done, payload.size () - done,
                    static_cast<off_t> (done));
      if (count < 0)
        {
          if (errno == EINTR)
            continue;
          return errno;
        }
      done += static_cast<std::size_t> (count);
    }
  if (ftruncate (descriptor, static_cast<off_t> (payload.size ())) != 0)
    return errno;
  return 0;
}

// Closes a descriptor of read_payload on every way out.
class scoped_descriptor
{
public:
  explicit scoped_descriptor (int descriptor) LUMEX_NOEXCEPT
      : m_descriptor (descriptor)
  {
  }
  ~scoped_descriptor () { close (m_descriptor); }
  scoped_descriptor (scoped_descriptor const &) = delete;
  scoped_descriptor &operator= (scoped_descriptor const &) = delete;

private:
  int m_descriptor;
};
} // namespace

// What a lock owns. It is made before anything is opened and releases what it
// got, so no way out of try_acquire leaks the descriptor or the lock.
struct interprocess_lock::impl
{
  impl () = default;
  impl (impl const &) = delete;
  impl &operator= (impl const &) = delete;

  ~impl ()
  {
    if (descriptor < 0)
      return;
    // Closing the last descriptor of the open file releases the lock as well;
    // unlocking first says so explicitly.
    if (locked)
      flock (descriptor, LOCK_UN);
    close (descriptor);
  }

  int descriptor = -1;
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
        return acquire_result_t (unexpect, failure (created.error_code ()));
    }

  std::unique_ptr<impl> state (new impl ());
  // 0666 before the umask, like any file a program creates: every user who
  // runs the program must be able to take the same lock.
  state->descriptor = open (file.c_str (), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
  if (state->descriptor < 0)
    return acquire_result_t (unexpect, failure (errno));

  // flock, not fcntl: a fcntl record lock belongs to the process and is
  // released when the process closes any descriptor of the file.
  int result = 0;
  do
    result = flock (state->descriptor, LOCK_EX | LOCK_NB);
  while (result != 0 && errno == EINTR);
  if (result != 0)
    return acquire_result_t (unexpect, failure (errno));
  state->locked = true;

  int const written = write_payload (state->descriptor, payload);
  if (written != 0)
    return acquire_result_t (unexpect, failure (written));

  return acquire_result_t (interprocess_lock (std::move (state)));
}

interprocess_lock::payload_result_t
interprocess_lock::read_payload (path_t const &file)
{
  using lumex::core::expected::result::unexpect;

  int const descriptor = open (file.c_str (), O_RDONLY | O_CLOEXEC);
  if (descriptor < 0)
    return payload_result_t (unexpect, failure (errno));
  scoped_descriptor const closer (descriptor);

  std::string text;
  std::array<char, 4096> buffer;
  while (text.size () < max_payload_read)
    {
      ssize_t const count = read (descriptor, buffer.data (), buffer.size ());
      if (count < 0)
        {
          if (errno == EINTR)
            continue;
          return payload_result_t (unexpect, failure (errno));
        }
      if (count == 0)
        break;
      text.append (buffer.data (), static_cast<std::size_t> (count));
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

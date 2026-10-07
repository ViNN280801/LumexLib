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

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#if defined(__MINGW32__)
#include <pthread.h>
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp"
#include "lumex/applied/serial/resolver/LumexPortProcessResolver.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace serial
{
namespace probe
{
namespace detail
{
namespace
{
/**
 * @brief Timeout text shared by both platform branches.
 * @param deadline Budget that expired.
 */
std::string
open_timed_out_text (std::chrono::milliseconds deadline)
{
  return "open timed out after " + std::to_string (deadline.count ()) + " ms";
}

#if defined(LUMEX_OS_WINDOWS)
/**
 * @brief Grace period after `CancelSynchronousIo` before the worker is
 * abandoned.
 */
std::chrono::milliseconds const KCANCEL_GRACE_MS (50);

/**
 * @brief The Win32 handle of a running thread.
 * @param worker The thread.
 * @details With the MSVC library `native_handle ()` is the handle itself; with
 * winpthreads (MinGW) it is a `pthread_t` that `pthread_gethandle` turns into
 * one.
 */
HANDLE
worker_win32_handle (std::thread &worker)
{
#if defined(__MINGW32__)
  return static_cast<HANDLE> (pthread_gethandle (worker.native_handle ()));
#else
  return static_cast<HANDLE> (worker.native_handle ());
#endif
}

/**
 * @brief State shared between the caller and the open worker.
 *
 * @details The worker owns a `shared_ptr` copy, so an abandoned worker can
 * finish safely after the caller has returned.
 */
struct shared_open_state_t
{
  std::mutex mutex;
  std::condition_variable ready;
  bool done = false;      ///< The worker stored its outcome.
  bool abandoned = false; ///< The caller stopped waiting; the worker closes
                          ///< a late handle itself.
  native_serial_handle_t handle = invalid_native_serial_handle ();
  std::uint32_t error_code = 0U;
};

/**
 * @brief Worker body: one blocking `CreateFileA`.
 * @param state Shared state to fill; owned through the `shared_ptr`.
 * @param open_path Path passed to `CreateFileA`.
 * @param open_flags `dwFlagsAndAttributes` for `CreateFileA`.
 */
void
open_port_worker (std::shared_ptr<shared_open_state_t> state,
                  std::string open_path, std::uint32_t open_flags)
{
  HANDLE const handle = ::CreateFileA (
      open_path.c_str (), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
      OPEN_EXISTING, static_cast<DWORD> (open_flags), nullptr);

  std::unique_lock<std::mutex> lock (state->mutex);
  if (state->abandoned)
    {
      lock.unlock ();
      // The caller gave up; the late handle belongs to this worker.
      if (handle != INVALID_HANDLE_VALUE)
        ::CloseHandle (handle);
      return;
    }
  state->handle = handle;
  state->error_code
      = (handle == INVALID_HANDLE_VALUE) ? ::GetLastError () : 0U;
  state->done = true;
  lock.unlock ();
  state->ready.notify_all ();
}
#endif
} // namespace

open_result_t
bounded_open_serial_port (std::string const &open_path,
                          std::uint32_t platform_open_flags,
                          std::chrono::milliseconds deadline)
{
  open_result_t result;

#if defined(LUMEX_OS_WINDOWS)
  if (deadline <= std::chrono::milliseconds (0))
    {
      result.outcome = open_outcome::timed_out;
      result.system_error = open_timed_out_text (deadline);
      return result;
    }

  std::shared_ptr<shared_open_state_t> const state
      = std::make_shared<shared_open_state_t> ();

  std::thread worker (open_port_worker, state, open_path, platform_open_flags);

  bool completed = false;
  {
    std::unique_lock<std::mutex> lock (state->mutex);
    completed = state->ready.wait_for (lock, deadline,
                                       [&state] () { return state->done; });
  }

  if (completed)
    {
      worker.join ();
      if (state->handle == invalid_native_serial_handle ())
        {
          result.outcome = open_outcome::failed;
          result.error_code = static_cast<int> (state->error_code);
          result.system_error
              = resolver::port_process_resolver::format_system_error (
                  static_cast<int> (state->error_code));
        }
      else
        {
          result.outcome = open_outcome::opened;
          result.handle = state->handle;
        }
      return result;
    }

  // The deadline expired inside a blocking driver call; ask Windows to
  // cancel it, then allow a short grace period for the worker to unwind.
  ::CancelSynchronousIo (worker_win32_handle (worker));

  {
    std::unique_lock<std::mutex> lock (state->mutex);
    completed = state->ready.wait_for (lock, KCANCEL_GRACE_MS,
                                       [&state] () { return state->done; });
    if (completed)
      {
        native_serial_handle_t const late_handle = state->handle;
        lock.unlock ();
        worker.join ();
        if (late_handle != invalid_native_serial_handle ())
          ::CloseHandle (static_cast<HANDLE> (late_handle));
        result.outcome = open_outcome::timed_out;
        result.system_error = open_timed_out_text (deadline);
        return result;
      }
    // The driver did not cancel, so the worker stays in the kernel until
    // the Bluetooth stack gives up. It owns its state through the
    // `shared_ptr` and closes a late handle itself.
    state->abandoned = true;
  }
  worker.detach ();

  result.outcome = open_outcome::timed_out;
  result.system_error = open_timed_out_text (deadline);
  return result;
#else
  static_cast<void> (platform_open_flags);

  if (deadline <= std::chrono::milliseconds (0))
    {
      result.outcome = open_outcome::timed_out;
      result.system_error = open_timed_out_text (deadline);
      return result;
    }

  // `O_NONBLOCK` never blocks on tty devices, so no worker is needed.
  int const fd = ::open (open_path.c_str (), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0)
    {
      result.outcome = open_outcome::failed;
      result.error_code = errno;
      result.system_error
          = resolver::port_process_resolver::format_system_error (errno);
      return result;
    }

  result.outcome = open_outcome::opened;
  result.handle = fd;
  return result;
#endif
}

void
close_serial_handle (native_serial_handle_t handle) LUMEX_NOEXCEPT
{
#if defined(LUMEX_OS_WINDOWS)
  if (handle != invalid_native_serial_handle ())
    ::CloseHandle (static_cast<HANDLE> (handle));
#else
  if (handle >= 0)
    ::close (handle);
#endif
}

enumeration::serial_port_state
state_from_open_result (open_result_t const &result) LUMEX_NOEXCEPT
{
  switch (result.outcome)
    {
    case open_outcome::opened:
      return enumeration::serial_port_state::available;
    case open_outcome::timed_out:
      return enumeration::serial_port_state::unresponsive;
    case open_outcome::failed:
    default:
      break;
    }

#if defined(LUMEX_OS_WINDOWS)
  if (result.error_code == ERROR_ACCESS_DENIED
      || result.error_code == ERROR_SHARING_VIOLATION)
    return enumeration::serial_port_state::busy;
#else
  if (result.error_code == EBUSY || result.error_code == EACCES)
    return enumeration::serial_port_state::busy;
#endif

  return enumeration::serial_port_state::free;
}

} // namespace detail
} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

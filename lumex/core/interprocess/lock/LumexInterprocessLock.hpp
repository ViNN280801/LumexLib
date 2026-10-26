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
 * @file LumexInterprocessLock.hpp
 * @brief `interprocess_lock`: an exclusive lock of a file that another
 * process (or another lock object of this process) cannot take while it is
 * held, released by the operating system when its process ends in any way.
 * @details One lock file per resource (an instrument, a port): the name of the
 * file is the key. `try_acquire` never waits: it takes the lock or says why
 * not, and when the lock is taken it writes a short payload into the file
 * (who holds it), which `read_payload` reads without taking the lock.
 *
 * - Windows: `CreateFileW` (sharing read and write) and `LockFileEx` with
 *   `LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY` on one byte far
 *   behind the data, so that the payload stays readable through another
 *   handle (a locked range cannot be read). The system releases the lock of a
 *   process that ended abnormally soon, but not always at once: a caller that
 *   starts right after such an end may retry a few times.
 * - POSIX: `open` and `flock (LOCK_EX | LOCK_NB)`. `flock` belongs to the open
 *   file, not to the process, so closing another descriptor of the same file
 *   in this process does not release it (a `fcntl` record lock would be
 *   released; that is why it is not used).
 *
 * The file is never deleted by the class: deleting a file another process has
 * open would let two processes lock two different files of the same name. A
 * program can still unlink it while it is held on POSIX (another process then
 * creates and locks a new file of the same name); on Windows the handle does
 * not share deletion, so the system refuses. The lock is advisory: it
 * protects against programs that take the same lock, not against any program
 * that opens the file. On POSIX the mode of the new file follows the umask
 * (0666 before it), so users of one lock need a shared directory and a group
 * or a umask that let them write the file. `flock` over NFS is emulated with
 * `fcntl` record locks by the Linux client, so there the lock is released when
 * the process closes any descriptor of the file, the caveat of `fcntl` that
 * `flock` avoids on a local file system.
 *
 * An object is not shared between threads; the class itself keeps no global
 * state.
 *
 * @code
 * namespace lk = lumex::core::interprocess::lock;
 * auto lock = lk::interprocess_lock::try_acquire (directory / "device_a.lock",
 *                                                 "{\"pid\":1234}");
 * if (!lock && lock.error ().kind == lk::lock_error::busy)
 *   {
 *     auto const owner = lk::interprocess_lock::read_payload (
 *         directory / "device_a.lock");
 *     // owner.value () names the process that holds it
 *   }
 * @endcode
 */
#ifndef LUMEX_CORE_INTERPROCESS_LOCK_HPP
#define LUMEX_CORE_INTERPROCESS_LOCK_HPP

#include "lumex/LumexExport.hpp"

#include <memory>
#include <string>

#include "lumex/core/expected/Expected"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/interprocess/lock/LumexInterprocessLockError.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace interprocess
{
namespace lock
{
/**
 * @brief An exclusive inter-process lock of a file (RAII, move-only).
 * @details An object exists only while it holds a lock: `try_acquire` is the
 * only way to make one. `release ()` or the destructor unlock and close the
 * file; a moved-from object holds nothing.
 */
class LUMEX_INTERPROCESS_API interprocess_lock
{
public:
  /// @brief The path type: UTF-8 on every system (converted to UTF-16 on
  /// Windows, where a path that is not valid UTF-8 is refused with
  /// `io_error` and `ERROR_INVALID_NAME`; POSIX takes the bytes as they are);
  /// converts from `std::filesystem::path` from C++17.
  using path_t = lumex::core::filesystem::fs::path;
  /// @brief The result of `try_acquire`: the lock, or why there is none.
  using acquire_result_t
      = lumex::core::expected::result::expected<interprocess_lock,
                                                lock_failure_t>;
  /// @brief The result of `read_payload`: the text, or why there is none.
  using payload_result_t
      = lumex::core::expected::result::expected<std::string, lock_failure_t>;

  /**
   * @brief Takes the lock of `file` without waiting.
   * @details Creates the directories of `file` and the file itself when they
   * do not exist. When the lock is taken, the file is cut to `payload`.
   * @param file The lock file.
   * @param payload What the holder writes into the file (for example its
   * process ID and a time as JSON); empty leaves the file empty.
   * @return The lock, or `lock_error::busy` when another open file holds it,
   * `access_denied` or `io_error` when the file cannot be used (a directory in
   * place of the file is `io_error` on both systems).
   */
  static acquire_result_t try_acquire (path_t const &file,
                                       std::string const &payload
                                       = std::string ());

  /**
   * @brief Reads the payload of `file` without taking the lock.
   * @details Works whether the lock is held or not. The text may be cut short
   * when the holder writes it at that moment: treat it as a hint for a
   * message, not as data. At most the first 64 KiB are read: any program may
   * write into the file.
   * @param file The lock file.
   * @return The text, or `io_error` when there is no such file.
   */
  static payload_result_t read_payload (path_t const &file);

  interprocess_lock (interprocess_lock &&other) LUMEX_NOEXCEPT;
  interprocess_lock &operator= (interprocess_lock &&other) LUMEX_NOEXCEPT;
  interprocess_lock (interprocess_lock const &) = delete;
  interprocess_lock &operator= (interprocess_lock const &) = delete;

  /// @brief Releases the lock (as `release ()`).
  ~interprocess_lock ();

  /**
   * @brief Unlocks and closes the file now; the file stays on disk.
   * @details Does nothing when the object holds no lock (moved from, or
   * released before).
   */
  void release () LUMEX_NOEXCEPT;

  /// @brief Whether the object still holds its lock.
  bool
  owns_lock () const LUMEX_NOEXCEPT
  {
    return static_cast<bool> (m_impl);
  }

private:
  struct impl;

  explicit interprocess_lock (std::unique_ptr<impl> state) LUMEX_NOEXCEPT;

  // C4251: the class is exported (LUMEX_INTERPROCESS_API) and its private
  // member is a std::unique_ptr, which has no DLL interface of its own. The
  // pointer is used by the library alone (the type it points to is defined in
  // the library's sources), so a client never touches it across the boundary.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::unique_ptr<impl> m_impl;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};
} // namespace lock
} // namespace interprocess
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_INTERPROCESS_LOCK_HPP

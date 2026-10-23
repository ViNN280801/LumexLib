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
 * @file LumexHidRuntime.hpp
 * @brief The threads of the HID transport: the open connection that a
 * reader thread serves and the module-wide runtime core (reader threads,
 * the blocking executor, the housekeeping thread that joins the readers).
 * @details Not a public API (see `hid/LumexHidDevice.hpp` for the rules).
 * The core is created on first use and never destroyed: a thread that
 * still runs at process exit never touches a destroyed object. A static
 * sentinel sets the stop flags of the remaining readers at static
 * destruction (`abandon`).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HID_RUNTIME_HPP
#define LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HID_RUNTIME_HPP

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace hid
{
namespace detail
{
/// @brief An open hidapi handle with its stop flag, shared by the device,
/// its reader thread and its writes.
class hid_connection
{
public:
  hid_connection (std::unique_ptr<hidapi_handle> handle, bool numbered,
                  std::size_t input_buffer_size, int slice_milliseconds);

  hid_connection (hid_connection const &) = delete;
  hid_connection &operator= (hid_connection const &) = delete;

  /// @brief Asks the reader to end (any thread).
  void
  request_stop () LUMEX_NOEXCEPT
  {
    stop_.store (true, std::memory_order_release);
  }

  bool
  stop_requested () const LUMEX_NOEXCEPT
  {
    return stop_.load (std::memory_order_acquire);
  }

  /// @brief The stop at process exit: the handle is left as it is, no
  /// hidapi call follows.
  void
  abandon () LUMEX_NOEXCEPT
  {
    abandoned_.store (true, std::memory_order_release);
    request_stop ();
  }

  bool
  abandoned () const LUMEX_NOEXCEPT
  {
    return abandoned_.load (std::memory_order_acquire);
  }

  bool
  numbered () const LUMEX_NOEXCEPT
  {
    return numbered_;
  }

  std::size_t
  input_buffer_size () const LUMEX_NOEXCEPT
  {
    return input_buffer_size_;
  }

  int
  slice_milliseconds () const LUMEX_NOEXCEPT
  {
    return slice_milliseconds_;
  }

  /// @brief Lets the reader start reading (the device's strand, once the
  /// connection is the device's).
  void
  release_reader () LUMEX_NOEXCEPT
  {
    released_.store (true, std::memory_order_release);
  }

  /// @brief Waits for `release_reader` (true) or the stop flag (false); the
  /// reader thread only, before its first read.
  bool wait_released () const;

  /// @brief One `hid_read_timeout` slice (the reader thread only).
  int read (std::uint8_t *data, std::size_t size, std::error_code &error);
  /// @brief hidapi's text of the last failure (the reader thread only).
  std::string read_error_text ();

  /// @brief Writes one report unless the connection was stopped or closed
  /// (`transport_errc::closed` then); `text` gets hidapi's error text.
  std::error_code write (std::vector<std::uint8_t> const &report,
                         std::string &text);

  /// @brief Closes the handle once, waiting for a write in progress (the
  /// reader thread, after its last read); nothing after `abandon`.
  void close_handle ();

private:
  std::unique_ptr<hidapi_handle> handle_;
  bool const numbered_;
  std::size_t const input_buffer_size_;
  int const slice_milliseconds_;
  std::atomic<bool> stop_;
  std::atomic<bool> abandoned_;
  std::atomic<bool> released_;
  // Guards handle_closed_ and every call on the handle but the reads, which
  // only the reader thread makes, before it closes.
  std::mutex handle_mutex_;
  bool handle_closed_;
};

/// @brief The module-wide runtime (see the file description).
class hid_runtime_core
{
public:
  typedef std::function<void ()> task_t;

  /// @brief The one instance (created on first use, never destroyed).
  static hid_runtime_core &instance ();

  hid_runtime_core ();
  hid_runtime_core (hid_runtime_core const &) = delete;
  hid_runtime_core &operator= (hid_runtime_core const &) = delete;

  /// @brief Runs `task` on the blocking executor.
  void post (task_t task);

  /// @brief Starts the reader thread of `connection` running `body`; false
  /// (and no thread) while `shutdown` or `abandon` stops the runtime.
  bool start_reader (std::shared_ptr<hid_connection> const &connection,
                     task_t body);

  /// @brief The reference an open device holds; `release` returns it.
  std::size_t acquire (std::weak_ptr<hid_connection> const &connection,
                       std::weak_ptr<datagram::message_impl> const &device);
  void release (std::size_t reference) LUMEX_NOEXCEPT;

  bool shutdown (deadline_t deadline);
  void abandon () LUMEX_NOEXCEPT;
  hid_runtime_stats_t stats ();

private:
  struct device_entry
  {
    std::weak_ptr<hid_connection> connection;
    std::weak_ptr<datagram::message_impl> device;
  };
  struct reader_entry
  {
    std::thread thread;
    std::weak_ptr<hid_connection> connection;
  };

  void run_reader (task_t const &body);
  void housekeeping_main ();
  void worker_main ();
  bool on_runtime_thread () const;
  bool idle () const;

  mutable std::mutex mutex_;
  // Serializes shutdown calls (taken before mutex_, never under it).
  std::timed_mutex shutdown_mutex_;
  // Signals the workers (tasks_, quit_), the housekeeping thread
  // (finished_, quit_) and shutdown (the counts) in one variable each.
  std::condition_variable tasks_changed_;
  std::condition_variable finished_changed_;
  std::condition_variable counts_changed_;
  std::map<std::thread::id, reader_entry> readers_;
  std::vector<std::thread> finished_;
  std::size_t live_readers_;
  std::map<std::size_t, device_entry> devices_;
  std::size_t next_reference_;
  std::deque<task_t> tasks_;
  std::vector<std::thread> workers_;
  std::size_t idle_workers_;
  std::size_t running_tasks_;
  std::thread housekeeping_;
  bool quit_;
  bool accepting_;
};

/// @brief A reference of `hid_runtime_core` (move-only; released on
/// destruction).
class runtime_reference
{
public:
  runtime_reference () LUMEX_NOEXCEPT : reference_ (0) {}
  ~runtime_reference () { reset (); }

  runtime_reference (runtime_reference const &) = delete;
  runtime_reference &operator= (runtime_reference const &) = delete;

  void
  assign (std::size_t reference) LUMEX_NOEXCEPT
  {
    reset ();
    reference_ = reference;
  }

  void
  reset () LUMEX_NOEXCEPT
  {
    if (reference_ != 0)
      hid_runtime_core::instance ().release (reference_);
    reference_ = 0;
  }

private:
  std::size_t reference_;
};
} // namespace detail
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HID_RUNTIME_HPP

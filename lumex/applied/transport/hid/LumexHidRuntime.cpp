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

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidRuntime.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
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
namespace
{
// The threads of the blocking executor: opens and writes of every device,
// one write of a device at a time.
std::size_t const KMAX_WORKERS = 2;

// Sets the stop flags of the readers left at static destruction (see the
// file description of LumexHidRuntime.hpp); joins nothing.
class exit_sentinel
{
public:
  explicit exit_sentinel (hid_runtime_core &core) : core_ (core) {}
  ~exit_sentinel () { core_.abandon (); }

  exit_sentinel (exit_sentinel const &) = delete;
  exit_sentinel &operator= (exit_sentinel const &) = delete;

private:
  hid_runtime_core &core_;
};
} // namespace

hidapi_handle::~hidapi_handle () {}

hidapi_seam::~hidapi_seam () {}

hid_connection::hid_connection (std::unique_ptr<hidapi_handle> handle,
                                bool numbered, std::size_t input_buffer_size,
                                int slice_milliseconds)
    : handle_ (std::move (handle)), numbered_ (numbered),
      input_buffer_size_ (input_buffer_size),
      slice_milliseconds_ (slice_milliseconds), stop_ (false),
      abandoned_ (false), released_ (false), handle_closed_ (false)
{
}

bool
hid_connection::wait_released () const
{
  // A short poll: the wait lasts from the open's task to the strand's
  // finish_open, and the stop flag has no condition variable.
  while (!released_.load (std::memory_order_acquire))
    {
      if (stop_requested ())
        return false;
      std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
  return !stop_requested ();
}

int
hid_connection::read (std::uint8_t *data, std::size_t size,
                      std::error_code &error)
{
  return handle_->read_timeout (data, size, slice_milliseconds_, error);
}

std::string
hid_connection::read_error_text ()
{
  return handle_->last_error_text ();
}

std::error_code
hid_connection::write (std::vector<std::uint8_t> const &report,
                       std::string &text)
{
  std::lock_guard<std::mutex> const lock (handle_mutex_);
  if (handle_closed_ || stop_requested ())
    return make_error_code (error::transport_errc::closed);
  std::error_code error;
  int const written = handle_->write (report.data (), report.size (), error);
  if (written < 0)
    {
      text = handle_->last_error_text ();
      if (!error)
        error = std::make_error_code (std::errc::io_error);
      return error;
    }
  if (static_cast<std::size_t> (written) < report.size ())
    {
      text = "short write: " + std::to_string (written) + " of "
             + std::to_string (report.size ()) + " B";
      return std::make_error_code (std::errc::io_error);
    }
  return std::error_code ();
}

void
hid_connection::close_handle ()
{
  if (abandoned ())
    return;
  std::lock_guard<std::mutex> const lock (handle_mutex_);
  if (handle_closed_)
    return;
  handle_closed_ = true;
  handle_->close ();
}

hid_runtime_core &
hid_runtime_core::instance ()
{
  // Never destroyed: a thread that still runs at exit finds it intact.
  static hid_runtime_core *const core = new hid_runtime_core ();
  static exit_sentinel const sentinel (*core);
  return *core;
}

hid_runtime_core::hid_runtime_core ()
    : live_readers_ (0), next_reference_ (1), idle_workers_ (0),
      running_tasks_ (0), quit_ (false), accepting_ (true)
{
}

void
hid_runtime_core::post (task_t task)
{
  std::lock_guard<std::mutex> const lock (mutex_);
  tasks_.push_back (std::move (task));
  // A task that no idle worker will take starts a worker (an idle worker
  // that was woken but has not taken its task yet still counts as idle:
  // comparing with the queue keeps a second task from waiting behind the
  // first one's worker). No worker starts while shutdown lets the old ones
  // quit: it would end at once and stay counted; shutdown starts one for the
  // queue afterwards.
  if (!quit_ && tasks_.size () > idle_workers_
      && workers_.size () < KMAX_WORKERS)
    {
      try
        {
          start_worker ();
        }
      catch (...)
        {
          // Without a worker the task would stay queued for a caller that
          // saw the failure and a later worker would run it. Another
          // worker, if any, would still take it, and then the post stands.
          if (workers_.empty ())
            {
              tasks_.pop_back ();
              throw;
            }
        }
    }
  tasks_changed_.notify_one ();
}

void
hid_runtime_core::start_worker ()
{
  if (failing_thread_starts_ > 0)
    {
      --failing_thread_starts_;
      throw std::system_error (
          std::make_error_code (std::errc::resource_unavailable_try_again));
    }
  // The slot first, so that a thread that started is never lost to a
  // failing push_back (a joinable std::thread must not be destroyed).
  workers_.emplace_back ();
  try
    {
      workers_.back () = std::thread (&hid_runtime_core::worker_main, this);
    }
  catch (...)
    {
      workers_.pop_back ();
      throw;
    }
}

bool
hid_runtime_core::start_reader (
    std::shared_ptr<hid_connection> const &connection, task_t body)
{
  std::lock_guard<std::mutex> const lock (mutex_);
  if (!accepting_)
    return false;
  if (!housekeeping_.joinable ())
    housekeeping_ = std::thread (&hid_runtime_core::housekeeping_main, this);
  // The new thread registers its end under the same mutex: it cannot find
  // itself missing from readers_.
  reader_entry reader;
  reader.thread = std::thread (&hid_runtime_core::run_reader, this, body);
  reader.connection = connection;
  std::thread::id const id = reader.thread.get_id ();
  readers_[id] = std::move (reader);
  ++live_readers_;
  return true;
}

void
hid_runtime_core::run_reader (task_t const &body)
{
  try
    {
      body ();
    }
  catch (...)
    {
      // The body catches what it can; nothing may escape a thread.
    }
  try
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      std::map<std::thread::id, reader_entry>::iterator const it
          = readers_.find (std::this_thread::get_id ());
      if (it != readers_.end ())
        {
          // Never joined by itself: the housekeeping thread joins it.
          finished_.push_back (std::move (it->second.thread));
          readers_.erase (it);
          finished_changed_.notify_one ();
        }
    }
  catch (...)
    {
    }
}

void
hid_runtime_core::housekeeping_main ()
{
  std::unique_lock<std::mutex> lock (mutex_);
  for (;;)
    {
      while (finished_.empty () && !quit_)
        finished_changed_.wait (lock);
      if (finished_.empty ())
        return;
      std::vector<std::thread> batch;
      batch.swap (finished_);
      lock.unlock ();
      for (std::size_t i = 0; i < batch.size (); ++i)
        batch[i].join ();
      lock.lock ();
      live_readers_ -= batch.size ();
      counts_changed_.notify_all ();
    }
}

void
hid_runtime_core::worker_main ()
{
  std::unique_lock<std::mutex> lock (mutex_);
  for (;;)
    {
      ++idle_workers_;
      while (tasks_.empty () && !quit_)
        tasks_changed_.wait (lock);
      --idle_workers_;
      // After the quit a task is left in the queue (shutdown starts workers
      // for it once it joined): a task stuck in the driver would keep the
      // join past the deadline.
      if (tasks_.empty () || quit_)
        return;
      task_t task = std::move (tasks_.front ());
      tasks_.pop_front ();
      ++running_tasks_;
      lock.unlock ();
      try
        {
          task ();
        }
      catch (...)
        {
          // A task reports its own failures; nothing may escape a thread.
        }
      // The task may hold the last reference of a device, whose destructor
      // releases its runtime reference under the mutex.
      task = task_t ();
      lock.lock ();
      --running_tasks_;
      counts_changed_.notify_all ();
    }
}

std::size_t
hid_runtime_core::acquire (std::weak_ptr<hid_connection> const &connection,
                           std::weak_ptr<datagram::message_impl> const &device)
{
  std::lock_guard<std::mutex> const lock (mutex_);
  std::size_t const reference = next_reference_++;
  device_entry entry = { connection, device };
  devices_[reference] = entry;
  return reference;
}

void
hid_runtime_core::release (std::size_t reference) LUMEX_NOEXCEPT
{
  try
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      devices_.erase (reference);
      counts_changed_.notify_all ();
    }
  catch (...)
    {
    }
}

bool
hid_runtime_core::on_runtime_thread () const
{
  std::thread::id const self = std::this_thread::get_id ();
  if (housekeeping_.get_id () == self || readers_.count (self) != 0)
    return true;
  for (std::size_t i = 0; i < workers_.size (); ++i)
    if (workers_[i].get_id () == self)
      return true;
  return false;
}

bool
hid_runtime_core::idle () const
{
  return live_readers_ == 0 && tasks_.empty () && running_tasks_ == 0;
}

bool
hid_runtime_core::shutdown (deadline_t deadline)
{
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (on_runtime_thread ())
      return false;
  }
  // One shutdown at a time: a second call (an explicit shutdown next to the
  // owner object's destructor) waits for the first within its own deadline,
  // then finds the runtime idle. Without it, the second call could reset
  // quit_ before the first one's threads saw it, and the first would wait in
  // join forever.
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + deadline;
  std::unique_lock<std::timed_mutex> serial (shutdown_mutex_, std::defer_lock);
  if (!serial.try_lock_until (end))
    return false;
  std::vector<std::shared_ptr<hid_connection>> connections;
  std::vector<std::shared_ptr<datagram::message_impl>> devices;
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    accepting_ = false;
    for (std::map<std::thread::id, reader_entry>::const_iterator it
         = readers_.begin ();
         it != readers_.end (); ++it)
      if (std::shared_ptr<hid_connection> const connection
          = it->second.connection.lock ())
        connections.push_back (connection);
    for (std::map<std::size_t, device_entry>::const_iterator it
         = devices_.begin ();
         it != devices_.end (); ++it)
      if (std::shared_ptr<datagram::message_impl> const device
          = it->second.device.lock ())
        devices.push_back (device);
  }
  // The readers end within a slice whether or not the devices' executors
  // still run; the closes end the devices' pending operations when they do.
  for (std::size_t i = 0; i < connections.size (); ++i)
    connections[i]->request_stop ();
  for (std::size_t i = 0; i < devices.size (); ++i)
    {
      try
        {
          devices[i]->start_close ();
        }
      catch (...)
        {
        }
    }
  // Released outside the mutex: a device's destructor takes it.
  connections.clear ();
  devices.clear ();

  std::unique_lock<std::mutex> lock (mutex_);
  if (!counts_changed_.wait_until (lock, end, [this] () { return idle (); }))
    {
      accepting_ = true;
      return false;
    }
  quit_ = true;
  std::vector<std::thread> workers;
  workers.swap (workers_);
  std::thread housekeeping (std::move (housekeeping_));
  task_t hook;
  hook.swap (shutdown_hook_);
  lock.unlock ();
  // The hook of the tests runs while the workers still sleep: the tasks it
  // posts meet workers that were told to quit. Notifying without the lock
  // loses nothing: a thread that has not waited yet sees quit_ set.
  if (hook)
    hook ();
  tasks_changed_.notify_all ();
  finished_changed_.notify_all ();
  for (std::size_t i = 0; i < workers.size (); ++i)
    workers[i].join ();
  if (housekeeping.joinable ())
    housekeeping.join ();
  lock.lock ();
  quit_ = false;
  accepting_ = true;
  // The tasks posted while the threads were quitting started no worker and
  // the workers that quit left them: one worker per task, up to the limit.
  for (std::size_t i = 0;
       i < tasks_.size () && workers_.size () < KMAX_WORKERS; ++i)
    {
      try
        {
          start_worker ();
        }
      catch (...)
        {
          break;
        }
    }
  return true;
}

void
hid_runtime_core::fail_next_thread_starts (int count)
{
  std::lock_guard<std::mutex> const lock (mutex_);
  failing_thread_starts_ = count;
}

void
hid_runtime_core::set_shutdown_hook (task_t hook)
{
  std::lock_guard<std::mutex> const lock (mutex_);
  shutdown_hook_ = std::move (hook);
}

void
hid_runtime_core::abandon () LUMEX_NOEXCEPT
{
  try
    {
      // At exit the operating system may have ended a thread that held the
      // mutex (a DLL unloading at process exit): never wait for it.
      std::unique_lock<std::mutex> const lock (mutex_, std::try_to_lock);
      if (!lock.owns_lock ())
        return;
      accepting_ = false;
      for (std::map<std::thread::id, reader_entry>::const_iterator it
           = readers_.begin ();
           it != readers_.end (); ++it)
        if (std::shared_ptr<hid_connection> const connection
            = it->second.connection.lock ())
          connection->abandon ();
    }
  catch (...)
    {
    }
}

hid_runtime_stats_t
hid_runtime_core::stats ()
{
  std::lock_guard<std::mutex> const lock (mutex_);
  hid_runtime_stats_t result;
  result.open_devices = devices_.size ();
  result.reader_threads = live_readers_;
  result.worker_threads = workers_.size ();
  result.pending_tasks = tasks_.size () + running_tasks_;
  result.housekeeping = housekeeping_.joinable ();
  result.accepting = accepting_;
  return result;
}
} // namespace detail

hid_runtime::hid_runtime () { detail::hid_runtime_core::instance (); }

hid_runtime::~hid_runtime ()
{
  try
    {
      shutdown (default_shutdown_deadline ());
    }
  catch (...)
    {
    }
}

bool
hid_runtime::shutdown (deadline_t deadline)
{
  return detail::hid_runtime_core::instance ().shutdown (deadline);
}

hid_runtime_stats_t
hid_runtime::stats ()
{
  return detail::hid_runtime_core::instance ().stats ();
}
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

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

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexPermissionDiagnostics.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/LumexHidReportDescriptor.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidRuntime.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"

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
using datagram::message_handler_t;
using datagram::message_t;
using error::transport_errc;

char const KCOMPONENT[] = "transport.hid";
// The states of a write handed to the runtime's executor.
int const KWRITE_QUEUED = 0;
int const KWRITE_STARTED = 1;
int const KWRITE_CANCELLED = 2;
// The states of an open handed to the runtime's executor: KOPEN_CANCELLED
// when cancel () or close () took it back before its task started (the task
// then does nothing), KOPEN_ABANDONED when they ended it after the task
// started (the task goes on to the end, but nothing of its result counts).
int const KOPEN_QUEUED = 0;
int const KOPEN_STARTED = 1;
int const KOPEN_CANCELLED = 2;
int const KOPEN_ABANDONED = 3;
// hidapi's HID_API_MAX_REPORT_DESCRIPTOR_SIZE.
std::size_t const KMAX_DESCRIPTOR_SIZE = 4096;

std::string
hex4 (std::uint16_t value)
{
  char text[8];
  std::snprintf (text, sizeof text, "%04x", static_cast<unsigned> (value));
  return text;
}

std::string
instance_name (hid_options_t const &options)
{
  if (!options.name.empty ())
    return options.name;
  if (!options.path.empty ())
    return options.path;
  return "hid " + hex4 (options.vendor_id) + ":" + hex4 (options.product_id);
}

bool
matches (hid_device_info_t const &info, hid_filter_t const &filter)
{
  return (filter.vendor_id == 0 || info.vendor_id == filter.vendor_id)
         && (filter.product_id == 0 || info.product_id == filter.product_id)
         && (filter.serial_number.empty ()
             || info.serial_number == filter.serial_number)
         && (filter.path.empty () || info.path == filter.path);
}

int
slice_milliseconds (deadline_t slice)
{
  if (slice.count () < 1)
    return 1;
  if (slice.count () > INT_MAX)
    return INT_MAX;
  return static_cast<int> (slice.count ());
}

// The message of one report as hidapi hands it: the first byte is the report
// ID only when the device numbers its reports.
message_t
to_message (std::vector<std::uint8_t> const &buffer, std::size_t size,
            bool numbered)
{
  message_t message;
  std::size_t first = 0;
  if (numbered && size > 0)
    {
      message.report_id = buffer[0];
      first = 1;
    }
  message.payload.assign (
      buffer.begin () + static_cast<std::ptrdiff_t> (first),
      buffer.begin () + static_cast<std::ptrdiff_t> (size));
  return message;
}
} // namespace

class hid_device_impl;

/// What the threads of the HID runtime (the blocking executor and the
/// readers) share with their device. They may outlive the `io_context`, so
/// they hold neither the implementation nor a copy of its strand: the gate
/// pins the implementation for as long as work of the device runs on them
/// (an open, a write, the reader), and a thread posts its step only under
/// the gate's lock and only while the pin is set. `drop` clears the pin under
/// the same lock when the context is being destroyed, and Asio shuts the
/// context's services down after that, so no runtime thread touches the
/// strand or destroys the implementation (and its strand) after the context.
/// The pin is taken and released on the strand (or in `drop`), never on a
/// runtime thread: the thread that runs the last release is the one that
/// holds the step's own reference, so it is not the last owner.
class runtime_gate
{
public:
  runtime_gate () : activity_ (0) {}

  runtime_gate (runtime_gate const &) = delete;
  runtime_gate &operator= (runtime_gate const &) = delete;

  /// One more piece of work of `impl` (the strand).
  template <typename Impl>
  void
  pin (std::shared_ptr<Impl> const &impl)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (activity_++ == 0)
      impl_ = impl;
  }

  /// One piece of work ended (the strand): the last one lets the
  /// implementation go.
  void
  unpin () LUMEX_NOEXCEPT
  {
    std::shared_ptr<hid_device_impl> released;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (activity_ == 0)
        return;
      if (--activity_ == 0)
        released.swap (impl_);
    }
  }

  /// A runtime thread adds work that belongs to a piece it runs already
  /// (the reader of an open); false once the context is gone. It never
  /// becomes the last piece: the caller's own is still counted.
  bool
  hold () LUMEX_NOEXCEPT
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!impl_)
      return false;
    ++activity_;
    return true;
  }

  /// Takes back a `hold` that was not used; the caller's own piece is still
  /// counted, so the pin stays.
  void
  unhold () LUMEX_NOEXCEPT
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (activity_ > 1)
      --activity_;
  }

  /// The context is being destroyed.
  void
  drop () LUMEX_NOEXCEPT
  {
    std::shared_ptr<hid_device_impl> released;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      activity_ = 0;
      released.swap (impl_);
    }
  }

  /// Posts `step` to the device's strand with the pinned implementation in
  /// its `impl`; false (and the step dies here) when the pin is not set.
  template <typename Step>
  bool
  post (Step step)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!impl_)
      return false;
    step.impl = impl_;
    asio::post (step.impl->strand (), std::move (step));
    return true;
  }

private:
  std::mutex mutex_;
  std::size_t activity_;
  std::shared_ptr<hid_device_impl> impl_;
};

/// The implementation of a HID device: every member but layout_ is used on
/// the strand only (see the file description of LumexHidDevice.hpp).
class hid_device_impl final : public datagram::message_impl
{
public:
  hid_device_impl (executor_type const &executor,
                   std::shared_ptr<hidapi_seam> seam,
                   hid_options_t const &options)
      : message_impl (executor, instance_name (options)),
        seam_ (std::move (seam)), options_ (options),
        gate_ (std::make_shared<runtime_gate> ())
  {
  }

  ~hid_device_impl () override
  {
    // No join and no hidapi call: the reader ends within a slice and closes
    // the handle itself (this may run on that very thread).
    if (connection_)
      connection_->request_stop ();
  }

  report_layout_t
  layout () const
  {
    std::lock_guard<std::mutex> const lock (layout_mutex_);
    return layout_;
  }

  // The io_context is being destroyed (no thread runs it any more): the
  // runtime threads post no more, and the open, the receives and the queued
  // and the running write die uncalled.
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    gate_->drop ();
    status_handler_t const dropped_open (std::move (open_handler_));
    status_handler_t const dropped_parked (std::move (parked_handler_));
    parked_ = false;
    if (open_token_)
      {
        // The open's task must not touch hidapi after the context: one that
        // has not started is taken back, one that runs is abandoned (it
        // closes what it opened). No unpin: the drop above cleared the count.
        int queued = KOPEN_QUEUED;
        if (!open_token_->compare_exchange_strong (queued, KOPEN_CANCELLED,
                                                   std::memory_order_acq_rel))
          open_token_->store (KOPEN_ABANDONED, std::memory_order_release);
        open_token_.reset ();
      }
    status_handler_t const dropped_write (std::move (write_handler_));
    std::deque<message_handler_t> dropped_receives;
    dropped_receives.swap (receives_);
    std::deque<queued_send> dropped_sends;
    dropped_sends.swap (sends_);
  }

  std::shared_ptr<runtime_gate> const &
  gate () const LUMEX_NOEXCEPT
  {
    return gate_;
  }

  /// On the strand: a piece of work of the runtime's threads has ended.
  void
  work_ended () LUMEX_NOEXCEPT
  {
    gate_->unpin ();
  }

  std::size_t
  stale_events () const LUMEX_NOEXCEPT
  {
    return stale_events_.load (std::memory_order_acquire);
  }
  // The steps on the strand.
  void finish_open (std::shared_ptr<std::atomic<int>> const &token,
                    std::shared_ptr<hid_connection> const &connection,
                    report_layout_t const &layout, std::error_code error,
                    std::string const &text);
  void finish_write (std::error_code error, std::string const &text);
  void deliver (std::shared_ptr<hid_connection> const &connection,
                message_t message);
  void reader_failed (std::shared_ptr<hid_connection> const &connection,
                      std::string const &text);

protected:
  void do_open (status_handler_t handler) override;
  void do_receive (message_handler_t handler) override;
  void do_send (message_t message, status_handler_t handler) override;
  void do_cancel () override;
  void do_close () override;

private:
  struct queued_send
  {
    std::vector<std::uint8_t> report;
    status_handler_t handler;
  };

  std::shared_ptr<hid_device_impl>
  self ()
  {
    return std::static_pointer_cast<hid_device_impl> (shared_from_this ());
  }

  void start_next_write ();
  void end_open ();
  void start_open (status_handler_t handler);
  void take_back_queued_write ();
  void end_queued_sends (transport_errc reason);
  void end_pending_receives (std::error_code error);

  std::shared_ptr<hidapi_seam> const seam_;
  hid_options_t const options_;
  // Shared with the runtime threads; pinned while work of this device runs
  // on them.
  std::shared_ptr<runtime_gate> const gate_;
  status_handler_t open_handler_;
  // The state of the open in progress on the runtime's executor (see the
  // KOPEN_* states); empty while no open is in progress. A result that
  // carries another token is the late result of an open that cancel (),
  // close () or the deadline ended: it is dropped.
  std::shared_ptr<std::atomic<int>> open_token_;
  // An open ended while its task runs: the task is alive until its late
  // step reaches finish_open. The device has one open task at most, so an
  // open that comes meanwhile is parked (its handler waits here, no task, no
  // pin) and starts when that step arrives.
  bool abandoned_running_ = false;
  bool parked_ = false;
  status_handler_t parked_handler_;
  std::shared_ptr<hid_connection> connection_;
  runtime_reference reference_;
  std::deque<message_t> incoming_;
  std::deque<message_handler_t> receives_;
  std::error_code reader_error_;
  std::deque<queued_send> sends_;
  status_handler_t write_handler_;
  bool writing_ = false;
  // The state of the write on the runtime's executor: KWRITE_QUEUED until
  // its task starts (KWRITE_STARTED), KWRITE_CANCELLED when cancel () or
  // close () took it back before that.
  std::shared_ptr<std::atomic<int>> write_token_;
  // Reports and read failures of a closed connection that reached the
  // strand (see stale_events).
  std::atomic<std::size_t> stale_events_{ 0 };
  // Written on the strand; read on the strand and by opened_report_layout.
  mutable std::mutex layout_mutex_;
  report_layout_t layout_;
};

namespace
{
// Owns the connection of an open that has not reached the strand yet: a step
// that dies uncalled (the context is destroyed) tells the reader to stop, and
// the reader closes the handle.
class connection_guard
{
public:
  connection_guard () {}
  explicit connection_guard (std::shared_ptr<hid_connection> connection)
      : connection_ (std::move (connection))
  {
  }
  ~connection_guard ()
  {
    if (connection_)
      connection_->request_stop ();
  }

  connection_guard (connection_guard const &) = delete;
  connection_guard &operator= (connection_guard const &) = delete;
  connection_guard (connection_guard &&other) LUMEX_NOEXCEPT
      : connection_ (std::move (other.connection_))
  {
    other.connection_.reset ();
  }
  connection_guard &
  operator= (connection_guard &&other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      {
        if (connection_)
          connection_->request_stop ();
        connection_ = std::move (other.connection_);
        other.connection_.reset ();
      }
    return *this;
  }

  /// The connection, which is the device's from now on.
  std::shared_ptr<hid_connection>
  take () LUMEX_NOEXCEPT
  {
    std::shared_ptr<hid_connection> taken;
    taken.swap (connection_);
    return taken;
  }

private:
  std::shared_ptr<hid_connection> connection_;
};

// The tasks of the runtime's threads hold the gate and copies of what they
// need, never the implementation (see runtime_gate).
struct open_task
{
  std::shared_ptr<runtime_gate> gate;
  std::shared_ptr<hidapi_seam> seam;
  hid_options_t options;
  std::string name;
  std::shared_ptr<std::atomic<int>> token;
  bool abandoned () const;
  void operator() () const;

private:
  struct result_t;
  void run (result_t &result) const;
};

struct write_task
{
  std::shared_ptr<runtime_gate> gate;
  std::shared_ptr<hid_connection> connection;
  std::vector<std::uint8_t> report;
  std::shared_ptr<std::atomic<int>> token;
  void operator() () const;
};

// Ends the piece of work of a step when the step leaves, also by an
// exception.
class work_end
{
public:
  explicit work_end (hid_device_impl &impl) : impl_ (impl) {}
  ~work_end () { impl_.work_ended (); }

  work_end (work_end const &) = delete;
  work_end &operator= (work_end const &) = delete;

private:
  hid_device_impl &impl_;
};

// The steps carry the implementation the gate pins (set by runtime_gate::
// post) and end the piece of work they stand for.
struct finish_open_step
{
  std::shared_ptr<hid_device_impl> impl;
  std::shared_ptr<std::atomic<int>> token;
  connection_guard connection;
  report_layout_t layout;
  std::error_code error;
  std::string text;
  void
  operator() ()
  {
    // The piece of work ends even if the finish throws.
    work_end const ended (*impl);
    std::shared_ptr<hid_connection> const taken = connection.take ();
    impl->finish_open (token, taken, layout, error, text);
  }
};

struct finish_write_step
{
  std::shared_ptr<hid_device_impl> impl;
  std::error_code error;
  std::string text;
  void
  operator() ()
  {
    work_end const ended (*impl);
    impl->finish_write (error, text);
  }
};

struct deliver_step
{
  std::shared_ptr<hid_device_impl> impl;
  std::shared_ptr<hid_connection> connection;
  message_t message;
  void
  operator() ()
  {
    impl->deliver (connection, std::move (message));
  }
};

struct reader_failed_step
{
  std::shared_ptr<hid_device_impl> impl;
  std::shared_ptr<hid_connection> connection;
  std::string text;
  void
  operator() ()
  {
    impl->reader_failed (connection, text);
  }
};

// The reader's last word: the piece of work it stood for ends.
struct reader_ended_step
{
  std::shared_ptr<hid_device_impl> impl;
  void
  operator() ()
  {
    impl->work_ended ();
  }
};

// Posts reader_ended_step when the reader thread leaves its body.
class reader_end
{
public:
  explicit reader_end (std::shared_ptr<runtime_gate> const &gate)
      : gate_ (gate)
  {
  }
  ~reader_end ()
  {
    try
      {
        reader_ended_step step = { std::shared_ptr<hid_device_impl> () };
        gate_->post (std::move (step));
      }
    catch (...)
      {
      }
  }

  reader_end (reader_end const &) = delete;
  reader_end &operator= (reader_end const &) = delete;

private:
  std::shared_ptr<runtime_gate> const gate_;
};

// The reader thread: slices of hid_read_timeout until the stop flag; the
// flag is checked before and after every slice, so a report that arrives
// after the stop is dropped (a zombie read). The handle is closed here,
// after the last read. A report that finds no pin (the context is gone)
// ends the reader.
struct reader_body
{
  std::shared_ptr<hid_connection> connection;
  std::shared_ptr<runtime_gate> gate;

  void
  operator() () const
  {
    reader_end const ended (gate);
    // Released by finish_open once the connection is the device's.
    if (!connection->wait_released ())
      {
        connection->close_handle ();
        return;
      }
    std::vector<std::uint8_t> buffer (connection->input_buffer_size ());
    while (!connection->stop_requested ())
      {
        std::error_code error;
        int const read
            = connection->read (buffer.data (), buffer.size (), error);
        if (connection->stop_requested ())
          break;
        if (read < 0)
          {
            std::string const text = connection->read_error_text ();
            reader_failed_step step
                = { std::shared_ptr<hid_device_impl> (), connection,
                    text.empty () ? error.message () : text };
            gate->post (std::move (step));
            break;
          }
        if (read == 0)
          continue;
        deliver_step step
            = { std::shared_ptr<hid_device_impl> (), connection,
                to_message (buffer, static_cast<std::size_t> (read),
                            connection->numbered ()) };
        if (!gate->post (std::move (step)))
          break;
      }
    connection->close_handle ();
  }
};
} // namespace

void
write_task::operator() () const
{
  // Taken back by cancel () or close () while it waited for a thread: its
  // handler has run already.
  int queued = KWRITE_QUEUED;
  if (!token->compare_exchange_strong (queued, KWRITE_STARTED,
                                       std::memory_order_acq_rel))
    return;
  std::string text;
  std::error_code error;
  try
    {
      error = connection->write (report, text);
    }
  catch (std::system_error const &failure)
    {
      error = failure.code ();
      text = std::string ("the write failed: ") + failure.what ();
    }
  catch (std::bad_alloc const &)
    {
      error = std::make_error_code (std::errc::not_enough_memory);
      text = "the write failed: out of memory";
    }
  catch (...)
    {
      error = std::make_error_code (std::errc::io_error);
      text = "the write failed";
    }
  // The step is posted on every path: without it the write stays pending
  // and the device keeps its pin.
  try
    {
      finish_write_step step
          = { std::shared_ptr<hid_device_impl> (), error, text };
      gate->post (std::move (step));
    }
  catch (...)
    {
    }
}

bool
open_task::abandoned () const
{
  return token->load (std::memory_order_acquire) == KOPEN_ABANDONED;
}

// What the task found; the step carries it to the strand.
struct open_task::result_t
{
  std::shared_ptr<hid_connection> connection;
  report_layout_t layout;
  std::error_code error;
  std::string text;
};

// Closes a handle that the task still owns when it unwinds.
class handle_guard
{
public:
  explicit handle_guard (std::unique_ptr<hidapi_handle> &handle)
      : handle_ (handle)
  {
  }
  ~handle_guard ()
  {
    if (!handle_)
      return;
    try
      {
        handle_->close ();
      }
    catch (...)
      {
      }
  }

  handle_guard (handle_guard const &) = delete;
  handle_guard &operator= (handle_guard const &) = delete;

private:
  std::unique_ptr<hidapi_handle> &handle_;
};

void
open_task::operator() () const
{
  // Taken back by cancel () or close () while it waited for a thread: its
  // handler has run already, and the pin it stood for is let go.
  int queued = KOPEN_QUEUED;
  if (!token->compare_exchange_strong (queued, KOPEN_STARTED,
                                       std::memory_order_acq_rel))
    return;
  result_t result;
  try
    {
      run (result);
    }
  catch (std::system_error const &failure)
    {
      result.error = failure.code ();
      result.text = std::string ("the open failed: ") + failure.what ();
    }
  catch (std::bad_alloc const &)
    {
      result.error = std::make_error_code (std::errc::not_enough_memory);
      result.text = "the open failed: out of memory";
    }
  catch (...)
    {
      result.error = std::make_error_code (std::errc::io_error);
      result.text = "the open failed";
    }
  if (result.error && result.connection)
    {
      // Not handed on: a reader that started stops and closes the handle.
      result.connection->request_stop ();
      result.connection.reset ();
    }
  // The step is posted on every path: it ends the piece of work, and the
  // open that waits for an abandoned task starts when it arrives. Posted
  // under the gate's lock, and only while the context lives; the step owns
  // the connection (its reader stops when the step dies uncalled).
  try
    {
      finish_open_step step = { std::shared_ptr<hid_device_impl> (),
                                token,
                                connection_guard (result.connection),
                                result.layout,
                                result.error,
                                result.text };
      gate->post (std::move (step));
    }
  catch (...)
    {
      // Out of memory: the pin stays until the context goes; the connection
      // (if any) is stopped by its guard.
    }
}

void
open_task::run (result_t &result) const
{
  std::error_code &error = result.error;
  std::string &text = result.text;
  report_layout_t &layout = result.layout;
  std::shared_ptr<hid_connection> &connection = result.connection;
  std::string path = options.path;
  if (options.resolve_path)
    {
      try
        {
          path = options.resolve_path ();
          if (path.empty ())
            {
              error = make_error_code (transport_errc::invalid_endpoint);
              text = "no HID device matches " + name;
            }
        }
      catch (std::system_error const &failure)
        {
          error = failure.code ()
                      ? failure.code ()
                      : make_error_code (transport_errc::invalid_endpoint);
          text = std::string ("finding the device failed: ") + failure.what ();
        }
      catch (...)
        {
          error = make_error_code (transport_errc::invalid_endpoint);
          text = "finding the device failed";
        }
    }
  else if (path.empty ())
    {
      hid_filter_t filter;
      filter.vendor_id = options.vendor_id;
      filter.product_id = options.product_id;
      filter.serial_number = options.serial_number;
      std::vector<hid_device_info_t> const found
          = enumerate_hid_devices_with (*seam, filter);
      if (found.empty ())
        {
          error = std::make_error_code (std::errc::no_such_device);
          text = "no HID device matches " + hex4 (options.vendor_id) + ":"
                 + hex4 (options.product_id)
                 + (options.serial_number.empty ()
                        ? std::string ()
                        : " serial " + options.serial_number);
        }
      else
        path = found.front ().path;
    }

  std::unique_ptr<hidapi_handle> handle;
  handle_guard const guard (handle);
  // An open that was ended while it looked for the device does not open it.
  if (!error && abandoned ())
    error = make_error_code (transport_errc::cancelled);
  if (!error)
    {
      handle = seam->open_path (path, error);
      if (handle && abandoned ())
        {
          // The caller has its answer: the handle is closed here, no reader
          // reads from it and nothing is posted that changes the device.
          // Defence in depth: an abandoned open that got past this point
          // would start a reader that the stale step stops before it reads,
          // so no test can tell the two apart; this saves the thread.
          handle->close ();
          handle.reset ();
          error = make_error_code (transport_errc::cancelled);
        }
      else if (!handle)
        {
          if (!error)
            error = std::make_error_code (std::errc::no_such_device);
          if (error.category () == std::system_category ()
              && transport::detail::is_permission_error (error.value ()))
            {
              // Off the io thread: the user database lookups may block.
              text = transport::detail::describe_permission_denied (path);
              error = make_error_code (transport_errc::permission_denied);
            }
          else
            text = path + ": " + error.message ();
        }
    }

  if (handle)
    {
      std::vector<std::uint8_t> descriptor (KMAX_DESCRIPTOR_SIZE);
      std::error_code read_error;
      int const size = handle->report_descriptor (
          descriptor.data (), descriptor.size (), read_error);
      if (size <= 0)
        {
          error = make_error_code (transport_errc::bad_report_descriptor);
          text = "no report descriptor: " + handle->last_error_text ();
        }
      else
        {
          report_layout_result_t const analyzed
              = analyze_report_descriptor (const_bytes_t (
                  descriptor.data (), static_cast<std::size_t> (size)));
          if (analyzed.has_value ())
            layout = *analyzed;
          else
            {
              error = analyzed.error ();
              text = "report descriptor not understood ("
                     + std::to_string (size) + " B)";
            }
        }
      if (error)
        {
          handle->close ();
          handle.reset ();
        }
    }

  if (handle)
    {
      // One byte more than the largest input report: its ID when numbered.
      connection = std::make_shared<hid_connection> (
          std::move (handle), layout.numbered,
          std::max<std::size_t> (layout.max_input_size () + 1U, 2U),
          slice_milliseconds (options.read_slice));
      // The reader starts here, while this task counts for shutdown: an
      // open that finishes during shutdown gets no reader. It reads nothing
      // until finish_open releases it on the strand, so no report reaches
      // the strand before the connection is the device's. The reader is a
      // piece of work of the device: the gate counts it before the thread
      // starts, and refuses once the context is gone.
      bool started = false;
      bool held = gate->hold ();
      if (!held)
        error = make_error_code (transport_errc::closed);
      else
        {
          try
            {
              reader_body body = { connection, gate };
              started = hid_runtime_core::instance ().start_reader (connection,
                                                                    body);
              if (!started)
                error = make_error_code (transport_errc::closed);
            }
          catch (std::system_error const &failure)
            {
              error = failure.code ();
            }
          catch (...)
            {
              error = std::make_error_code (std::errc::not_enough_memory);
            }
          if (!started)
            gate->unhold ();
        }
      if (!started)
        {
          text = "no reader thread: " + error.message ();
          connection->request_stop ();
          connection->close_handle ();
          connection.reset ();
        }
    }
}

void
hid_device_impl::finish_open (
    std::shared_ptr<std::atomic<int>> const &token,
    std::shared_ptr<hid_connection> const &connection,
    report_layout_t const &layout, std::error_code error,
    std::string const &text)
{
  if (token != open_token_)
    {
      // The late result of an open that cancel (), close () or the deadline
      // ended: its handler has run, and a later open may be in progress.
      // The reader it started never gets its release: it stops and closes
      // the handle.
      if (connection)
        connection->request_stop ();
      // Only an abandoned open leaves such a step. The task is over: the
      // open that waited for it starts now.
      abandoned_running_ = false;
      if (parked_)
        {
          parked_ = false;
          start_open (std::move (parked_handler_));
          parked_handler_ = nullptr;
        }
      return;
    }
  open_token_.reset ();
  status_handler_t handler = std::move (open_handler_);
  if (error)
    {
      LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                           "open failed: " + text, log::log_bytes_t ());
      complete (std::move (handler), error);
      return;
    }
  if (connection->stop_requested ())
    {
      // shutdown stopped the reader while the open completed.
      LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                           "open failed: the HID runtime shut down",
                           log::log_bytes_t ());
      complete (std::move (handler), transport_errc::closed);
      return;
    }
  connection_ = connection;
  {
    std::lock_guard<std::mutex> const lock (layout_mutex_);
    layout_ = layout;
  }
  incoming_.clear ();
  reader_error_ = std::error_code ();
  reference_.assign (
      hid_runtime_core::instance ().acquire (connection, shared_from_this ()));
  set_open (true);
  connection->release_reader ();
  LUMEX_TRANSPORT_LOG (info, KCOMPONENT, name ().c_str (),
                       std::string (layout.numbered
                                        ? "opened (numbered reports)"
                                        : "opened (unnumbered reports)"),
                       log::log_bytes_t ());
  complete (std::move (handler), std::error_code ());
}

void
hid_device_impl::do_open (status_handler_t handler)
{
  if (is_open ())
    {
      complete (std::move (handler), std::error_code ());
      return;
    }
  if (open_token_ || parked_)
    {
      complete (std::move (handler), transport_errc::cancelled);
      return;
    }
  if (abandoned_running_)
    {
      parked_handler_ = std::move (handler);
      parked_ = true;
      return;
    }
  start_open (std::move (handler));
}

void
hid_device_impl::start_open (status_handler_t handler)
{
  std::shared_ptr<std::atomic<int>> token;
  try
    {
      token = std::make_shared<std::atomic<int>> (KOPEN_QUEUED);
      open_task task = { gate_, seam_, options_, name (), token };
      gate_->pin (self ());
      try
        {
          hid_runtime_core::instance ().post (task);
        }
      catch (...)
        {
          gate_->unpin ();
          throw;
        }
    }
  catch (...)
    {
      // The task may be queued still (the runtime takes it back when no
      // worker can start, but never trust that): it must find the open
      // ended and do nothing. The handler is called whatever failed: this
      // also runs when an abandoned task's step wakes a parked open.
      if (token)
        token->store (KOPEN_CANCELLED, std::memory_order_release);
      std::error_code failure
          = std::make_error_code (std::errc::not_enough_memory);
      try
        {
          throw;
        }
      catch (std::system_error const &error)
        {
          failure = error.code ();
        }
      catch (...)
        {
        }
      complete (std::move (handler), failure);
      return;
    }
  open_token_ = std::move (token);
  open_handler_ = std::move (handler);
}

void
hid_device_impl::do_receive (message_handler_t handler)
{
  if (!incoming_.empty ())
    {
      message_t message = std::move (incoming_.front ());
      incoming_.pop_front ();
      complete (std::move (handler), std::error_code (), std::move (message));
      return;
    }
  if (reader_error_)
    {
      complete (std::move (handler), reader_error_, message_t ());
      return;
    }
  receives_.push_back (std::move (handler));
}

void
hid_device_impl::do_send (message_t message, status_handler_t handler)
{
  std::uint8_t const id = message.report_id;
  // The analysis gives a device that numbers none output report 0 at most,
  // and one that numbers its reports no report 0.
  if (!layout_.has_output (id))
    {
      LUMEX_TRANSPORT_LOG (warning, KCOMPONENT, name ().c_str (),
                           "no output report " + std::to_string (id),
                           log::log_bytes_t ());
      complete (std::move (handler), transport_errc::unknown_report_id);
      return;
    }
  std::size_t const size = layout_.output_size (id);
  if (message.payload.size () > size)
    {
      complete (std::move (handler), transport_errc::report_too_large);
      return;
    }
  // [ID][payload][zero padding up to the output report]; hidapi takes the
  // ID byte also when it is 0 (a device that numbers none).
  queued_send send;
  send.report.assign (1U + size, 0);
  send.report[0] = id;
  std::copy (message.payload.begin (), message.payload.end (),
             send.report.begin () + 1);
  send.handler = std::move (handler);
  sends_.push_back (std::move (send));
  start_next_write ();
}

void
hid_device_impl::start_next_write ()
{
  if (writing_ || sends_.empty () || !connection_)
    return;
  writing_ = true;
  queued_send &next = sends_.front ();
  write_handler_ = std::move (next.handler);
  write_token_ = std::make_shared<std::atomic<int>> (KWRITE_QUEUED);
  write_task task
      = { gate_, connection_, std::move (next.report), write_token_ };
  sends_.pop_front ();
  LUMEX_TRANSPORT_LOG (
      trace, KCOMPONENT, name ().c_str (), "write report",
      log::log_bytes_t (task.report.data (), task.report.size ()));
  gate_->pin (self ());
  try
    {
      hid_runtime_core::instance ().post (std::move (task));
    }
  catch (...)
    {
      gate_->unpin ();
      throw;
    }
}

void
hid_device_impl::finish_write (std::error_code error, std::string const &text)
{
  writing_ = false;
  write_token_.reset ();
  if (error)
    LUMEX_TRANSPORT_LOG (
        error, KCOMPONENT, name ().c_str (),
        "write failed: " + error.message ()
            + (text.empty () ? std::string () : " (" + text + ")"),
        log::log_bytes_t ());
  complete (std::move (write_handler_), error);
  start_next_write ();
}

void
hid_device_impl::deliver (std::shared_ptr<hid_connection> const &connection,
                          message_t message)
{
  // A report of a connection closed since is dropped.
  if (connection != connection_)
    {
      stale_events_.fetch_add (1, std::memory_order_acq_rel);
      return;
    }
  LUMEX_TRANSPORT_LOG (
      trace, KCOMPONENT, name ().c_str (),
      "report " + std::to_string (message.report_id),
      log::log_bytes_t (message.payload.data (), message.payload.size ()));
  if (!receives_.empty ())
    {
      message_handler_t handler = std::move (receives_.front ());
      receives_.pop_front ();
      complete (std::move (handler), std::error_code (), std::move (message));
      return;
    }
  if (incoming_.size ()
      >= std::max<std::size_t> (options_.max_queued_reports, 1U))
    {
      incoming_.pop_front ();
      LUMEX_TRANSPORT_LOG (warning, KCOMPONENT, name ().c_str (),
                           "report queue full: the oldest report dropped",
                           log::log_bytes_t ());
    }
  incoming_.push_back (std::move (message));
}

void
hid_device_impl::reader_failed (
    std::shared_ptr<hid_connection> const &connection, std::string const &text)
{
  if (connection != connection_)
    {
      stale_events_.fetch_add (1, std::memory_order_acq_rel);
      return;
    }
  LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                       "read failed: " + text, log::log_bytes_t ());
  reader_error_ = make_error_code (transport_errc::disconnected);
  end_pending_receives (reader_error_);
}

void
hid_device_impl::end_queued_sends (transport_errc reason)
{
  while (!sends_.empty ())
    {
      status_handler_t handler = std::move (sends_.front ().handler);
      sends_.pop_front ();
      complete (std::move (handler), reason);
    }
}

void
hid_device_impl::end_pending_receives (std::error_code error)
{
  while (!receives_.empty ())
    {
      message_handler_t handler = std::move (receives_.front ());
      receives_.pop_front ();
      complete (std::move (handler), error, message_t ());
    }
}

void
hid_device_impl::take_back_queued_write ()
{
  // A write whose task has not started yet ends with cancelled; one that
  // reached hidapi ends with its own result.
  if (!writing_ || !write_token_)
    return;
  int queued = KWRITE_QUEUED;
  if (!write_token_->compare_exchange_strong (queued, KWRITE_CANCELLED,
                                              std::memory_order_acq_rel))
    return;
  writing_ = false;
  write_token_.reset ();
  // Its task finds the token taken and posts nothing: the piece of work
  // ends here.
  gate_->unpin ();
  complete (std::move (write_handler_), transport_errc::cancelled);
}

void
hid_device_impl::end_open ()
{
  if (parked_)
    {
      // It waits for an abandoned task and has none of its own.
      parked_ = false;
      status_handler_t parked (std::move (parked_handler_));
      parked_handler_ = nullptr;
      complete (std::move (parked), transport_errc::cancelled);
      return;
    }
  if (!open_token_)
    return;
  std::shared_ptr<std::atomic<int>> token;
  token.swap (open_token_);
  status_handler_t handler = std::move (open_handler_);
  int queued = KOPEN_QUEUED;
  if (token->compare_exchange_strong (queued, KOPEN_CANCELLED,
                                      std::memory_order_acq_rel))
    {
      // Its task has not started: it finds the token taken and posts
      // nothing, so the piece of work ends here.
      gate_->unpin ();
    }
  else
    {
      // The task is inside hidapi (or the finder of the device): it cannot
      // be stopped, so it is abandoned. It closes the handle it opened and
      // posts a result that finish_open drops; that step ends the piece of
      // work.
      token->store (KOPEN_ABANDONED, std::memory_order_release);
      abandoned_running_ = true;
      LUMEX_TRANSPORT_LOG (
          warning, KCOMPONENT, name ().c_str (),
          "open ended while its task runs or its result is on the way: "
          "abandoned",
          log::log_bytes_t ());
    }
  complete (std::move (handler), transport_errc::cancelled);
}

void
hid_device_impl::do_cancel ()
{
  end_open ();
  take_back_queued_write ();
  end_queued_sends (transport_errc::cancelled);
  end_pending_receives (make_error_code (transport_errc::cancelled));
}

void
hid_device_impl::do_close ()
{
  end_open ();
  take_back_queued_write ();
  end_queued_sends (transport_errc::cancelled);
  end_pending_receives (make_error_code (transport_errc::cancelled));
  if (connection_)
    {
      // The reader ends within a slice and closes the handle; a write in
      // progress finishes first (the handle mutex).
      connection_->request_stop ();
      connection_.reset ();
      LUMEX_TRANSPORT_LOG (info, KCOMPONENT, name ().c_str (), "closed",
                           log::log_bytes_t ());
    }
  reference_.reset ();
  incoming_.clear ();
  reader_error_ = std::error_code ();
  std::lock_guard<std::mutex> const lock (layout_mutex_);
  layout_ = report_layout_t ();
}

std::vector<hid_device_info_t>
enumerate_hid_devices_with (hidapi_seam &seam, hid_filter_t const &filter)
{
  std::vector<hid_device_info_t> const all
      = seam.enumerate (filter.vendor_id, filter.product_id);
  std::vector<hid_device_info_t> found;
  for (std::size_t i = 0; i < all.size (); ++i)
    if (matches (all[i], filter))
      found.push_back (all[i]);
  return found;
}

hid_device
make_hid_device_with (std::shared_ptr<hidapi_seam> seam,
                      executor_type const &executor,
                      hid_options_t const &options)
{
  return hid_device (
      std::make_shared<hid_device_impl> (executor, std::move (seam), options));
}
} // namespace detail

std::vector<hid_device_info_t>
enumerate_hid_devices (hid_filter_t const &filter)
{
  return detail::enumerate_hid_devices_with (*detail::system_hidapi (),
                                             filter);
}

hid_device
make_hid_device (executor_type const &executor, hid_options_t const &options)
{
  return detail::make_hid_device_with (detail::system_hidapi (), executor,
                                       options);
}

std::size_t
detail::stale_events (hid_device const &device)
{
  detail::hid_device_impl const *const impl
      = dynamic_cast<detail::hid_device_impl const *> (device.impl ().get ());
  return impl == nullptr ? 0 : impl->stale_events ();
}

report_layout_t
opened_report_layout (hid_device const &device)
{
  detail::hid_device_impl const *const impl
      = dynamic_cast<detail::hid_device_impl const *> (device.impl ().get ());
  if (impl == nullptr)
    return report_layout_t ();
  return impl->layout ();
}
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

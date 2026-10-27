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

/// The implementation of a HID device: every member but layout_ is used on
/// the strand only (see the file description of LumexHidDevice.hpp).
class hid_device_impl final : public datagram::message_impl
{
public:
  hid_device_impl (executor_type const &executor,
                   std::shared_ptr<hidapi_seam> seam,
                   hid_options_t const &options)
      : message_impl (executor, instance_name (options)),
        seam_ (std::move (seam)), options_ (options)
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

  // The blocking part of open, on the runtime's executor.
  void open_blocking ();
  // One write, on the runtime's executor.
  void write_blocking (std::shared_ptr<hid_connection> const &connection,
                       std::vector<std::uint8_t> const &report,
                       std::shared_ptr<std::atomic<int>> const &token);

  std::size_t
  stale_events () const LUMEX_NOEXCEPT
  {
    return stale_events_.load (std::memory_order_acquire);
  }
  // From the reader thread: one report, the end of the reads.
  void post_report (std::shared_ptr<hid_connection> const &connection,
                    message_t message);
  void post_reader_failure (std::shared_ptr<hid_connection> const &connection,
                            std::string const &text);

  // The steps on the strand.
  void finish_open (std::shared_ptr<hid_connection> const &connection,
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
  void take_back_queued_write ();
  void end_queued_sends (transport_errc reason);
  void end_pending_receives (std::error_code error);

  std::shared_ptr<hidapi_seam> const seam_;
  hid_options_t const options_;
  status_handler_t open_handler_;
  bool opening_ = false;
  bool close_requested_ = false;
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
struct open_task
{
  std::shared_ptr<hid_device_impl> impl;
  void
  operator() () const
  {
    impl->open_blocking ();
  }
};

struct write_task
{
  std::shared_ptr<hid_device_impl> impl;
  std::shared_ptr<hid_connection> connection;
  std::vector<std::uint8_t> report;
  std::shared_ptr<std::atomic<int>> token;
  void
  operator() () const
  {
    impl->write_blocking (connection, report, token);
  }
};

struct finish_open_step
{
  std::shared_ptr<hid_device_impl> impl;
  std::shared_ptr<hid_connection> connection;
  report_layout_t layout;
  std::error_code error;
  std::string text;
  void
  operator() ()
  {
    impl->finish_open (connection, layout, error, text);
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

// The reader thread: slices of hid_read_timeout until the stop flag; the
// flag is checked before and after every slice, so a report that arrives
// after the stop is dropped (DChannel's zombie read). The handle is closed
// here, after the last read.
struct reader_body
{
  std::shared_ptr<hid_connection> connection;
  std::weak_ptr<hid_device_impl> device;

  void
  operator() () const
  {
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
            if (std::shared_ptr<hid_device_impl> const owner = device.lock ())
              owner->post_reader_failure (
                  connection, text.empty () ? error.message () : text);
            break;
          }
        if (read == 0)
          continue;
        std::shared_ptr<hid_device_impl> const owner = device.lock ();
        if (!owner)
          break;
        owner->post_report (
            connection, to_message (buffer, static_cast<std::size_t> (read),
                                    connection->numbered ()));
      }
    connection->close_handle ();
  }
};
} // namespace

void
hid_device_impl::open_blocking ()
{
  std::error_code error;
  std::string text;
  std::string path = options_.path;
  if (options_.resolve_path)
    {
      try
        {
          path = options_.resolve_path ();
          if (path.empty ())
            {
              error = make_error_code (transport_errc::invalid_endpoint);
              text = "no HID device matches " + name ();
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
      filter.vendor_id = options_.vendor_id;
      filter.product_id = options_.product_id;
      filter.serial_number = options_.serial_number;
      std::vector<hid_device_info_t> const found
          = enumerate_hid_devices_with (*seam_, filter);
      if (found.empty ())
        {
          error = std::make_error_code (std::errc::no_such_device);
          text = "no HID device matches " + hex4 (options_.vendor_id) + ":"
                 + hex4 (options_.product_id)
                 + (options_.serial_number.empty ()
                        ? std::string ()
                        : " serial " + options_.serial_number);
        }
      else
        path = found.front ().path;
    }

  std::unique_ptr<hidapi_handle> handle;
  if (!error)
    {
      handle = seam_->open_path (path, error);
      if (!handle)
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

  report_layout_t layout;
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

  std::shared_ptr<hid_connection> connection;
  if (handle)
    {
      // One byte more than the largest input report: its ID when numbered.
      connection = std::make_shared<hid_connection> (
          std::move (handle), layout.numbered,
          std::max<std::size_t> (layout.max_input_size () + 1U, 2U),
          slice_milliseconds (options_.read_slice));
      // The reader starts here, while this task counts for shutdown: an
      // open that finishes during shutdown gets no reader. It reads nothing
      // until finish_open releases it on the strand, so no report reaches
      // the strand before the connection is the device's.
      bool started = false;
      try
        {
          reader_body body = { connection, self () };
          started
              = hid_runtime_core::instance ().start_reader (connection, body);
          if (!started)
            error = make_error_code (transport_errc::closed);
        }
      catch (std::system_error const &failure)
        {
          error = failure.code ();
        }
      if (!started)
        {
          text = "no reader thread: " + error.message ();
          connection->request_stop ();
          connection->close_handle ();
          connection.reset ();
        }
    }
  finish_open_step step = { self (), connection, layout, error, text };
  asio::post (strand (), std::move (step));
}

void
hid_device_impl::finish_open (
    std::shared_ptr<hid_connection> const &connection,
    report_layout_t const &layout, std::error_code error,
    std::string const &text)
{
  opening_ = false;
  status_handler_t handler = std::move (open_handler_);
  if (error)
    {
      LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                           "open failed: " + text, log::log_bytes_t ());
      complete (std::move (handler), error);
      return;
    }
  // The reader waits for its release; a stopped one closes the handle.
  if (close_requested_)
    {
      close_requested_ = false;
      connection->request_stop ();
      complete (std::move (handler), transport_errc::cancelled);
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
  if (opening_)
    {
      complete (std::move (handler), transport_errc::cancelled);
      return;
    }
  opening_ = true;
  close_requested_ = false;
  open_handler_ = std::move (handler);
  open_task task = { self () };
  hid_runtime_core::instance ().post (task);
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
      = { self (), connection_, std::move (next.report), write_token_ };
  sends_.pop_front ();
  LUMEX_TRANSPORT_LOG (
      trace, KCOMPONENT, name ().c_str (), "write report",
      log::log_bytes_t (task.report.data (), task.report.size ()));
  hid_runtime_core::instance ().post (std::move (task));
}

void
hid_device_impl::write_blocking (
    std::shared_ptr<hid_connection> const &connection,
    std::vector<std::uint8_t> const &report,
    std::shared_ptr<std::atomic<int>> const &token)
{
  // Taken back by cancel () or close () while it waited for a thread: its
  // handler has run already.
  int queued = KWRITE_QUEUED;
  if (!token->compare_exchange_strong (queued, KWRITE_STARTED,
                                       std::memory_order_acq_rel))
    return;
  std::string text;
  std::error_code const error = connection->write (report, text);
  finish_write_step step = { self (), error, text };
  asio::post (strand (), std::move (step));
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
hid_device_impl::post_report (
    std::shared_ptr<hid_connection> const &connection, message_t message)
{
  deliver_step step = { self (), connection, std::move (message) };
  asio::post (strand (), std::move (step));
}

void
hid_device_impl::post_reader_failure (
    std::shared_ptr<hid_connection> const &connection, std::string const &text)
{
  reader_failed_step step = { self (), connection, text };
  asio::post (strand (), std::move (step));
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
  complete (std::move (write_handler_), transport_errc::cancelled);
}

void
hid_device_impl::do_cancel ()
{
  take_back_queued_write ();
  end_queued_sends (transport_errc::cancelled);
  end_pending_receives (make_error_code (transport_errc::cancelled));
}

void
hid_device_impl::do_close ()
{
  if (opening_)
    close_requested_ = true;
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

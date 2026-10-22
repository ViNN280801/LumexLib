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

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#if defined(_WIN32)
// Asio needs Winsock 2 declared before <windows.h> brings in Winsock 1.
#include <winsock2.h>

#include <windows.h>
#else
#include <termios.h>
#endif

#include "lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"
#include "lumex/applied/transport/detail/LumexPermissionDiagnostics.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/serial/LumexSerialStream.hpp"

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
namespace serial
{
namespace
{
namespace bounded = lumex::applied::serial::probe::detail;
namespace settings_ns = lumex::applied::serial::probe;
using error::transport_errc;

char const KCOMPONENT[] = "transport.serial";

// The defaults asio::serial_port::open gives a port, which assign does not:
// binary mode and timeouts that end a read as soon as bytes are there
// (Windows), raw mode (POSIX).
std::error_code
prepare_native_port (asio::serial_port::native_handle_type handle)
{
#if defined(_WIN32)
  ::DCB dcb;
  std::memset (&dcb, 0, sizeof (dcb));
  dcb.DCBlength = sizeof (dcb);
  if (!::GetCommState (handle, &dcb))
    return std::error_code (static_cast<int> (::GetLastError ()),
                            std::system_category ());
  dcb.fBinary = TRUE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fNull = FALSE;
  dcb.fAbortOnError = FALSE;
  if (!::SetCommState (handle, &dcb))
    return std::error_code (static_cast<int> (::GetLastError ()),
                            std::system_category ());
  ::COMMTIMEOUTS timeouts;
  timeouts.ReadIntervalTimeout = 1;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;
  if (!::SetCommTimeouts (handle, &timeouts))
    return std::error_code (static_cast<int> (::GetLastError ()),
                            std::system_category ());
  return std::error_code ();
#else
  ::termios ios;
  if (::tcgetattr (handle, &ios) != 0)
    return std::error_code (errno, std::system_category ());
  ios.c_iflag &= ~static_cast<tcflag_t> (IGNBRK | BRKINT | PARMRK | ISTRIP
                                         | INLCR | IGNCR | ICRNL | IXON);
  ios.c_oflag &= ~static_cast<tcflag_t> (OPOST);
  ios.c_lflag
      &= ~static_cast<tcflag_t> (ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  ios.c_cflag &= ~static_cast<tcflag_t> (CSIZE | PARENB);
  ios.c_cflag |= CS8;
  ios.c_iflag |= IGNPAR;
  ios.c_cflag |= CREAD | CLOCAL;
  if (::tcsetattr (handle, TCSANOW, &ios) != 0)
    return std::error_code (errno, std::system_category ());
  return std::error_code ();
#endif
}

// Applies the settings through Asio's options; the caller validated them.
std::error_code
apply_settings (asio::serial_port &port, serial_settings_t const &settings)
{
  asio::error_code error;
  port.set_option (asio::serial_port_base::baud_rate (settings.baud_rate),
                   error);
  if (!error)
    port.set_option (
        asio::serial_port_base::character_size (settings.data_bits), error);
  if (!error)
    {
      asio::serial_port_base::parity::type parity
          = asio::serial_port_base::parity::none;
      if (settings.parity == settings_ns::serial_parity::odd)
        parity = asio::serial_port_base::parity::odd;
      else if (settings.parity == settings_ns::serial_parity::even)
        parity = asio::serial_port_base::parity::even;
      port.set_option (asio::serial_port_base::parity (parity), error);
    }
  if (!error)
    {
      asio::serial_port_base::stop_bits::type stop
          = asio::serial_port_base::stop_bits::one;
      if (settings.stop_bits == settings_ns::serial_stop_bits::two)
        stop = asio::serial_port_base::stop_bits::two;
      else if (settings.stop_bits
               == settings_ns::serial_stop_bits::one_point_five)
        stop = asio::serial_port_base::stop_bits::onepointfive;
      port.set_option (asio::serial_port_base::stop_bits (stop), error);
    }
  if (!error)
    {
      asio::serial_port_base::flow_control::type flow
          = asio::serial_port_base::flow_control::none;
      if (settings.flow_control == settings_ns::serial_flow_control::software)
        flow = asio::serial_port_base::flow_control::software;
      else if (settings.flow_control
               == settings_ns::serial_flow_control::hardware)
        flow = asio::serial_port_base::flow_control::hardware;
      port.set_option (asio::serial_port_base::flow_control (flow), error);
    }
  return detail::from_asio (error);
}

class serial_stream_impl;
typedef std::shared_ptr<serial_stream_impl> serial_impl_ptr;

struct open_finished
{
  serial_impl_ptr impl;
  status_handler_t handler;
  bounded::open_result_t result;
  // The permission diagnostic, built by the worker thread; empty when the
  // open did not fail on a permission or the error log was off.
  std::string permission_text;
  // The resolve_path of the options failed: nothing was opened.
  std::error_code resolve_error;
  void operator() ();
};

struct open_worker
{
  serial_impl_ptr impl;
  status_handler_t handler;
  std::string path;
  std::function<std::string ()> resolve;
  deadline_t deadline;
  void operator() ();
};

struct io_finished
{
  serial_impl_ptr impl;
  io_handler_t handler;
  std::uint8_t const *data;
  bool is_read;
  void operator() (asio::error_code const &error, std::size_t bytes);
};

class serial_stream_impl final : public stream::stream_impl
{
public:
  serial_stream_impl (executor_type const &executor,
                      serial_stream_options_t const &options)
      : stream_impl (executor, options.path), options_ (options),
        port_ (strand ())
  {
  }

  ~serial_stream_impl () override
  {
    asio::error_code ignored;
    port_.close (ignored);
  }

  void
  finish_open (status_handler_t handler, bounded::open_result_t const &result,
               std::string const &permission_text,
               std::error_code const &resolve_error)
  {
    opening_ = false;
    if (resolve_error)
      {
        close_requested_ = false;
        LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                             "no port to open: " + resolve_error.message (),
                             log::log_bytes_t ());
        complete (std::move (handler), resolve_error);
        return;
      }
    if (result.outcome != bounded::open_outcome::opened)
      {
        bool const timed_out
            = result.outcome == bounded::open_outcome::timed_out;
        // POSIX EACCES / EPERM is permission_denied; every other failure,
        // and on Windows ERROR_ACCESS_DENIED (a busy port), stays the system
        // error.
        std::error_code const error
            = timed_out ? make_error_code (transport_errc::timeout)
                        : detail::open_failure_code (result.error_code);
        // The text with the node, owner and group comes from the worker
        // (the user database lookups must not run on the io thread).
        std::string const &text = !permission_text.empty ()
                                      ? permission_text
                                      : result.system_error;
        LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                             "open failed: " + text, log::log_bytes_t ());
        complete (std::move (handler), error);
        return;
      }
    if (close_requested_)
      {
        bounded::close_serial_handle (result.handle);
        close_requested_ = false;
        complete (std::move (handler), transport_errc::cancelled);
        return;
      }
    std::error_code error = prepare_native_port (result.handle);
    if (!error)
      {
        asio::error_code assigned;
        port_.assign (result.handle, assigned);
        error = detail::from_asio (assigned);
        if (error)
          bounded::close_serial_handle (result.handle);
      }
    else
      bounded::close_serial_handle (result.handle);
    if (!error)
      {
        error = apply_settings (port_, options_.settings);
        if (error)
          {
            asio::error_code ignored;
            port_.close (ignored);
          }
      }
    if (error)
      {
        LUMEX_TRANSPORT_LOG (error, KCOMPONENT, name ().c_str (),
                             "configure failed: " + error.message (),
                             log::log_bytes_t ());
        complete (std::move (handler), error);
        return;
      }
    set_open (true);
    LUMEX_TRANSPORT_LOG (info, KCOMPONENT, name ().c_str (), "opened",
                         log::log_bytes_t ());
    complete (std::move (handler), std::error_code ());
  }

  void
  finish_io (io_handler_t handler, std::uint8_t const *data, bool is_read,
             asio::error_code const &asio_error, std::size_t bytes)
  {
    std::error_code const error = detail::from_asio (asio_error);
    if (bytes > 0)
      LUMEX_TRANSPORT_LOG (trace, KCOMPONENT, name ().c_str (),
                           std::string (is_read ? "read " : "wrote ")
                               + std::to_string (bytes) + " B",
                           log::log_bytes_t (data, bytes));
    complete (std::move (handler), error, bytes);
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    if (is_open ())
      {
        complete (std::move (handler), std::error_code ());
        return;
      }
    std::error_code const unsupported
        = validate_serial_settings (options_.settings);
    if (unsupported)
      {
        complete (std::move (handler), unsupported);
        return;
      }
    if (opening_)
      {
        complete (std::move (handler), transport_errc::cancelled);
        return;
      }
    opening_ = true;
    close_requested_ = false;
    open_worker worker
        = { std::static_pointer_cast<serial_stream_impl> (shared_from_this ()),
            std::move (handler), serial_open_path (name ()),
            options_.resolve_path, options_.open_deadline };
    // The bounded open blocks up to its deadline: not on the strand.
    std::thread (std::move (worker)).detach ();
  }

  void
  do_read_some (mutable_bytes_t buffer, io_handler_t handler) override
  {
    io_finished done
        = { std::static_pointer_cast<serial_stream_impl> (shared_from_this ()),
            std::move (handler), buffer.data (), true };
    port_.async_read_some (asio::buffer (buffer.data (), buffer.size ()),
                           std::move (done));
  }

  void
  do_write_some (const_bytes_t data, io_handler_t handler) override
  {
    io_finished done
        = { std::static_pointer_cast<serial_stream_impl> (shared_from_this ()),
            std::move (handler), data.data (), false };
    port_.async_write_some (asio::buffer (data.data (), data.size ()),
                            std::move (done));
  }

  void
  do_cancel () override
  {
    asio::error_code ignored;
    port_.cancel (ignored);
  }

  void
  do_close () override
  {
    if (opening_)
      close_requested_ = true;
    if (port_.is_open ())
      {
        asio::error_code ignored;
        port_.close (ignored);
        LUMEX_TRANSPORT_LOG (info, KCOMPONENT, name ().c_str (), "closed",
                             log::log_bytes_t ());
      }
  }

private:
  serial_stream_options_t options_;
  asio::serial_port port_;
  bool opening_ = false;
  bool close_requested_ = false;
};

// The port of this open: `path`, or what `resolve` finds (on this thread:
// an enumeration may take a while).
std::error_code
resolve_open_path (std::function<std::string ()> const &resolve,
                   std::string &path)
{
  if (!resolve)
    return std::error_code ();
  try
    {
      std::string const found = resolve ();
      if (found.empty ())
        return make_error_code (transport_errc::invalid_endpoint);
      path = serial_open_path (found);
      return std::error_code ();
    }
  catch (std::system_error const &failure)
    {
      return failure.code ()
                 ? failure.code ()
                 : make_error_code (transport_errc::invalid_endpoint);
    }
  catch (...)
    {
      return make_error_code (transport_errc::invalid_endpoint);
    }
}

void
open_worker::operator() ()
{
  std::error_code const resolve_error = resolve_open_path (resolve, path);
  bounded::open_result_t result;
  if (!resolve_error)
    result = bounded::bounded_open_serial_port (path,
                                                static_cast<std::uint32_t> (
#if defined(_WIN32)
                                                    FILE_FLAG_OVERLAPPED
#else
                                                    0U
#endif
                                                    ),
                                                deadline);
  // The diagnostic looks up the node, its owner and group (stat, getpwuid_r,
  // getgrgid_r), which may block: here, on this worker thread, and only when
  // the error record would be written.
  std::string permission_text;
  if (!resolve_error && result.outcome == bounded::open_outcome::failed
      && detail::is_permission_error (result.error_code)
      && log::log_enabled (log::log_level::error, KCOMPONENT))
    permission_text = detail::describe_permission_denied (path);
  stream::stream_impl::strand_type const strand = impl->strand ();
  open_finished finished = { std::move (impl), std::move (handler), result,
                             std::move (permission_text), resolve_error };
  asio::post (strand, std::move (finished));
}

void
open_finished::operator() ()
{
  impl->finish_open (std::move (handler), result, permission_text,
                     resolve_error);
}

void
io_finished::operator() (asio::error_code const &error, std::size_t bytes)
{
  impl->finish_io (std::move (handler), data, is_read, error, bytes);
}
} // namespace

std::string
serial_open_path (std::string const &path)
{
#if defined(_WIN32)
  bool const is_com_name = path.size () > 3
                           && (path.compare (0, 3, "COM") == 0
                               || path.compare (0, 3, "com") == 0);
  if (is_com_name)
    return "\\\\.\\" + path;
#endif
  return path;
}

std::error_code
validate_serial_settings (serial_settings_t const &settings)
{
  if (settings.data_bits < 5 || settings.data_bits > 8
      || settings.parity == settings_ns::serial_parity::mark
      || settings.parity == settings_ns::serial_parity::space
      || settings.baud_rate == 0)
    return std::make_error_code (std::errc::not_supported);
#if !defined(_WIN32)
  if (settings.stop_bits == settings_ns::serial_stop_bits::one_point_five)
    return std::make_error_code (std::errc::not_supported);
#endif
  return std::error_code ();
}

stream::byte_stream
make_serial_stream (executor_type const &executor,
                    serial_stream_options_t const &options)
{
  return stream::byte_stream (
      std::make_shared<serial_stream_impl> (executor, options));
}
} // namespace serial
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

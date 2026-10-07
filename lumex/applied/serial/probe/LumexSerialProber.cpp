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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "lumex/applied/serial/probe/LumexSerialProber.hpp"
#include "lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp"
#include "lumex/applied/serial/probe/detail/LumexSerialProbeEngine.hpp"
#include "lumex/applied/serial/probe/detail/LumexSerialTransport.hpp"
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
 * @brief Successful transport result.
 * @param bytes Bytes written or read.
 */
transport_result_t
transport_ok_result (std::size_t bytes = 0U)
{
  transport_result_t result;
  result.status = transport_status::ok;
  result.bytes = bytes;
  return result;
}

/**
 * @brief Deadline transport result.
 * @param text Diagnostic text; may be empty.
 */
transport_result_t
transport_timed_out_result (std::string text = std::string ())
{
  transport_result_t result;
  result.status = transport_status::deadline_exceeded;
  result.system_error = std::move (text);
  return result;
}

/**
 * @brief Failed transport result; fills the text from the error code when
 * the caller has none.
 * @param error_code Platform error; 0 when none.
 * @param text Diagnostic text; may be empty.
 */
transport_result_t
transport_failed_result (int error_code, std::string text = std::string ())
{
  transport_result_t result;
  result.status = transport_status::failed;
  result.error_code = error_code;
  result.system_error
      = text.empty ()
            ? resolver::port_process_resolver::format_system_error (error_code)
            : std::move (text);
  return result;
}

#if defined(LUMEX_OS_WINDOWS)
/**
 * @brief Clamps a budget to the DWORD range of the Win32 waits.
 * @param budget Milliseconds left; non-positive means "do not wait".
 */
DWORD
wait_ms (std::chrono::milliseconds budget)
{
  if (budget <= std::chrono::milliseconds (0))
    return 0U;
  long long const value = static_cast<long long> (budget.count ());
  long long const k_max_wait = 0xFFFFFFFELL;
  return static_cast<DWORD> (value > k_max_wait ? k_max_wait : value);
}

/**
 * @brief Serial transport over the Win32 comm API.
 *
 * @details Opens through the bounded open with `FILE_FLAG_OVERLAPPED`, so
 * a stuck Bluetooth virtual COM port never blocks the caller past the
 * deadline, and drives every exchange with overlapped calls cancelled by
 * `CancelIoEx` when the budget expires. `read` first waits for `EV_RXCHAR`
 * with `WaitCommEvent` and then collects the bytes that arrived; the
 * `COMMTIMEOUTS` are set to "return what is available now", because the
 * waiting is done in the overlapped calls.
 */
class windows_transport final : public serial_transport_t
{
public:
  windows_transport (std::string port_path, serial_port_settings_t settings)
      : path_ (std::move (port_path)), settings_ (settings)
  {
  }

  ~windows_transport () override { close (); }

  bool
  is_open () const override
  {
    return handle_ != invalid_native_serial_handle ();
  }

  transport_result_t
  open (std::chrono::milliseconds deadline) override
  {
    if (is_open ())
      return transport_ok_result ();

    open_result_t const opened
        = bounded_open_serial_port (path_, FILE_FLAG_OVERLAPPED, deadline);
    if (opened.outcome == open_outcome::timed_out)
      return transport_timed_out_result (opened.system_error);
    if (opened.outcome == open_outcome::failed)
      return transport_failed_result (opened.error_code, opened.system_error);

    handle_ = opened.handle;
    if (!configure ())
      {
        int const error = static_cast<int> (::GetLastError ());
        close ();
        return transport_failed_result (error, std::string ());
      }
    return transport_ok_result ();
  }

  void
  close () override
  {
    if (is_open ())
      {
        close_serial_handle (handle_);
        handle_ = invalid_native_serial_handle ();
      }
  }

  transport_result_t
  write (std::uint8_t const *data, std::size_t size,
         std::chrono::milliseconds deadline) override
  {
    if (size == 0U)
      return transport_ok_result ();

    std::chrono::steady_clock::time_point const start
        = std::chrono::steady_clock::now ();
    auto remaining = [start, deadline] () -> std::chrono::milliseconds
      {
        std::chrono::milliseconds const used = elapsed_since (start);
        return deadline > used ? deadline - used
                               : std::chrono::milliseconds (0);
      };

    HANDLE const handle = static_cast<HANDLE> (handle_);
    std::size_t written = 0U;
    while (written < size)
      {
        if (remaining () <= std::chrono::milliseconds (0))
          {
            transport_result_t out = transport_timed_out_result (
                "the request was not written in full before the deadline");
            out.bytes = written;
            return out;
          }

        OVERLAPPED overlapped = OVERLAPPED ();
        overlapped.hEvent = ::CreateEventA (nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr)
          return transport_failed_result (static_cast<int> (::GetLastError ()),
                                          std::string ());

        std::size_t const pending = size - written;
        DWORD const chunk = static_cast<DWORD> (
            pending > 0xFFFFFFFFU ? 0xFFFFFFFFU : pending);
        BOOL const started = ::WriteFile (handle, data + written, chunk,
                                          nullptr, &overlapped);
        if (started == 0 && ::GetLastError () != ERROR_IO_PENDING)
          {
            int const error = static_cast<int> (::GetLastError ());
            ::CloseHandle (overlapped.hEvent);
            transport_result_t out
                = transport_failed_result (error, std::string ());
            out.bytes = written;
            return out;
          }

        DWORD const waited = ::WaitForSingleObject (overlapped.hEvent,
                                                    wait_ms (remaining ()));
        if (waited == WAIT_TIMEOUT)
          {
            ::CancelIoEx (handle, &overlapped);
            DWORD transferred = 0;
            ::GetOverlappedResult (handle, &overlapped, &transferred, TRUE);
            ::CloseHandle (overlapped.hEvent);
            transport_result_t out = transport_timed_out_result (
                "the request was not written in full before the deadline");
            out.bytes = written + static_cast<std::size_t> (transferred);
            return out;
          }

        DWORD transferred = 0;
        if (::GetOverlappedResult (handle, &overlapped, &transferred, FALSE)
            == 0)
          {
            int const error = static_cast<int> (::GetLastError ());
            ::CloseHandle (overlapped.hEvent);
            transport_result_t out
                = transport_failed_result (error, std::string ());
            out.bytes = written;
            return out;
          }
        ::CloseHandle (overlapped.hEvent);

        if (transferred == 0U)
          {
            transport_result_t out = transport_failed_result (
                0, "the write completed without progress");
            out.bytes = written;
            return out;
          }
        written += static_cast<std::size_t> (transferred);
      }

    return transport_ok_result (written);
  }

  transport_result_t
  read (std::uint8_t *buffer, std::size_t capacity,
        std::chrono::milliseconds deadline) override
  {
    if (capacity == 0U)
      return transport_ok_result ();

    std::chrono::steady_clock::time_point const start
        = std::chrono::steady_clock::now ();
    auto remaining = [start, deadline] () -> std::chrono::milliseconds
      {
        std::chrono::milliseconds const used = elapsed_since (start);
        return deadline > used ? deadline - used
                               : std::chrono::milliseconds (0);
      };

    HANDLE const handle = static_cast<HANDLE> (handle_);
    for (;;)
      {
        if (remaining () <= std::chrono::milliseconds (0))
          return transport_timed_out_result ();

        // Wait until at least one byte arrives, bounded by the budget.
        OVERLAPPED wait_overlapped = OVERLAPPED ();
        wait_overlapped.hEvent
            = ::CreateEventA (nullptr, TRUE, FALSE, nullptr);
        if (wait_overlapped.hEvent == nullptr)
          return transport_failed_result (static_cast<int> (::GetLastError ()),
                                          std::string ());

        DWORD events = 0;
        BOOL const waiting
            = ::WaitCommEvent (handle, &events, &wait_overlapped);
        if (waiting == 0 && ::GetLastError () != ERROR_IO_PENDING)
          {
            int const error = static_cast<int> (::GetLastError ());
            ::CloseHandle (wait_overlapped.hEvent);
            return transport_failed_result (error, std::string ());
          }

        DWORD const waited = ::WaitForSingleObject (wait_overlapped.hEvent,
                                                    wait_ms (remaining ()));
        if (waited == WAIT_TIMEOUT)
          {
            ::CancelIoEx (handle, &wait_overlapped);
            DWORD ignored = 0;
            ::GetOverlappedResult (handle, &wait_overlapped, &ignored, TRUE);
            ::CloseHandle (wait_overlapped.hEvent);
            return transport_timed_out_result ();
          }
        ::CloseHandle (wait_overlapped.hEvent);

        // Collect what is available; the configured COMMTIMEOUTS make the
        // read return immediately with the received bytes.
        OVERLAPPED read_overlapped = OVERLAPPED ();
        read_overlapped.hEvent
            = ::CreateEventA (nullptr, TRUE, FALSE, nullptr);
        if (read_overlapped.hEvent == nullptr)
          return transport_failed_result (static_cast<int> (::GetLastError ()),
                                          std::string ());

        DWORD const chunk = static_cast<DWORD> (
            capacity > 0xFFFFFFFFU ? 0xFFFFFFFFU : capacity);
        BOOL const reading
            = ::ReadFile (handle, buffer, chunk, nullptr, &read_overlapped);
        if (reading == 0 && ::GetLastError () != ERROR_IO_PENDING)
          {
            int const error = static_cast<int> (::GetLastError ());
            ::CloseHandle (read_overlapped.hEvent);
            return transport_failed_result (error, std::string ());
          }

        DWORD const read_waited = ::WaitForSingleObject (
            read_overlapped.hEvent, wait_ms (remaining ()));
        if (read_waited == WAIT_TIMEOUT)
          {
            ::CancelIoEx (handle, &read_overlapped);
            DWORD ignored = 0;
            ::GetOverlappedResult (handle, &read_overlapped, &ignored, TRUE);
            ::CloseHandle (read_overlapped.hEvent);
            return transport_timed_out_result ();
          }

        DWORD transferred = 0;
        if (::GetOverlappedResult (handle, &read_overlapped, &transferred,
                                   FALSE)
            == 0)
          {
            int const error = static_cast<int> (::GetLastError ());
            ::CloseHandle (read_overlapped.hEvent);
            return transport_failed_result (error, std::string ());
          }
        ::CloseHandle (read_overlapped.hEvent);

        if (transferred > 0U)
          return transport_ok_result (static_cast<std::size_t> (transferred));
        // The event can fire without data (for example an error event);
        // keep waiting inside the remaining budget.
      }
  }

  void
  purge () override
  {
    ::PurgeComm (static_cast<HANDLE> (handle_), PURGE_RXCLEAR | PURGE_TXCLEAR);
  }

private:
  bool
  configure ()
  {
    HANDLE const handle = static_cast<HANDLE> (handle_);

    DCB dcb;
    std::memset (&dcb, 0, sizeof (dcb));
    dcb.DCBlength = sizeof (dcb);
    if (::GetCommState (handle, &dcb) == 0)
      return false;

    dcb.BaudRate = settings_.baud_rate;
    dcb.ByteSize = settings_.data_bits;
    dcb.fBinary = TRUE;
    dcb.fParity = settings_.parity == serial_parity::none ? FALSE : TRUE;
    switch (settings_.parity)
      {
      case serial_parity::none:
        dcb.Parity = NOPARITY;
        break;
      case serial_parity::odd:
        dcb.Parity = ODDPARITY;
        break;
      case serial_parity::even:
        dcb.Parity = EVENPARITY;
        break;
      case serial_parity::mark:
        dcb.Parity = MARKPARITY;
        break;
      case serial_parity::space:
        dcb.Parity = SPACEPARITY;
        break;
      }
    switch (settings_.stop_bits)
      {
      case serial_stop_bits::one:
        dcb.StopBits = ONESTOPBIT;
        break;
      case serial_stop_bits::one_point_five:
        dcb.StopBits = ONE5STOPBITS;
        break;
      case serial_stop_bits::two:
        dcb.StopBits = TWOSTOPBITS;
        break;
      }

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    switch (settings_.flow_control)
      {
      case serial_flow_control::none:
        break;
      case serial_flow_control::software:
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        break;
      case serial_flow_control::hardware:
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        break;
      }

    if (::SetCommState (handle, &dcb) == 0)
      return false;

    // "Return what is available now": the waits live in the overlapped
    // calls above and are bounded by the caller deadline.
    COMMTIMEOUTS timeouts;
    std::memset (&timeouts, 0, sizeof (timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    return ::SetCommTimeouts (handle, &timeouts) != 0;
  }

  std::string path_;
  serial_port_settings_t settings_;
  native_serial_handle_t handle_ = invalid_native_serial_handle ();
};
#else
/**
 * @brief Clamps a budget to the `int` range of `poll`.
 * @param budget Milliseconds left; non-positive means "do not wait".
 */
int
poll_ms (std::chrono::milliseconds budget)
{
  if (budget <= std::chrono::milliseconds (0))
    return 0;
  long long const value = static_cast<long long> (budget.count ());
  long long const k_max_wait = 0x7FFFFFFFLL;
  return static_cast<int> (value > k_max_wait ? k_max_wait : value);
}

/**
 * @brief Serial transport over the POSIX termios API.
 *
 * @details Opens through the bounded open (`O_NONBLOCK`), applies the
 * caller settings on a raw termios base and bounds every wait with `poll`
 * on the remaining budget. Settings the platform cannot express (1.5 stop
 * bits, mark or space parity without `CMSPAR`, hardware flow control
 * without `CRTSCTS`, an unknown baud rate) fail the open with an
 * explanatory text instead of being silently dropped.
 */
class posix_transport final : public serial_transport_t
{
public:
  posix_transport (std::string port_path, serial_port_settings_t settings)
      : path_ (std::move (port_path)), settings_ (settings)
  {
  }

  ~posix_transport () override { close (); }

  bool
  is_open () const override
  {
    return fd_ >= 0;
  }

  transport_result_t
  open (std::chrono::milliseconds deadline) override
  {
    if (is_open ())
      return transport_ok_result ();

    std::string const invalid = validate_settings ();
    if (!invalid.empty ())
      return transport_failed_result (0, invalid);
    if (baud_constant () == 0)
      return transport_failed_result (0, "unsupported baud rate");

    open_result_t const opened
        = bounded_open_serial_port (path_, 0U, deadline);
    if (opened.outcome == open_outcome::timed_out)
      return transport_timed_out_result (opened.system_error);
    if (opened.outcome == open_outcome::failed)
      return transport_failed_result (opened.error_code, opened.system_error);

    fd_ = opened.handle;
    if (!configure ())
      {
        int const error = errno;
        close ();
        return transport_failed_result (error, std::string ());
      }
    return transport_ok_result ();
  }

  void
  close () override
  {
    if (is_open ())
      {
        ::close (fd_);
        fd_ = -1;
      }
  }

  transport_result_t
  write (std::uint8_t const *data, std::size_t size,
         std::chrono::milliseconds deadline) override
  {
    if (size == 0U)
      return transport_ok_result ();

    std::chrono::steady_clock::time_point const start
        = std::chrono::steady_clock::now ();
    auto remaining = [start, deadline] () -> std::chrono::milliseconds
      {
        std::chrono::milliseconds const used = elapsed_since (start);
        return deadline > used ? deadline - used
                               : std::chrono::milliseconds (0);
      };

    std::size_t sent = 0U;
    while (sent < size)
      {
        std::chrono::milliseconds const left = remaining ();
        if (left <= std::chrono::milliseconds (0))
          {
            transport_result_t out = transport_timed_out_result (
                "the request was not written in full before the deadline");
            out.bytes = sent;
            return out;
          }

        struct pollfd descriptor = {};
        descriptor.fd = fd_;
        descriptor.events = POLLOUT;
        int const ready = ::poll (&descriptor, 1, poll_ms (left));
        if (ready < 0)
          {
            if (errno == EINTR)
              continue;
            transport_result_t out
                = transport_failed_result (errno, std::string ());
            out.bytes = sent;
            return out;
          }
        if (ready == 0)
          {
            transport_result_t out = transport_timed_out_result (
                "the request was not written in full before the deadline");
            out.bytes = sent;
            return out;
          }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
          {
            transport_result_t out = transport_failed_result (
                0, "the device reported an error while writing");
            out.bytes = sent;
            return out;
          }

        ssize_t const put = ::write (fd_, data + sent, size - sent);
        if (put < 0)
          {
            if (errno == EAGAIN || errno == EINTR)
              continue;
            transport_result_t out
                = transport_failed_result (errno, std::string ());
            out.bytes = sent;
            return out;
          }
        sent += static_cast<std::size_t> (put);
      }

    return transport_ok_result (sent);
  }

  transport_result_t
  read (std::uint8_t *buffer, std::size_t capacity,
        std::chrono::milliseconds deadline) override
  {
    if (capacity == 0U)
      return transport_ok_result ();

    std::chrono::steady_clock::time_point const start
        = std::chrono::steady_clock::now ();
    auto remaining = [start, deadline] () -> std::chrono::milliseconds
      {
        std::chrono::milliseconds const used = elapsed_since (start);
        return deadline > used ? deadline - used
                               : std::chrono::milliseconds (0);
      };

    for (;;)
      {
        std::chrono::milliseconds const left = remaining ();
        if (left <= std::chrono::milliseconds (0))
          return transport_timed_out_result ();

        struct pollfd descriptor = {};
        descriptor.fd = fd_;
        descriptor.events = POLLIN;
        int const ready = ::poll (&descriptor, 1, poll_ms (left));
        if (ready < 0)
          {
            if (errno == EINTR)
              continue;
            return transport_failed_result (errno, std::string ());
          }
        if (ready == 0)
          return transport_timed_out_result ();
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
          return transport_failed_result (
              0, "the device reported an error while reading");

        ssize_t const got = ::read (fd_, buffer, capacity);
        if (got < 0)
          {
            if (errno == EAGAIN || errno == EINTR)
              continue;
            return transport_failed_result (errno, std::string ());
          }
        if (got == 0)
          continue;
        return transport_ok_result (static_cast<std::size_t> (got));
      }
  }

  void
  purge () override
  {
    ::tcflush (fd_, TCIOFLUSH);
  }

private:
  std::string
  validate_settings () const
  {
    if (settings_.data_bits < 5U || settings_.data_bits > 8U)
      return "data bits must be between 5 and 8";
    if (settings_.stop_bits == serial_stop_bits::one_point_five)
      return "1.5 stop bits are not supported on this platform";
#if !defined(CMSPAR)
    if (settings_.parity == serial_parity::mark
        || settings_.parity == serial_parity::space)
      return "mark and space parity are not supported on this platform";
#endif
#if !defined(CRTSCTS)
    if (settings_.flow_control == serial_flow_control::hardware)
      return "hardware flow control is not supported on this platform";
#endif
    return std::string ();
  }

  speed_t
  baud_constant () const
  {
    switch (settings_.baud_rate)
      {
      case 1200U:
        return B1200;
      case 2400U:
        return B2400;
      case 4800U:
        return B4800;
      case 9600U:
        return B9600;
      case 19200U:
        return B19200;
      case 38400U:
        return B38400;
      case 57600U:
        return B57600;
      case 115200U:
        return B115200;
#if defined(B230400)
      case 230400U:
        return B230400;
#endif
      default:
        return 0;
      }
  }

  bool
  configure ()
  {
    struct termios attributes = {};
    if (::tcgetattr (fd_, &attributes) != 0)
      return false;

    attributes.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR
                            | ICRNL | IXON | IXOFF | IXANY);
    attributes.c_oflag &= ~OPOST;
    attributes.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    attributes.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
#if defined(CRTSCTS)
    attributes.c_cflag &= ~CRTSCTS;
#endif

    switch (settings_.data_bits)
      {
      case 5U:
        attributes.c_cflag |= CS5;
        break;
      case 6U:
        attributes.c_cflag |= CS6;
        break;
      case 7U:
        attributes.c_cflag |= CS7;
        break;
      case 8U:
        attributes.c_cflag |= CS8;
        break;
      default:
        return false;
      }

    switch (settings_.parity)
      {
      case serial_parity::none:
        break;
      case serial_parity::odd:
        attributes.c_cflag |= PARENB | PARODD;
        break;
      case serial_parity::even:
        attributes.c_cflag |= PARENB;
        break;
      case serial_parity::mark:
#if defined(CMSPAR)
        attributes.c_cflag |= PARENB | CMSPAR | PARODD;
        break;
#else
        return false;
#endif
      case serial_parity::space:
#if defined(CMSPAR)
        attributes.c_cflag |= PARENB | CMSPAR;
        break;
#else
        return false;
#endif
      }

    if (settings_.stop_bits == serial_stop_bits::two)
      attributes.c_cflag |= CSTOPB;

    switch (settings_.flow_control)
      {
      case serial_flow_control::none:
        break;
      case serial_flow_control::software:
        attributes.c_iflag |= IXON | IXOFF;
        break;
      case serial_flow_control::hardware:
#if defined(CRTSCTS)
        attributes.c_cflag |= CRTSCTS;
        break;
#else
        return false;
#endif
      }

    attributes.c_cc[VMIN] = 0;
    attributes.c_cc[VTIME] = 0;

    speed_t const speed = baud_constant ();
    if (speed == 0 || ::cfsetispeed (&attributes, speed) != 0
        || ::cfsetospeed (&attributes, speed) != 0)
      return false;
    return ::tcsetattr (fd_, TCSANOW, &attributes) == 0;
  }

  std::string path_;
  serial_port_settings_t settings_;
  int fd_ = -1;
};
#endif

/**
 * @brief Creates the platform transport for one port.
 * @param port_path Path passed to the platform open.
 * @param settings Settings applied when the port is opened.
 */
std::unique_ptr<serial_transport_t>
make_platform_transport (std::string port_path,
                         serial_port_settings_t const &settings)
{
#if defined(LUMEX_OS_WINDOWS)
  return std::unique_ptr<serial_transport_t> (
      new windows_transport (std::move (port_path), settings));
#else
  return std::unique_ptr<serial_transport_t> (
      new posix_transport (std::move (port_path), settings));
#endif
}

} // namespace
} // namespace detail

/**
 * @brief State of one `SerialProber` session.
 */
struct SerialProber::Impl
{
  /**
   * @brief Builds the platform transport for `port_path`.
   * @param port_path Path passed to the platform open.
   * @param settings Settings applied when the port is opened.
   */
  Impl (std::string port_path, serial_port_settings_t const &settings)
      : transport (
            detail::make_platform_transport (std::move (port_path), settings))
  {
  }

  std::unique_ptr<detail::serial_transport_t> transport;
};

SerialProber::SerialProber (std::string port_path,
                            serial_port_settings_t const &settings)
    : impl_ (new Impl (std::move (port_path), settings))
{
}

SerialProber::~SerialProber () = default;

SerialProber::SerialProber (SerialProber &&other) LUMEX_NOEXCEPT = default;

SerialProber &SerialProber::operator= (SerialProber &&other) LUMEX_NOEXCEPT
    = default;

serial_io_result_t
SerialProber::open (std::chrono::milliseconds deadline)
{
  serial_io_result_t result;
  if (!impl_ || !impl_->transport)
    {
      result.status = serial_io_status::not_open;
      result.system_error = "the session was moved from";
      return result;
    }

  if (impl_->transport->is_open ())
    {
      result.status = serial_io_status::ok;
      return result;
    }

  detail::transport_result_t const opened = impl_->transport->open (deadline);
  switch (opened.status)
    {
    case detail::transport_status::ok:
      result.status = serial_io_status::ok;
      break;
    case detail::transport_status::deadline_exceeded:
      result.status = serial_io_status::deadline_exceeded;
      break;
    case detail::transport_status::failed:
      result.status = serial_io_status::failed;
      break;
    }
  result.system_error = opened.system_error;
  return result;
}

void
SerialProber::close () LUMEX_NOEXCEPT
{
  if (impl_ && impl_->transport)
    impl_->transport->close ();
}

bool
SerialProber::is_open () const LUMEX_NOEXCEPT
{
  return impl_ && impl_->transport && impl_->transport->is_open ();
}

serial_probe_result_t
SerialProber::probe (
    std::vector<std::uint8_t> const &request,
    std::function<bool (std::vector<std::uint8_t> const &)> const
        &response_complete,
    std::chrono::milliseconds deadline, std::size_t max_response_bytes)
{
  if (!impl_ || !impl_->transport)
    {
      serial_probe_result_t result;
      result.status = serial_probe_status::open_failed;
      result.system_error = "the session was moved from";
      return result;
    }
  return detail::run_probe (*impl_->transport, request, response_complete,
                            deadline, max_response_bytes);
}

serial_io_result_t
SerialProber::write (std::vector<std::uint8_t> const &data,
                     std::chrono::milliseconds deadline)
{
  serial_io_result_t result;
  if (!impl_ || !impl_->transport || !impl_->transport->is_open ())
    {
      result.status = serial_io_status::not_open;
      return result;
    }
  if (data.empty ())
    {
      result.status = serial_io_status::ok;
      return result;
    }

  detail::transport_result_t const written
      = impl_->transport->write (data.data (), data.size (), deadline);
  switch (written.status)
    {
    case detail::transport_status::ok:
      result.status = serial_io_status::ok;
      break;
    case detail::transport_status::deadline_exceeded:
      result.status = serial_io_status::deadline_exceeded;
      break;
    case detail::transport_status::failed:
      result.status = serial_io_status::failed;
      break;
    }
  result.bytes_transferred = written.bytes;
  result.system_error = written.system_error;
  return result;
}

serial_read_result_t
SerialProber::read (std::chrono::milliseconds deadline)
{
  serial_read_result_t result;
  if (!impl_ || !impl_->transport || !impl_->transport->is_open ())
    {
      result.status = serial_io_status::not_open;
      return result;
    }

  std::vector<std::uint8_t> buffer (detail::KPROBE_READ_CHUNK_BYTES);
  detail::transport_result_t const got
      = impl_->transport->read (buffer.data (), buffer.size (), deadline);
  switch (got.status)
    {
    case detail::transport_status::ok:
      result.status = serial_io_status::ok;
      break;
    case detail::transport_status::deadline_exceeded:
      result.status = serial_io_status::deadline_exceeded;
      break;
    case detail::transport_status::failed:
      result.status = serial_io_status::failed;
      break;
    }

  if (result.status == serial_io_status::ok)
    {
      std::size_t const count
          = got.bytes < buffer.size () ? got.bytes : buffer.size ();
      result.data.assign (buffer.begin (),
                          buffer.begin ()
                              + static_cast<std::ptrdiff_t> (count));
    }
  result.system_error = got.system_error;
  return result;
}

} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

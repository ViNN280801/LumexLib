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
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

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
namespace testing
{
using error::transport_errc;

/// The implementation of a mock stream; every member is guarded by mutex_
/// (the control is called from test threads, the do_* functions on the
/// strand).
class mock_stream_impl final : public stream::stream_impl
{
public:
  mock_stream_impl (executor_type const &executor, std::string const &name)
      : stream_impl (executor, name)
  {
  }

  void
  feed (std::vector<std::uint8_t> const &bytes)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    received_.insert (received_.end (), bytes.begin (), bytes.end ());
    serve_pending_read ();
  }

  void
  fail_open (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    open_error_ = error;
  }

  void
  fail_next_read (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    read_error_ = error;
    if (has_pending_read_)
      {
        has_pending_read_ = false;
        read_error_ = std::error_code ();
        complete (std::move (pending_handler_), error, 0);
      }
  }

  void
  fail_next_write (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    write_error_ = error;
  }

  void
  disconnect ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    disconnected_ = true;
    serve_pending_read ();
  }

  void
  set_write_chunk (std::size_t bytes)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    write_chunk_ = bytes;
  }

  std::vector<std::uint8_t>
  written () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return written_;
  }

  std::size_t
  open_count () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return open_count_;
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (open_error_)
      {
        std::error_code const error = open_error_;
        open_error_ = std::error_code ();
        complete (std::move (handler), error);
        return;
      }
    ++open_count_;
    disconnected_ = false;
    set_open (true);
    complete (std::move (handler), std::error_code ());
  }

  void
  do_read_some (mutable_bytes_t buffer, io_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (read_error_)
      {
        std::error_code const error = read_error_;
        read_error_ = std::error_code ();
        complete (std::move (handler), error, 0);
        return;
      }
    pending_buffer_ = buffer;
    pending_handler_ = std::move (handler);
    has_pending_read_ = true;
    serve_pending_read ();
  }

  void
  do_write_some (const_bytes_t data, io_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (write_error_)
      {
        std::error_code const error = write_error_;
        write_error_ = std::error_code ();
        complete (std::move (handler), error, 0);
        return;
      }
    std::size_t const count = write_chunk_ == 0
                                  ? data.size ()
                                  : std::min (write_chunk_, data.size ());
    written_.insert (written_.end (), data.begin (), data.begin () + count);
    complete (std::move (handler), std::error_code (), count);
  }

  void
  do_cancel () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_read (transport_errc::cancelled);
  }

  void
  do_close () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_read (transport_errc::cancelled);
  }

private:
  // Both expect mutex_ held.
  void
  serve_pending_read ()
  {
    if (!has_pending_read_)
      return;
    if (!received_.empty ())
      {
        std::size_t const count
            = std::min (pending_buffer_.size (), received_.size ());
        std::copy (received_.begin (),
                   received_.begin () + static_cast<std::ptrdiff_t> (count),
                   pending_buffer_.begin ());
        received_.erase (received_.begin (),
                         received_.begin ()
                             + static_cast<std::ptrdiff_t> (count));
        has_pending_read_ = false;
        complete (std::move (pending_handler_), std::error_code (), count);
        return;
      }
    if (disconnected_)
      end_pending_read (transport_errc::disconnected);
  }

  void
  end_pending_read (transport_errc reason)
  {
    if (!has_pending_read_)
      return;
    has_pending_read_ = false;
    complete (std::move (pending_handler_), make_error_code (reason), 0);
  }

  mutable std::mutex mutex_;
  std::deque<std::uint8_t> received_;
  std::vector<std::uint8_t> written_;
  mutable_bytes_t pending_buffer_;
  io_handler_t pending_handler_;
  bool has_pending_read_ = false;
  bool disconnected_ = false;
  std::size_t write_chunk_ = 0;
  std::size_t open_count_ = 0;
  std::error_code open_error_;
  std::error_code read_error_;
  std::error_code write_error_;
};

mock_stream_control::mock_stream_control (
    std::shared_ptr<mock_stream_impl> impl)
    : impl_ (std::move (impl))
{
}

void
mock_stream_control::feed (std::vector<std::uint8_t> const &bytes)
{
  impl_->feed (bytes);
}

void
mock_stream_control::fail_open (std::error_code error)
{
  impl_->fail_open (error);
}

void
mock_stream_control::fail_next_read (std::error_code error)
{
  impl_->fail_next_read (error);
}

void
mock_stream_control::fail_next_write (std::error_code error)
{
  impl_->fail_next_write (error);
}

void
mock_stream_control::disconnect ()
{
  impl_->disconnect ();
}

void
mock_stream_control::set_write_chunk (std::size_t bytes)
{
  impl_->set_write_chunk (bytes);
}

std::vector<std::uint8_t>
mock_stream_control::written () const
{
  return impl_->written ();
}

std::size_t
mock_stream_control::open_count () const
{
  return impl_->open_count ();
}

mock_stream
make_mock_stream (executor_type const &executor, std::string const &name)
{
  std::shared_ptr<mock_stream_impl> const impl
      = std::make_shared<mock_stream_impl> (executor, name);
  mock_stream result
      = { stream::byte_stream (impl), mock_stream_control (impl) };
  return result;
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

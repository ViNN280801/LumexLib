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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/detail/LumexScheduledDeliveries.hpp"
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
/// strand). Observers are called outside the lock.
class mock_stream_impl final : public stream::stream_impl
{
public:
  mock_stream_impl (executor_type const &executor,
                    mock_options_t const &options)
      : stream_impl (executor, options.name), timers_ (options.timers),
        script_enlisted_ (false)
  {
  }

  void
  feed (std::vector<std::uint8_t> const &bytes)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    received_.insert (received_.end (), bytes.begin (), bytes.end ());
    serve_reads ();
  }

  void
  fail_open (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    open_error_ = error;
  }

  void
  hold_next_open ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    hold_open_ = true;
  }

  // The oldest pending read ends with `error`; without one the next read
  // does.
  void
  fail_next_read (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!reads_.empty ())
      {
        io_handler_t handler = std::move (reads_.front ().handler);
        reads_.pop_front ();
        complete (std::move (handler), error, 0);
        return;
      }
    read_error_ = error;
  }

  void
  fail_next_write (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    write_error_ = error;
  }

  void
  fail_write_after (std::size_t writes, std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    writes_before_failure_ = writes;
    delayed_write_error_ = error;
  }

  void
  disconnect ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    disconnected_ = true;
    serve_reads ();
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

  void
  on_write (write_observer_t observer)
  {
    // An observer may own a control of this mock (a cycle): the shutdown of
    // the context drops it, which needs the mock to be enlisted.
    if (observer)
      detail::enlist_keeper (
          script_enlisted_, executor (),
          std::static_pointer_cast<mock_stream_impl> (shared_from_this ()));
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      observer_.swap (observer);
    }
    // The replaced observer dies here, outside the lock (it may own the last
    // reference to something that calls back).
  }

  void
  feed_after (std::chrono::milliseconds delay,
              std::vector<std::uint8_t> const &bytes)
  {
    std::shared_ptr<mock_stream_impl> const self
        = std::static_pointer_cast<mock_stream_impl> (shared_from_this ());
    std::lock_guard<std::mutex> const lock (mutex_);
    scheduled_.schedule (self, timers_, executor_type (strand ()), delay,
                         bytes);
  }

  // A delivery came due (on the strand): its bytes become readable.
  void
  deliver_scheduled (std::uint64_t id)
  {
    std::vector<std::uint8_t> bytes;
    timing::timer expired;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (!scheduled_.take (id, bytes, expired))
        return;
      received_.insert (received_.end (), bytes.begin (), bytes.end ());
      serve_reads ();
    }
  }

  void
  set_read_chunk (std::size_t bytes)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    read_chunk_ = bytes;
  }

  void
  hold_writes ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    hold_writes_ = true;
  }

  // Held writes go on the strand, in order, one by one (an observer may hold
  // again between two of them).
  void
  release_writes ()
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      hold_writes_ = false;
      if (writes_.empty ())
        return;
    }
    release_step step
        = { std::static_pointer_cast<mock_stream_impl> (shared_from_this ()) };
    asio::post (strand (), std::move (step));
  }

  void
  complete_next_read_empty ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    ++empty_reads_;
    serve_reads ();
  }

  // The io_context is being destroyed: the pending reads, writes and open and
  // the observer die uncalled, after the lock is released (their destructors
  // may destroy the last owner of this stream).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    std::deque<pending_read> reads;
    std::deque<pending_write> writes;
    status_handler_t open;
    write_observer_t observer;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      reads.swap (reads_);
      writes.swap (writes_);
      open = std::move (pending_open_);
      observer.swap (observer_);
      has_pending_open_ = false;
    }
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
    if (hold_open_)
      {
        hold_open_ = false;
        pending_open_ = std::move (handler);
        has_pending_open_ = true;
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
    pending_read read = { buffer, std::move (handler) };
    reads_.push_back (std::move (read));
    serve_reads ();
  }

  void
  do_write_some (const_bytes_t data, io_handler_t handler) override
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (hold_writes_ || !writes_.empty ())
        {
          // Behind the held writes, so that they keep their order.
          pending_write held = { data, std::move (handler) };
          writes_.push_back (std::move (held));
          return;
        }
    }
    accept_write (data, std::move (handler));
  }

  void
  do_cancel () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_open ();
    end_pending_reads (transport_errc::cancelled);
    end_pending_writes ();
  }

  void
  do_close () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_open ();
    end_pending_reads (transport_errc::cancelled);
    end_pending_writes ();
  }

private:
  using scheduled_t = detail::scheduled_deliveries<mock_stream_impl,
                                                   std::vector<std::uint8_t>>;

  struct pending_read
  {
    mutable_bytes_t buffer;
    io_handler_t handler;
  };

  struct pending_write
  {
    const_bytes_t data;
    io_handler_t handler;
  };

  // The posted step that lets the held writes go.
  struct release_step
  {
    std::shared_ptr<mock_stream_impl> impl;

    void
    operator() ()
    {
      impl->flush_writes ();
    }
  };

  // Takes one write (as it is, or from the held ones) under the lock, calls
  // the observer outside it. On the strand.
  void
  accept_write (const_bytes_t data, io_handler_t handler)
  {
    write_observer_t observer;
    std::vector<std::uint8_t> chunk;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (!take_write (data, std::move (handler), observer, chunk))
        return;
    }
    if (observer)
      observer (chunk);
  }

  void
  flush_writes ()
  {
    for (;;)
      {
        write_observer_t observer;
        std::vector<std::uint8_t> chunk;
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          if (hold_writes_ || writes_.empty ())
            return;
          pending_write held = std::move (writes_.front ());
          writes_.pop_front ();
          if (!take_write (held.data, std::move (held.handler), observer,
                           chunk))
            continue;
        }
        if (observer)
          observer (chunk);
      }
  }

  // Expects mutex_ held. Fails, or records the chunk and completes; true
  // when the write was accepted (the observer, if any, is returned with the
  // chunk to be called outside the lock).
  bool
  take_write (const_bytes_t data, io_handler_t handler,
              write_observer_t &observer, std::vector<std::uint8_t> &chunk)
  {
    if (write_error_)
      {
        std::error_code const error = write_error_;
        write_error_ = std::error_code ();
        complete (std::move (handler), error, 0);
        return false;
      }
    if (delayed_write_error_)
      {
        if (writes_before_failure_ == 0)
          {
            std::error_code const error = delayed_write_error_;
            delayed_write_error_ = std::error_code ();
            complete (std::move (handler), error, 0);
            return false;
          }
        --writes_before_failure_;
      }
    std::size_t const count = write_chunk_ == 0
                                  ? data.size ()
                                  : std::min (write_chunk_, data.size ());
    written_.insert (written_.end (), data.begin (), data.begin () + count);
    complete (std::move (handler), std::error_code (), count);
    if (observer_)
      {
        observer = observer_;
        chunk.assign (data.begin (), data.begin () + count);
      }
    return true;
  }

  // Expects mutex_ held: serves the pending reads in the order they started
  // while there is something to give them.
  void
  serve_reads ()
  {
    while (!reads_.empty ())
      {
        if (empty_reads_ > 0)
          {
            --empty_reads_;
            io_handler_t handler = std::move (reads_.front ().handler);
            reads_.pop_front ();
            complete (std::move (handler), std::error_code (), 0);
            continue;
          }
        if (!received_.empty ())
          {
            pending_read &front = reads_.front ();
            std::size_t count
                = std::min (front.buffer.size (), received_.size ());
            if (read_chunk_ != 0)
              count = std::min (count, read_chunk_);
            std::copy (received_.begin (),
                       received_.begin ()
                           + static_cast<std::ptrdiff_t> (count),
                       front.buffer.begin ());
            received_.erase (received_.begin (),
                             received_.begin ()
                                 + static_cast<std::ptrdiff_t> (count));
            io_handler_t handler = std::move (front.handler);
            reads_.pop_front ();
            complete (std::move (handler), std::error_code (), count);
            continue;
          }
        if (disconnected_)
          end_pending_reads (transport_errc::disconnected);
        return;
      }
  }

  void
  end_pending_open ()
  {
    if (!has_pending_open_)
      return;
    has_pending_open_ = false;
    complete (std::move (pending_open_),
              make_error_code (transport_errc::cancelled));
  }

  void
  end_pending_reads (transport_errc reason)
  {
    while (!reads_.empty ())
      {
        io_handler_t handler = std::move (reads_.front ().handler);
        reads_.pop_front ();
        complete (std::move (handler), make_error_code (reason), 0);
      }
  }

  void
  end_pending_writes ()
  {
    while (!writes_.empty ())
      {
        io_handler_t handler = std::move (writes_.front ().handler);
        writes_.pop_front ();
        complete (std::move (handler),
                  make_error_code (transport_errc::cancelled), 0);
      }
  }

  mutable std::mutex mutex_;
  timing::timer_source timers_;
  std::atomic<bool> script_enlisted_;
  std::deque<std::uint8_t> received_;
  std::vector<std::uint8_t> written_;
  std::deque<pending_read> reads_;
  std::deque<pending_write> writes_;
  scheduled_t scheduled_;
  write_observer_t observer_;
  status_handler_t pending_open_;
  bool has_pending_open_ = false;
  bool hold_open_ = false;
  bool hold_writes_ = false;
  bool disconnected_ = false;
  std::size_t write_chunk_ = 0;
  std::size_t read_chunk_ = 0;
  std::size_t empty_reads_ = 0;
  std::size_t open_count_ = 0;
  std::error_code open_error_;
  std::error_code read_error_;
  std::error_code write_error_;
  std::error_code delayed_write_error_;
  std::size_t writes_before_failure_ = 0;
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
mock_stream_control::hold_next_open ()
{
  impl_->hold_next_open ();
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
mock_stream_control::fail_write_after (std::size_t writes,
                                       std::error_code error)
{
  impl_->fail_write_after (writes, error);
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

void
mock_stream_control::on_write (write_observer_t observer)
{
  impl_->on_write (std::move (observer));
}

void
mock_stream_control::feed_after (std::chrono::milliseconds delay,
                                 std::vector<std::uint8_t> const &bytes)
{
  impl_->feed_after (delay, bytes);
}

void
mock_stream_control::set_read_chunk (std::size_t bytes)
{
  impl_->set_read_chunk (bytes);
}

void
mock_stream_control::hold_writes ()
{
  impl_->hold_writes ();
}

void
mock_stream_control::release_writes ()
{
  impl_->release_writes ();
}

void
mock_stream_control::complete_next_read_empty ()
{
  impl_->complete_next_read_empty ();
}

mock_stream
make_mock_stream (executor_type const &executor, mock_options_t const &options)
{
  std::shared_ptr<mock_stream_impl> const impl
      = std::make_shared<mock_stream_impl> (executor, options);
  mock_stream result
      = { stream::byte_stream (impl), mock_stream_control (impl) };
  return result;
}

mock_stream
make_mock_stream (executor_type const &executor, std::string const &name)
{
  mock_options_t options;
  options.name = name;
  return make_mock_stream (executor, options);
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

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

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"

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
namespace stream
{
namespace
{
using error::transport_errc;
typedef std::shared_ptr<stream_impl> impl_ptr;

// Each posted step is a small function object: C++11 lambdas cannot move a
// move-only handler into their capture.
struct post_open
{
  impl_ptr impl;
  status_handler_t handler;
  void operator() ();
};

struct post_read_some
{
  impl_ptr impl;
  mutable_bytes_t buffer;
  io_handler_t handler;
  void operator() ();
};

struct post_write_some
{
  impl_ptr impl;
  const_bytes_t data;
  io_handler_t handler;
  void operator() ();
};

} // namespace

// The access functions of the posted steps (they call protected members).
struct stream_impl_access
{
  static void
  open (stream_impl &impl, status_handler_t handler)
  {
    impl.do_open (std::move (handler));
  }
  static void
  read_some (stream_impl &impl, mutable_bytes_t buffer, io_handler_t handler)
  {
    if (!impl.is_open ())
      {
        impl.complete (std::move (handler), transport_errc::not_open, 0);
        return;
      }
    impl.do_read_some (buffer, std::move (handler));
  }
  static void
  write_some (stream_impl &impl, const_bytes_t data, io_handler_t handler)
  {
    if (!impl.is_open ())
      {
        impl.complete (std::move (handler), transport_errc::not_open, 0);
        return;
      }
    impl.do_write_some (data, std::move (handler));
  }
  static void
  write_more (stream_impl &impl, const_bytes_t data, io_handler_t handler)
  {
    impl.do_write_some (data, std::move (handler));
  }
  static std::size_t
  cancel_epoch (stream_impl const &impl)
  {
    return impl.cancel_epoch_;
  }
  static void
  cancel (stream_impl &impl)
  {
    ++impl.cancel_epoch_;
    impl.do_cancel ();
  }
  static void
  close (stream_impl &impl)
  {
    ++impl.cancel_epoch_;
    impl.do_close ();
    impl.set_open (false);
  }
  static void
  complete (stream_impl &impl, io_handler_t handler, std::error_code error,
            std::size_t bytes)
  {
    impl.complete (std::move (handler), error, bytes);
  }
  static detail::mark_registry &
  marks (stream_impl &impl) LUMEX_NOEXCEPT
  {
    return impl.marks_;
  }
};

namespace
{
void
post_open::operator() ()
{
  stream_impl_access::open (*impl, std::move (handler));
}

void
post_read_some::operator() ()
{
  stream_impl_access::read_some (*impl, buffer, std::move (handler));
}

void
post_write_some::operator() ()
{
  stream_impl_access::write_some (*impl, data, std::move (handler));
}

struct post_cancel
{
  impl_ptr impl;
  void
  operator() ()
  {
    stream_impl_access::cancel (*impl);
  }
};

struct post_close
{
  impl_ptr impl;
  void
  operator() ()
  {
    stream_impl_access::close (*impl);
  }
};

// The cancellation of one operation, on the strand: nothing when its
// handler ran since the signal (the mark is gone). A signal emitted before
// the operation settled acts, also when the result was handed on since.
struct post_cancel_operation
{
  impl_ptr impl;
  std::weak_ptr<detail::operation_mark> operation;
  void
  operator() ()
  {
    if (detail::take_cancellation_request (operation))
      stream_impl_access::cancel (*impl);
  }
};

// The handler the stream puts in the cancellation slot of an operation's
// handler. Only a terminal cancellation is supported: it cancels the stream
// (every pending operation), a side effect partial and total do not permit.
// It runs on the thread that emits the signal and only posts to the strand,
// and only while the operation is pending: not once complete () settled it.
class terminal_cancellation
{
public:
  terminal_cancellation (impl_ptr const &impl,
                         detail::operation_mark_ptr const &operation)
      : impl_ (impl), operation_ (operation)
  {
  }

  void
  operator() (asio::cancellation_type_t type)
  {
    if ((type & asio::cancellation_type::terminal)
        == asio::cancellation_type::none)
      return;
    impl_ptr const impl = impl_.lock ();
    if (!impl || !detail::request_cancellation (operation_))
      return;
    try
      {
        post_cancel_operation step = { impl, operation_ };
        asio::post (impl->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: the operation is not cancelled; a later signal may
        // try again.
        detail::take_cancellation_request (operation_);
      }
  }

private:
  std::weak_ptr<stream_impl> impl_;
  std::weak_ptr<detail::operation_mark> operation_;
};

// Puts the stream's slot handler in the cancellation slot of the erased
// handler (Asio chains it to the slot of the user's handler), and remembers
// the mark by that slot for complete ().
template <typename Handler>
void
take_cancellation (impl_ptr const &impl, Handler &handler,
                   detail::operation_mark_ptr const &mark)
{
  if (!mark)
    return;
  asio::cancellation_slot slot
      = asio::get_associated_cancellation_slot (handler);
  if (!slot.is_connected ())
    return;
  slot.template emplace<terminal_cancellation> (impl, mark);
  stream_impl_access::marks (*impl).watch (slot, mark);
}
} // namespace

namespace
{
// Writes the whole buffer with repeated write_some on the strand. A cancel
// or close between two writes (no device write pending, so do_cancel ends
// nothing) is seen through the cancel epoch taken at the start.
struct write_all_op
{
  impl_ptr impl;
  const_bytes_t data;
  std::size_t written;
  io_handler_t handler;
  std::size_t epoch;

  void start ();
  void step ();
};

// The completion of one write_some of a write_all; it runs on the strand
// (its associated executor), then starts the next write or completes.
struct write_all_next
{
  write_all_op op;

  void
  operator() (std::error_code error, std::size_t bytes)
  {
    op.written += bytes;
    if (error)
      {
        stream_impl_access::complete (*op.impl, std::move (op.handler), error,
                                      op.written);
        return;
      }
    op.step ();
  }

  typedef stream_impl::strand_type executor_type;
  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return op.impl->strand ();
  }
};

void
write_all_op::start ()
{
  if (!impl->is_open ())
    {
      stream_impl_access::complete (*impl, std::move (handler),
                                    transport_errc::not_open, written);
      return;
    }
  epoch = stream_impl_access::cancel_epoch (*impl);
  step ();
}

void
write_all_op::step ()
{
  if (written == data.size ())
    {
      stream_impl_access::complete (*impl, std::move (handler),
                                    std::error_code (), written);
      return;
    }
  if (epoch != stream_impl_access::cancel_epoch (*impl))
    {
      stream_impl_access::complete (*impl, std::move (handler),
                                    transport_errc::cancelled, written);
      return;
    }
  impl_ptr const keep = impl;
  const_bytes_t const rest (data.data () + written, data.size () - written);
  write_all_next next = { std::move (*this) };
  stream_impl_access::write_more (*keep, rest,
                                  io_handler_t (std::move (next)));
}
struct post_write_all
{
  write_all_op op;
  void
  operator() ()
  {
    op.start ();
  }
};
} // namespace

stream_impl::stream_impl (executor_type const &executor, std::string name)
    : executor_ (executor), strand_ (asio::make_strand (executor)),
      name_ (std::move (name)), open_ (false),
      counters_ (std::make_shared<statistics::statistics_counters> ()),
      cancel_epoch_ (0)
{
}

stream_impl::~stream_impl () {}

// The handlers arrive already wrapped in the counting handler (see the
// start_* templates of the header); write_all counts once, at the end,
// however many writes it took. The slot handler goes in before the post, on
// the thread that starts the operation, as Asio's own operations do.
void
stream_impl::enqueue_open (status_handler_t handler,
                           detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_open step = { self, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
stream_impl::enqueue_read_some (mutable_bytes_t buffer, io_handler_t handler,
                                detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_read_some step = { self, buffer, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
stream_impl::enqueue_write_some (const_bytes_t data, io_handler_t handler,
                                 detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_write_some step = { self, data, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
stream_impl::enqueue_write_all (const_bytes_t data, io_handler_t handler,
                                detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_write_all step = { { self, data, 0, std::move (handler), 0 } };
  asio::post (strand_, std::move (step));
}

void
stream_impl::start_cancel ()
{
  post_cancel step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
stream_impl::start_close ()
{
  post_close step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

// Posted through the strand: asio::post then runs the handler on its own
// associated executor (any_completion_executor cannot be posted to directly),
// and never inside the call that started the operation.
// The result is fixed here: the mark of the operation is settled first, so a
// signal emitted while the handler waits on its executor ends nothing.
void
stream_impl::complete (status_handler_t handler, std::error_code error)
{
  marks_.settle (handler);
  asio::post (strand_, asio::append (std::move (handler), error));
}

void
stream_impl::complete (io_handler_t handler, std::error_code error,
                       std::size_t bytes)
{
  marks_.settle (handler);
  asio::post (strand_, asio::append (std::move (handler), error, bytes));
}

// --- byte_stream
// -------------------------------------------------------------

byte_stream::byte_stream (std::shared_ptr<stream_impl> impl)
    : impl_ (std::move (impl))
{
}

byte_stream::~byte_stream ()
{
  if (impl_)
    impl_->start_close ();
}

byte_stream::byte_stream (byte_stream &&other) LUMEX_NOEXCEPT
    : impl_ (std::move (other.impl_))
{
}

byte_stream &
byte_stream::operator= (byte_stream &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      if (impl_)
        impl_->start_close ();
      impl_ = std::move (other.impl_);
    }
  return *this;
}

executor_type
byte_stream::get_executor () const LUMEX_NOEXCEPT
{
  return impl_->executor ();
}

std::string const &
byte_stream::name () const LUMEX_NOEXCEPT
{
  return impl_->name ();
}

bool
byte_stream::is_open () const LUMEX_NOEXCEPT
{
  return impl_ && impl_->is_open ();
}

statistics::statistics_t
byte_stream::statistics () const LUMEX_NOEXCEPT
{
  if (!impl_)
    return statistics::statistics_t ();
  return impl_->counters ().snapshot ();
}

void
byte_stream::reset_statistics () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->counters ().reset ();
}

void
byte_stream::cancel () LUMEX_NOEXCEPT
{
  try
    {
      impl_->start_cancel ();
    }
  catch (...)
    {
    }
}

void
byte_stream::close () LUMEX_NOEXCEPT
{
  try
    {
      impl_->start_close ();
    }
  catch (...)
    {
    }
}

status_result_t
byte_stream::open (deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_open (detail::sync_status_handler{ state });
  std::error_code const error
      = detail::wait (*state, deadline, *this, impl_->counters ()).error;
  if (error)
    return status_result_t (lumex::core::expected::result::unexpect, error);
  return status_result_t ();
}

io_result_t
byte_stream::read_some (mutable_bytes_t buffer, deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return io_result_t (lumex::core::expected::result::unexpect,
                          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_read_some (buffer, detail::sync_io_handler{ state });
  detail::sync_outcome const outcome
      = detail::wait (*state, deadline, *this, impl_->counters ());
  if (outcome.error)
    return io_result_t (lumex::core::expected::result::unexpect,
                        outcome.error);
  return io_result_t (outcome.bytes);
}

io_result_t
byte_stream::write_some (const_bytes_t data, deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return io_result_t (lumex::core::expected::result::unexpect,
                          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_write_some (data, detail::sync_io_handler{ state });
  detail::sync_outcome const outcome
      = detail::wait (*state, deadline, *this, impl_->counters ());
  if (outcome.error)
    return io_result_t (lumex::core::expected::result::unexpect,
                        outcome.error);
  return io_result_t (outcome.bytes);
}

status_result_t
byte_stream::write_all (const_bytes_t data, deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_write_all (data, detail::sync_io_handler{ state });
  std::error_code const error
      = detail::wait (*state, deadline, *this, impl_->counters ()).error;
  if (error)
    return status_result_t (lumex::core::expected::result::unexpect, error);
  return status_result_t ();
}
} // namespace stream
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

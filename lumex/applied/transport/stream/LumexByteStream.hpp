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
 * @file LumexByteStream.hpp
 * @brief `byte_stream`: a stream of bytes (serial port, TCP connection, mock)
 * with a synchronous API bounded by deadlines and an asynchronous API for any
 * completion token.
 * @details A `byte_stream` is a handle to a shared implementation
 * (`stream_impl`) that runs every operation, `cancel ()` and `close ()` on its
 * own strand of the given executor; both may be called from any thread.
 * Pending operations end with `transport_errc::cancelled`. A completion
 * handler is never called inside the call that starts the operation; it runs
 * on its associated executor (by default the strand). Handlers keep the
 * implementation alive, so the `byte_stream` may be destroyed at any time,
 * also inside a handler: the destructor closes and returns at once.
 *
 * A synchronous call starts the asynchronous operation and waits for it. On
 * the deadline it cancels the operation, waits for its completion (which the
 * strand delivers promptly) and returns `transport_errc::timeout`. It needs a
 * thread that runs the executor (an `io_runner`); called on such a thread it
 * would wait for itself and returns `transport_errc::would_deadlock` at once.
 * That check covers `io_context`, `thread_pool` and strands of them; for
 * another executor type, do not call the synchronous API from its threads.
 *
 * Every operation is counted in `stream_impl`, so each transport counts the
 * same; `byte_stream::statistics ()` returns the snapshot (see
 * `statistics/LumexTransportStatistics.hpp`).
 *
 * Per-operation cancellation: when the completion handler has a cancellation
 * slot (`asio::bind_cancellation_slot`, `asio::cancel_after`, a coroutine), a
 * `cancellation_type::terminal` emitted on it while the operation is pending
 * cancels the stream as `cancel ()` does: every operation pending on it ends
 * with `transport_errc::cancelled`, not only this one (terminal permits
 * that side effect), and the stream stays usable as after `cancel ()`.
 * `partial` and `total` alone are not supported and change nothing
 * (`cancellation_type::all` has the terminal bit).
 *
 * Mind what "every pending operation" means: a terminal cancellation of a
 * write also ends a read loop that runs concurrently on the same stream. The
 * operation counts as pending until its handler runs, so a signal that
 * arrives after the device completed it, but before its handler ran, still
 * cancels the other operations pending at that moment (the operation itself
 * completes with its result). Once the handler ran, a later signal reaches
 * nothing; the slot is left as Asio's own operations leave it. As with
 * Asio's objects, emit the signal on the handler's executor, not concurrently
 * with the completion of the operation.
 *
 * The timer of `asio::cancel_after` runs on the stream's strand (the executor
 * the initiations report). A handler bound to the bare executor of a runner
 * with several threads (or a coroutine spawned on it) would run beside that
 * timer, which Asio's `cancel_after` does not allow: bind such a handler to a
 * strand.
 *
 * The stream keeps no outstanding work on the executor associated with a
 * handler (a handler that keeps its owner alive, stored by a pending
 * operation, would keep that work forever and the destructor of its
 * io_context would never return). An io_context that only runs the handlers
 * (`asio::bind_executor (other, ...)`) may therefore run out of work and
 * return from `run ()` before the handler: keep an `executor_work_guard` on
 * it while an operation is pending. That executor must stay valid until the
 * handler ran or was destroyed.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_STREAM_BYTE_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_STREAM_BYTE_STREAM_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
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
namespace stream
{
struct stream_impl_access;

/**
 * @brief The implementation behind a `byte_stream`: a concrete transport
 * derives from it and implements the `do_*` functions, which always run on
 * the strand. A `do_*` operation completes its handler exactly once through
 * `complete`, never inside the `do_*` call itself.
 */
class stream_impl : public std::enable_shared_from_this<stream_impl>
{
public:
  typedef asio::strand<executor_type> strand_type;

  stream_impl (executor_type const &executor, std::string name);
  virtual ~stream_impl ();

  stream_impl (stream_impl const &) = delete;
  stream_impl &operator= (stream_impl const &) = delete;

  /// @brief The executor the stream was given.
  executor_type const &
  executor () const LUMEX_NOEXCEPT
  {
    return executor_;
  }

  /// @brief The strand every operation runs on.
  strand_type const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  /// @brief The name for logs ("COM3", "10.0.0.5:5000").
  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return name_;
  }

  /// @brief True between a successful open and a close (any thread).
  bool
  is_open () const LUMEX_NOEXCEPT
  {
    return open_.load (std::memory_order_acquire);
  }

  /// @brief The counters this stream counts in (see `statistics ()`).
  statistics::statistics_counters &
  counters () LUMEX_NOEXCEPT
  {
    return *counters_;
  }

  /// @brief The counters, for reading.
  statistics::statistics_counters const &
  counters () const LUMEX_NOEXCEPT
  {
    return *counters_;
  }

  // Called by byte_stream from any thread: each posts to the strand. The
  // start_* with a handler take the handler as the user gave it and wrap it
  // in the counting handler before the one type erasure (a counting wrapper
  // around an already erased handler would forward the erased handler's
  // allocator, which the outer erasure destroys first). With a cancellation
  // slot, the mark tells the enqueue_* to take its cancellation.
  template <typename Handler>
  void
  start_open (Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_open (status_handler_t (detail::count_with (
                      counters_, detail::open_recorder (),
                      std::forward<Handler> (handler), mark)),
                  mark);
  }

  template <typename Handler>
  void
  start_read_some (mutable_bytes_t buffer, Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_read_some (buffer,
                       io_handler_t (detail::count_with (
                           counters_, detail::read_recorder (),
                           std::forward<Handler> (handler), mark)),
                       mark);
  }

  template <typename Handler>
  void
  start_write_some (const_bytes_t data, Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_write_some (data,
                        io_handler_t (detail::count_with (
                            counters_, detail::write_recorder (),
                            std::forward<Handler> (handler), mark)),
                        mark);
  }

  /// @brief Counts as one write, however many writes it takes.
  template <typename Handler>
  void
  start_write_all (const_bytes_t data, Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_write_all (data,
                       io_handler_t (detail::count_with (
                           counters_, detail::write_recorder (),
                           std::forward<Handler> (handler), mark)),
                       mark);
  }

  void start_cancel ();
  void start_close ();

protected:
  /// @brief Opens; completes with an error or success.
  virtual void do_open (status_handler_t handler) = 0;
  /// @brief Reads at least one byte, or completes with an error.
  virtual void do_read_some (mutable_bytes_t buffer, io_handler_t handler) = 0;
  /// @brief Writes at least one byte, or completes with an error.
  virtual void do_write_some (const_bytes_t data, io_handler_t handler) = 0;
  /// @brief Ends the pending operations with `cancelled`.
  virtual void do_cancel () = 0;
  /// @brief Ends the pending operations and releases the device.
  virtual void do_close () = 0;

  /// @brief Marks the stream open or closed for `is_open`.
  void
  set_open (bool open) LUMEX_NOEXCEPT
  {
    open_.store (open, std::memory_order_release);
  }

  /// @brief Calls `handler` with the result on its associated executor.
  void complete (status_handler_t handler, std::error_code error);
  /// @brief Calls `handler` with the result on its associated executor.
  void complete (io_handler_t handler, std::error_code error,
                 std::size_t bytes);

private:
  // The posted steps of the .cpp reach the protected members through it.
  friend struct stream_impl_access;

  // With a mark, each first puts the slot handler of the stream in the
  // cancellation slot of the erased handler.
  void enqueue_open (status_handler_t handler,
                     detail::operation_mark_ptr const &mark);
  void enqueue_read_some (mutable_bytes_t buffer, io_handler_t handler,
                          detail::operation_mark_ptr const &mark);
  void enqueue_write_some (const_bytes_t data, io_handler_t handler,
                           detail::operation_mark_ptr const &mark);
  void enqueue_write_all (const_bytes_t data, io_handler_t handler,
                          detail::operation_mark_ptr const &mark);

  executor_type executor_;
  strand_type strand_;
  std::string name_;
  std::atomic<bool> open_;
  std::shared_ptr<statistics::statistics_counters> counters_;
};

/// @brief A byte stream (see the file description).
class byte_stream
{
  // The initiating functions of the asynchronous operations (declared first:
  // the return types of the async_* members name them). Each carries the
  // stream's strand, which tokens such as asio::cancel_after ask for: their
  // timer then runs on the strand, never beside the completion.
  struct initiate_base
  {
    typedef transport::executor_type executor_type;

    explicit initiate_base (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

  private:
    executor_type executor_;
  };

  struct initiate_open : initiate_base
  {
    explicit initiate_open (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl) const
    {
      impl->start_open (std::forward<Handler> (handler));
    }
  };

  struct initiate_read_some : initiate_base
  {
    explicit initiate_read_some (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                mutable_bytes_t buffer) const
    {
      impl->start_read_some (buffer, std::forward<Handler> (handler));
    }
  };

  struct initiate_write_some : initiate_base
  {
    explicit initiate_write_some (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                const_bytes_t data) const
    {
      impl->start_write_some (data, std::forward<Handler> (handler));
    }
  };

  struct initiate_write_all : initiate_base
  {
    explicit initiate_write_all (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                const_bytes_t data) const
    {
      impl->start_write_all (data, std::forward<Handler> (handler));
    }
  };

public:
  /// @brief A stream over a concrete implementation.
  explicit byte_stream (std::shared_ptr<stream_impl> impl);

  /// @brief Closes (asynchronously) and returns; pending handlers still run.
  ~byte_stream ();

  byte_stream (byte_stream const &) = delete;
  byte_stream &operator= (byte_stream const &) = delete;
  byte_stream (byte_stream &&other) LUMEX_NOEXCEPT;
  byte_stream &operator= (byte_stream &&other) LUMEX_NOEXCEPT;

  /// @brief The executor the stream was given.
  executor_type get_executor () const LUMEX_NOEXCEPT;

  /// @brief The name for logs.
  std::string const &name () const LUMEX_NOEXCEPT;

  /// @brief True between a successful open and a close.
  bool is_open () const LUMEX_NOEXCEPT;

  /// @brief A snapshot of the counters (any thread; no lock).
  statistics::statistics_t statistics () const LUMEX_NOEXCEPT;

  /// @brief Sets every counter to zero (any thread).
  void reset_statistics () LUMEX_NOEXCEPT;

  // --- synchronous
  // -----------------------------------------------------------

  /// @brief Opens within the deadline.
  status_result_t open (deadline_t deadline);

  /// @brief Reads at least one byte within the deadline.
  io_result_t read_some (mutable_bytes_t buffer, deadline_t deadline);

  /// @brief Writes at least one byte within the deadline.
  io_result_t write_some (const_bytes_t data, deadline_t deadline);

  /// @brief Writes all of `data` within the deadline.
  status_result_t write_all (const_bytes_t data, deadline_t deadline);

  // --- asynchronous
  // ----------------------------------------------------------

  // The return types name every argument as the body passes it (`impl_` is
  // an lvalue): a token such as asio::deferred keeps the argument types in
  // the type it returns.

  /// @brief Opens; the token gets `void (std::error_code)`.
  template <typename Token>
  auto
  async_open (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_open> (), token,
          std::declval<std::shared_ptr<stream_impl> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_open (impl_->strand ()), token, impl_);
  }

  /// @brief Reads at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_read_some (mutable_bytes_t buffer, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_read_some> (), token,
          std::declval<std::shared_ptr<stream_impl> &> (), buffer))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_read_some (impl_->strand ()), token, impl_, buffer);
  }

  /// @brief Writes at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_write_some (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_write_some> (), token,
          std::declval<std::shared_ptr<stream_impl> &> (), data))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_write_some (impl_->strand ()), token, impl_, data);
  }

  /// @brief Writes all of `data`; `void (std::error_code, std::size_t)` with
  /// the bytes written before an error.
  template <typename Token>
  auto
  async_write_all (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_write_all> (), token,
          std::declval<std::shared_ptr<stream_impl> &> (), data))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_write_all (impl_->strand ()), token, impl_, data);
  }

  /// @brief Ends the pending operations with `cancelled` (any thread).
  void cancel () LUMEX_NOEXCEPT;

  /// @brief Closes (any thread, idempotent); pending operations end with
  /// `cancelled`, later ones with `not_open`.
  void close () LUMEX_NOEXCEPT;

  /// @brief The implementation, for adapters of the library.
  std::shared_ptr<stream_impl> const &
  impl () const LUMEX_NOEXCEPT
  {
    return impl_;
  }

private:
  std::shared_ptr<stream_impl> impl_;
};
} // namespace stream
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_STREAM_BYTE_STREAM_HPP

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
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
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

  // Called by byte_stream from any thread: each posts to the strand.
  void start_open (status_handler_t handler);
  void start_read_some (mutable_bytes_t buffer, io_handler_t handler);
  void start_write_some (const_bytes_t data, io_handler_t handler);
  void start_write_all (const_bytes_t data, io_handler_t handler);
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

  executor_type executor_;
  strand_type strand_;
  std::string name_;
  std::atomic<bool> open_;
};

/// @brief A byte stream (see the file description).
class byte_stream
{
  // The initiating functions of the asynchronous operations (declared first:
  // the return types of the async_* members name them).
  struct initiate_open
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl) const
    {
      impl->start_open (status_handler_t (std::forward<Handler> (handler)));
    }
  };

  struct initiate_read_some
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                mutable_bytes_t buffer) const
    {
      impl->start_read_some (buffer,
                             io_handler_t (std::forward<Handler> (handler)));
    }
  };

  struct initiate_write_some
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                const_bytes_t data) const
    {
      impl->start_write_some (data,
                              io_handler_t (std::forward<Handler> (handler)));
    }
  };

  struct initiate_write_all
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<stream_impl> impl,
                const_bytes_t data) const
    {
      impl->start_write_all (data,
                             io_handler_t (std::forward<Handler> (handler)));
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

  /// @brief Opens; the token gets `void (std::error_code)`.
  template <typename Token>
  auto
  async_open (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_open> (), token,
          std::declval<std::shared_ptr<stream_impl>> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_open (), token, impl_);
  }

  /// @brief Reads at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_read_some (mutable_bytes_t buffer, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_read_some> (), token,
          std::declval<std::shared_ptr<stream_impl>> (), buffer))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_read_some (), token, impl_, buffer);
  }

  /// @brief Writes at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_write_some (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_write_some> (), token,
          std::declval<std::shared_ptr<stream_impl>> (), data))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_write_some (), token, impl_, data);
  }

  /// @brief Writes all of `data`; `void (std::error_code, std::size_t)` with
  /// the bytes written before an error.
  template <typename Token>
  auto
  async_write_all (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_write_all> (), token,
          std::declval<std::shared_ptr<stream_impl>> (), data))
  {
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_write_all (), token, impl_, data);
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

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
 * @file LumexBasicFramed.hpp
 * @brief `basic_framed<Transport>`: what `framed_stream` and
 * `framed_datagram` share: the frame reads and writes (synchronous with
 * deadlines, asynchronous for any completion token), cancel, close, the
 * statistics and the access to the transport underneath.
 * @details Internal to the transport library; the contracts it implements
 * are documented in `LumexFramedStream.hpp`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_BASIC_FRAMED_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_BASIC_FRAMED_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/framed/detail/LumexFramedHub.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/contracts/LumexContracts"
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
namespace framed
{
namespace detail
{
/// @brief The name of an empty (moved-from) adapter.
std::string const &empty_framed_name () LUMEX_NOEXCEPT;

/// @brief The part every framed adapter shares (see the file description).
template <typename Transport> class basic_framed
{
protected:
  typedef framed_hub<Transport> hub_type;
  typedef typename hub_type::frame_type frame_type;
  typedef lumex::core::expected::result::expected<frame_type, std::error_code>
      read_result_type;

  // The initiating functions (declared first: the return types of the
  // async_* members name them). Each reports the strand, which tokens such as
  // asio::cancel_after ask for: their timer then runs on the strand, as for
  // the transports' initiations.
  struct initiate_base
  {
    typedef ::lumex::applied::transport::executor_type executor_type;

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

  struct initiate_read_frame : initiate_base
  {
    explicit initiate_read_frame (
        ::lumex::applied::transport::executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<hub_type> hub) const
    {
      if (!hub)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              frame_type ());
          return;
        }
      hub->start_read_frame (std::forward<Handler> (handler));
    }
  };

  struct initiate_write_frame : initiate_base
  {
    explicit initiate_write_frame (
        ::lumex::applied::transport::executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<hub_type> hub,
                std::uint8_t report_id, const_bytes_t payload) const
    {
      if (!hub)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open));
          return;
        }
      hub->start_write_frame (report_id, payload,
                              std::forward<Handler> (handler));
    }
  };

  // What the completion handler of a synchronous read leaves behind.
  struct sync_frame_state : ::lumex::applied::transport::detail::sync_state
  {
    frame_type frame;
  };

  struct sync_frame_handler
  {
    std::shared_ptr<sync_frame_state> state;

    void
    operator() (std::error_code error, frame_type frame)
    {
      std::lock_guard<std::mutex> const lock (state->mutex);
      state->error = error;
      state->frame = std::move (frame);
      state->done = true;
      state->done_signal.notify_all ();
    }
  };

public:
  /// @brief The transport underneath.
  typedef Transport transport_type;

  /// @brief The transport the adapter owns: open it, read its name and its
  /// statistics through it. Do not read from it or send on it while the
  /// adapter works, the adapter's pump would compete for the bytes. A
  /// moved-from adapter has none: calling this on it is a contract
  /// violation.
  Transport &
  transport () LUMEX_NOEXCEPT
  {
    LUMEX_CONTRACT_ASSERT (hub_ != nullptr);
    return hub_->transport ();
  }

  Transport const &
  transport () const LUMEX_NOEXCEPT
  {
    LUMEX_CONTRACT_ASSERT (hub_ != nullptr);
    return hub_->transport ();
  }

  /// @brief The executor the transport was given (an empty one when moved
  /// from).
  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return hub_ ? hub_->transport ().get_executor () : executor_type ();
  }

  /// @brief The strand of the adapter's operations.
  using strand_type = typename hub_type::strand_type;

  /// @brief The strand every operation of the adapter runs on (a copy).
  /// @details Start an operation that uses `asio::cancel_after` and whose
  /// handler has no associated executor of its own inside this strand: from
  /// a handler that runs on it or through `asio::post (x.get_strand (),
  /// ...)`. Asio requires it, because the timer of `cancel_after` runs on
  /// this strand while the initiation installs the cancellation slot.
  /// Not for a moved-from object.
  strand_type
  get_strand () const LUMEX_NOEXCEPT
  {
    return hub_->strand ();
  }

  /// @brief The name for logs (the transport's).
  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return hub_ ? hub_->transport ().name () : empty_framed_name ();
  }

  /// @brief True while the transport is open.
  bool
  is_open () const LUMEX_NOEXCEPT
  {
    return hub_ && hub_->transport ().is_open ();
  }

  /// @brief A snapshot of the counters of the adapter (any thread; no lock).
  framed_statistics_t
  statistics () const LUMEX_NOEXCEPT
  {
    return hub_ ? hub_->counters ().snapshot () : framed_statistics_t ();
  }

  /// @brief A snapshot of the counters of the transport underneath.
  statistics::statistics_t
  transport_statistics () const LUMEX_NOEXCEPT
  {
    return hub_ ? hub_->transport ().statistics ()
                : statistics::statistics_t ();
  }

  /// @brief Sets the counters of the adapter and of the transport to zero.
  void
  reset_statistics () LUMEX_NOEXCEPT
  {
    if (!hub_)
      return;
    hub_->counters ().reset ();
    hub_->transport ().reset_statistics ();
  }

  /// @brief How many decoded frames wait in the queue (any thread).
  std::size_t
  queued_frames () const LUMEX_NOEXCEPT
  {
    return hub_ ? hub_->queued () : 0;
  }

  /// @brief Begins decoding now, without a reader: the frames wait in the
  /// queue. A `read_frame` begins it too. No effect while it runs.
  void
  start ()
  {
    if (hub_)
      hub_->start_pump ();
  }

  /**
   * @brief Reads the next frame within the deadline.
   * @return The frame; `timeout` at the deadline (every pending operation of
   * the adapter is cancelled, as for a transport); the error that ended the
   * transport's reading (`disconnected`, `not_open`, `closed`, ...);
   * `cancelled` after `cancel ()` or `close ()`; `would_deadlock` on a thread
   * of the adapter's executor.
   */
  read_result_type
  read_frame (deadline_t deadline)
  {
    if (!hub_)
      return read_result_type (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::not_open));
    if (refuse_sync ())
      return read_result_type (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::would_deadlock));
    std::shared_ptr<sync_frame_state> const state
        = std::make_shared<sync_frame_state> ();
    sync_frame_handler handler = { state };
    hub_->start_read_frame (std::move (handler));
    std::error_code const error
        = ::lumex::applied::transport::detail::wait (*state, deadline, *this,
                                                     hub_->counters ())
              .error;
    if (error)
      return read_result_type (lumex::core::expected::result::unexpect, error);
    return read_result_type (std::move (state->frame));
  }

  /// @brief Writes the frame of `payload` within the deadline: encode, then
  /// write the whole frame. `payload_too_large` and the other refusals of the
  /// codec come back at once; a frame already begun when the deadline passes
  /// may be partly on the wire (the receiver resynchronizes).
  status_result_t
  write_frame (const_bytes_t payload, deadline_t deadline)
  {
    return write_frame_as (0, payload, deadline);
  }

  /// @brief Reads the next frame; the token gets
  /// `void (std::error_code, frame)`. The decoder begins with the first read.
  template <typename Token>
  auto
  async_read_frame (Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, frame_type)> (
          std::declval<initiate_read_frame> (), token,
          std::declval<std::shared_ptr<hub_type> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code, frame_type)> (
        initiate_read_frame (initiation_executor ()), token, hub_);
  }

  /// @brief Writes the frame of `payload`; the token gets
  /// `void (std::error_code)`. `payload` must stay valid until the handler
  /// ran. Writes are sent one after the other in the order they were started.
  template <typename Token>
  auto
  async_write_frame (const_bytes_t payload, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_write_frame> (), token,
          std::declval<std::shared_ptr<hub_type> &> (),
          std::declval<std::uint8_t> (), std::declval<const_bytes_t &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_write_frame (initiation_executor ()), token, hub_,
        std::uint8_t (0), payload);
  }

  /// @brief Ends the pending reads and writes with `cancelled` (any thread).
  /// The adapter stays usable; the decoder goes on and the queue is kept.
  void
  cancel () LUMEX_NOEXCEPT
  {
    if (!hub_)
      return;
    try
      {
        hub_->start_cancel ();
      }
    catch (...)
      {
      }
  }

  /// @brief Ends the pending reads and writes with `cancelled`, forgets the
  /// queued frames and the partial frame of the decoder and closes the
  /// transport (any thread, idempotent).
  void
  close () LUMEX_NOEXCEPT
  {
    if (!hub_)
      return;
    try
      {
        hub_->start_close ();
      }
    catch (...)
      {
      }
  }

protected:
  explicit basic_framed (std::shared_ptr<hub_type> hub)
      : hub_ (std::move (hub))
  {
  }

  /// @brief Closes (asynchronously) and returns; pending handlers still run
  /// (with `cancelled`).
  ~basic_framed () { release (); }

  basic_framed (basic_framed const &) = delete;
  basic_framed &operator= (basic_framed const &) = delete;

  basic_framed (basic_framed &&other) LUMEX_NOEXCEPT
      : hub_ (std::move (other.hub_))
  {
  }

  basic_framed &
  operator= (basic_framed &&other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      {
        release ();
        hub_ = std::move (other.hub_);
      }
    return *this;
  }

  /// @brief What the initiations report: the strand of the adapter (the
  /// system executor for a moved-from adapter).
  executor_type
  initiation_executor () const
  {
    if (!hub_)
      return executor_type (asio::system_executor ());
    return executor_type (hub_->strand ());
  }

  /// @brief The synchronous write with a report ID.
  status_result_t
  write_frame_as (std::uint8_t report_id, const_bytes_t payload,
                  deadline_t deadline)
  {
    if (!hub_)
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::not_open));
    if (refuse_sync ())
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::would_deadlock));
    std::shared_ptr<::lumex::applied::transport::detail::sync_state> const
        state
        = std::make_shared<::lumex::applied::transport::detail::sync_state> ();
    ::lumex::applied::transport::detail::sync_status_handler handler
        = { state };
    hub_->start_write_frame (report_id, payload, std::move (handler));
    std::error_code const error
        = ::lumex::applied::transport::detail::wait (*state, deadline, *this,
                                                     hub_->counters ())
              .error;
    if (error)
      return status_result_t (lumex::core::expected::result::unexpect, error);
    return status_result_t ();
  }

  std::shared_ptr<hub_type> hub_;

private:
  // A synchronous call from a thread that runs the adapter's executor would
  // wait for itself. The refusal is not counted: neither the adapter's
  // framing counters nor the transport's (it never reached an operation).
  bool
  refuse_sync () const
  {
    return ::lumex::applied::transport::detail::runs_on (
        hub_->transport ().get_executor ());
  }

  void
  release () LUMEX_NOEXCEPT
  {
    if (!hub_)
      return;
    try
      {
        hub_->start_close ();
      }
    catch (...)
      {
      }
  }
};
} // namespace detail
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_BASIC_FRAMED_HPP

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
 * @file LumexFramedHub.hpp
 * @brief `framed_hub<Transport>`: the shared implementation behind
 * `framed_stream` and `framed_datagram`. It owns the transport, the decoder
 * pump, the frame queue, the waiting readers, the write queue and the
 * counters.
 * @details Internal to the transport library. Everything runs on the strand
 * of the transport underneath (`transport.impl ()->strand ()`), so the pump
 * handlers of the transport, the codec and the queue need no lock and no
 * extra hop. A user's handler is never called inside the hub: its result is
 * posted through the strand to the handler's associated executor.
 *
 * The decoder pump: the first `read_frame` (or `start`) begins a loop of
 * `async_read_some` / `async_receive` on the transport; every chunk goes to
 * the codec on the strand, and every frame it delivers goes to the oldest
 * waiting reader or into the bounded queue. A read that ends with an error
 * other than `cancelled` ends the loop and completes every waiting reader
 * with it; the next `read_frame` starts the loop again. A `cancelled` read
 * of an open transport (a write was cancelled, which cancels everything
 * pending on the transport) restarts the loop at once.
 *
 * The pump handlers and the write handlers hold the hub weakly (the
 * transport, owned by the hub, keeps them while its operation is pending) and
 * the buffer they fill or send by `shared_ptr`.
 *
 * Per-operation cancellation follows the transports (`stream::stream_impl`):
 * a terminal cancellation on the slot of a pending operation's handler does
 * what `cancel ()` does; a signal emitted after the hub handed the result on
 * ends nothing (`mark_registry`).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_HUB_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_HUB_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
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
/// @brief What differs between the transports a hub can sit on.
template <typename Transport> struct hub_traits;

template <> struct hub_traits<stream::byte_stream>
{
  /// What a frame is for the reader.
  typedef frame_t frame_type;
  /// The erased completion of a frame read.
  typedef frame_handler_t read_handler_type;
  /// The component of the log records.
  static char const *
  component () LUMEX_NOEXCEPT
  {
    return "transport.framed.stream";
  }
  /// True when the codec starts afresh with every received message.
  static bool
  per_message () LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <> struct hub_traits<datagram::message_transport>
{
  typedef datagram::message_t frame_type;
  typedef datagram::message_handler_t read_handler_type;
  static char const *
  component () LUMEX_NOEXCEPT
  {
    return "transport.framed.datagram";
  }
  static bool
  per_message () LUMEX_NOEXCEPT
  {
    return true;
  }
};

/// @brief The hub of a framed adapter over `Transport` (see the file
/// description).
template <typename Transport>
class framed_hub : public ::lumex::applied::transport::detail::handler_keeper,
                   public std::enable_shared_from_this<framed_hub<Transport>>
{
public:
  typedef hub_traits<Transport> traits_type;
  typedef typename traits_type::frame_type frame_type;
  typedef typename traits_type::read_handler_type read_handler_type;
  typedef asio::strand<executor_type> strand_type;
  typedef core::framing::codec::bytes_t bytes_t;
  typedef core::framing::codec::bytes_view_t bytes_view_t;

  /// @brief A hub over `transport` (not empty, moved from).
  framed_hub (Transport transport, framed_options_t const &options);
  ~framed_hub () override;

  framed_hub (framed_hub const &) = delete;
  framed_hub &operator= (framed_hub const &) = delete;

  /// @brief The transport the hub owns.
  Transport &
  transport () LUMEX_NOEXCEPT
  {
    return transport_;
  }

  Transport const &
  transport () const LUMEX_NOEXCEPT
  {
    return transport_;
  }

  /// @brief The strand every operation runs on (the transport's).
  strand_type const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  /// @brief The `io_context` is being destroyed: the pending reads and
  /// writes die uncalled (a reader whose handler owns the adapter would
  /// otherwise keep it for ever).
  void drop_kept_handlers () LUMEX_NOEXCEPT override;

  /// @brief The counters (any thread).
  framed_counters &
  counters () LUMEX_NOEXCEPT
  {
    return counters_;
  }

  framed_counters const &
  counters () const LUMEX_NOEXCEPT
  {
    return counters_;
  }

  /// @brief How many decoded frames wait in the queue (any thread).
  std::size_t
  queued () const LUMEX_NOEXCEPT
  {
    return queued_.load (std::memory_order_acquire);
  }

  // --- from any thread: each posts to the strand --------------------------

  /// @brief Reads the next frame; the handler takes the handler as the user
  /// gave it and erases it once, behind `shielded_handler`.
  template <typename Handler>
  void
  start_read_frame (Handler &&handler)
  {
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    enqueue_read (
        ::lumex::applied::transport::detail::erase_once<read_handler_type> (
            std::forward<Handler> (handler), mark),
        mark);
  }

  /// @brief Writes the frame of `payload` (with `report_id` for a message
  /// transport); the handler gets `void (std::error_code)`. `payload` must
  /// stay valid until the handler ran.
  template <typename Handler>
  void
  start_write_frame (std::uint8_t report_id, const_bytes_t payload,
                     Handler &&handler)
  {
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    enqueue_write (
        report_id, payload,
        ::lumex::applied::transport::detail::erase_once<status_handler_t> (
            std::forward<Handler> (handler), mark),
        mark);
  }

  /// @brief Begins the decoder pump (no effect while it runs).
  void start_pump ();
  /// @brief Ends the pending reads and writes with `cancelled`; the hub
  /// stays usable.
  void start_cancel ();
  /// @brief Ends the pending reads and writes with `cancelled`, clears the
  /// queue and the decoder and closes the transport.
  void start_close ();
  /// @brief Posts the cancellation a signal requested (see
  /// `terminal_cancellation` in the .cpp); the step acts only while the
  /// operation of `operation` is pending.
  void request_operation_cancel (
      std::weak_ptr<::lumex::applied::transport::detail::operation_mark>
          operation);

  // --- on the strand (the posted steps and the transport handlers) -------

  void enqueue_read (
      read_handler_type handler,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark);
  void enqueue_write (
      std::uint8_t report_id, const_bytes_t payload, status_handler_t handler,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark);

  void run_read (read_handler_type handler);
  void run_write (std::uint8_t report_id, const_bytes_t payload,
                  status_handler_t handler);
  void run_pump ();
  /// @brief `start ()` on the strand: begins the pump, or remembers the wish
  /// while a read of a closed generation is still pending.
  void run_start ();
  void cancel_all ();
  void close_all ();

  /// @brief A read of the transport ended: `error` and the bytes of a stream
  /// chunk or of one datagram; `generation` tells a read that began before a
  /// close of the adapter. A close of the transport itself, also one followed
  /// by an open before this runs, shows in the transport's close count
  /// (`connection_epoch`) against the one the hub saw last.
  void pump_data (std::error_code error, bytes_view_t data,
                  std::uint8_t report_id, std::size_t generation);
  /// @brief The write `id` ended.
  void write_done (std::size_t id, std::error_code error);

  /// @brief The generation of the pump now (bumped by a close).
  std::size_t
  generation () const LUMEX_NOEXCEPT
  {
    return generation_;
  }

  /// @brief The buffer a read of a stream fills (none for a message
  /// transport).
  std::shared_ptr<bytes_t> const &
  chunk () const LUMEX_NOEXCEPT
  {
    return chunk_;
  }

protected:
  /// @brief Frames `payload` into `wire` with the codec.
  virtual std::error_code do_encode (bytes_view_t payload, bytes_t &wire) = 0;
  /// @brief Feeds a chunk to the codec; the codec's sink calls `decoded`.
  virtual void do_decode (bytes_view_t chunk) = 0;
  /// @brief Forgets the partial frame of the codec.
  virtual void do_reset () LUMEX_NOEXCEPT = 0;

  /// @brief The sink of the codec: a frame (empty `error`) or the error of a
  /// dropped frame.
  void decoded (bytes_view_t frame, std::error_code error);

private:
  struct pending_write
  {
    std::shared_ptr<bytes_t> wire;
    std::size_t size;
    std::uint8_t report_id;
    status_handler_t handler;
  };

  void enlist ();
  void deliver (frame_type frame);
  void complete (read_handler_type handler, std::error_code error,
                 frame_type frame);
  void complete (status_handler_t handler, std::error_code error);
  void fail_waiters (std::error_code error);
  void fail_writes (std::error_code error);
  void end_of_connection ();
  void start_next_write ();
  void note_dequeued () LUMEX_NOEXCEPT;
  void count_error (std::error_code const &error);
  frame_type make_frame (bytes_view_t bytes) const;

  Transport transport_;
  strand_type strand_;
  std::size_t bound_;
  std::string name_;
  framed_counters counters_;
  std::deque<frame_type> queue_;
  std::deque<read_handler_type> waiters_;
  std::deque<pending_write> writes_;
  pending_write active_;
  bool write_active_;
  std::size_t active_id_;
  std::size_t write_sequence_;
  bool pump_pending_;
  bool restart_;
  // The close count of the transport when the pump last looked: a change
  // means the connection the codec has seen is over.
  std::size_t connection_seen_;
  bool overflowing_;
  bool error_burst_;
  std::uint8_t report_id_;
  std::size_t generation_;
  std::size_t events_;
  std::shared_ptr<bytes_t> chunk_;
  std::atomic<std::size_t> queued_;
  // Set when the hub was enlisted for the io_context shutdown.
  std::atomic<bool> enlisted_;
  // The marks of the pending operations with a cancellation slot, settled by
  // complete (): a signal emitted after that ends nothing.
  ::lumex::applied::transport::detail::mark_registry marks_;
};

extern template class framed_hub<stream::byte_stream>;
extern template class framed_hub<datagram::message_transport>;
} // namespace detail
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_HUB_HPP

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
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/framed/detail/LumexBasicFramed.hpp"
#include "lumex/applied/transport/framed/detail/LumexFramedHub.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingErrc.hpp"

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
namespace framed
{
namespace detail
{
namespace
{
namespace tdetail = ::lumex::applied::transport::detail;
using core::framing::codec::framing_errc;
using error::transport_errc;

// --- the posted steps ------------------------------------------------------

template <typename Hub> struct read_step
{
  std::shared_ptr<Hub> hub;
  typename Hub::read_handler_type handler;
  void
  operator() ()
  {
    hub->run_read (std::move (handler));
  }
};

template <typename Hub> struct write_step
{
  std::shared_ptr<Hub> hub;
  std::uint8_t report_id;
  const_bytes_t payload;
  status_handler_t handler;
  void
  operator() ()
  {
    hub->run_write (report_id, payload, std::move (handler));
  }
};

template <typename Hub> struct pump_step
{
  std::shared_ptr<Hub> hub;
  void
  operator() ()
  {
    hub->run_start ();
  }
};

template <typename Hub> struct cancel_step
{
  std::shared_ptr<Hub> hub;
  void
  operator() ()
  {
    hub->cancel_all ();
  }
};

template <typename Hub> struct close_step
{
  std::shared_ptr<Hub> hub;
  void
  operator() ()
  {
    hub->close_all ();
  }
};

// The cancellation of one operation, on the strand: nothing when its handler
// ran or its result was handed on since the signal (the mark is gone or
// settled).
template <typename Hub> struct cancel_operation_step
{
  std::shared_ptr<Hub> hub;
  std::weak_ptr<tdetail::operation_mark> operation;
  void
  operator() ()
  {
    if (tdetail::take_cancellation_request (operation))
      hub->cancel_all ();
  }
};

// --- the cancellation slot -------------------------------------------------

// The handler the hub puts in the cancellation slot of an operation's
// handler. Only a terminal cancellation is supported: it cancels the hub
// (every pending operation), as it cancels a transport. It runs on the thread
// that emits the signal and only posts to the strand, and only while the
// operation is pending: not once complete () settled it.
template <typename Hub> class terminal_cancellation
{
public:
  terminal_cancellation (std::shared_ptr<Hub> const &hub,
                         tdetail::operation_mark_ptr const &operation)
      : hub_ (hub), operation_ (operation)
  {
  }

  void
  operator() (asio::cancellation_type_t type)
  {
    if ((type & asio::cancellation_type::terminal)
        == asio::cancellation_type::none)
      return;
    std::shared_ptr<Hub> const hub = hub_.lock ();
    if (!hub || !tdetail::request_cancellation (operation_))
      return;
    try
      {
        hub->request_operation_cancel (operation_);
      }
    catch (...)
      {
        // Out of memory: the operation is not cancelled; a later signal may
        // try again.
        tdetail::take_cancellation_request (operation_);
      }
  }

private:
  std::weak_ptr<Hub> hub_;
  std::weak_ptr<tdetail::operation_mark> operation_;
};

// Puts the hub's slot handler in the cancellation slot of the erased handler
// (Asio chains it to the slot of the user's handler), and remembers the mark
// by that slot for complete ().
template <typename Hub, typename Handler>
void
take_cancellation (std::shared_ptr<Hub> const &hub, Handler &handler,
                   tdetail::operation_mark_ptr const &mark,
                   tdetail::mark_registry &marks)
{
  if (!mark)
    return;
  asio::cancellation_slot slot
      = asio::get_associated_cancellation_slot (handler);
  if (!slot.is_connected ())
    return;
  slot.template emplace<terminal_cancellation<Hub>> (hub, mark);
  marks.watch (slot, mark);
}

// --- the handlers given to the transport ----------------------------------

// The completion of a read of a byte stream: runs on the strand.
template <typename Hub> struct stream_read_done
{
  typedef typename Hub::strand_type executor_type;
  std::weak_ptr<Hub> hub;
  executor_type strand;
  std::size_t generation;
  std::shared_ptr<typename Hub::bytes_t> chunk;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error, std::size_t bytes)
  {
    std::shared_ptr<Hub> const alive = hub.lock ();
    if (!alive)
      return;
    std::size_t const size = (std::min)(bytes, chunk->size ());
    alive->pump_data (error, typename Hub::bytes_view_t (chunk->data (), size),
                      0, generation);
  }
};

// The completion of a receive of a message transport.
template <typename Hub> struct message_receive_done
{
  typedef typename Hub::strand_type executor_type;
  std::weak_ptr<Hub> hub;
  executor_type strand;
  std::size_t generation;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error, datagram::message_t message)
  {
    std::shared_ptr<Hub> const alive = hub.lock ();
    if (!alive)
      return;
    alive->pump_data (error,
                      typename Hub::bytes_view_t (message.payload.data (),
                                                  message.payload.size ()),
                      message.report_id, generation);
  }
};

// The completion of a write of a byte stream (write_all: error and bytes) or
// of a send of a message transport (error only). `wire` keeps the bytes of a
// stream write alive until the transport is done with them, whatever became
// of the hub.
template <typename Hub> struct write_finished
{
  typedef typename Hub::strand_type executor_type;
  std::weak_ptr<Hub> hub;
  executor_type strand;
  std::size_t id;
  std::shared_ptr<typename Hub::bytes_t> wire;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error, std::size_t)
  {
    finish (error);
  }

  void
  operator() (std::error_code error)
  {
    finish (error);
  }

private:
  void
  finish (std::error_code error)
  {
    std::shared_ptr<Hub> const alive = hub.lock ();
    if (alive)
      alive->write_done (id, error);
  }
};

// Starts the next read of the pump on the transport of the hub.
void
start_pump_read (framed_hub<stream::byte_stream> &hub)
{
  stream_read_done<framed_hub<stream::byte_stream>> handler = {
    std::weak_ptr<framed_hub<stream::byte_stream>> (hub.shared_from_this ()),
    hub.strand (), hub.generation (), hub.chunk ()
  };
  hub.transport ().async_read_some (
      mutable_bytes_t (hub.chunk ()->data (), hub.chunk ()->size ()),
      std::move (handler));
}

void
start_pump_read (framed_hub<datagram::message_transport> &hub)
{
  message_receive_done<framed_hub<datagram::message_transport>> handler
      = { std::weak_ptr<framed_hub<datagram::message_transport>> (
              hub.shared_from_this ()),
          hub.strand (), hub.generation () };
  hub.transport ().async_receive (std::move (handler));
}

// Starts writing the frame of `wire`: a stream writes all of it (the handler
// keeps the bytes), a message transport sends it as one message (the bytes
// move into the message).
void
start_wire_write (framed_hub<stream::byte_stream> &hub,
                  std::shared_ptr<core::framing::codec::bytes_t> const &wire,
                  std::uint8_t, std::size_t id)
{
  write_finished<framed_hub<stream::byte_stream>> handler = {
    std::weak_ptr<framed_hub<stream::byte_stream>> (hub.shared_from_this ()),
    hub.strand (), id, wire
  };
  hub.transport ().async_write_all (
      const_bytes_t (wire->data (), wire->size ()), std::move (handler));
}

void
start_wire_write (framed_hub<datagram::message_transport> &hub,
                  std::shared_ptr<core::framing::codec::bytes_t> const &wire,
                  std::uint8_t report_id, std::size_t id)
{
  write_finished<framed_hub<datagram::message_transport>> handler
      = { std::weak_ptr<framed_hub<datagram::message_transport>> (
              hub.shared_from_this ()),
          hub.strand (), id,
          std::shared_ptr<core::framing::codec::bytes_t> () };
  datagram::message_t message;
  message.report_id = report_id;
  message.payload.swap (*wire);
  hub.transport ().async_send (std::move (message), std::move (handler));
}

// A frame of the reader from decoded bytes.
void
assign_frame (frame_t &frame, core::framing::codec::bytes_view_t bytes,
              std::uint8_t)
{
  frame.assign (bytes.begin (), bytes.end ());
}

void
assign_frame (datagram::message_t &frame,
              core::framing::codec::bytes_view_t bytes, std::uint8_t report_id)
{
  frame.report_id = report_id;
  frame.payload.assign (bytes.begin (), bytes.end ());
}

template <typename Transport>
asio::strand<executor_type> const &
strand_of (Transport &transport)
{
  LUMEX_CONTRACT_ASSERT (transport.impl () != nullptr);
  return transport.impl ()->strand ();
}
} // namespace

std::string const &
empty_framed_name () LUMEX_NOEXCEPT
{
  static std::string const empty;
  return empty;
}

// --- framed_hub ------------------------------------------------------------

template <typename Transport>
framed_hub<Transport>::framed_hub (Transport transport,
                                   framed_options_t const &options)
    : transport_ (std::move (transport)), strand_ (strand_of (transport_)),
      bound_ (options.queue_bound == 0 ? 1 : options.queue_bound),
      name_ (transport_.name ()), active_ (), write_active_ (false),
      active_id_ (0), write_sequence_ (0), pump_pending_ (false),
      restart_ (false),
      connection_seen_ (transport_.impl ()->connection_epoch ()),
      overflowing_ (false), error_burst_ (false), report_id_ (0),
      generation_ (0), events_ (0), queued_ (0)
{
  if (!traits_type::per_message ())
    chunk_ = std::make_shared<bytes_t> (
        options.read_chunk_size == 0 ? 1 : options.read_chunk_size);
}

template <typename Transport> framed_hub<Transport>::~framed_hub () {}

template <typename Transport>
void
framed_hub<Transport>::start_pump ()
{
  pump_step<framed_hub> step = { this->shared_from_this () };
  asio::post (strand_, std::move (step));
}

template <typename Transport>
void
framed_hub<Transport>::start_cancel ()
{
  cancel_step<framed_hub> step = { this->shared_from_this () };
  asio::post (strand_, std::move (step));
}

template <typename Transport>
void
framed_hub<Transport>::start_close ()
{
  close_step<framed_hub> step = { this->shared_from_this () };
  asio::post (strand_, std::move (step));
}

template <typename Transport>
void
framed_hub<Transport>::request_operation_cancel (
    std::weak_ptr<tdetail::operation_mark> operation)
{
  cancel_operation_step<framed_hub> step
      = { this->shared_from_this (), operation };
  asio::post (strand_, std::move (step));
}

// The handler arrives erased already (see start_read_frame); the slot handler
// goes in before the post, on the thread that starts the operation, as Asio's
// own operations do.
template <typename Transport>
void
framed_hub<Transport>::enqueue_read (read_handler_type handler,
                                     tdetail::operation_mark_ptr const &mark)
{
  std::shared_ptr<framed_hub> const self = this->shared_from_this ();
  take_cancellation (self, handler, mark, marks_);
  read_step<framed_hub> step = { self, std::move (handler) };
  asio::post (strand_, std::move (step));
}

template <typename Transport>
void
framed_hub<Transport>::enqueue_write (std::uint8_t report_id,
                                      const_bytes_t payload,
                                      status_handler_t handler,
                                      tdetail::operation_mark_ptr const &mark)
{
  std::shared_ptr<framed_hub> const self = this->shared_from_this ();
  take_cancellation (self, handler, mark, marks_);
  write_step<framed_hub> step
      = { self, report_id, payload, std::move (handler) };
  asio::post (strand_, std::move (step));
}

// Posted through the strand: asio::post then runs the handler on its own
// associated executor (any_completion_executor cannot be posted to directly),
// and never inside the call that started the operation. The result is fixed
// here: the mark of the operation is settled first, so a signal emitted while
// the handler waits on its executor ends nothing.
template <typename Transport>
void
framed_hub<Transport>::complete (read_handler_type handler,
                                 std::error_code error, frame_type frame)
{
  marks_.settle (handler);
  asio::post (strand_,
              asio::append (std::move (handler), error, std::move (frame)));
}

template <typename Transport>
void
framed_hub<Transport>::complete (status_handler_t handler,
                                 std::error_code error)
{
  marks_.settle (handler);
  asio::post (strand_, asio::append (std::move (handler), error));
}

template <typename Transport>
void
framed_hub<Transport>::note_dequeued () LUMEX_NOEXCEPT
{
  // The burst of overflows ends when the reader has taken the queue down to
  // half of its bound.
  if (overflowing_ && queue_.size () <= bound_ / 2)
    overflowing_ = false;
  queued_.store (queue_.size (), std::memory_order_release);
}

template <typename Transport>
void
framed_hub<Transport>::run_read (read_handler_type handler)
{
  if (!queue_.empty ())
    {
      frame_type frame = std::move (queue_.front ());
      queue_.pop_front ();
      note_dequeued ();
      complete (std::move (handler), std::error_code (), std::move (frame));
      return;
    }
  waiters_.push_back (std::move (handler));
  run_pump ();
}

template <typename Transport>
void
framed_hub<Transport>::run_write (std::uint8_t report_id,
                                  const_bytes_t payload,
                                  status_handler_t handler)
{
  std::shared_ptr<bytes_t> const wire = std::make_shared<bytes_t> ();
  std::error_code error;
  try
    {
      error
          = do_encode (bytes_view_t (payload.data (), payload.size ()), *wire);
    }
  catch (...)
    {
      error = make_error_code (std::errc::not_enough_memory);
    }
  if (error)
    {
      counters_.add_error (error);
      LUMEX_TRANSPORT_LOG (warning, traits_type::component (), name_.c_str (),
                           "refused a payload of "
                               + std::to_string (payload.size ())
                               + " B: " + error.message (),
                           log::log_bytes_t ());
      complete (std::move (handler), error);
      return;
    }
  pending_write item = { wire, wire->size (), report_id, std::move (handler) };
  writes_.push_back (std::move (item));
  start_next_write ();
}

template <typename Transport>
void
framed_hub<Transport>::start_next_write ()
{
  if (write_active_ || writes_.empty ())
    return;
  active_ = std::move (writes_.front ());
  writes_.pop_front ();
  write_active_ = true;
  active_id_ = ++write_sequence_;
  LUMEX_TRANSPORT_LOG (debug, traits_type::component (), name_.c_str (),
                       "writing a frame of " + std::to_string (active_.size)
                           + " B",
                       log::log_bytes_t (active_.wire->data (), active_.size));
  try
    {
      start_wire_write (*this, active_.wire, active_.report_id, active_id_);
    }
  catch (...)
    {
      write_done (active_id_, make_error_code (std::errc::not_enough_memory));
    }
}

template <typename Transport>
void
framed_hub<Transport>::write_done (std::size_t id, std::error_code error)
{
  if (!write_active_ || id != active_id_)
    return;
  write_active_ = false;
  status_handler_t handler = std::move (active_.handler);
  std::size_t const size = active_.size;
  active_.wire.reset ();
  if (!error)
    counters_.add_frame_out (size);
  complete (std::move (handler), error);
  start_next_write ();
}

template <typename Transport>
void
framed_hub<Transport>::run_start ()
{
  // A `start ()` that comes while the read of a generation that a close ended
  // is pending must not be lost: that read ends without a restart.
  if (pump_pending_)
    restart_ = true;
  else
    run_pump ();
}

template <typename Transport>
void
framed_hub<Transport>::run_pump ()
{
  if (pump_pending_)
    return;
  pump_pending_ = true;
  try
    {
      start_pump_read (*this);
    }
  catch (...)
    {
      pump_pending_ = false;
      fail_waiters (make_error_code (std::errc::not_enough_memory));
    }
}

template <typename Transport>
void
framed_hub<Transport>::pump_data (std::error_code error, bytes_view_t data,
                                  std::uint8_t report_id,
                                  std::size_t generation)
{
  pump_pending_ = false;
  bool const restart = restart_;
  restart_ = false;
  if (generation != generation_)
    {
      // A read that began before a close: its bytes are stale. A reader that
      // came since (or a `start ()`) needs a new read.
      if (!waiters_.empty () || restart)
        run_pump ();
      return;
    }
  // The transport was closed since the pump last looked (and perhaps opened
  // again): the partial frame of the codec belongs to the connection that is
  // over, whatever the order in which the close, the open and this read came.
  std::size_t const now = transport_.impl ()->connection_epoch ();
  bool const reconnected = now != connection_seen_;
  connection_seen_ = now;
  if (reconnected)
    do_reset ();
  if (!data.empty ())
    {
      counters_.add_bytes_in (data.size ());
      LUMEX_TRANSPORT_LOG (trace, traits_type::component (), name_.c_str (),
                           "received " + std::to_string (data.size ()) + " B",
                           log::log_bytes_t (data.data (), data.size ()));
      report_id_ = report_id;
      std::size_t const before = events_;
      if (traits_type::per_message ())
        do_reset ();
      try
        {
          do_decode (data);
        }
      catch (...)
        {
          counters_.add_error (make_error_code (std::errc::not_enough_memory));
          LUMEX_TRANSPORT_LOG (
              warning, traits_type::component (), name_.c_str (),
              "the decoder failed (out of memory)", log::log_bytes_t ());
          do_reset ();
        }
      if (traits_type::per_message () && events_ == before)
        {
          // No codec reports a partial frame at the end of a datagram: one
          // that held no complete frame at all is counted as truncated.
          ++events_;
          count_error (make_error_code (framing_errc::truncated));
        }
    }
  if (!error)
    {
      run_pump ();
      return;
    }
  if (error::is_transport_error (error, transport_errc::cancelled))
    {
      // A cancel of the transport (of a write) ended the read: go on while
      // the transport is open and was not closed meanwhile. A close that did
      // not come through the adapter (`transport ().close ()`) ended it for
      // good, and with it the readers, also when the transport was opened
      // again before this handler ran (the close count tells).
      if (transport_.is_open () && !reconnected)
        {
          run_pump ();
          return;
        }
      end_of_connection ();
      fail_waiters (error);
      return;
    }
  LUMEX_TRANSPORT_LOG (info, traits_type::component (), name_.c_str (),
                       "reading ended: " + error.message (),
                       log::log_bytes_t ());
  end_of_connection ();
  fail_waiters (error);
}

// The reading ended for good (an error of the transport, or the transport
// closed by someone else): the partial frame belongs to a connection that is
// over, a transport opened again must not glue its first frame to it.
template <typename Transport>
void
framed_hub<Transport>::end_of_connection ()
{
  do_reset ();
  ++generation_;
}

template <typename Transport>
void
framed_hub<Transport>::decoded (bytes_view_t frame, std::error_code error)
{
  ++events_;
  if (error)
    {
      count_error (error);
      return;
    }
  error_burst_ = false;
  LUMEX_TRANSPORT_LOG (debug, traits_type::component (), name_.c_str (),
                       "decoded a frame of " + std::to_string (frame.size ())
                           + " B",
                       log::log_bytes_t (frame.data (), frame.size ()));
  deliver (make_frame (frame));
  // Counted after the delivery: see framed_statistics_t.
  counters_.add_frame_in ();
}

template <typename Transport>
typename framed_hub<Transport>::frame_type
framed_hub<Transport>::make_frame (bytes_view_t bytes) const
{
  frame_type frame;
  assign_frame (frame, bytes, report_id_);
  return frame;
}

template <typename Transport>
void
framed_hub<Transport>::count_error (std::error_code const &error)
{
  counters_.add_error (error);
  // One warning per burst of errors (a burst ends with the next good frame):
  // a line that is noisy for ever hides the rest of the log.
  if (error_burst_)
    {
      LUMEX_TRANSPORT_LOG (debug, traits_type::component (), name_.c_str (),
                           "dropped a frame: " + error.message (),
                           log::log_bytes_t ());
      return;
    }
  error_burst_ = true;
  LUMEX_TRANSPORT_LOG (warning, traits_type::component (), name_.c_str (),
                       "dropped a frame: " + error.message (),
                       log::log_bytes_t ());
}

template <typename Transport>
void
framed_hub<Transport>::deliver (frame_type frame)
{
  if (!waiters_.empty ())
    {
      read_handler_type handler = std::move (waiters_.front ());
      waiters_.pop_front ();
      complete (std::move (handler), std::error_code (), std::move (frame));
      return;
    }
  if (queue_.size () >= bound_)
    {
      queue_.pop_front ();
      counters_.add_dropped_frame ();
      if (!overflowing_)
        {
          overflowing_ = true;
          LUMEX_TRANSPORT_LOG (
              warning, traits_type::component (), name_.c_str (),
              "the frame queue is full (" + std::to_string (bound_)
                  + " frames): dropping the oldest frames",
              log::log_bytes_t ());
        }
    }
  queue_.push_back (std::move (frame));
  queued_.store (queue_.size (), std::memory_order_release);
}

template <typename Transport>
void
framed_hub<Transport>::fail_waiters (std::error_code error)
{
  std::deque<read_handler_type> waiting;
  waiting.swap (waiters_);
  for (typename std::deque<read_handler_type>::iterator it = waiting.begin ();
       it != waiting.end (); ++it)
    complete (std::move (*it), error, frame_type ());
}

template <typename Transport>
void
framed_hub<Transport>::fail_writes (std::error_code error)
{
  std::deque<pending_write> waiting;
  waiting.swap (writes_);
  for (typename std::deque<pending_write>::iterator it = waiting.begin ();
       it != waiting.end (); ++it)
    complete (std::move (it->handler), error);
}

template <typename Transport>
void
framed_hub<Transport>::cancel_all ()
{
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  fail_waiters (cancelled);
  fail_writes (cancelled);
  // A write the transport has started is ended by the transport (it reports
  // how far it got); the cancel also ends the pump's read, which restarts.
  if (write_active_)
    transport_.cancel ();
}

template <typename Transport>
void
framed_hub<Transport>::close_all ()
{
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  ++generation_;
  // The close of the transport is posted first: by the time a handler below
  // runs, the transport is closed (is_open () is false).
  transport_.close ();
  fail_waiters (cancelled);
  fail_writes (cancelled);
  if (write_active_)
    {
      write_active_ = false;
      status_handler_t handler = std::move (active_.handler);
      active_.wire.reset ();
      complete (std::move (handler), cancelled);
    }
  queue_.clear ();
  queued_.store (0, std::memory_order_release);
  overflowing_ = false;
  error_burst_ = false;
  do_reset ();
}

template class framed_hub<stream::byte_stream>;
template class framed_hub<datagram::message_transport>;
} // namespace detail
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

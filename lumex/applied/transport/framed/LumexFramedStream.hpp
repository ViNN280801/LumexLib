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
 * @file LumexFramedStream.hpp
 * @brief `framed_stream<Codec>`: frames over a `stream::byte_stream`, with
 * the frame codec of `core/framing`, a bounded queue of decoded frames and
 * the statistics of the framing layer.
 * @details The adapter owns the byte stream it is given (moved in); its
 * destructor closes the stream. Open the stream through `transport ()`
 * (`transport ().open (deadline)`) or before handing it over. A frame is the
 * decoded payload (`frame_t`); the codec is the one of `core/framing`
 * (`slip_codec`, `cobs_codec`, `delimiter_codec`, `length_prefix_codec`,
 * `header_codec`, `modbus_rtu_codec`, a `pipeline`), moved into the adapter.
 *
 * Reading: the first `read_frame` / `async_read_frame` (or `start ()`) begins
 * a decoder that reads the stream in chunks (`framed_options_t::
 * read_chunk_size`), feeds the codec on the stream's strand and keeps the
 * decoded frames in a queue of `framed_options_t::queue_bound` frames. A
 * reader takes the oldest frame, or waits for the next. When the queue is
 * full the oldest frame is dropped, counted (`dropped_frames`) and reported
 * with one warning per burst of drops through the transport log. Frames are
 * therefore not lost between two reads while the queue has room, and a
 * reader that is slower than the device sees the newest frames.
 *
 * The decoder keeps a read pending on the transport from the first read until
 * `close ()`. With a real transport (serial, TCP, UDP) that pending read is
 * Asio work on the transport's `io_context`: `run ()` does not return while
 * the adapter is open, also after the last handler ran. End it with `close ()`
 * (or destroy the adapter), or bound the run (`run_for`, `stop`). The mock
 * transports keep no such work.
 *
 * Framing errors (`bad_checksum`, `frame_too_large`, `truncated`, ...) are
 * counted per `framing_errc` and logged (one warning per burst, a debug line
 * for each), and the decoder resynchronizes as its codec defines: they are not
 * handed to the reader. A reader waits for the next good frame; a device
 * that needs a retry on a corrupt reply times out or counts the errors
 * (`statistics ().errors_of (...)`). The reason is that a read result
 * with an error would be indistinguishable from the failure of the transport,
 * and the errors of a noisy line are a rate, not an event (spec G4).
 * Errors of the stream itself end the decoder: every waiting reader
 * completes with the error (`disconnected`, `not_open`, ...), the partial
 * frame of the codec is forgotten (a transport opened again starts a new
 * frame), and the next read starts the decoder again.
 *
 * Writing: `write_frame` encodes the payload with the codec on the strand
 * and writes the whole frame; a payload the codec refuses
 * (`framing_errc::payload_too_large`) completes with that error and writes
 * nothing. Writes go out one after the other in the order they were started,
 * never interleaved.
 *
 * The rules of the transports hold: every operation runs on the stream's
 * strand and the initiations report it; the completion handler is never
 * called inside the call that starts the operation; it runs on its associated
 * executor (by default the strand); the adapter keeps no outstanding work on
 * it (keep an `executor_work_guard` on a context whose `run ()` must not
 * return before the handler). The synchronous calls wait with a deadline,
 * cancel the adapter at the deadline and return `timeout` (counted in
 * `framed_statistics_t::timeouts`); on a thread of the executor they return
 * `would_deadlock` (not counted). `cancel ()` and `close ()` may be
 * called from any thread; the adapter may be destroyed at any time, also
 * inside one of its handlers (it closes asynchronously and returns; the
 * pending handlers still run, with `cancelled`).
 *
 * Per-operation cancellation: a `cancellation_type::terminal` emitted on the
 * cancellation slot of the handler of a pending operation (a read or a
 * write; `asio::bind_cancellation_slot`, `asio::cancel_after`, a coroutine)
 * cancels the adapter as `cancel ()` does: every read and write pending on it
 * ends with `cancelled`, not only this one. The operation is pending until
 * the adapter hands its result on: a signal emitted after that ends nothing,
 * also while the handler still waits on its executor. `partial` and `total`
 * alone change nothing.
 *
 * @code
 * framed::framed_stream<core::framing::slip::slip_codec> link (
 *     std::move (stream), core::framing::slip::slip_codec ());
 * link.write_frame (payload, deadline_t (500));
 * framed::frame_result_t reply = link.read_frame (deadline_t (500));
 * @endcode
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STREAM_HPP

#include <memory>
#include <utility>

#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/framed/detail/LumexBasicFramed.hpp"
#include "lumex/applied/transport/framed/detail/LumexFramedCodecHub.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"

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
/// @brief Frames over a byte stream (see the file description).
template <typename Codec>
class framed_stream : public detail::basic_framed<stream::byte_stream>
{
  typedef detail::basic_framed<stream::byte_stream> base_type;

public:
  /// @brief The codec of `core/framing`.
  typedef Codec codec_type;

  /// @brief Frames over `device` (not empty; moved in) with `codec` (moved
  /// in). The decoder begins with the first read.
  framed_stream (stream::byte_stream device, Codec codec,
                 framed_options_t const &options = framed_options_t ())
      : base_type (
            std::make_shared<detail::codec_hub<stream::byte_stream, Codec>> (
                std::move (device), std::move (codec), options))
  {
  }

  framed_stream (framed_stream &&other) LUMEX_NOEXCEPT
      : base_type (std::move (static_cast<base_type &> (other)))
  {
  }

  framed_stream &
  operator= (framed_stream &&other) LUMEX_NOEXCEPT
  {
    base_type::operator= (std::move (static_cast<base_type &> (other)));
    return *this;
  }

  framed_stream (framed_stream const &) = delete;
  framed_stream &operator= (framed_stream const &) = delete;

  /// @brief Closes (asynchronously) and returns; pending handlers still run
  /// with `cancelled`.
  ~framed_stream () {}
};

/// @brief `framed_stream<Codec>` from the arguments (C++11 has no deduction
/// from a constructor).
template <typename Codec>
framed_stream<Codec>
make_framed_stream (stream::byte_stream device, Codec codec,
                    framed_options_t const &options = framed_options_t ())
{
  return framed_stream<Codec> (std::move (device), std::move (codec), options);
}
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STREAM_HPP

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
 * @file LumexFramedTypes.hpp
 * @brief The types the framed adapters share: the options, the frame of a
 * stream, the type-erased completion of a frame read and the results of the
 * synchronous calls.
 * @details A `framed_stream` delivers a frame as `frame_t`, the decoded
 * payload bytes. A `framed_datagram` delivers a `datagram::message_t`: the
 * decoded payload and the report ID of the datagram it came from.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_TYPES_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/core/expected/Expected"

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
/// @brief The decoded payload of one frame of a stream.
typedef std::vector<std::uint8_t> frame_t;

/// @brief The completion of a frame read of a `framed_stream`.
typedef asio::any_completion_handler<void (std::error_code, frame_t)>
    frame_handler_t;

/// @brief The result of a synchronous `framed_stream::read_frame`.
typedef lumex::core::expected::result::expected<frame_t, std::error_code>
    frame_result_t;

/// @brief The result of a synchronous `framed_datagram::read_frame` (the
/// completion of an asynchronous one is `datagram::message_handler_t`).
typedef datagram::message_result_t message_result_t;

/// @brief The options of a framed adapter.
struct framed_options_t
{
  /// How many decoded frames wait for the reader at most (0 is taken as 1).
  /// A frame decoded while the queue is full drops the oldest frame (counted
  /// in `dropped_frames`, one warning per burst of drops).
  std::size_t queue_bound = 64;
  /// The size of the buffer one read of a `framed_stream` fills (0 is taken
  /// as 1). Not used by a `framed_datagram`, which receives whole datagrams.
  std::size_t read_chunk_size = 4096;
};
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_TYPES_HPP

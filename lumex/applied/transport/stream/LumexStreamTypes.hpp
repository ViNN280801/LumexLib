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
 * @file LumexStreamTypes.hpp
 * @brief The types the transports share: byte views, the executor, the
 * type-erased completion handlers and the results of synchronous calls.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_STREAM_TYPES_HPP
#define LUMEX_APPLIED_TRANSPORT_STREAM_TYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <system_error>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/core/expected/Expected"
#include "lumex/core/span/LumexSpan"

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
/// @brief Bytes to read into.
typedef lumex::core::span::view::span<std::uint8_t> mutable_bytes_t;

/// @brief Bytes to write.
typedef lumex::core::span::view::span<std::uint8_t const> const_bytes_t;

/// @brief The executor every transport takes (an `io_context`, a strand, a
/// thread pool, ...).
typedef asio::any_io_executor executor_type;

/// @brief A deadline of a synchronous call, counted from the call.
typedef std::chrono::milliseconds deadline_t;

/// @brief The completion of an open, a send or a close.
typedef asio::any_completion_handler<void (std::error_code)> status_handler_t;

/// @brief The completion of a read or a write: the error and the byte count.
typedef asio::any_completion_handler<void (std::error_code, std::size_t)>
    io_handler_t;

/// @brief The result of a synchronous call without a value.
typedef lumex::core::expected::result::expected<void, std::error_code>
    status_result_t;

/// @brief The result of a synchronous read or write: the byte count.
typedef lumex::core::expected::result::expected<std::size_t, std::error_code>
    io_result_t;
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_STREAM_TYPES_HPP

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
 * @file LumexIoRunner.hpp
 * @brief `io_runner`: an `io_context` with its own threads, for a program
 * that uses the transports synchronously and runs no executor of its own.
 * @details The runner owns the context, a work guard and the threads. Its
 * destructor stops the context and joins the threads; destroy the transports
 * that use its executor first (a transport keeps no reference to the runner,
 * but its pending handlers are dropped with the context).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_STREAM_IO_RUNNER_HPP
#define LUMEX_APPLIED_TRANSPORT_STREAM_IO_RUNNER_HPP

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
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
namespace stream
{
/// @brief An `io_context` run by its own threads.
class io_runner
{
public:
  /// @brief Starts `threads` threads (at least one).
  explicit io_runner (std::size_t threads = 1);

  /// @brief Stops the context and joins the threads.
  ~io_runner ();

  io_runner (io_runner const &) = delete;
  io_runner &operator= (io_runner const &) = delete;

  /// @brief The executor to give the transports.
  executor_type get_executor () LUMEX_NOEXCEPT;

  /// @brief The context, for timers and Asio objects of the caller.
  asio::io_context &
  context () LUMEX_NOEXCEPT
  {
    return context_;
  }

  /// @brief The number of threads.
  std::size_t
  thread_count () const LUMEX_NOEXCEPT
  {
    return threads_.size ();
  }

  /// @brief Stops the context and joins the threads (idempotent; the
  /// destructor calls it).
  void stop () LUMEX_NOEXCEPT;

private:
  asio::io_context context_;
  asio::executor_work_guard<asio::io_context::executor_type> work_;
  std::vector<std::thread> threads_;
};
} // namespace stream
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_STREAM_IO_RUNNER_HPP

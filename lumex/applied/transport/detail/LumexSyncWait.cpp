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

#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"

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
namespace detail
{
bool
runs_on (executor_type const &executor) LUMEX_NOEXCEPT
{
  // any_io_executor has no running_in_this_thread (): ask the concrete types
  // the library and its users create.
  typedef asio::io_context::executor_type context_executor;
  typedef asio::thread_pool::executor_type pool_executor;
  if (context_executor const *const context
      = executor.target<context_executor> ())
    return context->running_in_this_thread ();
  if (pool_executor const *const pool = executor.target<pool_executor> ())
    return pool->running_in_this_thread ();
  if (asio::strand<context_executor> const *const strand
      = executor.target<asio::strand<context_executor>> ())
    return strand->running_in_this_thread ()
           || strand->get_inner_executor ().running_in_this_thread ();
  if (asio::strand<pool_executor> const *const strand
      = executor.target<asio::strand<pool_executor>> ())
    return strand->running_in_this_thread ()
           || strand->get_inner_executor ().running_in_this_thread ();
  return false;
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

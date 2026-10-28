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
 * @file LumexHandlerKeeper.hpp
 * @brief `handler_keeper` and `enlist_keeper`: how an implementation that
 * keeps a completion handler in a member lets the `io_context` shutdown
 * destroy it.
 * @details Internal to the transport library. A pending operation of a stream,
 * a message transport, a session or a framed adapter keeps its handler in the
 * implementation (a mock's pending read, a TCP open in progress, a session
 * waiter, a queued frame reader), not in an Asio queue. Asio's shutdown of an
 * `io_context` destroys only the handlers it queues itself. A handler that
 * owns its owner (a lambda that captures the object holding the transport,
 * the usual way to keep an operation alive) then leaves a cycle nobody can
 * break: handler -> owner -> transport -> implementation -> handler. The
 * implementation's `handler_keeper` is enlisted with an
 * `execution_context::service` of the transport (`enlist_keeper`); the
 * service's `shutdown ()` calls `drop_kept_handlers ()` on every live keeper,
 * which destroys the handlers it keeps WITHOUT calling them, exactly what
 * Asio does with its own queued handlers.
 *
 * The service is created with the first enlisted keeper, so it is registered
 * after the scheduler and the strand service of the context and Asio shuts it
 * down before them: the destructors of the dropped handlers (which may close
 * a transport and post to its strand) run while those services are still
 * alive. The registry holds the keepers weakly: it neither keeps an
 * implementation alive nor needs to be told when one dies.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_HANDLER_KEEPER_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_HANDLER_KEEPER_HPP

#include <atomic>
#include <memory>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
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
namespace detail
{
/**
 * @brief An object that keeps completion handlers outside Asio's queues.
 * @details It must be owned by a `std::shared_ptr` (the registry holds it
 * weakly).
 */
class handler_keeper
{
public:
  handler_keeper () = default;
  virtual ~handler_keeper ();

  handler_keeper (handler_keeper const &) = delete;
  handler_keeper &operator= (handler_keeper const &) = delete;

  /**
   * @brief Destroys, without calling them, the handlers this object keeps.
   * @details Called by the `io_context` shutdown on the thread that destroys
   * the context, where no thread runs the context any more; may be called
   * more than once. Move the handlers out of the members first and let them
   * die after any lock is released: a handler's destructor may destroy the
   * last owner of this very object.
   */
  virtual void drop_kept_handlers () LUMEX_NOEXCEPT = 0;
};

/// @brief Enlists `keeper` with the shutdown service of `context` (the
/// service is created on first use). Does nothing once the context has begun
/// its shutdown. Throws `std::bad_alloc` like any allocation.
void enlist_keeper_with (asio::execution_context &context,
                         std::shared_ptr<handler_keeper> const &keeper);

/**
 * @brief Enlists `keeper` once per `flag` with the context of `executor`.
 * @details Called by the `enqueue_*` of an implementation before it holds its
 * first handler, from any thread; the flag makes later calls two atomic
 * loads. If the allocation fails the flag is cleared, so a later call tries
 * again.
 */
template <typename Executor>
void
enlist_keeper (std::atomic<bool> &flag, Executor const &executor,
               std::shared_ptr<handler_keeper> const &keeper)
{
  if (flag.load (std::memory_order_acquire))
    return;
  if (flag.exchange (true, std::memory_order_acq_rel))
    return;
  try
    {
      enlist_keeper_with (asio::query (executor, asio::execution::context),
                          keeper);
    }
  catch (...)
    {
      flag.store (false, std::memory_order_release);
      throw;
    }
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_HANDLER_KEEPER_HPP

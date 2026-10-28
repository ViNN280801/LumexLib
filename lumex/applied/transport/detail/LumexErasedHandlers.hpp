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
 * @file LumexErasedHandlers.hpp
 * @brief The wrapper a session or a framed adapter puts around a user's
 * handler before it erases it: the allocator association that does not
 * forward the view allocator of an erased handler; and the completion of a
 * handler without an object to run it.
 * @details Internal to the transport library. An `any_completion_handler`
 * exposes as its allocator a view into its own storage
 * (`any_completion_handler_allocator`). A wrapper around it, at any depth
 * (`bind_executor (ex, any_completion_handler (...))`), that is erased once
 * more would forward that view, and the outer erasure would free its storage
 * through it after the inner erasure is gone (the use-after-free of the
 * reviews of WP12 and WP13). The wrappers here forward every association of
 * the handler but such an allocator, for which they give the default
 * candidate. The trait is the base's
 * (`detail::is_erased_handler_allocator` of `LumexCountingHandler.hpp`),
 * which applies the same rule to its counting handler.
 *
 * No wrapper keeps work on the handler's executor: a handler a session or an
 * adapter keeps outside an Asio operation (a waiter) is not destroyed by the
 * `io_context` shutdown, and outstanding work held by it made `~io_context`
 * wait for ever on Windows when the handler kept its own session alive
 * (REVIEW13 re-review H1, H2).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_ERASED_HANDLERS_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_ERASED_HANDLERS_HPP

#include <type_traits>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
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
 * @brief The allocator association of a wrapper around `Handler`: the
 * handler's own allocator, or `DefaultCandidate` when that is the view
 * allocator of an erased handler.
 */
template <typename Handler, typename DefaultCandidate>
struct shielded_allocator
{
  using inner_type = asio::associated_allocator<Handler, DefaultCandidate>;
  using erased
      = ::lumex::applied::transport::detail::is_erased_handler_allocator<
          typename inner_type::type>;
  using type = typename std::conditional<erased::value, DefaultCandidate,
                                         typename inner_type::type>::type;

  static type
  get (Handler const &handler,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
  {
    return pick (handler, candidate, erased ());
  }

private:
  static type
  pick (Handler const &, DefaultCandidate const &candidate,
        std::true_type) LUMEX_NOEXCEPT
  {
    return candidate;
  }

  static type
  pick (Handler const &handler, DefaultCandidate const &candidate,
        std::false_type) LUMEX_NOEXCEPT
  {
    return inner_type::get (handler, candidate);
  }
};

/**
 * @brief A user's handler as a session or an adapter erases it: every
 * association of the handler is forwarded but an erased handler's view
 * allocator. It owns the mark of its pending operation (with a cancellation
 * slot) until it is called or destroyed: a canceller that holds the mark
 * weakly reaches nothing after that.
 */
template <typename Handler> struct shielded_handler
{
  Handler handler;
  ::lumex::applied::transport::detail::operation_mark_ptr pending;

  template <typename... Args>
  void
  operator() (Args &&...args)
  {
    pending.reset ();
    std::move (handler) (std::forward<Args> (args)...);
  }
};

/// @brief `handler` erased once into `Erased` (an `any_completion_handler`),
/// through `shielded_handler`, which owns `pending` (the mark of the
/// operation, or none).
template <typename Erased, typename Handler>
Erased
erase_once (
    Handler &&handler,
    ::lumex::applied::transport::detail::operation_mark_ptr const &pending)
{
  shielded_handler<typename std::decay<Handler>::type> shielded
      = { std::forward<Handler> (handler), pending };
  return Erased (std::move (shielded));
}

/**
 * @brief Completes `handler` with `args` on its associated executor (the
 * system executor when it has none): the answer of a moved-from session.
 * Posted through the system executor: an erased handler's executor
 * (`any_completion_executor`) cannot be posted to directly.
 */
template <typename Handler, typename... Args>
void
complete_detached (Handler &&handler, Args &&...args)
{
  asio::post (asio::system_executor (),
              asio::append (std::forward<Handler> (handler),
                            std::forward<Args> (args)...));
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
template <template <typename, typename> class Associator, typename Handler,
          typename DefaultCandidate>
struct associator<Associator,
                  lumex::applied::transport::detail::shielded_handler<Handler>,
                  DefaultCandidate> : Associator<Handler, DefaultCandidate>
{
  using wrapper_type
      = lumex::applied::transport::detail::shielded_handler<Handler>;

  static typename Associator<Handler, DefaultCandidate>::type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler);
  }

  static auto
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
      -> decltype (Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                               candidate))
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                       candidate);
  }
};

template <typename Handler, typename DefaultCandidate>
struct associator<associated_allocator,
                  lumex::applied::transport::detail::shielded_handler<Handler>,
                  DefaultCandidate>
{
  using wrapper_type
      = lumex::applied::transport::detail::shielded_handler<Handler>;
  using shield_type = lumex::applied::transport::detail::shielded_allocator<
      Handler, DefaultCandidate>;
  using type = typename shield_type::type;

  static type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return shield_type::get (wrapper.handler, DefaultCandidate ());
  }

  static type
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
  {
    return shield_type::get (wrapper.handler, candidate);
  }
};
} // namespace asio

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_ERASED_HANDLERS_HPP

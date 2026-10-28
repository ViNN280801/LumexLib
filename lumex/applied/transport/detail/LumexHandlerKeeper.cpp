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

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"

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
// The id of the service is a static object, as Asio's own service ids are.
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace detail
{
handler_keeper::~handler_keeper () {}

namespace
{
// The size of the list at which the first sweep of dead keepers runs.
std::size_t const kfirst_sweep = 64;

/// The shutdown service of one binary's copy of the transport (the static
/// library gives every binary its own, with its own id, also when they share
/// an io_context).
class keeper_service : public asio::execution_context::service
{
public:
  static asio::execution_context::id id;

  explicit keeper_service (asio::execution_context &context)
      : asio::execution_context::service (context)
  {
  }

  void
  enlist (std::shared_ptr<handler_keeper> const &keeper)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (shut_down_)
      return;
    // Keepers that died are dropped as new ones come, so a long-lived context
    // that sees many short-lived transports does not collect dead entries:
    // the sweep runs when the list has doubled since the last one.
    if (keepers_.size () >= sweep_at_)
      {
        sweep ();
        sweep_at_ = keepers_.size () < kfirst_sweep ? kfirst_sweep
                                                    : keepers_.size () * 2;
      }
    keepers_.push_back (keeper);
  }

private:
  void
  shutdown () override
  {
    std::vector<std::weak_ptr<handler_keeper>> taken;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      shut_down_ = true;
      taken.swap (keepers_);
    }
    // Newest first, as Asio shuts its services down. Each keeper is held for
    // the call only; the last owner of one may be a handler another keeper
    // drops, so a keeper that died meanwhile is skipped.
    for (std::vector<std::weak_ptr<handler_keeper>>::reverse_iterator it
         = taken.rbegin ();
         it != taken.rend (); ++it)
      if (std::shared_ptr<handler_keeper> const alive = it->lock ())
        alive->drop_kept_handlers ();
  }

  void
  sweep ()
  {
    std::vector<std::weak_ptr<handler_keeper>> alive;
    alive.reserve (keepers_.size ());
    for (std::vector<std::weak_ptr<handler_keeper>>::iterator it
         = keepers_.begin ();
         it != keepers_.end (); ++it)
      if (!it->expired ())
        alive.push_back (*it);
    keepers_.swap (alive);
  }

  std::mutex mutex_;
  std::vector<std::weak_ptr<handler_keeper>> keepers_;
  std::size_t sweep_at_ = kfirst_sweep;
  bool shut_down_ = false;
};

asio::execution_context::id keeper_service::id;
} // namespace

void
enlist_keeper_with (asio::execution_context &context,
                    std::shared_ptr<handler_keeper> const &keeper)
{
  asio::use_service<keeper_service> (context).enlist (keeper);
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogue.hpp"
#include "lumex/applied/transport/protocol/dialogue/detail/LumexDialogueImpl.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/core/contracts/LumexContracts"

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
namespace protocol
{
namespace
{
std::string const &
empty_engine_name () LUMEX_NOEXCEPT
{
  static std::string const empty;
  return empty;
}
} // namespace

dialogue_engine::dialogue_engine (
    frame_link link,
    ::lumex::core::protocol::index::protocol_index_t const &index,
    dialogue_options_t const &options)
    : impl_ (detail::dialogue_impl::make (std::move (link), index, options))
{
}

dialogue_engine::~dialogue_engine () { release (); }

dialogue_engine::dialogue_engine (dialogue_engine &&other) LUMEX_NOEXCEPT
    : impl_ (std::move (other.impl_))
{
}

dialogue_engine &
dialogue_engine::operator= (dialogue_engine &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      release ();
      impl_ = std::move (other.impl_);
    }
  return *this;
}

void
dialogue_engine::release () LUMEX_NOEXCEPT
{
  if (!impl_)
    return;
  try
    {
      impl_->start_close ();
    }
  catch (...)
    {
      // Out of memory: the link closes when the engine dies.
    }
  impl_.reset ();
}

executor_type
dialogue_engine::get_executor () const
{
  return impl_ ? impl_->get_executor () : executor_type ();
}

dialogue_engine::strand_type
dialogue_engine::get_strand () const
{
  LUMEX_CONTRACT_ASSERT (impl_ != nullptr);
  return impl_->strand ();
}

std::string const &
dialogue_engine::name () const
{
  return impl_ ? impl_->name () : empty_engine_name ();
}

::lumex::core::protocol::index::protocol_index_t const &
dialogue_engine::index () const
{
  LUMEX_CONTRACT_ASSERT (impl_ != nullptr);
  return impl_->index ();
}

void
dialogue_engine::select_layer (
    ::lumex::core::protocol::index::protocol_index_t const &index,
    protocol_features_t const *features)
{
  if (impl_)
    impl_->start_select_layer (index, features);
}

::lumex::core::protocol::index::session_context_t &
dialogue_engine::session ()
{
  LUMEX_CONTRACT_ASSERT (impl_ != nullptr);
  return impl_->session ();
}

::lumex::core::protocol::index::session_context_t const &
dialogue_engine::session () const
{
  LUMEX_CONTRACT_ASSERT (impl_ != nullptr);
  return impl_->session ();
}

subscription_t
dialogue_engine::subscribe (::lumex::core::protocol::index::row_t const *row,
                            frame_handler_t handler)
{
  return impl_ ? impl_->subscribe (row, std::move (handler))
               : subscription_t ();
}

bool
dialogue_engine::unsubscribe (subscription_t subscription)
{
  return impl_ && impl_->unsubscribe (subscription);
}

void
dialogue_engine::on_unknown (frame_handler_t handler)
{
  if (impl_)
    impl_->set_unknown_handler (std::move (handler));
}

subscription_t
dialogue_engine::on_edge (::lumex::core::protocol::index::row_t const *row,
                          edge_handler_t handler)
{
  return impl_ ? impl_->subscribe_edge (row, std::move (handler))
               : subscription_t ();
}

void
dialogue_engine::on_link_state (link_state_handler_t handler)
{
  if (impl_)
    impl_->set_link_handler (std::move (handler));
}

bool
dialogue_engine::link_is_up () const LUMEX_NOEXCEPT
{
  return impl_ && impl_->link_up ();
}

bool
dialogue_engine::set_keepalive (keepalive_generator_t generator)
{
  return impl_ && impl_->set_keepalive (std::move (generator));
}

void
dialogue_engine::set_state (std::int64_t state)
{
  if (impl_)
    impl_->set_state (state);
}

void
dialogue_engine::clear_state () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->clear_state ();
}

std::int64_t
dialogue_engine::last_state () const LUMEX_NOEXCEPT
{
  return impl_ ? impl_->last_state () : 0;
}

bool
dialogue_engine::state_known () const LUMEX_NOEXCEPT
{
  return impl_ && impl_->state_known ();
}

dialogue_statistics_t
dialogue_engine::statistics () const LUMEX_NOEXCEPT
{
  return impl_ ? impl_->counters ().snapshot () : dialogue_statistics_t ();
}

void
dialogue_engine::reset_statistics () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->counters ().reset ();
}

void
dialogue_engine::cancel () LUMEX_NOEXCEPT
{
  if (!impl_)
    return;
  try
    {
      impl_->start_cancel ();
    }
  catch (...)
    {
    }
}

void
dialogue_engine::close () LUMEX_NOEXCEPT
{
  if (!impl_)
    return;
  try
    {
      impl_->start_close ();
    }
  catch (...)
    {
    }
}

executor_type
dialogue_engine::initiation_executor () const
{
  if (!impl_)
    return executor_type (asio::system_executor ());
  return executor_type (impl_->strand ());
}
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

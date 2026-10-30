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
#include <string>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
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
empty_link_name () LUMEX_NOEXCEPT
{
  static std::string const empty;
  return empty;
}
} // namespace

frame_link_impl::~frame_link_impl () {}

frame_link::frame_link () LUMEX_NOEXCEPT {}

frame_link::frame_link (std::shared_ptr<frame_link_impl> impl) LUMEX_NOEXCEPT
    : impl_ (std::move (impl))
{
}

frame_link::~frame_link () {}

frame_link::frame_link (frame_link &&other) LUMEX_NOEXCEPT
    : impl_ (std::move (other.impl_))
{
}

frame_link &
frame_link::operator= (frame_link &&other) LUMEX_NOEXCEPT
{
  impl_ = std::move (other.impl_);
  return *this;
}

executor_type
frame_link::get_executor () const
{
  return impl_ ? impl_->get_executor () : executor_type ();
}

frame_link::strand_type
frame_link::get_strand () const
{
  LUMEX_CONTRACT_ASSERT (impl_ != nullptr);
  return impl_->get_strand ();
}

std::string const &
frame_link::name () const
{
  return impl_ ? impl_->name () : empty_link_name ();
}

bool
frame_link::is_open () const
{
  return impl_ && impl_->is_open ();
}

void
frame_link::cancel () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->cancel ();
}

void
frame_link::close () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->close ();
}

executor_type
frame_link::initiation_executor () const
{
  if (!impl_)
    return executor_type (asio::system_executor ());
  return executor_type (impl_->get_strand ());
}
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

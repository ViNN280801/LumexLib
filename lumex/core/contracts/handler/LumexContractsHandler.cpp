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

#include <cstdio>

#include "lumex/core/contracts/handler/LumexContractsHandler.hpp"
#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"

namespace lumex
{
namespace core
{
namespace contracts
{
namespace
{
struct violation_handler_tag_t;

using handler_slot_t = utility::callback::lumex_callback_slot<
    violation_handler_tag_t, void (contract_violation const &)>;

// Non-zero while the current thread runs a handler. A trivially
// constructible and destructible thread-local, so it is usable from static
// constructors and destructors and needs no thread-exit hook.
thread_local int t_handler_depth = 0;

class handler_depth_guard
{
public:
  handler_depth_guard () LUMEX_NOEXCEPT { ++t_handler_depth; }
  ~handler_depth_guard () { --t_handler_depth; }
  handler_depth_guard (handler_depth_guard const &) = delete;
  handler_depth_guard &operator= (handler_depth_guard const &) = delete;
};
} // namespace

violation_handler_type
set_violation_handler (violation_handler_type handler) LUMEX_NOEXCEPT
{
  return handler_slot_t::exchange (handler);
}

violation_handler_type
get_violation_handler () LUMEX_NOEXCEPT
{
  return handler_slot_t::get ();
}

void
invoke_violation_handler (contract_violation const &violation)
{
  violation_handler_type const handler = handler_slot_t::get ();
  if (handler == nullptr || t_handler_depth != 0)
    {
      invoke_default_violation_handler (violation);
      return;
    }
  handler_depth_guard const guard;
  handler (violation);
}

void
invoke_default_violation_handler (contract_violation const &violation)
    LUMEX_NOEXCEPT
{
  source_location const &where = violation.location ();
  // One call, so lines of several threads do not interleave.
  std::fprintf (stderr,
                "%s:%lu:%lu: %s: contract violation: %s [kind: %s, "
                "semantic: %s, detection: %s]\n",
                where.file_name (), static_cast<unsigned long> (where.line ()),
                static_cast<unsigned long> (where.column ()),
                where.function_name (), violation.comment (),
                to_string (violation.kind ()),
                to_string (violation.semantic ()),
                to_string (violation.detection_mode ()));
  std::fflush (stderr);
}
} // namespace contracts
} // namespace core
} // namespace lumex

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

#define LUMEX_IMPLEMENTATION
#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"

#include "LumexExceptionWrapper.hpp"

namespace lumex
{
namespace core
{
namespace exceptions
{
namespace Wrapper
{
namespace
{
// Only this translation unit touches the slot, so the executable and the
// exceptions shared library see one reporter through the exported wrappers.
struct safe_call_reporter_tag_t;
using SafeCallReporterSlot
    = lumex::core::utility::callback::lumex_callback_slot<
        safe_call_reporter_tag_t, void (char const *)>;
} // namespace

LUMEX_PUBLIC_API
void
set_safe_call_reporter (safe_call_report_fn reporter) LUMEX_NOEXCEPT
{
  SafeCallReporterSlot::set (reporter);
}

LUMEX_PUBLIC_API
safe_call_report_fn
get_safe_call_reporter () LUMEX_NOEXCEPT
{
  return SafeCallReporterSlot::get ();
}
} // namespace Wrapper
} // namespace exceptions
} // namespace core
} // namespace lumex

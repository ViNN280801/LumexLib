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
#include "lumex/LumexExport.hpp"

#include "LumexStacktraceEntry.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
// Forward declaration for resolve_symbol_info from LumexStacktrace.hpp
namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace stacktrace
{
namespace detail
{
bool resolve_symbol_info (void *address, std::string &function_name,
                          std::string &source_file,
                          std::uint32_t &line_number) LUMEX_NOEXCEPT;
} // namespace detail
} // namespace stacktrace
} // namespace exceptions
} // namespace core
} // namespace lumex

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace stacktrace
{
LUMEX_PUBLIC_API
void
lumex_stacktrace_entry::ensure_cache_valid () const
{
  if (m_cache_valid || m_address == nullptr)
    return;

  detail::resolve_symbol_info (m_address, m_cached_description,
                               m_cached_source_file, m_cached_source_line);

  m_cache_valid = true;
}

LUMEX_PUBLIC_API
std::string
lumex_stacktrace_entry::description () const
{
  ensure_cache_valid ();
  return m_cached_description;
}

LUMEX_PUBLIC_API
std::string
lumex_stacktrace_entry::source_file () const
{
  ensure_cache_valid ();
  return m_cached_source_file;
}

LUMEX_PUBLIC_API
std::uint32_t
lumex_stacktrace_entry::source_line () const
{
  ensure_cache_valid ();
  return m_cached_source_line;
}

} // namespace stacktrace
} // namespace exceptions
} // namespace core
} // namespace lumex

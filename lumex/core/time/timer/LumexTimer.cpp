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
#include <ostream>

#include "LumexTimer.hpp"

namespace lumex
{
namespace core
{
namespace time
{
namespace timer
{
LUMEX_PUBLIC_API
void
LumexTimer::start_timer ()
{
  m_is_started = true;
  m_start_tp = std::chrono::high_resolution_clock::now ();
}

LUMEX_PUBLIC_API
void
LumexTimer::stop_timer ()
{
  m_is_started = false;
  m_end_tp = std::chrono::high_resolution_clock::now ();
}

LUMEX_PUBLIC_API
long long
LumexTimer::elapsed_time_ms () const
{
  auto elapsed = m_end_tp - m_start_tp;
  return std::chrono::duration_cast<std::chrono::milliseconds> (elapsed)
      .count ();
}

LUMEX_PUBLIC_API
std::string
extract_function_name (std::string const &expr_str)
{
  std::string processed = expr_str;

  // Trim leading whitespace.
  while (!processed.empty ()
         && (processed.front () == ' ' || processed.front () == '\t'))
    {
      processed.erase (processed.begin ());
    }

  // Drop everything from the first '(' onward.
  std::string::size_type pos = processed.find ('(');
  if (pos != std::string::npos)
    processed = processed.substr (0, pos);

  // Keep only the part after the last "->" or ".", i.e. strip any
  // member-access prefix.
  pos = processed.rfind ("->");
  if (pos != std::string::npos)
    {
      processed = processed.substr (pos + 2);
    }
  else
    {
      pos = processed.rfind ('.');
      if (pos != std::string::npos)
        processed = processed.substr (pos + 1);
    }

  // Trim trailing whitespace.
  while (!processed.empty ()
         && (processed.back () == ' ' || processed.back () == '\t'))
    {
      processed.pop_back ();
    }

  return processed.empty () ? expr_str : processed;
}

LUMEX_PUBLIC_API
void
write_measure_time_report (std::ostream &out, std::string const &message,
                           long long elapsed_ms)
{
  if (!message.empty ())
    {
      out << message << ": execution time: " << elapsed_ms << "ms\n"
          << std::flush;
    }
  else
    {
      out << "Time: " << elapsed_ms << " [ms]\n" << std::flush;
    }
}
} // namespace timer
} // namespace time
} // namespace core
} // namespace lumex

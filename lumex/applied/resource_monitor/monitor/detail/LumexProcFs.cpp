/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define LUMEX_IMPLEMENTATION
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>

#include "LumexProcFs.hpp"

namespace lumex
{
namespace applied
{
namespace resource_monitor
{
namespace monitor
{
namespace detail
{
LUMEX_PUBLIC_API optional<std::uint64_t>
parse_kib_field (lumex_string_view text, lumex_string_view key)
{
  std::size_t at = 0;
  while (at < text.size ())
    {
      std::size_t const end = text.find ('\n', at);
      lumex_string_view const line = text.substr (
          at,
          end == lumex_string_view::npos ? lumex_string_view::npos : end - at);
      if (line.size () > key.size () && line.compare (0, key.size (), key) == 0
          && line[key.size ()] == ':')
        {
          std::istringstream fields (
              std::string (line.substr (key.size () + 1)));
          std::uint64_t value{};
          std::string unit;
          if (!(fields >> value))
            return nullopt;
          fields >> unit;
          return unit == "kB" ? value * 1024U : value;
        }
      if (end == lumex_string_view::npos)
        break;
      at = end + 1;
    }
  return nullopt;
}

LUMEX_PUBLIC_API optional<proc_stat_cpu_t>
parse_proc_stat_cpu (lumex_string_view text)
{
  lumex_string_view const line = text.substr (0, text.find ('\n'));
  if (line.size () < 4 || line.compare (0, 4, "cpu ") != 0)
    return nullopt;

  // user nice system idle iowait irq softirq steal guest guest_nice
  std::istringstream fields (std::string (line.substr (4)));
  std::uint64_t value{};
  std::array<std::uint64_t, 8> values{};
  std::size_t count = 0;
  while (count < values.size () && fields >> value)
    values.at (count++) = value;
  if (count < 4)
    return nullopt;

  proc_stat_cpu_t cpu;
  cpu.idle = values.at (3) + values.at (4);
  for (std::size_t i = 0; i < count; ++i)
    cpu.total += values.at (i);
  return cpu;
}

LUMEX_PUBLIC_API optional<proc_meminfo_t>
parse_proc_meminfo (lumex_string_view text)
{
  optional<std::uint64_t> const total = parse_kib_field (text, "MemTotal");
  optional<std::uint64_t> const available
      = parse_kib_field (text, "MemAvailable");
  if (!total || !available)
    return nullopt;
  proc_meminfo_t memory;
  memory.total_bytes = *total;
  memory.available_bytes = *available;
  return memory;
}

LUMEX_PUBLIC_API optional<std::string>
read_whole_file (std::string const &path)
{
  std::ifstream in (path, std::ios::in | std::ios::binary);
  if (!in)
    return nullopt;
  return std::string (std::istreambuf_iterator<char> (in),
                      std::istreambuf_iterator<char> ());
}
} // namespace detail
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

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

#include <cstddef>
#include <cstdint>
#include <map>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidReportDescriptor.hpp"

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
namespace hid
{
namespace
{
// Item types (bits 3..2 of the prefix) and the tags (bits 7..4) the analysis
// needs (HID 1.11, 6.2.2).
unsigned const KTYPE_MAIN = 0;
unsigned const KTYPE_GLOBAL = 1;
unsigned const KTAG_INPUT = 0x8;
unsigned const KTAG_OUTPUT = 0x9;
unsigned const KTAG_FEATURE = 0xB;
unsigned const KTAG_REPORT_SIZE = 0x7;
unsigned const KTAG_REPORT_ID = 0x8;
unsigned const KTAG_REPORT_COUNT = 0x9;
unsigned const KTAG_PUSH = 0xA;
unsigned const KTAG_POP = 0xB;
// A long item: [0xFE][data size][long tag][data...].
unsigned const KLONG_ITEM_PREFIX = 0xFE;
// The largest report the analysis accepts: 64 KiB (hidraw and the Windows
// HID class stay far below it). It also bounds the reader's buffer.
std::uint64_t const KMAX_REPORT_BITS = 8U * 65536U;

// The part of the global state the sizes depend on.
struct global_state
{
  std::uint64_t report_size_bits = 0;
  std::uint64_t report_count = 0;
  std::uint32_t report_id = 0;
};

report_layout_result_t
bad_descriptor ()
{
  return report_layout_result_t (
      lumex::core::expected::result::unexpect,
      make_error_code (error::transport_errc::bad_report_descriptor));
}

std::vector<report_size_t>
to_sizes (std::map<std::uint32_t, std::uint64_t> const &bits)
{
  std::vector<report_size_t> sizes;
  for (std::map<std::uint32_t, std::uint64_t>::const_iterator it
       = bits.begin ();
       it != bits.end (); ++it)
    {
      report_size_t size;
      size.report_id = static_cast<std::uint8_t> (it->first);
      size.bytes = static_cast<std::size_t> ((it->second + 7U) / 8U);
      sizes.push_back (size);
    }
  return sizes;
}

std::size_t
size_of (std::vector<report_size_t> const &sizes,
         std::uint8_t report_id) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < sizes.size (); ++i)
    if (sizes[i].report_id == report_id)
      return sizes[i].bytes;
  return 0;
}
} // namespace

std::size_t
report_layout_t::input_size (std::uint8_t report_id) const LUMEX_NOEXCEPT
{
  return size_of (input_reports, report_id);
}

std::size_t
report_layout_t::output_size (std::uint8_t report_id) const LUMEX_NOEXCEPT
{
  return size_of (output_reports, report_id);
}

bool
report_layout_t::has_output (std::uint8_t report_id) const LUMEX_NOEXCEPT
{
  return output_size (report_id) > 0;
}

std::size_t
report_layout_t::max_input_size () const LUMEX_NOEXCEPT
{
  std::size_t largest = 0;
  for (std::size_t i = 0; i < input_reports.size (); ++i)
    if (input_reports[i].bytes > largest)
      largest = input_reports[i].bytes;
  return largest;
}

report_layout_result_t
analyze_report_descriptor (const_bytes_t descriptor)
{
  std::uint8_t const *const data = descriptor.data ();
  std::size_t const size = descriptor.size ();
  if (data == nullptr || size == 0)
    return bad_descriptor ();

  global_state state;
  std::vector<global_state> stack;
  std::map<std::uint32_t, std::uint64_t> input_bits;
  std::map<std::uint32_t, std::uint64_t> output_bits;
  bool numbered = false;
  bool main_without_id = false;

  std::size_t pos = 0;
  while (pos < size)
    {
      unsigned const prefix = data[pos];
      if (prefix == KLONG_ITEM_PREFIX)
        {
          // Not used by report items; skipped whole, but never past the end.
          if (size - pos < 3U || size - pos - 3U < data[pos + 1])
            return bad_descriptor ();
          pos += 3U + data[pos + 1];
          continue;
        }
      unsigned data_size = prefix & 0x3U;
      if (data_size == 3U)
        data_size = 4U;
      if (size - pos - 1U < data_size)
        return bad_descriptor ();
      std::uint64_t value = 0;
      for (unsigned i = 0; i < data_size; ++i)
        value |= static_cast<std::uint64_t> (data[pos + 1U + i]) << (8U * i);
      unsigned const type = (prefix >> 2) & 0x3U;
      unsigned const tag = prefix >> 4;
      if (type == KTYPE_GLOBAL)
        {
          if (tag == KTAG_REPORT_SIZE)
            state.report_size_bits = value;
          else if (tag == KTAG_REPORT_COUNT)
            state.report_count = value;
          else if (tag == KTAG_REPORT_ID)
            {
              // ID 0 is reserved; a report ID is one byte on the wire.
              if (value == 0 || value > 0xFFU)
                return bad_descriptor ();
              state.report_id = static_cast<std::uint32_t> (value);
              numbered = true;
            }
          else if (tag == KTAG_PUSH)
            stack.push_back (state);
          else if (tag == KTAG_POP)
            {
              if (stack.empty ())
                return bad_descriptor ();
              state = stack.back ();
              stack.pop_back ();
            }
        }
      else if (type == KTYPE_MAIN
               && (tag == KTAG_INPUT || tag == KTAG_OUTPUT
                   || tag == KTAG_FEATURE))
        {
          if (state.report_id == 0)
            main_without_id = true;
          // Each factor is below 2^32: the product cannot overflow.
          std::uint64_t const bits
              = state.report_size_bits * state.report_count;
          std::map<std::uint32_t, std::uint64_t> *const reports
              = tag == KTAG_INPUT    ? &input_bits
                : tag == KTAG_OUTPUT ? &output_bits
                                     : nullptr;
          if (reports != nullptr)
            {
              std::uint64_t &total = (*reports)[state.report_id];
              if (bits > KMAX_REPORT_BITS - total)
                return bad_descriptor ();
              total += bits;
            }
        }
      pos += 1U + data_size;
    }

  // A device that numbers its reports numbers all of them (HID 1.11,
  // 6.2.2.7): a report before the first Report ID has no ID on the wire.
  if (numbered && main_without_id)
    return bad_descriptor ();

  report_layout_t layout;
  layout.numbered = numbered;
  layout.input_reports = to_sizes (input_bits);
  layout.output_reports = to_sizes (output_bits);
  return report_layout_result_t (layout);
}
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

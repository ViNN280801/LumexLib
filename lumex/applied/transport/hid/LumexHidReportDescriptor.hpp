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
 * @file LumexHidReportDescriptor.hpp
 * @brief The analysis of a HID report descriptor: whether the device numbers
 * its reports and the size of every input and output report.
 * @details hidapi hands a report of a device that numbers its reports with
 * the report ID in its first byte and a report of a device without numbered
 * reports without it (Windows drops the zero byte, hidraw never has it), and
 * a write always takes the report ID in the first byte (0 for a device
 * without numbered reports). The transport therefore reads the descriptor at
 * open and never guesses. The analysis walks the short and long items:
 * `Report Size`, `Report Count` and `Report ID` of the global state (with
 * `Push` and `Pop`), and adds `Report Size * Report Count` bits to the report
 * of the current ID for every `Input` and `Output` main item (constant
 * padding items too, as `InputReportByteLength` of the Windows HID API
 * does). Sizes are in bytes, rounded up, without the report ID byte. Port of
 * DChannel's `AnalyzeHidReportDescriptor` with sizes per report ID.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_HID_HID_REPORT_DESCRIPTOR_HPP
#define LUMEX_APPLIED_TRANSPORT_HID_HID_REPORT_DESCRIPTOR_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/expected/Expected"
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
namespace hid
{
/// @brief The size of one report.
struct report_size_t
{
  /// The report ID (0 when the device numbers no report).
  std::uint8_t report_id = 0;
  /// The size in bytes, without the report ID byte.
  std::size_t bytes = 0;
};

/// @brief What the report descriptor says about the reports.
struct report_layout_t
{
  /// The device numbers its reports (the descriptor has a `Report ID`
  /// item): every report starts with its ID on the wire.
  bool numbered = false;
  /// The input reports by ascending ID; a device without numbered reports
  /// has at most one, with ID 0.
  std::vector<report_size_t> input_reports;
  /// The output reports, as `input_reports`.
  std::vector<report_size_t> output_reports;

  /// @brief The size of input report `report_id`; 0 when there is none.
  std::size_t input_size (std::uint8_t report_id) const LUMEX_NOEXCEPT;
  /// @brief The size of output report `report_id`; 0 when there is none.
  std::size_t output_size (std::uint8_t report_id) const LUMEX_NOEXCEPT;
  /// @brief True when the device has output report `report_id` (an output
  /// report of size 0 counts as none).
  bool has_output (std::uint8_t report_id) const LUMEX_NOEXCEPT;
  /// @brief The largest input report.
  std::size_t max_input_size () const LUMEX_NOEXCEPT;
};

/// @brief The result of the analysis.
typedef lumex::core::expected::result::expected<report_layout_t,
                                                std::error_code>
    report_layout_result_t;

/**
 * @brief Analyzes a report descriptor (see the file description).
 * @param descriptor The bytes of the descriptor, as
 * `hid_get_report_descriptor` returns them.
 * @return The layout, or `transport_errc::bad_report_descriptor` when the
 * descriptor is empty, truncated (an item longer than the bytes left), has
 * `Report ID` 0 or above 255, a `Pop` without a `Push`, or a main item
 * before the first `Report ID` of a descriptor that numbers its reports, or
 * a report larger than 64 KiB.
 */
report_layout_result_t analyze_report_descriptor (const_bytes_t descriptor);
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_HID_HID_REPORT_DESCRIPTOR_HPP

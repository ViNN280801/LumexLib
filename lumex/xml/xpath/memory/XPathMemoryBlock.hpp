/**
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

#ifndef LUMEX_XML_XPATH_MEMORY_XPATH_MEMORY_BLOCK_HPP
#define LUMEX_XML_XPATH_MEMORY_XPATH_MEMORY_BLOCK_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstddef>

#include "lumex/xml/xpath/constants/XPathConstants.hpp"

using namespace lumex::xml::xpath::constants::Constants;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace memory
{
/**
 * @brief Represents a block of memory used by XPathAllocator.
 * @details This structure defines a contiguous block of memory that the
 * `XPathAllocator` uses for its allocations. Each block can link to the next,
 * forming a singly linked list of memory pages. The union is used to ensure
 * proper alignment of the data buffer for various data types.
 *
 * @note The `kxpath_memory_page_size` constant (defined in
 * `XPathConstants.hpp`) determines the fixed size of the data buffer within
 * each block.
 */
struct LUMEX_API XPathMemoryBlock
{
  /// @brief Pointer to the next memory block in the chain. `nullptr` if this
  /// is the last block.
  XPathMemoryBlock *next;
  /// @brief The total usable capacity of the `data` array in bytes.
  std::size_t capacity;

  union
  {
    /// @brief The raw character array used for general-purpose memory
    /// allocation.
    std::array<char, kxpath_memory_page_size> data;
    /// @brief A dummy member used to ensure the `data` array is aligned for
    /// `double`s.
    double alignment;
  };
};
} // namespace memory
} // namespace xpath
} // namespace xml
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_XML_XPATH_MEMORY_XPATH_MEMORY_BLOCK_HPP

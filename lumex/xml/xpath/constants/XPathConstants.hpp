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

#ifndef LUMEX_XML_XPATH_CONSTANTS_HPP
#define LUMEX_XML_XPATH_CONSTANTS_HPP

#include <cstdint>

#include "lumex/xml/utility/XmlMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace constants
{
namespace Constants
{
LUMEX_XML_CONSTANT std::size_t kxpath_memory_page_size = 0x1000; // 4 kb
LUMEX_XML_CONSTANT std::size_t kxpath_ast_depth_limit = 0x400;   // 1 kb
LUMEX_XML_CONSTANT uintptr_t kxpath_memory_block_alignment
    = sizeof (double) > sizeof (void *) ? sizeof (double) : sizeof (void *);
} // namespace Constants
} // namespace constants
} // namespace xpath
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_XPATH_CONSTANTS_HPP

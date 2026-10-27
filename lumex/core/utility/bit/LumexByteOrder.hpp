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
 * @file LumexByteOrder.hpp
 * @brief `byte_order`: the order of the bytes of a multi-byte value on a
 * wire or in a file.
 * @details A light header (no other include) shared by the frame codecs of
 * `core/framing` (`framing::codec::byte_order` names this type) and the
 * field types of `core/protocol`, so that a protocol table does not need the
 * framing module for one enumeration.
 */
#ifndef LUMEX_CORE_UTILITY_BIT_BYTE_ORDER_HPP
#define LUMEX_CORE_UTILITY_BIT_BYTE_ORDER_HPP

namespace lumex
{
namespace core
{
namespace utility
{
namespace bit
{
/// @brief The order of the bytes of a multi-byte value on the wire.
enum class byte_order
{
  /// Most significant byte first (network order).
  big,
  /// Least significant byte first.
  little
};
} // namespace bit
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_BIT_BYTE_ORDER_HPP

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
 * @file LumexAsioErrors.hpp
 * @brief The translation of Asio's own error codes at the transport boundary.
 * @details A process that also loads DChannel holds two copies of Asio, each
 * with its own category objects, so a code of an Asio category is never
 * compared by a caller: `operation_aborted` becomes
 * `transport_errc::cancelled`, `eof` becomes `disconnected`, a name resolution
 * failure `invalid_endpoint`, and an operating system error of Asio's own
 * "asio.system" category keeps its value in `std::system_category`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_ASIO_ERRORS_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_ASIO_ERRORS_HPP

#include <system_error>

namespace lumex
{
namespace applied
{
namespace transport
{
namespace detail
{
/// @brief The code a caller sees for a code Asio reported.
std::error_code from_asio (std::error_code const &code);
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_ASIO_ERRORS_HPP

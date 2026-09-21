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

/**
 * @file LumexSerialPort.hpp
 * @brief Resolves short, configuration-style serial-port names into the
 * platform-specific path or identifier a communication channel actually needs
 * in order to open them.
 * @details Handles the differences between how Windows, Linux and macOS name
 * serial ports (`COM1` vs.
 *          `/dev/ttyUSB0` vs. `/dev/serial/by-id/...`), plus a couple of
 * historical fallback quirks (see @ref
 * lumex::applied::serial::resolve_serial_port_path for the full list) that
 * keep a slightly malformed configuration value from failing outright.
 */
#ifndef LUMEX_APPLIED_SERIAL_PORT_HPP
#define LUMEX_APPLIED_SERIAL_PORT_HPP

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <functional>
#include <string>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
/**
 * @brief Cross-platform helpers for naming/resolving serial (and
 * serial-adjacent) communication ports.
 */
namespace serial
{
namespace port
{
/**
 * @brief Numeric channel-type identifiers this module understands.
 * @details These mirror the values a serial/network communication library
 * (such as Lumex's own DChannel) typically assigns to its channel types. This
 * module does not depend on any particular communication library - it only
 * needs to tell a serial channel apart from a TCP/UDP one, so the values are
 * duplicated here rather than pulled in via a dependency. If the values below
 * and DChannel's own `DCT_*` macros ever drift apart, callers that bridge the
 * two should pass their own channel-type constant through instead of relying
 * on these.
 */
namespace Constants
{
LUMEX_CONST_NUM std::uint32_t KSERIAL_PORT_CHANNEL_TYPE
    = 0x100; ///< Asynchronous serial port.
LUMEX_CONST_NUM std::uint32_t KSERIAL_PORT_SYNC_CHANNEL_TYPE
    = 0x101; ///< Synchronous serial port.
LUMEX_CONST_NUM std::uint32_t KNET_TCP_CHANNEL_TYPE
    = 0x810; ///< TCP network channel.
LUMEX_CONST_NUM std::uint32_t KNET_UDP_CHANNEL_TYPE
    = 0x820; ///< UDP network channel.
} // namespace Constants

/**
 * @brief Converts a short serial-port name into the full, platform-specific
 * path a channel needs in order to actually open it.
 *
 * @details Handles several distinct name shapes:
 * - Windows COM port: `"COM1"`, `"COM2"` -> `"\\\\.\\COM1"`, `"\\\\.\\COM2"`.
 * - Linux tty device: `"ttyUSB0"`, `"ttyACM0"` -> `"/dev/ttyUSB0"`,
 * `"/dev/ttyACM0"`.
 * - Linux by-id device: `"usb-FTDI_..."` ->
 * `"/dev/serial/by-id/usb-FTDI_..."`.
 * - Invalid `"USB"`-style markers on Linux: `"USB"` -> `"/dev/ttyUSB0"`,
 * `"USB42"` -> `"/dev/ttyUSB42"` (a fallback for configuration values that
 * name a USB slot instead of a real device node).
 *
 * @param portName Short port name as it appears in configuration (e.g.
 * `"COM1"`, `"ttyUSB0"`,
 *                 `"USB"`, `"USB42"`).
 * @param channelType Channel-type identifier (see @ref Constants). Only used
 * to detect a network channel, in which case the port name is not a serial
 * port name at all and `getIpAddressCallback` is consulted instead.
 * @param getIpAddressCallback Callback used to obtain an IP address for
 * network channels
 *                             (`Constants::KNET_TCP_CHANNEL_TYPE` /
 * `Constants::KNET_UDP_CHANNEL_TYPE`). May be empty; if so, `portName` itself
 * is returned unchanged.
 * @param logWarningCallback Optional callback used to report a
 * resolved-but-unverified fallback path (see the warning below). Ignored on
 * platforms where the fallback path never triggers.
 *
 * @return The platform-specific path/identifier to use when opening the
 * channel.
 *
 * @note For network channels, the result is whatever `getIpAddressCallback`
 * returns.
 *
 * @note For invalid `"USB"`-style markers on Linux:
 *       - `"USB"` (no trailing number) resolves to `"/dev/ttyUSB0"` (default
 * fallback index).
 *       - `"USB42"` (trailing number) resolves to `"/dev/ttyUSB42"` (uses the
 * given index).
 *       - When this fallback fires, `logWarningCallback` (if provided) is
 * invoked with a human-readable description of what happened.
 *
 * @warning For invalid `"USB"` markers, this function returns a *guessed*
 * device path - it never checks whether that device actually exists or is
 * available. If `/dev/ttyUSB0` is already used by another device, or
 * `/dev/ttyUSB42` does not exist, opening the resulting channel will fail.
 * Prefer real port names (`"ttyUSB0"`, `"usb-FTDI_..."`) over slot markers
 * (`"USB"`, `"USB42"`) whenever possible.
 */
LUMEX_PUBLIC_API
std::string resolve_serial_port_path (
    std::string const &portName, std::uint32_t channelType,
    std::function<std::string ()> const &getIpAddressCallback = nullptr,
    LUMEX_ATTRIBUTE_MAYBE_UNUSED
        std::function<void (std::string const &)> const &logWarningCallback
    = nullptr) LUMEX_NOEXCEPT;

} // namespace port
} // namespace serial
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_SERIAL_PORT_HPP

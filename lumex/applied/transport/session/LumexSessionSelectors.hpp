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
 * @file LumexSessionSelectors.hpp
 * @brief Device selectors (spec G1): a serial port by name or by the USB
 * VID/PID/serial number of its adapter, a network peer by host and port; each
 * makes the factory of a session.
 * @details A serial selector by USB identity finds its port through the
 * serial enumeration (`applied/serial`) at every open, so a port that came
 * back under another name (`COM7` -> `COM9`) is found again. On Windows the
 * enumeration leaves `vid`, `pid` and `serial_number` of
 * `serial_port_info_t` empty: `usb_identity` reads them from the hardware ID
 * ("USB\\VID_16E0&PID_0001&REV_0100") and the device instance ID
 * ("USB\\VID_16E0&PID_0001\\SN123"; for the FTDI bus driver
 * "FTDIBUS\\VID_0403+PID_6001+B71946KXA\\0000" gives `B71946KXA`, the serial
 * the driver reports with its channel letter). The port of a composite
 * device (`&MI_xx`) has no serial number of its own there.
 *
 * The enumeration of the selectors and of `device_watcher` does not open the
 * ports (`enumerate_serial_ports_without_probe`): an open of every port
 * would pulse the modem lines of each device and collide with the open of a
 * session. The factory by USB identity does not enumerate either (a session
 * runs its factory on its strand at every attempt): the stream it makes
 * enumerates at each open, on the open's worker thread
 * (`serial_stream_options_t::resolve_path`). A session by path does not
 * enumerate.
 *
 * A selector by path matches the port of the same node: on POSIX both paths
 * are resolved through their symbolic links first (`realpath`), so
 * `/dev/serial/by-id/usb-FTDI_...-if00-port0` selects the `/dev/ttyUSB0`
 * the enumeration lists while the link exists; on Windows the `\\.\` and
 * `\\?\` prefixes (also with forward slashes) are dropped and `COM` names
 * compared without case.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_SELECTORS_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_SELECTORS_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/serial/LumexSerialStream.hpp"
#include "lumex/applied/transport/session/LumexMessageSession.hpp"
#include "lumex/applied/transport/session/LumexStreamSession.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"

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
namespace session
{
/// @brief One serial port of the enumeration.
using serial_port_info_t
    = lumex::applied::serial::enumeration::serial_port_info_t;

/// @brief The serial ports present now (the seam of selectors and of
/// `device_watcher`; tests give a fake).
using serial_enumeration_t = std::function<std::vector<serial_port_info_t> ()>;

/// @brief `enumerate_serial_ports_without_probe` with the openable paths
/// ("\\\\.\\COM12", "/dev/ttyACM0"): lists the ports without opening
/// them.
serial_enumeration_t default_serial_enumeration ();

/**
 * @brief A serial port to open: by `path`, or by the USB identity of its
 * adapter. With `path` set, the rest is not looked at; otherwise each
 * non-empty field of `vid`, `pid`, `serial_number` must match, and a
 * selector without any of them matches no port.
 */
struct serial_selector_t
{
  /// "COM3", "\\\\.\\COM3", "/dev/ttyUSB0", "/dev/serial/by-id/..." (see the
  /// file description for how it is compared).
  std::string path;
  /// USB vendor ID, hexadecimal ("16E0", "0x16e0").
  std::string vid;
  /// USB product ID, hexadecimal.
  std::string pid;
  /// USB serial number (compared without case).
  std::string serial_number;
  /// The settings the stream applies after the open.
  serial::serial_settings_t settings;
  /// The longest the open of the device may take.
  deadline_t open_deadline = deadline_t (3000);
};

/// @brief The USB identity of a port, as far as the enumeration tells it.
struct usb_identity_t
{
  /// Four upper-case hexadecimal digits, or empty.
  std::string vid;
  /// Four upper-case hexadecimal digits, or empty.
  std::string pid;
  /// The serial number, or empty.
  std::string serial_number;
};

/// @brief The USB identity of `port` (its own fields, else parsed from the
/// hardware and instance IDs; see the file description).
usb_identity_t usb_identity (serial_port_info_t const &port);

/// @brief True when `selector` names `port`.
bool matches (serial_selector_t const &selector,
              serial_port_info_t const &port);

/// @brief The path of the first of `ports` that `selector` names (the path
/// of the selector itself when it has one), or empty.
std::string find_serial_port (serial_selector_t const &selector,
                              std::vector<serial_port_info_t> const &ports);

/// @brief A readable form of the selector for logs ("COM3",
/// "USB VID 16E0 PID 0001 SN 123").
std::string describe (serial_selector_t const &selector);

/**
 * @brief The factory of a `stream_session` over the serial port `selector`
 * names. By USB identity the stream it makes (named `describe (selector)`)
 * finds its port at each open through `enumeration` (empty:
 * `default_serial_enumeration ()`); the open fails with
 * `transport_errc::invalid_endpoint` when no port matches (a failed
 * attempt). The factory itself does not enumerate.
 *
 * `enumeration` runs on the worker thread of each open, so it may run on
 * several threads at once (the sessions sharing the factory, an attempt and
 * the late open of the one before it): it must be thread-safe. An
 * enumeration that hangs leaves its worker thread (detached) behind, one per
 * attempt; the session gives up the attempt at its `open_timeout`, the
 * thread stays until the enumeration returns.
 */
stream_session::factory_t
make_serial_factory (serial_selector_t const &selector,
                     serial_enumeration_t enumeration
                     = serial_enumeration_t ());

/// @brief A network peer.
struct net_selector_t
{
  /// Host name or address.
  std::string host;
  /// Port number.
  std::uint16_t port = 0;
};

/// @brief The factory of a `stream_session` over TCP to `selector`; the host
/// and port of `options` are replaced by the selector's.
stream_session::factory_t make_tcp_factory (net_selector_t const &selector,
                                            net::tcp_options_t options
                                            = net::tcp_options_t ());

/// @brief The factory of a `message_session` over UDP to `selector`; the
/// host and port of `options` are replaced by the selector's.
message_session::factory_t make_udp_factory (net_selector_t const &selector,
                                             net::udp_options_t options
                                             = net::udp_options_t ());
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_SELECTORS_HPP

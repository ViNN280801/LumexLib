/**
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

/**
 * @file LumexSerialPortEnumeration.hpp
 * @brief Enumerates serial ports and reports structured device information.
 *
 * @details Two consumer surfaces:
 * 1. `enumerate_serial_ports_detailed` fills a `serial_port_info_t` per port
 *    (path, state, friendly name, USB identifiers, holder text).
 * 2. `print_serial_ports_info` writes a grouped table to any
 *    `std::basic_ostream`.
 *
 * Platform sources:
 * - Windows: SetupAPI classes `Ports` and `Modem`, registry `PortName`,
 *   `SPDRP_*` properties. Fallback: `COM1`..`COM255` via `CreateFileA`.
 * - POSIX: `/sys/class/tty` with USB sysfs attributes, then `/dev`.
 *
 * Enumeration is not thread-safe against a concurrent scan of the same
 * device set. Opening each port to classify its state can be slow on
 * Windows when many COM names exist.
 *
 * Bluetooth SPP/LE virtual COM ports (`BTHENUM` / `BTHLEENUM`) sit in the
 * same SetupAPI `"Ports"` class as real serial devices. Opening a paired
 * but disconnected Bluetooth port blocks in the radio stack
 * (`ERROR_SEM_TIMEOUT`). Pass `need_to_filter_bluetooth = true` to drop
 * those ports from the result. The default is `false` so callers that need
 * the Bluetooth COM names still receive them. Included Bluetooth ports are
 * not open-probed: state is `available` from SetupAPI presence only.
 * `is_bluetooth_enumerated_port` is the same enumerator-name predicate and
 * can be called on its own; it never opens the port.
 */
#ifndef LUMEX_APPLIED_SERIAL_ENUMERATION_HPP
#define LUMEX_APPLIED_SERIAL_ENUMERATION_HPP

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

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace applied
{
namespace serial
{
namespace enumeration
{
/**
 * @brief Result of an open-probe performed during enumeration.
 *
 * - `available`: open succeeded and the handle was closed immediately.
 * - `busy`: `ERROR_ACCESS_DENIED` / `ERROR_SHARING_VIOLATION` / `EBUSY` /
 *   `EACCES`.
 * - `free`: the device is absent or another error occurred.
 */
enum class serial_port_state
{
  free,
  busy,
  available
};

/**
 * @brief Convert `state` to a stable ASCII token.
 * @return `"Available"`, `"Busy"`, `"Free"`, or `"Unknown"`.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "Serial port state string is required for logging and diagnostics.")
LUMEX_PUBLIC_API char const *
serial_port_state_to_string (serial_port_state state) LUMEX_NOEXCEPT;

/**
 * @brief Structured description of one serial port discovered on this host.
 *
 * Unknown fields stay empty. Windows fills SetupAPI/registry properties;
 * Linux fills sysfs USB attributes when the node is a USB serial device.
 */
struct serial_port_info_t
{
  std::string path;
  serial_port_state state;
  std::string friendly_name;
  std::string hardware_id;
  std::string vid;
  std::string pid;
  std::string manufacturer;
  std::string product;
  std::string serial_number;
  std::string driver;
  std::string bus_info;
  std::string device_class;
  std::string instance_id;
  std::string revision;
  std::string holder_process_info;
  std::string device_details;
  std::string system_error;

  serial_port_info_t () : state (serial_port_state::available) {}
};

/**
 * @brief Enumerate serial ports with full `serial_port_info_t` records.
 *
 * Windows walks SetupAPI, then falls back to `COM1`..`COM255` when that
 * walk is empty. POSIX walks `/sys/class/tty` (prefixes `ttyS`, `ttyUSB`,
 * `ttyACM`, `ttyAMA`, `ttyXRUSB`, `ttyO`) and falls back to `/dev`.
 *
 * Each non-Bluetooth port is opened to classify `state`. `connected_port`
 * is treated as held by this process so Windows does not call
 * `NtQuerySystemInformation` against a handle this process already owns.
 * Bluetooth ports that remain after the filter are not opened.
 *
 * @param connected_port Short name or path of a port this process already
 * has open. Empty means none.
 * @param need_to_filter_bluetooth When `true`, Windows drops ports whose
 * SetupAPI enumerator name starts with `"BTH"` (Bluetooth SPP/LE). Default
 * `false` keeps those ports in the result. POSIX ignores the flag: BlueZ
 * pairing does not create `/dev/rfcommN` without an explicit `rfcomm bind`,
 * and `rfcomm` is not in the scanned prefix list.
 * @return Ports sorted by COM number on Windows and by path on POSIX.
 * Empty when nothing was found or the scan failed.
 */
LUMEX_PUBLIC_API
std::vector<serial_port_info_t>
enumerate_serial_ports_detailed (std::string const &connected_port
                                 = std::string (),
                                 bool need_to_filter_bluetooth = false);

/**
 * @brief Enumerate serial port paths only.
 * @param need_to_filter_bluetooth Forwarded to
 * `enumerate_serial_ports_detailed`.
 */
LUMEX_PUBLIC_API
std::vector<std::string>
enumerate_serial_port_names (bool need_to_filter_bluetooth = false);

/**
 * @brief True when Windows registered `port_name` through a Bluetooth
 * enumerator (`BTHENUM` / `BTHLEENUM`).
 *
 * The check is a SetupAPI walk only; it never opens the port. Safe to call
 * repeatedly while scanning `COM1`..`COM256`. Always `false` on non-Windows.
 *
 * @param port_name Short name (`"COM3"`) or path (`"\\\\.\\COM3"`).
 */
LUMEX_PUBLIC_API
bool
is_bluetooth_enumerated_port (std::string const &port_name) LUMEX_NOEXCEPT;

/**
 * @brief Write a grouped port table to `os`.
 *
 * Sections: available, busy, free. Busy rows append `holder_process_info`
 * when it is non-empty.
 */
template <typename CharT, typename Traits>
void print_serial_ports_info (std::basic_ostream<CharT, Traits> &os,
                              std::vector<serial_port_info_t> const &infos);

template <typename CharT, typename Traits>
void
print_serial_ports_info (std::basic_ostream<CharT, Traits> &os,
                         std::vector<serial_port_info_t> const &infos)
{
  std::vector<serial_port_info_t const *> by_free;
  std::vector<serial_port_info_t const *> by_busy;
  std::vector<serial_port_info_t const *> by_available;

  for (std::vector<serial_port_info_t>::const_iterator it = infos.begin ();
       it != infos.end (); ++it)
    {
      if (it->state == serial_port_state::free)
        by_free.push_back (&(*it));
      else if (it->state == serial_port_state::busy)
        by_busy.push_back (&(*it));
      else
        by_available.push_back (&(*it));
    }

  os << "\n=== Ports: available (device connected, port free) ===\n";
  for (std::size_t i = 0; i < by_available.size (); ++i)
    {
      serial_port_info_t const &info = *by_available[i];
      os << "  " << info.path.c_str ();
      if (!info.friendly_name.empty ())
        os << "  [" << info.friendly_name.c_str () << "]";
      if (!info.vid.empty () || !info.pid.empty ())
        os << "  VID:" << info.vid.c_str () << " PID:" << info.pid.c_str ();
      if (!info.manufacturer.empty ())
        os << "  Mfg:" << info.manufacturer.c_str ();
      if (!info.product.empty ())
        os << "  Product:" << info.product.c_str ();
      if (!info.serial_number.empty ())
        os << "  SN:" << info.serial_number.c_str ();
      if (!info.driver.empty ())
        os << "  Driver:" << info.driver.c_str ();
      if (!info.bus_info.empty ())
        os << "  Bus:" << info.bus_info.c_str ();
      if (info.vid.empty () && info.manufacturer.empty ()
          && !info.device_details.empty ())
        os << "  " << info.device_details.c_str ();
      if (!info.system_error.empty ())
        os << "  Error:" << info.system_error.c_str ();
      os << "\n";
    }

  os << "\n=== Ports: busy (used by another process) ===\n";
  for (std::size_t i = 0; i < by_busy.size (); ++i)
    {
      serial_port_info_t const &info = *by_busy[i];
      os << "  " << info.path.c_str ();
      if (!info.friendly_name.empty ())
        os << "  [" << info.friendly_name.c_str () << "]";
      if (!info.holder_process_info.empty ())
        os << "  | " << info.holder_process_info.c_str ();
      os << "\n";
    }

  os << "\n=== Ports: free (no physical connection) ===\n";
  for (std::size_t i = 0; i < by_free.size (); ++i)
    {
      serial_port_info_t const &info = *by_free[i];
      os << "  " << info.path.c_str ();
      os << "\n";
    }

  os << "(total: " << infos.size () << ")\n" << std::flush;
}

} // namespace enumeration
} // namespace serial
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif

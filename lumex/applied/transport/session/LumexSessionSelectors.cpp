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

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <stdlib.h>
#endif

#include "lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp"
#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/serial/LumexSerialStream.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"

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
namespace session
{
namespace
{
using error::transport_errc;

char
upper (char c) LUMEX_NOEXCEPT
{
  return static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
}

std::string
to_upper (std::string text)
{
  for (std::size_t i = 0; i < text.size (); ++i)
    text[i] = upper (text[i]);
  return text;
}

bool
equal_no_case (std::string const &lhs, std::string const &rhs)
{
  if (lhs.size () != rhs.size ())
    return false;
  for (std::size_t i = 0; i < lhs.size (); ++i)
    if (upper (lhs[i]) != upper (rhs[i]))
      return false;
  return true;
}

bool
starts_with_no_case (std::string const &text, std::string const &prefix)
{
  return text.size () >= prefix.size ()
         && equal_no_case (text.substr (0, prefix.size ()), prefix);
}

// The node a path names, through its symbolic links (POSIX realpath; a
// name without a directory is one of /dev); the path itself when it does not
// resolve (the node is gone) or on Windows.
std::string
canonical_node (std::string const &path)
{
#if defined(_WIN32)
  return path;
#else
  if (path.empty ())
    return path;
  std::string const full
      = path.find ('/') == std::string::npos ? "/dev/" + path : path;
  char *const resolved = ::realpath (full.c_str (), nullptr);
  if (resolved == nullptr)
    return path;
  std::string const node (resolved);
  std::free (resolved);
  return node;
#endif
}

// "\\.\COM3", "\\?\COM3", "//./COM3" -> "COM3"; "/dev/ttyUSB0" (or a
// link to it) -> "ttyUSB0"; "com3" -> "COM3".
std::string
normalize_path (std::string const &path)
{
  std::string name = canonical_node (path);
  if (name.compare (0, 4, "\\\\.\\") == 0
      || name.compare (0, 4, "\\\\?\\") == 0
      || name.compare (0, 4, "//./") == 0 || name.compare (0, 4, "//?/") == 0)
    name = name.substr (4);
  else if (name.compare (0, 5, "/dev/") == 0)
    name = name.substr (5);
  if (starts_with_no_case (name, "COM"))
    name = to_upper (name);
  return name;
}

bool
is_hex (char c) LUMEX_NOEXCEPT
{
  return std::isxdigit (static_cast<unsigned char> (c)) != 0;
}

// The four hexadecimal digits after `key` ("VID_") in `text`, upper case.
std::string
hex_after (std::string const &text, char const *key)
{
  std::string const upper_text = to_upper (text);
  std::string const upper_key = to_upper (key);
  std::size_t const at = upper_text.find (upper_key);
  if (at == std::string::npos)
    return std::string ();
  std::size_t const begin = at + upper_key.size ();
  if (begin + 4 > upper_text.size ())
    return std::string ();
  for (std::size_t i = begin; i < begin + 4; ++i)
    if (!is_hex (upper_text[i]))
      return std::string ();
  return upper_text.substr (begin, 4);
}

// A selector's ID: "0x16e0" -> "16E0" (left as it is otherwise, upper case).
std::string
normalize_id (std::string const &id)
{
  std::string text = id;
  if (starts_with_no_case (text, "0X"))
    text = text.substr (2);
  text = to_upper (text);
  while (text.size () < 4)
    text = "0" + text;
  return text;
}

std::vector<std::string>
split (std::string const &text, char separator)
{
  std::vector<std::string> parts;
  std::size_t begin = 0;
  for (;;)
    {
      std::size_t const end = text.find (separator, begin);
      if (end == std::string::npos)
        {
          parts.push_back (text.substr (begin));
          return parts;
        }
      parts.push_back (text.substr (begin, end - begin));
      begin = end + 1;
    }
}

// The serial number in a Windows device instance ID (see the header).
std::string
serial_from_instance_id (std::string const &instance_id)
{
  std::vector<std::string> const parts = split (instance_id, '\\');
  if (parts.size () != 3)
    return std::string ();
  std::string const &bus = parts[0];
  std::string const &ids = parts[1];
  if (equal_no_case (bus, "USB"))
    {
      if (hex_after (ids, "VID_").empty ()
          || to_upper (ids).find ("&MI_") != std::string::npos
          || parts[2].find ('&') != std::string::npos || parts[2].empty ())
        return std::string ();
      return parts[2];
    }
  if (equal_no_case (bus, "FTDIBUS"))
    {
      std::vector<std::string> const fields = split (ids, '+');
      if (fields.size () >= 3 && !fields[2].empty ())
        return fields[2];
    }
  return std::string ();
}
} // namespace

serial_enumeration_t
default_serial_enumeration ()
{
  return [] ()
    {
      return lumex::applied::serial::enumeration::
          enumerate_serial_ports_without_probe (false, true);
    };
}

usb_identity_t
usb_identity (serial_port_info_t const &port)
{
  usb_identity_t identity;
  identity.vid = port.vid.empty () ? std::string () : normalize_id (port.vid);
  identity.pid = port.pid.empty () ? std::string () : normalize_id (port.pid);
  identity.serial_number = port.serial_number;
  if (identity.vid.empty ())
    {
      identity.vid = hex_after (port.hardware_id, "VID_");
      if (identity.vid.empty ())
        identity.vid = hex_after (port.instance_id, "VID_");
    }
  if (identity.pid.empty ())
    {
      identity.pid = hex_after (port.hardware_id, "PID_");
      if (identity.pid.empty ())
        identity.pid = hex_after (port.instance_id, "PID_");
    }
  if (identity.serial_number.empty ())
    identity.serial_number = serial_from_instance_id (port.instance_id);
  return identity;
}

bool
matches (serial_selector_t const &selector, serial_port_info_t const &port)
{
  if (!selector.path.empty ())
    return normalize_path (selector.path) == normalize_path (port.path);
  if (selector.vid.empty () && selector.pid.empty ()
      && selector.serial_number.empty ())
    return false;
  usb_identity_t const identity = usb_identity (port);
  if (!selector.vid.empty () && normalize_id (selector.vid) != identity.vid)
    return false;
  if (!selector.pid.empty () && normalize_id (selector.pid) != identity.pid)
    return false;
  if (!selector.serial_number.empty ()
      && !equal_no_case (selector.serial_number, identity.serial_number))
    return false;
  return true;
}

std::string
find_serial_port (serial_selector_t const &selector,
                  std::vector<serial_port_info_t> const &ports)
{
  if (!selector.path.empty ())
    return selector.path;
  for (std::size_t i = 0; i < ports.size (); ++i)
    if (matches (selector, ports[i]))
      return ports[i].path;
  return std::string ();
}

std::string
describe (serial_selector_t const &selector)
{
  if (!selector.path.empty ())
    return selector.path;
  std::string text = "USB";
  if (!selector.vid.empty ())
    text += " VID " + normalize_id (selector.vid);
  if (!selector.pid.empty ())
    text += " PID " + normalize_id (selector.pid);
  if (!selector.serial_number.empty ())
    text += " SN " + selector.serial_number;
  return text;
}

stream_session::factory_t
make_serial_factory (serial_selector_t const &selector,
                     serial_enumeration_t enumeration)
{
  if (!enumeration)
    enumeration = default_serial_enumeration ();
  serial::serial_stream_options_t options;
  options.settings = selector.settings;
  options.open_deadline = selector.open_deadline;
  if (!selector.path.empty ())
    options.path = selector.path;
  else
    {
      // The port is found at each open, on the open's worker thread, not
      // here: a session runs its factory on its strand at every attempt.
      options.path = describe (selector);
      options.resolve_path = [selector, enumeration] ()
        { return find_serial_port (selector, enumeration ()); };
    }
  return [options] (executor_type const &executor) -> stream::byte_stream
           { return serial::make_serial_stream (executor, options); };
}

stream_session::factory_t
make_tcp_factory (net_selector_t const &selector, net::tcp_options_t options)
{
  options.host = selector.host;
  options.port = selector.port;
  return [options] (executor_type const &executor)
    { return net::make_tcp_stream (executor, options); };
}

message_session::factory_t
make_udp_factory (net_selector_t const &selector, net::udp_options_t options)
{
  options.host = selector.host;
  options.port = selector.port;
  return [options] (executor_type const &executor)
    { return net::make_udp_socket (executor, options); };
}
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

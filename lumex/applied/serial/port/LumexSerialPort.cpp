#define LUMEX_IMPLEMENTATION
#include <cctype>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#include "LumexSerialPort.hpp"

namespace lumex
{
namespace applied
{
namespace serial
{
namespace port
{
namespace
{
/// @brief C++11-portable `starts_with` (the standard one only arrives in
/// C++20).
bool
_starts_with (std::string const &value, std::string const &prefix)
{
  return value.compare (0, prefix.size (), prefix) == 0;
}

void
_warn_invalid_port (
    std::function<void (std::string const &)> const &logWarningCallback,
    std::string const &portName, std::string const &resolvedPath)
{
  if (!logWarningCallback)
    return;
  logWarningCallback ("Port name '" + portName
                      + "' is invalid for a serial channel. Resolving to '"
                      + resolvedPath
                      + "' (the device may not exist or may be unavailable)");
}
} // namespace

LUMEX_PUBLIC_API
std::string
resolve_serial_port_path (
    std::string const &portName, std::uint32_t channelType,
    std::function<std::string ()> const &getIpAddressCallback,
    std::function<void (std::string const &)> const &logWarningCallback)
    LUMEX_NOEXCEPT
{
  // Windows COM port: "COM1", "COM2" -> "\\.\COM1", "\\.\COM2"
  if (portName.length () >= 3 && _starts_with (portName, "COM"))
    return R"(\\.\)" + portName;

  // Linux tty device: "ttyUSB0", "ttyACM0" -> "/dev/ttyUSB0", "/dev/ttyACM0"
  if (_starts_with (portName, "tty"))
    return "/dev/" + portName;

  // Network channel: TCP or UDP.
  if (channelType == Constants::KNET_TCP_CHANNEL_TYPE
      || channelType == Constants::KNET_UDP_CHANNEL_TYPE)
    {
      // The channel "name" for a network connection is really an IP address.
      if (getIpAddressCallback)
        return getIpAddressCallback ();
      return portName; // Fallback: return the name as-is if no callback was
                       // provided.
    }

  // Bare "USB"/"USB1"/"USB34" is a slot marker, not a device name.
  // Valid names ("ttyUSB0", "usb-FTDI_...") are handled above or below.
  if (_starts_with (portName, "USB") && !_starts_with (portName, "ttyUSB")
      && !_starts_with (portName, "usb-"))
    {
      // "USB42" -> "42"; a bare "USB" uses index 0.
      std::string usbNumber = "0";
      if (portName.length () > 3)
        {
          // Anything after "USB" is expected to be a number: "USB42" -> "42".
          std::string const numberPart = portName.substr (3);

          bool isAllDigits = true;
          for (char const chr : numberPart)
            {
              if (std::isdigit (static_cast<unsigned char> (chr)) == 0)
                {
                  isAllDigits = false;
                  break;
                }
            }
          if (isAllDigits && !numberPart.empty ())
            usbNumber = numberPart;
        }

        // Linux can guess a ttyUSB node. Windows has no such node, so the
        // name is left as given. macOS keeps the usual /dev/ prefix.
#if !defined(_WIN32) && !defined(__APPLE__)
      std::string const resolvedPath = "/dev/ttyUSB" + usbNumber;
#elif defined(__APPLE__)
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (usbNumber);
      std::string const resolvedPath = "/dev/" + portName;
#else
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (usbNumber);
      std::string const resolvedPath = portName;
#endif
      _warn_invalid_port (logWarningCallback, portName, resolvedPath);
      return resolvedPath;
    }

// Everything else.
#if defined(_WIN32)
  return portName;
#elif defined(__APPLE__)
  return "/dev/" + portName;
#else
  // Linux: a "tty"-prefixed name goes under /dev/ directly, anything else is
  // assumed to be a by-id name.
  if (_starts_with (portName, "tty"))
    {
      return "/dev/" + portName;
    } // "ttyUSB0"/"ttyACM0"/"ttyS0"
  return "/dev/serial/by-id/" + portName; // "usb-FTDI_...-if00-port0"
#endif
}
} // namespace port
} // namespace serial
} // namespace applied
} // namespace lumex

#define LUMEX_IMPLEMENTATION
#include <cctype>

#include "LumexSerialPort.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
} // namespace

LUMEX_PUBLIC_API
std::string
resolve_serial_port_path (
    std::string const &portName, std::uint32_t channelType,
    std::function<std::string ()> const &getIpAddressCallback,
    std::function<void (std::string const &)> const &) LUMEX_NOEXCEPT
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

// Some serial-device configurations on Linux use a bare "USB"/"USB1"/"USB34"
// marker instead of a real port name. These are not valid serial-port names,
// so they get substituted below.
#if !defined(_WIN32) && !defined(__APPLE__)
  // Does the name start with "USB" without being an already-valid device name?
  // Valid: "ttyUSB0", "ttyUSB1", "usb-FTDI_...-if00-port0", etc.
  // Invalid: "USB", "USB1", "USB34", etc.
  if (_starts_with (portName, "USB") && !_starts_with (portName, "ttyUSB")
      && !_starts_with (portName, "usb-"))
    {
      // Extract the trailing number from "USB42" -> "42", or use "0" for a
      // bare "USB".
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

      std::string const resolvedPath = "/dev/ttyUSB" + usbNumber;

      if (logWarningCallback)
        {
          logWarningCallback (
              "Port name '" + portName
              + "' is invalid for a serial channel. Resolving to '"
              + resolvedPath
              + "' (the device may not exist or may be unavailable)");
        }
      return resolvedPath;
    }
#endif

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

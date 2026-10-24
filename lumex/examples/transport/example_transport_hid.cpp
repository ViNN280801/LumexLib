// USB HID devices with lumex::transport. Without an argument: lists the HID
// interfaces of this machine (the enumeration opens no device for use; on
// Windows hidapi opens each interface with zero access to read its strings)
// and exits 0 also when there are none, so the example runs anywhere without
// hardware. "--count" prints only how many there are (the CTest run: no device
// string of the machine reaches a log). With a
// selector, "VVVV:PPPP" or "VVVV:PPPP:serial" (hexadecimal IDs), or a
// platform path: opens the device through a hid_selector_t, so the device is
// found by what it is and not by where it is, and prints the report layout the
// descriptor analysis found. Nothing is written to the device.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;

#if defined(LUMEX_TRANSPORT_WITH_HID)
namespace hid = tr::hid;

namespace
{
std::string
hex4 (unsigned value)
{
  char text[8];
  std::snprintf (text, sizeof text, "%04X", value);
  return text;
}

// Only the count: no device string of the machine reaches a log (the CTest
// run uses this).
void
count_devices ()
{
  std::cout << hid::enumerate_hid_devices ().size () << " HID interface(s)\n";
}

void
list_devices ()
{
  std::vector<hid::hid_device_info_t> const devices
      = hid::enumerate_hid_devices ();
  if (devices.empty ())
    {
      std::cout << "no HID device found\n";
      return;
    }
  std::cout << devices.size () << " HID interface(s):\n";
  for (std::size_t i = 0; i < devices.size (); ++i)
    {
      hid::hid_device_info_t const &device = devices[i];
      std::cout << "  " << hex4 (device.vendor_id) << ':'
                << hex4 (device.product_id) << "  usage page "
                << hex4 (device.usage_page) << " usage " << hex4 (device.usage)
                << "  interface " << device.interface_number;
      if (!device.serial_number.empty ())
        std::cout << "  serial " << device.serial_number;
      if (!device.product.empty ())
        std::cout << "  \"" << device.product << '"';
      std::cout << "\n    " << device.path << '\n';
    }
}

// "VVVV:PPPP[:serial]" -> a selector by identity; anything else is a path.
hid::hid_selector_t
parse_selector (std::string const &text)
{
  hid::hid_selector_t selector;
  std::size_t const colon = text.find (':');
  if (colon == std::string::npos || colon == 0 || colon > 4)
    {
      selector.path = text;
      return selector;
    }
  std::size_t const second = text.find (':', colon + 1);
  std::string const product = text.substr (
      colon + 1,
      second == std::string::npos ? std::string::npos : second - colon - 1);
  selector.vendor_id = static_cast<std::uint16_t> (
      std::strtoul (text.substr (0, colon).c_str (), nullptr, 16));
  selector.product_id = static_cast<std::uint16_t> (
      std::strtoul (product.c_str (), nullptr, 16));
  if (second != std::string::npos)
    selector.serial_number = text.substr (second + 1);
  return selector;
}

void
print_reports (char const *title,
               std::vector<hid::report_size_t> const &reports)
{
  std::cout << "  " << title << ':';
  if (reports.empty ())
    std::cout << " none";
  for (std::size_t i = 0; i < reports.size (); ++i)
    std::cout << " [id " << static_cast<unsigned> (reports[i].report_id)
              << ", " << reports[i].bytes << " B]";
  std::cout << '\n';
}

int
describe_device (tr::stream::io_runner &runner, std::string const &argument)
{
  hid::hid_selector_t const selector = parse_selector (argument);
  std::cout << "opening " << hid::describe (selector) << '\n';
  tr::session::message_session::factory_t const factory
      = hid::make_hid_factory (selector);
  hid::hid_device device = factory (runner.get_executor ());
  tr::status_result_t const opened = device.open (tr::deadline_t (3000));
  if (!opened.has_value ())
    {
      // invalid_endpoint: no device matches the selector.
      std::cout << "open failed: " << opened.error ().message () << '\n';
      return 1;
    }
  hid::report_layout_t const layout = hid::opened_report_layout (device);
  std::cout << "  reports are "
            << (layout.numbered ? "numbered" : "not numbered") << '\n';
  print_reports ("input", layout.input_reports);
  print_reports ("output", layout.output_reports);
  device.close ();
  return 0;
}
} // namespace

int
main (int argc, char **argv)
{
  // The owner of the threads of every HID device: they are joined before
  // main returns.
  hid::hid_runtime runtime;
  tr::stream::io_runner runner (1);
  if (argc > 1 && std::string (argv[1]) == "--count")
    {
      count_devices ();
      return 0;
    }
  if (argc > 1)
    return describe_device (runner, argv[1]);
  list_devices ();
  return 0;
}
#else
int
main ()
{
  std::cout << "built without the HID transport (LUMEX_TRANSPORT_WITH_HID): "
               "nothing to show\n";
  return 0;
}
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

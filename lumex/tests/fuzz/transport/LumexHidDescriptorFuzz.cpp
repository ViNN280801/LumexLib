// libFuzzer target of the HID report descriptor analysis
// (lumex/applied/transport/hid/LumexHidReportDescriptor): every input is a
// report descriptor. The analysis must never read past it (AddressSanitizer)
// and must either fail with transport_errc::bad_report_descriptor or return a
// layout that is consistent: report IDs strictly ascending, one report with
// ID 0 at most when the device numbers none, IDs 1..255 when it does, no
// report above 64 KiB, and the accessors (input_size, output_size,
// has_output, max_input_size) agreeing with the lists. The same input twice
// gives the same layout. Anything else stops the run. Built with
// LUMEX_BUILD_FUZZERS (Clang only); CTest runs it for a short bounded time
// under the label fuzz. LLVMFuzzerInitialize writes the seed corpus (the
// synthetic descriptors of the HID tests and malformed variants of them) into
// the corpus directory named on the command line.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidReportDescriptor.hpp"

namespace
{
namespace hid = lumex::applied::transport::hid;
namespace error = lumex::applied::transport::error;

using bytes = std::vector<std::uint8_t>;

void
require (bool condition)
{
  if (!condition)
    std::abort ();
}

// One vendor collection with one input and one output report, no report ID.
bytes
unnumbered_descriptor (std::uint8_t input, std::uint8_t output)
{
  std::uint8_t const d[]
      = { 0x06, 0x00, 0xFF, 0x09,   0x01, 0xA1, 0x01,  0x15, 0x00,
          0x26, 0xFF, 0x00, 0x75,   0x08, 0x95, input, 0x09, 0x02,
          0x81, 0x02, 0x95, output, 0x09, 0x03, 0x91,  0x02, 0xC0 };
  return bytes (d, d + sizeof d);
}

// Reports 1 (8 in, 8 out), 2 (16 in, 4 out) and 3 (2 in).
bytes
numbered_descriptor ()
{
  std::uint8_t const d[]
      = { 0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x75, 0x08, 0x85,
          0x01, 0x95, 0x08, 0x09, 0x02, 0x81, 0x02, 0x95, 0x08, 0x09,
          0x03, 0x91, 0x02, 0x85, 0x02, 0x95, 0x10, 0x09, 0x04, 0x81,
          0x02, 0x95, 0x04, 0x09, 0x05, 0x91, 0x02, 0x85, 0x03, 0x95,
          0x02, 0x09, 0x06, 0x81, 0x02, 0xC0 };
  return bytes (d, d + sizeof d);
}

std::vector<bytes>
seeds ()
{
  std::vector<bytes> result;
  result.push_back (unnumbered_descriptor (8, 8));
  result.push_back (unnumbered_descriptor (64, 0));
  result.push_back (unnumbered_descriptor (0, 255));
  result.push_back (numbered_descriptor ());
  {
    // Truncated in the middle of an item, and after the first item.
    bytes d = numbered_descriptor ();
    d.resize (d.size () - 3);
    result.push_back (d);
    result.push_back (bytes (1, 0x06));
  }
  {
    // Report ID 0 and above 255 (a four-byte value).
    std::uint8_t const zero[]
        = { 0x85, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02 };
    std::uint8_t const wide[]
        = { 0x87, 0x00, 0x01, 0x00, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02 };
    result.push_back (bytes (zero, zero + sizeof zero));
    result.push_back (bytes (wide, wide + sizeof wide));
  }
  {
    // Report ID 255, and a main item before the first Report ID.
    std::uint8_t const last[]
        = { 0x85, 0xFF, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02 };
    std::uint8_t const before[]
        = { 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x85, 0x01, 0x81, 0x02 };
    result.push_back (bytes (last, last + sizeof last));
    result.push_back (bytes (before, before + sizeof before));
  }
  {
    // Push and Pop around a Report ID, and a Pop without a Push.
    std::uint8_t const stacked[] = { 0x75, 0x08, 0x95, 0x02, 0xA4, 0x85,
                                     0x05, 0x81, 0x02, 0xB4, 0x81, 0x02 };
    std::uint8_t const pop[] = { 0xB4, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02 };
    result.push_back (bytes (stacked, stacked + sizeof stacked));
    result.push_back (bytes (pop, pop + sizeof pop));
  }
  {
    // A long item (skipped whole), one cut short, a Feature item (not a
    // report of the layout), and the largest accepted report (64 KiB).
    std::uint8_t const long_item[]
        = { 0xFE, 0x02, 0x10, 0xAA, 0xBB, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02 };
    std::uint8_t const cut[] = { 0xFE, 0x05, 0x10, 0xAA };
    std::uint8_t const feature[] = { 0x75, 0x08, 0x95, 0x04, 0xB1, 0x02 };
    std::uint8_t const largest[]
        = { 0x75, 0x08, 0x97, 0x00, 0x00, 0x01, 0x00, 0x81, 0x02 };
    std::uint8_t const too_large[]
        = { 0x75, 0x08, 0x97, 0x00, 0x00, 0x01, 0x00, 0x81, 0x02, 0x81, 0x02 };
    result.push_back (bytes (long_item, long_item + sizeof long_item));
    result.push_back (bytes (cut, cut + sizeof cut));
    result.push_back (bytes (feature, feature + sizeof feature));
    result.push_back (bytes (largest, largest + sizeof largest));
    result.push_back (bytes (too_large, too_large + sizeof too_large));
  }
  return result;
}

void
check_reports (std::vector<hid::report_size_t> const &reports, bool numbered)
{
  for (std::size_t i = 0; i < reports.size (); ++i)
    {
      require (reports[i].bytes <= 65536U);
      if (i > 0)
        require (reports[i - 1].report_id < reports[i].report_id);
      if (numbered)
        require (reports[i].report_id >= 1);
      else
        require (reports[i].report_id == 0);
    }
  if (!numbered)
    require (reports.size () <= 1U);
}

void
check_layout (hid::report_layout_t const &layout)
{
  check_reports (layout.input_reports, layout.numbered);
  check_reports (layout.output_reports, layout.numbered);
  std::size_t largest = 0;
  for (std::size_t i = 0; i < layout.input_reports.size (); ++i)
    {
      hid::report_size_t const &report = layout.input_reports[i];
      require (layout.input_size (report.report_id) == report.bytes);
      if (report.bytes > largest)
        largest = report.bytes;
    }
  require (layout.max_input_size () == largest);
  for (std::size_t i = 0; i < layout.output_reports.size (); ++i)
    {
      hid::report_size_t const &report = layout.output_reports[i];
      require (layout.output_size (report.report_id) == report.bytes);
      require (layout.has_output (report.report_id) == (report.bytes > 0));
    }
}

bool
same_reports (std::vector<hid::report_size_t> const &lhs,
              std::vector<hid::report_size_t> const &rhs)
{
  if (lhs.size () != rhs.size ())
    return false;
  for (std::size_t i = 0; i < lhs.size (); ++i)
    if (lhs[i].report_id != rhs[i].report_id || lhs[i].bytes != rhs[i].bytes)
      return false;
  return true;
}
} // namespace

extern "C" int
LLVMFuzzerInitialize (int *argc, char ***argv)
{
  for (int i = 1; i < *argc; ++i)
    {
      std::string const argument ((*argv)[i]);
      if (argument.empty () || argument[0] == '-')
        continue;
      std::vector<bytes> const all = seeds ();
      for (std::size_t n = 0; n < all.size (); ++n)
        {
          std::ofstream file (argument + "/seed_" + std::to_string (n)
                                  + ".bin",
                              std::ios::binary);
          if (!file)
            break;
          file.write (reinterpret_cast<char const *> (all[n].data ()),
                      static_cast<std::streamsize> (all[n].size ()));
        }
      break;
    }
  return 0;
}

extern "C" int
LLVMFuzzerTestOneInput (std::uint8_t const *data, std::size_t size)
{
  // An exact copy: a read past the descriptor is a read past this buffer.
  bytes const descriptor (data, data + size);
  lumex::applied::transport::const_bytes_t const view (descriptor.data (),
                                                       descriptor.size ());
  hid::report_layout_result_t const first
      = hid::analyze_report_descriptor (view);
  hid::report_layout_result_t const second
      = hid::analyze_report_descriptor (view);
  require (first.has_value () == second.has_value ());
  if (!first.has_value ())
    {
      require (first.error () == error::transport_errc::bad_report_descriptor);
      return 0;
    }
  check_layout (*first);
  require (first->numbered == second->numbered);
  require (same_reports (first->input_reports, second->input_reports));
  require (same_reports (first->output_reports, second->output_reports));
  return 0;
}

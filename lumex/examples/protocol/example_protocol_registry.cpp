// A tutorial on the parameter registries of lumex::protocol, on an invented
// device: a sensor probe whose settings are numbered parameters, read and
// written by two general commands. The registry holds the name, the number,
// the type, the access, the limits and the firmware range of each parameter;
// the commands name a parameter by an enumerator of the registry. Shown here:
// the table, a write that is checked against the limits and the access of the
// parameter it names, the frames in the words of the table (describe), and
// verify_registry, which compares the registry with what a device says of its
// own parameters, with and without the firmware of the device. Nothing here
// talks to a device.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ratio>
#include <string>
#include <vector>

#include "example_protocol_check.hpp"
#include "lumex/core/protocol/LumexProtocol"

namespace probe_hub
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

struct codec
{
};

/// The parameters of the table are words of the registry, not of the rows.
struct header
{
  using lumex_vocabulary = lumex::core::protocol::registry::table_vocabulary;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The registry: the name, the number, a description, the type, the access and
// the options of each parameter. DELETE is a platform macro: P_AS keeps it as
// the name in the specification and the identifier free.
// clang-format off
#define PROBE_PARAMS(P, P_AS) \
  P (GAIN, (0x10), "Amplifier gain", \
     (i16_le_<scale<std::ratio<1, 10> > >), RW, (limits (-10, 10))) \
  P (AVERAGING, (0x11), "Readings averaged", (u8_<unit::count>), RW, \
     (limits (1, 64))) \
  P (TEMPERATURE, (0x12), "Probe temperature", \
     (i16_le_<scale<std::ratio<1, 100> >, unit::degree_celsius>), R, ()) \
  P (SERIAL, (0x13), "Serial number", (u32_le_), R, ()) \
  P (FILTER, (0x14), "Digital filter", (u8_), RW, (firmware (3, 9))) \
  P_AS (CLEAR, "DELETE", (0x15), "Clear the parameters", (u8_), W, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (probe, PROBE_PARAMS);

/// What GET_PARAMETER answers: the parameter and its value. The value is a
/// field of the registry: its type is the one the entry named by the key has.
LUMEX_PROTOCOL_LAYOUT (
    parameter_t,
    (registry::registry_key_<probe, u8_>,
     registry::registry_value_<probe, 0, i32_le_, registry::access_t::read>),
    ("parameter", "value"));

// clang-format off
#define PROBE_ROWS(X, X_AS) \
  X (SET_PARAMETER, 0x20, "Write a parameter", \
     (registry_key_<probe, u8_>, registry_value_<probe, 0, i32_le_>), \
     ("parameter", "value"), (), ()) \
  X (GET_PARAMETER, 0x21, "Read a parameter", \
     (registry_key_<probe, u8_>), ("parameter"), (parameter_t), (idempotent))
// clang-format on
LUMEX_PROTOCOL_DEFINE (probe_hub_v1, codec, header, PROBE_ROWS);
} // namespace probe_hub

namespace
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace hub = probe_hub;

std::string
hex (layout::bytes_t const &bytes)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < bytes.size (); ++i)
    {
      out += digits[bytes[i] >> 4];
      out += digits[bytes[i] & 0x0F];
      out += ' ';
    }
  return out;
}

layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

// What a device tells of its own parameters, entry by entry: verify_registry
// asks for position 0, 1, 2 ... until this says there are no more.
struct device_t
{
  std::vector<registry::device_entry_t> entries;

  bool
  operator() (std::size_t position, registry::device_entry_t &out) const
  {
    if (position >= entries.size ())
      return false;
    out = entries[position];
    return true;
  }
};

// The device that agrees with the table in everything.
device_t
device_like_the_table ()
{
  device_t device;
  registry::entries_t const table = hub::probe::entries ();
  for (std::size_t i = 0; i < table.size (); ++i)
    {
      registry::registry_entry_t const &entry = table.data ()[i];
      registry::device_entry_t told = registry::device_entry_t ();
      told.key = entry.key;
      told.name = entry.specification_name;
      told.size = static_cast<std::uint32_t> (entry.type.binary_size);
      told.has_signedness = true;
      told.is_signed = entry.type.is_signed;
      told.has_scale = true;
      told.scale_num = entry.type.scale_num;
      told.scale_den = entry.type.scale_den;
      told.has_limits = entry.has_limits;
      told.lower = entry.lower;
      told.upper = entry.upper;
      told.has_access = true;
      told.access = entry.access;
      device.entries.push_back (told);
    }
  return device;
}

using lumex::core::protocol::error::protocol_errc;

// How many differences of one kind a report holds.
std::size_t
count_of (registry::verify_report_t const &report,
          registry::mismatch_kind kind)
{
  std::size_t count = 0;
  for (std::size_t i = 0; i < report.mismatches.size (); ++i)
    if (report.mismatches[i].kind == kind)
      ++count;
  return count;
}

void
print_report (registry::verify_report_t const &report)
{
  std::cout << "  " << report.compared << " of " << report.device_entries
            << " parameters compared, "
            << (report.ok () ? "no difference" : "differences:") << '\n';
  for (std::size_t i = 0; i < report.mismatches.size (); ++i)
    std::cout << "    " << registry::to_string (report.mismatches[i]) << '\n';
  for (std::size_t i = 0; i < report.skipped.size (); ++i)
    std::cout << "    left out (not in this firmware): " << report.skipped[i]
              << '\n';
}
} // namespace

int
main ()
{
  using hub::probe_hub_v1;
  std::cout << "=== lumex::protocol: parameter registries ===\n\n";

  std::cout << "--- 1. The registry is a table of the program\n";
  registry::entries_t const table = hub::probe::entries ();
  check::expect (table.size () == 6, "the registry has six parameters");
  check::expect (table.data ()[0].has_limits && table.data ()[0].lower == -10
                     && table.data ()[0].upper == 10,
                 "GAIN has the limits -10..10");
  check::expect (table.data ()[4].has_firmware
                     && table.data ()[4].firmware_from == 3
                     && table.data ()[4].firmware_to == 9,
                 "FILTER exists from firmware 3 to 9");
  check::expect (std::string (table.data ()[5].specification_name) == "DELETE",
                 "the platform macro DELETE is the name in the specification");
  for (std::size_t i = 0; i < table.size (); ++i)
    {
      registry::registry_entry_t const &entry = table.data ()[i];
      std::cout << "  " << entry.specification_name << " ("
                << entry.description << "), "
                << registry::key_to_string (entry.key) << ", "
                << registry::access_to_string (entry.access);
      if (entry.has_limits)
        std::cout << ", " << entry.lower << ".." << entry.upper;
      if (entry.has_firmware)
        std::cout << ", firmware " << entry.firmware_from << ".."
                  << entry.firmware_to;
      std::cout << '\n';
    }

  std::cout << "\n--- 2. A write names its parameter by an enumerator\n";
  index::index_result_t const &built
      = index::protocol_index<probe_hub_v1, hub::probe> ();
  if (!built)
    {
      std::cout << "no index: " << built.error ().message << '\n';
      return 1;
    }
  layout::bytes_t set_gain;
  layout::encode_message<probe_hub_v1, probe_hub_v1::SET_PARAMETER_t> (
      set_gain, hub::probe::GAIN, 2.5);
  std::cout << "  SET_PARAMETER (GAIN, 2.5): " << hex (set_gain) << '\n'
            << "  "
            << built->describe (view_of (set_gain), index::frame_direction::tx)
            << '\n';
  check::expect (
      built->describe (view_of (set_gain), index::frame_direction::tx)
          == "TX probe_hub_v1.SET_PARAMETER (Write a parameter) "
             "parameter=GAIN value=2.5",
      "describe () tells the write in the words of the table");
  check::expect (hex (set_gain) == "20 10 19 00 00 00 ",
                 "SET_PARAMETER (GAIN, 2.5): key 0x20, parameter 0x10, 25 in "
                 "a 32-bit word");

  std::cout << "\n--- 3. The encoder checks what the entry says\n";
  layout::bytes_t out;
  layout::encode_result_t const beyond
      = layout::encode_message<probe_hub_v1, probe_hub_v1::SET_PARAMETER_t> (
          out, hub::probe::GAIN, 99.0);
  std::cout << "  GAIN = 99 (limits -10..10): " << beyond.error ().message ()
            << '\n';
  layout::encode_result_t const read_only
      = layout::encode_message<probe_hub_v1, probe_hub_v1::SET_PARAMETER_t> (
          out, hub::probe::SERIAL, 5);
  std::cout << "  SERIAL = 5 (a read-only parameter): "
            << read_only.error ().message () << '\n';
  std::cout << "  nothing was written: " << (out.empty () ? "yes" : "NO")
            << '\n';
  check::expect (!beyond && beyond.error () == protocol_errc::out_of_limits,
                 "GAIN = 99 is protocol_errc::out_of_limits");
  check::expect (!read_only
                     && read_only.error () == protocol_errc::access_denied,
                 "SERIAL = 5 is protocol_errc::access_denied");
  check::expect (out.empty (), "a refused write leaves the buffer empty");

  std::cout
      << "\n--- 4. A reply carries the value of the parameter it names\n";
  // The device says TEMPERATURE is 21.34 degrees: 2134 hundredths, as the
  // 32-bit word of the general command.
  layout::bytes_t const reply = { 0x21, 0x12, 0x56, 0x08, 0x00, 0x00 };
  std::cout << "  "
            << built->describe (view_of (reply), index::frame_direction::rx)
            << '\n';
  check::expect (built->describe (view_of (reply), index::frame_direction::rx)
                     == "RX probe_hub_v1.GET_PARAMETER (Read a parameter) "
                        "parameter=TEMPERATURE value=21.34 degC",
                 "describe () reads the value with the scale of TEMPERATURE");

  std::cout << "\n--- 5. verify_registry: the table against the device\n";
  std::cout << " a device that agrees:\n";
  device_t device = device_like_the_table ();
  registry::verify_report_t const agrees
      = registry::verify_registry<hub::probe> (device);
  print_report (agrees);
  check::expect (agrees.ok () && agrees.compared == 6
                     && agrees.device_entries == 6 && agrees.skipped.empty (),
                 "a device like the table: six compared, no difference");
  // A device whose firmware differs: a wider gain, no digital filter, and a
  // parameter the table does not know.
  std::cout << " a device that differs:\n";
  device.entries[0].upper = 20;
  device.entries[1].name = "AVG";
  device.entries.erase (device.entries.begin () + 4);
  registry::device_entry_t extra = device.entries[0];
  extra.key = registry::make_index_key (0x7F);
  extra.name = "SPARE";
  device.entries.push_back (extra);
  registry::verify_report_t const differs
      = registry::verify_registry<hub::probe> (device);
  print_report (differs);
  check::expect (
      !differs.ok () && differs.mismatches.size () == 4
          && count_of (differs, registry::mismatch_kind::limits) == 1
          && count_of (differs, registry::mismatch_kind::name) == 1
          && count_of (differs, registry::mismatch_kind::missing_in_device)
                 == 1
          && count_of (differs, registry::mismatch_kind::missing_in_table)
                 == 1,
      "four differences: limits, name, missing in the device, "
      "missing in the table");

  std::cout << "\n--- 6. With the firmware of the device\n";
  // FILTER exists from firmware 3: an old device is not expected to report it.
  lumex::core::protocol::index::session_context_t session;
  session.set_firmware (2);
  device_t old_device = device_like_the_table ();
  old_device.entries.erase (old_device.entries.begin () + 4);
  registry::verify_report_t const old_report
      = registry::verify_registry<hub::probe> (old_device, 65536, &session);
  print_report (old_report);
  check::expect (old_report.ok () && old_report.skipped.size () == 1
                     && old_report.skipped[0] == "FILTER",
                 "firmware 2 is not expected to report FILTER: left out, no "
                 "difference");
  return check::exit_code ();
}

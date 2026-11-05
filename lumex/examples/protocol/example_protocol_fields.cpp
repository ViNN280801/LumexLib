// A tutorial on the fields and the layouts of lumex::protocol, on an invented
// device: a heated block with a target temperature, a status word, a name
// and a short history. Nothing here talks to a device; the messages are made
// and read in the program. The steps: a request with engineering values and
// what goes on the wire, a reply with a word split into parts, a text and a
// counted list, values that do not fit, the same rows in a second layer with
// another layout (hexadecimal digits), a received message sent to its row by
// dispatch_message, and the smallest valid value of a row (sample_value).
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ratio>
#include <sstream>
#include <string>
#include <system_error>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace heater
{
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

/// What the block is doing, with names for the log.
LUMEX_DEFINE_REFLECTED_ENUM (heat_mode, std::uint8_t, (off, 0), (warming, 1),
                             (holding, 2), (cooling, 3))

/// The table only names the codec that frames the messages.
struct codec
{
};

/// The binary form: the key is the first byte of the message.
struct binary_header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The text form: every byte is two hexadecimal digits, the key the first two.
struct text_header
{
  using layout_type = layout::fixed_width_text_layout<16>;
  using key_reader = index::key_at_digits<0, 2, 16>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The status byte: one flag bit, the mode in three bits and a counter in
/// four. The parts of one word are a layout of their own.
LUMEX_PROTOCOL_LAYOUT (status_parts_t,
                       (bits_<u8_, 7, 1, bool>, bits_<u8_, 4, 3, heat_mode>,
                        bits_<u8_, 0, 4, std::uint8_t>),
                       ("lid_open", "mode", "errors"));

/// A reading: temperature in tenths of a degree (signed, big-endian), the
/// status byte as a split word, and the power in percent.
LUMEX_PROTOCOL_LAYOUT (
    reading_t,
    (i16_be_<scale<std::ratio<1, 10>>, unit::degree_celsius>,
     split_<u8_, status_parts_t>, u8_<unit::percent>),
    ("temperature", "status", "power"));

/// The last temperatures: a count, then that many values, at most six.
LUMEX_PROTOCOL_LAYOUT (
    history_t,
    (counted_<i16_be_<scale<std::ratio<1, 10>>, unit::degree_celsius>, u8_,
              6>),
    ("temperatures"));

// clang-format off
#define HEATER_ROWS(X, X_AS) \
  X (SET_TARGET, 0x10, "Set the target temperature", \
     (i16_be_<scale<std::ratio<1, 10> >, unit::degree_celsius>, u8_), \
     ("target", "ramp"), (), (timeout (500), retries (1))) \
  X (READ, 0x11, "Read the block", (), (), (reading_t), (idempotent)) \
  X (NAME, 0x12, "Name of the block", (), (), (text_<8>), (idempotent)) \
  X (HISTORY, 0x13, "Last temperatures", (), (), (history_t), (idempotent))
#define NO_ROWS(X, X_AS)
// clang-format on

/// The binary layer.
LUMEX_PROTOCOL_DEFINE (heater_binary, codec, binary_header, HEATER_ROWS);

/// The text layer: the same rows (the same types), another layout.
LUMEX_PROTOCOL_EXTEND_FRAMED (heater_text, heater_binary, codec, text_header,
                              NO_ROWS);
} // namespace heater

namespace
{
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
using lumex::core::protocol::error::protocol_errc;

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

// A number with one decimal, as a person reads a temperature.
std::string
number (double value)
{
  std::ostringstream text;
  text.setf (std::ios::fixed);
  text.precision (1);
  text << value;
  return text.str ();
}

// The reading, in words.
std::string
show_reading (
    layout::reply_value_of<heater::heater_binary::READ_t>::type const &reading)
{
  auto const &status = field::get<1> (reading);
  return "temperature " + number (field::get<0> (reading).value ())
         + " degC (raw " + std::to_string (field::get<0> (reading).raw)
         + "), mode " + to_string (field::get<1> (status.parts)) + ", lid "
         + (field::get<0> (status.parts) ? "open" : "closed") + ", errors "
         + std::to_string (int (field::get<2> (status.parts))) + ", power "
         + std::to_string (int (field::get<2> (reading))) + " %";
}

// What dispatch_message calls: one function per direction, a template over
// the row, so every row of the table is handled by the same code.
struct printer_t
{
  template <typename Row>
  void
  request (typename layout::request_of<Row>::type const &)
  {
    std::cout << "  a request of " << Row::identifier () << " ("
              << Row::specification_name () << ")\n";
  }

  template <typename Row, typename Value>
  void
  reply (Value const &)
  {
    std::cout << "  a reply of " << Row::identifier () << " ("
              << Row::specification_name () << ")\n";
  }
};
} // namespace

int
main ()
{
  using heater::heater_binary;
  using heater::heater_text;
  std::cout << "=== lumex::protocol: fields and layouts ===\n\n";

  std::cout << "--- 1. A request: one argument per field, in engineering "
               "units\n";
  layout::bytes_t fields;
  layout::encode_request<heater_binary, heater_binary::SET_TARGET_t> (
      fields, 37.5, 12);
  std::cout << "  SET_TARGET (37.5, 12): fields " << hex (fields)
            << " (375 tenths of a degree in two bytes, big-endian)\n";
  layout::bytes_t message;
  layout::encode_message<heater_binary, heater_binary::SET_TARGET_t> (
      message, 37.5, 12);
  std::cout << "  the message with its key (0x10 comes first): "
            << hex (message) << '\n';

  std::cout << "\n--- 2. A reply: a record of values\n";
  // The device answers READ with 24.5 degrees, the lid open, mode holding, one
  // error, 60 percent power: 0x00F5, 0xA1 = 1010 0001, 0x3C.
  layout::bytes_t const reading_fields = { 0x00, 0xF5, 0xA1, 0x3C };
  auto const reading
      = layout::decode_reply<heater_binary, heater_binary::READ_t> (
          view_of (reading_fields));
  if (!reading)
    {
      std::cout << "  cannot decode: " << reading.error ().message () << '\n';
      return 1;
    }
  std::cout << "  " << show_reading (*reading) << '\n';
  // A short message is an error, never a half-read value.
  layout::bytes_t const cut = { 0x00, 0xF5 };
  auto const truncated
      = layout::decode_reply<heater_binary, heater_binary::READ_t> (
          view_of (cut));
  std::cout << "  two bytes of the four: " << truncated.error ().message ()
            << '\n';

  std::cout << "\n--- 3. A text and a counted list\n";
  layout::bytes_t const name_fields = { 'B', 'l', 'o', 'c', 'k', '7', 0, 0 };
  auto const name
      = layout::decode_reply<heater_binary, heater_binary::NAME_t> (
          view_of (name_fields));
  std::cout << "  NAME: '" << std::string (name->chars, name->length)
            << "' (the NUL padding is not part of the text)\n";
  layout::bytes_t const history_fields
      = { 3, 0x00, 0xE6, 0x00, 0xEB, 0x00, 0xF0 };
  auto const history
      = layout::decode_reply<heater_binary, heater_binary::HISTORY_t> (
          view_of (history_fields));
  std::cout << "  HISTORY: " << field::get<0> (*history).count
            << " values, the last one "
            << number (field::get<0> (*history).items[2].value ())
            << " degC\n";

  std::cout << "\n--- 4. A value that does not fit is refused, not cut\n";
  layout::bytes_t refused;
  layout::encode_result_t const too_hot
      = layout::encode_request<heater_binary, heater_binary::SET_TARGET_t> (
          refused, 4000.0, 12);
  std::cout << "  SET_TARGET (4000.0, 12): " << too_hot.error ().message ()
            << " (error code " << too_hot.error ().value ()
            << " of lumex.protocol), "
            << (refused.empty () ? "nothing written" : "SOMETHING WRITTEN")
            << '\n';

  std::cout << "\n--- 5. The same rows in the text layer\n";
  layout::bytes_t text;
  layout::encode_message<heater_text, heater_text::SET_TARGET_t> (text, 37.5,
                                                                  12);
  std::cout << "  SET_TARGET (37.5, 12): '"
            << std::string (text.begin (), text.end ())
            << "' (key 10, then 0177, then 0C: digits of the bytes)\n";

  std::cout << "\n--- 6. A received message goes to its row\n";
  layout::bytes_t const received = { 0x11, 0x00, 0xF5, 0xA1, 0x3C };
  printer_t printer;
  std::error_code error = layout::dispatch_message<heater_binary> (
      view_of (received), index::frame_direction::rx, printer);
  std::cout << "  (rx) " << (error ? error.message () : "dispatched") << '\n';
  layout::bytes_t const stranger = { 0x7E, 0x00 };
  error = layout::dispatch_message<heater_binary> (
      view_of (stranger), index::frame_direction::rx, printer);
  std::cout << "  key 0x7E: " << error.message () << '\n';

  std::cout << "\n--- 7. The smallest valid value of a row\n";
  auto const sample
      = field::sample_value<heater_binary::READ_t::reply_type> ();
  layout::bytes_t sample_message;
  layout::encode_message<heater_binary, heater_binary::READ_t> (
      sample_message);
  std::cout << "  READ asks with: " << hex (sample_message)
            << "\n  the sample of its reply: " << show_reading (sample)
            << "\n";
  return error == protocol_errc::unknown_frame ? 0 : 1;
}

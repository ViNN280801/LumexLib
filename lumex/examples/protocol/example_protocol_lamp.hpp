// An invented device for the examples of the dialogue layer of
// lumex::transport: a stage lamp (a theatre
// spotlight controller). It stands for no real device;
// the names, the codes and the values are made up. The table shows what the
// client, the simulator and the discovery need from a table:
//  - one set of rows (lamp_text) and a second layer of the same rows with
//    another wire form (lamp_binary): hexadecimal digits in a line with a
//    check value, or bytes behind a length byte;
//  - a row that tells what the device is (WHO), a reply that tells it
//    refused (ack_nak), a row that only the device sends (LEVEL), a row whose
//    name is a platform macro (X_AS) and that needs a confirmation;
//  - a state of the device that decides which rows it accepts.
// The framing is not in the table: wire_text_t and wire_binary_t below say
// which codec of lumex::framing a layer is spoken with.
#ifndef LUMEX_EXAMPLES_PROTOCOL_LAMP_HPP
#define LUMEX_EXAMPLES_PROTOCOL_LAMP_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>
#include <sstream>
#include <string>
#include <system_error>

#include "lumex/applied/transport/LumexTransport"

// The dialogue layer of lumex::transport (the macro comes from the target):
// without it the protocol and framing headers below may not be installed.
#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lamp
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

/// What the lamp is doing, with names for the log.
LUMEX_DEFINE_REFLECTED_ENUM (run_state, std::uint8_t, (off, 0), (warming, 1),
                             (on, 2), (fault, 3))

/// The same states for `accepted_in`, which names a plain enumeration.
enum class state : int
{
  off = 0,
  warming = 1,
  on = 2,
  fault = 3
};

/// The table only names the codec; the program picks one (see the wires).
struct codec
{
};

/// The text form: every byte is two hexadecimal digits, the key is the first
/// two. The line around the digits is the codec's.
struct text_header
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  using layout_type = layout::fixed_width_text_layout<16>;
  using key_reader = index::key_at_digits<0, 2, 16>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  // What the table says of the dialogue: the row that tells the device, the
  // state the device reports in STATUS and a timeout for the rows without one.
  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::state_from<typename P::STATUS_t, 0>,
                          dialogue::default_timeout_ms<150>>;
};

/// The binary form: the key is the first byte of the message.
struct binary_header
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  using key_reader = index::key_at<0, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::state_from<typename P::STATUS_t, 0>,
                          dialogue::default_timeout_ms<150>>;
};

/// What WHO answers: the model number, the firmware and the serial number.
LUMEX_PROTOCOL_LAYOUT (who_t, (u8_, u16_be_, u16_be_),
                       ("model", "firmware", "serial"));

/// What the lamp is doing: the state, the brightness and the hours lit.
LUMEX_PROTOCOL_LAYOUT (status_t,
                       (enum_<u8_, run_state>,
                        u16_be_<scale<std::ratio<1, 10>>, unit::percent>,
                        u32_be_),
                       ("state", "brightness", "hours"));

/// The level the device reports by itself.
LUMEX_PROTOCOL_LAYOUT (level_t,
                       (u16_be_<scale<std::ratio<1, 10>>, unit::percent>, u8_),
                       ("brightness", "temperature"));

/// The parts of the reply of START: the channel that was started, or why it
/// was not (a code and a detail). They differ in shape, so a client tells
/// them apart.
LUMEX_PROTOCOL_LAYOUT (started_t, (u8_), ("channel"));
LUMEX_PROTOCOL_LAYOUT (refusal_t, (u8_, u16_be_), ("code", "detail"));

// clang-format off
#define LAMP_ROWS(X, X_AS) \
  X (WHO, 0x01, "Identify", (), (), (who_t), (idempotent)) \
  X (STATUS, 0x10, "Read the status", (), (), (status_t), \
     (idempotent, retries (2))) \
  X (SET_LEVEL, 0x11, "Set the brightness", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::percent>), \
     ("brightness"), (status_t), (accepted_in (state::off, state::warming))) \
  X (START, 0x12, "Start a channel", (u8_), ("channel"), \
     (ack_nak<started_t, refusal_t>), (accepted_in (state::off))) \
  X (STOP, 0x13, "Stop", (), (), (status_t), ()) \
  X (LEVEL, 0x20, "Level report", (), (), (level_t), (stream (100))) \
  X_AS (ERASE_LOG, "DELETE", 0x7F, "Erase the log of a slot", (u8_), ("slot"), \
        (status_t), (dangerous))
#define NO_ROWS(X, X_AS)
// clang-format on

/// The text layer.
LUMEX_PROTOCOL_DEFINE (lamp_text, codec, text_header, LAMP_ROWS);

/// The binary layer: the same rows, another wire form.
LUMEX_PROTOCOL_EXTEND_FRAMED (lamp_binary, lamp_text, codec, binary_header,
                              NO_ROWS);

/// A number with one decimal, as a person reads a brightness.
inline std::string
number (double value)
{
  std::ostringstream text;
  text.setf (std::ios::fixed);
  text.precision (1);
  text << value;
  return text.str ();
}

/// A reply of STATUS in words.
template <typename P>
std::string
show_status (
    lumex::applied::transport::protocol::reply_of<typename P::STATUS_t> const
        &status)
{
  return std::string ("state ") + to_string (field::get<0> (status))
         + ", brightness " + number (field::get<1> (status).value ()) + " %, "
         + std::to_string (field::get<2> (status)) + " h lit";
}

/// The reply of WHO that makes the simulator a lamp of this model, firmware
/// and serial number.
template <typename P>
lumex::applied::transport::protocol::reply_of<typename P::WHO_t>
who_reply (std::uint8_t model, std::uint16_t firmware, std::uint16_t serial)
{
  lumex::applied::transport::protocol::reply_of<typename P::WHO_t> who;
  field::get<0> (who) = model;
  field::get<1> (who) = firmware;
  field::get<2> (who) = serial;
  return who;
}

/// The text form on the wire: '#', the digits, the XOR of the characters as
/// two digits, CR.
using text_codec_t = lumex::core::framing::stage::pipeline<
    lumex::core::framing::delimiter::delimiter_codec,
    lumex::core::framing::stage::prefix_char_stage<'#'>,
    lumex::core::framing::stage::checksum_stage<
        lumex::core::framing::stage::xor8,
        lumex::core::framing::stage::ascii_hex_render>>;

/// The binary form on the wire: one length byte, then the message.
using binary_codec_t = lumex::core::framing::length::length_prefix_codec;

inline binary_codec_t
make_binary_codec ()
{
  lumex::core::framing::length::length_prefix_options_t options;
  options.width = 1;
  options.max_frame_size = 64;
  return binary_codec_t (options);
}

/// The wire of a layer: the codec its frames use. A table does not name it;
/// the program that talks to the device does.
template <typename P> struct wire_t;

template <> struct wire_t<lamp_text>
{
  using codec_type = text_codec_t;

  static codec_type
  make_codec ()
  {
    return codec_type ();
  }
};

template <> struct wire_t<lamp_binary>
{
  using codec_type = binary_codec_t;

  static codec_type
  make_codec ()
  {
    return make_binary_codec ();
  }
};
} // namespace lamp

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
/// How the reply of WHO reads as the identity of the device. It is told for
/// the text layer; the binary layer takes what its base says.
template <> struct identity_of_t<lamp::lamp_text>
{
  static std::error_code
  read (reply_of<lamp::lamp_text::WHO_t> const &reply, identity_t &identity)
  {
    namespace fl = lumex::core::protocol::field;
    // Model 0 is no lamp of ours (the sample reply of a simulator that was
    // not told who it is): a discovery takes it for another device.
    if (fl::get<0> (reply) == 0)
      return lumex::core::protocol::error::make_error_code (
          lumex::core::protocol::error::protocol_errc::invalid_value);
    identity.model = "StageLamp-" + std::to_string (int (fl::get<0> (reply)));
    identity.firmware = fl::get<1> (reply);
    identity.serial_number = std::to_string (int (fl::get<2> (reply)));
    return std::error_code ();
  }
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex
#endif // LUMEX_TRANSPORT_WITH_PROTOCOL

#endif // !LUMEX_EXAMPLES_PROTOCOL_LAMP_HPP

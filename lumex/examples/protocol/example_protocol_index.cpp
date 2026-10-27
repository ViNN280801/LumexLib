// The run-time index of a protocol table and the log lines it writes: a
// synthetic protocol (an invented beacon) is described once, the index is
// built from it, and describe turns frames into lines of the kind a log
// holds. Nothing here talks to a device; the frames are made in the program.
#include <cstdint>
#include <iostream>
#include <string>

#include "example_protocol_beacon.hpp"

namespace
{
// Inside a namespace of our own: at the global scope the alias index would
// clash with the POSIX function index ().
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

/// The frame of a request: the key byte, then the fields.
template <typename Row, typename... Args>
layout::bytes_t
request_frame (Args const &...args)
{
  layout::bytes_t frame;
  frame.push_back (static_cast<std::uint8_t> (Row::key ()));
  layout::encode_request<beacon::beacon_v1, Row> (frame, args...);
  return frame;
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::protocol: the index and describe ===\n\n";

  std::cout << "--- 1. The index is built from the table ---\n";
  index::index_result_t const &built
      = index::protocol_index<beacon::beacon_v1, beacon::lamp> ();
  if (!built)
    {
      std::cout << "no index: " << built.error ().message << '\n';
      return 1;
    }
  index::protocol_index_t const &protocol = *built;
  std::cout << "protocol " << protocol.protocol_name () << ", "
            << protocol.rows ().size () << " rows, the key is "
            << protocol.key_width () << " byte(s)\n";
  for (std::size_t i = 0; i < protocol.rows ().size (); ++i)
    {
      index::row_t const &row = protocol.rows ()[i];
      std::cout << "  0x" << std::hex << row.key.integer << std::dec << ' '
                << row.entry->specification_name << " ("
                << row.entry->description << ")"
                << (row.is_event ? ", sent by the device" : "") << '\n';
    }

  std::cout << "\n--- 2. Requests made by the program ---\n";
  layout::bytes_t const ping = request_frame<beacon::beacon_v1::PING_t> ();
  layout::bytes_t const level
      = request_frame<beacon::beacon_v1::SET_LEVEL_t> (50.5);
  std::cout << protocol.describe (view_of (ping), index::frame_direction::tx)
            << '\n'
            << protocol.describe (view_of (level), index::frame_direction::tx)
            << '\n';

  std::cout << "\n--- 3. Frames received from a device ---\n";
  layout::bytes_t const state = { 0x03, 0x02, 0x4B, 0xE2, 0x00 };
  layout::bytes_t const tick = { 0x04, 0x01, 0x14, 0x0F, 0x01 };
  std::cout << protocol.describe (view_of (state), index::frame_direction::rx)
            << '\n'
            << protocol.describe (view_of (tick), index::frame_direction::rx)
            << '\n';

  std::cout << "\n--- 4. Frames the table does not explain ---\n";
  layout::bytes_t const unknown = { 0x7E, 0x01, 0x02 };
  layout::bytes_t const damaged = { 0x03, 0x02, 0x4B };
  std::cout << protocol.describe (view_of (unknown),
                                  index::frame_direction::rx)
            << '\n'
            << protocol.describe (view_of (damaged),
                                  index::frame_direction::rx)
            << '\n';
  return 0;
}

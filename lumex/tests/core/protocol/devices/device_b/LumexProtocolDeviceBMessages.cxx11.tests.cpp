// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device B (lumex/tests/core/protocol/devices/device_b): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row. The aligned variant of the family table
// is only checked for its alignment (one of its rows is not aligned), so it
// has no messages.

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::device_b;
} // namespace

TEST (
    LumexProtocolDeviceBMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::device_b2> (),
             dsl::command_count<tables::device_b2>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_b2_m2> (),
             dsl::command_count<tables::device_b2_m2>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::b2_system> (),
             dsl::command_count<tables::b2_system>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_b1> (),
             dsl::command_count<tables::device_b1>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_b1_rx> (),
             dsl::command_count<tables::device_b1_rx>::value);
}

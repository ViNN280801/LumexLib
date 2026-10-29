// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device A (lumex/tests/core/protocol/devices/device_a): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row.

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::device_a;
} // namespace

TEST (
    LumexProtocolDeviceAMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::family_a> (),
             dsl::command_count<tables::family_a>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::a_stream> (),
             dsl::command_count<tables::a_stream>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::a_old> (),
             dsl::command_count<tables::a_old>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::a_system> (),
             dsl::command_count<tables::a_system>::value);
}

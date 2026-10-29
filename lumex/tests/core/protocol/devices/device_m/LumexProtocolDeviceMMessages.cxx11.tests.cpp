// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device M (lumex/tests/core/protocol/devices/device_m): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row.

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::device_m;
} // namespace

TEST (
    LumexProtocolDeviceMMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::device_m1> (),
             dsl::command_count<tables::device_m1>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_m1_m2> (),
             dsl::command_count<tables::device_m1_m2>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_m1_m3> (),
             dsl::command_count<tables::device_m1_m3>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_m2> (),
             dsl::command_count<tables::device_m2>::value);
}

// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device K (lumex/tests/core/protocol/devices/device_k): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row.

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_k/LumexProtocolDeviceKTable.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::tables_k;
} // namespace

TEST (
    LumexProtocolDeviceKMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::device_k> (),
             dsl::command_count<tables::device_k>::value);
}

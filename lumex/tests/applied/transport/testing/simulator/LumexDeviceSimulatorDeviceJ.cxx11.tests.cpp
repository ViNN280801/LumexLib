// The device simulator over the layers of device J: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::tables_j;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceJ)
{
  st::check_layer<dev::j_text> ();
  st::check_datagram_layer<dev::j_hid> ();
}

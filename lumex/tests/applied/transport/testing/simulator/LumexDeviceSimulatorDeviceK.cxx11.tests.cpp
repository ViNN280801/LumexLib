// The device simulator over the layers of device K: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_k/LumexProtocolDeviceKTable.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::tables_k;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceK)
{
  st::check_layer<dev::device_k> ();
}

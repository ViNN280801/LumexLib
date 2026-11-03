// The device simulator over the layers of device E: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_e/LumexProtocolDeviceETable.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::tables_e;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceE)
{
  st::check_layer<dev::family_e> ();
  st::check_layer<dev::device_e> ();
}

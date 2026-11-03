// The device simulator over the layers of device I: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_i/LumexProtocolDeviceITable.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::tables_i;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceI)
{
  st::check_layer<dev::device_i> ();
  st::check_layer<dev::alt_i> ();
}

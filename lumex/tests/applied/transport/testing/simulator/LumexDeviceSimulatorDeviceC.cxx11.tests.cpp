// The device simulator over the layers of device C: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_c/LumexProtocolDeviceC.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_c;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceC)
{
  st::check_layer<dev::c_i2c> ();
  st::check_layer<dev::device_c> ();
  st::check_layer<dev::device_c_rx> ();
}

// The device simulator over the layers of device M: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_m;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceM)
{
  st::check_layer<dev::device_m1> ();
  st::check_layer<dev::device_m1_m2> ();
  st::check_layer<dev::device_m1_m3> ();
  st::check_layer<dev::device_m2> ();
}

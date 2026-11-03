// The device simulator over the layers of device B: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_b;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceB)
{
  st::check_layer<dev::device_b2> ();
  st::check_layer<dev::device_b2_m2> ();
  st::check_layer<dev::b2_system> ();
  st::check_layer<dev::device_b1> ();
  st::check_layer<dev::device_b1_rx> ();
}

// The device simulator over the layers of device A: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_a;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceA)
{
  st::check_layer<dev::family_a> ();
  st::check_layer<dev::a_stream> ();
  st::check_layer<dev::a_old> ();
  st::check_layer<dev::a_system> ();
}

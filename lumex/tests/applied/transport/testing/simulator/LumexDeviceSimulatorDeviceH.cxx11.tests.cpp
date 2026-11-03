// The device simulator over the layers of device H: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_h/LumexProtocolDeviceH.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_h;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceH)
{
  st::check_layer<dev::h_can> ();
  st::check_layer<dev::device_h> ();
}

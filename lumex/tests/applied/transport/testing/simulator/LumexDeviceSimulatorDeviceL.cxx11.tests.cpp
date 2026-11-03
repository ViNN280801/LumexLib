// The device simulator over the layers of device L: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_l/LumexProtocolDeviceL.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::device_l;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceL)
{
  st::check_layer<dev::l_requests> ();
  st::check_layer<dev::l_replies> ();
}

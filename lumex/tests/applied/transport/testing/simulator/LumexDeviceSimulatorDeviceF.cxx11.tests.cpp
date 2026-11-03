// The device simulator over the layers of device F: every request row
// of every layer is answered through the dialogue engine and the reply
// decodes as the row says. The de-identified table of
// lumex/tests/core/protocol/devices.

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_f/LumexProtocolDeviceFTable.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace dev = lumex_tests::tables_f;
} // namespace

TEST (LumexDeviceSimulatorDevices, Simulator_AnswersEveryRowOfDeviceF)
{
  st::prepare_t const prepare
      = [] (lumex::core::protocol::index::session_context_t &session)
    { session.set ("thermo_layout", 0); };
  st::check_layer<dev::family_f> (prepare);
  st::check_layer<dev::device_f> (prepare);
}

// The transport matrix over device L: layer a l_requests (Modbus RTU with the
// options of the table). Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_l/LumexProtocolDeviceL.hpp"

namespace lumex_matrix_test
{
/// The wire of device L: Modbus RTU, the device side reading requests.
struct l_wire_t : wire_base_t
{
  using codec_type = fr::modbus_rtu::modbus_rtu_codec;

  static codec_type
  host ()
  {
    return codec_type (lumex_tests::device_l::codec_options (false));
  }

  static codec_type
  device ()
  {
    return codec_type (lumex_tests::device_l::codec_options (true));
  }
};

template <> struct wire_of<lumex_tests::device_l::l_requests> : l_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::device_l;

std::vector<mx::cell_t> const cells
    = mx::cells_of<dev::l_requests> ("device_l", "a");

bool const registered = (mx::register_cells (cells), true);
} // namespace

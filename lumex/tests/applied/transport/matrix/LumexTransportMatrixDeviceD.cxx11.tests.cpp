// The transport matrix over device D: layer a device_d (SLIP and a CRC-16).
// Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_d/LumexProtocolDeviceD.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::device_d::device_d> : slip_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::device_d;

std::vector<mx::cell_t> const cells
    = mx::cells_of<dev::device_d> ("device_d", "a");

bool const registered = (mx::register_cells (cells), true);
} // namespace

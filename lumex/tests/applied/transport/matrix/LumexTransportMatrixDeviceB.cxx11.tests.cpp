// The transport matrix over device B: layer a device_b2 (SLIP and a CRC-16);
// layer b device_b1 (SLIP and a CRC-16). Every behaviour of the simulator, one
// cell each (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::device_b::device_b2> : slip_wire_t
{
};

template <> struct wire_of<lumex_tests::device_b::device_b1> : slip_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::device_b;

std::vector<mx::cell_t> const cells
    = mx::join (mx::cells_of<dev::device_b2> ("device_b", "a"),
                mx::cells_of<dev::device_b1> ("device_b", "b"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

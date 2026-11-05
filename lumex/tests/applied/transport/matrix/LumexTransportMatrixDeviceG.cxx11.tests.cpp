// The transport matrix over device G: layer a device_g (SLIP and a CRC-16);
// layer b old_g (SLIP and a CRC-16). Every behaviour of the simulator, one
// cell each (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_g/LumexProtocolDeviceGTable.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::tables_g::device_g> : slip_wire_t
{
};

template <> struct wire_of<lumex_tests::tables_g::old_g> : slip_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_g;

std::vector<mx::cell_t> const cells
    = mx::join (mx::cells_of<dev::device_g> ("device_g", "a"),
                mx::cells_of<dev::old_g> ("device_g", "b"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

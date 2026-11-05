// The transport matrix over device A: layer a family_a (SLIP and a CRC-16);
// layer b a_old (SLIP and a CRC-16); layer c a_system (SLIP and a CRC-16).
// Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::device_a::family_a> : slip_wire_t
{
};

template <> struct wire_of<lumex_tests::device_a::a_old> : slip_wire_t
{
};

template <> struct wire_of<lumex_tests::device_a::a_system> : slip_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::device_a;

std::vector<mx::cell_t> const cells
    = mx::join (mx::join (mx::cells_of<dev::family_a> ("device_a", "a"),
                          mx::cells_of<dev::a_old> ("device_a", "b")),
                mx::cells_of<dev::a_system> ("device_a", "c"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

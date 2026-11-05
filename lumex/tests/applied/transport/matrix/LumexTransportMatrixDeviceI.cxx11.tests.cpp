// The transport matrix over device I: layer a device_i (SLIP and a CRC-16);
// layer b, the table of layer a over COBS and a CRC-16 (the tables name no
// framing: one layer runs over COBS so that the codec is in the matrix).
// Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_i/LumexProtocolDeviceITable.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::tables_i::device_i> : slip_wire_t
{
};

/// Layer b: device_i over COBS.
struct i_cobs_wire_t : cobs_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_i;

std::vector<mx::cell_t> const cells = mx::join (
    mx::cells_of<dev::device_i> ("device_i", "a"),
    mx::cells_of<dev::device_i, mx::i_cobs_wire_t> ("device_i", "b"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

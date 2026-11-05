// The transport matrix over device J: layer a j_text (text with a check and a
// carriage return); layer b j_hid (SLIP and a CRC-16 in datagrams of report
// 1). Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::tables_j::j_text> : text_wire_t
{
};

template <> struct wire_of<lumex_tests::tables_j::j_hid> : slip_datagram_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_j;

std::vector<mx::cell_t> const cells = mx::join (
    mx::join (mx::join (mx::cells_of<dev::j_text> ("device_j", "a"),
                        mx::cells_of<dev::j_hid> ("device_j", "b")),
              mx::cells_of<dev::j_text> ("device_j", "a", true)),
    mx::cells_of<dev::j_hid> ("device_j", "b", true));

bool const registered = (mx::register_cells (cells), true);
} // namespace

// The transport matrix over device F: layer a device_f (SLIP and a CRC-16).
// Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_f/LumexProtocolDeviceFTable.hpp"

namespace lumex_matrix_test
{
/// The wire of device F: its decoders read a value of the session.
struct f_wire_t : slip_wire_t
{
  static void
  prepare (index::session_context_t &session)
  {
    session.set ("thermo_layout", 0);
  }
};

template <> struct wire_of<lumex_tests::tables_f::device_f> : f_wire_t
{
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_f;

std::vector<mx::cell_t> const cells
    = mx::cells_of<dev::device_f> ("device_f", "a");

bool const registered = (mx::register_cells (cells), true);
} // namespace

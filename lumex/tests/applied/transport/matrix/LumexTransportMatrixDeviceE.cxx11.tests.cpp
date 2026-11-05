// The transport matrix over device E: layer a device_e (SLIP and a CRC-16;
// its row of accepted_in is called in the state that accepts it).
// Every behaviour of the simulator, one cell each
// (LumexTransportMatrixSupport.hpp). The de-identified table of
// lumex/tests/core/protocol/devices.

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_e/LumexProtocolDeviceETable.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::tables_e::device_e> : slip_wire_t
{
  static LUMEX_CONSTEXPR std::int64_t
  accepted_state () LUMEX_NOEXCEPT
  {
    return static_cast<std::int64_t> (
        lumex_tests::tables_e::manager_state::st_35);
  }
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_e;

std::vector<mx::cell_t> const cells
    = mx::cells_of<dev::device_e> ("device_e", "a");

bool const registered = (mx::register_cells (cells), true);
} // namespace

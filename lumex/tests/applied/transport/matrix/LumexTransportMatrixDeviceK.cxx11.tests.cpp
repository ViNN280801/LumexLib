// The transport matrix over device K: layer a device_k (text with a check and
// a carriage return; no retries for a row that names none). Every behaviour of
// the simulator, one cell each (LumexTransportMatrixSupport.hpp). The
// de-identified table of lumex/tests/core/protocol/devices.

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_k/LumexProtocolDeviceKTable.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::tables_k::device_k> : text_wire_t
{
  /// One layer per codec runs without default retries.
  static LUMEX_CONSTEXPR std::size_t
  default_retries () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

} // namespace lumex_matrix_test

namespace
{
namespace mx = lumex_matrix_test;
namespace dev = lumex_tests::tables_k;

std::vector<mx::cell_t> const cells
    = mx::cells_of<dev::device_k> ("device_k", "a");

bool const registered = (mx::register_cells (cells), true);
} // namespace

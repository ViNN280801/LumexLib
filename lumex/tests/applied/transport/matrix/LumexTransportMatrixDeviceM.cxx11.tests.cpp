// The transport matrix over device M: layer a device_m1 (text with a check and
// a carriage return); layer b device_m2 (a binary header and a CRC-32; no
// retries for a row that names none). Every
// behaviour of the simulator, one cell each (LumexTransportMatrixSupport.hpp).
// The de-identified table of lumex/tests/core/protocol/devices.

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::device_m::device_m1> : text_wire_t
{
};

template <> struct wire_of<lumex_tests::device_m::device_m2> : header_wire_t
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
namespace dev = lumex_tests::device_m;

std::vector<mx::cell_t> const cells
    = mx::join (mx::cells_of<dev::device_m1> ("device_m", "a"),
                mx::cells_of<dev::device_m2> ("device_m", "b"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

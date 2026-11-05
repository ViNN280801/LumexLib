// The transport matrix over device A: layer a family_a (SLIP and a CRC-16),
// here as family_checked, the same rows with identify_with on its query of
// the instrument information (the device tables name none), so that the
// client checks the device after every reconnect; layer b a_old (SLIP and a
// CRC-16); layer c a_system (SLIP and a CRC-16). Every behaviour of the
// simulator, one cell each (LumexTransportMatrixSupport.hpp). The
// de-identified table of lumex/tests/core/protocol/devices.

#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_tests
{
namespace device_a
{
/// The header of family_a with the check of the device after a reconnect.
struct family_checked_header : family_header
{
  using lumex_vocabulary
      = lumex::applied::transport::protocol::dialogue::vocabulary_t;

  template <typename P>
  using dialogue_options
      = lumex::applied::transport::protocol::dialogue::options<
          lumex::applied::transport::protocol::dialogue::identify_with<
              typename P::query_ad_t>>;
};

LUMEX_PROTOCOL_DEFINE (family_checked, codec, family_checked_header,
                       DEVICE_A_FAMILY_ROWS);
} // namespace device_a
} // namespace lumex_tests

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
/// The identity of family_checked: the model, the firmware and the serial
/// number of the instrument information.
template <> struct identity_of_t<lumex_tests::device_a::family_checked>
{
  static std::error_code
  read (
      reply_of<lumex_tests::device_a::family_checked::query_ad_t> const &reply,
      identity_t &identity)
  {
    namespace fl = lumex::core::protocol::field;
    identity.model = std::to_string (static_cast<int> (fl::get<0> (reply)));
    identity.firmware = fl::get<1> (reply);
    identity.serial_number = std::to_string (fl::get<8> (reply));
    return std::error_code ();
  }
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace lumex_matrix_test
{
template <> struct wire_of<lumex_tests::device_a::family_checked> : slip_wire_t
{
  static char const *
  identify_row () LUMEX_NOEXCEPT
  {
    return "query_ad";
  }
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
    = mx::join (mx::join (mx::cells_of<dev::family_checked> ("device_a", "a"),
                          mx::cells_of<dev::a_old> ("device_a", "b")),
                mx::cells_of<dev::a_system> ("device_a", "c"));

bool const registered = (mx::register_cells (cells), true);
} // namespace

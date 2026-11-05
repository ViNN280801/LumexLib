// The transport matrix itself: every cell is registered once, named by the
// letters of its device and layer, its transport and its behaviour, and the
// datagram layer leaves out exactly the kinds without a datagram meaning.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"

namespace
{
namespace mx = lumex_matrix_test;

// The layers of the device tables with a wire form of their own: 20 on a
// stream (one of them a layer of the matrix that reads replies of device L,
// one the table of device I over COBS), one on a datagram transport.
std::size_t const kstream_layers = 20;
std::size_t const kdatagram_layers = 1;

bool
is_kind (std::string const &name)
{
  std::vector<mx::behavior_kind> const kinds
      = lumex::applied::transport::testing::all_behavior_kinds ();
  for (std::size_t i = 0; i < kinds.size (); ++i)
    if (name
        == lumex::applied::transport::testing::behavior_kind_name (kinds[i]))
      return true;
  return false;
}

// device_<letter>.<letter>.<stream|datagram>.<kind>
bool
is_cell_name (std::string const &name)
{
  if (name.size () < 12 || name.compare (0, 7, "device_") != 0)
    return false;
  if (name[7] < 'a' || name[7] > 'm' || name[8] != '.' || name[9] < 'a'
      || name[9] > 'z' || name[10] != '.')
    return false;
  std::string const rest = name.substr (11);
  std::string kind;
  if (rest.compare (0, 7, "stream.") == 0)
    kind = rest.substr (7);
  else if (rest.compare (0, 9, "datagram.") == 0)
    kind = rest.substr (9);
  else
    return false;
  return is_kind (kind);
}
} // namespace

TEST (LumexTransportMatrix, Matrix_EveryCellIsNamedByLettersOnce)
{
  std::vector<std::string> names = mx::registered_cells ();
  std::size_t threaded = 0;
  for (std::size_t i = 0; i < names.size (); ++i)
    {
      std::string name = names[i];
      if (name.compare (0, 8, "threads.") == 0)
        {
          ++threaded;
          name = name.substr (8);
        }
      EXPECT_TRUE (is_cell_name (name)) << names[i];
    }
  std::sort (names.begin (), names.end ());
  EXPECT_TRUE (std::adjacent_find (names.begin (), names.end ())
               == names.end ());
  std::size_t const kinds = mx::kinds_for (false).size ();
  std::size_t const datagram_kinds = mx::kinds_for (true).size ();
  EXPECT_EQ (kinds, 18u);
  EXPECT_EQ (datagram_kinds, 16u);
  EXPECT_EQ (names.size () - threaded,
             kstream_layers * kinds + kdatagram_layers * datagram_kinds);
  // The subset for the thread sanitizer: one stream and one datagram layer.
  EXPECT_EQ (threaded, kinds + datagram_kinds);
}

TEST (LumexTransportMatrix, Matrix_DatagramLeavesOutTheStreamOnlyKinds)
{
  std::vector<mx::behavior_kind> const kinds = mx::kinds_for (true);
  EXPECT_TRUE (
      std::find (kinds.begin (), kinds.end (), mx::behavior_kind::zero_read)
      == kinds.end ());
  EXPECT_TRUE (
      std::find (kinds.begin (), kinds.end (), mx::behavior_kind::fragmented)
      == kinds.end ());
  EXPECT_TRUE (std::find (kinds.begin (), kinds.end (),
                          mx::behavior_kind::partial_then_frozen)
               != kinds.end ());
}

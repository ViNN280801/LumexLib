// The byte order shared by the frame codecs and the protocol fields
// (lumex/core/utility/bit/LumexByteOrder.hpp): a header without includes,
// two distinct values.

#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/bit/LumexByteOrder.hpp"

TEST (LumexByteOrderTest, GivenTheTwoOrders_WhenCompared_ThenTheyDiffer)
{
  typedef lumex::core::utility::bit::byte_order byte_order;
  EXPECT_TRUE (byte_order::big != byte_order::little);
  EXPECT_TRUE (std::is_enum<byte_order>::value);
  EXPECT_FALSE ((std::is_convertible<byte_order, int>::value));
}

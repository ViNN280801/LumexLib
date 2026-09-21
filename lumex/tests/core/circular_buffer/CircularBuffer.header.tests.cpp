#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/circular_buffer/CircularBuffer"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using lumex::core::circular_buffer::buffer::CircularBuffer;

// This TU includes the public umbrella before gtest so LUMEX_ASSERT /
// LUMEX_STATIC_ASSERT_MSG from CircularBuffer.hpp are resolved without a
// prior <cassert> or gtest include. CircularBuffer.tests.cpp hides that
// hole because it includes gtest first.

LUMEX_STATIC_ASSERT_MSG (
    (std::is_same<CircularBuffer<int>::value_type, int>::value),
    "CircularBuffer<int>::value_type is int");
LUMEX_STATIC_ASSERT_MSG (
    (std::is_same<CircularBuffer<std::string>::value_type,
                  std::string>::value),
    "CircularBuffer<std::string>::value_type is std::string");

TEST (CircularBufferHeader, FrontBackNonConstAndConst)
{
  CircularBuffer<std::string> buf (2);
  buf.push_back ("a");
  buf.push_back ("b");
  EXPECT_EQ (buf.front (), "a");
  EXPECT_EQ (buf.back (), "b");

  CircularBuffer<std::string> const &cbuf = buf;
  EXPECT_EQ (cbuf.front (), "a");
  EXPECT_EQ (cbuf.back (), "b");
}

TEST (CircularBufferHeader, FrontBackAfterRingOverwrite)
{
  CircularBuffer<std::string> buf (2);
  buf.push_back ("old");
  buf.push_back ("mid");
  buf.push_back ("new");
  EXPECT_EQ (buf.size (), 2U);
  EXPECT_TRUE (buf.full ());
  EXPECT_EQ (buf.front (), "mid");
  EXPECT_EQ (buf.back (), "new");

  CircularBuffer<std::string> const &cbuf = buf;
  EXPECT_EQ (cbuf.front (), "mid");
  EXPECT_EQ (cbuf.back (), "new");
}

TEST (CircularBufferHeader, FrontSafeBackSafeThrowOnEmpty)
{
  CircularBuffer<int> buf (3);
  EXPECT_TRUE (buf.empty ());
  EXPECT_THROW (buf.front_safe (), std::out_of_range);
  EXPECT_THROW (buf.back_safe (), std::out_of_range);

  buf.push_back (7);
  EXPECT_EQ (buf.front_safe (), 7);
  EXPECT_EQ (buf.back_safe (), 7);
}

TEST (CircularBufferHeader, OperatorIndexAndAt)
{
  CircularBuffer<int> buf (3);
  buf.push_back (10);
  buf.push_back (20);
  buf.push_back (30);
  EXPECT_EQ (buf[0], 10);
  EXPECT_EQ (buf[1], 20);
  EXPECT_EQ (buf[2], 30);
  EXPECT_EQ (buf.at (0), 10);
  EXPECT_THROW (buf.at (3), std::out_of_range);

  CircularBuffer<int> const &cbuf = buf;
  EXPECT_EQ (cbuf[1], 20);
  EXPECT_EQ (cbuf.at (2), 30);
}

TEST (CircularBufferHeader, PushPopFrontBack)
{
  CircularBuffer<int> buf (4);
  buf.push_back (1);
  buf.push_back (2);
  buf.push_back (3);
  buf.push_front (0);
  EXPECT_EQ (buf.front (), 0);
  EXPECT_EQ (buf.back (), 3);
  EXPECT_EQ (buf.size (), 4U);

  buf.pop_front ();
  EXPECT_EQ (buf.front (), 1);
  buf.pop_back ();
  EXPECT_EQ (buf.back (), 2);
  EXPECT_EQ (buf.size (), 2U);
}

TEST (CircularBufferHeader, EmplaceStringThenOverwrite)
{
  CircularBuffer<std::string> buf (2);
  buf.emplace_back (3, 'x');
  buf.emplace_back ("yz");
  EXPECT_EQ (buf.front (), "xxx");
  EXPECT_EQ (buf.back (), "yz");
  buf.push_back ("end");
  EXPECT_EQ (buf.front (), "yz");
  EXPECT_EQ (buf.back (), "end");
}

TEST (CircularBufferHeader, SwapExchangesContents)
{
  CircularBuffer<int> a (3);
  CircularBuffer<int> b (2);
  a.push_back (1);
  a.push_back (2);
  b.push_back (9);
  a.swap (b);
  EXPECT_EQ (a.size (), 1U);
  EXPECT_EQ (a.front (), 9);
  EXPECT_EQ (a.back (), 9);
  EXPECT_EQ (b.size (), 2U);
  EXPECT_EQ (b.front (), 1);
  EXPECT_EQ (b.back (), 2);
}

TEST (CircularBufferHeader, CopyAndMoveKeepFrontBack)
{
  CircularBuffer<std::string> src (3);
  src.push_back ("one");
  src.push_back ("two");

  CircularBuffer<std::string> copied (src);
  EXPECT_EQ (copied.front (), "one");
  EXPECT_EQ (copied.back (), "two");
  EXPECT_EQ (copied, src);

  CircularBuffer<std::string> moved (std::move (src));
  EXPECT_EQ (moved.front (), "one");
  EXPECT_EQ (moved.back (), "two");
  EXPECT_EQ (src.size (), 0U);
}

TEST (CircularBufferHeader, ZeroCapacityThrows)
{
  EXPECT_THROW (CircularBuffer<int> buf (0), std::length_error);
}

TEST (CircularBufferHeader, CompareAndClear)
{
  CircularBuffer<int> a (3);
  CircularBuffer<int> b (3);
  a.push_back (1);
  a.push_back (2);
  b.push_back (1);
  b.push_back (2);
  EXPECT_TRUE (a == b);
  EXPECT_FALSE (a < b);
  b.push_back (3);
  EXPECT_FALSE (a == b);
  EXPECT_TRUE (a < b);
  a.clear ();
  EXPECT_TRUE (a.empty ());
  EXPECT_EQ (a.size (), 0U);
}

TEST (CircularBufferHeader, CapacityEmptyFull)
{
  CircularBuffer<int> buf (2);
  EXPECT_EQ (buf.capacity (), 2U);
  EXPECT_TRUE (buf.empty ());
  EXPECT_FALSE (buf.full ());
  buf.push_back (1);
  EXPECT_FALSE (buf.empty ());
  EXPECT_FALSE (buf.full ());
  buf.push_back (2);
  EXPECT_TRUE (buf.full ());
  EXPECT_EQ (buf.size (), buf.capacity ());
}

TEST (CircularBufferHeader, PushFrontOverwritesNewest)
{
  CircularBuffer<int> buf (2);
  buf.push_back (1);
  buf.push_back (2);
  buf.push_front (0);
  EXPECT_EQ (buf.size (), 2U);
  EXPECT_EQ (buf.front (), 0);
  EXPECT_EQ (buf.back (), 1);
  EXPECT_EQ (buf[0], 0);
  EXPECT_EQ (buf[1], 1);
}

TEST (CircularBufferHeader, EmplaceFrontThenOverwrite)
{
  CircularBuffer<std::string> buf (2);
  buf.emplace_front (2, 'a');
  buf.emplace_front ("bb");
  EXPECT_EQ (buf.front (), "bb");
  EXPECT_EQ (buf.back (), "aa");
  buf.push_front ("cc");
  EXPECT_EQ (buf.front (), "cc");
  EXPECT_EQ (buf.back (), "bb");
}

TEST (CircularBufferHeader, IteratorsAndReverse)
{
  CircularBuffer<int> buf (4);
  buf.push_back (1);
  buf.push_back (2);
  buf.push_back (3);

  int sum = 0;
  for (int const value : buf)
    sum += value;
  EXPECT_EQ (sum, 6);

  CircularBuffer<int> const &cbuf = buf;
  EXPECT_EQ (*cbuf.begin (), 1);
  EXPECT_EQ (*cbuf.cbegin (), 1);
  EXPECT_EQ (cbuf.end () - cbuf.begin (), 3);

  EXPECT_EQ (*buf.rbegin (), 3);
  EXPECT_EQ (*buf.crbegin (), 3);
  auto rit = buf.rbegin ();
  ++rit;
  EXPECT_EQ (*rit, 2);
}

TEST (CircularBufferHeader, IteratorsAfterRingOverwrite)
{
  CircularBuffer<int> buf (3);
  buf.push_back (1);
  buf.push_back (2);
  buf.push_back (3);
  buf.push_back (4);
  int seen[3] = {};
  int i = 0;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
  for (int const value : buf)
    seen[i++] = value;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  EXPECT_EQ (i, 3);
  EXPECT_EQ (seen[0], 2);
  EXPECT_EQ (seen[1], 3);
  EXPECT_EQ (seen[2], 4);
}

TEST (CircularBufferHeader, CopyAndMoveAssignment)
{
  CircularBuffer<std::string> src (3);
  src.push_back ("a");
  src.push_back ("b");

  CircularBuffer<std::string> copied (1);
  copied = src;
  EXPECT_EQ (copied.capacity (), 3U);
  EXPECT_EQ (copied.front (), "a");
  EXPECT_EQ (copied.back (), "b");
  EXPECT_EQ (copied, src);

  CircularBuffer<std::string> moved (1);
  moved = std::move (src);
  EXPECT_EQ (moved.front (), "a");
  EXPECT_EQ (moved.back (), "b");
  EXPECT_EQ (src.size (), 0U);
}

TEST (CircularBufferHeader, GlobalSwap)
{
  CircularBuffer<int> a (2);
  CircularBuffer<int> b (3);
  a.push_back (1);
  b.push_back (8);
  b.push_back (9);
  swap (a, b);
  EXPECT_EQ (a.size (), 2U);
  EXPECT_EQ (a.front (), 8);
  EXPECT_EQ (a.back (), 9);
  EXPECT_EQ (b.size (), 1U);
  EXPECT_EQ (b.front (), 1);
}

TEST (CircularBufferHeader, SingleElementOverwrite)
{
  CircularBuffer<int> buf (1);
  buf.push_back (1);
  buf.push_back (2);
  EXPECT_EQ (buf.size (), 1U);
  EXPECT_TRUE (buf.full ());
  EXPECT_EQ (buf.front (), 2);
  EXPECT_EQ (buf.back (), 2);
  buf.pop_front ();
  EXPECT_TRUE (buf.empty ());
  EXPECT_THROW (buf.front_safe (), std::out_of_range);
}

TEST (CircularBufferHeader, EmptyOperations)
{
  CircularBuffer<int> buf (3);
  EXPECT_EQ (buf.begin (), buf.end ());
  EXPECT_EQ (buf.cbegin (), buf.cend ());
  buf.pop_front ();
  buf.pop_back ();
  EXPECT_TRUE (buf.empty ());
  buf.clear ();
  EXPECT_EQ (buf.capacity (), 3U);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

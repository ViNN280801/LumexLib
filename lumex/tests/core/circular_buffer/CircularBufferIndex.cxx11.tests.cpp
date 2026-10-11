// The index arithmetic of the ring (the wrap by a compare and a subtraction):
// capacities 1 and 2, every head position, every size, copies of wrapped
// buffers, and a model-based run against std::deque.
#include <deque>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/circular_buffer/CircularBuffer"

#include "CircularBufferTestSupport.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif

using lumex::core::circular_buffer::circular_buffer;

namespace
{
std::vector<int>
expected_range (int first, std::size_t count)
{
  std::vector<int> out;
  for (std::size_t i = 0; i < count; ++i)
    out.push_back (first + static_cast<int> (i));
  return out;
}
} // namespace

// Every capacity, every head position and every size: the logical sequence,
// front, back and the iterators read the right slots (wrap at capacity).
TEST (CircularBufferIndex, EveryHeadAndSize_ReadsTheLogicalSequence)
{
  for (std::size_t capacity = 1; capacity <= 7; ++capacity)
    for (std::size_t head = 0; head < capacity; ++head)
      for (std::size_t count = 0; count <= capacity; ++count)
        {
          circular_buffer<int> buffer
              = cbtest::make_buffer<circular_buffer<int>> (capacity, head,
                                                           count, 10);
          SCOPED_TRACE (testing::Message ()
                        << "capacity " << capacity << " head " << head
                        << " count " << count);
          ASSERT_EQ (buffer.size (), count);
          EXPECT_EQ (cbtest::content_of (buffer), expected_range (10, count));
          EXPECT_EQ (std::vector<int> (buffer.begin (), buffer.end ()),
                     expected_range (10, count));
          if (count > 0)
            {
              EXPECT_EQ (buffer.front (), 10);
              EXPECT_EQ (buffer.back (), 10 + static_cast<int> (count) - 1);
            }
        }
}

// Capacity 1: the only slot wraps onto itself in both directions.
TEST (CircularBufferIndex, CapacityOne_WrapsOntoItself)
{
  circular_buffer<int> buffer (1);
  for (int i = 0; i < 5; ++i)
    {
      buffer.push_back (i);
      ASSERT_EQ (buffer.size (), 1U);
      EXPECT_EQ (buffer.front (), i);
      EXPECT_EQ (buffer.back (), i);
    }
  for (int i = 10; i < 15; ++i)
    {
      buffer.push_front (i);
      ASSERT_EQ (buffer.size (), 1U);
      EXPECT_EQ (buffer.front (), i);
    }
  buffer.pop_front ();
  EXPECT_TRUE (buffer.empty ());
  buffer.push_front (7);
  buffer.pop_back ();
  EXPECT_TRUE (buffer.empty ());
  buffer.push_back (8);
  EXPECT_EQ (buffer[0], 8);
}

// Capacity 2: both neighbours of a slot are the other slot.
TEST (CircularBufferIndex, CapacityTwo_WrapsInBothDirections)
{
  circular_buffer<int> buffer (2);
  buffer.push_back (1);
  buffer.push_back (2);
  buffer.push_back (3); // overwrites 1: head moves to slot 1
  EXPECT_EQ (cbtest::content_of (buffer), (std::vector<int>{ 2, 3 }));
  buffer.push_back (4); // head wraps to slot 0
  EXPECT_EQ (cbtest::content_of (buffer), (std::vector<int>{ 3, 4 }));
  buffer.push_front (5); // overwrites the newest, head goes back to slot 1
  EXPECT_EQ (cbtest::content_of (buffer), (std::vector<int>{ 5, 3 }));
  buffer.push_front (6); // head wraps from slot 1 to slot 0
  EXPECT_EQ (cbtest::content_of (buffer), (std::vector<int>{ 6, 5 }));
  buffer.pop_front ();
  buffer.pop_front ();
  EXPECT_TRUE (buffer.empty ());
  buffer.push_front (9);
  buffer.push_front (8);
  EXPECT_EQ (cbtest::content_of (buffer), (std::vector<int>{ 8, 9 }));
}

// The head reaches the last slot and the next push or pop wraps exactly at
// the capacity (not one slot early or late).
TEST (CircularBufferIndex, WrapExactlyAtCapacity)
{
  circular_buffer<int> buffer (4);
  for (int i = 0; i < 4; ++i)
    buffer.push_back (i);
  for (int step = 0; step < 12; ++step)
    {
      buffer.pop_front ();
      buffer.push_back (100 + step);
      ASSERT_EQ (buffer.size (), 4U);
      EXPECT_EQ (buffer.back (), 100 + step);
    }
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 108, 109, 110, 111 }));
  for (int step = 0; step < 12; ++step)
    {
      buffer.pop_back ();
      buffer.push_front (200 + step);
      EXPECT_EQ (buffer.front (), 200 + step);
    }
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 211, 210, 209, 208 }));
}

// The copy constructor and the copy assignment copy the wrapped content in
// the logical order, for every head position and size.
TEST (CircularBufferIndex, CopyOfWrappedBuffer_KeepsLogicalOrder)
{
  for (std::size_t capacity = 1; capacity <= 6; ++capacity)
    for (std::size_t head = 0; head < capacity; ++head)
      for (std::size_t count = 0; count <= capacity; ++count)
        {
          circular_buffer<std::string> source (capacity);
          for (std::size_t i = 0; i < head; ++i)
            {
              source.push_back ("x");
              source.pop_front ();
            }
          for (std::size_t i = 0; i < count; ++i)
            source.push_back (std::string (40, 'a')
                              + std::to_string (static_cast<int> (i)));
          SCOPED_TRACE (testing::Message ()
                        << "capacity " << capacity << " head " << head
                        << " count " << count);

          circular_buffer<std::string> copy (source);
          EXPECT_EQ (copy.capacity (), capacity);
          EXPECT_EQ (cbtest::content_of (copy), cbtest::content_of (source));

          circular_buffer<std::string> assigned (3);
          assigned.push_back ("old");
          assigned = source;
          EXPECT_EQ (assigned.capacity (), capacity);
          EXPECT_EQ (cbtest::content_of (assigned),
                     cbtest::content_of (source));
          // the copy has its own storage and can grow after the copy
          copy.push_back ("tail");
          EXPECT_EQ (copy.back (), "tail");
        }
}

// Random operations against std::deque used as the reference model.
TEST (CircularBufferIndex, RandomOperations_MatchTheDequeModel)
{
  for (std::size_t capacity = 1; capacity <= 9; ++capacity)
    {
      cbtest::lcg random (static_cast<std::uint32_t> (capacity) * 7919U);
      circular_buffer<int> buffer (capacity);
      std::deque<int> model;
      for (int step = 0; step < 4000; ++step)
        {
          int const value = step;
          switch (random.below (7))
            {
            case 0:
            case 1:
              buffer.push_back (value);
              if (model.size () == capacity)
                model.pop_front ();
              model.push_back (value);
              break;
            case 2:
            case 3:
              buffer.push_front (value);
              if (model.size () == capacity)
                model.pop_back ();
              model.push_front (value);
              break;
            case 4:
              buffer.pop_front ();
              if (!model.empty ())
                model.pop_front ();
              break;
            case 5:
              buffer.pop_back ();
              if (!model.empty ())
                model.pop_back ();
              break;
            default:
              if (random.below (8) == 0)
                {
                  buffer.clear ();
                  model.clear ();
                }
              else
                {
                  buffer.emplace_back (value);
                  if (model.size () == capacity)
                    model.pop_front ();
                  model.push_back (value);
                }
              break;
            }
          ASSERT_EQ (buffer.size (), model.size ())
              << "capacity " << capacity << " step " << step;
          for (std::size_t i = 0; i < model.size (); ++i)
            ASSERT_EQ (buffer[i], model[i])
                << "capacity " << capacity << " step " << step << " index "
                << i;
          if (!model.empty ())
            {
              ASSERT_EQ (buffer.front (), model.front ());
              ASSERT_EQ (buffer.back (), model.back ());
            }
        }
    }
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

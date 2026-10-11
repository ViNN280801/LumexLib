// The random-access iterators: the mixed iterator / const_iterator
// comparison, operator[], the arithmetic, and the standard algorithms that
// need random access. Compiled at every standard of the module (at C++20 the
// rewritten comparison candidates must not be ambiguous).
#include <algorithm>
#include <iterator>
#include <numeric>
#include <string>
#include <type_traits>
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
using buffer_t = circular_buffer<int>;
using iterator = buffer_t::iterator;
using const_iterator = buffer_t::const_iterator;

static_assert (std::is_same<std::iterator_traits<iterator>::iterator_category,
                            std::random_access_iterator_tag>::value,
               "iterator is random access");
static_assert (
    std::is_same<std::iterator_traits<const_iterator>::iterator_category,
                 std::random_access_iterator_tag>::value,
    "const_iterator is random access");
static_assert (
    std::is_same<std::iterator_traits<iterator>::reference, int &>::value,
    "iterator reference");
static_assert (std::is_same<std::iterator_traits<const_iterator>::reference,
                            int const &>::value,
               "const_iterator reference");
static_assert (std::is_same<std::iterator_traits<const_iterator>::pointer,
                            int const *>::value,
               "const_iterator pointer");
static_assert (std::is_convertible<iterator, const_iterator>::value,
               "iterator converts to const_iterator");
static_assert (!std::is_convertible<const_iterator, iterator>::value,
               "const_iterator does not convert to iterator");
static_assert (std::is_default_constructible<iterator>::value
                   && std::is_default_constructible<const_iterator>::value,
               "iterators are default constructible");

/// A buffer of capacity 5 whose head is at slot 3: the content 1..5 wraps.
buffer_t
wrapped_buffer ()
{
  return cbtest::make_buffer<buffer_t> (5, 3, 5, 1);
}
} // namespace

TEST (CircularBufferIterators, MixedEquality_BothDirections)
{
  buffer_t buffer = wrapped_buffer ();
  iterator const first = buffer.begin ();
  const_iterator const cfirst = buffer.cbegin ();
  const_iterator const csecond = cfirst + 1;
  EXPECT_TRUE (first == cfirst);
  EXPECT_TRUE (cfirst == first);
  EXPECT_FALSE (first != cfirst);
  EXPECT_FALSE (cfirst != first);
  EXPECT_FALSE (first == csecond);
  EXPECT_FALSE (csecond == first);
  EXPECT_TRUE (first != csecond);
  EXPECT_TRUE (csecond != first);
  EXPECT_TRUE (buffer.end () == buffer.cend ());
  EXPECT_TRUE (buffer.cend () == buffer.end ());
}

TEST (CircularBufferIterators, MixedEquality_DifferentBuffersAreNotEqual)
{
  buffer_t a = wrapped_buffer ();
  buffer_t b = wrapped_buffer ();
  EXPECT_FALSE (a.begin () == b.cbegin ());
  EXPECT_FALSE (b.cbegin () == a.begin ());
  EXPECT_TRUE (a.begin () != b.cbegin ());
  EXPECT_FALSE (a.cbegin () == b.cbegin ());
  EXPECT_FALSE (a.begin () == b.begin ());
}

TEST (CircularBufferIterators, MixedOrdering_BothDirections)
{
  buffer_t buffer = wrapped_buffer ();
  iterator const low = buffer.begin () + 1;
  const_iterator const high = buffer.cbegin () + 3;
  EXPECT_TRUE (low < high);
  EXPECT_FALSE (high < low);
  EXPECT_TRUE (high > low);
  EXPECT_FALSE (low > high);
  EXPECT_TRUE (low <= high);
  EXPECT_FALSE (high <= low);
  EXPECT_TRUE (high >= low);
  EXPECT_FALSE (low >= high);
  EXPECT_TRUE (low <= buffer.cbegin () + 1);
  EXPECT_TRUE (buffer.cbegin () + 1 <= low);
  EXPECT_TRUE (low >= buffer.cbegin () + 1);
  EXPECT_TRUE (buffer.cbegin () + 1 >= low);
}

TEST (CircularBufferIterators, MixedDifference_BothDirections)
{
  buffer_t buffer = wrapped_buffer ();
  iterator const low = buffer.begin () + 1;
  const_iterator const high = buffer.cbegin () + 4;
  EXPECT_EQ (high - low, 3);
  EXPECT_EQ (low - high, -3);
  EXPECT_EQ (buffer.end () - buffer.cbegin (), 5);
  EXPECT_EQ (buffer.cend () - buffer.begin (), 5);
  EXPECT_EQ (std::distance (buffer.cbegin (), buffer.cend ()), 5);
}

TEST (CircularBufferIterators, Subscript_ReadsRelativeToTheIterator)
{
  buffer_t buffer = wrapped_buffer ();
  iterator const first = buffer.begin ();
  EXPECT_EQ (first[0], 1);
  EXPECT_EQ (first[2], 3);
  EXPECT_EQ (first[4], 5);
  EXPECT_EQ ((first + 3)[-1], 3);
  EXPECT_EQ ((buffer.end ())[-1], 5);
  first[1] = 20; // writable through the non-const iterator
  EXPECT_EQ (buffer[1], 20);
  const_iterator const cfirst = buffer.cbegin ();
  EXPECT_EQ (cfirst[1], 20);
  EXPECT_EQ ((cfirst + 4)[-4], 1);
  buffer_t const &view = buffer;
  EXPECT_EQ (view.begin ()[3], 4);
  EXPECT_EQ (buffer.rbegin ()[1], 4);
  EXPECT_EQ (buffer.crbegin ()[4], 1);
}

TEST (CircularBufferIterators, Arithmetic_ForwardAndBackward)
{
  buffer_t buffer = wrapped_buffer ();
  iterator it = buffer.begin ();
  it += 3;
  EXPECT_EQ (*it, 4);
  it -= 2;
  EXPECT_EQ (*it, 2);
  it += -1;
  EXPECT_EQ (*it, 1);
  it -= -2;
  EXPECT_EQ (*it, 3);
  EXPECT_EQ (*(it + 2), 5);
  EXPECT_EQ (*(2 + it), 5);
  EXPECT_EQ (*(it - 2), 1);
  EXPECT_EQ ((it + 2) - it, 2);
  EXPECT_EQ (it - (it + 2), -2);

  iterator copy = it++;
  EXPECT_EQ (*copy, 3);
  EXPECT_EQ (*it, 4);
  copy = it--;
  EXPECT_EQ (*copy, 4);
  EXPECT_EQ (*it, 3);
  EXPECT_EQ (*++it, 4);
  EXPECT_EQ (*--it, 3);

  const_iterator cit = buffer.cbegin ();
  cit += 4;
  EXPECT_EQ (*cit, 5);
  cit -= 4;
  EXPECT_EQ (*cit, 1);
  EXPECT_EQ (*(1 + cit), 2);
  EXPECT_EQ (*(cit + 1), 2);
  EXPECT_EQ ((cit + 4) - cit, 4);
}

TEST (CircularBufferIterators, ConversionToConstIterator_KeepsThePosition)
{
  buffer_t buffer = wrapped_buffer ();
  iterator const it = buffer.begin () + 2;
  const_iterator const cit = it;
  EXPECT_EQ (*cit, 3);
  EXPECT_TRUE (cit == it);
  const_iterator assigned;
  assigned = buffer.begin () + 4;
  EXPECT_EQ (*assigned, 5);
}

TEST (CircularBufferIterators, DefaultConstructedIterators_CompareEqual)
{
  EXPECT_TRUE (iterator () == iterator ());
  EXPECT_TRUE (const_iterator () == const_iterator ());
  EXPECT_TRUE (iterator () == const_iterator ());
  EXPECT_EQ (iterator () - iterator (), 0);
}

TEST (CircularBufferIterators, MemberAccessThroughArrow)
{
  circular_buffer<std::string> buffer (3);
  buffer.push_back ("one");
  buffer.push_back ("three");
  EXPECT_EQ (buffer.begin ()->size (), 3U);
  EXPECT_EQ ((buffer.cbegin () + 1)->size (), 5U);
  buffer.begin ()->append ("!");
  EXPECT_EQ (buffer.front (), "one!");
}

TEST (CircularBufferIterators, StandardAlgorithms_OnAWrappedBuffer)
{
  buffer_t buffer (7);
  for (std::size_t i = 0; i < 3; ++i) // move the head
    {
      buffer.push_back (0);
      buffer.pop_front ();
    }
  for (int value : { 5, 3, 9, 1, 7, 2, 8 })
    buffer.push_back (value);
  ASSERT_FALSE (buffer.empty ());

  EXPECT_EQ (std::accumulate (buffer.begin (), buffer.end (), 0), 35);
  EXPECT_EQ (*std::max_element (buffer.begin (), buffer.end ()), 9);
  EXPECT_EQ (std::count_if (buffer.cbegin (), buffer.cend (),
                            [] (int v) { return v > 4; }),
             4);

  std::sort (buffer.begin (), buffer.end ());
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 1, 2, 3, 5, 7, 8, 9 }));
  EXPECT_TRUE (std::is_sorted (buffer.cbegin (), buffer.cend ()));
  EXPECT_EQ (*std::lower_bound (buffer.begin (), buffer.end (), 7), 7);
  EXPECT_EQ (std::lower_bound (buffer.cbegin (), buffer.cend (), 4)
                 - buffer.cbegin (),
             3);

  std::reverse (buffer.begin (), buffer.end ());
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 9, 8, 7, 5, 3, 2, 1 }));
  std::rotate (buffer.begin (), buffer.begin () + 2, buffer.end ());
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 7, 5, 3, 2, 1, 9, 8 }));
  std::nth_element (buffer.begin (), buffer.begin () + 3, buffer.end ());
  EXPECT_EQ (buffer[3], 5);

  std::vector<int> copy (buffer.rbegin (), buffer.rend ());
  EXPECT_EQ (copy.size (), 7U);
  EXPECT_EQ (copy.front (), buffer.back ());
  std::fill (buffer.begin () + 1, buffer.end () - 1, 4);
  EXPECT_EQ (buffer[3], 4);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

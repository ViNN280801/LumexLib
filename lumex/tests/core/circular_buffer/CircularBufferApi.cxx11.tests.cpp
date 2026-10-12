// The members added after the Boost interface: default, range, fill and
// initializer_list construction, assign, resize, set_capacity, rset_capacity,
// insert, erase, erase_begin, erase_end, linearize, is_linearized, max_size.
// Expected values are written out by hand and checked against a vector model
// for every capacity, head position and size (the differential test against
// boost::circular_buffer is in CircularBufferBoost.cxx11.tests.cpp).
#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
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
#pragma clang diagnostic ignored "-Wpadded"
#endif

using cbtest::fragile;
using cbtest::ledger;
using lumex::core::circular_buffer::circular_buffer;

namespace
{
using ints = std::vector<int>;
using buffer_t = circular_buffer<int>;
using uptr = std::unique_ptr<int>;

struct clean_slate
{
  clean_slate ()
  {
    ledger ().reset ();
    fragile::reset ();
  }
  ~clean_slate () { fragile::reset (); }
};

using fragile_alloc = cbtest::tracking_alloc<fragile, false, false, false>;
using fragile_buffer = circular_buffer<fragile, fragile_alloc>;

ints
values_of (fragile_buffer const &buffer)
{
  ints out;
  for (std::size_t i = 0; i < buffer.size (); ++i)
    out.push_back (buffer[i].value);
  return out;
}

/// A buffer of the given capacity whose head is at slot `head` and that holds
/// `items` (at most `capacity` of them).
buffer_t
make_with (std::size_t capacity, std::size_t head, ints const &items)
{
  buffer_t buffer (capacity);
  for (std::size_t i = 0; i < head; ++i)
    {
      buffer.push_back (-1);
      buffer.pop_front ();
    }
  for (std::size_t i = 0; i < items.size (); ++i)
    buffer.push_back (items[i]);
  return buffer;
}

ints
iota_from (int first, std::size_t count)
{
  ints out;
  for (std::size_t i = 0; i < count; ++i)
    out.push_back (first + static_cast<int> (i));
  return out;
}

/// The result of an insert: the last `capacity` elements of the sequence.
ints
model_insert (ints content, std::size_t capacity, std::size_t pos,
              ints const &items)
{
  content.insert (content.begin () + static_cast<std::ptrdiff_t> (pos),
                  items.begin (), items.end ());
  if (content.size () > capacity)
    content.erase (content.begin (), content.begin ()
                                         + static_cast<std::ptrdiff_t> (
                                             content.size () - capacity));
  return content;
}

/// An allocator with a tiny max_size, to see max_size () and the length check.
template <class T> struct limited_alloc : std::allocator<T>
{
  template <class U> struct rebind
  {
    using other = limited_alloc<U>;
  };

  limited_alloc () = default;
  template <class U> limited_alloc (limited_alloc<U> const &) {}

  std::size_t
  max_size () const
  {
    return 10;
  }

  template <class U>
  bool
  operator== (limited_alloc<U> const &) const
  {
    return true;
  }

  template <class U>
  bool
  operator!= (limited_alloc<U> const &) const
  {
    return false;
  }
};

} // namespace

// === constructors ==========================================================

TEST (CircularBufferApi, DefaultConstructor_MakesAnEmptyBufferOfCapacityZero)
{
  buffer_t buffer;
  EXPECT_EQ (buffer.capacity (), 0U);
  EXPECT_EQ (buffer.size (), 0U);
  buffer.set_capacity (2);
  buffer.push_back (1);
  EXPECT_EQ (buffer.front (), 1);
}

TEST (CircularBufferApi, RangeConstructor_CapacityIsTheLengthOfTheRange)
{
  ints const source = { 4, 5, 6 };
  buffer_t from_vector (source.begin (), source.end ());
  EXPECT_EQ (from_vector.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (from_vector), source);
  EXPECT_TRUE (from_vector.full ());

  std::list<int> const list (source.begin (), source.end ());
  buffer_t from_list (list.begin (), list.end ());
  EXPECT_EQ (from_list.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (from_list), source);

  std::istringstream stream ("4 5 6");
  buffer_t from_input ((std::istream_iterator<int> (stream)),
                       std::istream_iterator<int> ());
  EXPECT_EQ (from_input.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (from_input), source);

  buffer_t from_empty (source.begin (), source.begin ());
  EXPECT_EQ (from_empty.capacity (), 0U);

  ints const strings_source = { 1 };
  circular_buffer<long> converted (strings_source.begin (),
                                   strings_source.end ());
  EXPECT_EQ (converted.front (), 1L);
}

TEST (CircularBufferApi, RangeConstructor_WithCapacity_KeepsTheLastElements)
{
  ints const source = iota_from (1, 5);
  std::list<int> const list (source.begin (), source.end ());

  buffer_t cut (3, source.begin (), source.end ());
  EXPECT_EQ (cut.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (cut), (ints{ 3, 4, 5 }));

  buffer_t cut_list (3, list.begin (), list.end ());
  EXPECT_EQ (cbtest::content_of (cut_list), (ints{ 3, 4, 5 }));

  buffer_t exact (5, source.begin (), source.end ());
  EXPECT_EQ (cbtest::content_of (exact), source);

  buffer_t roomy (8, list.begin (), list.end ());
  EXPECT_EQ (roomy.capacity (), 8U);
  EXPECT_EQ (cbtest::content_of (roomy), source);
  roomy.push_back (6);
  EXPECT_EQ (roomy.back (), 6);

  std::istringstream stream ("1 2 3 4 5");
  buffer_t cut_input (3, std::istream_iterator<int> (stream),
                      std::istream_iterator<int> ());
  EXPECT_EQ (cbtest::content_of (cut_input), (ints{ 3, 4, 5 }));

  std::istringstream stream2 ("1 2 3");
  buffer_t roomy_input (6, std::istream_iterator<int> (stream2),
                        std::istream_iterator<int> ());
  EXPECT_EQ (roomy_input.capacity (), 6U);
  EXPECT_EQ (cbtest::content_of (roomy_input), (ints{ 1, 2, 3 }));

  buffer_t none (0, source.begin (), source.end ());
  EXPECT_EQ (none.capacity (), 0U);
  EXPECT_TRUE (none.empty ());
}

TEST (CircularBufferApi, InitializerListConstructor_KeepsTheOrder)
{
  buffer_t full{ 1, 2, 3 };
  EXPECT_EQ (full.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (full), (ints{ 1, 2, 3 }));

  buffer_t roomy (5, { 1, 2, 3 });
  EXPECT_EQ (roomy.capacity (), 5U);
  EXPECT_EQ (cbtest::content_of (roomy), (ints{ 1, 2, 3 }));

  buffer_t cut (2, { 1, 2, 3 });
  EXPECT_EQ (cut.capacity (), 2U);
  EXPECT_EQ (cbtest::content_of (cut), (ints{ 2, 3 }));

  circular_buffer<std::string> strings{ "a", "b" };
  EXPECT_EQ (strings.back (), "b");
}

TEST (CircularBufferApi, FillConstructors_MakeCopiesOfTheItem)
{
  buffer_t full (4, 7);
  EXPECT_EQ (full.capacity (), 4U);
  EXPECT_EQ (cbtest::content_of (full), (ints{ 7, 7, 7, 7 }));

  buffer_t roomy (6, 2, 7);
  EXPECT_EQ (roomy.capacity (), 6U);
  EXPECT_EQ (cbtest::content_of (roomy), (ints{ 7, 7 }));

  buffer_t cut (2, 5, 7); // more copies than the capacity: cut to it
  EXPECT_EQ (cut.capacity (), 2U);
  EXPECT_EQ (cut.size (), 2U);

  // a capacity and an allocator: the explicit constructor, not a fill
  cbtest::plain_alloc alloc (4);
  circular_buffer<int, cbtest::plain_alloc> with_alloc (3, alloc);
  EXPECT_TRUE (with_alloc.empty ());
  EXPECT_EQ (with_alloc.get_allocator ().id, 4);
}

// === assign ================================================================

TEST (CircularBufferApi, Assign_Fill_SetsTheCapacityToTheCount)
{
  buffer_t buffer = make_with (3, 2, { 1, 2 });
  buffer.assign (4, 7);
  EXPECT_EQ (buffer.capacity (), 4U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 7, 7, 7, 7 }));

  buffer.assign (6, 2, 8);
  EXPECT_EQ (buffer.capacity (), 6U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 8, 8 }));

  buffer.assign (0, 3);
  EXPECT_EQ (buffer.capacity (), 0U);
  EXPECT_TRUE (buffer.empty ());
}

TEST (CircularBufferApi, Assign_Range_ReplacesTheContent)
{
  buffer_t buffer = make_with (3, 1, { 1, 2, 3 });
  ints const source = iota_from (10, 5);
  buffer.assign (source.begin (), source.end ());
  EXPECT_EQ (buffer.capacity (), 5U);
  EXPECT_EQ (cbtest::content_of (buffer), source);

  buffer.assign (3, source.begin (), source.end ());
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 12, 13, 14 }));

  buffer.assign (7, source.begin (), source.begin () + 2);
  EXPECT_EQ (buffer.capacity (), 7U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 10, 11 }));

  std::istringstream stream ("1 2 3");
  buffer.assign (std::istream_iterator<int> (stream),
                 std::istream_iterator<int> ());
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 3 }));
}

TEST (CircularBufferApi, Assign_InitializerList_ReplacesTheContent)
{
  buffer_t buffer = make_with (6, 4, { 1, 2, 3, 4 });
  buffer.assign ({ 7, 8, 9 });
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 7, 8, 9 }));
  buffer.assign (5, { 1, 2 });
  EXPECT_EQ (buffer.capacity (), 5U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2 }));
  buffer.assign (1, { 1, 2 });
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 2 }));
}

TEST (CircularBufferApi, Assign_FromTheBufferItself_IsSafe)
{
  circular_buffer<std::string> buffer (3);
  buffer.push_back (std::string (40, 'a'));
  buffer.push_back (std::string (40, 'b'));
  std::string const first = buffer.front ();
  buffer.assign (2, buffer.front ());
  EXPECT_EQ (buffer.size (), 2U);
  EXPECT_EQ (buffer[0], first);
  EXPECT_EQ (buffer[1], first);

  buffer_t numbers = make_with (3, 1, { 1, 2, 3 });
  numbers.assign (numbers.begin (), numbers.end ());
  EXPECT_EQ (cbtest::content_of (numbers), (ints{ 1, 2, 3 }));
  numbers.assign (2, numbers.begin () + 1, numbers.end ());
  EXPECT_EQ (cbtest::content_of (numbers), (ints{ 2, 3 }));
}

TEST (CircularBufferApi, Assign_KeepsTheAllocatorAndIsStrong)
{
  clean_slate slate;
  {
    circular_buffer<int, cbtest::plain_alloc> buffer (3,
                                                      cbtest::plain_alloc (3));
    buffer.push_back (1);
    buffer.assign (5, 2);
    EXPECT_EQ (buffer.get_allocator ().id, 3);
    EXPECT_EQ (buffer.size (), 5U);
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U);
  EXPECT_EQ (ledger ().live_blocks (), 0U);

  {
    fragile_buffer buffer (3, fragile_alloc (1));
    for (int v = 1; v <= 3; ++v)
      buffer.push_back (fragile (v));
    fragile const item (9);
    for (int allowed = 0; allowed < 4; ++allowed)
      {
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.assign (4, item), std::runtime_error);
        fragile::reset ();
        EXPECT_EQ (values_of (buffer), (ints{ 1, 2, 3 }));
        EXPECT_EQ (buffer.capacity (), 3U);
      }
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

// === resize, set_capacity, rset_capacity ===================================

TEST (CircularBufferApi, Resize_Grow_AppendsCopiesAndRaisesTheCapacity)
{
  buffer_t buffer = make_with (3, 2, { 1, 2 });
  buffer.resize (5, 7);
  EXPECT_EQ (buffer.capacity (), 5U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 7, 7, 7 }));

  buffer_t roomy = make_with (6, 4, { 1, 2 });
  roomy.resize (4, 9);
  EXPECT_EQ (roomy.capacity (), 6U); // within the capacity: unchanged
  EXPECT_EQ (cbtest::content_of (roomy), (ints{ 1, 2, 9, 9 }));

  buffer_t defaulted = make_with (4, 1, { 5 });
  defaulted.resize (3);
  EXPECT_EQ (cbtest::content_of (defaulted), (ints{ 5, 0, 0 }));
  EXPECT_EQ (defaulted.capacity (), 4U);
}

TEST (CircularBufferApi, Resize_Shrink_RemovesTheNewestElements)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  buffer.resize (2, 0);
  EXPECT_EQ (buffer.capacity (), 5U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2 }));
  buffer.resize (2, 0);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2 }));
  buffer.resize (0);
  EXPECT_TRUE (buffer.empty ());
}

TEST (CircularBufferApi, Resize_WithAnElementOfTheBuffer_IsSafe)
{
  circular_buffer<std::string> buffer (2);
  buffer.push_back (std::string (40, 'a'));
  buffer.push_back (std::string (40, 'b'));
  std::string const first = buffer.front ();
  buffer.resize (5, buffer.front ()); // reallocates: the item moves away
  ASSERT_EQ (buffer.size (), 5U);
  for (std::size_t i = 2; i < 5; ++i)
    EXPECT_EQ (buffer[i], first);
}

TEST (CircularBufferApi, Resize_ThrowingCopy_RemovesTheAddedElements)
{
  clean_slate slate;
  {
    fragile_buffer buffer (6, fragile_alloc (1));
    for (int v = 1; v <= 3; ++v)
      buffer.push_back (fragile (v));
    fragile const item (9);
    for (int allowed = 0; allowed < 3; ++allowed)
      {
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.resize (6, item), std::runtime_error);
        fragile::reset ();
        EXPECT_EQ (values_of (buffer), (ints{ 1, 2, 3 }));
      }
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferApi, SetCapacity_Grow_KeepsTheContentOfAWrappedBuffer)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  ASSERT_FALSE (buffer.is_linearized ());
  buffer.set_capacity (8);
  EXPECT_EQ (buffer.capacity (), 8U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 3, 4, 5 }));
  EXPECT_TRUE (buffer.is_linearized ());
  buffer.push_back (6);
  buffer.push_back (7);
  buffer.push_back (8);
  buffer.push_back (9); // overwrites 1
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 2, 3, 4, 5, 6, 7, 8, 9 }));
}

TEST (CircularBufferApi, SetCapacity_Shrink_RemovesTheNewestElements)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  buffer.set_capacity (3);
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 3 }));
  buffer.set_capacity (0);
  EXPECT_EQ (buffer.capacity (), 0U);
  EXPECT_TRUE (buffer.empty ());
  buffer.set_capacity (2); // from zero to something
  buffer.push_back (4);
  buffer.push_back (5);
  buffer.push_back (6);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 5, 6 }));
}

TEST (CircularBufferApi, RsetCapacity_Shrink_RemovesTheOldestElements)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  buffer.rset_capacity (3);
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5 }));
  buffer.rset_capacity (6);
  EXPECT_EQ (buffer.capacity (), 6U);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5 }));
  buffer.rset_capacity (0);
  EXPECT_EQ (buffer.capacity (), 0U);
  EXPECT_TRUE (buffer.empty ());
}

TEST (CircularBufferApi, SetCapacity_SameCapacity_DoesNothing)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  int const *const first = &buffer[0];
  buffer.set_capacity (5);
  buffer.rset_capacity (5);
  EXPECT_EQ (&buffer[0], first); // no reallocation
  EXPECT_FALSE (buffer.is_linearized ());
}

TEST (CircularBufferApi,
      SetCapacity_AboveMaxSize_ThrowsLengthErrorAndKeepsTheBuffer)
{
  circular_buffer<int, limited_alloc<int>> buffer (3);
  buffer.push_back (1);
  EXPECT_EQ (buffer.max_size (), 10U);
  EXPECT_THROW (buffer.set_capacity (11), std::length_error);
  EXPECT_THROW (buffer.rset_capacity (11), std::length_error);
  EXPECT_THROW (buffer.assign (11, 1), std::length_error);
  EXPECT_THROW ((circular_buffer<int, limited_alloc<int>> (11)),
                std::length_error);
  EXPECT_EQ (buffer.capacity (), 3U);
  EXPECT_EQ (buffer.front (), 1);
  buffer.set_capacity (10);
  EXPECT_EQ (buffer.capacity (), 10U);
}

TEST (CircularBufferApi, SetCapacity_ThrowingCopy_IsStrongAndDoesNotLeak)
{
  clean_slate slate;
  {
    fragile_buffer buffer (4, fragile_alloc (1));
    buffer.push_back (fragile (0));
    buffer.pop_front ();
    for (int v = 1; v <= 4; ++v)
      buffer.push_back (fragile (v)); // wrapped
    for (int allowed = 0; allowed < 3; ++allowed)
      {
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.set_capacity (8), std::runtime_error);
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.rset_capacity (3), std::runtime_error);
        fragile::reset ();
        EXPECT_EQ (values_of (buffer), (ints{ 1, 2, 3, 4 }));
        EXPECT_EQ (buffer.capacity (), 4U);
        EXPECT_EQ (ledger ().live_blocks (), 1U);
        EXPECT_EQ (fragile::state ().live, 4);
      }
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferApi, MoveOnlyElements_SupportTheLayoutOperations)
{
  circular_buffer<uptr> buffer (3);
  for (int v = 1; v <= 5; ++v)
    buffer.push_back (uptr (new int (v))); // wraps: holds 3, 4, 5
  buffer.set_capacity (5);
  ASSERT_EQ (buffer.size (), 3U);
  EXPECT_EQ (*buffer[0], 3);
  buffer.rset_capacity (2);
  EXPECT_EQ (*buffer[0], 4);
  EXPECT_EQ (*buffer[1], 5);
  buffer.set_capacity (4);
  buffer.insert (buffer.begin () + 1, uptr (new int (9)));
  buffer.push_front (uptr (new int (1)));
  EXPECT_EQ (buffer.size (), 4U);
  EXPECT_EQ (*buffer[0], 1);
  EXPECT_EQ (*buffer[1], 4);
  EXPECT_EQ (*buffer[2], 9);
  EXPECT_EQ (*buffer[3], 5);
  buffer.erase (buffer.begin () + 2);
  EXPECT_EQ (*buffer[2], 5);
  buffer.linearize ();
  EXPECT_TRUE (buffer.is_linearized ());
  buffer.erase_begin (1);
  EXPECT_EQ (*buffer.front (), 4);
}

// === insert ================================================================

TEST (CircularBufferApi, InsertSingle_NonFull_ShiftsTheTail)
{
  buffer_t buffer = make_with (6, 4, { 1, 2, 3 });
  buffer_t::iterator at = buffer.insert (buffer.begin () + 1, 9);
  EXPECT_EQ (at - buffer.begin (), 1);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 9, 2, 3 }));
  at = buffer.insert (buffer.begin (), 8);
  EXPECT_EQ (at - buffer.begin (), 0);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 8, 1, 9, 2, 3 }));
  at = buffer.insert (buffer.end (), 7);
  EXPECT_EQ (at - buffer.begin (), 5);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 8, 1, 9, 2, 3, 7 }));
  EXPECT_TRUE (buffer.full ());
}

TEST (CircularBufferApi, InsertSingle_Full_DropsTheOldestElement)
{
  buffer_t buffer = make_with (4, 2, { 1, 2, 3, 4 });
  buffer_t::iterator at = buffer.insert (buffer.begin () + 2, 9);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 2, 9, 3, 4 }));
  EXPECT_EQ (at - buffer.begin (), 1);
  EXPECT_EQ (*at, 9);

  at = buffer.insert (buffer.end (), 5);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 9, 3, 4, 5 }));
  EXPECT_EQ (at - buffer.begin (), 3);

  at = buffer.insert (buffer.begin () + 1, 6);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 6, 3, 4, 5 }));
  EXPECT_EQ (at - buffer.begin (), 0);
}

TEST (CircularBufferApi, InsertSingle_FullAtBegin_IsIgnored)
{
  buffer_t buffer = make_with (3, 1, { 1, 2, 3 });
  buffer_t::iterator at = buffer.insert (buffer.begin (), 9);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 3 }));
  EXPECT_TRUE (at == buffer.begin ());

  buffer_t none (0);
  at = none.insert (none.begin (), 5);
  EXPECT_TRUE (none.empty ());
  EXPECT_TRUE (at == none.begin ());
}

TEST (CircularBufferApi, InsertSingle_AcceptsConstIteratorsAndRvalues)
{
  circular_buffer<std::string> buffer (4);
  buffer.push_back ("a");
  buffer.push_back ("c");
  circular_buffer<std::string>::const_iterator const pos
      = buffer.cbegin () + 1;
  buffer.insert (pos, std::string ("b"));
  std::string const d = "d";
  buffer.insert (buffer.cend (), d);
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<std::string>{ "a", "b", "c", "d" }));
}

TEST (CircularBufferApi, Insert_AnElementOfTheBuffer_IsSafe)
{
  using poison_buffer = circular_buffer<cbtest::poison>;
  poison_buffer single (3);
  for (int v = 1; v <= 3; ++v)
    single.push_back (cbtest::poison (v));
  // full buffer: the insert drops the oldest element, which is the argument;
  // S = 1 2 1 3, the last three: 2 1 3
  single.insert (single.begin () + 2, single.front ());
  ASSERT_EQ (single.size (), 3U);
  EXPECT_EQ (single[0].value, 2);
  EXPECT_EQ (single[1].value, 1);
  EXPECT_EQ (single[2].value, 3);

  poison_buffer fill (3);
  for (int v = 1; v <= 3; ++v)
    fill.push_back (cbtest::poison (v));
  // S = 1 2 1 1 3 (two copies of the front before 3), the last three: 1 1 3
  fill.insert (fill.begin () + 2, 2, fill.front ());
  ASSERT_EQ (fill.size (), 3U);
  EXPECT_EQ (fill[0].value, 1);
  EXPECT_EQ (fill[1].value, 1);
  EXPECT_EQ (fill[2].value, 3);

  poison_buffer moved (3);
  for (int v = 1; v <= 3; ++v)
    moved.push_back (cbtest::poison (v));
  moved.insert (moved.begin () + 2, std::move (moved.front ()));
  ASSERT_EQ (moved.size (), 3U);
  EXPECT_EQ (moved[1].value, 1);

  circular_buffer<std::string> strings (3);
  strings.push_back (std::string (40, 'a'));
  strings.push_back (std::string (40, 'b'));
  strings.push_back (std::string (40, 'c'));
  strings.insert (strings.begin () + 2, strings.front ());
  EXPECT_EQ (strings[0], std::string (40, 'b'));
  EXPECT_EQ (strings[1], std::string (40, 'a'));
  EXPECT_EQ (strings[2], std::string (40, 'c'));
}

TEST (CircularBufferApi, InsertFill_KeepsTheLastCapacityElements)
{
  buffer_t buffer = make_with (3, 1, { 1, 2, 3 });
  buffer.insert (buffer.begin () + 1, 5, 9);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 9, 2, 3 }));

  buffer_t roomy = make_with (7, 5, { 1, 2, 3 });
  roomy.insert (roomy.begin () + 1, 2, 9);
  EXPECT_EQ (cbtest::content_of (roomy), (ints{ 1, 9, 9, 2, 3 }));
  roomy.insert (roomy.begin () + 3, 4, 8); // 4 more: the oldest 2 drop
  EXPECT_EQ (cbtest::content_of (roomy), (ints{ 9, 8, 8, 8, 8, 2, 3 }));

  buffer_t none = make_with (3, 0, { 1, 2 });
  none.insert (none.begin (), 0, 5);
  EXPECT_EQ (cbtest::content_of (none), (ints{ 1, 2 }));
}

TEST (CircularBufferApi, InsertRange_ForwardAndInputIteratorsAgree)
{
  ints const items = { 10, 11, 12, 13 };
  std::list<int> const list (items.begin (), items.end ());
  buffer_t from_vector = make_with (5, 3, { 1, 2, 3 });
  buffer_t from_list = make_with (5, 3, { 1, 2, 3 });
  buffer_t from_input = make_with (5, 3, { 1, 2, 3 });
  from_vector.insert (from_vector.begin () + 1, items.begin (), items.end ());
  from_list.insert (from_list.begin () + 1, list.begin (), list.end ());
  std::istringstream stream ("10 11 12 13");
  from_input.insert (from_input.begin () + 1,
                     std::istream_iterator<int> (stream),
                     std::istream_iterator<int> ());
  // S = 1 10 11 12 13 2 3, the last 5: 11 12 13 2 3
  EXPECT_EQ (cbtest::content_of (from_vector), (ints{ 11, 12, 13, 2, 3 }));
  EXPECT_EQ (cbtest::content_of (from_list), (ints{ 11, 12, 13, 2, 3 }));
  EXPECT_EQ (cbtest::content_of (from_input), (ints{ 11, 12, 13, 2, 3 }));

  buffer_t at_end = make_with (6, 2, { 1, 2 });
  at_end.insert (at_end.end (), items.begin (), items.end ());
  EXPECT_EQ (cbtest::content_of (at_end), (ints{ 1, 2, 10, 11, 12, 13 }));

  buffer_t overflow_at_end = make_with (3, 2, { 1, 2, 3 });
  overflow_at_end.insert (overflow_at_end.end (), items.begin (),
                          items.end ());
  EXPECT_EQ (cbtest::content_of (overflow_at_end), (ints{ 11, 12, 13 }));

  buffer_t at_begin_full = make_with (3, 2, { 1, 2, 3 });
  at_begin_full.insert (at_begin_full.begin (), items.begin (), items.end ());
  EXPECT_EQ (cbtest::content_of (at_begin_full), (ints{ 1, 2, 3 })); // ignored

  buffer_t empty_range = make_with (3, 2, { 1, 2 });
  empty_range.insert (empty_range.begin () + 1, items.begin (),
                      items.begin ());
  EXPECT_EQ (cbtest::content_of (empty_range), (ints{ 1, 2 }));
}

TEST (CircularBufferApi, InsertInitializerList)
{
  buffer_t buffer = make_with (5, 4, { 1, 2 });
  buffer.insert (buffer.begin () + 1, { 7, 8 });
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 7, 8, 2 }));
  buffer.insert (buffer.begin () + 2, { 5, 6 });
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 7, 5, 6, 8, 2 }));
}

TEST (CircularBufferApi,
      Insert_EveryCapacityHeadSizeAndPosition_MatchesTheModel)
{
  for (std::size_t capacity = 1; capacity <= 5; ++capacity)
    for (std::size_t head = 0; head < capacity; ++head)
      for (std::size_t size = 0; size <= capacity; ++size)
        for (std::size_t pos = 0; pos <= size; ++pos)
          for (std::size_t count = 0; count <= capacity + 2; ++count)
            {
              SCOPED_TRACE (testing::Message ()
                            << "capacity " << capacity << " head " << head
                            << " size " << size << " pos " << pos << " count "
                            << count);
              ints const content = iota_from (1, size);
              ints const items = iota_from (100, count);
              ints const expected
                  = model_insert (content, capacity, pos, items);

              buffer_t fill = make_with (capacity, head, content);
              fill.insert (fill.begin () + static_cast<std::ptrdiff_t> (pos),
                           count, 100);
              ints expected_fill = content;
              expected_fill.insert (expected_fill.begin ()
                                        + static_cast<std::ptrdiff_t> (pos),
                                    count, 100);
              if (expected_fill.size () > capacity)
                expected_fill.erase (
                    expected_fill.begin (),
                    expected_fill.begin ()
                        + static_cast<std::ptrdiff_t> (expected_fill.size ()
                                                       - capacity));
              ASSERT_EQ (cbtest::content_of (fill), expected_fill);

              buffer_t range = make_with (capacity, head, content);
              range.insert (range.begin () + static_cast<std::ptrdiff_t> (pos),
                            items.begin (), items.end ());
              ASSERT_EQ (cbtest::content_of (range), expected);
              ASSERT_EQ (range.capacity (), capacity);

              std::list<int> const list (items.begin (), items.end ());
              buffer_t from_list = make_with (capacity, head, content);
              from_list.insert (from_list.begin ()
                                    + static_cast<std::ptrdiff_t> (pos),
                                list.begin (), list.end ());
              ASSERT_EQ (cbtest::content_of (from_list), expected);

              if (count == 1)
                {
                  buffer_t single = make_with (capacity, head, content);
                  buffer_t::iterator at = single.insert (
                      single.begin () + static_cast<std::ptrdiff_t> (pos),
                      100);
                  ASSERT_EQ (cbtest::content_of (single), expected);
                  std::size_t const dropped
                      = size + 1 > capacity ? size + 1 - capacity : 0;
                  std::ptrdiff_t const index
                      = (size == capacity && pos == 0)
                            ? 0
                            : static_cast<std::ptrdiff_t> (pos)
                                  - static_cast<std::ptrdiff_t> (dropped);
                  ASSERT_EQ (at - single.begin (), index);
                }
            }
}

TEST (CircularBufferApi, Insert_ThrowingCopy_RemovesTheAddedElements)
{
  clean_slate slate;
  {
    fragile_buffer buffer (7, fragile_alloc (1));
    for (int v = 1; v <= 3; ++v)
      buffer.push_back (fragile (v));
    fragile const item (9);
    std::vector<fragile> const items (3, fragile (5));
    for (int allowed = 0; allowed < 3; ++allowed)
      {
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.insert (buffer.begin () + 1, 3, item),
                      std::runtime_error);
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (
            buffer.insert (buffer.begin () + 1, items.begin (), items.end ()),
            std::runtime_error);
        fragile::reset ();
        EXPECT_EQ (values_of (buffer), (ints{ 1, 2, 3 }));
      }
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

// === erase =================================================================

TEST (CircularBufferApi, EraseSingle_ReturnsTheFollowingElement)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  buffer_t::iterator next = buffer.erase (buffer.begin () + 1);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 3, 4, 5 }));
  EXPECT_EQ (*next, 3);
  EXPECT_EQ (next - buffer.begin (), 1);
  next = buffer.erase (buffer.begin ());
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5 }));
  EXPECT_EQ (*next, 3);
  next = buffer.erase (buffer.end () - 1);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4 }));
  EXPECT_TRUE (next == buffer.end ());
  EXPECT_EQ (buffer.capacity (), 5U); // erase does not change the capacity
  buffer_t::const_iterator const pos = buffer.cbegin ();
  buffer.erase (pos);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 4 }));
}

TEST (CircularBufferApi, EraseRange_ReturnsTheFollowingElement)
{
  buffer_t buffer = make_with (6, 4, { 1, 2, 3, 4, 5, 6 });
  buffer_t::iterator next
      = buffer.erase (buffer.begin () + 1, buffer.begin () + 3);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 4, 5, 6 }));
  EXPECT_EQ (*next, 4);
  EXPECT_EQ (next - buffer.begin (), 1);

  next = buffer.erase (buffer.begin () + 2, buffer.begin () + 2);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 4, 5, 6 }));
  EXPECT_EQ (next - buffer.begin (), 2);

  next = buffer.erase (buffer.begin () + 2, buffer.end ());
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 4 }));
  EXPECT_TRUE (next == buffer.end ());

  next = buffer.erase (buffer.begin (), buffer.end ());
  EXPECT_TRUE (buffer.empty ());
  EXPECT_TRUE (next == buffer.end ());
  buffer.push_back (3);
  EXPECT_EQ (buffer.front (), 3);
}

TEST (CircularBufferApi, EraseBeginAndEnd_RemoveFromTheRightEnd)
{
  buffer_t buffer = make_with (6, 4, { 1, 2, 3, 4, 5, 6 });
  buffer.erase_begin (2);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5, 6 }));
  buffer.erase_end (1);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5 }));
  buffer.erase_begin (0);
  buffer.erase_end (0);
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 3, 4, 5 }));
  buffer.erase_begin (10); // more than the size: everything goes
  EXPECT_TRUE (buffer.empty ());
  buffer.push_back (1);
  buffer.push_back (2);
  buffer.erase_end (10);
  EXPECT_TRUE (buffer.empty ());
  EXPECT_EQ (buffer.capacity (), 6U);
}

TEST (CircularBufferApi, Erase_EveryCapacityHeadSizeAndRange_MatchesTheModel)
{
  for (std::size_t capacity = 1; capacity <= 5; ++capacity)
    for (std::size_t head = 0; head < capacity; ++head)
      for (std::size_t size = 0; size <= capacity; ++size)
        for (std::size_t from = 0; from <= size; ++from)
          for (std::size_t to = from; to <= size; ++to)
            {
              SCOPED_TRACE (testing::Message ()
                            << "capacity " << capacity << " head " << head
                            << " size " << size << " from " << from << " to "
                            << to);
              ints const content = iota_from (1, size);
              ints expected = content;
              expected.erase (
                  expected.begin () + static_cast<std::ptrdiff_t> (from),
                  expected.begin () + static_cast<std::ptrdiff_t> (to));
              buffer_t range = make_with (capacity, head, content);
              buffer_t::iterator next = range.erase (
                  range.begin () + static_cast<std::ptrdiff_t> (from),
                  range.begin () + static_cast<std::ptrdiff_t> (to));
              ASSERT_EQ (cbtest::content_of (range), expected);
              ASSERT_EQ (next - range.begin (),
                         static_cast<std::ptrdiff_t> (from));
              ASSERT_EQ (range.capacity (), capacity);
              // the buffer keeps working after the erase
              range.push_back (500);
              ASSERT_EQ (range.back (), 500);

              if (to == from + 1)
                {
                  buffer_t single = make_with (capacity, head, content);
                  buffer_t::iterator following = single.erase (
                      single.begin () + static_cast<std::ptrdiff_t> (from));
                  ASSERT_EQ (cbtest::content_of (single), expected);
                  ASSERT_EQ (following - single.begin (),
                             static_cast<std::ptrdiff_t> (from));
                }
              buffer_t front_end = make_with (capacity, head, content);
              front_end.erase_begin (from);
              ASSERT_EQ (
                  cbtest::content_of (front_end),
                  ints (content.begin () + static_cast<std::ptrdiff_t> (from),
                        content.end ()));
              buffer_t back_end = make_with (capacity, head, content);
              back_end.erase_end (size - to);
              ASSERT_EQ (
                  cbtest::content_of (back_end),
                  ints (content.begin (),
                        content.begin () + static_cast<std::ptrdiff_t> (to)));
            }
}

// === linearize, is_linearized, max_size ====================================

TEST (CircularBufferApi, IsLinearized_EveryHeadAndSize)
{
  for (std::size_t capacity = 0; capacity <= 6; ++capacity)
    for (std::size_t head = 0; head < (capacity == 0 ? 1 : capacity); ++head)
      for (std::size_t size = 0; size <= capacity; ++size)
        {
          SCOPED_TRACE (testing::Message ()
                        << "capacity " << capacity << " head " << head
                        << " size " << size);
          buffer_t buffer = make_with (capacity, head, iota_from (1, size));
          EXPECT_EQ (buffer.is_linearized (), head + size <= capacity);
        }
}

TEST (CircularBufferApi, Linearize_WrappedBuffer_MakesTheContentContiguous)
{
  buffer_t buffer = make_with (5, 3, { 1, 2, 3, 4, 5 });
  ASSERT_FALSE (buffer.is_linearized ());
  int *const data = buffer.linearize ();
  ASSERT_NE (data, nullptr);
  EXPECT_TRUE (buffer.is_linearized ());
  for (std::size_t i = 0; i < 5; ++i)
    {
      EXPECT_EQ (data[i], static_cast<int> (i) + 1);
      EXPECT_EQ (&buffer[i], data + i);
    }
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 1, 2, 3, 4, 5 }));
  buffer.push_back (6); // still a ring afterwards
  EXPECT_EQ (cbtest::content_of (buffer), (ints{ 2, 3, 4, 5, 6 }));

  buffer_t partial = make_with (6, 4, { 1, 2, 3, 4 });
  ASSERT_FALSE (partial.is_linearized ());
  int *const partial_data = partial.linearize ();
  EXPECT_EQ (partial.capacity (), 6U);
  for (std::size_t i = 0; i < 4; ++i)
    EXPECT_EQ (partial_data[i], static_cast<int> (i) + 1);
}

TEST (CircularBufferApi,
      Linearize_AlreadyLinear_ReturnsThePointerWithoutMoving)
{
  buffer_t buffer = make_with (6, 2, { 1, 2, 3 });
  ASSERT_TRUE (buffer.is_linearized ());
  int *const first = &buffer[0];
  EXPECT_EQ (buffer.linearize (), first);
  EXPECT_EQ (&buffer[0], first);

  buffer_t empty (4);
  EXPECT_EQ (empty.linearize (), nullptr);
  buffer_t none;
  EXPECT_EQ (none.linearize (), nullptr);
  EXPECT_TRUE (none.is_linearized ());
}

TEST (CircularBufferApi, Linearize_ThrowingCopy_IsStrongAndDoesNotLeak)
{
  clean_slate slate;
  {
    fragile_buffer buffer (4, fragile_alloc (1));
    buffer.push_back (fragile (0));
    buffer.push_back (fragile (0));
    buffer.pop_front ();
    buffer.pop_front ();
    for (int v = 1; v <= 4; ++v)
      buffer.push_back (fragile (v));
    buffer.pop_front ();
    buffer.push_back (fragile (5)); // wrapped
    ASSERT_FALSE (buffer.is_linearized ());
    for (int allowed = 0; allowed < 4; ++allowed)
      {
        fragile::state ().copies_before_throw = allowed;
        EXPECT_THROW (buffer.linearize (), std::runtime_error);
        fragile::reset ();
        EXPECT_EQ (values_of (buffer), (ints{ 2, 3, 4, 5 }));
        EXPECT_EQ (ledger ().live_blocks (), 1U);
        EXPECT_EQ (fragile::state ().live, 4);
      }
    buffer.linearize ();
    EXPECT_EQ (values_of (buffer), (ints{ 2, 3, 4, 5 }));
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferApi, MaxSize_IsLimitedByTheAllocatorAndTheDifferenceType)
{
  circular_buffer<int, limited_alloc<int>> limited;
  EXPECT_EQ (limited.max_size (), 10U);

  buffer_t buffer;
  EXPECT_GE (buffer.max_size (), 1000000U);
  EXPECT_LE (
      buffer.max_size (),
      static_cast<std::size_t> (std::numeric_limits<std::ptrdiff_t>::max ()));
  circular_buffer<char> chars;
  EXPECT_EQ (
      chars.max_size (),
      static_cast<std::size_t> (std::numeric_limits<std::ptrdiff_t>::max ()));
  EXPECT_THROW (buffer_t (buffer.max_size () + 1), std::length_error);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

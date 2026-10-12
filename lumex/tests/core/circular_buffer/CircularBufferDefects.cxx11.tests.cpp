// Regression tests of the memory defects found by the sanitizers: capacity 0
// and moved-from buffers, aliasing pushes into a full buffer, leaks when a
// copy throws, and copy assignment between unequal allocators.
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#pragma clang diagnostic ignored "-Wself-move"
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif

using cbtest::fragile;
using cbtest::ledger;
using cbtest::pinned;
using lumex::core::circular_buffer::circular_buffer;

namespace
{
/// Clears the ledger and the limits of `fragile` at the start of a test.
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

std::string
long_text (char letter)
{
  return std::string (80, letter);
}

std::vector<int>
values_of (circular_buffer<fragile, fragile_alloc> const &buffer)
{
  std::vector<int> out;
  for (std::size_t i = 0; i < buffer.size (); ++i)
    out.push_back (buffer[i].value);
  return out;
}

fragile_buffer
make_fragile_buffer (std::size_t capacity, std::size_t count, int first,
                     int id = 0)
{
  fragile_buffer buffer (capacity, fragile_alloc (id));
  for (std::size_t i = 0; i < count; ++i)
    buffer.push_back (fragile (first + static_cast<int> (i)));
  return buffer;
}
} // namespace

// === capacity 0 ============================================================

TEST (CircularBufferDefects, ZeroCapacity_IsValidEmptyAndFull)
{
  clean_slate slate;
  circular_buffer<int, cbtest::plain_alloc> from_ctor (0);
  circular_buffer<int, cbtest::plain_alloc> from_default;
  for (circular_buffer<int, cbtest::plain_alloc> *buffer :
       { &from_ctor, &from_default })
    {
      EXPECT_EQ (buffer->capacity (), 0U);
      EXPECT_EQ (buffer->size (), 0U);
      EXPECT_TRUE (buffer->empty ());
      EXPECT_TRUE (buffer->full ());
      EXPECT_TRUE (buffer->begin () == buffer->end ());
      EXPECT_THROW (buffer->at (0), std::out_of_range);
      EXPECT_THROW (buffer->front_safe (), std::out_of_range);
      EXPECT_THROW (buffer->back_safe (), std::out_of_range);
      buffer->pop_front ();
      buffer->pop_back ();
      buffer->clear ();
      circular_buffer<int, cbtest::plain_alloc> copy (*buffer);
      EXPECT_EQ (copy.capacity (), 0U);
      EXPECT_TRUE (copy == *buffer);
    }
  EXPECT_EQ (ledger ().allocations, 0U); // nothing is allocated for capacity 0
}

TEST (CircularBufferDefects, ZeroCapacity_PushIsNoOp)
{
  clean_slate slate;
  circular_buffer<std::string> strings (0);
  std::string const lvalue = long_text ('a');
  strings.push_back (lvalue);
  strings.push_back (long_text ('b'));
  strings.push_front (lvalue);
  strings.push_front (long_text ('c'));
  EXPECT_TRUE (strings.empty ());
  EXPECT_EQ (strings.capacity (), 0U);

  circular_buffer<int> ints;
  ints.push_back (1);
  ints.push_front (2);
  EXPECT_TRUE (ints.empty ());

  circular_buffer<fragile> objects (0);
  objects.push_back (fragile (1));
  objects.push_front (fragile (2));
  EXPECT_TRUE (objects.empty ());
  EXPECT_EQ (fragile::state ().live, 0);
}

TEST (CircularBufferDefects, ZeroCapacity_EmplaceThrowsLengthError)
{
  circular_buffer<std::string> buffer (0);
  EXPECT_THROW (buffer.emplace_back ("x"), std::length_error);
  EXPECT_THROW (buffer.emplace_front ("x"), std::length_error);
  EXPECT_TRUE (buffer.empty ());
}

TEST (CircularBufferDefects, MovedFrom_ConstructorLeavesValidReusableBuffer)
{
  clean_slate slate;
  {
    circular_buffer<std::string> source (3);
    source.push_back (long_text ('a'));
    source.push_back (long_text ('b'));
    circular_buffer<std::string> target (std::move (source));
    EXPECT_EQ (target.size (), 2U);
    EXPECT_EQ (target.front (), long_text ('a'));

    EXPECT_EQ (source.capacity (), 0U);
    EXPECT_EQ (source.size (), 0U);
    EXPECT_TRUE (source.empty ());
    EXPECT_TRUE (source.full ());
    source.push_back (long_text ('c')); // no-op, no crash
    source.push_front (long_text ('d'));
    EXPECT_TRUE (source.empty ());
    EXPECT_THROW (source.emplace_back ("x"), std::length_error);

    source = circular_buffer<std::string> (2); // reassignable
    source.push_back (long_text ('e'));
    source.push_back (long_text ('f'));
    source.push_back (long_text ('g'));
    EXPECT_EQ (source.size (), 2U);
    EXPECT_EQ (source.front (), long_text ('f'));
  }
}

TEST (CircularBufferDefects, MovedFrom_AssignmentLeavesValidReusableBuffer)
{
  clean_slate slate;
  circular_buffer<std::string> source (2);
  source.push_back (long_text ('a'));
  circular_buffer<std::string> target (4);
  target.push_back (long_text ('t'));
  target = std::move (source);
  EXPECT_EQ (target.capacity (), 2U);
  EXPECT_EQ (target.front (), long_text ('a'));

  EXPECT_EQ (source.capacity (), 0U);
  EXPECT_TRUE (source.empty ());
  source.push_back (long_text ('x')); // no-op
  EXPECT_TRUE (source.empty ());
  circular_buffer<std::string> other (1);
  source.swap (other); // a moved-from buffer can be swapped
  EXPECT_EQ (source.capacity (), 1U);
  EXPECT_EQ (other.capacity (), 0U);
  other = target;
  EXPECT_EQ (other.size (), 1U);
}

// === aliasing ==============================================================

// A push of an element of the buffer itself into a full buffer must copy the
// value before the overwritten element is destroyed.
TEST (CircularBufferDefects, Aliasing_PushBackOfAnElement_FullBuffer)
{
  clean_slate slate;
  for (std::size_t capacity = 1; capacity <= 3; ++capacity)
    {
      SCOPED_TRACE (testing::Message () << "capacity " << capacity);
      circular_buffer<std::string> buffer (capacity);
      for (std::size_t i = 0; i < capacity; ++i)
        buffer.push_back (long_text (static_cast<char> ('a' + i)));
      ASSERT_TRUE (buffer.full ());

      std::string const oldest = buffer.front ();
      buffer.push_back (buffer.front ()); // the element that is destroyed
      EXPECT_EQ (buffer.back (), oldest);
      EXPECT_EQ (buffer.size (), capacity);

      std::string const newest = buffer.back ();
      buffer.push_back (buffer.back ());
      EXPECT_EQ (buffer.back (), newest);
      EXPECT_EQ (buffer.size (), capacity);

      std::string const front_before = buffer.front ();
      buffer.push_back (std::move (buffer.front ()));
      EXPECT_EQ (buffer.back (), front_before);
    }
}

TEST (CircularBufferDefects, Aliasing_PushFrontOfAnElement_FullBuffer)
{
  clean_slate slate;
  for (std::size_t capacity = 1; capacity <= 3; ++capacity)
    {
      SCOPED_TRACE (testing::Message () << "capacity " << capacity);
      circular_buffer<std::string> buffer (capacity);
      for (std::size_t i = 0; i < capacity; ++i)
        buffer.push_back (long_text (static_cast<char> ('a' + i)));

      std::string const newest = buffer.back ();
      buffer.push_front (buffer.back ()); // the element that is destroyed
      EXPECT_EQ (buffer.front (), newest);
      EXPECT_EQ (buffer.size (), capacity);

      std::string const oldest = buffer.front ();
      buffer.push_front (buffer.front ());
      EXPECT_EQ (buffer.front (), oldest);

      std::string const back_before = buffer.back ();
      buffer.push_front (std::move (buffer.back ()));
      EXPECT_EQ (buffer.front (), back_before);
    }
}

// The same with an element whose destructor poisons the value: a copy made
// from the destroyed element would show -999.
TEST (CircularBufferDefects,
      Aliasing_PoisonedDestructor_PushesCopyBeforeDestroying)
{
  using poison_buffer = circular_buffer<cbtest::poison>;
  poison_buffer buffer (2);
  buffer.push_back (cbtest::poison (1));
  buffer.push_back (cbtest::poison (2));
  buffer.push_back (buffer.front ()); // 1 is destroyed and copied
  EXPECT_EQ (buffer[0].value, 2);
  EXPECT_EQ (buffer[1].value, 1);
  buffer.push_front (buffer.back ());
  EXPECT_EQ (buffer[0].value, 1);
  EXPECT_EQ (buffer[1].value, 2);
  buffer.emplace_back (buffer.front ());
  EXPECT_EQ (buffer[0].value, 2);
  EXPECT_EQ (buffer[1].value, 1);
  buffer.emplace_front (buffer.back ());
  EXPECT_EQ (buffer[0].value, 1);
  EXPECT_EQ (buffer[1].value, 2);
}

TEST (CircularBufferDefects,
      Aliasing_EmplaceWithArgumentsOfTheOverwrittenElement)
{
  clean_slate slate;
  circular_buffer<std::string> buffer (2);
  buffer.push_back (long_text ('a'));
  buffer.push_back (long_text ('b'));

  // The arguments are a pointer into the heap block of the element that
  // emplace_back destroys, and a reference to the element that emplace_front
  // destroys.
  char const *const text = buffer.front ().c_str ();
  std::string const expected_back = buffer.front ();
  buffer.emplace_back (text);
  EXPECT_EQ (buffer.back (), expected_back);

  std::string const expected_front = buffer.back ();
  buffer.emplace_front (buffer.back ().c_str ());
  EXPECT_EQ (buffer.front (), expected_front);

  // the reference form of the argument
  std::string const expected_again = buffer.front ();
  buffer.emplace_back (buffer.front (), 0U, 10U);
  EXPECT_EQ (buffer.back (), expected_again.substr (0, 10));
}

// The new element is built first: when its construction throws, a full buffer
// is unchanged (nothing was destroyed).
TEST (CircularBufferDefects,
      Overwrite_ThrowingConstruction_LeavesBufferUnchanged)
{
  clean_slate slate;
  {
    fragile_buffer buffer = make_fragile_buffer (3, 3, 1);
    ASSERT_TRUE (buffer.full ());
    int const live_before = fragile::state ().live;

    fragile::state ().constructions_before_throw = 0;
    EXPECT_THROW (buffer.emplace_back (9), std::runtime_error);
    fragile::reset ();
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_EQ (fragile::state ().live, live_before);

    fragile::state ().constructions_before_throw = 0;
    EXPECT_THROW (buffer.emplace_front (9), std::runtime_error);
    fragile::reset ();
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 1, 2, 3 }));

    fragile const extra (7);
    fragile::state ().copies_before_throw = 0;
    EXPECT_THROW (buffer.push_back (extra), std::runtime_error);
    EXPECT_THROW (buffer.push_front (extra), std::runtime_error);
    fragile::reset ();
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 1, 2, 3 }));
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

// When the move into the freed slot throws, the buffer is consistent (one
// element fewer) and nothing leaks.
TEST (CircularBufferDefects, Overwrite_ThrowingMove_LeavesConsistentBuffer)
{
  clean_slate slate;
  {
    fragile_buffer buffer = make_fragile_buffer (3, 3, 1);
    fragile const extra (7);
    fragile::state ().copies_before_throw = 1; // the copy to the temporary
                                               // passes, the move throws
    EXPECT_THROW (buffer.push_back (extra), std::runtime_error);
    fragile::reset ();
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 2, 3 }));

    buffer.push_back (fragile (4)); // the buffer works on
    buffer.push_back (fragile (5));
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 3, 4, 5 }));

    fragile::state ().copies_before_throw = 1;
    EXPECT_THROW (buffer.push_front (extra), std::runtime_error);
    fragile::reset ();
    EXPECT_EQ (values_of (buffer), (std::vector<int>{ 3, 4 }));
  }
  EXPECT_EQ (fragile::state ().live, 0);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

// A type that cannot be moved is overwritten by destroying the oldest element
// and constructing the new one in place.
TEST (CircularBufferDefects, Overwrite_NonMovableType_ConstructsInPlace)
{
  static_assert (!std::is_move_constructible<pinned>::value,
                 "pinned must not be move-constructible");
  circular_buffer<pinned> buffer (2);
  for (int i = 1; i <= 5; ++i)
    buffer.emplace_back (i);
  EXPECT_EQ (buffer[0].value, 4);
  EXPECT_EQ (buffer[1].value, 5);
  for (int i = 6; i <= 8; ++i)
    buffer.emplace_front (i);
  EXPECT_EQ (buffer[0].value, 8);
  EXPECT_EQ (buffer[1].value, 7);
  pinned const copy (9);
  buffer.push_back (copy);
  EXPECT_EQ (buffer[1].value, 9);
}

// === leaks when a copy throws ==============================================

TEST (CircularBufferDefects, CopyConstructor_ThrowingCopy_DoesNotLeak)
{
  clean_slate slate;
  std::size_t const count = 5;
  for (int allowed = 0; allowed < static_cast<int> (count); ++allowed)
    {
      SCOPED_TRACE (testing::Message () << "copies allowed " << allowed);
      fragile_buffer source = make_fragile_buffer (6, count, 1);
      source.pop_front (); // wrap: the content is not at the start
      source.push_back (fragile (100));
      source.push_back (fragile (101));
      source.pop_front ();
      std::size_t const blocks = ledger ().live_blocks ();
      int const live = fragile::state ().live;

      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (fragile_buffer copy (source), std::runtime_error);
      fragile::reset ();

      EXPECT_EQ (ledger ().live_blocks (), blocks); // the block was released
      EXPECT_EQ (fragile::state ().live, live);     // no element survived
      EXPECT_EQ (ledger ().wrong_owner, 0U);
    }
}

TEST (CircularBufferDefects, CopyAssignment_ThrowingCopy_LeavesTargetUnchanged)
{
  clean_slate slate;
  std::size_t const count = 4;
  for (int allowed = 0; allowed < static_cast<int> (count); ++allowed)
    {
      SCOPED_TRACE (testing::Message () << "copies allowed " << allowed);
      fragile_buffer source = make_fragile_buffer (5, count, 50);
      fragile_buffer target = make_fragile_buffer (3, 3, 1);
      std::size_t const blocks = ledger ().live_blocks ();
      int const live = fragile::state ().live;

      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (target = source, std::runtime_error);
      fragile::reset ();

      EXPECT_EQ (values_of (target), (std::vector<int>{ 1, 2, 3 }));
      EXPECT_EQ (target.capacity (), 3U);
      EXPECT_EQ (ledger ().live_blocks (), blocks);
      EXPECT_EQ (fragile::state ().live, live);
    }
}

TEST (CircularBufferDefects, CopyAssignment_SelfAssignment_KeepsContent)
{
  clean_slate slate;
  fragile_buffer buffer = make_fragile_buffer (4, 3, 1);
  fragile_buffer &alias = buffer;
  buffer = alias;
  EXPECT_EQ (values_of (buffer), (std::vector<int>{ 1, 2, 3 }));
}

TEST (CircularBufferDefects, RangeAndFillConstructors_ThrowingCopy_DoNotLeak)
{
  clean_slate slate;
  std::vector<fragile> const source
      = { fragile (1), fragile (2), fragile (3), fragile (4) };
  fragile const item (9);
  int const live = fragile::state ().live;
  for (int allowed = 0; allowed < 4; ++allowed)
    {
      SCOPED_TRACE (testing::Message () << "copies allowed " << allowed);
      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (fragile_buffer (source.begin (), source.end ()),
                    std::runtime_error);
      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (fragile_buffer (4, source.begin (), source.end ()),
                    std::runtime_error);
      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (fragile_buffer (4, item), std::runtime_error);
      fragile::state ().copies_before_throw = allowed;
      EXPECT_THROW (fragile_buffer (5, 4, item), std::runtime_error);
      fragile::reset ();
      EXPECT_EQ (ledger ().live_blocks (), 0U);
      EXPECT_EQ (fragile::state ().live, live);
    }
}

// === allocators ============================================================

TEST (CircularBufferDefects, CopyAssignment_NonPropagating_KeepsOwnAllocator)
{
  clean_slate slate;
  {
    circular_buffer<int, cbtest::plain_alloc> target (3,
                                                      cbtest::plain_alloc (1));
    target.push_back (1);
    circular_buffer<int, cbtest::plain_alloc> source (5,
                                                      cbtest::plain_alloc (2));
    source.push_back (10);
    source.push_back (20);
    source.push_back (30);
    ASSERT_FALSE (target.get_allocator () == source.get_allocator ());

    target = source;
    EXPECT_EQ (target.get_allocator ().id, 1); // the own allocator is kept
    EXPECT_EQ (source.get_allocator ().id, 2);
    EXPECT_EQ (target.capacity (), 5U);
    EXPECT_EQ (cbtest::content_of (target), (std::vector<int>{ 10, 20, 30 }));
    EXPECT_EQ (ledger ().wrong_owner, 0U);
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U); // nothing was freed by the wrong one
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferDefects, CopyAssignment_Propagating_TakesAllocatorOfSource)
{
  clean_slate slate;
  using buffer_t = circular_buffer<int, cbtest::propagating_alloc>;
  {
    buffer_t target (3, cbtest::propagating_alloc (1));
    target.push_back (1);
    buffer_t source (5, cbtest::propagating_alloc (2));
    source.push_back (10);
    source.push_back (20);

    target = source;
    EXPECT_EQ (target.get_allocator ().id, 2); // the allocator of the source
    EXPECT_EQ (target.capacity (), 5U);
    EXPECT_EQ (cbtest::content_of (target), (std::vector<int>{ 10, 20 }));
    // the old block (made by allocator 1) was released with allocator 1
    EXPECT_EQ (ledger ().wrong_owner, 0U);
    EXPECT_EQ (ledger ().live_blocks (), 2U);
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferDefects,
      CopyConstructor_UsesSelectOnContainerCopyConstruction)
{
  clean_slate slate;
  {
    circular_buffer<int, cbtest::plain_alloc> source (3,
                                                      cbtest::plain_alloc (5));
    source.push_back (1);
    circular_buffer<int, cbtest::plain_alloc> copy (source);
    EXPECT_EQ (copy.get_allocator ().id, 1005);
    EXPECT_EQ (source.get_allocator ().id, 5);
    EXPECT_EQ (cbtest::content_of (copy), (std::vector<int>{ 1 }));
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferDefects, MoveOperations_CarryTheAllocatorWithTheStorage)
{
  clean_slate slate;
  {
    circular_buffer<int, cbtest::plain_alloc> source (3,
                                                      cbtest::plain_alloc (5));
    source.push_back (1);
    circular_buffer<int, cbtest::plain_alloc> moved (std::move (source));
    EXPECT_EQ (moved.get_allocator ().id, 5);
    circular_buffer<int, cbtest::plain_alloc> target (2,
                                                      cbtest::plain_alloc (7));
    target.push_back (9);
    target = std::move (moved);
    EXPECT_EQ (target.get_allocator ().id, 5);
    EXPECT_EQ (cbtest::content_of (target), (std::vector<int>{ 1 }));
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

TEST (CircularBufferDefects, Swap_Propagating_ExchangesAllocatorsAndStorage)
{
  clean_slate slate;
  using buffer_t = circular_buffer<int, cbtest::propagating_alloc>;
  {
    buffer_t a (3, cbtest::propagating_alloc (1));
    a.push_back (1);
    buffer_t b (2, cbtest::propagating_alloc (2));
    b.push_back (2);
    b.push_back (3);
    a.swap (b);
    EXPECT_EQ (a.get_allocator ().id, 2);
    EXPECT_EQ (b.get_allocator ().id, 1);
    EXPECT_EQ (cbtest::content_of (a), (std::vector<int>{ 2, 3 }));
    EXPECT_EQ (cbtest::content_of (b), (std::vector<int>{ 1 }));
  }
  EXPECT_EQ (ledger ().wrong_owner, 0U);
  EXPECT_EQ (ledger ().live_blocks (), 0U);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

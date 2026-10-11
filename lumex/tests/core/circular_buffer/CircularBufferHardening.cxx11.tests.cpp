// Gaps found after the first review: aliasing for a copyable type without a
// move constructor, the strong guarantee of reallocation for a type whose move
// can throw, aliasing with the element in the last physical slot, an input
// iterator insert into a full buffer at begin (), and allocator destroy ()
// calls for trivially destructible elements.
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>
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
#pragma clang diagnostic ignored "-Wpadded"
#endif

using lumex::core::circular_buffer::circular_buffer;

namespace
{
using ints = std::vector<int>;

/// Copyable, move constructor deleted, destructor poisons the value.
struct copy_only_poison
{
  int value;

  explicit copy_only_poison (int v = 0) : value (v) {}
  copy_only_poison (copy_only_poison const &other) : value (other.value) {}
  copy_only_poison (copy_only_poison &&) = delete;
  copy_only_poison &
  operator= (copy_only_poison const &other)
  {
    value = other.value;
    return *this;
  }
  ~copy_only_poison () { static_cast<volatile int &> (value) = -999; }
};

/// Copy never throws; the move constructor throws once `moves_left` is 0
/// (-1: never). The moved-from value is -1.
struct move_throws
{
  static int &
  moves_left ()
  {
    static int value = -1;
    return value;
  }

  int value;

  explicit move_throws (int v = 0) : value (v) {}
  move_throws (move_throws const &other) : value (other.value) {}
  move_throws (move_throws &&other) noexcept (false) : value (other.value)
  {
    if (moves_left () == 0)
      throw std::runtime_error ("move_throws: move");
    if (moves_left () > 0)
      --moves_left ();
    other.value = -1;
  }
  move_throws &
  operator= (move_throws const &other)
  {
    value = other.value;
    return *this;
  }
  move_throws &
  operator= (move_throws &&other) noexcept (false)
  {
    value = other.value;
    other.value = -1;
    return *this;
  }
};

/// Counts construct () and destroy () calls of the element allocator.
struct alloc_counts
{
  std::size_t constructed;
  std::size_t destroyed;
};

alloc_counts &
counts ()
{
  static alloc_counts instance = { 0, 0 };
  return instance;
}

template <class T> struct counting_alloc
{
  using value_type = T;
  counting_alloc () = default;
  template <class U> counting_alloc (counting_alloc<U> const &) {}

  T *
  allocate (std::size_t n)
  {
    return std::allocator<T> ().allocate (n);
  }
  void
  deallocate (T *p, std::size_t n)
  {
    std::allocator<T> ().deallocate (p, n);
  }
  template <class U, class... Args>
  void
  construct (U *p, Args &&...args)
  {
    ++counts ().constructed;
    ::new (static_cast<void *> (p)) U (std::forward<Args> (args)...);
  }
  template <class U>
  void
  destroy (U *p)
  {
    ++counts ().destroyed;
    p->~U ();
  }
  template <class U> struct rebind
  {
    using other = counting_alloc<U>;
  };
};

template <class T, class U>
bool
operator== (counting_alloc<T> const &, counting_alloc<U> const &)
{
  return true;
}
template <class T, class U>
bool
operator!= (counting_alloc<T> const &, counting_alloc<U> const &)
{
  return false;
}

/// A full buffer of capacity `capacity` of poison values 1..capacity whose
/// head is at slot `head` (the front sits in slot `head`).
circular_buffer<cbtest::poison>
poison_buffer (std::size_t capacity, std::size_t head)
{
  circular_buffer<cbtest::poison> buffer (capacity);
  for (std::size_t i = 0; i < head; ++i)
    {
      buffer.push_back (cbtest::poison (-1));
      buffer.pop_front ();
    }
  for (std::size_t i = 0; i < capacity; ++i)
    buffer.push_back (cbtest::poison (static_cast<int> (i) + 1));
  return buffer;
}

ints
values (circular_buffer<cbtest::poison> const &buffer)
{
  ints out;
  for (std::size_t i = 0; i < buffer.size (); ++i)
    out.push_back (buffer[i].value);
  return out;
}
} // namespace

TEST (CircularBufferHardening,
      CopyOnlyType_PushOfAnElement_CopiesBeforeDestroying)
{
  static_assert (!std::is_move_constructible<copy_only_poison>::value
                     && std::is_copy_constructible<copy_only_poison>::value,
                 "copy only");
  for (std::size_t capacity = 1; capacity <= 3; ++capacity)
    {
      SCOPED_TRACE (testing::Message () << "capacity " << capacity);
      circular_buffer<copy_only_poison> buffer (capacity);
      for (std::size_t i = 0; i < capacity; ++i)
        buffer.emplace_back (static_cast<int> (i) + 1);
      int const oldest = buffer.front ().value;
      buffer.push_back (buffer.front ());
      EXPECT_EQ (buffer.back ().value, oldest);
      int const newest = buffer.back ().value;
      buffer.push_front (buffer.back ());
      EXPECT_EQ (buffer.front ().value, newest);
      int const front_now = buffer.front ().value;
      buffer.emplace_back (buffer.front ());
      EXPECT_EQ (buffer.back ().value, front_now);
      int const back_now = buffer.back ().value;
      buffer.emplace_front (buffer.back ());
      EXPECT_EQ (buffer.front ().value, back_now);
      for (std::size_t i = 0; i < buffer.size (); ++i)
        EXPECT_NE (buffer[i].value, -999);
    }
}

TEST (CircularBufferHardening,
      ReallocationOfAThrowingMoveType_CopiesAndStaysStrong)
{
  move_throws::moves_left () = -1;
  circular_buffer<move_throws> buffer (4);
  buffer.push_back (move_throws (0));
  buffer.pop_front ();
  for (int v = 1; v <= 4; ++v)
    buffer.push_back (move_throws (v)); // wrapped
  ASSERT_FALSE (buffer.is_linearized ());
  std::vector<int> expected;
  expected.push_back (1);
  expected.push_back (2);
  expected.push_back (3);
  expected.push_back (4);
  move_throws::moves_left () = 0; // any move now throws
  EXPECT_NO_THROW (buffer.set_capacity (8));
  EXPECT_NO_THROW (buffer.rset_capacity (5));
  EXPECT_NO_THROW (buffer.linearize ());
  EXPECT_NO_THROW (buffer.resize (7, move_throws (9)));
  move_throws::moves_left () = -1;
  std::vector<int> got;
  for (std::size_t i = 0; i < buffer.size (); ++i)
    got.push_back (buffer[i].value);
  ASSERT_EQ (got.size (), 7U);
  for (std::size_t i = 0; i < 4; ++i)
    EXPECT_EQ (got[i], expected[i]); // not -1: the sources were not moved from
}

TEST (CircularBufferHardening, Aliasing_ElementInTheLastPhysicalSlot)
{
  for (std::size_t capacity = 2; capacity <= 4; ++capacity)
    {
      SCOPED_TRACE (testing::Message () << "capacity " << capacity);
      // the front sits in the last slot: head = capacity - 1
      circular_buffer<cbtest::poison> a
          = poison_buffer (capacity, capacity - 1);
      a.insert (a.begin () + 1, a.front ()); // S = 1 1 2 .. cap, last cap kept
      ASSERT_EQ (a.size (), capacity);
      EXPECT_EQ (a[capacity - 1].value, static_cast<int> (capacity));
      EXPECT_EQ (a[0].value, 1);
      for (std::size_t i = 0; i < a.size (); ++i)
        EXPECT_NE (a[i].value, -999);

      circular_buffer<cbtest::poison> b
          = poison_buffer (capacity, capacity - 1);
      b.insert (b.begin () + 1, 2, b.front ());
      for (std::size_t i = 0; i < b.size (); ++i)
        EXPECT_NE (b[i].value, -999);

      // the back sits in the last slot: head = 0
      circular_buffer<cbtest::poison> c = poison_buffer (capacity, 0);
      c.resize (capacity + 2, c.back ());
      ASSERT_EQ (c.size (), capacity + 2);
      EXPECT_EQ (c[capacity + 1].value, static_cast<int> (capacity));

      circular_buffer<cbtest::poison> d = poison_buffer (capacity, 0);
      d.assign (2, d.back ());
      EXPECT_EQ (values (d), (ints{ static_cast<int> (capacity),
                                    static_cast<int> (capacity) }));

      circular_buffer<cbtest::poison> e
          = poison_buffer (capacity, capacity - 1);
      e.push_back (e.front ());
      e.push_front (e.back ());
      for (std::size_t i = 0; i < e.size (); ++i)
        EXPECT_NE (e[i].value, -999);
    }
}

TEST (CircularBufferHardening,
      InputIteratorInsertAtBeginOfFullBuffer_IsIgnored)
{
  circular_buffer<int> forward (3);
  circular_buffer<int> input (3);
  for (int v = 1; v <= 3; ++v)
    {
      forward.push_back (v);
      input.push_back (v);
    }
  ints const items = { 10, 11, 12, 13 };
  forward.insert (forward.begin (), items.begin (), items.end ());
  std::istringstream stream ("10 11 12 13");
  input.insert (input.begin (), std::istream_iterator<int> (stream),
                std::istream_iterator<int> ());
  EXPECT_EQ (cbtest::content_of (forward), (ints{ 1, 2, 3 }));
  EXPECT_EQ (cbtest::content_of (input), (ints{ 1, 2, 3 }));

  // and in the middle both paths agree
  std::istringstream stream2 ("10 11");
  input.insert (input.begin () + 1, std::istream_iterator<int> (stream2),
                std::istream_iterator<int> ());
  forward.insert (forward.begin () + 1, items.begin (), items.begin () + 2);
  EXPECT_EQ (cbtest::content_of (input), cbtest::content_of (forward));
}

TEST (CircularBufferHardening, AllocatorDestroyIsCalledForEveryConstruct)
{
  counts ().constructed = 0;
  counts ().destroyed = 0;
  {
    using buffer_t = circular_buffer<int, counting_alloc<int>>;
    buffer_t buffer (4);
    for (int v = 0; v < 11; ++v)
      buffer.push_back (v); // overwrites
    for (int v = 0; v < 5; ++v)
      buffer.push_front (v);
    buffer.pop_front ();
    buffer.pop_back ();
    buffer.insert (buffer.begin () + 1, 7);
    buffer.insert (buffer.begin () + 1, 2, 8);
    buffer.erase (buffer.begin ());
    buffer.set_capacity (9);
    buffer.resize (7, 3);
    buffer.rset_capacity (5);
    buffer.assign (3, 6);
    buffer_t copy (buffer);
    buffer_t assigned (2);
    assigned = copy;
    buffer.push_back (1);
    buffer.linearize ();
    buffer.clear ();
    buffer_t moved (std::move (copy));
    moved.push_back (4);
  }
  EXPECT_GT (counts ().constructed, 20U);
  EXPECT_EQ (counts ().constructed, counts ().destroyed);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

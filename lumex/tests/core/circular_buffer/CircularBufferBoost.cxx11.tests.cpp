// Differential tests against boost::circular_buffer: the same random sequence
// of operations is applied to both containers and the size, the capacity and
// the content are compared after every step. Compiled only when the build
// finds Boost.CircularBuffer headers (LUMEX_TEST_BOOST_INCLUDE_DIR, or the
// variable LUMEX_TEST_HAS_BOOST_CIRCULAR_BUFFER defined for the target); the
// rest of the suites build without Boost.
#include <list>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#if defined(LUMEX_TEST_HAS_BOOST_CIRCULAR_BUFFER)

#include <boost/circular_buffer.hpp>

#include "lumex/core/circular_buffer/CircularBuffer"

#include "CircularBufferTestSupport.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

namespace
{
using lumex_int = lumex::core::circular_buffer::circular_buffer<int>;
using boost_int = boost::circular_buffer<int>;
using lumex_text = lumex::core::circular_buffer::circular_buffer<std::string>;
using boost_text = boost::circular_buffer<std::string>;

template <class T> T make_value (int v);

template <>
int
make_value<int> (int v)
{
  return v;
}

template <>
std::string
make_value<std::string> (int v)
{
  return std::string (30, 'x') + std::to_string (v);
}

template <class L, class B>
::testing::AssertionResult
same (L const &lumex_buffer, B const &boost_buffer)
{
  if (lumex_buffer.size () != boost_buffer.size ())
    return ::testing::AssertionFailure () << "size " << lumex_buffer.size ()
                                          << " vs " << boost_buffer.size ();
  if (lumex_buffer.capacity () != boost_buffer.capacity ())
    return ::testing::AssertionFailure ()
           << "capacity " << lumex_buffer.capacity () << " vs "
           << boost_buffer.capacity ();
  if (lumex_buffer.empty () != boost_buffer.empty ()
      || lumex_buffer.full () != boost_buffer.full ())
    return ::testing::AssertionFailure () << "empty () or full ()";
  for (std::size_t i = 0; i < lumex_buffer.size (); ++i)
    if (!(lumex_buffer[i] == boost_buffer[i]))
      return ::testing::AssertionFailure () << "element " << i;
  if (lumex_buffer.max_size () != boost_buffer.max_size ())
    return ::testing::AssertionFailure () << "max_size";
  return ::testing::AssertionSuccess ();
}

/// Applies random operations to both containers and compares after each.
template <class T, class L, class B>
void
run_random_operations (std::uint32_t seed, std::size_t max_capacity, int steps)
{
  cbtest::lcg random (seed);
  std::size_t const first_capacity = 1 + random.below (max_capacity);
  L a (first_capacity);
  B b (first_capacity);
  int counter = 1;
  auto next_value = [&counter] () { return make_value<T> (counter++); };

  for (int step = 0; step < steps; ++step)
    {
      std::size_t const size = a.size ();
      std::size_t const operation = random.below (24);
      SCOPED_TRACE (testing::Message ()
                    << "seed " << seed << " step " << step << " operation "
                    << operation << " size " << size << " capacity "
                    << a.capacity ());
      switch (operation)
        {
        case 0:
        case 1:
          {
            T const v = next_value ();
            a.push_back (v);
            b.push_back (v);
            break;
          }
        case 2:
          {
            T const v = next_value ();
            a.push_front (v);
            b.push_front (v);
            break;
          }
        case 3:
          if (size > 0)
            {
              a.pop_front ();
              b.pop_front ();
            }
          break;
        case 4:
          if (size > 0)
            {
              a.pop_back ();
              b.pop_back ();
            }
          break;
        case 5:
        case 6:
          {
            std::size_t const pos = random.below (size + 1);
            T const v = next_value ();
            auto ia
                = a.insert (a.begin () + static_cast<std::ptrdiff_t> (pos), v);
            auto ib
                = b.insert (b.begin () + static_cast<std::ptrdiff_t> (pos), v);
            ASSERT_EQ (ia - a.begin (), ib - b.begin ());
            break;
          }
        case 7:
          {
            std::size_t const pos = random.below (size + 1);
            std::size_t const n = random.below (a.capacity () + 3);
            T const v = next_value ();
            a.insert (a.begin () + static_cast<std::ptrdiff_t> (pos), n, v);
            b.insert (b.begin () + static_cast<std::ptrdiff_t> (pos), n, v);
            break;
          }
        case 8:
        case 9:
          {
            std::size_t const pos = random.below (size + 1);
            std::size_t const n = random.below (a.capacity () + 3);
            std::vector<T> items;
            for (std::size_t i = 0; i < n; ++i)
              items.push_back (next_value ());
            a.insert (a.begin () + static_cast<std::ptrdiff_t> (pos),
                      items.begin (), items.end ());
            b.insert (b.begin () + static_cast<std::ptrdiff_t> (pos),
                      items.begin (), items.end ());
            break;
          }
        case 10:
          if (size > 0)
            {
              std::size_t const pos = random.below (size);
              auto ia
                  = a.erase (a.begin () + static_cast<std::ptrdiff_t> (pos));
              auto ib
                  = b.erase (b.begin () + static_cast<std::ptrdiff_t> (pos));
              ASSERT_EQ (ia - a.begin (), ib - b.begin ());
            }
          break;
        case 11:
        case 12:
          {
            std::size_t const from = random.below (size + 1);
            std::size_t const to = from + random.below (size - from + 1);
            auto ia = a.erase (a.begin () + static_cast<std::ptrdiff_t> (from),
                               a.begin () + static_cast<std::ptrdiff_t> (to));
            auto ib = b.erase (b.begin () + static_cast<std::ptrdiff_t> (from),
                               b.begin () + static_cast<std::ptrdiff_t> (to));
            ASSERT_EQ (ia - a.begin (), ib - b.begin ());
            break;
          }
        case 13:
          {
            std::size_t const n = random.below (size + 1);
            a.erase_begin (n);
            b.erase_begin (n);
            break;
          }
        case 14:
          {
            std::size_t const n = random.below (size + 1);
            a.erase_end (n);
            b.erase_end (n);
            break;
          }
        case 15:
          {
            std::size_t const n = random.below (max_capacity + 4);
            T const v = next_value ();
            a.resize (n, v);
            b.resize (n, v);
            break;
          }
        case 16:
          {
            std::size_t const n = random.below (max_capacity + 3);
            a.set_capacity (n);
            b.set_capacity (n);
            break;
          }
        case 17:
          {
            std::size_t const n = random.below (max_capacity + 3);
            a.rset_capacity (n);
            b.rset_capacity (n);
            break;
          }
        case 18:
          {
            std::size_t const n = random.below (max_capacity + 3);
            T const v = next_value ();
            a.assign (n, v);
            b.assign (n, v);
            break;
          }
        case 19:
          {
            std::size_t const capacity = 1 + random.below (max_capacity);
            std::size_t const n = random.below (capacity + 1);
            T const v = next_value ();
            a.assign (capacity, n, v);
            b.assign (capacity, n, v);
            break;
          }
        case 20:
          {
            std::size_t const capacity = random.below (max_capacity + 2);
            std::size_t const n = random.below (max_capacity + 5);
            std::list<T> items; // not random access
            for (std::size_t i = 0; i < n; ++i)
              items.push_back (next_value ());
            if (random.below (2) == 0)
              {
                a.assign (items.begin (), items.end ());
                b.assign (items.begin (), items.end ());
              }
            else
              {
                a.assign (capacity, items.begin (), items.end ());
                b.assign (capacity, items.begin (), items.end ());
              }
            break;
          }
        case 21:
          {
            auto const *pa = a.linearize ();
            auto const *pb = b.linearize ();
            ASSERT_EQ (pa == nullptr, pb == nullptr);
            ASSERT_TRUE (a.is_linearized ());
            // An empty Boost buffer can report false (its test is on the
            // pointers); the condition on the addresses holds vacuously.
            if (!a.empty ())
              {
                ASSERT_TRUE (b.is_linearized ());
              }
            for (std::size_t i = 0; i < a.size (); ++i)
              ASSERT_EQ (&a[i], pa + i);
            break;
          }
        case 22:
          if (random.below (4) == 0)
            {
              a.clear ();
              b.clear ();
            }
          else
            {
              L copy_a (a);
              B copy_b (b);
              ASSERT_TRUE (same (copy_a, copy_b));
              a = copy_a;
              b = copy_b;
            }
          break;
        default:
          {
            L other_a (a.capacity () + 1);
            B other_b (b.capacity () + 1);
            a.swap (other_a);
            b.swap (other_b);
            ASSERT_TRUE (same (other_a, other_b));
            other_a.swap (a);
            other_b.swap (b);
            break;
          }
        }
      ASSERT_TRUE (same (a, b));
      for (std::size_t i = 0; i < a.size (); ++i)
        {
          ASSERT_TRUE (a.begin ()[static_cast<std::ptrdiff_t> (i)] == b[i]);
        }
    }
}
} // namespace

TEST (CircularBufferBoost, RandomOperations_Int_MatchBoost)
{
  for (std::uint32_t seed = 1; seed <= 60; ++seed)
    run_random_operations<int, lumex_int, boost_int> (seed, 8, 600);
}

TEST (CircularBufferBoost, RandomOperations_String_MatchBoost)
{
  for (std::uint32_t seed = 101; seed <= 120; ++seed)
    run_random_operations<std::string, lumex_text, boost_text> (seed, 6, 300);
}

TEST (CircularBufferBoost, Constructors_MatchBoost)
{
  std::vector<int> items;
  for (int i = 1; i <= 7; ++i)
    items.push_back (i);
  std::list<int> const list (items.begin (), items.end ());
  for (std::size_t n = 0; n <= 7; ++n)
    {
      EXPECT_TRUE (
          same (lumex_int (items.begin (),
                           items.begin () + static_cast<std::ptrdiff_t> (n)),
                boost_int (items.begin (),
                           items.begin () + static_cast<std::ptrdiff_t> (n))));
      EXPECT_TRUE (same (lumex_int (n, 5), boost_int (n, 5)));
    }
  for (std::size_t capacity = 0; capacity <= 9; ++capacity)
    {
      EXPECT_TRUE (same (lumex_int (capacity, items.begin (), items.end ()),
                         boost_int (capacity, items.begin (), items.end ())));
      EXPECT_TRUE (same (lumex_int (capacity, list.begin (), list.end ()),
                         boost_int (capacity, list.begin (), list.end ())));
      EXPECT_TRUE (same (lumex_int (capacity), boost_int (capacity)));
      for (std::size_t n = 0; n <= capacity; ++n)
        EXPECT_TRUE (
            same (lumex_int (capacity, n, 3), boost_int (capacity, n, 3)));
    }
  EXPECT_TRUE (same (lumex_int (), boost_int ()));
}

TEST (CircularBufferBoost, Comparisons_MatchBoost)
{
  cbtest::lcg random (77);
  for (int round = 0; round < 500; ++round)
    {
      lumex_int a (4);
      lumex_int b (5);
      boost_int ba (4);
      boost_int bb (5);
      for (std::size_t i = 0, n = random.below (5); i < n; ++i)
        {
          int const v = static_cast<int> (random.below (3));
          a.push_back (v);
          ba.push_back (v);
        }
      for (std::size_t i = 0, n = random.below (6); i < n; ++i)
        {
          int const v = static_cast<int> (random.below (3));
          b.push_back (v);
          bb.push_back (v);
        }
      ASSERT_EQ (a == b, ba == bb);
      ASSERT_EQ (a != b, ba != bb);
      ASSERT_EQ (a < b, ba < bb);
      ASSERT_EQ (a <= b, ba <= bb);
      ASSERT_EQ (a > b, ba > bb);
      ASSERT_EQ (a >= b, ba >= bb);
    }
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#else

TEST (CircularBufferBoost, DifferentialTests_NeedBoostCircularBufferHeaders)
{
  GTEST_SKIP () << "boost/circular_buffer.hpp was not found at configure time "
                   "(LUMEX_TEST_BOOST_INCLUDE_DIR)";
}

#endif

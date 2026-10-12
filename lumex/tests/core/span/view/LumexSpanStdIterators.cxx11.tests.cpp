// LumexSpanStdIterators.cxx11.tests.cpp
//
// The iterators of the standard containers are recognized as contiguous
// without a specialization (libstdc++, libc++ and, untested here, the
// standard library of MSVC), in every standard: below C++20 by name, from
// C++20 by std::contiguous_iterator. The span is made from them with the
// iterator and count and the iterator pair constructors. The iterators of
// containers that are not contiguous and the iterator classes of a program
// stay not contiguous until the program names them.
#include <array>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <initializer_list>
#include <iterator>
#include <list>
#include <string>
#include <type_traits>
#include <valarray>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::is_contiguous_iterator;
using lumex::core::span::view::span;

namespace lumex_span_test
{
// A user's allocator: the iterator of the vector is the same wrapper.
template <typename T> struct simple_allocator
{
  using value_type = T;
  simple_allocator () = default;
  template <typename U> simple_allocator (simple_allocator<U> const &) {}
  T *
  allocate (std::size_t count)
  {
    return std::allocator<T> ().allocate (count);
  }
  void
  deallocate (T *pointer, std::size_t count)
  {
    std::allocator<T> ().deallocate (pointer, count);
  }
};
template <typename T, typename U>
bool
operator== (simple_allocator<T> const &, simple_allocator<U> const &)
{
  return true;
}
template <typename T, typename U>
bool
operator!= (simple_allocator<T> const &, simple_allocator<U> const &)
{
  return false;
}
} // namespace lumex_span_test

using lumex_span_test::simple_allocator;

// -- The trait --

TEST (LumexSpanStdIteratorsTest, GivenVector_WhenIterators_ThenContiguous)
{
  EXPECT_TRUE ((is_contiguous_iterator<std::vector<int>::iterator>::value));
  EXPECT_TRUE (
      (is_contiguous_iterator<std::vector<int>::const_iterator>::value));
  EXPECT_TRUE (
      (is_contiguous_iterator<std::vector<std::string>::iterator>::value));
  EXPECT_TRUE ((is_contiguous_iterator<
                std::vector<int, simple_allocator<int>>::iterator>::value));
  EXPECT_TRUE ((is_contiguous_iterator<std::vector<int *>::iterator>::value));
}

TEST (LumexSpanStdIteratorsTest, GivenString_WhenIterators_ThenContiguous)
{
  EXPECT_TRUE ((is_contiguous_iterator<std::string::iterator>::value));
  EXPECT_TRUE ((is_contiguous_iterator<std::string::const_iterator>::value));
  EXPECT_TRUE ((is_contiguous_iterator<std::wstring::iterator>::value));
  EXPECT_TRUE (
      (is_contiguous_iterator<std::u16string::const_iterator>::value));
}

TEST (LumexSpanStdIteratorsTest, GivenArray_WhenIterators_ThenContiguous)
{
  EXPECT_TRUE ((is_contiguous_iterator<std::array<int, 3>::iterator>::value));
  EXPECT_TRUE (
      (is_contiguous_iterator<std::array<int, 3>::const_iterator>::value));
  EXPECT_TRUE ((is_contiguous_iterator<std::array<int, 0>::iterator>::value));
}

TEST (LumexSpanStdIteratorsTest, GivenValarrayAndInitializerList_WhenBegin_Ok)
{
  EXPECT_TRUE ((is_contiguous_iterator<decltype (std::begin (
                    std::declval<std::valarray<int> &> ()))>::value));
  EXPECT_TRUE ((is_contiguous_iterator<decltype (std::begin (
                    std::declval<std::initializer_list<int> &> ()))>::value));
}

TEST (LumexSpanStdIteratorsTest,
      GivenOtherContainers_WhenIterators_NotContiguous)
{
  EXPECT_FALSE ((is_contiguous_iterator<std::deque<int>::iterator>::value));
  EXPECT_FALSE ((is_contiguous_iterator<std::list<int>::iterator>::value));
  EXPECT_FALSE (
      (is_contiguous_iterator<std::forward_list<int>::iterator>::value));
  EXPECT_FALSE ((is_contiguous_iterator<std::vector<bool>::iterator>::value));
  EXPECT_FALSE (
      (is_contiguous_iterator<std::vector<bool>::const_iterator>::value));
}

TEST (LumexSpanStdIteratorsTest, GivenWrappers_WhenReverseOrMove_NotContiguous)
{
  EXPECT_FALSE ((is_contiguous_iterator<
                 std::reverse_iterator<std::vector<int>::iterator>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<
                 std::move_iterator<std::vector<int>::iterator>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<
                 std::back_insert_iterator<std::vector<int>>>::value));
}

TEST (LumexSpanStdIteratorsTest, GivenUserIterator_WhenNotNamed_NotContiguous)
{
  // Random access, a pointer inside, and still not contiguous until named.
  EXPECT_FALSE ((is_contiguous_iterator<
                 lumex_span_test::unregistered_iterator<int>>::value));
  EXPECT_TRUE (
      (is_contiguous_iterator<lumex_span_test::pointer_iterator<int>>::value));
}

// -- The span from them --

TEST (LumexSpanStdIteratorsTest, GivenVector_WhenSpanFromIterators_ThenView)
{
  std::vector<int> values = { 1, 2, 3, 4, 5 };
  span<int> const by_pair (values.begin (), values.end ());
  EXPECT_EQ (by_pair.size (), 5u);
  EXPECT_EQ (by_pair.data (), values.data ());
  span<int> const by_count (values.begin () + 1, 3);
  EXPECT_EQ (by_count.size (), 3u);
  EXPECT_EQ (by_count.data (), values.data () + 1);
  span<int const> const by_const (values.cbegin (), values.cend ());
  EXPECT_EQ (by_const.size (), 5u);
  span<int const> const to_const (values.begin (), 2);
  EXPECT_EQ (to_const.size (), 2u);
  static_assert (
      !std::is_constructible<span<int>, std::vector<int>::const_iterator,
                             std::vector<int>::const_iterator>::value,
      "const elements do not become mutable");
}

TEST (LumexSpanStdIteratorsTest,
      GivenEmptyVector_WhenSpanFromIterators_ThenEmpty)
{
  std::vector<int> none;
  span<int> const view (none.begin (), none.end ());
  EXPECT_EQ (view.size (), 0u);
  span<int> const counted (none.begin (), 0);
  EXPECT_EQ (counted.size (), 0u);
}

TEST (LumexSpanStdIteratorsTest, GivenString_WhenSpanFromIterators_ThenView)
{
  std::string text = "hello";
  span<char> const view (text.begin (), text.end ());
  EXPECT_EQ (view.size (), 5u);
  view[0] = 'j';
  EXPECT_EQ (text, "jello");
  span<char const> const tail (text.cbegin () + 2, 3);
  EXPECT_EQ (tail.size (), 3u);
  EXPECT_EQ (tail[0], 'l');
}

TEST (LumexSpanStdIteratorsTest, GivenArray_WhenSpanFromIterators_ThenView)
{
  std::array<int, 4> values = { { 1, 2, 3, 4 } };
  span<int> const view (values.begin (), values.end ());
  EXPECT_EQ (view.size (), 4u);
  span<int, 4> const fixed (values.begin (), values.end ());
  EXPECT_EQ (fixed.size (), 4u);
  span<int const> const read_only (values.cbegin (), 2);
  EXPECT_EQ (read_only.size (), 2u);
}

TEST (LumexSpanStdIteratorsTest,
      GivenValarrayAndList_WhenSpan_ThenViewOrRejected)
{
  std::valarray<int> values (4);
  span<int> const view (std::begin (values), std::end (values));
  EXPECT_EQ (view.size (), 4u);
  std::initializer_list<int> list = { 1, 2, 3 };
  span<int const> const from_list (list.begin (), list.end ());
  EXPECT_EQ (from_list.size (), 3u);
  static_assert (!std::is_constructible<span<int>, std::list<int>::iterator,
                                        std::list<int>::iterator>::value,
                 "a list iterator is not contiguous");
  static_assert (!std::is_constructible<span<int>, std::deque<int>::iterator,
                                        std::size_t>::value,
                 "a deque iterator is not contiguous");
  static_assert (
      !std::is_constructible<span<bool>, std::vector<bool>::iterator,
                             std::size_t>::value,
      "a vector<bool> iterator is not contiguous");
}

TEST (LumexSpanStdIteratorsTest,
      GivenUserIterator_WhenSpan_ThenRejectedUntilNamed)
{
  static_assert (
      !std::is_constructible<span<int>,
                             lumex_span_test::unregistered_iterator<int>,
                             std::size_t>::value,
      "an unnamed iterator class is rejected");
  static_assert (
      std::is_constructible<span<int>, lumex_span_test::pointer_iterator<int>,
                            std::size_t>::value,
      "a named iterator class is accepted");
  SUCCEED ();
}

// -- The wrappers of the libraries, by their own names (below C++20) --

#if !LUMEX_SPAN_HAS_CONCEPTS && defined(__GLIBCXX__)
TEST (LumexSpanStdIteratorsTest,
      GivenLibstdcxxWrapper_WhenNotOverPointer_NotContiguous)
{
  using lumex::core::span::view::is_contiguous_iterator;
  // The wrapper is contiguous exactly when it wraps a contiguous iterator.
  EXPECT_TRUE (
      (is_contiguous_iterator<
          __gnu_cxx::__normal_iterator<int *, std::vector<int>>>::value));
  EXPECT_FALSE (
      (is_contiguous_iterator<
          __gnu_cxx::__normal_iterator<void *, std::vector<int>>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<__gnu_cxx::__normal_iterator<
                     std::deque<int>::iterator, std::deque<int>>>::value));
}
#endif

#if !LUMEX_SPAN_HAS_CONCEPTS && defined(_LIBCPP_VERSION)
TEST (LumexSpanStdIteratorsTest,
      GivenLibcxxWrappers_WhenOverPointer_Contiguous)
{
  using lumex::core::span::view::is_contiguous_iterator;
  EXPECT_TRUE ((is_contiguous_iterator<std::__wrap_iter<int *>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<std::__wrap_iter<void *>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<
                 std::__wrap_iter<std::deque<int>::iterator>>::value));
#if defined(LUMEX_SPAN_HAS_LIBCPP_BOUNDED_ITER)
  EXPECT_TRUE ((is_contiguous_iterator<std::__bounded_iter<int *>>::value));
  EXPECT_TRUE ((is_contiguous_iterator<
                std::__bounded_iter<std::__wrap_iter<int *>>>::value));
  EXPECT_FALSE ((is_contiguous_iterator<
                 std::__bounded_iter<std::deque<int>::iterator>>::value));
#endif
#if defined(LUMEX_SPAN_HAS_LIBCPP_STATIC_BOUNDED_ITER)
  EXPECT_TRUE (
      (is_contiguous_iterator<std::__static_bounded_iter<int *, 4>>::value));
  EXPECT_FALSE (
      (is_contiguous_iterator<std::__static_bounded_iter<void *, 4>>::value));
#endif
}
#endif

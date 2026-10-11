#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <vector>

#include "lumex/core/span/LumexSpan"

namespace example
{
// A program's own iterator over contiguous storage: a pointer with a name.
class cursor
{
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = std::int32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = std::int32_t *;
  using reference = std::int32_t &;

  cursor () : m_current (nullptr) {}
  explicit cursor (std::int32_t *current) : m_current (current) {}

  reference
  operator* () const
  {
    return *m_current;
  }
  pointer
  operator->() const
  {
    return m_current;
  }
  reference
  operator[] (difference_type n) const
  {
    return m_current[n];
  }
  cursor &
  operator++ ()
  {
    ++m_current;
    return *this;
  }
  cursor
  operator++ (int)
  {
    cursor old (*this);
    ++m_current;
    return old;
  }
  cursor &
  operator-- ()
  {
    --m_current;
    return *this;
  }
  cursor
  operator-- (int)
  {
    cursor old (*this);
    --m_current;
    return old;
  }
  cursor &
  operator+= (difference_type n)
  {
    m_current += n;
    return *this;
  }
  cursor &
  operator-= (difference_type n)
  {
    m_current -= n;
    return *this;
  }
  friend cursor
  operator+ (cursor it, difference_type n)
  {
    return it += n;
  }
  friend cursor
  operator+ (difference_type n, cursor it)
  {
    return it += n;
  }
  friend cursor
  operator- (cursor it, difference_type n)
  {
    return it -= n;
  }
  friend difference_type
  operator- (cursor a, cursor b)
  {
    return a.m_current - b.m_current;
  }
  friend bool
  operator== (cursor a, cursor b)
  {
    return a.m_current == b.m_current;
  }
  friend bool
  operator!= (cursor a, cursor b)
  {
    return a.m_current != b.m_current;
  }
  friend bool
  operator< (cursor a, cursor b)
  {
    return a.m_current < b.m_current;
  }
  friend bool
  operator> (cursor a, cursor b)
  {
    return a.m_current > b.m_current;
  }
  friend bool
  operator<= (cursor a, cursor b)
  {
    return a.m_current <= b.m_current;
  }
  friend bool
  operator>= (cursor a, cursor b)
  {
    return a.m_current >= b.m_current;
  }

private:
  std::int32_t *m_current;
};
} // namespace example

// The iterators of std::vector, std::array, std::basic_string and the like
// are recognized by the library in every standard. The iterator class of a
// program is not: before C++20 the language cannot tell a contiguous iterator
// from a deque's, so the program names it. The line below is a promise that
// *it and *(it + n) are n elements apart. From C++20 the answer is
// std::contiguous_iterator, so the line is redundant only for an iterator that
// models it (one with iterator_concept = std::contiguous_iterator_tag and an
// element_type); the class below declares neither, so it needs the line at
// every standard.
namespace lumex
{
namespace core
{
namespace span
{
namespace view
{
template <> struct is_contiguous_iterator<example::cursor> : std::true_type
{
};
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

using namespace lumex::core::span::view;
using example::cursor;

int
main ()
{
  std::vector<std::int32_t> values = { 3, 1, 4, 1, 5, 9, 2, 6 };

  span<std::int32_t> const by_pointers (values.data (), values.data () + 5);
  span<std::int32_t> const by_iterators (values.begin () + 2, values.end ());
  span<std::int32_t> const by_count (values.begin (), 3);
  std::cout << "pointers=" << by_pointers.size ()
            << " iterators=" << by_iterators.size ()
            << " count=" << by_count.size () << '\n';

  // An empty range from an iterator pair has a null pointer below C++20,
  // because the iterator of an empty range is not dereferenced.
  std::vector<std::int32_t> empty;
  span<std::int32_t> const none (empty.begin (), empty.end ());
  std::cout << "empty size=" << none.size ()
            << " null=" << (none.data () == nullptr) << '\n';

  // The program's own iterator, named above.
  span<std::int32_t> const by_cursor (cursor (values.data () + 1), 3);
  std::cout << "user=" << by_cursor.size () << " first=" << by_cursor.front ()
            << '\n';
  return 0;
}

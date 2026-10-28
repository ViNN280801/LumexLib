// bench_circular_buffer_impl.hpp
// Selects the bounded sequence under test: BENCH_RING_IMPL is
//   1  circular_buffer of this library,
//   2  boost::circular_buffer of Boost.CircularBuffer,
//   3  std::deque used as a bounded queue (pop_front before push_back when
//      full),
//   4  std::vector with manual index math, the wrap done by a compare and a
//      subtraction,
//   5  the same vector ring with the wrap done by `%` (the way the library
//      computed its physical index before it was changed to a compare; it
//      shows what a run-time division costs),
// The scenarios of bench_circular_buffer.cpp are written once against
// bench::ring<T>, which has, in every implementation, the members they use:
// ring (capacity), size, capacity, clear, push_back (overwrites the oldest
// element when full), pop_front, front, back, operator[], begin, end, copy and
// move. Implementations 3 to 5 are what a user writes without a ring library;
// they are not a published container and are kept to the members measured.
#ifndef LUMEX_BENCHMARKS_CIRCULAR_BUFFER_IMPL_HPP
#define LUMEX_BENCHMARKS_CIRCULAR_BUFFER_IMPL_HPP

#include <cstddef>
#include <deque>
#include <iterator>
#include <utility>
#include <vector>

#define BENCH_RING_LUMEX 1
#define BENCH_RING_BOOST 2
#define BENCH_RING_DEQUE 3
#define BENCH_RING_VECTOR 4
#define BENCH_RING_VECTOR_MOD 5

#ifndef BENCH_RING_IMPL
#error                                                                        \
    "define BENCH_RING_IMPL to 1 (lumex), 2 (boost), 3 (deque), 4 (vector) or 5 (vector with %)"
#endif

namespace bench
{
/** A bounded queue on std::deque: pop_front before push_back when full. */
template <typename T> class deque_ring
{
public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = typename std::deque<T>::iterator;
  using const_iterator = typename std::deque<T>::const_iterator;

  explicit deque_ring (size_type capacity) : m_capacity (capacity) {}

  size_type
  size () const
  {
    return m_items.size ();
  }
  size_type
  capacity () const
  {
    return m_capacity;
  }
  void
  clear ()
  {
    m_items.clear ();
  }
  void
  push_back (T const &value)
  {
    if (m_items.size () == m_capacity)
      m_items.pop_front ();
    m_items.push_back (value);
  }
  void
  push_back (T &&value)
  {
    if (m_items.size () == m_capacity)
      m_items.pop_front ();
    m_items.push_back (std::move (value));
  }
  void
  pop_front ()
  {
    m_items.pop_front ();
  }
  T &
  front ()
  {
    return m_items.front ();
  }
  T &
  back ()
  {
    return m_items.back ();
  }
  T &
  operator[] (size_type index)
  {
    return m_items[index];
  }
  T const &
  operator[] (size_type index) const
  {
    return m_items[index];
  }
  iterator
  begin ()
  {
    return m_items.begin ();
  }
  iterator
  end ()
  {
    return m_items.end ();
  }
  const_iterator
  begin () const
  {
    return m_items.begin ();
  }
  const_iterator
  end () const
  {
    return m_items.end ();
  }

private:
  std::deque<T> m_items;
  size_type m_capacity;
};

/** A ring on std::vector with head and size; Mod selects `%` for the wrap. */
template <typename T, bool Mod> class vector_ring
{
  template <typename R, typename V> class basic_iterator
  {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = V;
    using difference_type = std::ptrdiff_t;
    using pointer = V *;
    using reference = V &;
    basic_iterator (R *ring, std::size_t index)
        : m_ring (ring), m_index (index)
    {
    }
    V &
    operator* () const
    {
      return (*m_ring)[m_index];
    }
    basic_iterator &
    operator++ ()
    {
      ++m_index;
      return *this;
    }
    bool
    operator!= (basic_iterator const &other) const
    {
      return m_index != other.m_index;
    }
    bool
    operator== (basic_iterator const &other) const
    {
      return m_index == other.m_index;
    }

  private:
    R *m_ring;
    std::size_t m_index;
  };

public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = basic_iterator<vector_ring, T>;
  using const_iterator = basic_iterator<vector_ring const, T const>;

  explicit vector_ring (size_type capacity)
      : m_items (capacity), m_capacity (capacity)
  {
  }

  size_type
  size () const
  {
    return m_size;
  }
  size_type
  capacity () const
  {
    return m_capacity;
  }
  void
  clear ()
  {
    m_head = 0;
    m_size = 0;
  }
  void
  push_back (T const &value)
  {
    m_items[wrap (m_head + m_size)] = value;
    advance ();
  }
  void
  push_back (T &&value)
  {
    m_items[wrap (m_head + m_size)] = std::move (value);
    advance ();
  }
  void
  pop_front ()
  {
    m_head = wrap (m_head + 1);
    --m_size;
  }
  T &
  front ()
  {
    return m_items[m_head];
  }
  T &
  back ()
  {
    return m_items[wrap (m_head + m_size - 1)];
  }
  T &
  operator[] (size_type index)
  {
    return m_items[wrap (m_head + index)];
  }
  T const &
  operator[] (size_type index) const
  {
    return m_items[wrap (m_head + index)];
  }
  iterator
  begin ()
  {
    return iterator (this, 0);
  }
  iterator
  end ()
  {
    return iterator (this, m_size);
  }
  const_iterator
  begin () const
  {
    return const_iterator (this, 0);
  }
  const_iterator
  end () const
  {
    return const_iterator (this, m_size);
  }

private:
  size_type
  wrap (size_type index) const
  {
    return Mod ? index % m_capacity
               : (index >= m_capacity ? index - m_capacity : index);
  }
  void
  advance ()
  {
    if (m_size == m_capacity)
      m_head = wrap (m_head + 1);
    else
      ++m_size;
  }

  std::vector<T> m_items;
  size_type m_capacity;
  size_type m_head{ 0 };
  size_type m_size{ 0 };
};
} // namespace bench

#if BENCH_RING_IMPL == BENCH_RING_LUMEX

#include "lumex/core/circular_buffer/CircularBuffer"

namespace bench
{
template <typename T>
using ring = lumex::core::circular_buffer::circular_buffer<T>;
#define BENCH_RING_IMPL_NAME "lumex"
} // namespace bench

#elif BENCH_RING_IMPL == BENCH_RING_BOOST

#include <boost/circular_buffer.hpp>

namespace bench
{
template <typename T> using ring = boost::circular_buffer<T>;
#define BENCH_RING_IMPL_NAME "boost"
} // namespace bench

#elif BENCH_RING_IMPL == BENCH_RING_DEQUE

namespace bench
{
template <typename T> using ring = deque_ring<T>;
#define BENCH_RING_IMPL_NAME "deque"
} // namespace bench

#elif BENCH_RING_IMPL == BENCH_RING_VECTOR

namespace bench
{
template <typename T> using ring = vector_ring<T, false>;
#define BENCH_RING_IMPL_NAME "vector"
} // namespace bench

#elif BENCH_RING_IMPL == BENCH_RING_VECTOR_MOD

namespace bench
{
template <typename T> using ring = vector_ring<T, true>;
#define BENCH_RING_IMPL_NAME "vector_mod"
} // namespace bench

#else
#error "BENCH_RING_IMPL must be 1, 2, 3, 4 or 5"
#endif

#endif // !LUMEX_BENCHMARKS_CIRCULAR_BUFFER_IMPL_HPP

// Helpers shared by the circular_buffer test files of this directory: an
// allocator that records who allocated which block, an element type that
// counts its live objects and throws on demand, and a small pseudo random
// generator. Header-only, C++11.
#ifndef LUMEX_TESTS_CORE_CIRCULAR_BUFFER_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_CIRCULAR_BUFFER_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace cbtest
{
/// What the tracking allocators did since the last `reset ()`.
struct ledger_t
{
  std::size_t allocations{ 0 };
  std::size_t deallocations{ 0 };
  /// Blocks freed by an allocator with another id than the one that made them.
  std::size_t wrong_owner{ 0 };
  /// Block address -> id of the allocator that made it.
  std::map<void const *, int> owners;

  void
  reset ()
  {
    allocations = 0;
    deallocations = 0;
    wrong_owner = 0;
    owners.clear ();
  }

  std::size_t
  live_blocks () const
  {
    return owners.size ();
  }
};

inline ledger_t &
ledger ()
{
  static ledger_t instance;
  return instance;
}

/// An allocator with an id; blocks are checked against the id on release.
/// `Pocca`, `Pocma` and `Pocs` select the propagation traits. Two allocators
/// are equal when their ids are equal. `select_on_container_copy_construction`
/// returns an allocator with the id plus 1000.
template <class T, bool Pocca, bool Pocma, bool Pocs> struct tracking_alloc
{
  using value_type = T;
  using propagate_on_container_copy_assignment
      = std::integral_constant<bool, Pocca>;
  using propagate_on_container_move_assignment
      = std::integral_constant<bool, Pocma>;
  using propagate_on_container_swap = std::integral_constant<bool, Pocs>;
  using is_always_equal = std::false_type;

  template <class U> struct rebind
  {
    using other = tracking_alloc<U, Pocca, Pocma, Pocs>;
  };

  int id;

  explicit tracking_alloc (int the_id = 0) : id (the_id) {}

  template <class U>
  tracking_alloc (tracking_alloc<U, Pocca, Pocma, Pocs> const &other)
      : id (other.id)
  {
  }

  T *
  allocate (std::size_t n)
  {
    void *block = ::operator new (n * sizeof (T));
    ledger ().owners[block] = id;
    ++ledger ().allocations;
    return static_cast<T *> (block);
  }

  void
  deallocate (T *block, std::size_t)
  {
    std::map<void const *, int>::iterator it = ledger ().owners.find (block);
    if (it == ledger ().owners.end () || it->second != id)
      ++ledger ().wrong_owner;
    if (it != ledger ().owners.end ())
      ledger ().owners.erase (it);
    ++ledger ().deallocations;
    ::operator delete (block);
  }

  tracking_alloc
  select_on_container_copy_construction () const
  {
    return tracking_alloc (id + 1000);
  }
};

template <class T, class U, bool A, bool B, bool C>
bool
operator== (tracking_alloc<T, A, B, C> const &lhs,
            tracking_alloc<U, A, B, C> const &rhs)
{
  return lhs.id == rhs.id;
}

template <class T, class U, bool A, bool B, bool C>
bool
operator!= (tracking_alloc<T, A, B, C> const &lhs,
            tracking_alloc<U, A, B, C> const &rhs)
{
  return !(lhs == rhs);
}

/// Element with a value, a count of live objects and copies that throw when
/// asked to: `copies_before_throw` is the number of copies still allowed
/// (-1: no limit). Its move constructor is not `noexcept`, so a container
/// that moves only when moving cannot throw copies it.
struct fragile
{
  /// The counters (function-local, so that no definition is needed in C++11).
  struct state_t
  {
    int live;
    int copies_before_throw;
    int constructions_before_throw;
  };

  static state_t &
  state ()
  {
    static state_t instance = { 0, -1, -1 };
    return instance;
  }

  int value;

  explicit fragile (int v = 0) : value (v)
  {
    if (state ().constructions_before_throw == 0)
      throw std::runtime_error ("fragile: construction");
    if (state ().constructions_before_throw > 0)
      --state ().constructions_before_throw;
    ++state ().live;
  }

  fragile (fragile const &other) : value (other.value)
  {
    count_copy ();
    ++state ().live;
  }

  fragile (fragile &&other) /* not noexcept */ : value (other.value)
  {
    count_copy ();
    ++state ().live;
  }

  fragile &
  operator= (fragile const &other)
  {
    value = other.value;
    return *this;
  }

  fragile &
  operator= (fragile &&other)
  {
    value = other.value;
    return *this;
  }

  ~fragile () { --state ().live; }

  /// Removes the limits (the live count is left alone).
  static void
  reset ()
  {
    state ().copies_before_throw = -1;
    state ().constructions_before_throw = -1;
  }

  bool
  operator== (fragile const &other) const
  {
    return value == other.value;
  }

  bool
  operator< (fragile const &other) const
  {
    return value < other.value;
  }

private:
  static void
  count_copy ()
  {
    if (state ().copies_before_throw == 0)
      throw std::runtime_error ("fragile: copy");
    if (state ().copies_before_throw > 0)
      --state ().copies_before_throw;
  }
};

/// Copyable but not movable: an overwrite has to construct in place.
struct pinned
{
  int value;

  explicit pinned (int v = 0) : value (v) {}
  pinned (pinned const &other) : value (other.value) {}
  pinned (pinned &&) = delete;
  pinned &
  operator= (pinned const &other)
  {
    value = other.value;
    return *this;
  }
  pinned &operator= (pinned &&) = delete;

  bool
  operator== (pinned const &other) const
  {
    return value == other.value;
  }
};

/// An element whose destructor overwrites the value (a volatile store, so it
/// is not optimized away): a copy made from a destroyed element is visible.
struct poison
{
  int value;

  explicit poison (int v = 0) : value (v) {}
  poison (poison const &) = default;
  poison &operator= (poison const &) = default;
  ~poison () { static_cast<volatile int &> (value) = -999; }
};

/// Deterministic pseudo random numbers (linear congruential, no <random>).
class lcg
{
public:
  explicit lcg (std::uint32_t seed) : m_state (seed) {}

  std::uint32_t
  next ()
  {
    m_state = m_state * 1664525U + 1013904223U;
    return m_state >> 8;
  }

  /// A number in [0, bound).
  std::size_t
  below (std::size_t bound)
  {
    return static_cast<std::size_t> (next ()) % bound;
  }

private:
  std::uint32_t m_state;
};

/// The content of a buffer, oldest first, through its random-access
/// operator[] and size (works for the library and for Boost).
template <class Buffer>
std::vector<typename Buffer::value_type>
content_of (Buffer const &buffer)
{
  std::vector<typename Buffer::value_type> out;
  for (std::size_t i = 0; i < buffer.size (); ++i)
    out.push_back (buffer[i]);
  return out;
}

/// A buffer of capacity `capacity` whose head is at slot `head`, holding
/// `count` elements `first, first + 1, ...` (built by pushes and pops, so the
/// storage wraps when `head + count > capacity`).
template <class Buffer>
Buffer
make_buffer (std::size_t capacity, std::size_t head, std::size_t count,
             int first = 0)
{
  Buffer buffer (capacity);
  if (capacity == 0)
    return buffer;
  for (std::size_t i = 0; i < head; ++i)
    {
      buffer.push_back (typename Buffer::value_type (-1));
      buffer.pop_front ();
    }
  for (std::size_t i = 0; i < count; ++i)
    buffer.push_back (
        typename Buffer::value_type (first + static_cast<int> (i)));
  return buffer;
}

using plain_alloc = tracking_alloc<int, false, false, false>;
using propagating_alloc = tracking_alloc<int, true, true, true>;
} // namespace cbtest

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_TESTS_CORE_CIRCULAR_BUFFER_TEST_SUPPORT_HPP

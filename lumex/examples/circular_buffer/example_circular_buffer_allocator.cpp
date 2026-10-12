#include <cstddef>
#include <iostream>
#include <memory>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

namespace
{
// An allocator that counts the bytes it hands out.
std::size_t g_bytes = 0;

template <class T> struct counting_allocator
{
  using value_type = T;

  counting_allocator () = default;
  template <class U>
  counting_allocator (counting_allocator<U> const &) noexcept
  {
  }

  T *
  allocate (std::size_t n)
  {
    g_bytes += n * sizeof (T);
    return std::allocator<T> ().allocate (n);
  }
  void
  deallocate (T *p, std::size_t n) noexcept
  {
    std::allocator<T> ().deallocate (p, n);
  }
};

template <class T, class U>
bool
operator== (counting_allocator<T> const &, counting_allocator<U> const &)
{
  return true;
}
template <class T, class U>
bool
operator!= (counting_allocator<T> const &, counting_allocator<U> const &)
{
  return false;
}
}

int
main ()
{
  std::cout << "=== Allocator: one allocation of capacity slots ===\n\n";

  circular_buffer<int, counting_allocator<int>> buf (1000);
  std::cout << "after construction: " << g_bytes << " bytes requested\n";
  for (int i = 0; i < 5000; ++i)
    buf.push_back (i);
  std::cout << "after 5000 push_back: " << g_bytes
            << " bytes requested, size=" << buf.size ()
            << " front=" << buf.front () << '\n';
  counting_allocator<int> const same;
  std::cout << "get_allocator () equals a new counting_allocator: "
            << (buf.get_allocator () == same
                && !(buf.get_allocator () != same))
            << '\n';
  return 0;
}

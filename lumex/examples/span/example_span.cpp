#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "lumex/core/span/LumexSpan"

// The names of the module live in lumex::core::span::view and are not
// injected into the global namespace.
using lumex::core::span::view::as_bytes;
using lumex::core::span::view::as_writable_bytes;
using lumex::core::span::view::byte;
using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;
using lumex::core::span::view::to_integer;

namespace
{
// One parameter type takes an array, a vector, a std::array, a pointer and a
// count, a subview of another span or an initializer_list. Nothing is copied.
std::int64_t
total (span<std::int32_t const> values)
{
  std::int64_t sum = 0;
  for (std::int32_t value : values)
    {
      sum += value;
    }
  return sum;
}

// A span of fixed size says in the type how many values the function needs.
std::int32_t
dot3 (span<std::int32_t const, 3> left, span<std::int32_t const, 3> right)
{
  return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

void
print_values (char const *label, span<std::int32_t const> values)
{
  std::cout << label << " (" << values.size () << "):";
  for (std::int32_t value : values)
    {
      std::cout << ' ' << value;
    }
  std::cout << '\n';
}
} // namespace

int
main ()
{
  std::cout << "=== span: a non-owning view of contiguous elements ===\n\n";

  std::cout << "--- 1. One view type for many sources ---\n";
  std::int32_t c_array[5] = { 5, 3, 8, 1, 9 };
  std::vector<std::int32_t> vector_values (c_array, c_array + 5);
  std::array<std::int32_t, 5> array_values = { { 5, 3, 8, 1, 9 } };
  std::cout << "array=" << total (c_array)
            << " vector=" << total (vector_values)
            << " std::array=" << total (array_values) << " pointer+count="
            << total (span<std::int32_t const> (c_array, 3))
            << " initializer_list="
            << total (std::initializer_list<std::int32_t>{ 1, 2, 3 }) << '\n';
  std::cout << "sizeof(span<int32_t>)=" << sizeof (span<std::int32_t>)
            << " sizeof(span<int32_t, 5>)=" << sizeof (span<std::int32_t, 5>)
            << " (a static extent is not stored)\n";

  std::cout << "\n--- 2. Reading, writing, checked access ---\n";
  span<std::int32_t> window (vector_values);
  window[0] = 50;
  window.back () = 90;
  std::cout << "vector after writes through the span: ";
  for (std::int32_t value : vector_values)
    {
      std::cout << value << ' ';
    }
  std::cout << "\nfront=" << window.front () << " back=" << window.back ()
            << " size=" << window.size ()
            << " size_bytes=" << window.size_bytes ()
            << " empty=" << (window.empty () ? "yes" : "no") << '\n';
  try
    {
      std::cout << window.at (window.size ()) << '\n';
    }
  catch (std::out_of_range const &error)
    {
      std::cout << "at(size()) throws std::out_of_range: " << error.what ()
                << '\n';
    }

  std::cout << "\n--- 3. Subviews ---\n";
  span<std::int32_t> const all (c_array);
  print_values ("first(2)", all.first (2));
  print_values ("last(2)", all.last (2));
  print_values ("subspan(1, 3)", all.subspan (1, 3));
  print_values ("subspan(3)", all.subspan (3));
  // The counts known at compile time give a view with a static extent.
  span<std::int32_t, 5> const fixed (c_array);
  span<std::int32_t, 2> const head = fixed.first<2> ();
  span<std::int32_t, 3> const tail = fixed.subspan<2> ();
  std::cout << "first<2> extent=" << head.extent
            << " subspan<2> extent=" << tail.extent
            << " dot3=" << dot3 (tail, tail) << '\n';
  std::cout << "dynamic_extent is the largest size_t: "
            << (dynamic_extent == static_cast<std::size_t> (-1) ? "yes" : "no")
            << '\n';

  std::cout << "\n--- 4. Iteration and algorithms ---\n";
  std::vector<std::int32_t> to_sort = { 9, 4, 7, 1, 8, 2 };
  span<std::int32_t> const middle
      = span<std::int32_t> (to_sort).subspan (1, 4);
  std::sort (middle.begin (), middle.end ());
  std::cout << "sorted the middle four in place: ";
  for (std::int32_t value : to_sort)
    {
      std::cout << value << ' ';
    }
  std::cout << "\nreversed: ";
  for (span<std::int32_t>::reverse_iterator it = middle.rbegin ();
       it != middle.rend (); ++it)
    {
      std::cout << *it << ' ';
    }
  std::cout << '\n';

  std::cout << "\n--- 5. Conversions ---\n";
  span<std::int32_t, 3> const three (c_array, 3);
  span<std::int32_t> const dynamic = three; // static to dynamic: implicit
  span<std::int32_t const> const read_only = three; // adds const: implicit
  span<std::int32_t, 3> const again (dynamic); // dynamic to static: explicit
  std::cout << "dynamic=" << dynamic.size ()
            << " read_only=" << read_only.size ()
            << " again extent=" << again.extent << '\n';
  std::cout << "std::vector<bool> is not contiguous, so it is rejected at "
               "compile time; so is a std::deque.\n";

  std::cout << "\n--- 6. The bytes of the viewed objects ---\n";
  std::uint16_t words[3] = { 0x0102, 0x0304, 0x0506 };
  span<byte const, 6> const raw = as_bytes (span<std::uint16_t, 3> (words));
  std::cout << "as_bytes extent=" << raw.extent << " bytes (host order):";
  for (byte value : raw)
    {
      std::cout << ' ' << std::hex
                << static_cast<int> (to_integer<int> (value)) << std::dec;
    }
  std::cout << '\n';
  span<byte> const writable = as_writable_bytes (span<std::uint16_t> (words));
  writable[0] = static_cast<byte> (0xFF);
  std::cout << "first byte of words[0] after as_writable_bytes: 0x" << std::hex
            << to_integer<int> (as_bytes (span<std::uint16_t> (words))[0])
            << std::dec << '\n';

  std::cout << "\n=== Span example finished ===\n";
  return 0;
}

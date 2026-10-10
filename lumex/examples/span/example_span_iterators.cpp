#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include "lumex/core/span/LumexSpan"

// Before C++20 the language cannot tell the iterator of a std::vector from the
// iterator of a std::deque, so a span accepts a pointer pair always and any
// other iterator only after the type is named as contiguous. From C++20 the
// answer is std::contiguous_iterator and the line below is redundant.
namespace lumex
{
namespace core
{
namespace span
{
namespace view
{
template <>
struct is_contiguous_iterator<std::vector<std::int32_t>::iterator>
    : std::true_type
{
};
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

using namespace lumex::core::span::view;

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
  return 0;
}

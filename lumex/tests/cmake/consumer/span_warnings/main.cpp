// Every element accessor of span, at () of a const span included, called in a
// translation unit that a strict consumer compiles (see CMakeLists.txt). Exit
// code 0 when every check held.

#include <array>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "lumex/core/span/LumexSpan"

namespace sp = lumex::core::span::view;

namespace
{
using dynamic_t = sp::span<int>;
using const_dynamic_t = sp::span<int const>;
using static_t = sp::span<int, 3>;

// at () of a const object (a span is const here: its members are const) and of
// every extent: the accessor whose error branch ended in a comma operand
// (C4702 under cl /W4).
int
read_at (const_dynamic_t const &view, std::size_t index)
{
  return view.at (index);
}

int
read_at_mutable (dynamic_t const &view, std::size_t index)
{
  return view.at (index);
}

int
read_at_static (static_t const &view, std::size_t index)
{
  return view.at (index);
}

// 0 when `ok`, else 1. A call, so that a strict compiler does not fold a
// comparison of the constants above into a conditional with a dead arm.
int
missed (bool ok)
{
  return ok ? 0 : 1;
}

template <typename Observer>
bool
throws (Observer const &observer)
{
  try
    {
      static_cast<void> (observer ());
    }
  catch (std::out_of_range const &)
    {
      return true;
    }
  return false;
}

// The constexpr tier: at () of a constant span from C++11.
constexpr int kconstants[3] = { 10, 20, 30 };
constexpr const_dynamic_t kconstant_view (kconstants, 3);
constexpr int kconstant_at = kconstant_view.at (1);
constexpr int kconstant_front = kconstant_view.front ();
constexpr int kconstant_back = kconstant_view.back ();
constexpr int kconstant_index = kconstant_view[2];
// A static extent as well.
constexpr sp::span<int const, 3> kconstant_static (kconstants);
constexpr int kstatic_at = kconstant_static.at (2);
constexpr int kstatic_front = kconstant_static.front ();
constexpr int kstatic_back = kconstant_static.back ();
constexpr int kstatic_index = kconstant_static[1];
} // namespace

int
main ()
{
  int failures = 0;

  std::vector<int> storage;
  storage.push_back (1);
  storage.push_back (2);
  storage.push_back (3);
  dynamic_t const view (storage.data (), storage.size ());
  const_dynamic_t const const_view (storage.data (), storage.size ());
  std::array<int, 3> array = { { 4, 5, 6 } };
  static_t const fixed (array);
  dynamic_t const empty;

  failures += missed (read_at (const_view, 0) == 1);
  failures += missed (read_at_mutable (view, 2) == 3);
  failures += missed (read_at_static (fixed, 1) == 5);
  failures += missed (view[1] == 2 && const_view[1] == 2 && fixed[2] == 6);
  failures += missed (view.front () == 1 && view.back () == 3);
  failures += missed (fixed.front () == 4 && fixed.back () == 6);
  failures += missed (view.data () == storage.data ());
  failures += missed (view.size () == 3 && !view.empty () && empty.empty ());
  failures += missed (*view.begin () == 1 && *std::prev (view.end ()) == 3);
  failures += missed (*view.cbegin () == 1 && *std::prev (view.cend ()) == 3);
  failures += missed (*view.rbegin () == 3 && *std::prev (view.rend ()) == 1);
  failures
      += missed (*view.crbegin () == 3 && *std::prev (view.crend ()) == 1);
  failures += missed (view.first (2).size () == 2 && view.last (1)[0] == 3);
  failures += missed (view.subspan (1, 1)[0] == 2);
  failures += missed (view.size_bytes () == 3 * sizeof (int));
  failures += missed (sp::as_bytes (view).size () == 3 * sizeof (int));
  failures += missed (kconstant_at == 20 && kconstant_front == 10
                      && kconstant_back == 30 && kconstant_index == 30);
  failures += missed (kstatic_at == 30 && kstatic_front == 10
                      && kstatic_back == 30 && kstatic_index == 20);

  failures
      += missed (throws ([&const_view] { return read_at (const_view, 3); }));
  failures += missed (throws ([&view] { return read_at_mutable (view, 9); }));
  failures += missed (throws ([&fixed] { return read_at_static (fixed, 3); }));
  failures += missed (throws ([&empty] { return empty.at (0); }));

  if (failures != 0)
    {
      std::printf ("span_warnings: %d check(s) failed\n", failures);
      return 1;
    }
  std::printf ("span_warnings: every accessor compiled and held\n");
  return 0;
}

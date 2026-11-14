// The accessors and views of lumex_string_view, at () and substr () included,
// called in a translation unit that a strict consumer compiles (see
// CMakeLists.txt). Exit code 0 when every check held.

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>

#include "lumex/core/string_view/LumexStringView"

namespace sv = lumex::core::string_view::view;
using view_t = sv::lumex_string_view;

namespace
{
// at () and substr () of a const view: the members whose error branch ended in
// a comma operand (C4702 under cl /W4 without optimization).
char
read_at (view_t const &view, std::size_t index)
{
  return view.at (index);
}

view_t
read_substr (view_t const &view, std::size_t pos, std::size_t count)
{
  return view.substr (pos, count);
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

// The constexpr tier: at (), substr () and the accessors of a constant view.
constexpr view_t kconstant ("hello world", 11);
constexpr char kconstant_at = kconstant.at (4);
constexpr char kconstant_front = kconstant.front ();
constexpr char kconstant_back = kconstant.back ();
constexpr char kconstant_index = kconstant[6];
constexpr std::size_t kconstant_size = kconstant.size ();
constexpr std::size_t kconstant_sub_size = kconstant.substr (6).size ();
} // namespace

int
main ()
{
  int failures = 0;

  view_t const view ("hello world", 11);
  view_t const empty;
  std::string const owner ("needle in a haystack");
  view_t const haystack (owner.data (), owner.size ());

  failures += missed (read_at (view, 0) == 'h' && read_at (view, 10) == 'd');
  failures += missed (view[4] == 'o' && view.front () == 'h'
                      && view.back () == 'd');
  failures += missed (read_substr (view, 6, 5).size () == 5);
  failures += missed (read_substr (view, 6, view_t::npos).size () == 5);
  failures += missed (read_substr (view, 11, 3).empty ());
  failures
      += missed (view.data () != nullptr && view.size () == 11
                 && view.length () == 11 && !view.empty () && empty.empty ());
  failures
      += missed (*view.begin () == 'h' && *std::prev (view.end ()) == 'd');
  failures
      += missed (*view.cbegin () == 'h' && *std::prev (view.cend ()) == 'd');
  failures
      += missed (*view.rbegin () == 'd' && *std::prev (view.rend ()) == 'h');
  failures
      += missed (*view.crbegin () == 'd' && *std::prev (view.crend ()) == 'h');
  failures += missed (view.compare (view_t ("hello world", 11)) == 0
                      && view.compare (view_t ("zzz", 3)) < 0
                      && view.compare (0, 5, view_t ("hello", 5)) == 0);
  failures += missed (view.starts_with (view_t ("hello", 5))
                      && view.ends_with (view_t ("world", 5))
                      && view.starts_with ('h') && view.ends_with ('d'));
  failures
      += missed (view.contains (view_t ("o w", 3)) && view.contains ('w'));
  failures += missed (view.find (view_t ("world", 5)) == 6
                      && view.find ('o') == 4 && view.rfind ('o') == 7);
  failures += missed (view.find_first_of (view_t ("xyzw", 4)) == 6
                      && view.find_last_of (view_t ("lo", 2)) == 9
                      && view.find_first_not_of (view_t ("hel", 3)) == 4
                      && view.find_last_not_of (view_t ("ld", 2)) == 8);
  failures += missed (haystack.find (view_t ("hay", 3)) == 12);
  char copied[6] = { 0, 0, 0, 0, 0, 0 };
  failures += missed (view.copy (copied, 5, 6) == 5 && copied[0] == 'w');
  view_t trimmed = view;
  trimmed.remove_prefix (6);
  trimmed.remove_suffix (1);
  failures += missed (trimmed.size () == 4 && trimmed.front () == 'w');
  failures += missed (kconstant_at == 'o' && kconstant_front == 'h'
                      && kconstant_back == 'd' && kconstant_index == 'w'
                      && kconstant_size == 11 && kconstant_sub_size == 5);

  failures += missed (throws ([&view] { return read_at (view, 11); }));
  failures += missed (throws ([&empty] { return read_at (empty, 0); }));
  failures += missed (throws ([&view] { return read_substr (view, 12, 1); }));
  failures += missed (throws (
      [&view, &copied]
        {
          char buffer[2] = { 0, 0 };
          return view.copy (buffer, 1, 12)
                 + static_cast<std::size_t> (copied[0]);
        }));

  if (failures != 0)
    {
      std::printf ("string_view_warnings: %d check(s) failed\n", failures);
      return 1;
    }
  std::printf ("string_view_warnings: every accessor compiled and held\n");
  return 0;
}

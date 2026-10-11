// Built at C++17 or C++20: the hash of a view equals the one the C++11 unit
// computes, a set filled there is searched here, and a set filled here is
// searched there.
//
// What this can and cannot detect. The hash is one function body in every
// standard (detail::hash_chars); a hash that depended on the standard (as the
// delegation to std::hash<std::string_view> did) gives different values in
// the two units and is caught here. The fixture is built with -O2 on purpose:
// at -O0 both units emit the same weak symbol and the linker keeps one of
// them, which hides a body that differs between the units; with inlining each
// unit computes with its own body. What it cannot detect: a difference that
// shows only for inputs not in the lists below, or a compiler that merges the
// bodies for another reason. producer_cplusplus () proves that the units
// really differ in the standard.
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_set>

#include "producer.hpp"

using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;

namespace
{
int failures = 0;

void
check (bool ok, char const *what)
{
  if (!ok)
    {
      std::fprintf (stderr, "string_view hash mismatch: %s\n", what);
      ++failures;
    }
}
} // namespace

int
main ()
{
  check (producer_cplusplus () == 201103L,
         "the producer is compiled as C++11");
  check (static_cast<long> (__cplusplus) > 201103L,
         "this unit is compiled as a later standard");

  std::size_t count = 0;
  char const *const *keys = producer_keys (count);
  std::unordered_set<lumex_string_view> own (keys, keys + count);
  std::unordered_set<lumex_string_view> const theirs = producer_make_set ();
  for (std::size_t i = 0; i < count; ++i)
    {
      std::size_t const size = std::strlen (keys[i]);
      check (
          std::hash<lumex_string_view> () (lumex_string_view (keys[i], size))
              == producer_hash_char (keys[i], size),
          "char");
      // filled here, searched by C++11 code, and the reverse
      check (producer_find (own, keys[i], size),
             "a key of this set is found by the C++11 code");
      check (theirs.count (lumex_string_view (keys[i], size)) == 1,
             "a key of the C++11 set is found here");
    }
  // A key that only this unit inserts, searched by the C++11 code.
  std::string const extra ("inserted by the consumer");
  own.insert (lumex_string_view (extra));
  check (producer_find (own, extra.data (), extra.size ()),
         "a key inserted here is found by the C++11 code");
  check (!producer_find (theirs, extra.data (), extra.size ()),
         "an absent key is not found by the C++11 code");
  check (std::hash<lumex_wstring_view> () (lumex_wstring_view (L"wide key", 8))
             == producer_hash_wchar (L"wide key", 8),
         "wchar_t");
  check (std::hash<lumex_u16string_view> () (
             lumex_u16string_view (u"wide key", 8))
             == producer_hash_char16 (u"wide key", 8),
         "char16_t");
  check (std::hash<lumex_u32string_view> () (
             lumex_u32string_view (U"wide key", 8))
             == producer_hash_char32 (U"wide key", 8),
         "char32_t");
  check (std::hash<lumex_string_view> () (lumex_string_view ())
             == producer_hash_char (nullptr, 0),
         "the null view");
  if (failures == 0)
    std::puts ("string_view hash: equal at C++11 and here");
  return failures == 0 ? 0 : 1;
}

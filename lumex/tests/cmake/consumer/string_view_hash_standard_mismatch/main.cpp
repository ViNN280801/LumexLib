// Built at C++17 or C++20: the hash of a view equals the one the C++11 unit
// computes, and a set filled there is searched here.
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_set>

#include "producer.hpp"

#include "lumex/core/string_view/LumexStringView"

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
  std::size_t count = 0;
  char const *const *keys = producer_keys (count);
  std::unordered_set<lumex_string_view> own (keys, keys + count);
  for (std::size_t i = 0; i < count; ++i)
    {
      std::size_t const size = std::strlen (keys[i]);
      check (
          std::hash<lumex_string_view> () (lumex_string_view (keys[i], size))
              == producer_hash_char (keys[i], size),
          "char");
      check (producer_set_contains (keys[i], size),
             "a key of the C++11 set is found by the C++11 set");
      check (own.count (lumex_string_view (keys[i], size)) == 1,
             "a key of this set");
    }
  check (!producer_set_contains ("absent", 6), "an absent key");
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
  // The hash of a view is not the hash of the standard view, and no program
  // may rely on it being: only the equality across standards is asserted.
  if (failures == 0)
    std::puts ("string_view hash: equal at C++11 and here");
  return failures == 0 ? 0 : 1;
}

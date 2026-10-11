// Built at C++11 (see CMakeLists.txt).
#include <iterator>
#include <string>

#include "producer.hpp"

using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;

namespace
{
char const *const k_keys[]
    = { "key",       "",  "a longer key of the fixture, 0123456789",
        "k\x80\xff", "K", "12345678",
        "123456789" };
} // namespace

std::size_t
producer_hash_char (char const *data, std::size_t size)
{
  return std::hash<lumex_string_view> () (lumex_string_view (data, size));
}

std::size_t
producer_hash_wchar (wchar_t const *data, std::size_t size)
{
  return std::hash<lumex_wstring_view> () (lumex_wstring_view (data, size));
}

std::size_t
producer_hash_char16 (char16_t const *data, std::size_t size)
{
  return std::hash<lumex_u16string_view> () (
      lumex_u16string_view (data, size));
}

std::size_t
producer_hash_char32 (char32_t const *data, std::size_t size)
{
  return std::hash<lumex_u32string_view> () (
      lumex_u32string_view (data, size));
}

long
producer_cplusplus ()
{
  return static_cast<long> (__cplusplus);
}

std::unordered_set<lumex_string_view>
producer_make_set ()
{
  return std::unordered_set<lumex_string_view> (std::begin (k_keys),
                                                std::end (k_keys));
}

bool
producer_find (std::unordered_set<lumex_string_view> const &set,
               char const *data, std::size_t size)
{
  return set.count (lumex_string_view (data, size)) != 0;
}

char const *const *
producer_keys (std::size_t &count)
{
  count = sizeof k_keys / sizeof k_keys[0];
  return k_keys;
}

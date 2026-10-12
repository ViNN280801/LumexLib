#ifndef LUMEX_TESTS_CMAKE_STRING_VIEW_HASH_PRODUCER_HPP
#define LUMEX_TESTS_CMAKE_STRING_VIEW_HASH_PRODUCER_HPP

#include <cstddef>
#include <unordered_set>

#include "lumex/core/string_view/LumexStringView"

// Compiled at C++11 (producer.cpp). Texts of every character type are passed
// as pointer and size; the hash is std::hash of the view built there.
std::size_t producer_hash_char (char const *data, std::size_t size);
std::size_t producer_hash_wchar (wchar_t const *data, std::size_t size);
std::size_t producer_hash_char16 (char16_t const *data, std::size_t size);
std::size_t producer_hash_char32 (char32_t const *data, std::size_t size);

// The value of __cplusplus in producer.cpp: the hook that proves the two
// translation units really were compiled as different standards.
long producer_cplusplus ();

// A set of views FILLED by the C++11 unit (the keys of producer_keys ()),
// returned to the caller, which searches it with its own code.
std::unordered_set<lumex_string_view> producer_make_set ();
// A set filled by the CALLER, searched by C++11 code.
bool producer_find (std::unordered_set<lumex_string_view> const &set,
                    char const *data, std::size_t size);
char const *const *producer_keys (std::size_t &count);

#endif

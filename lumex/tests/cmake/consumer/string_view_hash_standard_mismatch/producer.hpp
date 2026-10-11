#ifndef LUMEX_TESTS_CMAKE_STRING_VIEW_HASH_PRODUCER_HPP
#define LUMEX_TESTS_CMAKE_STRING_VIEW_HASH_PRODUCER_HPP

#include <cstddef>

// Compiled at C++11 (producer.cpp). Texts of every character type are passed
// as pointer and size; the hash is std::hash of the view built there.
std::size_t producer_hash_char (char const *data, std::size_t size);
std::size_t producer_hash_wchar (wchar_t const *data, std::size_t size);
std::size_t producer_hash_char16 (char16_t const *data, std::size_t size);
std::size_t producer_hash_char32 (char32_t const *data, std::size_t size);
// A set of views filled at C++11 with the texts of keys () .
bool producer_set_contains (char const *data, std::size_t size);
char const *const *producer_keys (std::size_t &count);

#endif

// What a unit of the fixture tells main: the routine its copy of the public
// function selects, and the result of the public functions on a buffer.
#ifndef LUMEX_FIXTURE_UNICODE_MIXED_FLAGS_UNIT_HPP
#define LUMEX_FIXTURE_UNICODE_MIXED_FLAGS_UNIT_HPP

#include <cstddef>
#include <cstdint>

struct unit_result
{
  int variant;
  std::size_t prefix;
  int valid;
};

unit_result plain_unit (char const *data, std::size_t size);
std::uintptr_t plain_unit_ssse3_address ();
std::uintptr_t isa_unit_ssse3_address ();
unit_result isa_unit (char const *data, std::size_t size);
unit_result no_simd_unit (char const *data, std::size_t size);

#endif

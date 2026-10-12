// The field reflection header under -Wall -Wextra -Wpedantic -Werror
// (cmake.hygiene_compile_checks). At C++14 on GCC the field count and the
// indexed get go through the friend-injection loophole (CWG 2118), whose
// friend declaration names a non-template function: GCC reports it with
// -Wnon-template-friend unless the header silences it. From C++17 (structured
// bindings) the loophole is not used and the count and get compile as well.

#include <cstddef>

#include "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp"

// External linkage, so that an unused function is no warning of its own.
struct two_fields_t
{
  int number;
  double ratio;
};

std::size_t
field_count ()
{
  return lumex::core::reflection::field_reflection::tuple_size<
      two_fields_t>::value;
}

#if __cplusplus >= 201402L
double
second_field (two_fields_t &value)
{
  return lumex::core::reflection::field_reflection::get<1> (value);
}
#endif

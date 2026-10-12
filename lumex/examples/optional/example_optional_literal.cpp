#include <iostream>
#include <type_traits>

#include "lumex/core/optional/LumexOptional"

namespace
{
// A constant of an optional: the type is a literal type, so the constructors
// and the observers work in a constant expression (C++11).
constexpr optional<int> default_baud = 115200;
constexpr optional<int> unset_baud;

static_assert (default_baud.value_or (9600) == 115200,
               "a constant optional holds its value");
static_assert (unset_baud.value_or (9600) == 9600,
               "an empty constant gives the default");

// An optional of a trivially copyable type is trivially copyable: it is copied
// with memcpy and passed like a plain struct.
static_assert (std::is_trivially_copyable<optional<int>>::value,
               "optional<int> is trivially copyable");
static_assert (std::is_trivially_destructible<optional<double>>::value,
               "optional<double> needs no destructor call");
}

int
main ()
{
  std::cout << "baud=" << default_baud.value_or (9600)
            << " unset=" << unset_baud.value_or (9600) << '\n';
  return 0;
}

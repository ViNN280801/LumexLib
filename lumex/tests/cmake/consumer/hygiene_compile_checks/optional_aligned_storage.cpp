// The optional header under -Wall -Wextra -Wpedantic -Wdeprecated-declarations
// -Werror, built by cmake.hygiene_compile_checks at C++11 to C++20 and, where
// the compiler has it, at C++23: the value lives in an alignas byte buffer,
// not in std::aligned_storage, which C++23 deprecates (the standard library
// warns on every use). Every member that touches the storage is instantiated,
// for a small type, a type with a destructor and an over-aligned type.

#include <cstddef>
#include <string>
#include <utility>

#include "lumex/core/optional/LumexOptional"

namespace
{

struct alignas (32) over_aligned
{
  char data[40];
};

template <typename T>
bool
use_storage (T const &value)
{
  lumex::core::optional::opt::optional<T> first (value);
  lumex::core::optional::opt::optional<T> second (first);
  lumex::core::optional::opt::optional<T> third (std::move (second));
  third.emplace (value);
  first.swap (third);
  bool const had_value = first.has_value ();
  first.reset ();
  return had_value && !first.has_value ();
}

} // namespace

bool
optional_storage_check ()
{
  over_aligned wide = {};
  return use_storage (1) && use_storage (std::string ("lumex"))
         && use_storage (wide)
         && sizeof (lumex::core::optional::opt::optional<over_aligned>)
                    % alignof (over_aligned)
                == 0;
}

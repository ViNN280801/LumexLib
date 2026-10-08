// Discarded-result check of cmake.hygiene_compile_checks, built with
// -Wall -Wextra -Werror. LUMEX_ATTRIBUTE_NODISCARD is [[nodiscard]] from
// C++17 and __attribute__ ((warn_unused_result)) below it. GCC reports a
// result of the attribute that is only cast to void, so the macro that
// silences a result on purpose, LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR, must do more
// than cast there.
//
// Exactly one of LUMEX_HYGIENE_GOOD_CASE (the macro on every kind of
// expression: must compile) or LUMEX_HYGIENE_BAD_DISCARD (the same result
// discarded without the macro: must NOT compile, which shows that the
// attribute is active and the good case proves something) is defined.

#include <memory>
#include <string>
#include <utility>

#include "lumex/core/utility/attr/LumexAttributes.hpp"

// External linkage, so that an unused function is no warning of its own and
// the bad case fails only for the discarded result.
LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
int
produce_int (int value)
{
  return value + 1;
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
std::string
produce_text ()
{
  return std::string ("text");
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
std::unique_ptr<int>
produce_owner ()
{
  return std::unique_ptr<int> (new int (3));
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
int &
produce_reference (int &value)
{
  return value;
}

void
produce_nothing ()
{
}

struct holder_t
{
  LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
  int
  member_result () const
  {
    return 1;
  }
};

template <typename Value>
LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
Value produce_template (Value value)
{
  return value;
}
// The idiom that expands a pack for its side effects: an array of unknown
// bound in a template, which no reference can bind to before C++20.
template <typename... Args>
int
count_with_array (Args &&...args)
{
  int expanded[] = { 0, (static_cast<void> (args), 0)... };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expanded);
  return static_cast<int> (sizeof...(Args));
}

int
main_like (int unused_parameter, int const *unused_pointer)
{
  int local = 0;
  holder_t holder;

#if defined(LUMEX_HYGIENE_GOOD_CASE)
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (produce_int (1));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (produce_text ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (produce_owner ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (produce_reference (local));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (produce_nothing ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (holder.member_result ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
      produce_template<std::pair<int, int>> (std::pair<int, int> (1, 2)));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_with_array (1, 2.5, "text"));
  std::unique_ptr<int> owner (new int (1));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (owner);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (unused_parameter);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (unused_pointer);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (sizeof (local));
#elif defined(LUMEX_HYGIENE_BAD_DISCARD)
  produce_int (1);
  static_cast<void> (unused_parameter);
  static_cast<void> (unused_pointer);
  static_cast<void> (local);
  static_cast<void> (holder);
#else
#error "define LUMEX_HYGIENE_GOOD_CASE or LUMEX_HYGIENE_BAD_DISCARD"
#endif
  return local;
}

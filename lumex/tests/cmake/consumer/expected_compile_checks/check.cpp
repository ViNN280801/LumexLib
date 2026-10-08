// Compile checks of the global names of the expected umbrella, built by
// cmake.expected_compile_checks. Exactly one of LUMEX_EXPECTED_GOOD_CASE,
// LUMEX_EXPECTED_OWN_NAME_CASE=<n> or LUMEX_EXPECTED_BAD_CASE=<n> is defined;
// the fixture compiles with warnings as errors.
//
// `unexpected` and `in_place` have no global alias. A program may therefore
// declare its own `unexpected` (the MinGW runtime does: <eh.h> declares a
// global function `unexpected`), and the bad cases, which name either of them
// at global scope, must not compile. The names that stay global (`expected`,
// `bad_expected_access`, `make_unexpected`, `unexpect`, `unexpect_t`,
// `in_place_tag`) are used by the good case, so a typo in a bad case cannot
// pass for the check it stands for.

#include <string>
#include <type_traits>

#if defined(LUMEX_EXPECTED_OWN_NAME_CASE)
// The program's own `unexpected`: declared before the umbrella is included
// (cases 1 and 3) or after it (cases 2 and 4); a function, as <eh.h> has it
// (the header itself where MinGW has it), or a class template, as another
// library has it.
#if LUMEX_EXPECTED_OWN_NAME_CASE == 1
#if defined(__MINGW32__)
#include <typeinfo>

#include <eh.h>
#else
void unexpected ();
#endif
#elif LUMEX_EXPECTED_OWN_NAME_CASE == 3
template <typename ErrorType> class unexpected
{
public:
  ErrorType error;
};
#endif
#endif

#include "lumex/core/expected/Expected"

#if defined(LUMEX_EXPECTED_OWN_NAME_CASE)
#if LUMEX_EXPECTED_OWN_NAME_CASE == 2
void unexpected ();
#elif LUMEX_EXPECTED_OWN_NAME_CASE == 4
template <typename ErrorType> class unexpected
{
public:
  ErrorType error;
};
#endif
#endif

namespace
{
using library_expected_t
    = lumex::core::expected::result::expected<int, std::string>;
} // namespace

int
main ()
{
  int used = 0;

#if defined(LUMEX_EXPECTED_GOOD_CASE)
  // The names that stay global are the names of the module.
  static_assert (
      std::is_same<::expected<int, std::string>, library_expected_t>::value,
      "the global expected is the expected of the module");
  static_assert (
      std::is_same<
          ::bad_expected_access<int>,
          lumex::core::expected::error::bad_expected_access<int>>::value,
      "the global bad_expected_access is the one of the module");
  static_assert (
      std::is_same<
          decltype (::make_unexpected<std::string> (3u, 'z')),
          lumex::core::expected::error::unexpected<std::string>>::value,
      "make_unexpected is global and returns the unexpected of the module");
  static_assert (
      std::is_same<decltype (::unexpect), ::unexpect_t const>::value,
      "unexpect and unexpect_t are global");
  static_assert (
      std::is_same<::in_place_tag,
                   lumex::core::expected::result::in_place_tag>::value,
      "in_place_tag is global");

  // `unexpected` is written in full, or under the program's own
  // using-declaration.
  library_expected_t const failed
      = lumex::core::expected::error::unexpected<std::string> (
          std::string ("bad"));
  using lumex::core::expected::error::unexpected;
  library_expected_t const refused = unexpected<std::string> ("no");
  used += failed.has_value () ? 1 : 0;
  used += refused.has_value () ? 1 : 0;
#elif defined(LUMEX_EXPECTED_OWN_NAME_CASE)
  // Next to the program's own `unexpected` the class is reached by its full
  // name.
  lumex::core::expected::error::unexpected<int> const wrapped (1);
  library_expected_t const failed
      = lumex::core::expected::error::unexpected<std::string> (
          std::string ("bad"));
  used += wrapped.error ();
  used += failed.has_value () ? 1 : 0;
#elif LUMEX_EXPECTED_BAD_CASE == 1
  // The class under a qualified global name.
  ::unexpected<int> const wrapped (1);
  used += wrapped.error ();
#elif LUMEX_EXPECTED_BAD_CASE == 2
  // The class under its bare name, with no using-declaration or directive.
  unexpected<int> const wrapped (1);
  used += wrapped.error ();
#elif LUMEX_EXPECTED_BAD_CASE == 3
  // A using-declaration of the global name.
  using ::unexpected;
  used += 1;
#elif LUMEX_EXPECTED_BAD_CASE == 4
  // The tag of the module under the name `in_place` at global scope (it was
  // removed in the same release).
  library_expected_t const value (::in_place, 1);
  used += value.has_value () ? 1 : 0;
#else
#error "LUMEX_EXPECTED_BAD_CASE is not one of the cases of check.cpp"
#endif
  return used == 12345 ? 1 : 0;
}

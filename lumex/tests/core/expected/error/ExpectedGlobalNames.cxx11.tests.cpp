// The expected umbrella leaves the global name `unexpected` to the program.
// The MinGW runtime declares a global function `unexpected ()` in <eh.h>, and
// a global alias of the class once made that header and the umbrella
// impossible in one file. The function below stands in for the program's (or
// the runtime's) own declaration, so it carries the name the umbrella must not
// clash with rather than this library's naming rules: a using-declaration of
// the class template at global scope in any header of the module makes this
// file fail to compile. The names that stay global are still the names of the
// module. The negative side (no `::unexpected`, no `::in_place`) is the
// fixture cmake.expected_compile_checks. The tests compile from C++11, so
// every expected suite (C++11, C++17, C++20) runs them.

#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

// The program's own declaration.
static int
unexpected (int code)
{
  return code + 1;
}

namespace
{
using library_expected_t = lumex::core::expected::result::expected<int, int>;
using library_string_expected_t
    = lumex::core::expected::result::expected<int, std::string>;
using library_unexpected_t = lumex::core::expected::error::unexpected<int>;
} // namespace

TEST (ExpectedGlobalNamesTest,
      GlobalFunctionNamedUnexpected_WithUmbrella_Works)
{
  EXPECT_EQ (unexpected (1), 2);
  EXPECT_EQ (::unexpected (41), 42);
}

TEST (ExpectedGlobalNamesTest, UnexpectedClass_ByFullName_NextToTheFunction)
{
  library_expected_t const failed
      = lumex::core::expected::error::unexpected<int> (7);
  ASSERT_FALSE (failed.has_value ());
  EXPECT_EQ (failed.error (), 7);
  EXPECT_EQ (unexpected (failed.error ()), 8);
}

TEST (ExpectedGlobalNamesTest, ResultNamespace_StillHasUnexpected)
{
  static_assert (
      std::is_same<lumex::core::expected::result::unexpected<int>,
                   library_unexpected_t>::value,
      "lumex::core::expected::result::unexpected is the class of error/");
  lumex::core::expected::result::unexpected<std::string> const wrapped (
      std::string ("bad"));
  EXPECT_EQ (wrapped.error (), "bad");
}

TEST (ExpectedGlobalNamesTest, OtherGlobalNames_StayTheNamesOfTheModule)
{
  static_assert (std::is_same<::expected<int, int>, library_expected_t>::value,
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
      std::is_same<decltype (::unexpect),
                   lumex::core::expected::result::unexpect_t const>::value,
      "the global unexpect is the unexpect of the module");
  static_assert (
      std::is_same<::unexpect_t,
                   lumex::core::expected::result::unexpect_t>::value,
      "the global unexpect_t is the unexpect_t of the module");
  static_assert (
      std::is_same<::in_place_tag,
                   lumex::core::expected::result::in_place_tag>::value,
      "the global in_place_tag is the in_place_tag of the module");

  ::expected<int, std::string> const failed (::unexpect, std::string ("bad"));
  ASSERT_FALSE (failed.has_value ());
  EXPECT_EQ (failed.error (), "bad");
  EXPECT_THROW ((void)failed.value (), ::bad_expected_access<std::string>);

  library_string_expected_t const made
      = ::make_unexpected<std::string> (3u, 'z');
  ASSERT_FALSE (made.has_value ());
  EXPECT_EQ (made.error (), "zzz");

  ::expected<int, int> const tagged (lumex::core::expected::result::in_place,
                                     5);
  ASSERT_TRUE (tagged.has_value ());
  EXPECT_EQ (*tagged, 5);
}

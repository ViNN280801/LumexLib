// LumexJsonHelperStringView.cxx11.tests.cpp
//
// Detail::is_empty_value on the string view of the library, in every
// standard: an empty lumex_string_view is an empty value, like an empty
// std::string (the std::string_view case is in LumexJsonHelper.cxx17.tests.cpp
// and is an exact-match overload of its own). The json suites of every
// standard compile this file.
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/LumexStringView"

#include "lumex/tests/applied/json/helper/LumexJsonHelperTestFixture.hpp"

using lumex::applied::json::helper::Detail::is_empty_value;

TEST_F (LumexJsonHelperTest,
        GivenLumexStringView_WhenIsEmptyValue_ThenByContent)
{
  EXPECT_TRUE (is_empty_value (lumex_string_view ()));
  EXPECT_TRUE (is_empty_value (lumex_string_view ("abc", 0)));
  EXPECT_FALSE (is_empty_value (lumex_string_view ("x")));
  EXPECT_FALSE (is_empty_value (lumex_string_view ("\0", 1)));
  lumex_string_view const view ("text", 4);
  EXPECT_FALSE (is_empty_value (view));
  // The generic overload answers "false" for every non-string type, so this
  // "true" comes from the overload for the view.
  EXPECT_TRUE (is_empty_value (view.substr (4)));
}

TEST_F (LumexJsonHelperTest, GivenOtherForms_WhenIsEmptyValue_ThenUnchanged)
{
  EXPECT_TRUE (is_empty_value (""));
  EXPECT_FALSE (is_empty_value ("x"));
  EXPECT_TRUE (is_empty_value (std::string ()));
  EXPECT_FALSE (is_empty_value (std::string ("x")));
  EXPECT_FALSE (is_empty_value (0));
  EXPECT_FALSE (is_empty_value (1.5));
}

// LumexMacros.tests.cpp
// LUMEX_FUNCTION_NAME, LUMEX_STRINGIZE and LUMEX_CONCAT of LumexMacros.hpp.
// LUMEX_FUNCTION_NAME is chosen by compiler: __FUNCSIG__ with _MSC_VER
// (MSVC, clang-cl), __PRETTY_FUNCTION__ with GCC and Clang (MinGW included),
// __func__ otherwise. LumexMacrosMinGW.tests.cpp checks the MinGW case on
// other hosts.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/macros/LumexMacros.hpp"

namespace
{
std::string
lumex_function_name_probe ()
{
  return LUMEX_FUNCTION_NAME;
}

struct function_name_holder_t
{
  static std::string
  member_probe ()
  {
    return LUMEX_FUNCTION_NAME;
  }
};
} // namespace

TEST (LumexMacrosTest,
      GivenAFreeFunction_WhenItReadsFunctionName_ThenTheNameIsInIt)
{
  // Act
  std::string const name = lumex_function_name_probe ();

  // Assert
  EXPECT_NE (std::string::npos, name.find ("lumex_function_name_probe"))
      << name;
}

TEST (LumexMacrosTest,
      GivenAStaticMemberFunction_WhenItReadsFunctionName_ThenTheNameIsInIt)
{
  // Act
  std::string const name = function_name_holder_t::member_probe ();

  // Assert
  EXPECT_NE (std::string::npos, name.find ("member_probe")) << name;
}

TEST (LumexMacrosTest,
      GivenThisCompiler_WhenFunctionNameIsChosen_ThenTheCompilerDecides)
{
#if defined(_MSC_VER)
  char const *const expected = "__FUNCSIG__";
#elif defined(__GNUC__) || defined(__clang__)
  char const *const expected = "__PRETTY_FUNCTION__";
#else
  char const *const expected = "__func__";
#endif

  EXPECT_STREQ (expected, LUMEX_STRINGIZE (LUMEX_FUNCTION_NAME));
}

TEST (LumexMacrosTest,
      GivenAMacroArgument_WhenStringized_ThenItIsExpandedFirst)
{
#define LUMEX_MACROS_TEST_VALUE 42
  EXPECT_STREQ ("42", LUMEX_STRINGIZE (LUMEX_MACROS_TEST_VALUE));
#undef LUMEX_MACROS_TEST_VALUE
}

TEST (LumexMacrosTest, GivenTwoMacroArguments_WhenConcatenated_ThenOneToken)
{
#define LUMEX_MACROS_TEST_PREFIX lumex_
  int const LUMEX_CONCAT (LUMEX_MACROS_TEST_PREFIX, value) = 7;
#undef LUMEX_MACROS_TEST_PREFIX

  EXPECT_EQ (7, lumex_value);
}

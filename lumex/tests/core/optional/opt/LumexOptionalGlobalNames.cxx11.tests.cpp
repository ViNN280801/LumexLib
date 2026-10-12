// The types header of the module declares nothing at global scope. A program
// that already has global declarations named optional, nullopt, make_optional
// and lumex_bad_optional_access (its own optional, another library's, or std
// names brought in with a using-directive) must still compile when it includes
// lumex/core/optional/opt/LumexOptional.hpp and uses both: LumexMemRead.hpp
// includes that header before C++17, so every file that includes the utility
// umbrella sees it. A global declaration of any of those names in the header
// makes this file fail to compile. The declarations below stand in for that
// program, so they carry the names they must not clash with rather than this
// library's naming rules. The umbrella (lumex/core/optional/LumexOptional)
// is not included here: it is the header that adds the global aliases.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

// The program's declarations.
template <typename T> struct optional
{
  T value;
  bool engaged;
};

static const int nullopt = 42;

template <typename T>
optional<T>
make_optional (T value)
{
  optional<T> result;
  result.value = value;
  result.engaged = true;
  return result;
}

struct lumex_bad_optional_access
{
  int code;
};

TEST (LumexOptionalGlobalNamesTest,
      GivenProgramWithOwnGlobalNames_WhenIncludeTypesHeader_ThenBothWork)
{
  optional<int> const mine = make_optional (5);
  EXPECT_TRUE (mine.engaged);
  EXPECT_EQ (mine.value, 5);
  EXPECT_EQ (nullopt, 42);
  lumex_bad_optional_access const error = { 7 };
  EXPECT_EQ (error.code, 7);

  lumex::core::optional::opt::optional<std::string> theirs ("lumex");
  ASSERT_TRUE (theirs.has_value ());
  EXPECT_EQ (*theirs, "lumex");
  EXPECT_TRUE (theirs != lumex::core::optional::opt::nullopt);
  lumex::core::optional::opt::optional<int> const none;
  EXPECT_TRUE (none == lumex::core::optional::opt::nullopt);
  EXPECT_THROW ((void)none.value (),
                lumex::core::optional::opt::lumex_bad_optional_access);
}

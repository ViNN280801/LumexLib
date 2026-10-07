// LumexTypeTraits.cxx20.tests.cpp
// LUMEX_DEFINE_ENUM_TRAITS and lumex_enum_traits_t. The macro expands to
// `using enum`, which the compiler must have (LUMEX_HAS_USING_ENUM): GCC 8
// accepts -std=c++2a without it, so there the tests skip. The suites from
// C++20 up compile this file together with the .cxx11 and .cxx17 files.
#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#if LUMEX_HAS_USING_ENUM
// Must be invoked at global scope: lumex_enum_traits_t<T> (see
// LumexTypeTraits.hpp) is declared in the global namespace, and
// [temp.expl.spec] requires explicit specializations to live in a namespace
// enclosing the primary template's namespace - so this cannot be nested in an
// anonymous namespace.
LUMEX_DEFINE_ENUM_TRAITS (LumexTypeTraitsTestColor, unsigned char, Red, Green,
                          Blue);

LUMEX_DEFINE_ENUM_TRAITS (LumexTypeTraitsTestSingle, int, Only);
#endif

TEST (LumexTypeTraitsTest,
      GivenReflectedEnum_WhenUsingEnumTraits_ThenValuesFirstLastSizeAreCorrect)
{
#if LUMEX_HAS_USING_ENUM
  using enum LumexTypeTraitsTestColor;

  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestColor>::size, 3U);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestColor>::first, Red);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestColor>::last, Blue);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestColor>::values[1], Green);
#else
  GTEST_SKIP () << "the compiler has no using enum";
#endif
}

TEST (LumexTypeTraitsTest,
      GivenSingleEnumerator_WhenUsingEnumTraits_ThenFirstEqualsLast)
{
#if LUMEX_HAS_USING_ENUM
  using enum LumexTypeTraitsTestSingle;
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestSingle>::size, 1U);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestSingle>::first, Only);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestSingle>::last, Only);
  EXPECT_EQ (lumex_enum_traits_t<LumexTypeTraitsTestSingle>::values[0], Only);
#else
  GTEST_SKIP () << "the compiler has no using enum";
#endif
}

TEST (LumexTypeTraitsTest,
      GivenReflectedEnum_WhenIteratingValues_ThenVisitsEveryEnumerator)
{
#if LUMEX_HAS_USING_ENUM
  using enum LumexTypeTraitsTestColor;
  std::size_t count = 0;
  bool saw_red = false;
  bool saw_green = false;
  bool saw_blue = false;
  for (auto const value :
       lumex_enum_traits_t<LumexTypeTraitsTestColor>::values)
    {
      ++count;
      if (value == Red)
        saw_red = true;
      if (value == Green)
        saw_green = true;
      if (value == Blue)
        saw_blue = true;
    }
  EXPECT_EQ (count, 3U);
  EXPECT_TRUE (saw_red);
  EXPECT_TRUE (saw_green);
  EXPECT_TRUE (saw_blue);
#else
  GTEST_SKIP () << "the compiler has no using enum";
#endif
}

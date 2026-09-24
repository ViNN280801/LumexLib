// LumexReflectedEnum.tests.cpp
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::reflection;

namespace
{
// LUMEX_DEFINE_REFLECTED_ENUM: default toString() from the enumerator
// identifiers.
LUMEX_DEFINE_REFLECTED_ENUM (Color, std::uint8_t, (Red), (Green, 5), (Blue))

// LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING: custom display strings via an
// X-macro.
// clang-format off
  #define SHAPE_STRINGS(ENTRY) \
    ENTRY(Circle, "circle") \
    ENTRY(Square, "square") \
    ENTRY(Triangle, "triangle")
// clang-format on
LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING (Shape, std::uint8_t, SHAPE_STRINGS,
                                       (Circle), (Square, 10), (Triangle))

// clang-format off
#define STATUS_STRINGS(ENTRY) \
  ENTRY(Ok, "ok") \
  ENTRY(Empty, "") \
  ENTRY(Fail, "fail")
// clang-format on
LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING (Status, std::uint8_t, STATUS_STRINGS,
                                       (Ok), (Empty), (Fail), (Hidden))

LUMEX_DEFINE_REFLECTED_ENUM (Single, int, (Only))

struct Wrapper
{
  // A reflected enum invoked at class scope must also work (documented
  // supported use case).
  LUMEX_DEFINE_REFLECTED_ENUM (Nested, int, (A), (B))
};
} // namespace

TEST (LumexReflectedEnumTest,
      GivenDefaultReflectedEnum_WhenEnumeratorsDeclared_ThenValuesMatch)
{
  EXPECT_EQ (static_cast<std::uint8_t> (Color::Red), 0);
  EXPECT_EQ (static_cast<std::uint8_t> (Color::Green), 5);
  EXPECT_EQ (static_cast<std::uint8_t> (Color::Blue), 6);
}

TEST (LumexReflectedEnumTest,
      GivenDefaultReflectedEnum_WhenToString_ThenReturnsEnumeratorIdentifier)
{
  EXPECT_STREQ (toString (Color::Red), "Red");
  EXPECT_STREQ (toString (Color::Green), "Green");
  EXPECT_STREQ (toString (Color::Blue), "Blue");
}

TEST (
    LumexReflectedEnumTest,
    GivenDefaultReflectedEnum_WhenToStringOnUnknownValue_ThenReturnsUnknownPlaceholder)
{
  auto const unknown = static_cast<Color> (123);
  EXPECT_STREQ (toString (unknown), "<Unknown>");
}

TEST (LumexReflectedEnumTest, ToString_WhenFound_ThenEnumeratorName)
{
  EXPECT_STREQ (toString (Color::Red), "Red");
}

TEST (LumexReflectedEnumTest, ToString_WhenUnfound_ThenUnknownPlaceholder)
{
  EXPECT_STREQ (toString (static_cast<Color> (123)), "<Unknown>");
}

TEST (
    LumexReflectedEnumTest,
    GivenDefaultReflectedEnum_WhenReflectionConstantsQueried_ThenTheyMatchDeclarationOrder)
{
  EXPECT_EQ (ColorSize, 3u);
  EXPECT_EQ (ColorFirst, Color::Red);
  EXPECT_EQ (ColorLast, Color::Blue);
  ASSERT_EQ (ColorValues.size (), 3u);
  EXPECT_EQ (ColorValues[0], Color::Red);
  EXPECT_EQ (ColorValues[1], Color::Green);
  EXPECT_EQ (ColorValues[2], Color::Blue);
}

TEST (LumexReflectedEnumTest,
      GivenToStringVariant_WhenToString_ThenReturnsCustomString)
{
  EXPECT_STREQ (toString (Shape::Circle), "circle");
  EXPECT_STREQ (toString (Shape::Square), "square");
  EXPECT_STREQ (toString (Shape::Triangle), "triangle");
}

TEST (
    LumexReflectedEnumTest,
    GivenToStringVariant_WhenReflectionConstantsQueried_ThenTheyMatchDeclarationOrder)
{
  EXPECT_EQ (ShapeSize, 3u);
  EXPECT_EQ (ShapeFirst, Shape::Circle);
  EXPECT_EQ (ShapeLast, Shape::Triangle);
}

TEST (
    LumexReflectedEnumTest,
    GivenClassScopeReflectedEnum_WhenUsedAsMember_ThenWorksLikeNamespaceScope)
{
  EXPECT_EQ (static_cast<int> (Wrapper::Nested::A), 0);
  EXPECT_EQ (static_cast<int> (Wrapper::Nested::B), 1);
  EXPECT_STREQ (Wrapper::toString (Wrapper::Nested::B), "B");
  EXPECT_EQ (Wrapper::NestedSize, 2u);
}

TEST (LumexReflectedEnumTest,
      GivenToStringResult_WhenCalled_ThenNeverReturnsNull)
{
  // The public contract explicitly promises a never-null pointer.
  ASSERT_NE (toString (Color::Red), nullptr);
  ASSERT_NE (toString (static_cast<Color> (200)), nullptr);
}

TEST (LumexReflectedEnumTest,
      GivenCustomEmptyDisplayString_WhenToString_ThenReturnsEmptyNotUnknown)
{
  EXPECT_STREQ (toString (Status::Empty), "");
  EXPECT_STRNE (toString (Status::Empty), "<Unknown>");
}

TEST (
    LumexReflectedEnumTest,
    GivenEnumeratorOmittedFromXMacro_WhenToString_ThenReturnsUnknownPlaceholder)
{
  EXPECT_STREQ (toString (Status::Hidden), "<Unknown>");
}

TEST (LumexReflectedEnumTest,
      GivenSingleEnumerator_WhenReflectionConstantsQueried_ThenFirstEqualsLast)
{
  EXPECT_EQ (SingleSize, 1u);
  EXPECT_EQ (SingleFirst, Single::Only);
  EXPECT_EQ (SingleLast, Single::Only);
  EXPECT_STREQ (toString (Single::Only), "Only");
}

// The enumerators of a LUMEX_DEFINE_REFLECTED_ENUM found by
// argument-dependent lookup: lumex_enum_values (E ()) gives them in the
// order of the declaration, so that generic code (the sample values of
// lumex/core/protocol) reaches the enumerators of any reflected enumeration,
// whatever their values.

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

#include <gtest/gtest.h>

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace values_tables
{
LUMEX_DEFINE_REFLECTED_ENUM (stage, std::uint8_t, (warm, 3), (hot, 7))

LUMEX_DEFINE_REFLECTED_ENUM (wide, std::uint64_t, (top, 0x8000000000000000ull),
                             (low, 1))

LUMEX_DEFINE_REFLECTED_ENUM (offset, std::int32_t, (back, -70000),
                             (ahead, 70000))

// clang-format off
#define VALUES_SHAPES(ENTRY) \
  ENTRY (circle, "round") \
  ENTRY (square, "four corners")
// clang-format on
LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING (shape, std::uint8_t, VALUES_SHAPES,
                                       (circle, 4), (square))
} // namespace values_tables

namespace
{
namespace tables = values_tables;

// What generic code does: an unqualified call that ADL resolves.
template <typename E>
auto
values_of () -> decltype (lumex_enum_values (E ()))
{
  return lumex_enum_values (E ());
}

static_assert (std::tuple_size<decltype (values_of<tables::stage> ())>::value
                   == 2,
               "one value per enumerator");
static_assert (std::tuple_size<decltype (values_of<tables::shape> ())>::value
                   == 2,
               "the custom-string variant has its values too");
} // namespace

TEST (
    LumexReflectedEnumValuesTest,
    GivenAReflectedEnumeration_WhenItsValuesAreAskedByADL_ThenTheyAreTheEnumeratorsInOrder)
{
  std::array<tables::stage, 2> const stages = values_of<tables::stage> ();
  EXPECT_TRUE (stages[0] == tables::stage::warm);
  EXPECT_TRUE (stages[1] == tables::stage::hot);
  std::array<tables::shape, 2> const shapes = values_of<tables::shape> ();
  EXPECT_TRUE (shapes[0] == tables::shape::circle);
  EXPECT_TRUE (shapes[1] == tables::shape::square);
  EXPECT_EQ (static_cast<int> (shapes[1]), 5);
}

TEST (LumexReflectedEnumValuesTest,
      GivenEnumeratorsFarFromZero_WhenAskedByADL_ThenTheirValuesAreKept)
{
  std::array<tables::wide, 2> const wides = values_of<tables::wide> ();
  EXPECT_EQ (static_cast<std::uint64_t> (wides[0]), 0x8000000000000000ull);
  EXPECT_EQ (static_cast<std::uint64_t> (wides[1]), 1u);
  std::array<tables::offset, 2> const offsets = values_of<tables::offset> ();
  EXPECT_EQ (static_cast<std::int32_t> (offsets[0]), -70000);
  EXPECT_EQ (static_cast<std::int32_t> (offsets[1]), 70000);
}

TEST (LumexReflectedEnumValuesTest,
      GivenTheValues_WhenComparedWithTheCompanionArray_ThenTheyAreTheSame)
{
  EXPECT_TRUE (values_of<tables::stage> () == tables::stageValues);
  EXPECT_TRUE (values_of<tables::wide> () == tables::wideValues);
}

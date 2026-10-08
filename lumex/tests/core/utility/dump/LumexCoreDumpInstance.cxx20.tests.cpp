// LumexCoreDumpInstance.cxx20.tests.cpp
//
// With <ranges> (C++20) core_dump_generator::get_memory_filters_range returns
// the view that std::views::all makes of the filter list,
// std::ranges::ref_view<std::vector<std::string> const>, as it did before the
// C++11 port; before it (or on a toolchain without <ranges>: GCC 8 accepts
// -std=c++2a without it) the result is the iterator_range of this library,
// checked in LumexCoreDumpInstance.cxx11. The alias memory_filters_range_t is
// the one place that chooses.
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"

#if LUMEX_HAS_STD_RANGES

using lumex::core::utility::dump::core_dump_generator;

TEST (LumexCoreDumpInstanceCxx20Test,
      GivenStdRanges_WhenMemoryFiltersRangeAlias_ThenRefViewOfTheFilterList)
{
  static_assert (
      std::is_same<
          core_dump_generator::memory_filters_range_t,
          std::ranges::ref_view<std::vector<std::string> const>>::value,
      "ref_view of the constant filter list");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx20Test,
      GivenStdRanges_WhenGetMemoryFiltersRange_ThenSameTypeAsViewsAll)
{
  static_assert (
      std::is_same<
          decltype (core_dump_generator::get_memory_filters_range ()),
          decltype (std::views::all (
              std::declval<std::vector<std::string> const &> ()))>::value,
      "the result is what std::views::all gives for the filter list");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx20Test,
      GivenStdRanges_WhenMemoryFiltersRange_ThenIsAViewAndASizedRange)
{
  using range_t = core_dump_generator::memory_filters_range_t;
  static_assert (std::ranges::view<range_t>, "a view");
  static_assert (std::ranges::sized_range<range_t>, "a sized range");
  static_assert (std::ranges::random_access_range<range_t>,
                 "a random access range");
  static_assert (std::ranges::borrowed_range<range_t>,
                 "a borrowed range, as ref_view is");
  static_assert (std::is_same<std::ranges::range_reference_t<range_t>,
                              std::string const &>::value,
                 "the elements are constant strings");
  SUCCEED ();
}

#else

TEST (LumexCoreDumpInstanceCxx20Test, GivenNoStdRanges_ThenNothingToCheck)
{
  GTEST_SKIP () << "this toolchain has no <ranges>";
}

#endif

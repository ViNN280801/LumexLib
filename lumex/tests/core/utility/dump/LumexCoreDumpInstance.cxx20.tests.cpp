// LumexCoreDumpInstance.cxx20.tests.cpp
//
// With <ranges> (C++20) core_dump_generator::get_memory_filters_range still
// returns the iterator_range of this library, never std::ranges::ref_view; the
// iterator_range opts into std::ranges::enable_view and enable_borrowed_range,
// so it is what ref_view was: a view and a borrowed, sized, random access
// range of constant strings (the type itself and the behavior are checked in
// LumexCoreDumpInstance.cxx11 in every suite). GCC 8 accepts -std=c++2a
// without <ranges>, so there the tests skip.
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
      GivenStdRanges_WhenMemoryFiltersRangeAlias_ThenNotARefView)
{
  static_assert (
      !std::is_same<
          core_dump_generator::memory_filters_range_t,
          std::ranges::ref_view<std::vector<std::string> const>>::value,
      "never std::ranges::ref_view");
  static_assert (
      std::is_same<core_dump_generator::memory_filters_range_t,
                   lumex::core::utility::ranges::iterator_range<
                       std::vector<std::string>::const_iterator>>::value,
      "the iterator_range of the library");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx20Test,
      GivenStdRanges_WhenGetMemoryFiltersRange_ThenNotWhatViewsAllGives)
{
  static_assert (
      !std::is_same<
          decltype (core_dump_generator::get_memory_filters_range ()),
          decltype (std::views::all (
              std::declval<std::vector<std::string> const &> ()))>::value,
      "the result is not what std::views::all gives for the filter list");
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

TEST (LumexCoreDumpInstanceCxx20Test,
      GivenStdRanges_WhenPipedAsRvalue_ThenViewsComposeWithTheRange)
{
  // A borrowed view can be the rvalue operand of a view adaptor.
  auto const lengths = core_dump_generator::get_memory_filters_range ()
                       | std::views::transform ([] (std::string const &filter)
                                                  { return filter.size (); });
  EXPECT_EQ (std::ranges::distance (lengths), 0);
}

#else

TEST (LumexCoreDumpInstanceCxx20Test, GivenNoStdRanges_ThenNothingToCheck)
{
  GTEST_SKIP () << "this toolchain has no <ranges>";
}

#endif

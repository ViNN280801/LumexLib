// LumexSpanChecks.cxx11.tests.cpp
//
// The precondition checks of span (LUMEX_SPAN_CHECKS): every check aborts with
// its message when it is violated and lets the border value through; with the
// checks off nothing aborts; the macro takes the value the suite sets
// (LUMEX_TEST_SPAN_EXPECT_CHECKS from the variant, or the default of the
// build); the checks are usable in constant expressions.
#include <array>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

// -- The value of the macro --

#if defined(LUMEX_TEST_SPAN_EXPECT_CHECKS)
static_assert (LUMEX_SPAN_CHECKS == LUMEX_TEST_SPAN_EXPECT_CHECKS,
               "the suite sets LUMEX_SPAN_CHECKS and the library keeps it");
#elif defined(NDEBUG)
static_assert (LUMEX_SPAN_CHECKS == 0,
               "by default the checks are off with NDEBUG");
#else
static_assert (LUMEX_SPAN_CHECKS == 1,
               "by default the checks are on without NDEBUG");
#endif

namespace
{
// The checks are plain expressions, so the border values work in constant
// expressions at C++11 whether the checks are on or off.
constexpr int kConstantValues[4] = { 1, 2, 3, 4 };
constexpr span<int const> kConstantView (kConstantValues, 4);
constexpr span<int const, 4> kConstantStatic (kConstantValues);

static_assert (kConstantView[3] == 4, "operator[] at the last index");
static_assert (kConstantView.front () == 1 && kConstantView.back () == 4,
               "front and back of a span that is not empty");
static_assert (kConstantView.first (4).size () == 4
                   && kConstantView.first (0).size () == 0,
               "first at the borders");
static_assert (kConstantView.last (4).size () == 4
                   && kConstantView.last (0).size () == 0,
               "last at the borders");
static_assert (kConstantView.subspan (4).size () == 0
                   && kConstantView.subspan (1, 3).size () == 3
                   && kConstantView.subspan (0, 4).size () == 4,
               "subspan at the borders");
static_assert (kConstantView.first<4> ().size () == 4
                   && kConstantView.last<4> ().size () == 4
                   && kConstantView.subspan<4, 0> ().size () == 0
                   && kConstantView.subspan<1, 3> ().size () == 3,
               "static subviews of a dynamic span at the borders");
static_assert (span<int const, 4> (kConstantValues, 4).size () == 4,
               "a static extent from a pointer and the equal count");
static_assert (
    span<int const, 4> (kConstantValues, kConstantValues + 4).size () == 4,
    "a static extent from a pointer pair of the equal length");
static_assert (span<int const, 4> (kConstantView).size () == 4,
               "a static extent from a dynamic span of the equal size");
static_assert (kConstantStatic[0] == 1, "static operator[]");

} // namespace

#if LUMEX_SPAN_CHECKS && GTEST_HAS_DEATH_TEST

namespace
{
// A cast to void does not silence warn_unused_result of GCC: the value of the
// statement is passed to a function that ignores it.
template <typename T>
void
discard (T const &)
{
}

#define LUMEX_SPAN_TEST_ABORTS(statement, message)                            \
  LUMEX_SPAN_EXPECT_ABORT (discard (statement),                               \
                           message ".* failed at .*LumexSpan\\.hpp:.*")
} // namespace

class LumexSpanChecksDeathTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // The tests start no threads. The "threadsafe" style spawns the child with
    // clone (), which AddressSanitizer with GoogleTest 1.12 crashes in.
    GTEST_FLAG_SET (death_test_style, "fast");
  }

  int values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
};

TEST_F (LumexSpanChecksDeathTest, GivenIndex_WhenAtOrAfterSize_ThenAborts)
{
  span<int> const view (values, 4);
  EXPECT_EQ (view[0], 10);
  EXPECT_EQ (view[3], 13);
  LUMEX_SPAN_TEST_ABORTS (view[4], "index < m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view[7], "index < m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view[static_cast<std::size_t> (-1)],
                          "index < m_storage.size \\(\\)");
  span<int> const none;
  LUMEX_SPAN_TEST_ABORTS (none[0], "index < m_storage.size \\(\\)");
  span<int, 4> const fixed (values, 4);
  EXPECT_EQ (fixed[3], 13);
  LUMEX_SPAN_TEST_ABORTS (fixed[4], "index < m_storage.size \\(\\)");
}

TEST_F (LumexSpanChecksDeathTest, GivenFrontAndBack_WhenEmpty_ThenAbort)
{
  span<int> const empty (values, 0);
  LUMEX_SPAN_TEST_ABORTS (empty.front (), "!empty \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (empty.back (), "!empty \\(\\)");
  span<int> const one (values + 2, 1);
  EXPECT_EQ (one.front (), 12);
  EXPECT_EQ (one.back (), 12);
  span<int> const four (values, 4);
  EXPECT_EQ (four.front (), 10);
  EXPECT_EQ (four.back (), 13);
}

TEST_F (LumexSpanChecksDeathTest,
        GivenFirstAndLastCount_WhenOverSize_ThenAbort)
{
  span<int> const view (values, 4);
  EXPECT_EQ (view.first (4).size (), 4u);
  EXPECT_EQ (view.first (0).size (), 0u);
  EXPECT_EQ (view.last (4).size (), 4u);
  EXPECT_EQ (view.last (0).size (), 0u);
  LUMEX_SPAN_TEST_ABORTS (view.first (5), "count <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.last (5), "count <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.first (static_cast<std::size_t> (-1)),
                          "count <= m_storage.size \\(\\)");
}

TEST_F (LumexSpanChecksDeathTest, GivenSubspan_WhenOutOfRange_ThenAborts)
{
  span<int> const view (values, 4);
  EXPECT_EQ (view.subspan (4).size (), 0u);
  EXPECT_EQ (view.subspan (4, 0).size (), 0u);
  EXPECT_EQ (view.subspan (1).size (), 3u);
  EXPECT_EQ (view.subspan (1, 3).size (), 3u);
  EXPECT_EQ (view.subspan (0, 4).size (), 4u);
  EXPECT_EQ (view.subspan (2, dynamic_extent).size (), 2u);
  LUMEX_SPAN_TEST_ABORTS (view.subspan (5), "offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.subspan (5, 0),
                          "offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.subspan (1, 4),
                          "offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.subspan (0, 5),
                          "offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.subspan (4, 1),
                          "offset <= m_storage.size \\(\\)");
}

TEST_F (LumexSpanChecksDeathTest,
        GivenStaticSubviews_WhenOverDynamicSize_Abort)
{
  span<int> const view (values, 4);
  EXPECT_EQ (view.first<4> ().size (), 4u);
  EXPECT_EQ (view.last<4> ().size (), 4u);
  EXPECT_EQ (view.subspan<4> ().size (), 0u);
  EXPECT_EQ ((view.subspan<4, 0> ().size ()), 0u);
  EXPECT_EQ ((view.subspan<2, 2> ().size ()), 2u);
  LUMEX_SPAN_TEST_ABORTS (view.first<5> (), "Count <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.last<5> (), "Count <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS (view.subspan<5> (),
                          "Offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS ((view.subspan<2, 3> ()),
                          "Offset <= m_storage.size \\(\\)");
  LUMEX_SPAN_TEST_ABORTS ((view.subspan<4, 1> ()),
                          "Offset <= m_storage.size \\(\\)");
}

TEST_F (LumexSpanChecksDeathTest,
        GivenStaticExtent_WhenCountDiffers_ThenAborts)
{
  EXPECT_EQ ((span<int, 4> (values, 4).size ()), 4u);
  LUMEX_SPAN_TEST_ABORTS ((span<int, 4> (values, 5)), "count == Extent");
  LUMEX_SPAN_TEST_ABORTS ((span<int, 4> (values, 3)), "count == Extent");
  EXPECT_EQ ((span<int, 4> (values, values + 4).size ()), 4u);
  LUMEX_SPAN_TEST_ABORTS ((span<int, 4> (values, values + 3)), "== Extent");
  LUMEX_SPAN_TEST_ABORTS ((span<int, 4> (values, values + 5)), "== Extent");
}

TEST_F (LumexSpanChecksDeathTest, GivenPointerPair_WhenReversed_ThenAborts)
{
  EXPECT_EQ ((span<int> (values, values + 4).size ()), 4u);
  EXPECT_EQ ((span<int> (values + 4, values + 4).size ()), 0u);
  LUMEX_SPAN_TEST_ABORTS ((span<int> (values + 4, values + 3)), ">= 0");
}

TEST_F (LumexSpanChecksDeathTest,
        GivenStaticExtent_WhenSourceSizeDiffers_Abort)
{
  std::vector<int> const three (3, 1);
  std::vector<int> four (4, 1);
  EXPECT_EQ ((span<int, 4> (four).size ()), 4u);
  LUMEX_SPAN_TEST_ABORTS ((span<int const, 4> (three)),
                          "range.size \\(\\)\\) == Extent");
  span<int> const dynamic_three (values, 3);
  span<int> const dynamic_four (values, 4);
  EXPECT_EQ ((span<int, 4> (dynamic_four).size ()), 4u);
  LUMEX_SPAN_TEST_ABORTS ((span<int, 4> (dynamic_three)),
                          "other.size \\(\\) == Extent");
}

TEST_F (LumexSpanChecksDeathTest,
        GivenViolation_WhenReported_ThenMessageHasTheFixedFormat)
{
  span<int> const view (values, 4);
  LUMEX_SPAN_EXPECT_ABORT (
      discard (view[9]),
      "^LUMEX_SPAN_CHECKS: index < m_storage.size \\(\\) failed at "
      ".*LumexSpan\\.hpp:.*\n");
}

#elif !LUMEX_SPAN_CHECKS

// -- Checks off: the same calls do not abort --
// Nothing here executes undefined behavior: every call below only makes a
// span, a size or a pointer inside the buffer, or reads an element inside it.

TEST (LumexSpanChecksOffTest, GivenCheckMacro_WhenChecksOff_ThenOnlyTheValue)
{
  // The condition of a check is not even evaluated.
  int evaluations = 0;
  int const value = LUMEX_SPAN_CHECKED (++evaluations < 0, 7);
  EXPECT_EQ (value, 7);
  EXPECT_EQ (evaluations, 0);
}

TEST (LumexSpanChecksOffTest,
      GivenViolatedPreconditions_WhenChecksOff_ThenNoAbort)
{
  int values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
  span<int> const view (values + 2, 4);
  EXPECT_EQ (view[5], 17);
  EXPECT_EQ (view.first (5).size (), 5u);
  EXPECT_EQ (view.subspan (5, 2).size (), 2u);
  EXPECT_EQ (view.subspan (4, 1).size (), 1u);
  EXPECT_EQ ((view.first<5> ().size ()), 5u);
  EXPECT_EQ ((view.subspan<2, 3> ().size ()), 3u);
  span<int> const empty (values + 2, 0);
  EXPECT_EQ (empty.front (), 12);
  EXPECT_EQ ((span<int, 4> (values, 5).size ()), 4u);
  EXPECT_EQ ((span<int, 4> (values, values + 5).size ()), 4u);
  EXPECT_EQ ((span<int, 4> (span<int> (values, 5)).size ()), 4u);
  std::vector<int> three (3, 1);
  EXPECT_EQ ((span<int, 4> (three).size ()), 4u);
}

#else

TEST (LumexSpanChecksTest, GivenNoDeathTests_WhenBuilt_ThenSkipped)
{
  GTEST_SKIP () << "death tests are not supported on this platform";
}

#endif

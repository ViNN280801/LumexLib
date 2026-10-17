// C++20 additions of shared_ptr: the three-way comparison
// ([util.smartptr.shared.cmp]) compared with std::shared_ptr for every pair,
// the synthesized relational operators, and the std::ranges comparison
// objects over the module's pointers.

#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#if __has_include(<concepts>)
#include <concepts>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

// Needs <compare> and <concepts> (not in GCC 8 -std=c++2a)
#if LUMEX_HAS_STD_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS
template <class F>
void
scenario_three_way (Trace &t)
{
  static int values[4] = { 0, 1, 2, 3 };
  shared_of<F, int> owner (new int (0));
  std::vector<shared_of<F, int>> pointers;
  for (int i = 0; i < 4; ++i)
    pointers.push_back (shared_of<F, int> (owner, &values[i]));
  pointers.push_back (shared_of<F, int> ());
  auto note_order = [&t] (std::strong_ordering order)
    {
      t.note_bool (order == std::strong_ordering::less);
      t.note_bool (order == std::strong_ordering::equal);
      t.note_bool (order == std::strong_ordering::greater);
    };
  for (auto const &a : pointers)
    {
      for (auto const &b : pointers)
        note_order (a <=> b);
      note_order (a <=> nullptr);
    }
  shared_of<F, int const> constant (pointers[2]);
  note_order (constant <=> pointers[1]);
  note_order (pointers[3] <=> constant);
}

TEST (LumexSharedPtrCpp20Test,
      GivenTheSpaceship_WhenComparingAllPairs_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_three_way);
}

TEST (LumexSharedPtrCpp20Test,
      GivenTheOperators_WhenChecked_ThenTheyAreTotalAndRewritten)
{
  using ptr = sp::shared_ptr<int>;
  static_assert (std::three_way_comparable<ptr>, "");
  static_assert (std::totally_ordered<ptr>, "");
  static_assert (std::equality_comparable<ptr>, "");
  static_assert (
      std::three_way_comparable_with<ptr, sp::shared_ptr<int const>>, "");
  static_assert (std::equality_comparable_with<ptr, std::nullptr_t>, "");
  static_assert (std::is_same_v<decltype (std::declval<ptr const &> ()
                                          <=> std::declval<ptr const &> ()),
                                std::strong_ordering>);
  static_assert (
      std::is_same_v<decltype (std::declval<ptr const &> () <=> nullptr),
                     std::strong_ordering>);
  static_assert (noexcept (std::declval<ptr const &> ()
                           <=> std::declval<ptr const &> ()));
  SUCCEED ();
}
#endif // LUMEX_HAS_STD_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS

// Needs std::ranges (not in GCC 8 -std=c++2a)
#if LUMEX_HAS_STD_RANGES
TEST (LumexSharedPtrCpp20Test,
      GivenTheRangesObjects_WhenComparing_ThenTheyWork)
{
  sp::shared_ptr<int> a (new int (1));
  sp::shared_ptr<int> b = a;
  sp::shared_ptr<int> c (new int (2));
  EXPECT_TRUE (std::ranges::equal_to () (a, b));
  EXPECT_FALSE (std::ranges::equal_to () (a, c));
  EXPECT_TRUE (std::ranges::less () (a, c) != std::ranges::less () (c, a));
  EXPECT_TRUE (std::ranges::greater_equal () (a, b));
  EXPECT_TRUE (std::ranges::not_equal_to () (a, c));
  std::vector<sp::shared_ptr<int>> v{ c, a, b };
  std::ranges::sort (v);
  EXPECT_TRUE (v[0] <= v[1]);
  EXPECT_TRUE (v[1] <= v[2]);
}
#endif // LUMEX_HAS_STD_RANGES

TEST (LumexSharedPtrCpp20Test,
      GivenReversedOperands_WhenComparingWithNullptr_ThenTheRewriteWorks)
{
  sp::shared_ptr<int> p (new int (1));
  sp::shared_ptr<int> empty;
  EXPECT_TRUE (nullptr != p);
  EXPECT_TRUE (nullptr == empty);
  EXPECT_TRUE (nullptr < p || nullptr > p || nullptr == p);
  EXPECT_FALSE (p <= nullptr && p >= nullptr && p != nullptr);
}

TEST (LumexSharedPtrCpp20Test,
      GivenAnRvalueAlias_WhenTheStandardHasIt_ThenBothFamiliesAgree)
{
  // C++20 added the aliasing constructor for an rvalue to std::shared_ptr; the
  // module has it on every standard (checked in the construction tests).
  std::shared_ptr<int> standard (new int (1));
  std::shared_ptr<int> alias (std::move (standard), nullptr);
// Needs the rvalue aliasing constructor of std::shared_ptr (not in libstdc++
// 8 with GCC 8 -std=c++2a; there is no feature-test macro for it)
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE >= 11
  EXPECT_EQ (standard.get (), nullptr);
#endif
  sp::shared_ptr<int> mine (new int (1));
  sp::shared_ptr<int> my_alias (std::move (mine), nullptr);
  EXPECT_EQ (mine.get (), nullptr);
  EXPECT_EQ (my_alias.use_count (), 1);
}
} // namespace

// The module declares nothing at global scope. A program that already has
// global templates named atomic_shared_ptr and atomic_weak_ptr (its own
// helpers, or another library's) must still compile when it includes
// lumex/core/atomic/LumexAtomic and uses both its templates and LumexLib's.
// A global declaration of either name in LumexLib makes this file fail to
// compile. The two global templates below stand in for that program, so they
// carry the names they must not clash with rather than this library's naming
// rules, and this file does not include the shared test support header,
// whose using-declarations would make the short names ambiguous.

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"

// The program's declarations, then its definitions.
template <typename T> struct atomic_shared_ptr;
template <typename T> struct atomic_weak_ptr;

template <typename T> struct atomic_shared_ptr
{
  std::shared_ptr<T> value;
  int generation;
};

template <typename T> struct atomic_weak_ptr
{
  std::weak_ptr<T> value;
};

namespace lumex_smart_ptr = lumex::core::atomic::smart_ptr;

TEST (LumexAtomicGlobalNamesTest,
      GivenGlobalTemplatesOfTheProgram_WhenNamed_ThenTheyAreNotLumexTypes)
{
  static_assert (!std::is_same<::atomic_shared_ptr<int>,
                               lumex_smart_ptr::atomic_shared_ptr<int>>::value,
                 "the global atomic_shared_ptr is the program's own");
  static_assert (!std::is_same<::atomic_weak_ptr<int>,
                               lumex_smart_ptr::atomic_weak_ptr<int>>::value,
                 "the global atomic_weak_ptr is the program's own");

  // Unqualified names outside any namespace find the program's templates.
  atomic_shared_ptr<int> plain_shared = { std::shared_ptr<int> (), 0 };
  atomic_weak_ptr<int> plain_weak = { std::weak_ptr<int> () };
  static_assert (
      std::is_same<decltype (plain_shared.value), std::shared_ptr<int>>::value,
      "the program's atomic_shared_ptr");
  static_assert (
      std::is_same<decltype (plain_weak.value), std::weak_ptr<int>>::value,
      "the program's atomic_weak_ptr");
  EXPECT_FALSE (plain_shared.value);
  EXPECT_TRUE (plain_weak.value.expired ());
}

TEST (LumexAtomicGlobalNamesTest,
      GivenBothSetsOfTemplates_WhenUsedTogether_ThenEachKeepsItsBehaviour)
{
  lumex_smart_ptr::atomic_shared_ptr<int> shared (std::make_shared<int> (7));
  lumex_smart_ptr::atomic_weak_ptr<int> weak;

  ::atomic_shared_ptr<int> mine = { shared.load (), 1 };
  weak.store (mine.value);
  ::atomic_weak_ptr<int> mine_weak = { weak.load () };

  ASSERT_TRUE (mine.value);
  EXPECT_EQ (*mine.value, 7);
  EXPECT_EQ (mine.generation, 1);
  EXPECT_EQ (mine.value.use_count (), 2) << "held by shared and by mine";
  ASSERT_FALSE (mine_weak.value.expired ());
  EXPECT_EQ (mine_weak.value.lock (), mine.value);

  shared.store (std::make_shared<int> (8));
  mine.value.reset ();
  EXPECT_TRUE (mine_weak.value.expired ())
      << "the old value lost its last owner";
  EXPECT_TRUE (weak.load ().expired ());
  EXPECT_EQ (*shared.load (), 8);
}

TEST (LumexAtomicGlobalNamesTest,
      GivenAUsingDeclarationInAnInnerScope_WhenNamed_ThenItHidesTheGlobalOnes)
{
  using lumex::core::atomic::smart_ptr::atomic_shared_ptr;
  using lumex::core::atomic::smart_ptr::atomic_weak_ptr;

  static_assert (std::is_same<atomic_shared_ptr<int>,
                              lumex_smart_ptr::atomic_shared_ptr<int>>::value,
                 "the using-declaration names LumexLib's template");
  static_assert (std::is_same<atomic_weak_ptr<int>,
                              lumex_smart_ptr::atomic_weak_ptr<int>>::value,
                 "the using-declaration names LumexLib's template");

  atomic_shared_ptr<int> shared (std::make_shared<int> (5));
  atomic_weak_ptr<int> weak (shared.load ());
  EXPECT_EQ (*shared.load (), 5);
  EXPECT_EQ (weak.load ().lock (), shared.load ());
}

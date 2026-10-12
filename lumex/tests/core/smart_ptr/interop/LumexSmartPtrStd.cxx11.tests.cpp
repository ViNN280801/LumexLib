// Tests of from_std and to_std: explicit wrapping between the module's
// shared_ptr and std::shared_ptr. The wrapper has its own counts; the two
// directions round trip to the original pointer; aliases and empty pointers
// are carried over; the objects are destroyed exactly once whichever side
// drops the last reference.

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

TEST (LumexSmartPtrStdTest,
      GivenAStdPointer_WhenWrapped_ThenTheObjectIsSharedWithOwnCounts)
{
  lumex_test::ObjectLedger ledger (2);
  std::shared_ptr<Probe> standard = std::make_shared<Probe> (ledger, 4);
  sp::shared_ptr<Probe> wrapped = sp::from_std (standard);
  EXPECT_EQ (wrapped.get (), standard.get ());
  EXPECT_EQ (wrapped->value, 4);
  // The wrapper holds one std reference; its own count is separate.
  EXPECT_EQ (standard.use_count (), 2);
  EXPECT_EQ (wrapped.use_count (), 1);
  sp::shared_ptr<Probe> copy = wrapped;
  EXPECT_EQ (wrapped.use_count (), 2);
  EXPECT_EQ (standard.use_count (), 2)
      << "lumex copies do not touch the std count";
  copy.reset ();
  wrapped.reset ();
  EXPECT_EQ (standard.use_count (), 1)
      << "the last lumex owner released its std reference";
  EXPECT_EQ (ledger.alive (), 1u);
  standard.reset ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSmartPtrStdTest,
      GivenARvalueStdPointer_WhenWrapped_ThenItIsTakenOver)
{
  std::shared_ptr<int> standard = std::make_shared<int> (5);
  std::weak_ptr<int> watcher = standard;
  sp::shared_ptr<int> wrapped = sp::from_std (std::move (standard));
  EXPECT_EQ (standard.get (), nullptr);
  EXPECT_EQ (watcher.use_count (), 1) << "no extra std reference";
  EXPECT_EQ (*wrapped, 5);
  wrapped.reset ();
  EXPECT_TRUE (watcher.expired ());
}

TEST (LumexSmartPtrStdTest,
      GivenALumexPointer_WhenWrapped_ThenTheStdPointerKeepsItAlive)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> mine = sp::make_shared<Probe> (ledger, 6);
  std::shared_ptr<Probe> standard = sp::to_std (mine);
  EXPECT_EQ (standard.get (), mine.get ());
  EXPECT_EQ (standard.use_count (), 1);
  EXPECT_EQ (mine.use_count (), 2) << "the wrapper holds one lumex reference";
  mine.reset ();
  EXPECT_EQ (ledger.alive (), 1u) << "the std pointer is enough";
  EXPECT_EQ (standard->value, 6);
  std::shared_ptr<Probe> another = standard;
  standard.reset ();
  EXPECT_EQ (ledger.alive (), 1u);
  another.reset ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSmartPtrStdTest,
      GivenARoundTrip_WhenUnwrapping_ThenTheOriginalComesBack)
{
  lumex_test::ObjectLedger ledger (2);
  std::shared_ptr<Probe> standard = std::make_shared<Probe> (ledger, 1);
  sp::shared_ptr<Probe> wrapped = sp::from_std (standard);
  std::shared_ptr<Probe> back = sp::to_std (wrapped);
  EXPECT_EQ (back, standard);
  EXPECT_FALSE (back.owner_before (standard) || standard.owner_before (back))
      << "the same std owner, not a wrapper of the wrapper";
  EXPECT_EQ (standard.use_count (), 3) << "standard, the wrapper block, back";

  sp::shared_ptr<Probe> mine = sp::make_shared<Probe> (ledger, 2);
  std::shared_ptr<Probe> as_std = sp::to_std (mine);
  sp::shared_ptr<Probe> again = sp::from_std (as_std);
  EXPECT_EQ (again, mine);
  EXPECT_FALSE (again.owner_before (mine) || mine.owner_before (again))
      << "the same lumex owner";
  EXPECT_EQ (mine.use_count (), 3) << "mine, the std deleter, again";
}

TEST (LumexSmartPtrStdTest,
      GivenAnAlias_WhenWrapped_ThenTheStoredPointerIsCarried)
{
  struct Pair
  {
    int first;
    int second;
  };
  std::shared_ptr<Pair> owner = std::make_shared<Pair> ();
  owner->first = 1;
  owner->second = 2;
  std::shared_ptr<int> std_alias (owner, &owner->second);
  sp::shared_ptr<int> wrapped = sp::from_std (std_alias);
  EXPECT_EQ (wrapped.get (), &owner->second);
  EXPECT_EQ (*wrapped, 2);

  sp::shared_ptr<Pair> mine = sp::make_shared<Pair> ();
  sp::shared_ptr<int> my_alias (mine, &mine->first);
  std::shared_ptr<int> as_std = sp::to_std (my_alias);
  EXPECT_EQ (as_std.get (), &mine->first);
  // An alias through the wrapper unwraps to an alias of the original owner.
  sp::shared_ptr<int> back = sp::from_std (as_std);
  EXPECT_EQ (back.get (), &mine->first);
  EXPECT_FALSE (back.owner_before (mine) || mine.owner_before (back));
  // A std alias made from a wrapped pointer unwraps with the new pointer.
  std::shared_ptr<Pair> wrapped_pair = sp::to_std (mine);
  std::shared_ptr<int> second_alias (wrapped_pair, &mine->second);
  sp::shared_ptr<int> unwrapped = sp::from_std (second_alias);
  EXPECT_EQ (unwrapped.get (), &mine->second);
  EXPECT_FALSE (unwrapped.owner_before (mine)
                || mine.owner_before (unwrapped));
}

TEST (LumexSmartPtrStdTest, GivenEmptyPointers_WhenWrapped_ThenTheyStayEmpty)
{
  sp::shared_ptr<int> empty = sp::from_std (std::shared_ptr<int> ());
  EXPECT_EQ (empty.use_count (), 0);
  EXPECT_EQ (empty.get (), nullptr);
  std::shared_ptr<int> std_empty = sp::to_std (sp::shared_ptr<int> ());
  EXPECT_EQ (std_empty.use_count (), 0);
  EXPECT_EQ (std_empty.get (), nullptr);
  // Aliases of empty owners keep their pointer.
  int stored = 3;
  std::shared_ptr<int> alias_of_empty (std::shared_ptr<int> (), &stored);
  sp::shared_ptr<int> wrapped = sp::from_std (alias_of_empty);
  EXPECT_EQ (wrapped.use_count (), 0);
  EXPECT_EQ (wrapped.get (), &stored);
  sp::shared_ptr<int> my_alias_of_empty (sp::shared_ptr<int> (), &stored);
  std::shared_ptr<int> as_std = sp::to_std (my_alias_of_empty);
  EXPECT_EQ (as_std.use_count (), 0);
  EXPECT_EQ (as_std.get (), &stored);
}

TEST (LumexSmartPtrStdTest,
      GivenConvertedPointers_WhenWrapped_ThenBaseAndDerivedAgree)
{
  std::atomic<int> live (0);
  {
    std::shared_ptr<Dog> dog = std::make_shared<Dog> (&live);
    std::shared_ptr<Animal> animal = dog;
    sp::shared_ptr<Animal> mine = sp::from_std (animal);
    sp::shared_ptr<Dog> mine_dog = sp::dynamic_pointer_cast<Dog> (mine);
    ASSERT_NE (mine_dog, nullptr);
    EXPECT_EQ (mine_dog->legs (), 4);
    EXPECT_EQ (live.load (), 1);
  }
  EXPECT_EQ (live.load (), 0);
}

TEST (LumexSmartPtrStdTest,
      GivenAWeakPointerOfAWrapper_WhenTheWrapperGoes_ThenItExpires)
{
  std::shared_ptr<int> standard = std::make_shared<int> (3);
  sp::shared_ptr<int> wrapped = sp::from_std (standard);
  sp::weak_ptr<int> watcher = wrapped;
  wrapped.reset ();
  EXPECT_TRUE (watcher.expired ())
      << "the lumex count ended, whatever std owners remain";
  EXPECT_EQ (standard.use_count (), 1);
}

TEST (LumexSmartPtrStdTest,
      GivenTheDeleterDestructionOrder_WhenEitherSideEnds_ThenTheObjectDiesOnce)
{
  lumex_test::ObjectLedger ledger (4);
  for (int order = 0; order < 2; ++order)
    {
      sp::shared_ptr<Probe> mine = sp::make_shared<Probe> (ledger, 1);
      std::shared_ptr<Probe> standard = sp::to_std (mine);
      sp::shared_ptr<Probe> back = sp::from_std (standard);
      if (order == 0)
        {
          mine.reset ();
          standard.reset ();
          back.reset ();
        }
      else
        {
          back.reset ();
          standard.reset ();
          mine.reset ();
        }
    }
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSmartPtrStdTest,
      GivenTheSignatures_WhenChecked_ThenNothingConvertsImplicitly)
{
  static_assert (
      !std::is_convertible<std::shared_ptr<int>, sp::shared_ptr<int>>::value,
      "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<int>, std::shared_ptr<int>>::value,
      "");
  static_assert (!std::is_constructible<sp::shared_ptr<int>,
                                        std::shared_ptr<int> const &>::value,
                 "");
  static_assert (!std::is_constructible<std::shared_ptr<int>,
                                        sp::shared_ptr<int> const &>::value,
                 "");
  static_assert (!std::is_constructible<sp::weak_ptr<int>,
                                        std::weak_ptr<int> const &>::value,
                 "");
  static_assert (!std::is_constructible<sp::weak_ptr<int>,
                                        std::shared_ptr<int> const &>::value,
                 "");
  static_assert (
      std::is_same<decltype (sp::from_std (
                       std::declval<std::shared_ptr<int> const &> ())),
                   sp::shared_ptr<int>>::value,
      "");
  static_assert (
      std::is_same<decltype (sp::to_std (
                       std::declval<sp::shared_ptr<int> const &> ())),
                   std::shared_ptr<int>>::value,
      "");
  SUCCEED ();
}
} // namespace

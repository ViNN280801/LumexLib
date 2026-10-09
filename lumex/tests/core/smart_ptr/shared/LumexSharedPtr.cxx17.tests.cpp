// C++17 additions of shared_ptr: the array forms shared_ptr<T[]> and
// shared_ptr<T[N]> ([util.smartptr.shared.general], element_type and
// operator[]), their constraints, weak_ptr and unique_ptr of arrays, the
// deduction guides and weak_type. The module has these on every standard; the
// suite for C++17 compares them with std::shared_ptr, which has them from
// C++17 on.

#include <atomic>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F>
void
scenario_array_basics (Trace &t)
{
  shared_of<F, int[]> a (new int[4]{ 1, 2, 3, 4 });
  t.note (a.use_count ());
  t.note (a[0]);
  t.note (a[3]);
  a[2] = 30;
  t.note (a[2]);
  shared_of<F, int[]> b = a;
  t.note (a.use_count ());
  t.note_bool (b.get () == a.get ());
  t.note_bool (static_cast<bool> (a));
  shared_of<F, int const[]> c = a;
  t.note (c[2]);
  t.note (a.use_count ());
  shared_of<F, int[]> empty;
  t.note_bool (!empty);
  t.note (empty.use_count ());
}

template <class F>
void
scenario_array_destruction (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Animal[]> a (new Animal[5]{ Animal (&live), Animal (&live),
                                             Animal (&live), Animal (&live),
                                             Animal (&live) });
    t.note (live.load ());
    shared_of<F, Animal[]> b = a;
    a.reset ();
    t.note (live.load ());
  }
  t.note (live.load ());
  {
    shared_of<F, Animal[3]> fixed (
        new Animal[3]{ Animal (&live), Animal (&live), Animal (&live) });
    t.note (live.load ());
    t.note (fixed.use_count ());
  }
  t.note (live.load ());
}

template <class F>
void
scenario_array_deleters (Trace &t)
{
  std::atomic<int> calls (0);
  {
    shared_of<F, int[]> a (new int[3],
                           [&calls] (int *p)
                             {
                               calls.fetch_add (1);
                               delete[] p;
                             });
    t.note (a.use_count ());
  }
  t.note (calls.load ());
  AllocStats stats;
  {
    shared_of<F, int[]> b (
        new int[3],
        [&calls] (int *p)
          {
            calls.fetch_add (1);
            delete[] p;
          },
        CountingAllocator<int> (&stats));
    t.note (stats.allocations.load ());
  }
  t.note (calls.load ());
  t.note (stats.deallocations.load ());
  {
    shared_of<F, int[]> c (nullptr,
                           [&calls] (std::nullptr_t) { calls.fetch_add (1); });
    t.note (c.use_count ());
    t.note_bool (c.get () == nullptr);
  }
  t.note (calls.load ());
}

template <class F>
void
scenario_array_conversions (Trace &t)
{
  shared_of<F, int[4]> fixed (new int[4]{ 1, 2, 3, 4 });
  shared_of<F, int[]> unbounded = fixed;
  t.note (fixed.use_count ());
  t.note (unbounded[3]);
  shared_of<F, int const[4]> const_fixed = fixed;
  t.note (const_fixed[0]);
  shared_of<F, int const[]> const_unbounded = std::move (unbounded);
  t.note (unbounded.use_count ());
  t.note (const_unbounded.use_count ());
  weak_of<F, int const[]> weak = const_unbounded;
  t.note (weak.use_count ());
  shared_of<F, int const[]> locked = weak.lock ();
  t.note_bool (locked.get () == fixed.get ());
  weak_of<F, int const[]> from_fixed = fixed;
  t.note (from_fixed.use_count ());
}

template <class F>
void
scenario_array_reset_and_unique (Trace &t)
{
  std::atomic<int> live (0);
  shared_of<F, Animal[]> a;
  a.reset (new Animal[2]{ Animal (&live), Animal (&live) });
  t.note (live.load ());
  a.reset (new Animal[3]{ Animal (&live), Animal (&live), Animal (&live) });
  t.note (live.load ());
  std::unique_ptr<Animal[]> u (
      new Animal[2]{ Animal (&live), Animal (&live) });
  t.note (live.load ());
  shared_of<F, Animal[]> from_unique (std::move (u));
  t.note_bool (u == nullptr);
  t.note (live.load ());
  a = std::move (from_unique);
  t.note (live.load ());
  a.reset ();
  t.note (live.load ());
}

template <class F>
void
scenario_array_aliasing_and_compare (Trace &t)
{
  shared_of<F, int[]> a (new int[4]{ 10, 20, 30, 40 });
  shared_of<F, int> second (a, &a[1]);
  t.note (a.use_count ());
  t.note (*second);
  t.note_bool (second.get () == a.get () + 1);
  shared_of<F, int[]> b = a;
  t.note_bool (a == b);
  t.note_bool (a != b);
  t.note_bool (a < b);
  t.note_bool (a == nullptr);
  std::hash<shared_of<F, int[]>> hasher;
  t.note_bool (hasher (a) == std::hash<int *> () (a.get ()));
  std::unordered_set<shared_of<F, int[]>> set;
  set.insert (a);
  set.insert (b);
  t.note (static_cast<long> (set.size ()));
}

TEST (LumexSharedPtrArrayTest, GivenUnboundedArrays_WhenUsed_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_array_basics);
}

TEST (LumexSharedPtrArrayTest,
      GivenArrays_WhenDestroyed_ThenEveryElementIsDestroyed)
{
  EXPECT_TRACES_EQUAL (scenario_array_destruction);
}

TEST (LumexSharedPtrArrayTest, GivenArrayDeleters_WhenOwned_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_array_deleters);
}

TEST (LumexSharedPtrArrayTest,
      GivenArrayConversions_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_array_conversions);
}

TEST (LumexSharedPtrArrayTest,
      GivenResetAndUniquePtr_WhenArrays_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_array_reset_and_unique);
}

TEST (LumexSharedPtrArrayTest,
      GivenAliasesAndComparisons_WhenArrays_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_array_aliasing_and_compare);
}

TEST (LumexSharedPtrArrayTest,
      GivenTheTypes_WhenChecked_ThenElementTypeAndOperatorsFollowTheArrayRules)
{
  static_assert (std::is_same<sp::shared_ptr<int[]>::element_type, int>::value,
                 "");
  static_assert (
      std::is_same<sp::shared_ptr<int[4]>::element_type, int>::value, "");
  static_assert (std::is_same<sp::shared_ptr<int const[]>::element_type,
                              int const>::value,
                 "");
  static_assert (std::is_same<sp::weak_ptr<int[]>::element_type, int>::value,
                 "");
  static_assert (std::is_same<sp::shared_ptr<int[]>::weak_type,
                              sp::weak_ptr<int[]>>::value,
                 "");
  static_assert (
      std::is_same<
          decltype (std::declval<sp::shared_ptr<int[]> const &> ()[0]),
          int &>::value,
      "");
  static_assert (
      !decltype (array_detect::deref<sp::shared_ptr<int[]>> (0))::value,
      "no operator* for arrays");
  static_assert (
      !decltype (array_detect::arrow<sp::shared_ptr<int[]>> (0))::value,
      "no operator-> for arrays");
  static_assert (
      decltype (array_detect::index<sp::shared_ptr<int[]>> (0))::value, "");
  static_assert (
      decltype (array_detect::index<sp::shared_ptr<int[4]>> (0))::value, "");
  static_assert (
      !decltype (array_detect::index<sp::shared_ptr<int>> (0))::value,
      "no operator[] for objects");
  static_assert (
      decltype (array_detect::deref<sp::shared_ptr<int>> (0))::value, "");
  static_assert (
      decltype (array_detect::arrow<sp::shared_ptr<Dog>> (0))::value, "");
  SUCCEED ();
}

TEST (LumexSharedPtrArrayTest,
      GivenTheConstraints_WhenArrays_ThenTheStandardConversionsOnlyExist)
{
  // From a raw pointer to the element.
  static_assert (std::is_constructible<sp::shared_ptr<int[]>, int *>::value,
                 "");
  static_assert (std::is_constructible<sp::shared_ptr<int[4]>, int *>::value,
                 "");
  static_assert (
      std::is_constructible<sp::shared_ptr<int const[]>, int *>::value, "");
  static_assert (
      !std::is_constructible<sp::shared_ptr<Animal[]>, Dog *>::value,
      "Dog (*)[] does not convert to Animal (*)[]");
  static_assert (
      !std::is_constructible<sp::shared_ptr<int[]>, double *>::value, "");
  static_assert (
      !std::is_constructible<sp::shared_ptr<int[]>, int const *>::value, "");
  // Between shared pointers.
  static_assert (std::is_convertible<sp::shared_ptr<int[4]>,
                                     sp::shared_ptr<int[]>>::value,
                 "");
  static_assert (std::is_convertible<sp::shared_ptr<int[]>,
                                     sp::shared_ptr<int const[]>>::value,
                 "");
  static_assert (std::is_convertible<sp::shared_ptr<int[4]>,
                                     sp::shared_ptr<int const[]>>::value,
                 "");
  static_assert (!std::is_convertible<sp::shared_ptr<int[]>,
                                      sp::shared_ptr<int[4]>>::value,
                 "");
  static_assert (!std::is_convertible<sp::shared_ptr<int[4]>,
                                      sp::shared_ptr<int[5]>>::value,
                 "");
  static_assert (!std::is_convertible<sp::shared_ptr<Dog[]>,
                                      sp::shared_ptr<Animal[]>>::value,
                 "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<int[]>, sp::shared_ptr<int>>::value,
      "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<int>, sp::shared_ptr<int[]>>::value,
      "");
  // unique_ptr and weak_ptr of arrays.
  static_assert (std::is_constructible<sp::shared_ptr<int[]>,
                                       std::unique_ptr<int[]> &&>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<int[]>,
                                        std::unique_ptr<int> &&>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<int>,
                                        std::unique_ptr<int[]> &&>::value,
                 "");
  static_assert (std::is_constructible<sp::shared_ptr<int[]>,
                                       sp::weak_ptr<int[4]> const &>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<int[4]>,
                                        sp::weak_ptr<int[]> const &>::value,
                 "");
  SUCCEED ();
}

TEST (LumexSharedPtrArrayTest,
      GivenAnArrayOfFixedSize_WhenIndexed_ThenTheElementsAreReachable)
{
  sp::shared_ptr<int[3]> fixed (new int[3]{ 7, 8, 9 });
  EXPECT_EQ (fixed[0], 7);
  EXPECT_EQ (fixed[2], 9);
  fixed[1] = 80;
  sp::shared_ptr<int[]> view = fixed;
  EXPECT_EQ (view[1], 80);
  EXPECT_EQ (fixed.use_count (), 2);
}

TEST (LumexSharedPtrArrayTest,
      GivenAnArrayOfLargeObjects_WhenReleased_ThenOnlyDeleteBracketsIsUsed)
{
  lumex_test::ObjectLedger ledger (8);
  // Probe has no default constructor, so the array is built with a helper
  // type that registers itself.
  struct Cell
  {
    Cell () : entry (*ledger_pointer ()) {}
    static lumex_test::ObjectLedger *&
    ledger_pointer ()
    {
      static lumex_test::ObjectLedger *current = nullptr;
      return current;
    }
    lumex_test::LedgerEntry entry;
  };
  Cell::ledger_pointer () = &ledger;
  {
    sp::shared_ptr<Cell[]> cells (new Cell[6]);
    EXPECT_EQ (ledger.alive (), 6u);
    sp::shared_ptr<Cell[]> copy = cells;
    cells.reset ();
    EXPECT_EQ (ledger.alive (), 6u);
  }
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
  Cell::ledger_pointer () = nullptr;
}

TEST (LumexSharedPtrArrayTest,
      GivenTheDeductionGuides_WhenConstructing_ThenTheTypeIsDeduced)
{
  sp::shared_ptr<int> owner (new int (3));
  sp::weak_ptr weak (owner);
  static_assert (std::is_same<decltype (weak), sp::weak_ptr<int>>::value, "");
  sp::shared_ptr promoted (weak);
  static_assert (std::is_same<decltype (promoted), sp::shared_ptr<int>>::value,
                 "");
  EXPECT_EQ (*promoted, 3);
  sp::shared_ptr from_unique (std::unique_ptr<double> (new double (2.5)));
  static_assert (
      std::is_same<decltype (from_unique), sp::shared_ptr<double>>::value, "");
  EXPECT_EQ (*from_unique, 2.5);
  sp::shared_ptr array_from_unique (
      std::unique_ptr<int[]> (new int[2]{ 1, 2 }));
  static_assert (
      std::is_same<decltype (array_from_unique), sp::shared_ptr<int[]>>::value,
      "");
  EXPECT_EQ (array_from_unique[1], 2);
}

TEST (
    LumexSharedPtrArrayTest,
    GivenStdEnableSharedFromThisAtC17_WhenWeakFromThis_ThenTheModuleBaseHasIt)
{
  struct Node : sp::enable_shared_from_this<Node>
  {
  };
  sp::shared_ptr<Node> p (new Node ());
  sp::weak_ptr<Node> w = p->weak_from_this ();
  EXPECT_EQ (w.lock (), p);
  Node const &constant = *p;
  sp::weak_ptr<Node const> cw = constant.weak_from_this ();
  EXPECT_EQ (cw.lock (), p);
  static_assert (noexcept (p->weak_from_this ()), "");
}
} // namespace

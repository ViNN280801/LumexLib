// Tests of the observers, the comparison operators, owner_before, the stream
// inserter and the hash of shared_ptr ([util.smartptr.shared.obs],
// [util.smartptr.shared.cmp], [util.smartptr.shared.io],
// [util.smartptr.hash]). The comparison of all pairs of a small set of
// pointers, with nullptr and with a pointer to const, is run on
// std::shared_ptr too.

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F>
void
scenario_comparison (Trace &t)
{
  static int values[4] = { 0, 1, 2, 3 };
  shared_of<F, int> owner (new int (0));
  std::vector<shared_of<F, int>> pointers;
  for (int i = 0; i < 4; ++i)
    pointers.push_back (shared_of<F, int> (owner, &values[i]));
  pointers.push_back (shared_of<F, int> ());
  for (std::size_t i = 0; i < pointers.size (); ++i)
    for (std::size_t j = 0; j < pointers.size (); ++j)
      {
        shared_of<F, int> const &a = pointers[i];
        shared_of<F, int> const &b = pointers[j];
        t.note_bool (a == b);
        t.note_bool (a != b);
        // Ordering of the null pointer against array elements is not
        // specified across unrelated objects; the owner is an alias of one
        // array, and null is ordered by std::less, which is a total order.
        t.note_bool (a < b);
        t.note_bool (a > b);
        t.note_bool (a <= b);
        t.note_bool (a >= b);
      }
  for (std::size_t i = 0; i < pointers.size (); ++i)
    {
      shared_of<F, int> const &a = pointers[i];
      t.note_bool (a == nullptr);
      t.note_bool (nullptr == a);
      t.note_bool (a != nullptr);
      t.note_bool (nullptr != a);
      t.note_bool (a < nullptr);
      t.note_bool (nullptr < a);
      t.note_bool (a > nullptr);
      t.note_bool (nullptr > a);
      t.note_bool (a <= nullptr);
      t.note_bool (nullptr <= a);
      t.note_bool (a >= nullptr);
      t.note_bool (nullptr >= a);
    }
  // A pointer to const against a pointer to non-const of the same object.
  shared_of<F, int const> constant (pointers[2]);
  t.note_bool (constant == pointers[2]);
  t.note_bool (pointers[2] == constant);
  t.note_bool (constant < pointers[3]);
  t.note_bool (pointers[1] < constant);
  t.note_bool (constant <= pointers[2]);
  t.note_bool (constant >= pointers[2]);
  // Comparison compares the stored pointers, not the owners.
  shared_of<F, int> other_owner (new int (9));
  shared_of<F, int> alias (other_owner, &values[1]);
  t.note_bool (alias == pointers[1]);
  t.note_bool (alias != pointers[1]);
}

template <class F>
void
scenario_derived_comparison (Trace &t)
{
  shared_of<F, Both> both (new Both ());
  shared_of<F, Left> left = both;
  shared_of<F, Right> right = both;
  // The same object through two bases: the stored pointers differ.
  t.note_bool (static_cast<void *> (left.get ())
               == static_cast<void *> (right.get ()));
  t.note_bool (left == both);
  t.note_bool (right == both);
  t.note_bool (both == left);
}

template <class F>
void
scenario_owner_before (Trace &t)
{
  shared_of<F, int> a (new int (1));
  shared_of<F, int> b (new int (2));
  shared_of<F, int> alias (a, b.get ());
  shared_of<F, int> a_copy = a;
  weak_of<F, int> wa = a;
  weak_of<F, int> wb = b;
  // The order is implementation-defined, so only its properties are noted:
  // irreflexive, antisymmetric between distinct owners, equivalence for
  // shared owners, the same answers for shared and weak operands.
  t.note_bool (a.owner_before (a));
  t.note_bool (a.owner_before (b) != b.owner_before (a));
  t.note_bool (a.owner_before (alias) || alias.owner_before (a));
  t.note_bool (a.owner_before (a_copy) || a_copy.owner_before (a));
  t.note_bool (a.owner_before (wa) || wa.owner_before (a));
  t.note_bool (a.owner_before (wb) == a.owner_before (b));
  t.note_bool (wa.owner_before (wb) == a.owner_before (b));
  t.note_bool (wb.owner_before (wa) == b.owner_before (a));
  t.note_bool (alias.owner_before (b) || b.owner_before (alias));
  shared_of<F, int> empty1;
  shared_of<F, int> empty2;
  t.note_bool (empty1.owner_before (empty2) || empty2.owner_before (empty1));
}

template <class F>
void
scenario_observers (Trace &t)
{
  struct Pair
  {
    int first;
    int second;
  };
  shared_of<F, Pair> p (new Pair{ 3, 4 });
  t.note (p->first);
  t.note ((*p).second);
  p->first = 10;
  t.note ((*p).first);
  t.note_bool (static_cast<bool> (p));
  t.note_bool (!p);
  if (p)
    t.note (1);
  else
    t.note (0);
  std::vector<shared_of<F, Pair>> copies (10, p);
  t.note (p.use_count ());
  copies.resize (4);
  t.note (p.use_count ());
  copies.clear ();
  t.note (p.use_count ());
  shared_of<F, Pair> empty;
  t.note_bool (!empty);
  t.note_bool (empty && p ? true : false);
  t.note_bool (p && p ? true : false);
  shared_of<F, void> erased = p;
  t.note (erased.use_count ());
  t.note_bool (erased.get () == static_cast<void *> (p.get ()));
}

template <class F>
void
scenario_hash_and_containers (Trace &t)
{
  shared_of<F, int> a (new int (1));
  shared_of<F, int> b (new int (2));
  std::hash<shared_of<F, int>> hasher;
  t.note_bool (hasher (a) == std::hash<int *> () (a.get ()));
  t.note_bool (hasher (b) == std::hash<int *> () (b.get ()));
  t.note_bool (hasher (shared_of<F, int> ()) == std::hash<int *> () (nullptr));
  shared_of<F, int> a2 = a;
  t.note_bool (hasher (a) == hasher (a2));
  std::unordered_set<shared_of<F, int>> set;
  set.insert (a);
  set.insert (a2);
  set.insert (b);
  set.insert (shared_of<F, int> ());
  t.note (static_cast<long> (set.size ()));
  t.note_bool (set.count (a) == 1);
  t.note (a.use_count ());
  std::set<shared_of<F, int>> ordered;
  ordered.insert (a);
  ordered.insert (b);
  ordered.insert (a2);
  t.note (static_cast<long> (ordered.size ()));
  // owner_less groups pointers by owner, not by stored pointer.
  typedef typename std::conditional<
      std::is_same<F, lumex_family>::value, sp::owner_less<shared_of<F, int>>,
      std::owner_less<shared_of<F, int>>>::type less_type;
  std::set<shared_of<F, int>, less_type> by_owner;
  by_owner.insert (a);
  by_owner.insert (shared_of<F, int> (a, b.get ()));
  by_owner.insert (b);
  t.note (static_cast<long> (by_owner.size ()));
}

template <class F>
void
scenario_stream (Trace &t)
{
  shared_of<F, int> a (new int (1));
  std::ostringstream via_ptr;
  via_ptr << a;
  std::ostringstream via_raw;
  via_raw << a.get ();
  t.note_bool (via_ptr.str () == via_raw.str ());
  std::ostringstream null_stream;
  null_stream << shared_of<F, int> ();
  std::ostringstream null_raw;
  null_raw << static_cast<int *> (nullptr);
  t.note_bool (null_stream.str () == null_raw.str ());
  std::wostringstream wide;
  wide << a;
  t.note_bool (!wide.str ().empty ());
}

TEST (LumexSharedPtrObserversTest,
      GivenAllPairsOfPointers_WhenComparing_ThenTheResultsMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_comparison);
}

TEST (
    LumexSharedPtrObserversTest,
    GivenBasesAtDifferentOffsets_WhenComparing_ThenTheyCompareAsTheSameObject)
{
  EXPECT_TRACES_EQUAL (scenario_derived_comparison);
  sp::shared_ptr<Both> both (new Both ());
  sp::shared_ptr<Right> right = both;
  sp::shared_ptr<Left> left = both;
  EXPECT_TRUE (both == left);
  EXPECT_TRUE (both == right);
  EXPECT_TRUE (right == both);
  EXPECT_FALSE (both != right);
#if !defined(__cpp_impl_three_way_comparison)
  // Before C++20 the order converts both pointers to their common type. From
  // C++20 the standard compares with std::compare_three_way, which orders
  // pointers of different types by address (libstdc++ does it for
  // std::shared_ptr too), so only equality is promised across offsets there.
  EXPECT_FALSE (both < right);
  EXPECT_FALSE (both > right);
  EXPECT_TRUE (both <= right);
  EXPECT_TRUE (both >= right);
#endif
}

TEST (LumexSharedPtrObserversTest,
      GivenOwnerBefore_WhenOrdering_ThenItIsAStrictWeakOrderOfOwners)
{
  EXPECT_TRACES_EQUAL (scenario_owner_before);
  sp::shared_ptr<int> a (new int (1));
  sp::shared_ptr<int> b (new int (2));
  sp::shared_ptr<int> alias (a, b.get ());
  // The alias is equivalent to a (same owner), not to b (same pointer).
  EXPECT_FALSE (a.owner_before (alias));
  EXPECT_FALSE (alias.owner_before (a));
  EXPECT_TRUE (a.owner_before (b) != b.owner_before (a));
  EXPECT_TRUE (alias.owner_before (b) != b.owner_before (alias));
  // Transitivity over three owners.
  sp::shared_ptr<int> c (new int (3));
  sp::shared_ptr<int> three[3] = { a, b, c };
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k)
        {
          bool const chain = three[i].owner_before (three[j])
                             && three[j].owner_before (three[k]);
          EXPECT_TRUE (!chain || three[i].owner_before (three[k]));
        }
}

TEST (LumexSharedPtrObserversTest, GivenTheObservers_WhenUsed_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_observers);
}

TEST (LumexSharedPtrObserversTest,
      GivenTheHash_WhenUsedInContainers_ThenItIsTheHashOfTheStoredPointer)
{
  EXPECT_TRACES_EQUAL (scenario_hash_and_containers);
  sp::shared_ptr<int> a (new int (1));
  EXPECT_EQ (std::hash<sp::shared_ptr<int>> () (a),
             std::hash<int *> () (a.get ()));
}

TEST (LumexSharedPtrObserversTest,
      GivenTheInserter_WhenStreaming_ThenItWritesTheStoredPointer)
{
  EXPECT_TRACES_EQUAL (scenario_stream);
}

TEST (LumexSharedPtrObserversTest,
      GivenTheHashAndComparisonTraits_WhenChecked_ThenTheSignaturesAreRight)
{
  typedef sp::shared_ptr<int> ptr;
  static_assert (noexcept (std::declval<ptr const &> ().get ()), "");
  static_assert (noexcept (std::declval<ptr const &> ().use_count ()), "");
  static_assert (noexcept (static_cast<bool> (std::declval<ptr const &> ())),
                 "");
  static_assert (
      noexcept (std::declval<ptr const &> () == std::declval<ptr const &> ()),
      "");
  static_assert (
      noexcept (std::declval<ptr const &> () < std::declval<ptr const &> ()),
      "");
  static_assert (noexcept (std::declval<ptr const &> ().owner_before (
                     std::declval<ptr const &> ())),
                 "");
  static_assert (
      std::is_same<decltype (std::declval<ptr const &> ().use_count ()),
                   long>::value,
      "use_count returns long");
  static_assert (std::is_same<decltype (std::declval<ptr const &> ().get ()),
                              int *>::value,
                 "");
  static_assert (std::is_same<ptr::element_type, int>::value, "");
  static_assert (!std::is_convertible<ptr, bool>::value,
                 "operator bool is explicit");
  static_assert (std::is_constructible<bool, ptr>::value, "");
  static_assert (
      std::is_same<decltype (*std::declval<ptr const &> ()), int &>::value,
      "");
  static_assert (
      std::is_same<
          decltype (*std::declval<sp::shared_ptr<int const> const &> ()),
          int const &>::value,
      "");
  SUCCEED ();
}

TEST (LumexSharedPtrObserversTest,
      GivenVoid_WhenDereferencing_ThenTheOperatorsAreNotAvailable)
{
  // shared_ptr<void> has get () and the observers but no operator*.
  typedef sp::shared_ptr<void> ptr;
  static_assert (std::is_same<ptr::element_type, void>::value, "");
  static_assert (std::is_same<decltype (std::declval<ptr const &> ().get ()),
                              void *>::value,
                 "");
  static_assert (!decltype (deref_detect::test<ptr> (0))::value,
                 "operator* of shared_ptr<void> must not exist");
  static_assert (decltype (deref_detect::test<sp::shared_ptr<int>> (0))::value,
                 "operator* of shared_ptr<int> exists");
  SUCCEED ();
}

TEST (LumexSharedPtrObserversTest,
      GivenConstness_WhenDereferencing_ThenTheConstnessOfTheElementIsKept)
{
  sp::shared_ptr<int const> constant (new int (4));
  EXPECT_EQ (*constant, 4);
  sp::shared_ptr<int> const mutable_element (new int (5));
  *mutable_element = 6; // a const shared_ptr still gives a mutable element
  EXPECT_EQ (*mutable_element, 6);
}
} // namespace

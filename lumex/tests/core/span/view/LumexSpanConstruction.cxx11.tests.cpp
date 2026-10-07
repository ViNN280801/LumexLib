// LumexSpanConstruction.cxx11.tests.cpp
//
// Every way to make a span: default, pointer and count, pointer pair, array,
// std::array, contiguous range, initializer_list object, copy and conversion
// from another span; the elements it must not accept; the implicit and
// explicit forms.
#include <array>
#include <cstddef>
#include <deque>
#include <initializer_list>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;
using lumex_span_test::base_element;
using lumex_span_test::count_sentinel;
using lumex_span_test::derived_element;
using lumex_span_test::guarded_iterator;
using lumex_span_test::is_explicit_only;
using lumex_span_test::is_implicitly_listable;
using lumex_span_test::pointer_iterator;
using lumex_span_test::unregistered_iterator;

namespace
{
int
sum_of (span<int const> view)
{
  int total = 0;
  for (std::size_t index = 0; index < view.size (); ++index)
    {
      total += view[index];
    }
  return total;
}

std::size_t
size_of_dynamic (span<int> view)
{
  return view.size ();
}
} // namespace

// -- Default constructor --

TEST (LumexSpanConstructionTest, GivenDynamicExtent_WhenDefault_ThenEmptyNull)
{
  span<int> const view;
  EXPECT_EQ (view.size (), 0u);
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), nullptr);
  EXPECT_EQ (view.begin (), view.end ());
}

TEST (LumexSpanConstructionTest, GivenZeroExtent_WhenDefault_ThenEmptyNull)
{
  span<int, 0> const view;
  EXPECT_EQ (view.size (), 0u);
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), nullptr);
}

TEST (LumexSpanConstructionTest,
      GivenStaticExtent_WhenDefaultConstructible_ThenNo)
{
  static_assert (std::is_default_constructible<span<int>>::value,
                 "dynamic extent");
  static_assert (std::is_default_constructible<span<int, 0>>::value,
                 "zero extent");
  static_assert (!std::is_default_constructible<span<int, 1>>::value,
                 "static extent 1 has no default");
  static_assert (!std::is_default_constructible<span<int, 3>>::value,
                 "static extent 3 has no default");
  static_assert (std::is_nothrow_default_constructible<span<int>>::value,
                 "default is noexcept");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenValueInitialization_WhenBraces_ThenEmpty)
{
  span<int> const view{};
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), nullptr);
  std::vector<span<int>> const many (3);
  for (std::size_t index = 0; index < many.size (); ++index)
    {
      EXPECT_EQ (many[index].size (), 0u);
    }
}

// -- Pointer and count --

TEST (LumexSpanConstructionTest,
      GivenPointerAndCount_WhenDynamic_ThenViewsThem)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (values, 3);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values);
  EXPECT_EQ (view[2], 3);
}

TEST (LumexSpanConstructionTest,
      GivenPointerAndCount_WhenStaticExtent_ThenViewsThem)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int, 4> const view (values, 4);
  EXPECT_EQ (view.size (), 4u);
  EXPECT_EQ (view.data (), values);
  EXPECT_EQ (view.back (), 4);
}

TEST (LumexSpanConstructionTest, GivenNullPointerAndZero_WhenDynamic_ThenEmpty)
{
  span<int> const view (static_cast<int *> (nullptr), 0);
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), nullptr);
}

TEST (LumexSpanConstructionTest,
      GivenCountLiteralZero_WhenPointerAndCount_ThenCountForm)
{
  int values[2] = { 1, 2 };
  span<int> const view (values, 0);
  EXPECT_EQ (view.size (), 0u);
  EXPECT_EQ (view.data (), values);
}

TEST (LumexSpanConstructionTest,
      GivenCountOfOtherIntegerTypes_WhenPointerAndCount_ThenCountForm)
{
  int values[4] = { 1, 2, 3, 4 };
  EXPECT_EQ ((span<int> (values, 2u)).size (), 2u);
  EXPECT_EQ ((span<int> (values, 3ul)).size (), 3u);
  EXPECT_EQ ((span<int> (values, static_cast<unsigned char> (1))).size (), 1u);
  EXPECT_EQ ((span<int> (values, 4)).size (), 4u);
}

TEST (LumexSpanConstructionTest,
      GivenConstElement_WhenPointerToNonConst_ThenQualificationConversion)
{
  int values[3] = { 1, 2, 3 };
  span<int const> const view (values, 3);
  EXPECT_EQ (view.data (), values);
  static_assert (
      std::is_constructible<span<int const>, int *, std::size_t>::value,
      "int * to int const *");
  static_assert (
      std::is_constructible<span<int const>, int const *, std::size_t>::value,
      "int const * to int const *");
  static_assert (
      !std::is_constructible<span<int>, int const *, std::size_t>::value,
      "int const * must not become int *");
  static_assert (std::is_constructible<span<int const volatile>, int *,
                                       std::size_t>::value,
                 "int * to int const volatile *");
  static_assert (
      !std::is_constructible<span<int>, int volatile *, std::size_t>::value,
      "volatile must not be dropped");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenDerivedPointer_WhenBaseSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<base_element>, derived_element *,
                                        std::size_t>::value,
                 "a span of Base must not view an array of Derived");
  static_assert (!std::is_constructible<span<base_element const>,
                                        derived_element *, std::size_t>::value,
                 "not even const");
  static_assert (!std::is_constructible<span<char>, int *, std::size_t>::value,
                 "unrelated pointers");
  static_assert (
      !std::is_constructible<span<unsigned char>, char *, std::size_t>::value,
      "signedness differs");
  static_assert (!std::is_constructible<span<int>, void *, std::size_t>::value,
                 "void * has no element type");
  static_assert (
      !std::is_constructible<span<int>, std::nullptr_t, std::size_t>::value,
      "nullptr_t is not a pointer to an element");
  SUCCEED ();
}

TEST (
    LumexSpanConstructionTest,
    GivenDynamicAndStaticExtent_WhenPointerAndCount_ThenExplicitnessMatchesStandard)
{
  static_assert (is_implicitly_listable<span<int>, int *, std::size_t>::value,
                 "dynamic extent: implicit");
  static_assert (
      !is_implicitly_listable<span<int, 2>, int *, std::size_t>::value,
      "static extent: explicit");
  static_assert (
      std::is_constructible<span<int, 2>, int *, std::size_t>::value,
      "static extent: direct-initialization works");
  int values[2] = { 1, 2 };
  EXPECT_EQ (size_of_dynamic ({ values, 2 }), 2u);
}

// -- Pointer pair --

TEST (LumexSpanConstructionTest, GivenPointerPair_WhenDynamic_ThenViewsRange)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int> const view (values + 1, values + 4);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values + 1);
  EXPECT_EQ (view.front (), 2);
  EXPECT_EQ (view.back (), 4);
}

TEST (LumexSpanConstructionTest,
      GivenPointerPair_WhenStaticExtent_ThenViewsRange)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 3> const view (values + 1, values + 4);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values + 1);
}

TEST (LumexSpanConstructionTest, GivenEmptyPointerPair_WhenSpan_ThenEmpty)
{
  int values[2] = { 1, 2 };
  span<int> const view (values, values);
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), values);
}

TEST (LumexSpanConstructionTest,
      GivenMixedConstPointers_WhenPair_ThenSentinelMayDiffer)
{
  int values[3] = { 1, 2, 3 };
  int *const first = values;
  int const *const last = values + 3;
  span<int const> const view (first, last);
  EXPECT_EQ (view.size (), 3u);
  static_assert (
      std::is_constructible<span<int const>, int *, int const *>::value,
      "a const sentinel pointer is allowed");
  static_assert (
      !std::is_constructible<span<int>, int const *, int const *>::value,
      "const elements must not become non-const");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenPairAndCountForms_WhenSecondArgumentIsInteger_ThenNoAmbiguity)
{
  int values[4] = { 1, 2, 3, 4 };
  EXPECT_EQ ((span<int> (values, 2)).size (), 2u);
  EXPECT_EQ ((span<int> (values, values + 2)).size (), 2u);
  static_assert (!std::is_constructible<span<int>, int *, char *>::value,
                 "pointers to unrelated types do not make a range");
  SUCCEED ();
}

TEST (
    LumexSpanConstructionTest,
    GivenDynamicAndStaticExtent_WhenPointerPair_ThenExplicitnessMatchesStandard)
{
  static_assert (is_implicitly_listable<span<int>, int *, int *>::value,
                 "dynamic extent: implicit");
  static_assert (!is_implicitly_listable<span<int, 2>, int *, int *>::value,
                 "static extent: explicit");
  static_assert (std::is_constructible<span<int, 2>, int *, int *>::value,
                 "static extent: direct-initialization works");
  SUCCEED ();
}

// -- Iterator classes --

TEST (LumexSpanConstructionTest,
      GivenRegisteredIteratorClass_WhenCount_ThenViewsRange)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (pointer_iterator<int> (values + 1), 2);
  EXPECT_EQ (view.size (), 2u);
  EXPECT_EQ (view.data (), values + 1);
}

TEST (LumexSpanConstructionTest,
      GivenRegisteredIteratorClass_WhenPair_ThenViewsRange)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (pointer_iterator<int> (values),
                        pointer_iterator<int> (values + 3));
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values);
}

TEST (
    LumexSpanConstructionTest,
    GivenRegisteredIteratorOfNonConst_WhenConstSpan_ThenQualificationConversion)
{
  int values[3] = { 1, 2, 3 };
  span<int const> const view (pointer_iterator<int> (values), 3);
  EXPECT_EQ (view.data (), values);
  static_assert (!std::is_constructible<span<int>, pointer_iterator<int const>,
                                        std::size_t>::value,
                 "const elements must not become non-const");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenEmptyRangeOfIteratorClass_WhenSpan_ThenSizeZero)
{
  int values[2] = { 1, 2 };
  span<int> const view{ pointer_iterator<int> (values),
                        pointer_iterator<int> (values) };
  EXPECT_TRUE (view.empty ());
}

TEST (LumexSpanConstructionTest,
      GivenEmptyRangeOfGuardedIterator_WhenSpan_ThenIteratorIsNotDereferenced)
{
  int values[2] = { 1, 2 };
  // operator* of the iterator throws at the end of its range, as a checked
  // iterator does: an empty range must be viewed without a dereference.
  guarded_iterator<int> const end_of_values (values + 2, values + 2);
  EXPECT_NO_THROW ((span<int> (end_of_values, 0)));
  EXPECT_NO_THROW ((span<int> (end_of_values, end_of_values)));
  span<int> const empty_view (end_of_values, 0);
  EXPECT_TRUE (empty_view.empty ());
  guarded_iterator<int> const first (values, values + 2);
  span<int> const view (first, 2);
  EXPECT_EQ (view.data (), values);
  EXPECT_EQ (view.size (), 2u);
}

TEST (LumexSpanConstructionTest,
      GivenSentinelThatIsACount_WhenTwoArguments_ThenTheCountFormIsUsed)
{
  int values[4] = { 1, 2, 3, 4 };
  count_sentinel const count = { 3 };
  span<int> const view (pointer_iterator<int> (values), count);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values);
}

TEST (LumexSpanConstructionTest,
      GivenUnregisteredIteratorClass_WhenSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<int>, unregistered_iterator<int>,
                                        std::size_t>::value,
                 "an iterator class is accepted only when registered");
  static_assert (!std::is_constructible<span<int>, unregistered_iterator<int>,
                                        unregistered_iterator<int>>::value,
                 "also as a pair");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenContainerIterators_WhenSpan_ThenRejectedOrRegistered)
{
  static_assert (!std::is_constructible<span<int>, std::list<int>::iterator,
                                        std::size_t>::value,
                 "a list iterator is not contiguous");
  static_assert (!std::is_constructible<span<int>, std::deque<int>::iterator,
                                        std::size_t>::value,
                 "a deque iterator is not contiguous");
  static_assert (!std::is_constructible<span<int>, std::vector<int>::iterator,
                                        std::list<int>::iterator>::value,
                 "mixed iterator types are not a range");
  SUCCEED ();
}

// -- Built-in arrays --

TEST (LumexSpanConstructionTest, GivenBuiltInArray_WhenDynamic_ThenViewsAll)
{
  int values[3] = { 4, 5, 6 };
  span<int> const view (values);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values);
}

TEST (LumexSpanConstructionTest,
      GivenBuiltInArray_WhenMatchingStaticExtent_ThenImplicit)
{
  int values[3] = { 4, 5, 6 };
  span<int, 3> const view = values;
  EXPECT_EQ (view.size (), 3u);
  static_assert (std::is_convertible<int (&)[3], span<int, 3>>::value,
                 "implicit for a matching extent");
  static_assert (std::is_convertible<int (&)[3], span<int>>::value,
                 "implicit for a dynamic extent");
}

TEST (LumexSpanConstructionTest,
      GivenBuiltInArray_WhenOtherStaticExtent_ThenRejected)
{
  static_assert (!std::is_constructible<span<int, 4>, int (&)[3]>::value,
                 "extent 4 from an array of 3");
  static_assert (!std::is_constructible<span<int, 2>, int (&)[3]>::value,
                 "extent 2 from an array of 3");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenBuiltInArray_WhenConstQualified_ThenOnlyConstSpan)
{
  int const values[3] = { 4, 5, 6 };
  span<int const> const view (values);
  EXPECT_EQ (view.size (), 3u);
  static_assert (!std::is_constructible<span<int>, int const (&)[3]>::value,
                 "a const array must not give a non-const span");
  static_assert (std::is_constructible<span<int const>, int (&)[3]>::value,
                 "a non-const array gives a const span");
  static_assert (
      std::is_constructible<span<int const>, int const (&)[3]>::value,
      "a const array gives a const span");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenCharArrayLiteral_WhenSpanOfConstChar_ThenTerminatorIncluded)
{
  span<char const> const view ("abc");
  EXPECT_EQ (view.size (), 4u);
  EXPECT_EQ (view.back (), '\0');
}

TEST (LumexSpanConstructionTest, GivenArrayOfDerived_WhenBaseSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<base_element>,
                                        derived_element (&)[2]>::value,
                 "an array of Derived is not an array of Base");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenRvalueArray_WhenMutableSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<int>, int (&&)[3]>::value,
                 "an array that is about to die is not viewed mutably");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenArrayOfArrays_WhenSpan_ThenRowsAreElements)
{
  int matrix[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  span<int[3]> const rows (matrix);
  EXPECT_EQ (rows.size (), 2u);
  EXPECT_EQ (rows[1][2], 6);
}

// -- std::array --

TEST (LumexSpanConstructionTest, GivenStdArray_WhenDynamic_ThenViewsAll)
{
  std::array<int, 4> values = { { 1, 2, 3, 4 } };
  span<int> const view (values);
  EXPECT_EQ (view.size (), 4u);
  EXPECT_EQ (view.data (), values.data ());
}

TEST (LumexSpanConstructionTest,
      GivenStdArray_WhenMatchingStaticExtent_ThenImplicit)
{
  std::array<int, 4> values = { { 1, 2, 3, 4 } };
  span<int, 4> const view = values;
  EXPECT_EQ (view.size (), 4u);
  static_assert (
      std::is_convertible<std::array<int, 4> &, span<int, 4>>::value,
      "implicit for a matching extent");
  static_assert (
      !std::is_constructible<span<int, 3>, std::array<int, 4> &>::value,
      "other static extent");
  static_assert (
      !std::is_constructible<span<int, 5>, std::array<int, 4> &>::value,
      "other static extent");
}

TEST (LumexSpanConstructionTest,
      GivenConstStdArray_WhenSpan_ThenOnlyConstElements)
{
  std::array<int, 3> const values = { { 1, 2, 3 } };
  span<int const> const view (values);
  EXPECT_EQ (view.size (), 3u);
  static_assert (
      !std::is_constructible<span<int>, std::array<int, 3> const &>::value,
      "a const array must not give a non-const span");
  static_assert (
      std::is_constructible<span<int const>, std::array<int, 3> &>::value,
      "a non-const array gives a const span");
  static_assert (std::is_constructible<span<int const>,
                                       std::array<int, 3> const &>::value,
                 "a const array gives a const span");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenStdArrayOfConst_WhenSpan_ThenConstSpanOnly)
{
  std::array<int const, 2> values = { { 7, 8 } };
  span<int const> const view (values);
  EXPECT_EQ (view[1], 8);
  static_assert (
      !std::is_constructible<span<int>, std::array<int const, 2> &>::value,
      "elements are const");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenEmptyStdArray_WhenSpan_ThenEmpty)
{
  std::array<int, 0> values;
  span<int> const view (values);
  EXPECT_TRUE (view.empty ());
  span<int, 0> const fixed (values);
  EXPECT_TRUE (fixed.empty ());
}

TEST (LumexSpanConstructionTest,
      GivenStdArrayOfDerived_WhenBaseSpan_ThenRejected)
{
  static_assert (
      !std::is_constructible<span<base_element>,
                             std::array<derived_element, 2> &>::value,
      "an array of Derived is not an array of Base");
  SUCCEED ();
}

// -- Contiguous ranges --

TEST (LumexSpanConstructionTest, GivenVector_WhenSpan_ThenViewsItsStorage)
{
  std::vector<int> values = { 1, 2, 3 };
  span<int> const view (values);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values.data ());
  view[0] = 9;
  EXPECT_EQ (values[0], 9);
}

TEST (LumexSpanConstructionTest,
      GivenConstVector_WhenSpan_ThenOnlyConstElements)
{
  std::vector<int> const values = { 1, 2, 3 };
  span<int const> const view (values);
  EXPECT_EQ (view.size (), 3u);
  static_assert (
      !std::is_constructible<span<int>, std::vector<int> const &>::value,
      "a const vector must not give a non-const span");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenEmptyVector_WhenSpan_ThenSizeZero)
{
  std::vector<int> values;
  span<int> const view (values);
  EXPECT_EQ (view.size (), 0u);
  EXPECT_TRUE (view.empty ());
}

TEST (LumexSpanConstructionTest,
      GivenRvalueContainer_WhenSpan_ThenOnlyConstElements)
{
  static_assert (!std::is_constructible<span<int>, std::vector<int>>::value,
                 "an rvalue vector must not give a mutable span");
  static_assert (
      std::is_constructible<span<int const>, std::vector<int>>::value,
      "an rvalue vector may give a const span (function argument)");
  static_assert (!std::is_constructible<span<int>, std::vector<int> &&>::value,
                 "the same with an explicit rvalue reference");
  EXPECT_EQ (sum_of (std::vector<int>{ 1, 2, 3 }), 6);
}

TEST (LumexSpanConstructionTest,
      GivenString_WhenSpanOfConstChar_ThenViewsItsCharacters)
{
  std::string const text = "hello";
  span<char const> const view (text);
  EXPECT_EQ (view.size (), 5u);
  EXPECT_EQ (view.data (), text.data ());
  static_assert (
      !std::is_constructible<span<char>, std::string const &>::value,
      "a const string must not give a mutable span");
}

TEST (LumexSpanConstructionTest,
      GivenInitializerListLvalue_WhenSpanOfConst_ThenViewsIt)
{
  std::initializer_list<int> const list = { 5, 6, 7 };
  span<int const> const view (list);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), list.begin ());
  static_assert (
      !std::is_constructible<span<int>, std::initializer_list<int> &>::value,
      "the elements of a list are const");
}

TEST (LumexSpanConstructionTest,
      GivenNonContiguousContainers_WhenSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<int>, std::deque<int> &>::value,
                 "a deque is not contiguous");
  static_assert (
      !std::is_constructible<span<int const>, std::deque<int> &>::value,
      "not even to const");
  static_assert (!std::is_constructible<span<int>, std::list<int> &>::value,
                 "a list is not contiguous");
  static_assert (
      !std::is_constructible<span<bool>, std::vector<bool> &>::value,
      "std::vector<bool> is not contiguous");
  static_assert (
      !std::is_constructible<span<bool const>, std::vector<bool> &>::value,
      "std::vector<bool> is not contiguous, also to const");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenRangeOfDerived_WhenBaseSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<base_element>,
                                        std::vector<derived_element> &>::value,
                 "a vector of Derived is not a range of Base");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenDynamicAndStaticExtent_WhenRange_ThenExplicitnessMatchesStandard)
{
  static_assert (std::is_convertible<std::vector<int> &, span<int>>::value,
                 "dynamic extent: implicit");
  static_assert (is_explicit_only<span<int, 3>, std::vector<int> &>::value,
                 "static extent: explicit");
  static_assert (
      std::is_convertible<std::vector<int> &, span<int const>>::value,
      "dynamic extent, const elements: implicit");
  static_assert (
      is_explicit_only<span<int const, 3>, std::vector<int> &>::value,
      "static extent, const elements: explicit");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenStaticExtentFromRange_WhenDirectInitialization_ThenViewsIt)
{
  std::vector<int> values = { 1, 2, 3 };
  span<int, 3> const view (values);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values.data ());
}

namespace
{
// A range with data () and size () that is not a std container, with the data
// pointer one past a non-const element: checks that only the members matter.
struct custom_buffer
{
  int storage[4];
  int *
  data ()
  {
    return storage;
  }
  int const *
  data () const
  {
    return storage;
  }
  std::size_t
  size () const
  {
    return 4;
  }
};

// Has data () but no size (): not a sized range.
struct data_only
{
  int storage[2];
  int *
  data ()
  {
    return storage;
  }
};

// Has size () but no data (): not contiguous.
struct size_only
{
  std::size_t
  size () const
  {
    return 2;
  }
};

// data () returns something that is not a pointer.
struct data_not_pointer
{
  int
  data () const
  {
    return 0;
  }
  std::size_t
  size () const
  {
    return 1;
  }
};
} // namespace

TEST (LumexSpanConstructionTest,
      GivenCustomRangeWithDataAndSize_WhenSpan_ThenViewsIt)
{
  custom_buffer buffer = { { 1, 2, 3, 4 } };
  span<int> const view (buffer);
  EXPECT_EQ (view.size (), 4u);
  EXPECT_EQ (view.data (), buffer.storage);
  custom_buffer const &read_only = buffer;
  span<int const> const const_view (read_only);
  EXPECT_EQ (const_view.size (), 4u);
}

TEST (LumexSpanConstructionTest,
      GivenTypesMissingAMember_WhenSpan_ThenRejected)
{
  static_assert (!std::is_constructible<span<int>, data_only &>::value,
                 "no size ()");
  static_assert (!std::is_constructible<span<int>, size_only &>::value,
                 "no data ()");
  static_assert (!std::is_constructible<span<int>, data_not_pointer &>::value,
                 "data () is not a pointer");
  static_assert (!std::is_constructible<span<int>, int &>::value, "an int");
  static_assert (!std::is_constructible<span<int>, int>::value,
                 "an int value");
  static_assert (!std::is_constructible<span<int>, std::nullptr_t>::value,
                 "nullptr");
  static_assert (!std::is_constructible<span<int>, int *>::value,
                 "a pointer alone is not a range");
  SUCCEED ();
}

// -- Initializer lists --

TEST (LumexSpanConstructionTest,
      GivenInitializerListObject_WhenFunctionArgumentOfConstSpan_ThenViewsIt)
{
  EXPECT_EQ (sum_of (std::initializer_list<int>{ 1, 2, 3, 4 }), 10);
  EXPECT_EQ (sum_of (std::initializer_list<int>{ 7 }), 7);
  EXPECT_EQ (sum_of (std::initializer_list<int>{}), 0);
}

TEST (LumexSpanConstructionTest, GivenBracedList_WhenSpan_ThenNotConvertible)
{
  // The constructor from a braced list of C++26 is not provided: a call such
  // as f ({1, 2}) stays unambiguous next to an overload for a std::vector.
  static_assert (!is_implicitly_listable<span<int const>, int, int>::value,
                 "two integers do not make a span");
  static_assert (!is_implicitly_listable<span<int const>, int, int *>::value,
                 "an integer and a pointer do not make a span");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenInitializerList_WhenConstructibility_ThenConstElementsOnly)
{
  static_assert (std::is_constructible<span<int const>,
                                       std::initializer_list<int>>::value,
                 "const elements");
  static_assert (
      std::is_convertible<std::initializer_list<int>, span<int const>>::value,
      "implicit for a dynamic extent");
  static_assert (
      !std::is_constructible<span<int>, std::initializer_list<int>>::value,
      "mutable elements");
  static_assert (
      is_explicit_only<span<int const, 2>, std::initializer_list<int>>::value,
      "explicit for a static extent");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenStaticExtentSpan_WhenInitializedFromAListObject_ThenViewsIt)
{
  span<int const, 3> const view (std::initializer_list<int>{ 1, 2, 3 });
  EXPECT_EQ (view.size (), 3u);
}

// -- Copy --

TEST (LumexSpanConstructionTest, GivenSpan_WhenCopied_ThenSameView)
{
  int values[3] = { 1, 2, 3 };
  span<int> const original (values);
  span<int> copy (original);
  EXPECT_EQ (copy.data (), original.data ());
  EXPECT_EQ (copy.size (), original.size ());
  span<int> assigned;
  assigned = original;
  EXPECT_EQ (assigned.data (), values);
  EXPECT_EQ (assigned.size (), 3u);
}

TEST (LumexSpanConstructionTest, GivenSpan_WhenTraits_ThenTriviallyCopyable)
{
  static_assert (std::is_trivially_copyable<span<int>>::value, "dynamic");
  static_assert (std::is_trivially_copyable<span<int, 3>>::value, "static");
  static_assert (std::is_trivially_copy_constructible<span<int>>::value,
                 "copy constructor is trivial");
  static_assert (std::is_trivially_destructible<span<int>>::value,
                 "destructor is trivial");
  static_assert (std::is_nothrow_copy_constructible<span<int>>::value,
                 "copy is noexcept");
  static_assert (std::is_nothrow_copy_assignable<span<int>>::value,
                 "assignment is noexcept");
  static_assert (std::is_nothrow_move_constructible<span<int>>::value,
                 "move is noexcept");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenSpans_WhenSizeof_ThenStaticExtentStoresNoSize)
{
  static_assert (sizeof (span<int, 3>) == sizeof (int *),
                 "a static extent is not stored");
  static_assert (sizeof (span<int, 0>) == sizeof (int *),
                 "a zero extent is not stored");
  static_assert (sizeof (span<int>) == 2 * sizeof (void *),
                 "a dynamic extent stores a pointer and a size");
  SUCCEED ();
}

// -- Conversion from another span --

TEST (LumexSpanConstructionTest, GivenMutableSpan_WhenConstSpan_ThenImplicit)
{
  int values[3] = { 1, 2, 3 };
  span<int> const mutable_view (values);
  span<int const> const_view = mutable_view;
  EXPECT_EQ (const_view.data (), values);
  EXPECT_EQ (const_view.size (), 3u);
  static_assert (std::is_convertible<span<int>, span<int const>>::value, "");
  static_assert (std::is_convertible<span<int, 3>, span<int const, 3>>::value,
                 "");
  static_assert (!std::is_constructible<span<int>, span<int const>>::value,
                 "const must not be dropped");
  static_assert (
      !std::is_constructible<span<int, 3>, span<int const, 3>>::value,
      "const must not be dropped");
}

TEST (LumexSpanConstructionTest, GivenStaticSpan_WhenDynamic_ThenImplicit)
{
  int values[3] = { 1, 2, 3 };
  span<int, 3> const fixed (values);
  span<int> const dynamic = fixed;
  EXPECT_EQ (dynamic.size (), 3u);
  EXPECT_EQ (dynamic.data (), values);
  static_assert (std::is_convertible<span<int, 3>, span<int>>::value, "");
  static_assert (std::is_convertible<span<int, 3>, span<int const>>::value,
                 "");
}

TEST (LumexSpanConstructionTest, GivenDynamicSpan_WhenStatic_ThenExplicitOnly)
{
  int values[3] = { 1, 2, 3 };
  span<int> const dynamic (values);
  span<int, 3> const fixed (dynamic);
  EXPECT_EQ (fixed.size (), 3u);
  EXPECT_EQ (fixed.data (), values);
  static_assert (is_explicit_only<span<int, 3>, span<int>>::value,
                 "dynamic to static is explicit");
  static_assert (is_explicit_only<span<int const, 3>, span<int>>::value,
                 "also with a qualification conversion");
}

TEST (LumexSpanConstructionTest, GivenStaticSpans_WhenOtherExtent_ThenRejected)
{
  static_assert (!std::is_constructible<span<int, 4>, span<int, 3>>::value,
                 "3 to 4");
  static_assert (!std::is_constructible<span<int, 2>, span<int, 3>>::value,
                 "3 to 2");
  static_assert (std::is_constructible<span<int, 3>, span<int, 3>>::value,
                 "3 to 3");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest, GivenSpanOfDerived_WhenBaseSpan_ThenRejected)
{
  static_assert (
      !std::is_constructible<span<base_element>, span<derived_element>>::value,
      "Derived to Base");
  static_assert (!std::is_constructible<span<base_element const>,
                                        span<derived_element>>::value,
                 "Derived to const Base");
  static_assert (!std::is_constructible<span<char>, span<int>>::value,
                 "unrelated elements");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenSpanSource_WhenRangeConstructorCandidate_ThenConversionOneIsUsed)
{
  int values[3] = { 1, 2, 3 };
  span<int, 3> const fixed (values);
  // A span source is converted by the span conversion, never by the range
  // constructor; the static to dynamic form must stay implicit.
  span<int const> const view = fixed;
  EXPECT_EQ (view.size (), 3u);
  static_assert (
      std::is_convertible<span<int, 3> const &, span<int const>>::value,
      "an lvalue span converts implicitly");
  static_assert (std::is_convertible<span<int, 3>, span<int const>>::value,
                 "an rvalue span converts implicitly");
  SUCCEED ();
}

TEST (LumexSpanConstructionTest,
      GivenTemporarySpan_WhenFunctionArgument_ThenConverts)
{
  int values[3] = { 1, 2, 3 };
  EXPECT_EQ (sum_of (span<int, 3> (values)), 6);
  EXPECT_EQ (sum_of (values), 6);
  std::array<int, 3> array_values = { { 1, 2, 3 } };
  EXPECT_EQ (sum_of (array_values), 6);
  std::vector<int> const vector_values = { 1, 2, 3 };
  EXPECT_EQ (sum_of (vector_values), 6);
}

TEST (LumexSpanConstructionTest,
      GivenNoexceptness_WhenConstructors_ThenAsStandard)
{
  static_assert (
      std::is_nothrow_constructible<span<int>, int *, std::size_t>::value,
      "pointer and count");
  static_assert (std::is_nothrow_constructible<span<int>, int *, int *>::value,
                 "pointer pair");
  static_assert (std::is_nothrow_constructible<span<int>, int (&)[3]>::value,
                 "array");
  static_assert (
      std::is_nothrow_constructible<span<int>, std::array<int, 3> &>::value,
      "std::array");
  static_assert (
      std::is_nothrow_constructible<span<int const>, span<int>>::value,
      "span conversion");
  SUCCEED ();
}

// -- Incomplete element types --

namespace
{
// A class may hold a span of itself: the element type is only needed
// complete where an element is accessed.
struct tree_node
{
  int value;
  span<tree_node> children;
  span<tree_node const, 2> pair;
};
} // namespace

TEST (LumexSpanConstructionTest,
      GivenClassHoldingSpanOfItself_WhenUsed_ThenChildrenAreReachable)
{
  tree_node leaves[2] = {
    { 2, span<tree_node> (), span<tree_node const, 2> (leaves, leaves + 2) },
    { 3, span<tree_node> (), span<tree_node const, 2> (leaves, leaves + 2) }
  };
  tree_node root
      = { 1, span<tree_node> (leaves), span<tree_node const, 2> (leaves) };
  EXPECT_EQ (root.children.size (), 2u);
  EXPECT_EQ (root.children[1].value, 3);
  EXPECT_EQ (root.pair.back ().value, 3);
  EXPECT_TRUE (root.children[0].children.empty ());
}

// -- Elements with an overloaded operator& --

namespace
{
// operator& is hostile: a span must find the address of the first element
// without it (std::addressof does the same).
struct hostile_address
{
  int value;
  hostile_address *
  operator& ()
  {
    return nullptr;
  }
  hostile_address const *
  operator& () const
  {
    return nullptr;
  }
};
} // namespace

TEST (
    LumexSpanConstructionTest,
    GivenElementWithOverloadedAddressOperator_WhenIteratorClass_ThenRealAddress)
{
  hostile_address values[3] = { { 1 }, { 2 }, { 3 } };
  span<hostile_address> const view (pointer_iterator<hostile_address> (values),
                                    3);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_TRUE (view.data () != nullptr);
  EXPECT_EQ (view[2].value, 3);
  span<hostile_address> const pair (
      pointer_iterator<hostile_address> (values + 1),
      pointer_iterator<hostile_address> (values + 3));
  EXPECT_EQ (pair.size (), 2u);
  EXPECT_EQ (pair.front ().value, 2);
}

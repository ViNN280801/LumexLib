// LumexRangeTraits.tests.cpp
#include <array>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

using lumex::core::utility::traits::has_elements_convertible_to;
using lumex::core::utility::traits::has_streamable_elements;
using lumex::core::utility::traits::is_iterable;
using lumex::core::utility::traits::range_reference;

namespace
{
struct streamable_t
{
  int value;
};

std::ostream &
operator<< (std::ostream &os, streamable_t const &item)
{
  return os << item.value;
}

struct not_streamable_t
{
  int value;
};

struct begin_only_t
{
  int *
  begin () const
  {
    return nullptr;
  }
};

struct member_range_t
{
  int const *
  begin () const
  {
    return data;
  }
  int const *
  end () const
  {
    return data + 2;
  }
  int data[2];
};

struct string_like_t
{
  operator std::string () const { return "text"; }
};

template <typename T, typename = void>
struct has_reference_type : std::false_type
{
};

template <typename T>
struct has_reference_type<
    T, lumex::core::utility::traits::void_t<typename T::type>> : std::true_type
{
};
} // namespace

TEST (LumexRangeTraitsTest, GivenRanges_WhenRangeReference_ThenElementRef)
{
  EXPECT_TRUE ((std::is_same<range_reference<std::vector<int>>::type,
                             int const &>::value));
  EXPECT_TRUE (
      (std::is_same<range_reference<int[4]>::type, int const &>::value));
  EXPECT_TRUE (
      (std::is_same<range_reference<std::string>::type, char const &>::value));
  EXPECT_TRUE ((std::is_same<range_reference<std::map<int, char>>::type,
                             std::pair<int const, char> const &>::value));
  EXPECT_TRUE ((std::is_same<range_reference<member_range_t>::type,
                             int const &>::value));
}

TEST (LumexRangeTraitsTest, GivenNonRanges_WhenRangeReference_ThenNoType)
{
  EXPECT_FALSE ((has_reference_type<range_reference<int>>::value));
  EXPECT_FALSE ((has_reference_type<range_reference<int *>>::value));
  EXPECT_FALSE ((has_reference_type<range_reference<begin_only_t>>::value));
  EXPECT_FALSE (
      (has_reference_type<range_reference<std::unique_ptr<int>>>::value));
  EXPECT_TRUE ((has_reference_type<range_reference<std::list<int>>>::value));
}

TEST (LumexRangeTraitsTest, GivenTypes_WhenIsIterable_ThenMatchesRangeFor)
{
  EXPECT_TRUE (is_iterable<std::vector<int>>::value);
  EXPECT_TRUE (is_iterable<std::set<std::string>>::value);
  EXPECT_TRUE ((is_iterable<std::array<int, 3>>::value));
  EXPECT_TRUE (is_iterable<int[2]>::value);
  EXPECT_TRUE (is_iterable<std::string>::value);
  EXPECT_TRUE (is_iterable<member_range_t>::value);
  EXPECT_FALSE (is_iterable<int>::value);
  EXPECT_FALSE (is_iterable<int *>::value);
  EXPECT_FALSE (is_iterable<begin_only_t>::value);
  EXPECT_FALSE (is_iterable<streamable_t>::value);
}

TEST (LumexRangeTraitsTest, GivenElements_WhenHasStreamableElements_ThenDecays)
{
  EXPECT_TRUE (has_streamable_elements<std::vector<int>>::value);
  EXPECT_TRUE (has_streamable_elements<std::vector<streamable_t>>::value);
  EXPECT_TRUE (has_streamable_elements<std::list<char const *>>::value);
  EXPECT_TRUE (has_streamable_elements<double[3]>::value);
  EXPECT_FALSE (has_streamable_elements<std::vector<not_streamable_t>>::value);
  EXPECT_FALSE ((has_streamable_elements<std::map<int, int>>::value));
  EXPECT_FALSE (has_streamable_elements<std::vector<std::vector<int>>>::value);
  EXPECT_FALSE (has_streamable_elements<int>::value);
}

TEST (LumexRangeTraitsTest, GivenElements_WhenConvertibleToString_ThenExpected)
{
  EXPECT_TRUE ((has_elements_convertible_to<std::vector<std::string>,
                                            std::string const &>::value));
  EXPECT_TRUE ((has_elements_convertible_to<std::vector<char const *>,
                                            std::string const &>::value));
  EXPECT_TRUE ((has_elements_convertible_to<std::vector<string_like_t>,
                                            std::string const &>::value));
  EXPECT_FALSE ((has_elements_convertible_to<std::vector<int>,
                                             std::string const &>::value));
  EXPECT_FALSE ((has_elements_convertible_to<std::vector<streamable_t>,
                                             std::string const &>::value));
  EXPECT_FALSE (
      (has_elements_convertible_to<int, std::string const &>::value));
}

TEST (LumexRangeTraitsTest, GivenNumericTargets_WhenConvertible_ThenExpected)
{
  EXPECT_TRUE ((has_elements_convertible_to<std::vector<int>, long>::value));
  EXPECT_TRUE ((has_elements_convertible_to<int[2], double>::value));
  EXPECT_FALSE (
      (has_elements_convertible_to<std::vector<std::string>, int>::value));
}

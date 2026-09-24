// LumexTextConstraints.tests.cpp
//
// Which join / quote calls are viable. A rejected call must be removed from
// overload resolution (SFINAE below C++20, a requires-clause at C++20), not
// fail inside the function body, so these detection checks can see it.
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexJoin.hpp"
#include "lumex/core/string/text/LumexQuote.hpp"

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

struct string_like_t
{
  operator std::string () const { return "text"; }
};

struct begin_only_t
{
  int *
  begin () const
  {
    return nullptr;
  }
};

template <typename... T> struct make_void
{
  typedef void type;
};

template <typename Range, typename Separator, typename = void>
struct can_join : std::false_type
{
};

template <typename Range, typename Separator>
struct can_join<Range, Separator,
                typename make_void<decltype (lumex::core::string::text::join (
                    std::declval<Range const &> (),
                    std::declval<Separator const &> ()))>::type>
    : std::true_type
{
};

template <typename Range, typename Separator, typename = void>
struct can_quote : std::false_type
{
};

template <typename Range, typename Separator>
struct can_quote<
    Range, Separator,
    typename make_void<decltype (lumex::core::string::text::quote (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ())),
                       decltype (lumex::core::string::text::quote_double (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ())),
                       decltype (lumex::core::string::text::quote_single (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ()))>::type>
    : std::true_type
{
};

typedef char const separator_literal_t[3];
} // namespace

// --- join: viable in every standard ---

TEST (LumexTextConstraintsTest, GivenStreamableElements_ThenJoinViable)
{
  EXPECT_TRUE ((can_join<std::vector<int>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::vector<std::string>, std::string>::value));
  EXPECT_TRUE ((can_join<std::list<double>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::deque<char>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::set<long>, separator_literal_t>::value));
  EXPECT_TRUE (
      (can_join<std::vector<char const *>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::vector<streamable_t>, std::string>::value));
  EXPECT_TRUE ((can_join<int[3], separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::array<int, 2>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::string, separator_literal_t>::value));
}

// --- join: rejected in every standard ---

TEST (LumexTextConstraintsTest, GivenNonStreamableElements_ThenJoinRejected)
{
  EXPECT_FALSE (
      (can_join<std::vector<not_streamable_t>, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<std::map<int, int>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_join<std::vector<std::vector<int>>, separator_literal_t>::value));
}

TEST (LumexTextConstraintsTest, GivenNonRange_ThenJoinRejected)
{
  EXPECT_FALSE ((can_join<int, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<streamable_t, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<begin_only_t, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<std::unique_ptr<int>, separator_literal_t>::value));
}

TEST (LumexTextConstraintsTest, GivenNonStreamableSeparator_ThenJoinRejected)
{
  EXPECT_FALSE ((can_join<std::vector<int>, not_streamable_t>::value));
  EXPECT_FALSE ((can_join<std::vector<int>, std::vector<char>>::value));
}

// --- quote: viable / rejected in every standard ---

TEST (LumexTextConstraintsTest, GivenStringElements_ThenQuoteViable)
{
  EXPECT_TRUE (
      (can_quote<std::vector<std::string>, separator_literal_t>::value));
  EXPECT_TRUE (
      (can_quote<std::vector<char const *>, separator_literal_t>::value));
  EXPECT_TRUE ((can_quote<std::list<std::string>, std::string>::value));
  EXPECT_TRUE ((can_quote<std::set<std::string>, separator_literal_t>::value));
  EXPECT_TRUE ((can_quote<std::string[2], separator_literal_t>::value));
  EXPECT_TRUE (
      (can_quote<std::vector<string_like_t>, separator_literal_t>::value));
}

TEST (LumexTextConstraintsTest, GivenNonStringElements_ThenQuoteRejected)
{
  EXPECT_FALSE ((can_quote<std::vector<int>, separator_literal_t>::value));
  EXPECT_FALSE ((can_quote<std::vector<double>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_quote<std::vector<streamable_t>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_quote<std::vector<not_streamable_t>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_quote<std::map<std::string, int>, separator_literal_t>::value));
}

TEST (LumexTextConstraintsTest, GivenNonRange_ThenQuoteRejected)
{
  EXPECT_FALSE ((can_quote<int, separator_literal_t>::value));
  EXPECT_FALSE ((can_quote<begin_only_t, separator_literal_t>::value));
}

TEST (LumexTextConstraintsTest, GivenNonStreamableSeparator_ThenQuoteRejected)
{
  EXPECT_FALSE (
      (can_quote<std::vector<std::string>, not_streamable_t>::value));
}

#if __cplusplus < 202002L

// --- C++11 to C++17: any streamable separator, and the Detail traits ---

TEST (LumexTextConstraintsTest, GivenStreamableNonStringSeparator_ThenViable)
{
  EXPECT_TRUE ((can_join<std::vector<int>, char>::value));
  EXPECT_TRUE ((can_join<std::vector<int>, int>::value));
  EXPECT_TRUE ((can_join<std::vector<int>, streamable_t>::value));
  EXPECT_TRUE ((can_quote<std::vector<std::string>, char>::value));
}

// The traits behind these checks (range_reference, has_streamable_elements,
// has_elements_convertible_to, is_streamable) are tested in
// lumex/tests/core/utility/LumexRangeTraits.tests.cpp and
// LumexStreamTraits.tests.cpp.

#else

// --- C++20: the separator must convert to std::string_view ---

TEST (LumexTextConstraintsTest, GivenNonStringViewSeparator_ThenRejected)
{
  EXPECT_FALSE ((can_join<std::vector<int>, char>::value));
  EXPECT_FALSE ((can_join<std::vector<int>, int>::value));
  EXPECT_FALSE ((can_quote<std::vector<std::string>, char>::value));
}

#endif

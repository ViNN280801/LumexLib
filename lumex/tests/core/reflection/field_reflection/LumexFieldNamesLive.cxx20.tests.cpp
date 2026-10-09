/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// LumexFieldNamesLive.cxx20.tests.cpp
//
// The names the compiler supplies, read live from the compiler under test:
// aggregates in nested and unnamed namespaces, in a class, of a class
// template, with digits and underscores in the names, a nested aggregate as a
// member. They are not registered, so names_as_array answers with the pretty
// strings of the compiler and the parser of the library (the recorded strings
// of every compiler are in LumexFieldNamesParser.cxx11.tests.cpp). The
// library has the compiler's names only when __cplusplus is at least 202002L
// (GCC 8 reports 201709L at -std=c++2a), so the tests skip below it. Built
// when LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the
// other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::field_reflection;

namespace live_outer
{
namespace live_inner
{
struct LiveDeep
{
  int first_1;
  double _second;
  std::string third_3_x;
};
} // namespace live_inner
} // namespace live_outer

namespace
{
namespace live_hidden
{
struct LiveHidden
{
  int hidden_a;
  bool hidden_b2;
};
} // namespace live_hidden
} // namespace

namespace live_outer
{
template <typename Item> struct LiveBox
{
  Item item;
  int count;
};

class LiveHost
{
public:
  struct inner_t
  {
    int host_member;
    std::string other;
  };
};

struct LiveNested
{
  live_inner::LiveDeep deep;
  std::vector<live_inner::LiveDeep> list;
  LiveBox<std::string> box;
  int tail;
};

struct LiveSameAsScope
{
  // Members named like the scopes around them.
  int live_outer;
  int LiveSameAsScope_x;
};
} // namespace live_outer

using namespace live_outer;

namespace
{
#if __cplusplus >= 202002L
template <typename Agg>
std::array<char const *, tuple_size<Agg>::value>
compiler_names_of ()
{
  return detail::names_builder<Agg,
                               typename detail::make_index_sequence<
                                   tuple_size<Agg>::value>::type>::build ();
}

// The raw pretty string of one field.
template <typename Agg, std::size_t I>
std::string
pretty_of ()
{
  return detail::field_pretty<Agg, I> ();
}

// The tail of the pretty string after the member is nothing but closing
// delimiters and the end of the signature (`void`, `noexcept` of MSVC), with
// no operator that could be taken for the start of another name.
template <typename Agg, std::size_t I>
void
expect_name_is_the_tail (char const *expected)
{
  std::string const pretty = pretty_of<Agg, I> ();
  std::string const name = expected;
  std::size_t const begin
      = detail::member_name_begin (pretty.c_str (), pretty.size ());
  ASSERT_LT (begin, pretty.size ()) << pretty;
  EXPECT_EQ (pretty.compare (begin, name.size (), name), 0) << pretty;

  std::string tail = pretty.substr (begin + name.size ());
  EXPECT_EQ (tail.find_first_of (".:-"), std::string::npos) << pretty;
  char const *const words[] = { "noexcept", "void" };
  for (char const *word : words)
    {
      std::size_t at = tail.find (word);
      while (at != std::string::npos)
        {
          tail.erase (at, std::string (word).size ());
          at = tail.find (word);
        }
    }
  EXPECT_EQ (tail.find_first_not_of (")]>(; ,"), std::string::npos) << pretty;
}
#endif
} // namespace

TEST (LumexFieldNamesLiveTest,
      GivenNestedNamespaces_WhenNamed_ThenLastIdentifier)
{
#if __cplusplus >= 202002L
  typedef live_inner::LiveDeep deep_t;
  std::array<char const *, 3> const names = compiler_names_of<deep_t> ();
  EXPECT_STREQ (names[0], "first_1");
  EXPECT_STREQ (names[1], "_second");
  EXPECT_STREQ (names[2], "third_3_x");
  EXPECT_STREQ (names_as_array<deep_t> ()[0], "first_1");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenUnnamedNamespace_WhenNamed_ThenLastIdentifier)
{
#if __cplusplus >= 202002L
  typedef live_hidden::LiveHidden hidden_t;
  std::array<char const *, 2> const names = compiler_names_of<hidden_t> ();
  EXPECT_STREQ (names[0], "hidden_a");
  EXPECT_STREQ (names[1], "hidden_b2");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenClassTemplate_WhenNamed_ThenMemberNotArgument)
{
#if __cplusplus >= 202002L
  typedef LiveBox<std::string> string_box_t;
  typedef LiveBox<LiveBox<int>> nested_box_t;
  std::array<char const *, 2> const names = compiler_names_of<string_box_t> ();
  EXPECT_STREQ (names[0], "item");
  EXPECT_STREQ (names[1], "count");
  std::array<char const *, 2> const inner = compiler_names_of<nested_box_t> ();
  EXPECT_STREQ (inner[0], "item");
  EXPECT_STREQ (inner[1], "count");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenAggregateInClass_WhenNamed_ThenLastIdentifier)
{
#if __cplusplus >= 202002L
  std::array<char const *, 2> const names
      = compiler_names_of<LiveHost::inner_t> ();
  EXPECT_STREQ (names[0], "host_member");
  EXPECT_STREQ (names[1], "other");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenNestedAggregateMembers_WhenNamed_ThenMemberNames)
{
#if __cplusplus >= 202002L
  std::array<char const *, 4> const names = compiler_names_of<LiveNested> ();
  EXPECT_STREQ (names[0], "deep");
  EXPECT_STREQ (names[1], "list");
  EXPECT_STREQ (names[2], "box");
  EXPECT_STREQ (names[3], "tail");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenMembersNamedLikeScopes_WhenNamed_ThenMemberNames)
{
#if __cplusplus >= 202002L
  std::array<char const *, 2> const names
      = compiler_names_of<LiveSameAsScope> ();
  EXPECT_STREQ (names[0], "live_outer");
  EXPECT_STREQ (names[1], "LiveSameAsScope_x");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest,
      GivenLivePrettyStrings_WhenParsed_ThenNameIsTheTailOfTheString)
{
#if __cplusplus >= 202002L
  expect_name_is_the_tail<live_inner::LiveDeep, 0> ("first_1");
  expect_name_is_the_tail<live_inner::LiveDeep, 2> ("third_3_x");
  expect_name_is_the_tail<LiveBox<std::string>, 0> ("item");
  expect_name_is_the_tail<LiveHost::inner_t, 1> ("other");
  expect_name_is_the_tail<LiveNested, 3> ("tail");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesLiveTest, GivenLiveName_WhenAskedTwice_ThenSameStorage)
{
#if __cplusplus >= 202002L
  // The name of a field is parsed once and lives for the whole run.
  EXPECT_EQ (names_as_array<live_inner::LiveDeep> ()[0],
             names_as_array<live_inner::LiveDeep> ()[0]);
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)

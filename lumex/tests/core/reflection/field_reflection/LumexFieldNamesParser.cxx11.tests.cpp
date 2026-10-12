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

// LumexFieldNamesParser.cxx11.tests.cpp
//
// The parser of the compiler's field names (detail::copy_parsed_name), over
// the pretty strings of nttp_pretty: the template function whose
// LUMEX_FUNCTION_NAME carries the member as the last template argument, a
// pointer into a phantom object. The strings are DATA, recorded from the
// compilers, so that the test runs on every platform at every standard and
// not only where the compiler that printed a string is the one under test
// (the live names are in LumexFieldNamesLive.cxx20.tests.cpp and
// LumexFieldNamesAgreement.cxx20.tests.cpp).
//
// What the compilers print before the member differs, which broke the
// parser once: GCC 13 qualifies the member by its class after the last `.`
// (`.ns::Type::member`), the older GCC and Clang do not, and MSVC separates
// the path with `->`. The parser takes the identifier after the last member
// access or scope operator, so the qualification, the namespaces and the
// template arguments in front of it do not matter. Recorded:
//   GCC 13.2.0 (/opt/gcc-13.2.0), Clang 23.1.0 with libstdc++ and libc++:
//     captured from the library's own nttp_pretty on 2026-10-08.
//   GCC 8.3 (g++-8, and MinGW GCC 8.3 posix): there is no string to record.
//     Both reject a pointer to a subobject of an object ("is not a valid
//     template argument") whether it comes from std::addressof of a
//     structured binding or from `&storage.value.member`, so the library has
//     no compiler names there (__cplusplus is 201709L at -std=c++2a, the live
//     tests skip) and the registration is the only source.
//   MSVC: no compiler available here; the strings follow the shape the
//     library's code documents (`...fake_object_storage<T>->value->id`), they
//     were NOT captured from a real MSVC.
// Every suite of the module compiles this file. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <cstddef>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::field_reflection;

namespace
{
struct recorded_t
{
  char const *compiler;
  char const *pretty;
  char const *expected;
};

// clang-format off
recorded_t const k_recorded[] = {
  // GCC 13.2.0: the member is qualified by its class after the last `.`.
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Plain; auto Pointer = (& fake_object_storage<ns1::ns2::Plain>.fake_object_wrapper_t<ns1::ns2::Plain>::value.ns1::ns2::Plain::id)]",
    "id" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Plain; auto Pointer = (& fake_object_storage<ns1::ns2::Plain>.fake_object_wrapper_t<ns1::ns2::Plain>::value.ns1::ns2::Plain::value_2)]",
    "value_2" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Plain; auto Pointer = (& fake_object_storage<ns1::ns2::Plain>.fake_object_wrapper_t<ns1::ns2::Plain>::value.ns1::ns2::Plain::_name)]",
    "_name" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Tpl<int>; auto Pointer = (& fake_object_storage<ns1::ns2::Tpl<int> >.fake_object_wrapper_t<ns1::ns2::Tpl<int> >::value.ns1::ns2::Tpl<int>::item)]",
    "item" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Tpl<std::__cxx11::basic_string<char> >; auto Pointer = (& fake_object_storage<ns1::ns2::Tpl<std::__cxx11::basic_string<char> > >.fake_object_wrapper_t<ns1::ns2::Tpl<std::__cxx11::basic_string<char> > >::value.ns1::ns2::Tpl<std::__cxx11::basic_string<char> >::item)]",
    "item" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Nested; auto Pointer = (& fake_object_storage<ns1::ns2::Nested>.fake_object_wrapper_t<ns1::ns2::Nested>::value.ns1::ns2::Nested::list)]",
    "list" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Inner::Deep; auto Pointer = (& fake_object_storage<ns1::ns2::Inner::Deep>.fake_object_wrapper_t<ns1::ns2::Inner::Deep>::value.ns1::ns2::Inner::Deep::z)]",
    "z" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = {anonymous}::Anon; auto Pointer = (& fake_object_storage<{anonymous}::Anon>.fake_object_wrapper_t<{anonymous}::Anon>::value.{anonymous}::Anon::a_b9)]",
    "a_b9" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = {anonymous}::ns4::Ns; auto Pointer = (& fake_object_storage<{anonymous}::ns4::Ns>.fake_object_wrapper_t<{anonymous}::ns4::Ns>::value.{anonymous}::ns4::Ns::q)]",
    "q" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = Global; auto Pointer = (& fake_object_storage<Global>.fake_object_wrapper_t<Global>::value.Global::g)]",
    "g" },

  // GCC 13.2.0, a member of a base class: through the object (the base is an
  // anonymous subobject of the derived class) and as a plain pointer to a
  // member.
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = Derived; auto Pointer = (& fake_object_storage<Derived>.fake_object_wrapper_t<Derived>::value.Derived::<anonymous>.Base::b)]",
    "b" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = Derived; auto Pointer = &Base::b]",
    "b" },
  { "gcc13",
    "const char* lumex::core::reflection::field_reflection::detail::nttp_pretty() [with Agg = ns1::ns2::Plain; auto Pointer = &ns1::ns2::Plain::id]",
    "id" },

  // Clang 23.1.0 with libstdc++: the path is `fake_object_storage.value.`.
  { "clang23-libstdc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = ns1::ns2::Plain, Pointer = &fake_object_storage.value.id]",
    "id" },
  { "clang23-libstdc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = ns1::ns2::Plain, Pointer = &fake_object_storage.value._name]",
    "_name" },
  { "clang23-libstdc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = ns1::ns2::Tpl<std::basic_string<char>>, Pointer = &fake_object_storage.value.item]",
    "item" },
  { "clang23-libstdc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = (anonymous namespace)::Anon, Pointer = &fake_object_storage.value.a_b9]",
    "a_b9" },
  { "clang23-libstdc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = ns1::ns2::Inner::Deep, Pointer = &fake_object_storage.value.z]",
    "z" },
  // Clang 23.1.0 with libc++.
  { "clang23-libc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = ns1::ns2::Tpl<std::string>, Pointer = &fake_object_storage.value.item]",
    "item" },
  { "clang23-libc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = (anonymous namespace)::ns4::Ns, Pointer = &fake_object_storage.value.q]",
    "q" },
  { "clang23-libc++",
    "const char *lumex::core::reflection::field_reflection::detail::nttp_pretty() [Agg = Global, Pointer = &fake_object_storage.value.g]",
    "g" },

  // MSVC, in the shape the library documents (not captured from a real one):
  // `->` between the parts of the path, `struct`/`class` before the types,
  // the parameter list and `noexcept` after the template arguments.
  { "msvc",
    "const char *__cdecl lumex::core::reflection::field_reflection::detail::nttp_pretty<struct ns1::ns2::Plain,&lumex::core::reflection::field_reflection::detail::fake_object_storage<struct ns1::ns2::Plain>->value->id>(void) noexcept",
    "id" },
  { "msvc",
    "const char *__cdecl lumex::core::reflection::field_reflection::detail::nttp_pretty<struct ns1::ns2::Tpl<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,&lumex::core::reflection::field_reflection::detail::fake_object_storage<struct ns1::ns2::Tpl<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > > >->value->item>(void) noexcept",
    "item" },
  { "msvc",
    "const char *__cdecl lumex::core::reflection::field_reflection::detail::nttp_pretty<struct `anonymous namespace'::Anon,&lumex::core::reflection::field_reflection::detail::fake_object_storage<struct `anonymous namespace'::Anon>->value->a_b9>(void) noexcept",
    "a_b9" },
};
// clang-format on

std::string
parse (char const *pretty)
{
  char buf[128];
  detail::copy_parsed_name (buf, sizeof (buf), pretty);
  return buf;
}
} // namespace

TEST (LumexFieldNamesParserTest,
      GivenRecordedPrettyStringsOfEveryCompiler_WhenParsed_ThenMemberName)
{
  for (std::size_t i = 0; i < sizeof (k_recorded) / sizeof (k_recorded[0]);
       ++i)
    EXPECT_EQ (parse (k_recorded[i].pretty), k_recorded[i].expected)
        << k_recorded[i].compiler << ": " << k_recorded[i].pretty;
}

TEST (LumexFieldNamesParserTest,
      GivenMemberQualifiedAfterTheLastDot_WhenParsed_ThenNotTheNamespace)
{
  // The regression of GCC 13: the text after the last `.` is the qualified
  // member, whose first identifier is the namespace.
  EXPECT_EQ (parse ("[with Pointer = (& s.w::value.my_ns::my_type::field)]"),
             "field");
  EXPECT_EQ (parse ("[with Pointer = (& s.w::value.a::b::c::d::e)]"), "e");
}

TEST (LumexFieldNamesParserTest,
      GivenAnonymousNamespaceQualifier_WhenParsed_ThenMember)
{
  EXPECT_EQ (parse ("(& s.value.{anonymous}::T::m)]"), "m");
  EXPECT_EQ (parse ("&s.value.(anonymous namespace)::T::m]"), "m");
  EXPECT_EQ (parse ("&s->`anonymous namespace'::T::m>(void) noexcept"), "m");
}

TEST (LumexFieldNamesParserTest,
      GivenTemplateTypeQualifier_WhenParsed_ThenMember)
{
  EXPECT_EQ (parse ("(& s.value.box<int, box<long> >::item)]"), "item");
  EXPECT_EQ (parse ("(& s.value.ns::box<std::pair<int, int> >::second)]"),
             "second");
}

TEST (LumexFieldNamesParserTest,
      GivenMemberOfBaseClass_WhenParsed_ThenMemberName)
{
  // A plain pointer to a member, and one qualified by a base class.
  EXPECT_EQ (parse ("[Agg = ns::Derived, Pointer = &ns::Base::b]"), "b");
  EXPECT_EQ (parse ("(& s.value.ns::Derived::ns::Base::b)]"), "b");
  EXPECT_EQ (parse ("&s->value->ns::Base::b>(void)"), "b");
}

TEST (LumexFieldNamesParserTest,
      GivenNamesWithDigitsAndUnderscores_WhenParsed_ThenWholeName)
{
  EXPECT_EQ (parse ("[Pointer = &s.value.field_1]"), "field_1");
  EXPECT_EQ (parse ("[Pointer = &s.value._leading]"), "_leading");
  EXPECT_EQ (parse ("[Pointer = &s.value.trailing_]"), "trailing_");
  EXPECT_EQ (parse ("[Pointer = &s.value.a1b2_c3]"), "a1b2_c3");
  EXPECT_EQ (parse ("[Pointer = &s.value.__double]"), "__double");
  EXPECT_EQ (parse ("(& s.value.n::T::x_9)]"), "x_9");
}

TEST (LumexFieldNamesParserTest, GivenUtf8Identifier_WhenParsed_ThenWholeName)
{
  // An identifier written in UTF-8: every byte of the sequence belongs to it.
  EXPECT_EQ (parse ("[Pointer = &s.value.\xd0\xb8\xd0\xbc\xd1\x8f_1]"),
             "\xd0\xb8\xd0\xbc\xd1\x8f_1");
}

TEST (LumexFieldNamesParserTest,
      GivenFloatingPointTemplateArgumentInFront_WhenParsed_ThenMember)
{
  // A `.` inside a floating-point argument is followed by a digit, not by an
  // identifier, and anyway stands before the member.
  EXPECT_EQ (parse ("[Agg = ns::box<1.5e+0>, Pointer = &s.value.item]"),
             "item");
  EXPECT_EQ (parse ("[Agg = ns::box<1.e5>, Pointer = &s.value.item]"), "item");
  EXPECT_EQ (parse ("[Agg = ns::box<2.5>, Pointer = &ns::box<2.5>::item]"),
             "item");
}

TEST (LumexFieldNamesParserTest,
      GivenSignatureTailOfMsvc_WhenParsed_ThenMember)
{
  EXPECT_EQ (parse ("f<S,&x->value->id>(void) noexcept"), "id");
  EXPECT_EQ (parse ("f<S,&x->value->id>(void)"), "id");
  EXPECT_EQ (parse ("f<S,&x->value->id>"), "id");
}

TEST (LumexFieldNamesParserTest,
      GivenMemberNamedLikeAScope_WhenParsed_ThenMember)
{
  EXPECT_EQ (parse ("(& s.value.live::live::live)]"), "live");
  EXPECT_EQ (parse ("(& s.value.T::T)]"), "T");
}

TEST (LumexFieldNamesParserTest, GivenStringWithoutMember_WhenParsed_ThenEmpty)
{
  EXPECT_EQ (parse (""), "");
  EXPECT_EQ (parse ("no operator here"), "");
  EXPECT_EQ (parse ("ends with an operator."), "");
  EXPECT_EQ (parse ("ends with a scope::"), "");
  EXPECT_EQ (parse ("ends with an arrow->"), "");
  EXPECT_EQ (parse ("a single : colon and a - dash"), "");
  EXPECT_EQ (parse ("1.5 and 2::3"), "");
}

TEST (LumexFieldNamesParserTest,
      GivenLongName_WhenParsedIntoSmallBuffer_ThenCut)
{
  char buf[4];
  detail::copy_parsed_name (buf, sizeof (buf), "&s.value.abcdefgh]");
  EXPECT_STREQ (buf, "abc");

  char one[1] = { 'x' };
  detail::copy_parsed_name (one, sizeof (one), "&s.value.abc]");
  EXPECT_STREQ (one, "");

  char untouched[1] = { 'x' };
  detail::copy_parsed_name (untouched, 0, "&s.value.abc]");
  EXPECT_EQ (untouched[0], 'x');

  std::string const name (200, 'n');
  std::string const pretty = "&s.value." + name + "]";
  char wide[128];
  detail::copy_parsed_name (wide, sizeof (wide), pretty.c_str ());
  EXPECT_EQ (std::strlen (wide), 127u);
  EXPECT_EQ (std::string (wide), std::string (127, 'n'));
}

TEST (LumexFieldNamesParserTest, GivenString_WhenBeginAsked_ThenIndexOfTheName)
{
  std::string const pretty = "a.b::c->d_1)]";
  EXPECT_EQ (detail::member_name_begin (pretty.c_str (), pretty.size ()),
             pretty.find ("d_1"));
  std::string const none = "none";
  EXPECT_EQ (detail::member_name_begin (none.c_str (), none.size ()),
             none.size ());
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)

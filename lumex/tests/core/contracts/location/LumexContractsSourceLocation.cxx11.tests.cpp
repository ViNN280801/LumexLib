// source_location: the accessors of std::source_location of C++20, a
// constructor from four values, comparison by content, current () where the
// compiler has the builtins.

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/location/LumexContractsSourceLocation.hpp"

namespace
{
using lumex::core::contracts::source_location;

TEST (ContractsSourceLocation, DefaultIsEmpty)
{
  source_location const where;
  EXPECT_STREQ (where.file_name (), "");
  EXPECT_STREQ (where.function_name (), "");
  EXPECT_EQ (where.line (), 0u);
  EXPECT_EQ (where.column (), 0u);
}

TEST (ContractsSourceLocation, KeepsTheGivenValues)
{
  source_location const where ("dir/file.cpp", "void f ()", 42, 7);
  EXPECT_STREQ (where.file_name (), "dir/file.cpp");
  EXPECT_STREQ (where.function_name (), "void f ()");
  EXPECT_EQ (where.line (), 42u);
  EXPECT_EQ (where.column (), 7u);
}

TEST (ContractsSourceLocation, ColumnDefaultsToZero)
{
  source_location const where ("file.cpp", "f", 3);
  EXPECT_EQ (where.line (), 3u);
  EXPECT_EQ (where.column (), 0u);
}

TEST (ContractsSourceLocation, NullNamesBecomeEmpty)
{
  source_location const where (nullptr, nullptr, 5, 6);
  EXPECT_STREQ (where.file_name (), "");
  EXPECT_STREQ (where.function_name (), "");
  EXPECT_EQ (where.line (), 5u);
  EXPECT_EQ (where.column (), 6u);
}

TEST (ContractsSourceLocation, StringsAreNotCopied)
{
  static char const file[] = "static.cpp";
  source_location const where (file, "f", 1);
  EXPECT_EQ (where.file_name (), static_cast<char const *> (file));
}

TEST (ContractsSourceLocation, ComparesByContent)
{
  std::string const first = "same.cpp";
  std::string const second = std::string ("sa") + "me.cpp";
  ASSERT_NE (first.c_str (), second.c_str ());
  EXPECT_TRUE (source_location (first.c_str (), "f", 1, 2)
               == source_location (second.c_str (), "f", 1, 2));
  EXPECT_FALSE (source_location (first.c_str (), "f", 1, 2)
                != source_location (second.c_str (), "f", 1, 2));
  EXPECT_TRUE (source_location ("a.cpp", "f", 1, 2)
               != source_location ("b.cpp", "f", 1, 2));
  EXPECT_TRUE (source_location ("a.cpp", "f", 1, 2)
               != source_location ("a.cpp", "g", 1, 2));
  EXPECT_TRUE (source_location ("a.cpp", "f", 1, 2)
               != source_location ("a.cpp", "f", 2, 2));
  EXPECT_TRUE (source_location ("a.cpp", "f", 1, 2)
               != source_location ("a.cpp", "f", 1, 3));
  EXPECT_TRUE (source_location ("a.cpp", "f", 1, 2)
               != source_location ("a.cpp.", "f", 1, 2));
  EXPECT_TRUE (source_location () == source_location ());
}

TEST (ContractsSourceLocation, IsTriviallyCopyable)
{
  EXPECT_TRUE (std::is_trivially_copyable<source_location>::value);
  EXPECT_TRUE (std::is_nothrow_default_constructible<source_location>::value);
  EXPECT_TRUE (
      (std::is_nothrow_constructible<source_location, char const *,
                                     char const *, std::uint_least32_t,
                                     std::uint_least32_t>::value));
}

TEST (ContractsSourceLocation, IsALiteralValue)
{
  constexpr source_location where ("c.cpp", "g", 10, 20);
  static_assert (where.line () == 10, "line");
  static_assert (where.column () == 20, "column");
  static_assert (where.file_name ()[0] == 'c', "file");
  static_assert (where == source_location ("c.cpp", "g", 10, 20),
                 "equal by content");
  static_assert (where != source_location ("c.cpp", "g", 11, 20),
                 "different line");
  static_assert (source_location ().line () == 0, "empty");
  SUCCEED ();
}

#if LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION
TEST (ContractsSourceLocation, CurrentIsThePlaceOfTheCall)
{
  std::uint_least32_t const before = __LINE__;
  source_location const where = source_location::current ();
  EXPECT_EQ (where.line (), before + 1);
  std::string const file = where.file_name ();
  EXPECT_NE (file.find ("LumexContractsSourceLocation"), std::string::npos)
      << file;
  // The function of a TEST body is named TestBody by gtest.
  EXPECT_NE (std::string (where.function_name ()).find ("TestBody"),
             std::string::npos)
      << where.function_name ();
}

source_location
call_site (source_location const &where = source_location::current ())
{
  return where;
}

TEST (ContractsSourceLocation, CurrentAsDefaultArgumentIsTheCaller)
{
  std::uint_least32_t const before = __LINE__;
  source_location const where = call_site ();
  EXPECT_EQ (where.line (), before + 1);
  EXPECT_NE (std::string (where.function_name ()).find ("TestBody"),
             std::string::npos)
      << where.function_name ();
}

TEST (ContractsSourceLocation, CurrentIsConstexprInTheMacroForm)
{
  constexpr source_location where (source_location::current ());
  EXPECT_GT (where.line (), 0u);
}
#endif
} // namespace

// source_location converts from std::source_location (C++20).

#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/location/LumexContractsSourceLocation.hpp"

namespace
{
using lumex::core::contracts::source_location;

TEST (ContractsSourceLocation20, ConvertsFromTheStandardOne)
{
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
  std::source_location const standard = std::source_location::current ();
  source_location const where = standard; // implicit
  EXPECT_STREQ (where.file_name (), standard.file_name ());
  EXPECT_STREQ (where.function_name (), standard.function_name ());
  EXPECT_EQ (where.line (), standard.line ());
  EXPECT_EQ (where.column (), standard.column ());
  EXPECT_TRUE (
      (std::is_convertible<std::source_location, source_location>::value));
  EXPECT_FALSE (
      (std::is_convertible<source_location, std::source_location>::value));
  constexpr source_location literal
      = source_location (std::source_location::current ());
  EXPECT_GT (literal.line (), 0u);
#else
  GTEST_SKIP () << "no std::source_location in this library";
#endif
}
} // namespace

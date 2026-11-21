// LumexFilesystemLiteral.cxx11.tests.cpp
//
// The classes of the module that hold only literal members are literal types
// at every standard of the build (todo 108): file_status, space_info and
// filesystem_result<int> and <void> are built by their public constructors in
// constant expressions and have trivial destructors. path, directory_entry
// and directory_iterator own a std::string or a shared state and never are
// literal before C++20 (the string is not), so nothing is pinned for them. A
// static_assert here stops the build when a constructor loses its constexpr.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"

namespace
{
using lumex::file_status;
using lumex::file_type;
using lumex::filesystem_result;
using lumex::perms;
using lumex::space_info;

constexpr file_status k_default_status;
constexpr file_status k_typed_status (file_type::directory);
constexpr file_status k_full_status (file_type::regular, perms::unknown);
constexpr filesystem_result<int> k_int_result;
constexpr filesystem_result<void> k_void_result;
constexpr space_info k_space{};

static_assert (std::is_trivially_destructible<file_status>::value,
               "file_status");
static_assert (std::is_trivially_destructible<space_info>::value,
               "space_info");
static_assert (std::is_trivially_destructible<filesystem_result<int>>::value,
               "filesystem_result<int>");
static_assert (std::is_trivially_destructible<filesystem_result<void>>::value,
               "filesystem_result<void>");
} // namespace

TEST (LumexFilesystemLiteralTest, ConstantObjects_AgreeWithTheRunTimeOnes)
{
  EXPECT_EQ (k_default_status.type (), file_type::none);
  EXPECT_EQ (k_default_status.permissions (), perms::unknown);
  EXPECT_EQ (k_typed_status.type (), file_type::directory);
  EXPECT_EQ (k_full_status.type (), file_type::regular);
  EXPECT_TRUE (static_cast<bool> (k_int_result));
  EXPECT_TRUE (static_cast<bool> (k_void_result));
  EXPECT_EQ (k_space.capacity, 0U);
  EXPECT_EQ (k_space.free, 0U);
  EXPECT_EQ (k_space.available, 0U);
}

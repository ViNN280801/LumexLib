// LumexFilesystemStdPath.cxx17.tests.cpp
//
// From C++17 lumex::path converts implicitly to and from
// std::filesystem::path. The conversions are member templates (never
// exported), the mixed comparisons and `/` are constrained templates that win
// over the operators of both libraries, and nothing that worked before becomes
// ambiguous: a std::string and a C string still go to lumex::path.
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/compiler/LumexCheckCompiler.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

namespace stdfs = std::filesystem;
using lumex::core::filesystem::fs::lumex_filesystem;
using lumex::core::filesystem::fs::path;

// libstdc++ 8 has a std::filesystem::path constructor from std::string&& next
// to the copy and move constructors, so a direct initialization or an
// assignment from a lumex path (which also converts to std::string) is
// ambiguous there; a copy initialization and push_back work.
#if LUMEX_COMPILER_IS_GCC() && LUMEX_GCC_BEFORE(9, 0)
#define LUMEX_TEST_DIRECT_INIT 0
#else
#define LUMEX_TEST_DIRECT_INIT 1
#endif

namespace
{
// Overload set that takes each kind of path: a lumex path must pick its own,
// a standard path its own.
int
pick (path const &)
{
  return 1;
}

int
pick (stdfs::path const &)
{
  return 2;
}

int
pick_lumex_only (path const &)
{
  return 1;
}

std::string
pick_std_only (stdfs::path const &value)
{
  return value.generic_string ();
}

// UTF-8 bytes of "e acute, a CJK character, sharp s": the narrow path of both
// libraries holds them unchanged on POSIX.
char const *const kNonAscii = "dir/\xC3\xA9\xE6\x97\xA5\xC3\x9F/file.txt";
} // namespace

static_assert (std::is_convertible<path, stdfs::path>::value,
               "a lumex path converts to std::filesystem::path");
static_assert (std::is_convertible<stdfs::path, path>::value,
               "std::filesystem::path converts to a lumex path");
static_assert (std::is_convertible<path const &, stdfs::path>::value,
               "a constant lumex path converts too");
static_assert (std::is_convertible<stdfs::path const &, path>::value,
               "a constant standard path converts too");
// What was accepted before is accepted still.
static_assert (std::is_convertible<std::string, path>::value,
               "a std::string is still a path");
static_assert (std::is_convertible<char const *, path>::value,
               "a C string is still a path");
static_assert (std::is_convertible<path, std::string>::value,
               "a lumex path still converts to std::string");
static_assert (!std::is_convertible<int, path>::value,
               "a number is not a path");

TEST (LumexFilesystemStdPathTest, GivenLumexPath_WhenConvertToStd_ThenSamePath)
{
  path const source ("some/dir/file.txt");
  stdfs::path const converted = source;
  EXPECT_EQ (converted.generic_string (), "some/dir/file.txt");
  EXPECT_EQ (converted.filename (), "file.txt");
  EXPECT_EQ (converted.extension (), ".txt");
#if LUMEX_TEST_DIRECT_INIT
  stdfs::path const direct (source);
  EXPECT_EQ (direct, converted);
  EXPECT_EQ (static_cast<stdfs::path> (source), converted);
#endif
}

TEST (LumexFilesystemStdPathTest, GivenStdPath_WhenConvertToLumex_ThenSamePath)
{
  stdfs::path const source ("some/dir/file.txt");
  path const converted = source;
  EXPECT_EQ (converted.string (), "some/dir/file.txt");
  EXPECT_EQ (converted.filename ().string (), "file.txt");
  path const direct (source);
  EXPECT_EQ (direct, converted);
  EXPECT_EQ (static_cast<path> (source), converted);
}

TEST (LumexFilesystemStdPathTest, GivenRoundTrip_WhenConvertBack_ThenUnchanged)
{
  for (char const *text : { "a", "a/b", "/abs/path", "rel/../x", "./y",
                            "with space/and.dots.txt", "a/b/", "/", "." })
    {
      path const lumex_path (text);
      stdfs::path const standard = lumex_path;
      path const back = standard;
      EXPECT_EQ (back.string (), text) << text;
      EXPECT_EQ (standard.string (), std::string (text)) << text;
      stdfs::path const from_standard (text);
      path const lumex_from_standard = from_standard;
      stdfs::path const again = lumex_from_standard;
      EXPECT_EQ (again, from_standard) << text;
    }
}

TEST (LumexFilesystemStdPathTest, GivenEmptyPath_WhenConvert_ThenEmptyBothWays)
{
  path const lumex_empty;
  stdfs::path const standard = lumex_empty;
  EXPECT_TRUE (standard.empty ());
  stdfs::path const standard_empty;
  path const lumex_back = standard_empty;
  EXPECT_TRUE (lumex_back.empty ());
  EXPECT_EQ (lumex_back.string (), "");
}

TEST (LumexFilesystemStdPathTest, GivenTrailingSeparator_WhenConvert_ThenKept)
{
  path const lumex_path ("dir/sub/");
  stdfs::path const standard = lumex_path;
  EXPECT_EQ (standard.string (), "dir/sub/");
  EXPECT_TRUE (standard.has_relative_path ());
  EXPECT_FALSE (standard.has_filename ());
  path const back = standard;
  EXPECT_EQ (back.string (), "dir/sub/");
  stdfs::path const from_standard ("x/y//");
  path const lumex_back = from_standard;
  EXPECT_EQ (lumex_back.string (), "x/y//");
}

#if !defined(LUMEX_OS_WINDOWS)
TEST (LumexFilesystemStdPathTest, GivenNonAsciiPath_WhenConvert_ThenBytesKept)
{
  // POSIX: the bytes of the narrow path are the native path of both classes.
  path const lumex_path (kNonAscii);
  stdfs::path const standard = lumex_path;
  EXPECT_EQ (standard.string (), kNonAscii);
  EXPECT_EQ (standard.filename ().string (), "file.txt");
  EXPECT_EQ (standard.parent_path ().filename ().string (),
             "\xC3\xA9\xE6\x97\xA5\xC3\x9F");
  path const back = standard;
  EXPECT_EQ (back.string (), kNonAscii);
  EXPECT_EQ (back.u8string (), kNonAscii);
  stdfs::path const from_standard (std::string ("\xE6\x97\xA5/\xC3\xA9"));
  path const lumex_back = from_standard;
  EXPECT_EQ (lumex_back.string (), "\xE6\x97\xA5/\xC3\xA9");
}
#else
TEST (LumexFilesystemStdPathTest, GivenNonAsciiPath_WhenConvert_ThenSameWide)
{
  // Windows: the narrow path is UTF-8 and the standard path is wide.
  path const lumex_path (kNonAscii);
  stdfs::path const standard = lumex_path;
  EXPECT_EQ (standard.wstring (), lumex_path.wstring ());
  path const back = standard;
  EXPECT_EQ (back.string (), kNonAscii);
}
#endif

TEST (LumexFilesystemStdPathTest, GivenMixedPaths_WhenCompare_ThenNoAmbiguity)
{
  path const a ("dir/a");
  path const b ("dir/b");
  stdfs::path const std_a ("dir/a");
  stdfs::path const std_b ("dir/b");
  EXPECT_TRUE (a == std_a);
  EXPECT_TRUE (std_a == a);
  EXPECT_FALSE (a != std_a);
  EXPECT_FALSE (std_a != a);
  EXPECT_TRUE (a != std_b);
  EXPECT_TRUE (std_b != a);
  EXPECT_TRUE (a < std_b);
  EXPECT_TRUE (std_a < b);
  EXPECT_FALSE (b < std_a);
  EXPECT_TRUE (b > std_a);
  EXPECT_TRUE (std_b > a);
  EXPECT_TRUE (a <= std_a);
  EXPECT_TRUE (std_a <= a);
  EXPECT_TRUE (a <= std_b);
  EXPECT_TRUE (b >= std_a);
  EXPECT_TRUE (std_b >= b);
  EXPECT_FALSE (a >= std_b);
  EXPECT_FALSE (std_a >= b);
  EXPECT_FALSE (std_b <= a);
  EXPECT_FALSE (b <= std_a);
  EXPECT_FALSE (std_a > a);
  EXPECT_FALSE (a > std_b);
  EXPECT_FALSE (std_b < a);
  // The comparisons of the same classes are untouched.
  EXPECT_TRUE (a == path ("dir/a"));
  EXPECT_TRUE (std_a == stdfs::path ("dir/a"));
  EXPECT_TRUE (a == "dir/a");
  EXPECT_TRUE (a == std::string ("dir/a"));
}

TEST (LumexFilesystemStdPathTest,
      GivenMixedPaths_WhenJoin_ThenTypeOfLeftOperand)
{
  path const lumex_base ("base");
  stdfs::path const std_base ("base");
  path const lumex_tail ("tail");
  stdfs::path const std_tail ("tail");
  static_assert (std::is_same<decltype (lumex_base / std_tail), path>::value,
                 "lumex / std is a lumex path");
  static_assert (
      std::is_same<decltype (std_base / lumex_tail), stdfs::path>::value,
      "std / lumex is a standard path");
  EXPECT_EQ ((lumex_base / std_tail).string (), "base/tail");
  EXPECT_EQ ((std_base / lumex_tail).generic_string (), "base/tail");
  // The other operands keep their calls.
  EXPECT_EQ ((lumex_base / "x").string (), "base/x");
  EXPECT_EQ (("x" / lumex_base).string (), "x/base");
  EXPECT_EQ ((lumex_base / std::string ("x")).string (), "base/x");
  EXPECT_EQ ((lumex_base / lumex_tail).string (), "base/tail");
  EXPECT_EQ ((std_base / std_tail).generic_string (), "base/tail");
  path appended ("p");
  appended /= std_tail;
  EXPECT_EQ (appended.string (), "p/tail");
  appended += std_tail;
  EXPECT_EQ (appended.string (), "p/tailtail");
  appended /= "z";
  appended /= std::string ("w");
  EXPECT_EQ (appended.string (), "p/tailtail/z/w");
}

TEST (LumexFilesystemStdPathTest,
      GivenOverloads_WhenCallWithEachPath_ThenOwnOverload)
{
  path const lumex_path ("x");
  stdfs::path const standard ("x");
  EXPECT_EQ (pick (lumex_path), 1);
  EXPECT_EQ (pick (standard), 2);
  // One overload only: the other kind of path converts.
  EXPECT_EQ (pick_lumex_only (standard), 1);
  EXPECT_EQ (pick_std_only (lumex_path), "x");
  EXPECT_EQ (pick_lumex_only (std::string ("x")), 1);
  EXPECT_EQ (pick_lumex_only ("x"), 1);
}

TEST (LumexFilesystemStdPathTest,
      GivenContainers_WhenStoreEitherPath_ThenConverted)
{
  path const lumex_path ("one");
  stdfs::path const standard ("two");
  std::vector<stdfs::path> standards;
  standards.push_back (lumex_path);
  standards.push_back (standard);
  std::vector<path> lumexes;
  lumexes.push_back (standard);
  lumexes.push_back (lumex_path);
#if LUMEX_TEST_DIRECT_INIT
  standards.emplace_back (lumex_path);
  lumexes.emplace_back (standard);
  EXPECT_EQ (standards.back ().string (), "one");
  EXPECT_EQ (lumexes.back ().string (), "two");
#endif
  EXPECT_EQ (standards.front ().string (), "one");
  EXPECT_EQ (lumexes.front ().string (), "two");
  // POSIX, known limit: assigning a lumex path to a standard path is
  // ambiguous, as the standard assignment operators for path&&, const path&
  // and string_type&& are all reachable through a different conversion of the
  // lumex path (it has converted to std::string for long). It compiled before
  // through the string; now the standard path is built explicitly. On Windows
  // the standard string type is wide, so the assignment works.
#if defined(LUMEX_OS_WINDOWS)
  static_assert (std::is_assignable<stdfs::path &, path const &>::value,
                 "the assignment works on Windows");
#else
  static_assert (!std::is_assignable<stdfs::path &, path const &>::value,
                 "the assignment is ambiguous");
#endif
#if LUMEX_TEST_DIRECT_INIT
  stdfs::path assigned;
  assigned = stdfs::path (lumex_path);
  EXPECT_EQ (assigned.string (), "one");
#endif
  path lumex_assigned;
  lumex_assigned = standard;
  EXPECT_EQ (lumex_assigned.string (), "two");
}

TEST (LumexFilesystemStdPathTest,
      GivenStdPath_WhenCallFilesystem_ThenWorksOnDisk)
{
  stdfs::path const root
      = stdfs::temp_directory_path () / "LumexFilesystemStdPathTest_tree";
  stdfs::remove_all (root);
  stdfs::path const nested = root / "a" / "b";
  // A standard path where the library takes a lumex path.
  EXPECT_TRUE (lumex_filesystem::create_directories (nested).success ());
  EXPECT_TRUE (lumex_filesystem::exists (nested));
  EXPECT_TRUE (lumex_filesystem::is_directory (nested));
  EXPECT_TRUE (stdfs::is_directory (nested));
  // A lumex path where the standard library takes a standard path.
  path const lumex_nested = nested;
  EXPECT_TRUE (stdfs::exists (lumex_nested));
  EXPECT_TRUE (stdfs::is_directory (lumex_nested));
  EXPECT_TRUE (lumex_filesystem::remove_all (root).success ());
  EXPECT_FALSE (stdfs::exists (root));
}

TEST (LumexFilesystemStdPathTest, GivenPaths_WhenStream_ThenEachItsOwnInserter)
{
  std::ostringstream lumex_out;
  lumex_out << path ("dir/file");
  EXPECT_EQ (lumex_out.str (), "dir/file");
  std::ostringstream std_out;
  std_out << stdfs::path ("dir/file");
  EXPECT_EQ (std_out.str (), "\"dir/file\"");
}

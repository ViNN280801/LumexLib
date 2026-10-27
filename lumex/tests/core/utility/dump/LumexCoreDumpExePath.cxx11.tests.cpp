// LumexCoreDumpExePath.cxx11.tests.cpp
//
// The default dump directory of the generator is "<directory of the
// executable>/dumps". An executable in the root directory (for example
// "/server") has an empty directory part, which is an answer and not a
// refusal: the dumps go to "/dumps". Only a Windows executable path that is
// not valid UTF-16 is refused (the flag, never an empty string). The path
// splitting is a private static function; these tests reach it the way the
// other private-member tests of the module do (an explicit instantiation
// ignores access checks).
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using lumex::core::utility::dump::core_dump_generator;

namespace
{
template <typename Tag> struct slot
{
  static typename Tag::type value;
};

template <typename Tag>
typename Tag::type slot<Tag>::value = typename Tag::type ();

/// Stores `Member` in the slot of `Tag` when the program starts.
template <typename Tag, typename Tag::type Member> struct reveal
{
  reveal () { slot<Tag>::value = Member; }

  static reveal const instance;
};

template <typename Tag, typename Tag::type Member>
reveal<Tag, Member> const reveal<Tag, Member>::instance;

struct directory_of_path_tag
{
  using type = std::string (*) (std::string const &);
};

struct default_directory_tag
{
  using type = bool (*) (std::string &);
};

template struct reveal<directory_of_path_tag,
                       &core_dump_generator::_directory_of_path>;
template struct reveal<default_directory_tag,
                       &core_dump_generator::_default_dump_directory>;
} // namespace

TEST (LumexCoreDumpExePathTest,
      GivenPathWithoutSeparator_WhenDirectoryOfPath_ThenDot)
{
  EXPECT_EQ (slot<directory_of_path_tag>::value ("server"), ".");
  EXPECT_EQ (slot<directory_of_path_tag>::value (""), ".");
}

TEST (LumexCoreDumpExePathTest,
      GivenPathWithDirectories_WhenDirectoryOfPath_ThenTheDirectoryPart)
{
  EXPECT_EQ (slot<directory_of_path_tag>::value ("/usr/bin/server"),
             "/usr/bin");
  EXPECT_EQ (slot<directory_of_path_tag>::value ("/opt/server"), "/opt");
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (slot<directory_of_path_tag>::value ("C:\\tools\\server.exe"),
             "C:\\tools");
#endif
}

// The root case: the directory part is empty and that is not an error; the
// dump directory built from it is "/dumps", as before the refusal flag.
TEST (LumexCoreDumpExePathTest,
      GivenExecutableInTheRootDirectory_WhenDirectoryOfPath_ThenEmptyNotDot)
{
  EXPECT_EQ (slot<directory_of_path_tag>::value ("/server"), "");
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (slot<directory_of_path_tag>::value ("\\server.exe"), "");
#endif
}

TEST (LumexCoreDumpExePathTest,
      GivenRunningExecutable_WhenDefaultDumpDirectory_ThenSucceedsWithDumps)
{
  std::string directory = "left over";
  EXPECT_TRUE (slot<default_directory_tag>::value (directory));
  ASSERT_GT (directory.size (), std::string ("/dumps").size () - 1);
  EXPECT_EQ (directory.substr (directory.size () - 6), "/dumps");
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

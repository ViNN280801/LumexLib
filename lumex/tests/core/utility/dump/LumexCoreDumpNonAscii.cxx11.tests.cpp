// LumexCoreDumpNonAscii.cxx11.tests.cpp
//
// The dump generator on Windows with a non-ASCII directory. The paths were
// widened byte by byte (`std::wstring (s.begin (), s.end ())`) and the
// executable directory came from GetModuleFileNameA, so a Cyrillic directory
// named another directory or none. The directories and files of these tests
// are made with the wide Win32 API (lumex_test::utf8_files), never with the
// converter under test; the children are copies of this binary started from
// a Cyrillic folder (LumexTestCyrillicCopy.hpp). The tests are about Windows
// only; on other systems the file is empty.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#include "lumex/tests/support/LumexTestCyrillicCopy.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if LUMEX_OS_WINDOWS
using lumex::core::utility::dump::core_dump_generator;
using lumex::core::utility::dump::DumpType;
namespace copy = lumex_test::cyrillic_copy;
namespace files = lumex_test::utf8_files;

namespace
{
// --- Access to the private functions ---

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

struct create_directory_tag
{
  using type = bool (*) (std::string const &);
};

struct create_file_tag
{
  using type = bool (*) (std::string const &, std::string const &);
};

struct create_directory_atomic_tag
{
  using type = bool (*) (std::string const &);
};

template struct reveal<create_directory_atomic_tag,
                       &core_dump_generator::_create_directory_atomically>;
template struct reveal<create_directory_tag,
                       &core_dump_generator::_create_directory_recursive>;
template struct reveal<create_file_tag,
                       &core_dump_generator::_create_file_atomically>;

/// The names in `directory` that end with `.dmp`.
std::size_t
count_dumps (std::string const &directory)
{
  std::size_t count = 0;
  std::vector<std::string> const names = copy::entries (directory);
  for (std::size_t i = 0; i < names.size (); ++i)
    if (names[i].size () > 4
        && names[i].compare (names[i].size () - 4, 4, ".dmp") == 0)
      ++count;
  return count;
}

/// A fresh parent directory with a Cyrillic name; the caller removes it.
std::string
make_parent (std::string const &suffix)
{
  std::string const parent
      = copy::self_directory () + "/" + files::cyrillic (suffix);
  copy::remove_tree (parent);
  return files::make_directory (parent) ? parent : std::string ();
}
} // namespace

TEST (LumexCoreDumpNonAsciiTest,
      GivenCyrillicPath_WhenCreateDirectoryRecursive_ThenItExistsUnderItsName)
{
  std::string const parent = make_parent ("_dump_dir");
  ASSERT_FALSE (parent.empty ());
  std::string const nested
      = parent + "/" + files::cyrillic ("_a") + "/" + files::cyrillic ("_b");

  bool const created = slot<create_directory_tag>::value (nested);

  // The name asked for exists, made by the wide API: a byte by byte widening
  // made a directory of the bytes of the letters.
  bool const exists = files::exists (nested);
  copy::remove_tree (parent);
  EXPECT_TRUE (created);
  EXPECT_TRUE (exists);
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenCyrillicFileName_WhenCreateFileAtomically_ThenTheFileHoldsTheText)
{
  std::string const parent = make_parent ("_dump_file");
  ASSERT_FALSE (parent.empty ());
  std::string const file = parent + "/" + files::cyrillic (".txt");

  bool const created = slot<create_file_tag>::value (file, "dump text");

  std::string content;
  bool const read = files::read_file (file, content);
  copy::remove_tree (parent);
  EXPECT_TRUE (created);
  ASSERT_TRUE (read);
  EXPECT_EQ (content, "dump text");
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenCyrillicDirectory_WhenCreateDirectoryAtomically_ThenItExists)
{
  std::string const parent = make_parent ("_dump_atomic");
  ASSERT_FALSE (parent.empty ());
  std::string const nested = parent + "/" + files::cyrillic ("_once");

  bool const first = slot<create_directory_atomic_tag>::value (nested);
  bool const exists = files::exists (nested);
  copy::remove_tree (parent);
  EXPECT_TRUE (first);
  EXPECT_TRUE (exists);
}

// A path that is not UTF-8 is refused: a skipped or replaced byte would
// create another directory (0xFF is the letter y with a diaeresis in
// Latin-1, which the byte by byte widening made of it).
TEST (LumexCoreDumpNonAsciiTest,
      GivenPathThatIsNotUtf8_WhenCreateDirectoryRecursive_ThenNothingIsCreated)
{
  std::string const parent = make_parent ("_dump_bad");
  ASSERT_FALSE (parent.empty ());
  std::string const bad = parent + "/a\xFF";

  bool const created = slot<create_directory_tag>::value (bad);

  bool const another = files::exists (parent + "/a");
  bool const latin1
      = GetFileAttributesW ((files::wide (parent) + L"\\a\u00FF").c_str ())
        != INVALID_FILE_ATTRIBUTES;
  copy::remove_tree (parent);
  EXPECT_FALSE (created);
  EXPECT_FALSE (another);
  EXPECT_FALSE (latin1);
}

// Child: starts the generator without a directory (the dump directory is
// "<executable directory>/dumps") and writes a dump by hand.
TEST (LumexCoreDumpNonAsciiTest, ChildWritesDumpNextToExecutable)
{
  if (!copy::is_child ())
    GTEST_SKIP () << "runs only as the child of the test below";
  core_dump_generator::initialize ("", DumpType::MINI_DUMP_NORMAL, false);
  EXPECT_TRUE (core_dump_generator::generate_dump ("non-ASCII directory"));
  EXPECT_EQ (core_dump_generator::get_dump_directory (),
             copy::self_directory () + "/dumps");
}

// Child: installs the exception handler and crashes with an access
// violation. gtest does not catch it (--gtest_catch_exceptions=0), so the
// handler of the generator writes the dump.
TEST (LumexCoreDumpNonAsciiTest, ChildCrashes)
{
  if (!copy::is_child ())
    GTEST_SKIP () << "runs only as the child of the test below";
  SetErrorMode (SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
  core_dump_generator::initialize ("", DumpType::MINI_DUMP_NORMAL, true);
  *static_cast<volatile int *> (nullptr) = 1;
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenBinaryInCyrillicDirectory_WhenDumpIsWritten_ThenItIsNextToTheBinary)
{
  if (copy::is_child ())
    GTEST_SKIP () << "the child runs the test above";
  std::string directory;
  ASSERT_TRUE (copy::make_copy ("_dump_manual", directory));

  int const code = copy::run (
      directory, "LumexCoreDumpNonAsciiTest.ChildWritesDumpNextToExecutable");

  std::size_t const dumps = count_dumps (directory + "/dumps");
  copy::remove_tree (directory);
  EXPECT_EQ (code, 0);
  EXPECT_GT (dumps, 0u) << "no .dmp file in <Cyrillic directory>/dumps";
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenBinaryInCyrillicDirectory_WhenItCrashes_ThenTheHandlerWritesTheDump)
{
  if (copy::is_child ())
    GTEST_SKIP () << "the child runs the test above";
  std::string directory;
  ASSERT_TRUE (copy::make_copy ("_dump_crash", directory));

  int const code
      = copy::run (directory, "LumexCoreDumpNonAsciiTest.ChildCrashes",
                   "--gtest_catch_exceptions=0");

  std::size_t const dumps = count_dumps (directory + "/dumps");
  copy::remove_tree (directory);
  EXPECT_EQ (code, static_cast<int> (EXCEPTION_ACCESS_VIOLATION));
  EXPECT_GT (dumps, 0u) << "the handler wrote no .dmp file";
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

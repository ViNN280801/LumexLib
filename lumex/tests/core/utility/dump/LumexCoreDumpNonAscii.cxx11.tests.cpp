// LumexCoreDumpNonAscii.cxx11.tests.cpp
//
// The dump generator on Windows with a non-ASCII directory. The paths were
// widened byte by byte (`std::wstring (s.begin (), s.end ())`) and the
// executable directory came from GetModuleFileNameA, so a Cyrillic directory
// named another directory or none. The directories and files of these tests
// are made with the wide Win32 API (lumex_test::non_ascii), never with the
// converter under test; the children are copies of this binary started from
// a Cyrillic folder (LumexTestNonAsciiDirectory.hpp). The tests are about
// Windows only; on other systems the file is empty.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#include "lumex/tests/support/LumexTestNonAsciiDirectory.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if LUMEX_OS_WINDOWS
using lumex::core::utility::dump::core_dump_generator;
using lumex::core::utility::dump::DumpType;
namespace na = lumex_test::non_ascii;

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

template struct reveal<create_directory_tag,
                       &core_dump_generator::_create_directory_recursive>;
template struct reveal<create_file_tag,
                       &core_dump_generator::_create_file_atomically>;

/// A Cyrillic name as UTF-16 code units.
wchar_t const *const kNestedName
    = L"\u0412\u043B\u043E\u0436\u0435\u043D\u043D\u0430\u044F";
/// A Cyrillic name with the extension .txt as UTF-16 code units.
wchar_t const *const kFileName = L"\u0424\u0430\u0439\u043B.txt";

bool
is_directory (std::wstring const &path)
{
  DWORD const attributes = GetFileAttributesW (path.c_str ());
  return attributes != INVALID_FILE_ATTRIBUTES
         && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool
exists (std::wstring const &path)
{
  return GetFileAttributesW (path.c_str ()) != INVALID_FILE_ATTRIBUTES;
}
} // namespace

TEST (LumexCoreDumpNonAsciiTest,
      GivenCyrillicPath_WhenCreateDirectoryRecursive_ThenItExistsUnderItsName)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::wstring const nested = space.directory () + L"\\" + kNestedName;

  bool const created
      = slot<create_directory_tag>::value (na::utf8_of (nested));

  EXPECT_TRUE (created);
  EXPECT_TRUE (is_directory (nested));
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenCyrillicFileName_WhenCreateFileAtomically_ThenTheFileHoldsTheText)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::wstring const file = space.directory () + L"\\" + kFileName;

  bool const created
      = slot<create_file_tag>::value (na::utf8_of (file), "dump text");

  EXPECT_TRUE (created);
  std::string content;
  ASSERT_TRUE (na::read_bytes (file, content));
  EXPECT_EQ (content, "dump text");
}

// A path that is not UTF-8 is refused: a skipped or replaced byte would
// create another directory (0xFF is the letter y with a diaeresis in
// Latin-1, which the byte by byte widening made of it).
TEST (LumexCoreDumpNonAsciiTest,
      GivenPathThatIsNotUtf8_WhenCreateDirectoryRecursive_ThenNothingIsCreated)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::string const bad = na::utf8_of (space.root ()) + "\\a\xFF";

  EXPECT_FALSE (slot<create_directory_tag>::value (bad));

  EXPECT_FALSE (exists (space.root () + L"\\a\u00FF"));
  EXPECT_FALSE (exists (space.root () + L"\\a"));
}

// Child: starts the generator without a directory (the dump directory is
// "<executable directory>/dumps"), writes a dump by hand and reports the
// dump directory. Never runs by itself (DISABLED_).
TEST (LumexCoreDumpNonAsciiTest, DISABLED_ChildWritesDumpNextToExecutable)
{
  if (na::child_result_path ().empty ())
    GTEST_SKIP () << "started without a result file";
  core_dump_generator::initialize ("", DumpType::MINI_DUMP_NORMAL, false);
  EXPECT_TRUE (core_dump_generator::generate_dump ("non-ASCII directory"));
  std::string const directory = core_dump_generator::get_dump_directory ();
  EXPECT_TRUE (na::write_child_result (directory.data (), directory.size ()));
}

// Child: installs the exception handler, reports the dump directory and
// crashes with an access violation. gtest does not catch it
// (--gtest_catch_exceptions=0), so the handler of the generator writes the
// dump.
TEST (LumexCoreDumpNonAsciiTest, DISABLED_ChildCrashes)
{
  if (na::child_result_path ().empty ())
    GTEST_SKIP () << "started without a result file";
  SetErrorMode (SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
  core_dump_generator::initialize ("", DumpType::MINI_DUMP_NORMAL, true);
  std::string const directory = core_dump_generator::get_dump_directory ();
  EXPECT_TRUE (na::write_child_result (directory.data (), directory.size ()));
  *static_cast<volatile int *> (nullptr) = 1;
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenBinaryInCyrillicDirectory_WhenDumpIsWritten_ThenItIsNextToTheBinary)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::wstring const binary = space.copy_binary ();
  ASSERT_FALSE (binary.empty ());

  DWORD const code
      = space.run (binary, L"--gtest_filter=LumexCoreDumpNonAsciiTest."
                           L"DISABLED_ChildWritesDumpNextToExecutable "
                           L"--gtest_also_run_disabled_tests");

  ASSERT_EQ (code, 0u) << space.child_log ();
  std::string reported;
  ASSERT_TRUE (na::read_bytes (space.result_file (), reported));
  EXPECT_EQ (reported, na::utf8_of (space.directory ()) + "/dumps");
  EXPECT_TRUE (is_directory (space.directory () + L"\\dumps"));
  EXPECT_TRUE (na::has_file (space.directory () + L"\\dumps", L"*.dmp"));
}

TEST (LumexCoreDumpNonAsciiTest,
      GivenBinaryInCyrillicDirectory_WhenItCrashes_ThenTheHandlerWritesTheDump)
{
  na::workspace space;
  ASSERT_TRUE (space.ok ());
  std::wstring const binary = space.copy_binary ();
  ASSERT_FALSE (binary.empty ());

  DWORD const code = space.run (
      binary,
      L"--gtest_filter=LumexCoreDumpNonAsciiTest.DISABLED_ChildCrashes "
      L"--gtest_also_run_disabled_tests --gtest_catch_exceptions=0");

  EXPECT_EQ (code, static_cast<DWORD> (EXCEPTION_ACCESS_VIOLATION))
      << space.child_log ();
  std::string reported;
  ASSERT_TRUE (na::read_bytes (space.result_file (), reported));
  EXPECT_EQ (reported, na::utf8_of (space.directory ()) + "/dumps");
  EXPECT_TRUE (na::has_file (space.directory () + L"\\dumps", L"*.dmp"));
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

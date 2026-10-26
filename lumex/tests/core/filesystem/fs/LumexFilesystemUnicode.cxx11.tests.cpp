// lumex/tests/core/filesystem/fs/LumexFilesystemUnicode.cxx11.tests.cpp
//
// The conversions of a path between UTF-8 and wchar_t (to_wide_string,
// from_wide_string, path::wstring) are lumex::core::unicode::convert::to_wide
// and to_utf8: the same on every platform and in every C locale. They used to
// call mbstowcs and wcstombs on POSIX, which depend on the C locale, so a
// UTF-8 path gave an empty string in the "C" locale. Invalid input is skipped
// (WideCharToMultiByte and MultiByteToWideChar write U+FFFD instead). An
// operation on a file does not use that skipping: on Windows it refuses a path
// that is not valid UTF-8 (lumex_filesystem::to_wide_path), because the
// skipped name is the name of another file.
//
// A wchar_t is 4 bytes wide (UTF-32) on Linux and 2 bytes (UTF-16) on
// Windows; the expectations are written with \u and \U escapes, which the
// compiler turns into the encoding of the platform, and the tests that depend
// on the width branch on sizeof (wchar_t).
#include <algorithm>
#include <cerrno>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"

using lumex::core::filesystem::fs::lumex_filesystem;
using lumex::core::filesystem::fs::path;

namespace
{
// "Привет" (Russian) in UTF-8 and as wchar_t.
char const *const kPrivetUtf8
    = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
wchar_t const *const kPrivetWide = L"Привет";

// A 4-byte code point (U+1F600): one wchar_t on Linux, a pair on Windows.
char const *const kEmojiUtf8 = "\xF0\x9F\x98\x80";
wchar_t const *const kEmojiWide = L"\U0001F600";

// A path with Cyrillic and a 4-byte code point.
std::string
mixed_utf8 ()
{
  return std::string ("/tmp/") + kPrivetUtf8 + "/" + kEmojiUtf8 + ".txt";
}

std::wstring
mixed_wide ()
{
  return std::wstring (L"/tmp/") + kPrivetWide + L"/" + kEmojiWide + L".txt";
}

// Changes the C locale of the process and puts the old one back.
class LocaleGuard
{
public:
  LocaleGuard ()
  {
    char const *const current = std::setlocale (LC_ALL, nullptr);
    if (current != nullptr)
      m_saved = current;
  }

  ~LocaleGuard ()
  {
    if (!m_saved.empty ())
      std::setlocale (LC_ALL, m_saved.c_str ());
  }

  LocaleGuard (LocaleGuard const &) = delete;
  LocaleGuard &operator= (LocaleGuard const &) = delete;

  // True when the locale exists on this machine and is set now.
  bool
  set (char const *name)
  {
    return std::setlocale (LC_ALL, name) != nullptr;
  }

private:
  std::string m_saved;
};
} // namespace

TEST (LumexFilesystemUnicodeTest, GivenAscii_WhenConverted_ThenTheSameText)
{
  EXPECT_EQ (lumex_filesystem::to_wide_string ("dir/file.txt"),
             std::wstring (L"dir/file.txt"));
  EXPECT_EQ (lumex_filesystem::from_wide_string (L"dir/file.txt"),
             std::string ("dir/file.txt"));
}

TEST (LumexFilesystemUnicodeTest,
      GivenCyrillic_WhenConverted_ThenTheCodePoints)
{
  EXPECT_EQ (lumex_filesystem::to_wide_string (kPrivetUtf8),
             std::wstring (kPrivetWide));
  EXPECT_EQ (lumex_filesystem::from_wide_string (kPrivetWide),
             std::string (kPrivetUtf8));
}

TEST (LumexFilesystemUnicodeTest,
      GivenFourByteCodePoint_WhenConverted_ThenOneCharacterOrAPair)
{
  std::wstring const wide = lumex_filesystem::to_wide_string (kEmojiUtf8);
  EXPECT_EQ (wide, std::wstring (kEmojiWide));
  // UTF-16 where wchar_t has 2 bytes, UTF-32 where it has 4.
  EXPECT_EQ (wide.size (), sizeof (wchar_t) == 2 ? 2U : 1U);
  EXPECT_EQ (lumex_filesystem::from_wide_string (wide),
             std::string (kEmojiUtf8));
}

TEST (LumexFilesystemUnicodeTest, GivenMixedPath_WhenRoundTripped_ThenSame)
{
  std::string const narrow = mixed_utf8 ();
  std::wstring const wide = lumex_filesystem::to_wide_string (narrow);
  EXPECT_EQ (wide, mixed_wide ());
  EXPECT_EQ (lumex_filesystem::from_wide_string (wide), narrow);
}

TEST (LumexFilesystemUnicodeTest, GivenEmptyText_WhenConverted_ThenEmpty)
{
  EXPECT_TRUE (lumex_filesystem::to_wide_string (std::string ()).empty ());
  EXPECT_TRUE (lumex_filesystem::from_wide_string (std::wstring ()).empty ());
}

TEST (LumexFilesystemUnicodeTest,
      GivenInvalidUtf8_WhenToWide_ThenWhatCanNotBeDecodedIsSkipped)
{
  // A stray continuation byte, a byte that never occurs in UTF-8, and a
  // sequence cut short: each leaves no trace (no U+FFFD, no empty result).
  EXPECT_EQ (lumex_filesystem::to_wide_string ("a\x80z"),
             std::wstring (L"az"));
  EXPECT_EQ (lumex_filesystem::to_wide_string ("a\xFF"
                                               "b"),
             std::wstring (L"ab"));
  EXPECT_EQ (lumex_filesystem::to_wide_string ("ab\xE2\x82"),
             std::wstring (L"ab"));
  EXPECT_EQ (lumex_filesystem::to_wide_string ("\xFF\xFE"), std::wstring ());
  // The valid text around the bad bytes survives.
  EXPECT_EQ (lumex_filesystem::to_wide_string (std::string (kPrivetUtf8)
                                               + "\xC3" + "/x"),
             std::wstring (kPrivetWide) + L"/x");
}

TEST (LumexFilesystemUnicodeTest,
      GivenUnpairedSurrogate_WhenFromWide_ThenFollowsTheWidthOfWchar)
{
  std::wstring input (L"a");
  input.push_back (static_cast<wchar_t> (0xD800));
  input.push_back (L'b');
  if (sizeof (wchar_t) == 2)
    // UTF-16: the unpaired surrogate is skipped.
    EXPECT_EQ (lumex_filesystem::from_wide_string (input), "ab");
  else
    // UTF-32 units are not validated by the unicode module: U+D800 is written
    // as three bytes, neither skipped nor an error.
    EXPECT_EQ (lumex_filesystem::from_wide_string (input),
               std::string ("a\xED\xA0\x80"
                            "b"));
}

TEST (LumexFilesystemUnicodeTest,
      GivenEmbeddedZero_WhenConverted_ThenTheWholeStringIsConverted)
{
  std::string const narrow ("a\0b", 3);
  std::wstring const wide = lumex_filesystem::to_wide_string (narrow);
  ASSERT_EQ (wide.size (), 3U);
  EXPECT_EQ (wide[1], L'\0');
  EXPECT_EQ (lumex_filesystem::from_wide_string (wide), narrow);
}

TEST (LumexFilesystemUnicodeTest,
      GivenCyrillicPath_WhenWstring_ThenTheWideCodePoints)
{
  path const cyrillic (mixed_utf8 ());
  EXPECT_EQ (cyrillic.wstring (), mixed_wide ());
  EXPECT_EQ (path (std::string (kPrivetUtf8)).wstring (),
             std::wstring (kPrivetWide));
  EXPECT_TRUE (path ().wstring ().empty ());
}

// The public conversion keeps skipping (see the file description); the
// operations on a file do not use it for a path that is not UTF-8, they refuse
// it on Windows (LumexFilesystemInvalidUtf8Test below).
TEST (LumexFilesystemUnicodeTest,
      GivenInvalidBytesInPath_WhenWstring_ThenSkippedNotEmpty)
{
  EXPECT_EQ (path ("dir/a\xFF"
                   "b")
                 .wstring (),
             std::wstring (L"dir/ab"));
}

// The bug: mbstowcs and wcstombs read the C locale, and in the "C" locale a
// byte above 0x7F is an error, so a UTF-8 path gave an empty string.
TEST (LumexFilesystemUnicodeTest,
      GivenCLocale_WhenUtf8PathConverted_ThenTheTextNotAnEmptyString)
{
  LocaleGuard locale;
  ASSERT_TRUE (locale.set ("C"));
  EXPECT_EQ (lumex_filesystem::to_wide_string (kPrivetUtf8),
             std::wstring (kPrivetWide));
  EXPECT_EQ (lumex_filesystem::from_wide_string (kPrivetWide),
             std::string (kPrivetUtf8));
  EXPECT_EQ (lumex_filesystem::to_wide_string (mixed_utf8 ()), mixed_wide ());
  EXPECT_EQ (lumex_filesystem::from_wide_string (mixed_wide ()),
             mixed_utf8 ());
  EXPECT_EQ (path (mixed_utf8 ()).wstring (), mixed_wide ());
}

TEST (LumexFilesystemUnicodeTest,
      GivenAnyAvailableLocale_WhenConverted_ThenTheResultDoesNotChange)
{
  // The names differ between systems; a locale that is not installed is
  // skipped. "C" is always there. A single-byte locale ("russian" is
  // ISO-8859-5 on glibc) reads the same bytes as other characters through
  // mbstowcs; the conversion of the library must not.
  char const *const names[]
      = { "C",           "POSIX",        "C.UTF-8",
          "en_US.UTF-8", "ru_RU.UTF-8",  "ru_RU.utf8",
          "russian",     "ru_RU.KOI8-R", "ru_RU.ISO-8859-5" };
  LocaleGuard locale;
  std::size_t applied = 0;
  for (char const *name : names)
    {
      if (!locale.set (name))
        continue;
      ++applied;
      SCOPED_TRACE (name);
      EXPECT_EQ (lumex_filesystem::to_wide_string (mixed_utf8 ()),
                 mixed_wide ());
      EXPECT_EQ (lumex_filesystem::from_wide_string (mixed_wide ()),
                 mixed_utf8 ());
    }
  EXPECT_GE (applied, 1U);
}

// --- Names on disk ---------------------------------------------------------
//
// The bug: on Windows exists, is_directory, is_regular_file and
// create_directory passed the UTF-8 bytes of the path to the ...A functions,
// which read them in the ANSI code page. A Cyrillic directory was created
// under a garbled name, and a file created by another program under its real
// name was not found. The checks below ask the system itself, through its wide
// interface on Windows and through the bytes on POSIX, so they see the name
// the way every other program does.
namespace
{
class LumexFilesystemUnicodeDiskTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // One directory per case: gtest_discover_tests runs every case in its
    // own process, and a leftover of another case must not be seen. Its name
    // is ASCII, so creating it does not depend on the fix under test. It may
    // stay from a run that ended before TearDown (or from the ...A functions,
    // which left garbled names in it that TearDown does not know).
    m_base = path (
        std::string ("lumex_fs_unicode_")
        + ::testing::UnitTest::GetInstance ()->current_test_info ()->name ());
    ASSERT_TRUE (native_create_directory (m_base)
                 || native_is_directory (m_base));
    m_created.push_back (std::make_pair (m_base, true));
  }

  void
  TearDown () override
  {
    remove_tree ();
  }

  // Asks the system whether `p` is a directory.
  static bool
  native_is_directory (path const &p)
  {
#if defined(_WIN32)
    DWORD const attrs = GetFileAttributesW (p.wstring ().c_str ());
    return attrs != INVALID_FILE_ATTRIBUTES
           && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat status;
    return stat (p.c_str (), &status) == 0 && S_ISDIR (status.st_mode);
#endif
  }

  static bool
  native_create_directory (path const &p)
  {
#if defined(_WIN32)
    return CreateDirectoryW (p.wstring ().c_str (), nullptr) != 0;
#else
    return mkdir (p.c_str (), 0755) == 0;
#endif
  }

  static bool
  native_create_file (path const &p, std::string const &content = "")
  {
#if defined(_WIN32)
    HANDLE const file
        = CreateFileW (p.wstring ().c_str (), GENERIC_WRITE, 0, nullptr,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
      return false;
    DWORD written = 0;
    bool const ok = content.empty ()
                    || (WriteFile (file, content.data (),
                                   static_cast<DWORD> (content.size ()),
                                   &written, nullptr)
                            != 0
                        && written == content.size ());
    CloseHandle (file);
    return ok;
#else
    int const descriptor
        = open (p.c_str (), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (descriptor < 0)
      return false;
    bool const ok = content.empty ()
                    || write (descriptor, content.data (), content.size ())
                           == static_cast<ssize_t> (content.size ());
    close (descriptor);
    return ok;
#endif
  }

  // Asks the system whether there is anything under the name `p`.
  static bool
  native_exists (path const &p)
  {
#if defined(_WIN32)
    return GetFileAttributesW (p.wstring ().c_str ())
           != INVALID_FILE_ATTRIBUTES;
#else
    struct stat status;
    return lstat (p.c_str (), &status) == 0;
#endif
  }

  // The size the system reports; -1 when there is no such file.
  static long long
  native_file_size (path const &p)
  {
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW (p.wstring ().c_str (), GetFileExInfoStandard,
                              &data)
        == 0)
      return -1;
    return (static_cast<long long> (data.nFileSizeHigh) << 32)
           | static_cast<long long> (data.nFileSizeLow);
#else
    struct stat status;
    if (stat (p.c_str (), &status) != 0)
      return -1;
    return static_cast<long long> (status.st_size);
#endif
  }

  // Whether the owner may not write the file.
  static bool
  native_is_read_only (path const &p)
  {
#if defined(_WIN32)
    DWORD const attrs = GetFileAttributesW (p.wstring ().c_str ());
    return attrs != INVALID_FILE_ATTRIBUTES
           && (attrs & FILE_ATTRIBUTE_READONLY) != 0;
#else
    struct stat status;
    return stat (p.c_str (), &status) == 0 && (status.st_mode & S_IWUSR) == 0;
#endif
  }

  // Remembers a file (false) or a directory (true) for TearDown, which removes
  // them in reverse order.
  void
  created (path const &p, bool directory)
  {
    m_created.push_back (std::make_pair (p, directory));
  }

  path m_base;

private:
  // Removes what the case made, deepest first, through the system and not
  // through remove_all of the module: a case that fails may leave a name the
  // module would not find, and the cleanup must not depend on the code under
  // test.
  void
  remove_tree ()
  {
    for (std::size_t i = m_created.size (); i > 0; --i)
      {
        path const &p = m_created[i - 1].first;
#if defined(_WIN32)
        if (m_created[i - 1].second)
          RemoveDirectoryW (p.wstring ().c_str ());
        else
          {
            // DeleteFileW refuses a read-only file (the permissions case).
            SetFileAttributesW (p.wstring ().c_str (), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW (p.wstring ().c_str ());
          }
#else
        if (m_created[i - 1].second)
          rmdir (p.c_str ());
        else
          unlink (p.c_str ());
#endif
      }
    m_created.clear ();
  }

  std::vector<std::pair<path, bool>> m_created;
};
} // namespace

TEST_F (
    LumexFilesystemUnicodeDiskTest,
    GivenCyrillicAndEmojiDirectories_WhenCreateDirectories_ThenTheSystemSeesTheirNames)
{
  path const cyrillic = m_base / kPrivetUtf8;
  path const emoji = cyrillic / kEmojiUtf8;
  // Both directories are made by the call; TearDown removes them even when an
  // expectation fails.
  created (cyrillic, true);
  created (emoji, true);

  auto const result = lumex_filesystem::create_directories (emoji);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_TRUE (result.value ());
  EXPECT_TRUE (native_is_directory (cyrillic));
  EXPECT_TRUE (native_is_directory (emoji));
  EXPECT_TRUE (lumex_filesystem::exists (emoji));
  EXPECT_TRUE (lumex_filesystem::is_directory (emoji));
}

TEST_F (
    LumexFilesystemUnicodeDiskTest,
    GivenExistingCyrillicDirectory_WhenCreateDirectory_ThenSuccessWithoutCreating)
{
  path const cyrillic = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (cyrillic));
  created (cyrillic, true);

  auto const result = lumex_filesystem::create_directory (cyrillic);

  // ERROR_ALREADY_EXISTS is not an error when the name is a directory.
  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_FALSE (result.value ());
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenQueried_ThenItExistsAsARegularFile)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file));
  created (file, false);

  EXPECT_TRUE (lumex_filesystem::exists (file));
  EXPECT_TRUE (lumex_filesystem::is_regular_file (file));
  EXPECT_FALSE (lumex_filesystem::is_directory (file));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenMissingCyrillicName_WhenQueried_ThenItDoesNotExist)
{
  path const missing = m_base / kPrivetUtf8;

  EXPECT_FALSE (lumex_filesystem::exists (missing));
  EXPECT_FALSE (lumex_filesystem::is_directory (missing));
  EXPECT_FALSE (lumex_filesystem::is_regular_file (missing));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenStatus_ThenARegularFile)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file));
  created (file, false);

  auto const status = lumex_filesystem::status (file);
  auto const link_status = lumex_filesystem::symlink_status (file);

  ASSERT_TRUE (status.success ()) << "error " << status.error_code ();
  EXPECT_EQ (status.value ().type (),
             lumex::core::filesystem::fs::file_type::regular);
  ASSERT_TRUE (link_status.success ())
      << "error " << link_status.error_code ();
  EXPECT_EQ (link_status.value ().type (),
             lumex::core::filesystem::fs::file_type::regular);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenFileSizeAndResize_ThenTheSystemAgrees)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file, "12345"));
  created (file, false);

  auto const size = lumex_filesystem::file_size (file);
  ASSERT_TRUE (size.success ()) << "error " << size.error_code ();
  EXPECT_EQ (size.value (), 5U);

  auto const resized = lumex_filesystem::resize_file (file, 2);
  ASSERT_TRUE (resized.success ()) << "error " << resized.error_code ();
  EXPECT_EQ (native_file_size (file), 2);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenLastWriteTimeSet_ThenItReadsBack)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file));
  created (file, false);
  // 14.11.2023 22:13:20 UTC: whole seconds, far from now.
  std::time_t const when = 1700000000;

  auto const set = lumex_filesystem::last_write_time (file, when);
  ASSERT_TRUE (set.success ()) << "error " << set.error_code ();
  auto const got = lumex_filesystem::last_write_time (file);

  ASSERT_TRUE (got.success ()) << "error " << got.error_code ();
  EXPECT_EQ (got.value (), when);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenPermissionsWithoutWrite_ThenReadOnly)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file));
  created (file, false);
  using lumex::core::filesystem::fs::perms;

  auto const result = lumex_filesystem::permissions (
      file, perms::owner_read | perms::group_read | perms::others_read);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_TRUE (native_is_read_only (file));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicDirectory_WhenSpace_ThenTheDiskIsReported)
{
  path const cyrillic = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (cyrillic));
  created (cyrillic, true);

  auto const info = lumex_filesystem::space (cyrillic);

  ASSERT_TRUE (info.success ()) << "error " << info.error_code ();
  EXPECT_GT (info.value ().capacity, 0U);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicName_WhenReadSymlink_ThenTheNameSurvives)
{
  path const target = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (target));
  created (target, false);
#if defined(_WIN32)
  // A symbolic link needs a privilege on Windows; the function opens the path
  // and returns the final name of what it opened, so a plain file shows the
  // conversion as well.
  path const link = target;
#else
  path const link = m_base / (std::string (kPrivetUtf8) + ".link");
  ASSERT_EQ (symlink (target.c_str (), link.c_str ()), 0);
  created (link, false);
#endif

  auto const result = lumex_filesystem::read_symlink (link);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_EQ (result.value ().filename ().string (),
             std::string (kPrivetUtf8) + ".txt");
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicFile_WhenCopyFile_ThenTheCopyHasItsNameAndContent)
{
  path const from = m_base / (std::string (kPrivetUtf8) + ".txt");
  path const to = m_base / (std::string (kPrivetUtf8) + kEmojiUtf8 + ".txt");
  ASSERT_TRUE (native_create_file (from, "abc"));
  created (from, false);
  created (to, false);

  auto const result = lumex_filesystem::copy_file (from, to);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_EQ (native_file_size (to), 3);
  EXPECT_EQ (native_file_size (from), 3);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicFile_WhenRenameAndMoveFile_ThenOnlyTheNewNameExists)
{
  path const first = m_base / (std::string (kPrivetUtf8) + "1.txt");
  path const second = m_base / (std::string (kPrivetUtf8) + "2.txt");
  path const third = m_base / (std::string (kPrivetUtf8) + "3.txt");
  ASSERT_TRUE (native_create_file (first));
  created (first, false);
  created (second, false);
  created (third, false);

  auto const renamed = lumex_filesystem::rename (first, second);
  ASSERT_TRUE (renamed.success ()) << "error " << renamed.error_code ();
  EXPECT_FALSE (native_exists (first));
  EXPECT_TRUE (native_exists (second));

  auto const moved = lumex_filesystem::move_file (second, third);
  ASSERT_TRUE (moved.success ()) << "error " << moved.error_code ();
  EXPECT_FALSE (native_exists (second));
  EXPECT_TRUE (native_exists (third));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicDirectory_WhenMoveDirectory_ThenOnlyTheNewNameExists)
{
  path const from = m_base / kPrivetUtf8;
  path const to = m_base / (std::string (kPrivetUtf8) + kEmojiUtf8);
  ASSERT_TRUE (native_create_directory (from));
  created (from, true);
  created (to, true);

  auto const result = lumex_filesystem::move_directory (from, to);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_FALSE (native_exists (from));
  EXPECT_TRUE (native_is_directory (to));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicFileAndDirectory_WhenRemove_ThenTheyAreGone)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  path const directory = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_file (file));
  ASSERT_TRUE (native_create_directory (directory));
  created (file, false);
  created (directory, true);

  auto const file_removed = lumex_filesystem::remove (file);
  auto const directory_removed = lumex_filesystem::remove (directory);

  ASSERT_TRUE (file_removed.success ())
      << "error " << file_removed.error_code ();
  EXPECT_TRUE (file_removed.value ());
  EXPECT_FALSE (native_exists (file));
  ASSERT_TRUE (directory_removed.success ())
      << "error " << directory_removed.error_code ();
  EXPECT_TRUE (directory_removed.value ());
  EXPECT_FALSE (native_exists (directory));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicTarget_WhenCreateSymlink_ThenTheLinkHasItsName)
{
  path const target = m_base / (std::string (kPrivetUtf8) + ".txt");
  path const link = m_base / (std::string (kPrivetUtf8) + ".link");
  path const directory = m_base / kPrivetUtf8;
  path const directory_link = m_base / (std::string (kPrivetUtf8) + ".dir");
  ASSERT_TRUE (native_create_file (target));
  ASSERT_TRUE (native_create_directory (directory));
  created (target, false);
  created (directory, true);

  auto const file_link = lumex_filesystem::create_symlink (target, link);
#if defined(_WIN32)
  // Without Developer Mode or the privilege, Windows refuses any link: the
  // conversion is then not reachable on this machine.
  if (!file_link.success ()
      && file_link.error_code () == ERROR_PRIVILEGE_NOT_HELD)
    GTEST_SKIP () << "symbolic links need SeCreateSymbolicLinkPrivilege";
#endif
  created (link, false);
  ASSERT_TRUE (file_link.success ()) << "error " << file_link.error_code ();
  EXPECT_TRUE (native_exists (link));

  auto const dir_link
      = lumex_filesystem::create_directory_symlink (directory, directory_link);
  // A directory link is removed as a directory on Windows, as a file on POSIX.
#if defined(_WIN32)
  created (directory_link, true);
#else
  created (directory_link, false);
#endif
  ASSERT_TRUE (dir_link.success ()) << "error " << dir_link.error_code ();
  EXPECT_TRUE (native_exists (directory_link));
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicAndEmojiNames_WhenDirectoryContents_ThenTheUtf8Names)
{
  path const directory = m_base / kPrivetUtf8;
  path const file = directory / (std::string (kEmojiUtf8) + ".txt");
  path const sub = directory / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (directory));
  ASSERT_TRUE (native_create_file (file));
  ASSERT_TRUE (native_create_directory (sub));
  created (directory, true);
  created (file, false);
  created (sub, true);

  std::vector<std::string> names;
  for (auto const &entry : lumex_filesystem::directory_contents (directory))
    names.push_back (entry.path ().filename ().string ());
  std::sort (names.begin (), names.end ());

  std::vector<std::string> expected;
  expected.push_back (std::string (kEmojiUtf8) + ".txt");
  expected.push_back (kPrivetUtf8);
  std::sort (expected.begin (), expected.end ());
  EXPECT_EQ (names, expected);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicTree_WhenRemoveAll_ThenEverythingIsGone)
{
  path const directory = m_base / kPrivetUtf8;
  path const sub = directory / kEmojiUtf8;
  path const file = sub / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_directory (directory));
  ASSERT_TRUE (native_create_directory (sub));
  ASSERT_TRUE (native_create_file (file));
  created (directory, true);
  created (sub, true);
  created (file, false);

  auto const result = lumex_filesystem::remove_all (directory);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_EQ (result.value (), 3U);
  EXPECT_FALSE (native_exists (directory));
}

namespace
{
// The absolute form of `p`, from the system itself.
std::string
native_absolute (path const &p)
{
#if defined(_WIN32)
  std::wstring const wide = p.wstring ();
  DWORD const size = GetFullPathNameW (wide.c_str (), 0, nullptr, nullptr);
  std::wstring full (size, L'\0');
  DWORD const length
      = GetFullPathNameW (wide.c_str (), size, &full[0], nullptr);
  full.resize (length);
  return lumex_filesystem::from_wide_string (full);
#else
  char *const resolved = realpath (p.c_str (), nullptr);
  std::string const full = resolved != nullptr ? resolved : "";
  std::free (resolved);
  return full;
#endif
}

// Changes the current directory of the process and puts the old one back.
class CurrentDirectoryGuard
{
public:
  CurrentDirectoryGuard ()
  {
#if defined(_WIN32)
    DWORD const size = GetCurrentDirectoryW (0, nullptr);
    m_saved.resize (size);
    m_saved.resize (GetCurrentDirectoryW (size, &m_saved[0]));
#else
    char *const current = getcwd (nullptr, 0);
    m_saved = current != nullptr ? current : "";
    std::free (current);
#endif
  }

  ~CurrentDirectoryGuard ()
  {
#if defined(_WIN32)
    SetCurrentDirectoryW (m_saved.c_str ());
#else
    if (chdir (m_saved.c_str ()) != 0)
      std::perror ("chdir");
#endif
  }

  CurrentDirectoryGuard (CurrentDirectoryGuard const &) = delete;
  CurrentDirectoryGuard &operator= (CurrentDirectoryGuard const &) = delete;

private:
#if defined(_WIN32)
  std::wstring m_saved;
#else
  std::string m_saved;
#endif
};

// Sets the variable GetTempPathW (Windows: TMP) or the POSIX branch (TMPDIR)
// reads first, and puts the old value back.
class TemporaryVariableGuard
{
public:
  explicit TemporaryVariableGuard (std::string const &value)
  {
#if defined(_WIN32)
    DWORD const size = GetEnvironmentVariableW (L"TMP", nullptr, 0);
    m_had = size != 0;
    if (m_had)
      {
        m_saved.resize (size);
        m_saved.resize (GetEnvironmentVariableW (L"TMP", &m_saved[0], size));
      }
    SetEnvironmentVariableW (
        L"TMP", lumex_filesystem::to_wide_string (value).c_str ());
#else
    char const *const old = std::getenv ("TMPDIR");
    m_had = old != nullptr;
    if (m_had)
      m_saved = old;
    setenv ("TMPDIR", value.c_str (), 1);
#endif
  }

  ~TemporaryVariableGuard ()
  {
#if defined(_WIN32)
    SetEnvironmentVariableW (L"TMP", m_had ? m_saved.c_str () : nullptr);
#else
    if (m_had)
      setenv ("TMPDIR", m_saved.c_str (), 1);
    else
      unsetenv ("TMPDIR");
#endif
  }

  TemporaryVariableGuard (TemporaryVariableGuard const &) = delete;
  TemporaryVariableGuard &operator= (TemporaryVariableGuard const &) = delete;

private:
  bool m_had;
#if defined(_WIN32)
  std::wstring m_saved;
#else
  std::string m_saved;
#endif
};
} // namespace

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicDirectory_WhenCurrentPathSetAndRead_ThenItsName)
{
  path const cyrillic = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (cyrillic));
  created (cyrillic, true);
  std::string const expected = native_absolute (cyrillic);
  ASSERT_FALSE (expected.empty ());

  {
    CurrentDirectoryGuard const restore;
    auto const set = lumex_filesystem::current_path (cyrillic);
    ASSERT_TRUE (set.success ()) << "error " << set.error_code ();
    auto const got = lumex_filesystem::current_path ();
    ASSERT_TRUE (got.success ()) << "error " << got.error_code ();
    EXPECT_EQ (got.value ().string (), expected);
  }
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicWorkingDirectory_WhenCanonical_ThenTheUtf8AbsoluteName)
{
  // The working directory matters: GetFullPathNameA reads the bytes of a
  // relative path in the ANSI code page and writes them back the same way
  // (an undefined byte maps to a C1 control and back), so the bytes of the
  // argument survive; the working directory it puts in front comes from the
  // system in that code page, not in UTF-8.
  path const cyrillic = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (cyrillic));
  created (cyrillic, true);
  // realpath (POSIX) resolves only a name that exists.
  ASSERT_TRUE (native_create_file (cyrillic / "x"));
  created (cyrillic / "x", false);
  std::string const expected = native_absolute (cyrillic) + "/x";

  path result;
  {
    CurrentDirectoryGuard const restore;
#if defined(_WIN32)
    ASSERT_NE (SetCurrentDirectoryW (cyrillic.wstring ().c_str ()), 0);
#else
    ASSERT_EQ (chdir (cyrillic.c_str ()), 0);
#endif
    result = lumex_filesystem::canonical (path ("x"));
  }

  // GetFullPathNameW writes backslashes; the expectation is built with "/".
  std::string text = result.string ();
  std::replace (text.begin (), text.end (), '\\', '/');
  std::string wanted = expected;
  std::replace (wanted.begin (), wanted.end (), '\\', '/');
  EXPECT_EQ (text, wanted);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicTemporaryDirectory_WhenTempDirectoryPath_ThenItsName)
{
  path const cyrillic = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (cyrillic));
  created (cyrillic, true);
  std::string const absolute = native_absolute (cyrillic);
  ASSERT_FALSE (absolute.empty ());
  TemporaryVariableGuard const variable (absolute);

  auto const result = lumex_filesystem::temp_directory_path ();

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  // GetTempPathW ends the directory with a separator; TMPDIR is returned as
  // it is.
  std::string text = result.value ().string ();
  if (!text.empty () && (text.back () == '\\' || text.back () == '/'))
    text.erase (text.size () - 1);
  EXPECT_EQ (text, absolute);
}

// The streams: std::ifstream / std::ofstream opened with the narrow name go
// through the ANSI code page on Windows, like the ...A functions.
TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenFileWithCyrillicName_WhenReadableAndWritable_ThenBoth)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".txt");
  ASSERT_TRUE (native_create_file (file, "abc"));
  created (file, false);

  EXPECT_TRUE (lumex_filesystem::is_readable (file));
  EXPECT_TRUE (lumex_filesystem::is_writable (file));
  EXPECT_TRUE (lumex::core::filesystem::fs::is_file_exists (file.string ()));
  // is_writable opens for reading and writing: the content stays.
  EXPECT_EQ (native_file_size (file), 3);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicName_WhenReplaceFileContent_ThenTheFileHasIt)
{
  path const file = m_base / (std::string (kPrivetUtf8) + kEmojiUtf8 + ".txt");
  created (file, false);

  auto const result = lumex_filesystem::replace_file_content (
      file, "12345", lumex::core::filesystem::fs::write_mode::binary);

  ASSERT_TRUE (result.success ()) << "error " << result.error_code ();
  EXPECT_EQ (native_file_size (file), 5);
}

// open_stream gives a stream the name of a UTF-8 path: the UTF-16 name on
// Windows, where a narrow name is read in the ANSI code page. The other
// modules that open files (settings, logging, temporary, resource_monitor)
// call it.
TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenCyrillicDirectory_WhenStreamsOpenedByPath_ThenTheyWriteAndRead)
{
  path const dir = m_base / kPrivetUtf8;
  ASSERT_TRUE (native_create_directory (dir));
  created (dir, true);
  path const file = dir / (std::string (kPrivetUtf8) + kEmojiUtf8 + ".txt");
  created (file, false);

  {
    std::ofstream out;
    auto const opened = lumex_filesystem::open_stream (out, file);
    ASSERT_TRUE (opened.success ()) << "error " << opened.error_code ();
    ASSERT_TRUE (out.is_open ());
    out << "abc";
  }
  // The system sees the file under the name asked for, not a garbled one.
  EXPECT_EQ (native_file_size (file), 3);

  std::ifstream in;
  auto const opened_in = lumex_filesystem::open_stream (in, file);
  ASSERT_TRUE (opened_in.success ()) << "error " << opened_in.error_code ();
  std::string text;
  in >> text;
  EXPECT_EQ (text, "abc");

  std::fstream both;
  auto const opened_both = lumex_filesystem::open_stream (
      both, file, std::ios_base::in | std::ios_base::out | std::ios_base::app);
  ASSERT_TRUE (opened_both.success ())
      << "error " << opened_both.error_code ();
  both << "def";
  both.close ();
  EXPECT_EQ (native_file_size (file), 6);
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenMissingFile_WhenStreamOpenedForReading_ThenErrorAndFailbit)
{
  std::ifstream in;
  auto const opened = lumex_filesystem::open_stream (
      in, m_base / (std::string (kPrivetUtf8) + "_missing.txt"));

  EXPECT_FALSE (opened.success ());
  EXPECT_EQ (opened.error_code (), ENOENT) << "the errno of the open";
  EXPECT_FALSE (in.is_open ());
  EXPECT_TRUE (in.fail ());
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenOpenStream_WhenOpenedAgain_ThenBusyAndFailbit)
{
  path const first = m_base / (std::string (kPrivetUtf8) + "_first.txt");
  path const second = m_base / (std::string (kPrivetUtf8) + "_second.txt");
  ASSERT_TRUE (native_create_file (first, "a"));
  ASSERT_TRUE (native_create_file (second, "b"));
  created (first, false);
  created (second, false);
  std::ifstream in;
  ASSERT_TRUE (lumex_filesystem::open_stream (in, first).success ());

  auto const again = lumex_filesystem::open_stream (in, second);

  EXPECT_FALSE (again.success ());
  EXPECT_EQ (again.error_code (), EBUSY);
  EXPECT_TRUE (in.fail ());
  EXPECT_TRUE (in.is_open ()) << "the stream stays on its first file";
}

TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenOpenModeBinary_WhenStreamOpenedByPath_ThenTheModeIsKept)
{
  path const file = m_base / (std::string (kPrivetUtf8) + ".bin");
  created (file, false);
  {
    std::ofstream out;
    ASSERT_TRUE (lumex_filesystem::open_stream (
                     out, file, std::ios_base::out | std::ios_base::binary)
                     .success ());
    out << "a\nb";
  }
  // In binary mode a newline is one byte on Windows too.
  EXPECT_EQ (native_file_size (file), 3);
}

#if defined(_WIN32)
// A path that is not valid UTF-8 is refused, as the other operations do: the
// name of the skipped bytes would be the name of another file.
TEST_F (LumexFilesystemUnicodeDiskTest,
        GivenInvalidUtf8_WhenStreamOpenedByPath_ThenInvalidNameAndNoFile)
{
  path const bad (m_base.string ()
                  + "/a\xFF"
                    "b.txt");
  path const other = m_base / "ab.txt";

  std::ofstream out;
  auto const opened = lumex_filesystem::open_stream (out, bad);

  EXPECT_FALSE (opened.success ());
  EXPECT_EQ (opened.error_code (), static_cast<int> (ERROR_INVALID_NAME));
  EXPECT_FALSE (out.is_open ());
  EXPECT_TRUE (out.fail ()) << "a refusal sets failbit like any failed open";
  EXPECT_FALSE (static_cast<bool> (out));
  EXPECT_FALSE (native_exists (other)) << "the skipped name was created";
}
#endif

#if defined(_WIN32)
// to_wide_path is the one place where an operation turns a path into a wide
// string: strict UTF-8 (Unicode table 3-7), the error ERROR_INVALID_NAME.
TEST (LumexFilesystemWidePathTest,
      GivenValidUtf8_WhenToWidePath_ThenTheSameAsToWideString)
{
  std::vector<std::string> const valid = {
    "",
    "dir/file.txt",
    kPrivetUtf8,
    mixed_utf8 (),
    "\xC2\x80",         // U+0080, the lowest two-byte code point
    "\xDF\xBF",         // U+07FF
    "\xE0\xA0\x80",     // U+0800, the lowest three-byte code point
    "\xED\x9F\xBF",     // U+D7FF, just below the surrogates
    "\xEE\x80\x80",     // U+E000, just above the surrogates
    "\xEF\xBF\xBF",     // U+FFFF
    "\xF0\x90\x80\x80", // U+10000, the lowest four-byte code point
    "\xF4\x8F\xBF\xBF", // U+10FFFF, the highest
  };
  for (std::string const &text : valid)
    {
      SCOPED_TRACE (text);
      auto const wide = lumex_filesystem::to_wide_path (path (text));
      ASSERT_TRUE (wide.success ()) << "error " << wide.error_code ();
      EXPECT_EQ (wide.value (), lumex_filesystem::to_wide_string (text));
    }
}

TEST (LumexFilesystemWidePathTest,
      GivenInvalidUtf8_WhenToWidePath_ThenInvalidNameAndNoString)
{
  std::vector<std::string> const invalid = {
    "\x80",             // a continuation byte alone
    "\xBF",             //
    "\xC0\x80",         // overlong U+0000
    "\xC1\xBF",         // overlong U+007F
    "\xE0\x80\x80",     // overlong U+0000
    "\xE0\x9F\xBF",     // overlong U+07FF
    "\xF0\x80\x80\x80", // overlong U+0000
    "\xF0\x8F\xBF\xBF", // overlong U+FFFF
    "\xED\xA0\x80",     // the surrogate U+D800
    "\xED\xBF\xBF",     // the surrogate U+DFFF
    "\xF4\x90\x80\x80", // U+110000, above the range
    "\xF5\x80\x80\x80", // a lead byte that never occurs
    "\xFE",             //
    "\xFF",             //
    "\xC3",             // cut short
    "\xE2\x82",         // cut short
    "\xF0\x9F\x98",     // cut short
    "a\xC3"
    "(", // a lead byte and an ASCII byte
    "\xE2\x82"
    "A", // a continuation byte missing
    "\xF0\x9F\x98"
    "A",            //
    "\xE2\x28\xA1", // a bad second byte
    "dir/"
    "a\xFF"
    "b.txt", // the byte in a name
  };
  for (std::string const &text : invalid)
    {
      SCOPED_TRACE (text);
      auto const wide = lumex_filesystem::to_wide_path (path (text));
      EXPECT_FALSE (wide.success ());
      EXPECT_EQ (wide.error_code (), static_cast<int> (ERROR_INVALID_NAME));
      EXPECT_TRUE (wide.value ().empty ());
    }
}
#endif

// --- Invalid UTF-8 in a path -----------------------------------------------
//
// The bug: to_wide_string (and path::wstring) skip a byte that is not UTF-8,
// so on Windows "a\xFF" "b" reached the file "ab": exists answered for it,
// remove, rename and the others acted on it. Every operation refuses such a
// path on Windows with ERROR_INVALID_NAME (a bool function answers false);
// POSIX takes the bytes as they are. "ab" and "xy" below are the files the
// skipped names would reach.
namespace
{
std::string
bad_ab ()
{
  return std::string ("a\xFF") + "b";
}

std::string
bad_xy ()
{
  return std::string ("x\xFF") + "y";
}

class LumexFilesystemInvalidUtf8Test : public LumexFilesystemUnicodeDiskTest
{
protected:
  void
  SetUp () override
  {
    LumexFilesystemUnicodeDiskTest::SetUp ();
    m_plain = m_base / "ab";
    m_bad = m_base / bad_ab ();
    m_other_plain = m_base / "xy";
    m_other_bad = m_base / bad_xy ();
    ASSERT_TRUE (native_create_file (m_plain, "x"));
  }

  void
  TearDown () override
  {
    // Whatever a failing case left under ASCII names, a directory included.
    lumex_filesystem::remove_all (m_base);
    LumexFilesystemUnicodeDiskTest::TearDown ();
  }

  // Whether "ab" is still what SetUp made, and "xy" is still not there.
  void
  expect_untouched () const
  {
    EXPECT_EQ (native_file_size (m_plain), 1);
    EXPECT_FALSE (native_exists (m_other_plain));
  }

  path m_plain;
  path m_bad;
  path m_other_plain;
  path m_other_bad;
};
} // namespace

#if defined(_WIN32)
namespace
{
int const kInvalidName = ERROR_INVALID_NAME;
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenQueried_ThenNothingIsFound)
{
  EXPECT_FALSE (lumex_filesystem::exists (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_directory (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_regular_file (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_empty (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_symlink (m_bad));
  EXPECT_TRUE (lumex_filesystem::exists (m_plain));
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenStatus_ThenTheErrorAndNoSuchFile)
{
  auto const status = lumex_filesystem::status (m_bad);
  auto const link_status = lumex_filesystem::symlink_status (m_bad);

  EXPECT_FALSE (status.success ());
  EXPECT_EQ (status.error_code (), kInvalidName);
  EXPECT_EQ (status.value ().type (),
             lumex::core::filesystem::fs::file_type::not_found);
  EXPECT_FALSE (link_status.success ());
  EXPECT_EQ (link_status.error_code (), kInvalidName);
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenFileAttributesAreRead_ThenTheError)
{
  auto const size = lumex_filesystem::file_size (m_bad);
  auto const time = lumex_filesystem::last_write_time (m_bad);
  auto const space = lumex_filesystem::space (m_bad);
  auto const target = lumex_filesystem::read_symlink (m_bad);

  EXPECT_EQ (size.error_code (), kInvalidName);
  EXPECT_EQ (time.error_code (), kInvalidName);
  EXPECT_EQ (space.error_code (), kInvalidName);
  EXPECT_EQ (target.error_code (), kInvalidName);
  EXPECT_TRUE (target.value ().empty ());
  // canonical has no way to report: it returns an empty path.
  EXPECT_TRUE (lumex_filesystem::canonical (m_bad).empty ());
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenAttributesAreChanged_ThenTheErrorAndNoChange)
{
  auto const resized = lumex_filesystem::resize_file (m_bad, 0);
  auto const timed = lumex_filesystem::last_write_time (m_bad, 0);
  auto const permitted = lumex_filesystem::permissions (
      m_bad, lumex::core::filesystem::fs::perms::owner_read);

  EXPECT_EQ (resized.error_code (), kInvalidName);
  EXPECT_EQ (timed.error_code (), kInvalidName);
  EXPECT_EQ (permitted.error_code (), kInvalidName);
  // "ab" kept its size, its time and its write permission.
  EXPECT_EQ (native_file_size (m_plain), 1);
  EXPECT_FALSE (native_is_read_only (m_plain));
  EXPECT_GT (lumex_filesystem::last_write_time (m_plain).value (), 1000);
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenCreated_ThenTheErrorAndNothingIsMade)
{
  auto const directory = lumex_filesystem::create_directory (m_other_bad);
  auto const nested
      = lumex_filesystem::create_directories (m_other_bad / "child");
  auto const same = lumex_filesystem::create_directories (m_other_bad);

  EXPECT_FALSE (directory.success ());
  EXPECT_EQ (directory.error_code (), kInvalidName);
  EXPECT_FALSE (nested.success ());
  EXPECT_EQ (nested.error_code (), kInvalidName);
  EXPECT_FALSE (same.success ());
  EXPECT_EQ (same.error_code (), kInvalidName);
  expect_untouched ();
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenRemoved_ThenTheErrorAndTheOtherFileStays)
{
  auto const removed = lumex_filesystem::remove (m_bad);
  auto const removed_all = lumex_filesystem::remove_all (m_bad);

  // Not "false, nothing there": the call refused.
  EXPECT_FALSE (removed.success ());
  EXPECT_EQ (removed.error_code (), kInvalidName);
  EXPECT_FALSE (removed_all.success ());
  EXPECT_EQ (removed_all.error_code (), kInvalidName);
  EXPECT_TRUE (native_exists (m_plain));
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenRenamedOrMoved_ThenTheErrorAndNothingMoves)
{
  path const target = m_base / "target";
  path const source_directory = m_base / "source_directory";
  ASSERT_TRUE (native_create_directory (source_directory));

  // The bad name as the source, and as the target.
  auto const from_bad = lumex_filesystem::rename (m_bad, target);
  auto const to_bad = lumex_filesystem::rename (m_plain, m_other_bad);
  auto const move_from_bad = lumex_filesystem::move_file (m_bad, target);
  auto const move_to_bad = lumex_filesystem::move_file (m_plain, m_other_bad);
  auto const directory_from_bad
      = lumex_filesystem::move_directory (m_bad, target);
  auto const directory_to_bad
      = lumex_filesystem::move_directory (source_directory, m_other_bad);

  EXPECT_EQ (from_bad.error_code (), kInvalidName);
  EXPECT_EQ (to_bad.error_code (), kInvalidName);
  EXPECT_EQ (move_from_bad.error_code (), kInvalidName);
  EXPECT_EQ (move_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (directory_from_bad.error_code (), kInvalidName);
  EXPECT_EQ (directory_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (native_file_size (m_plain), 1);
  EXPECT_FALSE (native_exists (target));
  EXPECT_FALSE (native_exists (m_other_plain));
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenCopied_ThenTheErrorAndNothingIsCopied)
{
  path const target = m_base / "target";
  path const source_directory = m_base / "source_directory";
  ASSERT_TRUE (native_create_directory (source_directory));

  auto const file_from_bad = lumex_filesystem::copy_file (m_bad, target);
  auto const file_to_bad = lumex_filesystem::copy_file (m_plain, m_other_bad);
  auto const copy_from_bad = lumex_filesystem::copy (m_bad, target);
  auto const copy_to_bad = lumex_filesystem::copy (m_plain, m_other_bad);
  // A directory as the source: its copy is made first.
  auto const directory_to_bad
      = lumex_filesystem::copy (source_directory, m_other_bad);

  EXPECT_EQ (file_from_bad.error_code (), kInvalidName);
  EXPECT_EQ (file_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (copy_from_bad.error_code (), kInvalidName);
  EXPECT_EQ (copy_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (directory_to_bad.error_code (), kInvalidName);
  EXPECT_FALSE (native_exists (target));
  EXPECT_FALSE (native_exists (m_other_plain));
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenSymlinkIsMade_ThenTheErrorAndNoLink)
{
  // The check comes before the system call, so no privilege is needed.
  auto const to_bad = lumex_filesystem::create_symlink (m_plain, m_other_bad);
  auto const from_bad
      = lumex_filesystem::create_symlink (m_bad, m_other_plain);
  auto const directory_to_bad
      = lumex_filesystem::create_directory_symlink (m_base, m_other_bad);
  auto const directory_from_bad
      = lumex_filesystem::create_directory_symlink (m_bad, m_other_plain);
  auto const copied_to_bad
      = lumex_filesystem::copy_symlink (m_plain, m_other_bad);
  auto const copied_from_bad
      = lumex_filesystem::copy_symlink (m_bad, m_other_plain);

  EXPECT_EQ (to_bad.error_code (), kInvalidName);
  EXPECT_EQ (from_bad.error_code (), kInvalidName);
  EXPECT_EQ (directory_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (directory_from_bad.error_code (), kInvalidName);
  EXPECT_EQ (copied_to_bad.error_code (), kInvalidName);
  EXPECT_EQ (copied_from_bad.error_code (), kInvalidName);
  expect_untouched ();
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenDirectoryWithInvalidUtf8Name_WhenIterated_ThenNoEntries)
{
  path const plain_directory = m_base / "de";
  ASSERT_TRUE (native_create_directory (plain_directory));
  ASSERT_TRUE (native_create_file (plain_directory / "inside.txt"));
  path const bad_directory = m_base / (std::string ("d\xFF") + "e");

  lumex::directory_iterator it (bad_directory);

  // The iterator of the skipped name would have listed "de".
  EXPECT_EQ (it, lumex::directory_iterator ());
  EXPECT_TRUE (lumex_filesystem::directory_contents (bad_directory).empty ());
  EXPECT_TRUE (
      lumex_filesystem::directory_paths (bad_directory).value ().empty ());
  EXPECT_NE (lumex::directory_iterator (plain_directory),
             lumex::directory_iterator ());
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenOpenedAsAStream_ThenNotOpened)
{
  ASSERT_TRUE (lumex_filesystem::is_readable (m_plain));

  EXPECT_FALSE (lumex_filesystem::is_readable (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_writable (m_bad));
  EXPECT_FALSE (lumex_filesystem::is_accessible (m_bad));
  // The stream is opened by the name alone: no exists () before it.
  EXPECT_TRUE (
      lumex::core::filesystem::fs::is_file_exists (m_plain.string ()));
  EXPECT_FALSE (lumex::core::filesystem::fs::is_file_exists (m_bad.string ()));
}

TEST_F (
    LumexFilesystemInvalidUtf8Test,
    GivenInvalidUtf8Path_WhenReplaceFileContent_ThenTheErrorAndNothingWritten)
{
  auto const result = lumex_filesystem::replace_file_content (
      m_bad, "12345", lumex::core::filesystem::fs::write_mode::binary);
  auto const other = lumex_filesystem::replace_file_content (
      m_other_bad, "12345", lumex::core::filesystem::fs::write_mode::binary);

  EXPECT_FALSE (result.success ());
  EXPECT_EQ (result.error_code (), kInvalidName);
  EXPECT_EQ (other.error_code (), kInvalidName);
  // "ab" was not replaced, and "xy" was not made.
  expect_untouched ();
}

TEST_F (
    LumexFilesystemInvalidUtf8Test,
    GivenInvalidUtf8Path_WhenCurrentPathIsSet_ThenTheErrorAndTheDirectoryStays)
{
  path const plain_directory = m_base / "de";
  ASSERT_TRUE (native_create_directory (plain_directory));
  path const bad_directory = m_base / (std::string ("d\xFF") + "e");
  CurrentDirectoryGuard const restore;
  std::string const before
      = lumex_filesystem::current_path ().value ().string ();

  auto const result = lumex_filesystem::current_path (bad_directory);

  EXPECT_FALSE (result.success ());
  EXPECT_EQ (result.error_code (), kInvalidName);
  EXPECT_EQ (lumex_filesystem::current_path ().value ().string (), before);
}

TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Path_WhenWstringOrToWideString_ThenTheByteIsSkipped)
{
  // The public conversions keep their contract: the byte is skipped. Only the
  // operations on a path refuse it.
  EXPECT_EQ (m_bad.wstring (), m_plain.wstring ());
  EXPECT_EQ (lumex_filesystem::to_wide_string (bad_ab ()),
             std::wstring (L"ab"));
}
#else
TEST_F (LumexFilesystemInvalidUtf8Test,
        GivenInvalidUtf8Name_OnPosix_WhenUsed_ThenTheBytesAreTakenAsTheyAre)
{
  ASSERT_TRUE (native_create_file (m_bad, "123"));
  created (m_bad, false);

  EXPECT_TRUE (lumex_filesystem::exists (m_bad));
  EXPECT_TRUE (lumex_filesystem::is_regular_file (m_bad));
  EXPECT_EQ (lumex_filesystem::file_size (m_bad).value (), 3U);
  EXPECT_TRUE (lumex_filesystem::is_readable (m_bad));
  // The file "ab" is another file.
  EXPECT_EQ (native_file_size (m_plain), 1);

  auto const renamed = lumex_filesystem::rename (m_bad, m_other_bad);
  created (m_other_bad, false);
  ASSERT_TRUE (renamed.success ()) << "error " << renamed.error_code ();
  EXPECT_FALSE (native_exists (m_bad));
  EXPECT_TRUE (native_exists (m_other_bad));

  auto const removed = lumex_filesystem::remove (m_other_bad);
  EXPECT_TRUE (removed.success ());
  EXPECT_TRUE (removed.value ());
  EXPECT_TRUE (native_exists (m_plain));
}
#endif

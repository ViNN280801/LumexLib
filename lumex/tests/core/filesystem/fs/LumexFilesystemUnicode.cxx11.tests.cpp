// lumex/tests/core/filesystem/fs/LumexFilesystemUnicode.cxx11.tests.cpp
//
// The conversions of a path between UTF-8 and wchar_t (to_wide_string,
// from_wide_string, path::wstring) are lumex::core::unicode::convert::to_wide
// and to_utf8: the same on every platform and in every C locale. They used to
// call mbstowcs and wcstombs on POSIX, which depend on the C locale, so a
// UTF-8 path gave an empty string in the "C" locale. Invalid input is skipped
// (WideCharToMultiByte and MultiByteToWideChar write U+FFFD instead).
//
// A wchar_t is 4 bytes wide (UTF-32) on Linux and 2 bytes (UTF-16) on
// Windows; the expectations are written with \u and \U escapes, which the
// compiler turns into the encoding of the platform, and the tests that depend
// on the width branch on sizeof (wchar_t).
#include <clocale>
#include <cstddef>
#include <cstring>
#include <ctime>
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
// under a garbled name (or not at all: U+1F600 has a byte that code page 1251
// does not define), and a file created by another program under its real name
// was not found. The checks below ask the system itself, through its wide
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
  // Removes what the case made, deepest first, through the system: remove_all
  // of the module still goes through the ...A functions on Windows.
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

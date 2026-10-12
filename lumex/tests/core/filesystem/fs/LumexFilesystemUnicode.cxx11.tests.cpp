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
#include <string>
#include <vector>

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

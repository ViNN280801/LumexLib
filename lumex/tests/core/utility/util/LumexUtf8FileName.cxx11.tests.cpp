// LumexUtf8FileName.cxx11.tests.cpp
//
// The strict UTF-8 check and conversions of LumexUtf8FileName.hpp, and the
// opening of a file stream by a UTF-8 name. On Windows a narrow name given to
// a stream is read in the ANSI code page, so a Cyrillic directory (a user
// profile) opened another file or none; the header gives the stream the UTF-16
// name and refuses a name that is not valid UTF-8 (the name of the skipped
// bytes would be the name of another file).
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/util/LumexUtf8FileName.hpp"
#include "lumex/tests/support/LumexTestUtf8Files.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace names = lumex::core::utility::utf8_file_name;
namespace files = lumex_test::utf8_files;

namespace
{
// A directory of the case, with a Cyrillic directory in it; removes both.
class LumexUtf8FileNameDiskTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    m_base
        = std::string ("lumex_utf8_file_name_")
          + ::testing::UnitTest::GetInstance ()->current_test_info ()->name ();
    files::make_directory (m_base); // may stay from an earlier run
    m_directory = m_base + "/" + files::cyrillic ();
    ASSERT_TRUE (files::make_directory (m_directory));
  }

  void
  TearDown () override
  {
    for (std::size_t i = 0; i < m_files.size (); ++i)
      files::remove_file (m_files[i]);
    files::remove_directory (m_directory);
    files::remove_directory (m_base);
  }

  std::string
  file (std::string const &name)
  {
    m_files.push_back (m_directory + "/" + name);
    return m_files.back ();
  }

  std::string m_base;
  std::string m_directory;
  std::vector<std::string> m_files;
};
} // namespace

TEST (LumexUtf8FileNameTest, GivenValidText_WhenIsValid_ThenTrue)
{
  char const *const valid[] = {
    "",
    "dir/file.txt",
    "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82",
    "\xC2\x80",         // U+0080, the lowest two-byte code point
    "\xDF\xBF",         // U+07FF
    "\xE0\xA0\x80",     // U+0800, the lowest three-byte code point
    "\xED\x9F\xBF",     // U+D7FF, just below the surrogates
    "\xEE\x80\x80",     // U+E000, just above the surrogates
    "\xEF\xBF\xBF",     // U+FFFF
    "\xF0\x90\x80\x80", // U+10000, the lowest four-byte code point
    "\xF4\x8F\xBF\xBF", // U+10FFFF, the highest
  };
  for (char const *text : valid)
    {
      SCOPED_TRACE (text);
      EXPECT_TRUE (names::is_valid (text));
    }
}

TEST (LumexUtf8FileNameTest, GivenInvalidText_WhenIsValid_ThenFalse)
{
  char const *const invalid[] = {
    "\x80",             // a continuation byte alone
    "\xBF",             //
    "\xC0\x80",         // overlong U+0000
    "\xC1\xBF",         // overlong U+007F
    "\xC2",             // truncated
    "\xE0\x9F\xBF",     // overlong U+07FF
    "\xE0\xA0",         // truncated
    "\xED\xA0\x80",     // U+D800, a surrogate
    "\xED\xBF\xBF",     // U+DFFF, a surrogate
    "\xF0\x8F\xBF\xBF", // overlong U+FFFF
    "\xF4\x90\x80\x80", // U+110000, above the range
    "\xF5\x80\x80\x80", // a lead byte that never starts a sequence
    "\xFF",             //
    "a\xFF"
    "b",        // a stray byte inside ASCII
    "\xC2\x41", // a continuation byte that is not one
  };
  for (char const *text : invalid)
    {
      SCOPED_TRACE (text);
      EXPECT_FALSE (names::is_valid (text));
    }
}

TEST (LumexUtf8FileNameTest, GivenText_WhenToWide_ThenTheCodePoints)
{
  std::wstring wide;
  ASSERT_TRUE (names::to_wide ("a\xD0\x9F\xD1\x80", wide));
  EXPECT_EQ (wide, std::wstring (L"aПр"));

  // U+1F600: one unit where wchar_t has 4 bytes, a surrogate pair where 2.
  ASSERT_TRUE (names::to_wide ("\xF0\x9F\x98\x80", wide));
  EXPECT_EQ (wide, std::wstring (L"\U0001F600"));
  EXPECT_EQ (wide.size (), sizeof (wchar_t) == 2 ? 2U : 1U);

  ASSERT_TRUE (names::to_wide ("", wide));
  EXPECT_TRUE (wide.empty ());
}

TEST (LumexUtf8FileNameTest, GivenInvalidText_WhenToWide_ThenFalseAndEmpty)
{
  std::wstring wide = L"left over";
  EXPECT_FALSE (names::to_wide ("a\xFF"
                                "b",
                                wide));
  EXPECT_TRUE (wide.empty ()) << "a skipped byte would name another file";
}

TEST (LumexUtf8FileNameTest, GivenWideText_WhenToUtf8_ThenRoundTrips)
{
  std::string const utf8 = "dir/\xD0\x9F\xD1\x80\xD0\xB8 \xF0\x9F\x98\x80.txt";
  std::wstring wide;
  ASSERT_TRUE (names::to_wide (utf8, wide));
  EXPECT_EQ (names::to_utf8 (wide), utf8);
  EXPECT_EQ (names::to_utf8 (L"Пр"), "\xD0\x9F\xD1\x80");
}

TEST (LumexUtf8FileNameTest, GivenUnpairedSurrogate_WhenToUtf8_ThenReplacement)
{
  if (sizeof (wchar_t) != 2)
    GTEST_SKIP () << "an unpaired surrogate exists in UTF-16 only";
  std::wstring wide (1, static_cast<wchar_t> (0xD800));
  wide += L"x";
  EXPECT_EQ (names::to_utf8 (wide), "\xEF\xBF\xBD"
                                    "x");
}

TEST_F (LumexUtf8FileNameDiskTest,
        GivenCyrillicDirectory_WhenStreamsOpened_ThenTheyWriteAndRead)
{
  std::string const name = file (files::cyrillic (".txt"));

  {
    std::ofstream out;
    ASSERT_TRUE (names::open (out, name, std::ios_base::out));
    out << "abc";
  }
  // The system sees the file under the name asked for, not a garbled one.
  EXPECT_EQ (files::file_size (name), 3);

  std::ifstream in;
  ASSERT_TRUE (names::open (in, name, std::ios_base::in));
  std::string text;
  in >> text;
  EXPECT_EQ (text, "abc");

  std::fstream both;
  ASSERT_TRUE (names::open (both, name,
                            std::ios_base::in | std::ios_base::out
                                | std::ios_base::app));
  both << "def";
  both.close ();
  EXPECT_EQ (files::file_size (name), 6);
}

TEST_F (LumexUtf8FileNameDiskTest,
        GivenMissingFile_WhenOpenedForReading_ThenFalseAndFailbit)
{
  std::ifstream in;
  EXPECT_FALSE (
      names::open (in, m_directory + "/missing.txt", std::ios_base::in));
  EXPECT_FALSE (in.is_open ());
  EXPECT_TRUE (in.fail ());
}

#if defined(_WIN32)
TEST_F (LumexUtf8FileNameDiskTest,
        GivenInvalidUtf8Name_WhenOpened_ThenRefusedAndNoOtherFile)
{
  // "a" "\xFF" "b" would be created as "ab" by a converter that skips bytes.
  std::string const other = file ("ab.txt");
  std::ofstream out;

  EXPECT_FALSE (names::open (out,
                             m_directory
                                 + "/a\xFF"
                                   "b.txt",
                             std::ios_base::out));
  EXPECT_FALSE (out.is_open ());
  EXPECT_TRUE (out.fail ());
  EXPECT_FALSE (files::exists (other));
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

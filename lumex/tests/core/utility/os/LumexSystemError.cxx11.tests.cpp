// LumexSystemError.cxx11.tests.cpp
// format_system_error() turns GetLastError () / errno into UTF-8 text that
// ends with the code; 0 is no error.
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#include "lumex/core/utility/os/LumexSystemError.hpp"

namespace
{
using lumex::core::utility::os::format_system_error;

// ERROR_FILE_NOT_FOUND on Windows, ENOENT on POSIX: both are 2.
int const kFileNotFound = 2;
// A code no system has a text for (Windows) or has only a generic one for
// (POSIX: "Unknown error ...").
int const kUnknownCode = 0x3ABCDEF1;

bool
ends_with (std::string const &text, std::string const &suffix)
{
  return text.size () >= suffix.size ()
         && text.compare (text.size () - suffix.size (), suffix.size (),
                          suffix)
                == 0;
}

// Strict UTF-8: no stray continuation bytes, no truncated sequences.
bool
is_valid_utf8 (std::string const &text)
{
  std::size_t i = 0;
  while (i < text.size ())
    {
      unsigned char const lead = static_cast<unsigned char> (text[i]);
      std::size_t length = 0;
      if (lead < 0x80)
        length = 1;
      else if ((lead & 0xE0) == 0xC0 && lead >= 0xC2)
        length = 2;
      else if ((lead & 0xF0) == 0xE0)
        length = 3;
      else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4)
        length = 4;
      else
        return false;
      if (i + length > text.size ())
        return false;
      for (std::size_t k = 1; k < length; ++k)
        if ((static_cast<unsigned char> (text[i + k]) & 0xC0) != 0x80)
          return false;
      i += length;
    }
  return true;
}
} // namespace

TEST (LumexSystemErrorTest, GivenZero_WhenFormat_ThenEmpty)
{
  EXPECT_TRUE (format_system_error (0).empty ());
}

TEST (LumexSystemErrorTest, GivenFileNotFound_WhenFormat_ThenTextAndCode)
{
  std::string const text = format_system_error (kFileNotFound);
  ASSERT_TRUE (ends_with (text, " (2)")) << text;
  // Some words come before the code.
  EXPECT_GT (text.size (), std::string (" (2)").size ()) << text;
}

TEST (LumexSystemErrorTest, GivenFileNotFound_WhenFormat_ThenNoLineBreak)
{
  // FormatMessageW ends its text with "\r\n"; it must not reach the result.
  std::string const text = format_system_error (kFileNotFound);
  EXPECT_EQ (text.find ('\r'), std::string::npos) << text;
  EXPECT_EQ (text.find ('\n'), std::string::npos) << text;
}

TEST (LumexSystemErrorTest, GivenFileNotFound_WhenFormat_ThenValidUtf8)
{
  // On a system in another language the text is not ASCII (a Russian
  // Windows answers in Cyrillic): it must still be UTF-8, not the ANSI code
  // page.
  EXPECT_TRUE (is_valid_utf8 (format_system_error (kFileNotFound)));
}

#if defined(LUMEX_OS_WINDOWS)
TEST (LumexSystemErrorTest, GivenCyrillicUtf16_WhenConverted_ThenUtf8Bytes)
{
  // "Отказ", as a Russian Windows begins "Access is denied": UTF-8, not the
  // ANSI code page (cp1251 would give 5 bytes, not 10).
  wchar_t const text[] = L"Отказ";
  EXPECT_EQ (lumex::core::utility::os::detail::utf16_to_utf8 (text, 5),
             "\xD0\x9E\xD1\x82\xD0\xBA\xD0\xB0\xD0\xB7");
}

TEST (LumexSystemErrorTest, GivenNoUnits_WhenConverted_ThenEmpty)
{
  wchar_t const text[] = L"x";
  EXPECT_TRUE (
      lumex::core::utility::os::detail::utf16_to_utf8 (text, 0).empty ());
  EXPECT_TRUE (
      lumex::core::utility::os::detail::utf16_to_utf8 (text, -1).empty ());
}
#endif

TEST (LumexSystemErrorTest, GivenUnknownCode_WhenFormat_ThenNamesTheCode)
{
  std::string const text = format_system_error (kUnknownCode);
  ASSERT_FALSE (text.empty ());
  EXPECT_NE (text.find (std::to_string (kUnknownCode)), std::string::npos)
      << text;
#if defined(LUMEX_OS_WINDOWS)
  EXPECT_EQ (text, "error " + std::to_string (kUnknownCode));
#endif
}

TEST (LumexSystemErrorTest, GivenNegativeCode_WhenFormat_ThenNamesTheCode)
{
  // Unfound: a negative value is not a code of either system.
  std::string const text = format_system_error (-1);
  ASSERT_FALSE (text.empty ());
  EXPECT_NE (text.find ("-1"), std::string::npos) << text;
}

TEST (LumexSystemErrorTest, GivenSameCode_WhenFormatTwice_ThenSameText)
{
  EXPECT_EQ (format_system_error (kFileNotFound),
             format_system_error (kFileNotFound));
}

TEST (LumexSystemErrorTest, GivenFormat_WhenCalled_ThenIsNoexcept)
{
  LUMEX_STATIC_ASSERT_MSG (noexcept (format_system_error (0)),
                           "format_system_error is noexcept");
  SUCCEED ();
}

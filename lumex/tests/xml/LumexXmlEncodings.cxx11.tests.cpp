// lumex/tests/xml/LumexXmlEncodings.cxx11.tests.cpp
//
// The encodings of XmlDocument::load_buffer, load and save: UTF-8, UTF-16 and
// UTF-32 in both byte orders, Latin-1, the byte order mark, the detection of
// the encoding from the first bytes and from the XML declaration, and output
// that is longer than the serializer's buffer. The bytes of the inputs and
// the expected bytes of the outputs are made by the small reference encoders
// below, not by the library's transcoders (lumex/core/unicode), which these
// tests exercise through the XML module.
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::constants::Constants::kformat_no_declaration;
using lumex::xml::constants::Constants::kformat_raw;
using lumex::xml::constants::Constants::kformat_write_bom;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::types::Types::xml_encoding;

namespace
{
typedef std::vector<unsigned char> byte_vector;

void
put_u16 (byte_vector &out, std::uint32_t unit, bool big_endian)
{
  if (big_endian)
    {
      out.push_back (static_cast<unsigned char> (unit >> 8));
      out.push_back (static_cast<unsigned char> (unit & 0xFF));
    }
  else
    {
      out.push_back (static_cast<unsigned char> (unit & 0xFF));
      out.push_back (static_cast<unsigned char> (unit >> 8));
    }
}

void
put_u32 (byte_vector &out, std::uint32_t unit, bool big_endian)
{
  for (int i = 0; i < 4; ++i)
    {
      int const shift = big_endian ? 24 - 8 * i : 8 * i;
      out.push_back (static_cast<unsigned char> ((unit >> shift) & 0xFF));
    }
}

// The bytes of `text` in `encoding`; Latin-1 takes code points up to 0xFF
// only. `bom` puts U+FEFF in front (UTF-8: EF BB BF).
byte_vector
encode (std::u32string const &text, xml_encoding encoding, bool bom)
{
  byte_vector out;
  std::u32string full = text;
  if (bom)
    full.insert (full.begin (), static_cast<char32_t> (0xFEFF));
  for (std::size_t i = 0; i < full.size (); ++i)
    {
      std::uint32_t const cp = static_cast<std::uint32_t> (full[i]);
      switch (encoding)
        {
        case xml_encoding::encoding_utf8:
          if (cp < 0x80)
            out.push_back (static_cast<unsigned char> (cp));
          else if (cp < 0x800)
            {
              out.push_back (static_cast<unsigned char> (0xC0 | (cp >> 6)));
              out.push_back (static_cast<unsigned char> (0x80 | (cp & 0x3F)));
            }
          else if (cp < 0x10000)
            {
              out.push_back (static_cast<unsigned char> (0xE0 | (cp >> 12)));
              out.push_back (
                  static_cast<unsigned char> (0x80 | ((cp >> 6) & 0x3F)));
              out.push_back (static_cast<unsigned char> (0x80 | (cp & 0x3F)));
            }
          else
            {
              out.push_back (static_cast<unsigned char> (0xF0 | (cp >> 18)));
              out.push_back (
                  static_cast<unsigned char> (0x80 | ((cp >> 12) & 0x3F)));
              out.push_back (
                  static_cast<unsigned char> (0x80 | ((cp >> 6) & 0x3F)));
              out.push_back (static_cast<unsigned char> (0x80 | (cp & 0x3F)));
            }
          break;
        case xml_encoding::encoding_utf16_le:
        case xml_encoding::encoding_utf16_be:
          {
            bool const big = encoding == xml_encoding::encoding_utf16_be;
            if (cp < 0x10000)
              put_u16 (out, cp, big);
            else
              {
                put_u16 (out, 0xD800 + ((cp - 0x10000) >> 10), big);
                put_u16 (out, 0xDC00 + ((cp - 0x10000) & 0x3FF), big);
              }
            break;
          }
        case xml_encoding::encoding_utf32_le:
          put_u32 (out, cp, false);
          break;
        case xml_encoding::encoding_utf32_be:
          put_u32 (out, cp, true);
          break;
        case xml_encoding::encoding_latin1:
          out.push_back (static_cast<unsigned char> (cp));
          break;
        default:
          ADD_FAILURE () << "encoding not supported by the test encoder";
          break;
        }
    }
  return out;
}

std::string
utf8_of (std::u32string const &text)
{
  byte_vector const bytes = encode (text, xml_encoding::encoding_utf8, false);
  return std::string (bytes.begin (), bytes.end ());
}

// Attribute and text hold a letter with an accent, the euro sign (three bytes
// in UTF-8) and a character outside the Basic Multilingual Plane (a
// surrogate pair in UTF-16).
std::u32string
rich_value ()
{
  return U"café € \U0001F600 end";
}

std::u32string
rich_document ()
{
  return U"<r a=\"" + rich_value () + U"\">" + rich_value () + U"</r>";
}

std::u32string
latin1_value ()
{
  return U"café naïve ÿ";
}

std::u32string
latin1_document ()
{
  return U"<r a=\"" + latin1_value () + U"\">" + latin1_value () + U"</r>";
}

struct unicode_encoding_t
{
  xml_encoding encoding;
  char const *name;
};

std::vector<unicode_encoding_t>
unicode_encodings ()
{
  std::vector<unicode_encoding_t> encodings;
  encodings.push_back ({ xml_encoding::encoding_utf8, "utf8" });
  encodings.push_back ({ xml_encoding::encoding_utf16_le, "utf16_le" });
  encodings.push_back ({ xml_encoding::encoding_utf16_be, "utf16_be" });
  encodings.push_back ({ xml_encoding::encoding_utf32_le, "utf32_le" });
  encodings.push_back ({ xml_encoding::encoding_utf32_be, "utf32_be" });
  return encodings;
}

class XmlEncodingTest : public XmlFixture
{
protected:
  lumex::xml::text::xml_parse_result_t
  load (byte_vector const &bytes, xml_encoding encoding)
  {
    return doc.load_buffer (bytes.empty () ? nullptr : &bytes[0],
                            bytes.size (), kparse_default, encoding);
  }

  void
  expect_rich ()
  {
    EXPECT_STREQ (doc.child ("r").attribute ("a").value (),
                  utf8_of (rich_value ()).c_str ());
    EXPECT_STREQ (doc.child ("r").text ().get (),
                  utf8_of (rich_value ()).c_str ());
  }

  std::string
  saved (xml_encoding encoding, unsigned int flags)
  {
    std::ostringstream out;
    doc.save (out, "", flags, encoding);
    return out.str ();
  }
};

std::string
as_string (byte_vector const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}
} // namespace

// ------------------------------------------------------------------ load

TEST_F (
    XmlEncodingTest,
    GivenEachUnicodeEncodingNamed_WhenLoadBuffer_ThenTextAndAttributeAreDecoded)
{
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = load (encode (rich_document (), encodings[i].encoding, false),
                  encodings[i].encoding);
      ASSERT_TRUE (result) << encodings[i].name << ": "
                           << result.description ();
      expect_rich ();
      EXPECT_EQ (result.encoding, encodings[i].encoding) << encodings[i].name;
    }
}

TEST_F (XmlEncodingTest,
        GivenLatin1Named_WhenLoadBuffer_ThenTheAccentedLettersBecomeUtf8)
{
  lumex::xml::text::xml_parse_result_t const result = load (
      encode (latin1_document (), xml_encoding::encoding_latin1, false),
      xml_encoding::encoding_latin1);
  ASSERT_TRUE (result) << result.description ();
  EXPECT_STREQ (doc.child ("r").text ().get (),
                utf8_of (latin1_value ()).c_str ());
  EXPECT_STREQ (doc.child ("r").attribute ("a").value (),
                utf8_of (latin1_value ()).c_str ());
}

TEST_F (XmlEncodingTest,
        GivenAByteOrderMark_WhenLoadBufferAuto_ThenTheEncodingIsDetected)
{
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = load (encode (rich_document (), encodings[i].encoding, true),
                  xml_encoding::encoding_auto);
      ASSERT_TRUE (result) << encodings[i].name << ": "
                           << result.description ();
      EXPECT_EQ (result.encoding, encodings[i].encoding) << encodings[i].name;
      expect_rich ();
    }
}

TEST_F (XmlEncodingTest,
        GivenNoByteOrderMark_WhenLoadBufferAuto_ThenTheFirstCharactersDecide)
{
  // '<' followed by the zero bytes of the wider encodings.
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = load (encode (rich_document (), encodings[i].encoding, false),
                  xml_encoding::encoding_auto);
      ASSERT_TRUE (result) << encodings[i].name << ": "
                           << result.description ();
      EXPECT_EQ (result.encoding, encodings[i].encoding) << encodings[i].name;
      expect_rich ();
    }
}

TEST_F (XmlEncodingTest,
        GivenADeclarationNamingLatin1_WhenLoadBufferAuto_ThenLatin1IsUsed)
{
  char const *const declarations[]
      = { "encoding=\"ISO-8859-1\"", "encoding='ISO-8859-1'",
          "encoding=\"iso-8859-1\"", "encoding = \"Iso-8859-1\"",
          "encoding=\"latin1\"",     "encoding=\"LATIN1\"",
          "encoding='Latin1'" };
  for (std::size_t i = 0; i < sizeof (declarations) / sizeof (declarations[0]);
       ++i)
    {
      doc.reset ();
      std::string const header
          = std::string ("<?xml version=\"1.0\" ") + declarations[i] + "?>";
      byte_vector bytes (header.begin (), header.end ());
      byte_vector const body
          = encode (U"<r>café</r>", xml_encoding::encoding_latin1, false);
      bytes.insert (bytes.end (), body.begin (), body.end ());
      lumex::xml::text::xml_parse_result_t const result
          = load (bytes, xml_encoding::encoding_auto);
      ASSERT_TRUE (result) << declarations[i] << ": " << result.description ();
      EXPECT_EQ (result.encoding, xml_encoding::encoding_latin1)
          << declarations[i];
      EXPECT_STREQ (doc.child ("r").text ().get (), "caf\xC3\xA9")
          << declarations[i];
    }
}

TEST_F (
    XmlEncodingTest,
    GivenADeclarationNamingAnotherEncoding_WhenLoadBufferAuto_ThenUtf8IsUsed)
{
  char const *const declarations[]
      = { "encoding=\"UTF-8\"", "encoding=\"windows-1252\"",
          "encoding=\"ISO-8859-15\"", "encoding=\"latin\"" };
  for (std::size_t i = 0; i < sizeof (declarations) / sizeof (declarations[0]);
       ++i)
    {
      doc.reset ();
      std::string const text = std::string ("<?xml version=\"1.0\" ")
                               + declarations[i] + "?><r>caf\xC3\xA9</r>";
      byte_vector const bytes (text.begin (), text.end ());
      lumex::xml::text::xml_parse_result_t const result
          = load (bytes, xml_encoding::encoding_auto);
      ASSERT_TRUE (result) << declarations[i] << ": " << result.description ();
      EXPECT_EQ (result.encoding, xml_encoding::encoding_utf8)
          << declarations[i];
      EXPECT_STREQ (doc.child ("r").text ().get (), "caf\xC3\xA9")
          << declarations[i];
    }
}

TEST_F (XmlEncodingTest,
        GivenABufferShorterThanFourBytes_WhenLoadBufferAuto_ThenUtf8IsAssumed)
{
  // Autodetection needs four bytes. The parse itself fails on these
  // fragments; the encoding of the result is what matters.
  byte_vector const three
      = encode (U"<a>", xml_encoding::encoding_utf8, false);
  EXPECT_EQ (load (three, xml_encoding::encoding_auto).encoding,
             xml_encoding::encoding_utf8);
  byte_vector const one (1, 'x');
  EXPECT_EQ (load (one, xml_encoding::encoding_auto).encoding,
             xml_encoding::encoding_utf8);
}

TEST_F (XmlEncodingTest,
        GivenAFourByteDocument_WhenLoadBufferAuto_ThenUtf8IsAssumed)
{
  byte_vector const bytes
      = encode (U"<r/>", xml_encoding::encoding_utf8, false);
  lumex::xml::text::xml_parse_result_t const result
      = load (bytes, xml_encoding::encoding_auto);
  ASSERT_TRUE (result) << result.description ();
  EXPECT_EQ (result.encoding, xml_encoding::encoding_utf8);
  EXPECT_TRUE (doc.child ("r"));
}

TEST_F (XmlEncodingTest,
        GivenNativeByteOrderRequested_WhenLoadBuffer_ThenTheMachineOrderIsUsed)
{
  // encoding_utf16 and encoding_utf32 mean the byte order of this machine.
  unsigned int const probe = 1;
  bool const little = *reinterpret_cast<unsigned char const *> (&probe) == 1;

  doc.reset ();
  lumex::xml::text::xml_parse_result_t result
      = load (encode (rich_document (),
                      little ? xml_encoding::encoding_utf16_le
                             : xml_encoding::encoding_utf16_be,
                      false),
              xml_encoding::encoding_utf16);
  ASSERT_TRUE (result) << result.description ();
  expect_rich ();

  doc.reset ();
  result = load (encode (rich_document (),
                         little ? xml_encoding::encoding_utf32_le
                                : xml_encoding::encoding_utf32_be,
                         false),
                 xml_encoding::encoding_utf32);
  ASSERT_TRUE (result) << result.description ();
  expect_rich ();
}

TEST_F (XmlEncodingTest,
        GivenWcharTextOfThePlatform_WhenLoadBufferAsWchar_ThenItIsDecoded)
{
  // encoding_wchar is whatever wchar_t is: UTF-16 on Windows, UTF-32 on Linux.
  std::wstring wide;
  std::u32string const text = rich_document ();
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      std::uint32_t const cp = static_cast<std::uint32_t> (text[i]);
      if (sizeof (wchar_t) == 2 && cp >= 0x10000)
        {
          wide.push_back (
              static_cast<wchar_t> (0xD800 + ((cp - 0x10000) >> 10)));
          wide.push_back (
              static_cast<wchar_t> (0xDC00 + ((cp - 0x10000) & 0x3FF)));
        }
      else
        wide.push_back (static_cast<wchar_t> (cp));
    }
  lumex::xml::text::xml_parse_result_t const result
      = doc.load_buffer (wide.c_str (), wide.size () * sizeof (wchar_t),
                         kparse_default, xml_encoding::encoding_wchar);
  ASSERT_TRUE (result) << result.description ();
  expect_rich ();
}

TEST_F (XmlEncodingTest,
        GivenAStreamOfUtf16_WhenLoad_ThenTheEncodingIsDetected)
{
  byte_vector const bytes
      = encode (rich_document (), xml_encoding::encoding_utf16_be, true);
  std::istringstream stream (as_string (bytes));
  lumex::xml::text::xml_parse_result_t const result
      = doc.load (stream, kparse_default, xml_encoding::encoding_auto);
  ASSERT_TRUE (result) << result.description ();
  EXPECT_EQ (result.encoding, xml_encoding::encoding_utf16_be);
  expect_rich ();
}

TEST_F (XmlEncodingTest,
        GivenMalformedUtf8_WhenLoadBufferAsUtf8_ThenTheBytesAreKeptAsTheyAre)
{
  // With the encoding named UTF-8 the buffer is not converted, so nothing
  // checks or drops the bad bytes.
  std::string const text = "<r>a\x80z\xE2\x82</r>";
  byte_vector const bytes (text.begin (), text.end ());
  lumex::xml::text::xml_parse_result_t const result
      = load (bytes, xml_encoding::encoding_utf8);
  ASSERT_TRUE (result) << result.description ();
  EXPECT_STREQ (doc.child ("r").text ().get (), "a\x80z\xE2\x82");
}

// ------------------------------------------------------------------ save

TEST_F (XmlEncodingTest,
        GivenEachUnicodeEncoding_WhenSave_ThenTheBytesAreTheReferenceEncoding)
{
  ASSERT_TRUE (
      load (encode (rich_document (), xml_encoding::encoding_utf8, false),
            xml_encoding::encoding_utf8));
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      std::string const out = saved (encodings[i].encoding,
                                     kformat_raw | kformat_no_declaration);
      EXPECT_EQ (out, as_string (encode (rich_document (),
                                         encodings[i].encoding, false)))
          << encodings[i].name;
    }
}

TEST_F (XmlEncodingTest,
        GivenTheBomFlag_WhenSave_ThenTheMarkOfTheEncodingComesFirst)
{
  ASSERT_TRUE (
      load (encode (rich_document (), xml_encoding::encoding_utf8, false),
            xml_encoding::encoding_utf8));
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      std::string const out
          = saved (encodings[i].encoding,
                   kformat_raw | kformat_no_declaration | kformat_write_bom);
      EXPECT_EQ (out, as_string (encode (rich_document (),
                                         encodings[i].encoding, true)))
          << encodings[i].name;
    }
}

TEST_F (XmlEncodingTest,
        GivenLatin1Text_WhenSaveAsLatin1_ThenEveryCharacterIsOneByte)
{
  ASSERT_TRUE (
      load (encode (latin1_document (), xml_encoding::encoding_utf8, false),
            xml_encoding::encoding_utf8));
  EXPECT_EQ (saved (xml_encoding::encoding_latin1,
                    kformat_raw | kformat_no_declaration),
             as_string (encode (latin1_document (),
                                xml_encoding::encoding_latin1, false)));
}

TEST_F (
    XmlEncodingTest,
    GivenCharactersOutsideLatin1_WhenSaveAsLatin1_ThenEachCodePointBecomesAQuestionMark)
{
  ASSERT_TRUE (load (
      encode (U"<r>a€\U0001F600āz</r>", xml_encoding::encoding_utf8, false),
      xml_encoding::encoding_utf8));
  EXPECT_EQ (saved (xml_encoding::encoding_latin1,
                    kformat_raw | kformat_no_declaration),
             "<r>a???z</r>");
}

TEST_F (XmlEncodingTest, GivenTheBomFlagAndLatin1_WhenSave_ThenNoMarkIsWritten)
{
  ASSERT_TRUE (
      load (encode (latin1_document (), xml_encoding::encoding_utf8, false),
            xml_encoding::encoding_utf8));
  std::string const out
      = saved (xml_encoding::encoding_latin1,
               kformat_raw | kformat_no_declaration | kformat_write_bom);
  EXPECT_EQ (out, as_string (encode (latin1_document (),
                                     xml_encoding::encoding_latin1, false)));
}

TEST_F (
    XmlEncodingTest,
    GivenNoDeclaration_WhenSave_ThenTheDeclarationNamesTheEncodingOfLatin1Only)
{
  ASSERT_TRUE (load (encode (U"<r/>", xml_encoding::encoding_utf8, false),
                     xml_encoding::encoding_utf8));
  EXPECT_EQ (saved (xml_encoding::encoding_utf8, kformat_raw),
             "<?xml version=\"1.0\"?><r/>");
  EXPECT_EQ (saved (xml_encoding::encoding_latin1, kformat_raw),
             "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><r/>");
  EXPECT_EQ (saved (xml_encoding::encoding_utf16_le, kformat_raw),
             as_string (encode (U"<?xml version=\"1.0\"?><r/>",
                                xml_encoding::encoding_utf16_le, false)));
}

TEST_F (XmlEncodingTest,
        GivenEachEncodingSavedAndLoaded_WhenCompared_ThenTheDocumentIsTheSame)
{
  ASSERT_TRUE (
      load (encode (rich_document (), xml_encoding::encoding_utf16_be, true),
            xml_encoding::encoding_auto));
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      std::string const out
          = saved (encodings[i].encoding, kformat_raw | kformat_write_bom);
      lumex::xml::document::XmlDocument again;
      lumex::xml::text::xml_parse_result_t const result
          = again.load_buffer (out.data (), out.size (), kparse_default,
                               xml_encoding::encoding_auto);
      ASSERT_TRUE (result) << encodings[i].name << ": "
                           << result.description ();
      EXPECT_EQ (result.encoding, encodings[i].encoding) << encodings[i].name;
      EXPECT_STREQ (again.child ("r").text ().get (),
                    utf8_of (rich_value ()).c_str ())
          << encodings[i].name;
    }
}

// Output longer than the serializer's buffer (about 2000 characters), so a
// multi-byte sequence or a surrogate pair can fall on a flush boundary. The
// leading ASCII characters shift where the boundary falls.
TEST_F (XmlEncodingTest,
        GivenALongText_WhenSavedAndLoadedInEveryEncoding_ThenNoSequenceIsSplit)
{
  std::u32string pieces;
  pieces += U'é';
  pieces += U'€';
  pieces += U'\U0001F600';
  pieces += U'中';
  pieces += U'x';
  for (std::size_t lead = 0; lead < 6; ++lead)
    {
      std::u32string text (lead, U'a');
      for (std::size_t i = 0; i < 5000; ++i)
        text += pieces[(i * 7 + i / 3) % pieces.size ()];
      std::u32string const document = U"<r>" + text + U"</r>";

      std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
      for (std::size_t k = 0; k < encodings.size (); ++k)
        {
          doc.reset ();
          ASSERT_TRUE (
              load (encode (document, xml_encoding::encoding_utf8, false),
                    xml_encoding::encoding_utf8));
          std::string const out = saved (encodings[k].encoding,
                                         kformat_raw | kformat_no_declaration);
          ASSERT_EQ (
              out, as_string (encode (document, encodings[k].encoding, false)))
              << encodings[k].name << " lead " << lead;

          lumex::xml::document::XmlDocument again;
          ASSERT_TRUE (again.load_buffer (
              out.data (), out.size (), kparse_default, encodings[k].encoding))
              << encodings[k].name << " lead " << lead;
          EXPECT_EQ (std::string (again.child ("r").text ().get ()),
                     utf8_of (text))
              << encodings[k].name << " lead " << lead;
        }
    }
}

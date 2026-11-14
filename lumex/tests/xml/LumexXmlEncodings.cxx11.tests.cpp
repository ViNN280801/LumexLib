// lumex/tests/xml/LumexXmlEncodings.cxx11.tests.cpp
//
// The encodings of XmlDocument::load_buffer, load and save: UTF-8, UTF-16 and
// UTF-32 in both byte orders, Latin-1, the byte order mark, the detection of
// the encoding from the first bytes and from the XML declaration, and output
// that is longer than the serializer's buffer. The bytes of the inputs and
// the expected bytes of the outputs are made by the small reference encoders
// below, not by the library's transcoders (lumex/core/unicode), which these
// tests exercise through the XML module.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <istream>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>
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
using byte_vector = std::vector<unsigned char>;

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
        GivenInvalidUtf32Units_WhenLoadBuffer_ThenTheyAreSkipped)
{
  // A surrogate code point and a value above U+10FFFF are no characters: the
  // decoder drops them, the rest of the text is parsed.
  std::u32string text (U"<a>x");
  text.push_back (static_cast<char32_t> (0xD800));
  text.push_back (U'y');
  text.push_back (static_cast<char32_t> (0x110000));
  text += U"</a>";
  xml_encoding const encodings[]
      = { xml_encoding::encoding_utf32_le, xml_encoding::encoding_utf32_be };
  for (std::size_t i = 0; i < 2; ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = load (encode (text, encodings[i], false), encodings[i]);
      ASSERT_TRUE (result) << result.description () << " " << i;
      EXPECT_STREQ (doc.child ("a").text ().get (), "xy") << i;
    }
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
        GivenMalformedUtf8_WhenLoadBufferAsUtf8_ThenTheBadBytesAreDropped)
{
  // A UTF-8 buffer is cleaned the way the other encodings are decoded: a stray
  // continuation byte and a truncated sequence are dropped (every kind of node
  // and every load path are covered by the XmlUtf8LoadTest suite).
  std::string const text = "<r>a\x80z\xE2\x82</r>";
  byte_vector const bytes (text.begin (), text.end ());
  lumex::xml::text::xml_parse_result_t const result
      = load (bytes, xml_encoding::encoding_utf8);
  ASSERT_TRUE (result) << result.description ();
  EXPECT_STREQ (doc.child ("r").text ().get (), "az");
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

// The setters of the tree take the bytes they are given, so a tree can hold
// an overlong form, a surrogate or a value above U+10FFFF that a load would
// have dropped. Writing it in another encoding goes through the UTF-8
// decoder, which skips them; it used to turn C0 BC into a raw '<' of the
// output.
TEST_F (
    XmlEncodingTest,
    GivenIllFormedUtf8SetThroughTheApi_WhenSavedInAnotherEncoding_ThenTheBytesAreSkipped)
{
  struct bad_t
  {
    char const *name;
    std::string bytes;
  };
  std::vector<bad_t> bad;
  bad.push_back ({ "overlong <", std::string ("\xC0\xBC") });
  bad.push_back ({ "overlong /", std::string ("\xC0\xAF") });
  bad.push_back ({ "overlong 3 bytes", std::string ("\xE0\x80\xBC") });
  bad.push_back ({ "overlong 4 bytes", std::string ("\xF0\x80\x80\xBC") });
  bad.push_back ({ "surrogate D800", std::string ("\xED\xA0\x80") });
  bad.push_back ({ "surrogate DFFF", std::string ("\xED\xBF\xBF") });
  bad.push_back ({ "above 10FFFF", std::string ("\xF4\x90\x80\x80") });
  bad.push_back ({ "lead F7", std::string ("\xF7\xBF\xBF\xBF") });

  std::vector<unicode_encoding_t> encodings = unicode_encodings ();
  encodings.push_back ({ xml_encoding::encoding_latin1, "latin1" });
  for (std::size_t i = 0; i < bad.size (); ++i)
    for (std::size_t k = 0; k < encodings.size (); ++k)
      {
        if (encodings[k].encoding == xml_encoding::encoding_utf8)
          continue;
        doc.reset ();
        lumex::xml::node::XmlNode root = doc.append_child ("r");
        ASSERT_TRUE (root.append_attribute ("a").set_value (
            ("x" + bad[i].bytes + "y").c_str ()))
            << bad[i].name;
        ASSERT_TRUE (root.text ().set (("p" + bad[i].bytes + "q").c_str ()))
            << bad[i].name;
        EXPECT_EQ (saved (encodings[k].encoding,
                          kformat_raw | kformat_no_declaration),
                   as_string (encode (U"<r a=\"xy\">pq</r>",
                                      encodings[k].encoding, false)))
            << bad[i].name << " as " << encodings[k].name;
      }
}

// --------------------------------------------- ill-formed UTF-8 on load
//
// A UTF-8 document is cleaned on load: a byte that starts no well-formed
// sequence (Unicode Table 3-7) is dropped, as the decoders drop it for the
// other encodings, so the tree holds well-formed UTF-8 only and the UTF-8,
// UTF-16 and UTF-32 saves write the same text (Latin-1 writes '?' for what it
// cannot hold). Valid UTF-8 is not touched. A document whose declaration names
// another encoding (windows-1251, KOI8-R) is not cleaned: it is loaded byte
// for byte. The expected text of every case is written out in the case, and
// the exhaustive tests compare with a reference written from the table, not
// with the decoder of the unicode module. The validator itself is tested with
// the unicode module (lumex/tests/core/unicode/utf).

namespace
{
unsigned char
byte_at (std::string const &s, std::size_t i)
{
  return static_cast<unsigned char> (s[i]);
}

bool
byte_in (unsigned char b, unsigned int low, unsigned int high)
{
  return b >= low && b <= high;
}

// Table 3-7: the length of the well-formed sequence that starts at s[i], or 0
// when none does.
std::size_t
table_sequence_length (std::string const &s, std::size_t i)
{
  std::size_t const left = s.size () - i;
  unsigned char const b0 = byte_at (s, i);
  if (b0 <= 0x7F)
    return 1;
  if (byte_in (b0, 0xC2, 0xDF))
    return (left >= 2 && byte_in (byte_at (s, i + 1), 0x80, 0xBF)) ? 2 : 0;
  if (byte_in (b0, 0xE0, 0xEF))
    {
      unsigned int low = 0x80;
      unsigned int high = 0xBF;
      if (b0 == 0xE0)
        low = 0xA0;
      if (b0 == 0xED)
        high = 0x9F;
      return (left >= 3 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF))
                 ? 3
                 : 0;
    }
  if (byte_in (b0, 0xF0, 0xF4))
    {
      unsigned int low = 0x80;
      unsigned int high = 0xBF;
      if (b0 == 0xF0)
        low = 0x90;
      if (b0 == 0xF4)
        high = 0x8F;
      return (left >= 4 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF)
              && byte_in (byte_at (s, i + 3), 0x80, 0xBF))
                 ? 4
                 : 0;
    }
  return 0;
}

// The text without the bytes that start no well-formed sequence.
std::string
table_clean (std::string const &s)
{
  std::string out;
  std::size_t i = 0;
  while (i < s.size ())
    {
      std::size_t const length = table_sequence_length (s, i);
      if (length == 0)
        ++i;
      else
        {
          out.append (s, i, length);
          i += length;
        }
    }
  return out;
}

// The offset of the first byte that starts no well-formed sequence.
std::size_t
table_valid_prefix (std::string const &s)
{
  std::size_t i = 0;
  while (i < s.size ())
    {
      std::size_t const length = table_sequence_length (s, i);
      if (length == 0)
        return i;
      i += length;
    }
  return s.size ();
}

std::string
hex_of (std::string const &s)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < s.size (); ++i)
    {
      out.push_back (digits[byte_at (s, i) >> 4]);
      out.push_back (digits[byte_at (s, i) & 15]);
      out.push_back (' ');
    }
  return out;
}

// The conversion of a UTF-8 buffer, copying and in place, against the
// reference written from the table.
void
expect_conversion_matches_reference (std::string const &text)
{
  using lumex::xml::types::Types::char_t;
  std::string const clean = table_clean (text);
  bool const changed = clean.size () != text.size ();

  // Three continuation bytes stand behind the text, outside the size that is
  // passed: a conversion that reads past the end completes a truncated
  // sequence with them, and one that writes past the end changes them.
  std::vector<char> padded (text.begin (), text.end ());
  padded.push_back ('\x80');
  padded.push_back ('\x80');
  padded.push_back ('\x80');

  // a copy: the cleaned text and the terminator
  char_t *copy = nullptr;
  std::size_t copy_length = 0;
  if (!lumex::xml::utility::convert_buffer (
          copy, copy_length, xml_encoding::encoding_utf8, &padded[0],
          text.size (), false, true))
    {
      ADD_FAILURE () << "no copy of " << hex_of (text);
      return;
    }
  EXPECT_EQ (std::string (copy, copy_length), clean + std::string (1, '\0'))
      << hex_of (text);
  std::free (copy);

  // not cleaned: the bytes come back as they are
  char_t *raw = nullptr;
  std::size_t raw_length = 0;
  if (lumex::xml::utility::convert_buffer (
          raw, raw_length, xml_encoding::encoding_utf8, &padded[0],
          text.size (), false, false))
    {
      EXPECT_EQ (std::string (raw, raw_length), text + std::string (1, '\0'))
          << hex_of (text);
      std::free (raw);
    }

  // in place: the buffer of the caller holds the cleaned text, nothing is
  // written behind it, and when bytes were dropped the freed byte after the
  // text is the terminator (the byte the parser takes for the end)
  char_t *own = nullptr;
  std::size_t own_length = 0;
  if (!lumex::xml::utility::convert_buffer (
          own, own_length, xml_encoding::encoding_utf8, &padded[0],
          text.size (), true, true))
    {
      ADD_FAILURE () << "no in-place conversion of " << hex_of (text);
      return;
    }
  EXPECT_EQ (own, &padded[0]) << hex_of (text);
  // a text that already ends with a NUL gets no second one
  bool const ends_with_nul
      = !clean.empty () && clean[clean.size () - 1] == '\0';
  EXPECT_EQ (std::string (own, own_length), (changed && !ends_with_nul)
                                                ? clean + std::string (1, '\0')
                                                : clean)
      << hex_of (text);
  EXPECT_LE (own_length, text.size ()) << hex_of (text);
  EXPECT_EQ (std::string (padded.end () - 3, padded.end ()),
             std::string ("\x80\x80\x80"))
      << hex_of (text);
}

struct bad_utf8_t
{
  char const *name;
  std::string bytes;   // what the document holds
  std::u32string kept; // what the tree must hold
};

std::vector<bad_utf8_t>
bad_utf8_cases ()
{
  std::vector<bad_utf8_t> bad;
  bad.push_back ({ "overlong C0 BC", std::string ("\xC0\xBC"), U"" });
  bad.push_back ({ "overlong C0 AF", std::string ("\xC0\xAF"), U"" });
  bad.push_back ({ "overlong C1 BF", std::string ("\xC1\xBF"), U"" });
  bad.push_back ({ "overlong E0 80 BC", std::string ("\xE0\x80\xBC"), U"" });
  bad.push_back ({ "overlong E0 9F BF", std::string ("\xE0\x9F\xBF"), U"" });
  bad.push_back (
      { "overlong F0 80 80 BC", std::string ("\xF0\x80\x80\xBC"), U"" });
  bad.push_back (
      { "overlong F0 8F BF BF", std::string ("\xF0\x8F\xBF\xBF"), U"" });
  bad.push_back ({ "surrogate ED A0 80", std::string ("\xED\xA0\x80"), U"" });
  bad.push_back ({ "surrogate ED BF BF", std::string ("\xED\xBF\xBF"), U"" });
  bad.push_back (
      { "CESU-8 pair", std::string ("\xED\xA0\xBD\xED\xB8\x80"), U"" });
  bad.push_back (
      { "above F4 90 80 80", std::string ("\xF4\x90\x80\x80"), U"" });
  bad.push_back ({ "lead F5", std::string ("\xF5\x80\x80\x80"), U"" });
  bad.push_back ({ "lead F7", std::string ("\xF7\xBF\xBF\xBF"), U"" });
  bad.push_back ({ "lead F8", std::string ("\xF8\x88\x80\x80\x80"), U"" });
  bad.push_back ({ "lead FC", std::string ("\xFC\x84\x80\x80\x80\x80"), U"" });
  bad.push_back ({ "byte FE", std::string ("\xFE"), U"" });
  bad.push_back ({ "byte FF", std::string ("\xFF"), U"" });
  bad.push_back ({ "continuation 80", std::string ("\x80"), U"" });
  bad.push_back ({ "continuation BF", std::string ("\xBF"), U"" });
  bad.push_back ({ "continuations", std::string ("\x80\x80\x80"), U"" });
  bad.push_back ({ "truncated 2 bytes", std::string ("\xC3"), U"" });
  bad.push_back ({ "truncated 3 bytes", std::string ("\xE2\x82"), U"" });
  bad.push_back ({ "truncated 4 bytes", std::string ("\xF0\x9F\x98"), U"" });
  bad.push_back ({ "truncated then valid",
                   std::string ("\xE2\x82") + std::string ("\xC3\xA9"),
                   U"é" });
  bad.push_back ({ "valid around invalid",
                   std::string ("\xC3\xA9\xC0\xBC\xE2\x82\xAC"), U"é€" });
  bad.push_back ({ "overlong then continuations",
                   std::string ("\xC0\xBC\x80\x80"), U"" });
  return bad;
}

// The document of the cases: the bytes go to an attribute value, text, a
// comment, a processing instruction and a CDATA section.
template <typename S>
S
framed (S const &inner, S const &x, S const &y, S const &a, S const &b,
        S const &c, S const &d, S const &e, S const &f)
{
  return a + x + inner + y + b + x + inner + y + c + x + inner + y + d + x
         + inner + y + e + x + inner + y + f;
}

std::string
framed_bytes (std::string const &inner)
{
  return framed<std::string> (inner, "x", "y", "<r a=\"", "\">", "<!--",
                              "--><?pi ", "?><![CDATA[", "]]></r>");
}

std::u32string
framed_text (std::u32string const &inner)
{
  return framed<std::u32string> (inner, U"x", U"y", U"<r a=\"", U"\">",
                                 U"<!--", U"--><?pi ", U"?><![CDATA[",
                                 U"]]></r>");
}

unsigned int const kFullOptions
    = lumex::xml::constants::Constants::kparse_default
      | lumex::xml::constants::Constants::kparse_comments
      | lumex::xml::constants::Constants::kparse_pi;

enum class load_path_t
{
  buffer,
  inplace,
  inplace_own,
  string,
  file,
  stream,
  stream_without_seek,
  auto_detected,
  append_buffer
};

std::vector<load_path_t>
load_paths ()
{
  std::vector<load_path_t> paths;
  paths.push_back (load_path_t::buffer);
  paths.push_back (load_path_t::inplace);
  paths.push_back (load_path_t::inplace_own);
  paths.push_back (load_path_t::string);
  paths.push_back (load_path_t::file);
  paths.push_back (load_path_t::stream);
  paths.push_back (load_path_t::stream_without_seek);
  paths.push_back (load_path_t::auto_detected);
  paths.push_back (load_path_t::append_buffer);
  return paths;
}

char const *
load_path_name (load_path_t path)
{
  switch (path)
    {
    case load_path_t::buffer:
      return "load_buffer";
    case load_path_t::inplace:
      return "load_buffer_inplace";
    case load_path_t::inplace_own:
      return "load_buffer_inplace_own";
    case load_path_t::string:
      return "load_string";
    case load_path_t::file:
      return "load_file";
    case load_path_t::stream:
      return "load (istream)";
    case load_path_t::stream_without_seek:
      return "load (istream without seek)";
    case load_path_t::append_buffer:
      return "XmlNode::append_buffer";
    case load_path_t::auto_detected:
      return "load_buffer (auto)";
    }
  return "?";
}

// An input stream that cannot seek, so that the document loads it in chunks.
class chunked_streambuf : public std::streambuf
{
public:
  explicit chunked_streambuf (std::string text) : m_text (std::move (text))
  {
    if (!m_text.empty ())
      setg (&m_text[0], &m_text[0], &m_text[0] + m_text.size ());
  }

private:
  std::string m_text;
};

class XmlUtf8LoadTest : public XmlEncodingTest
{
protected:
  // Loads `bytes` as a UTF-8 document by `path`. For the in-place path it
  // checks that guard bytes around the caller's buffer are not touched.
  lumex::xml::text::xml_parse_result_t
  load_by (load_path_t path, std::string const &bytes,
           unsigned int options = kFullOptions)
  {
    doc.reset ();
    switch (path)
      {
      case load_path_t::buffer:
        return doc.load_buffer (bytes.data (), bytes.size (), options,
                                xml_encoding::encoding_utf8);
      case load_path_t::auto_detected:
        return doc.load_buffer (bytes.data (), bytes.size (), options,
                                xml_encoding::encoding_auto);
      case load_path_t::append_buffer:
        return doc.append_buffer (bytes.data (), bytes.size (), options,
                                  xml_encoding::encoding_utf8);
      case load_path_t::inplace:
        {
          std::size_t const guard = 16;
          m_storage.assign (guard + bytes.size () + guard, '\0');
          std::fill (m_storage.begin (), m_storage.begin () + guard, '\xA5');
          std::fill (m_storage.end () - guard, m_storage.end (), '\x5A');
          std::copy (bytes.begin (), bytes.end (), m_storage.begin () + guard);
          lumex::xml::text::xml_parse_result_t const result
              = doc.load_buffer_inplace (&m_storage[guard], bytes.size (),
                                         options, xml_encoding::encoding_utf8);
          m_data = &m_storage[guard];
          m_size = bytes.size ();
          for (std::size_t i = 0; i < guard; ++i)
            {
              EXPECT_EQ (m_storage[i], '\xA5') << "guard before, byte " << i;
              EXPECT_EQ (m_storage[m_storage.size () - 1 - i], '\x5A')
                  << "guard behind, byte " << i;
            }
          return result;
        }
      case load_path_t::inplace_own:
        {
          // exactly the bytes of the document: the cleaning of the buffer
          // must not write behind it
          void *own = std::malloc (bytes.empty () ? 1 : bytes.size ());
          EXPECT_NE (own, static_cast<void *> (nullptr));
          if (!bytes.empty ())
            std::memcpy (own, bytes.data (), bytes.size ());
          m_data = static_cast<char *> (own);
          m_size = bytes.size ();
          return doc.load_buffer_inplace_own (own, bytes.size (), options,
                                              xml_encoding::encoding_utf8);
        }
      case load_path_t::string:
        return doc.load_string (bytes.c_str (), options);
      case load_path_t::file:
        {
          // the suites of the standards run side by side in one directory
          std::string const name
              = std::string ("lumex_xml_utf8_load_")
                + std::to_string (static_cast<long> (__cplusplus)) + "_"
                + ::testing::UnitTest::GetInstance ()
                      ->current_test_info ()
                      ->name ()
                + ".xml";
          {
            std::ofstream out (name.c_str (), std::ios::binary);
            out.write (bytes.data (),
                       static_cast<std::streamsize> (bytes.size ()));
          }
          lumex::xml::text::xml_parse_result_t const result = doc.load_file (
              name.c_str (), options, xml_encoding::encoding_utf8);
          std::remove (name.c_str ());
          return result;
        }
      case load_path_t::stream:
        {
          std::istringstream in (bytes);
          return doc.load (in, options, xml_encoding::encoding_utf8);
        }
      case load_path_t::stream_without_seek:
        {
          chunked_streambuf buffer (bytes);
          std::istream in (&buffer);
          return doc.load (in, options, xml_encoding::encoding_utf8);
        }
      }
    return lumex::xml::text::xml_parse_result_t ();
  }

  std::vector<char> m_storage;
  char *m_data = nullptr;
  std::size_t m_size = 0;
};
} // namespace

TEST_F (
    XmlUtf8LoadTest,
    GivenIllFormedUtf8InEveryKindOfNode_WhenLoadedByAnyPath_ThenTheTreeHoldsTheCleanedText)
{
  std::vector<bad_utf8_t> const bad = bad_utf8_cases ();
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    for (std::size_t i = 0; i < bad.size (); ++i)
      {
        std::string const label
            = std::string (load_path_name (paths[p])) + ", " + bad[i].name;
        std::string const expected_text = utf8_of (bad[i].kept);
        lumex::xml::text::xml_parse_result_t const result
            = load_by (paths[p], framed_bytes (bad[i].bytes));
        ASSERT_TRUE (result) << label << ": " << result.description ();

        EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                   "x" + expected_text + "y")
            << label;
        EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
                   "x" + expected_text + "y")
            << label;
        // the text of the tree is the same text in UTF-8, UTF-16 and UTF-32
        std::u32string const expected_document = framed_text (bad[i].kept);
        EXPECT_EQ (saved (xml_encoding::encoding_utf8,
                          kformat_raw | kformat_no_declaration),
                   utf8_of (expected_document))
            << label;
        std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
        for (std::size_t k = 0; k < encodings.size (); ++k)
          EXPECT_EQ (saved (encodings[k].encoding,
                            kformat_raw | kformat_no_declaration),
                     as_string (encode (expected_document,
                                        encodings[k].encoding, false)))
              << label << " saved as " << encodings[k].name;
      }
}

TEST_F (
    XmlUtf8LoadTest,
    GivenIllFormedUtf8_WhenLoadedAndSavedAsUtf16_ThenTheUtf16TextConvertedBackIsTheUtf8Save)
{
  // The probe of the review of the UTF-8 decoder: C0 BC in a UTF-8 document
  // stayed in the tree, a UTF-8 save wrote it raw, a UTF-16 save dropped it.
  std::string const text = "<r>p\xC0\xBCq</r>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (), kparse_default,
                                xml_encoding::encoding_utf8));
  EXPECT_EQ (saved (xml_encoding::encoding_utf8,
                    kformat_raw | kformat_no_declaration),
             "<r>pq</r>");
  EXPECT_EQ (saved (xml_encoding::encoding_utf16_le,
                    kformat_raw | kformat_no_declaration),
             as_string (encode (U"<r>pq</r>", xml_encoding::encoding_utf16_le,
                                false)));
  EXPECT_STREQ (doc.child ("r").text ().get (), "pq");
}

TEST_F (XmlUtf8LoadTest,
        GivenValidUtf8_WhenLoadedByAnyPath_ThenTheSavedBytesAreTheInput)
{
  // Every length of sequence, the edges of the ranges Table 3-7 narrows, a
  // real U+FFFD, a NUL-free text with a combining mark.
  std::u32string text (U"\u0080 ߿ ࠀ ퟿  � ");
  text.push_back (static_cast<char32_t> (0x10000));
  text.push_back (U' ');
  text.push_back (static_cast<char32_t> (0x10FFFF));
  text += U" é́ €";
  std::string const document = framed_bytes (utf8_of (text));
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      ASSERT_TRUE (load_by (paths[p], document)) << load_path_name (paths[p]);
      EXPECT_EQ (saved (xml_encoding::encoding_utf8,
                        kformat_raw | kformat_no_declaration),
                 document)
          << load_path_name (paths[p]);
      EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
                 "x" + utf8_of (text) + "y")
          << load_path_name (paths[p]);
    }
}

TEST_F (XmlUtf8LoadTest,
        GivenUtf8WithAByteOrderMark_WhenLoaded_ThenTheBomIsNotPartOfTheText)
{
  std::string const document = "\xEF\xBB\xBF<r>p\xC0\xBC\xC3\xA9q</r>";
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      ASSERT_TRUE (load_by (paths[p], document, kparse_default))
          << load_path_name (paths[p]);
      EXPECT_STREQ (doc.child ("r").text ().get (), "p\xC3\xA9q")
          << load_path_name (paths[p]);
    }
}

TEST_F (
    XmlUtf8LoadTest,
    GivenIllFormedUtf8_WhenLoadedInPlace_ThenTheBufferOfTheCallerHoldsTheTree)
{
  std::vector<bad_utf8_t> const bad = bad_utf8_cases ();
  for (std::size_t i = 0; i < bad.size (); ++i)
    {
      ASSERT_TRUE (load_by (load_path_t::inplace, framed_bytes (bad[i].bytes)))
          << bad[i].name;
      // the strings of the tree are in the caller's buffer, not in a copy
      char const *const value = doc.child ("r").attribute ("a").value ();
      EXPECT_TRUE (value >= m_data && value < m_data + m_size) << bad[i].name;
      char const *const text = doc.child ("r").child_value ();
      EXPECT_TRUE (text >= m_data && text < m_data + m_size) << bad[i].name;
      EXPECT_EQ (std::string (value), "x" + utf8_of (bad[i].kept) + "y")
          << bad[i].name;
    }
}

TEST_F (XmlUtf8LoadTest,
        GivenBytesAfterTheRoot_WhenLoaded_ThenTheyAreNotInTheTree)
{
  // After the root only comments, processing instructions and white space may
  // stand; the bytes go into a comment and an instruction there, where the
  // tree shows whether they were dropped.
  std::string const document = std::string ("<r>t</r><!--c\xC0\xBC")
                               + "d\xE2\x82" + "--><?pi e\xFF" + "f?>\xC0\xBC";
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      ASSERT_TRUE (load_by (paths[p], document)) << load_path_name (paths[p]);
      EXPECT_STREQ (doc.child ("r").text ().get (), "t")
          << load_path_name (paths[p]);
      std::string comment;
      std::string instruction;
      for (lumex::xml::node::XmlNode n = doc.first_child (); n;
           n = n.next_sibling ())
        {
          if (n.type () == lumex::xml::types::Types::node_comment)
            comment = n.value ();
          if (n.type () == lumex::xml::types::Types::node_pi)
            instruction = n.value ();
        }
      EXPECT_EQ (comment, "cd") << load_path_name (paths[p]);
      EXPECT_EQ (instruction, "ef") << load_path_name (paths[p]);
    }
}

TEST_F (XmlUtf8LoadTest,
        GivenADocumentOfIllFormedBytesOnly_WhenLoaded_ThenItIsEmpty)
{
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      lumex::xml::text::xml_parse_result_t const result
          = load_by (paths[p], "\xC0\xBC\xFF\x80", kparse_default);
      EXPECT_FALSE (result) << load_path_name (paths[p]);
      EXPECT_EQ (result.status, lumex::xml::types::Types::xml_parse_status::
                                    status_no_document_element)
          << load_path_name (paths[p]);
      EXPECT_FALSE (doc.first_child ()) << load_path_name (paths[p]);

      lumex::xml::text::xml_parse_result_t const fragment = load_by (
          paths[p], "\xC0\xBC\xFF\x80",
          kparse_default | lumex::xml::constants::Constants::kparse_fragment);
      EXPECT_TRUE (fragment) << load_path_name (paths[p]);
      EXPECT_FALSE (doc.first_child ()) << load_path_name (paths[p]);
    }
}

TEST_F (XmlUtf8LoadTest,
        GivenIllFormedBytesInANameOrTheMarkup_WhenLoaded_ThenTheMarkupIsRead)
{
  // The bytes are dropped before the parser sees the document: a name that
  // held one is the name without it, and the document is no longer
  // ill-formed in the parser's eyes.
  std::string const text = std::string ("<a\xC0\xBC") + "b c\xFF" + "=\"v\"/>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (), kparse_default,
                                xml_encoding::encoding_utf8));
  EXPECT_TRUE (doc.child ("ab"));
  EXPECT_STREQ (doc.child ("ab").attribute ("c").value (), "v");
}

TEST_F (
    XmlUtf8LoadTest,
    GivenIllFormedUtf8_WhenSavedWithTheByteOrderMark_ThenTheFileIsStrictUtf8)
{
  std::string const text = "<r>\xED\xA0\x80\xF4\x90\x80\x80\xC3\xA9</r>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (), kparse_default,
                                xml_encoding::encoding_utf8));
  std::string const out
      = saved (xml_encoding::encoding_utf8,
               kformat_raw | kformat_no_declaration | kformat_write_bom);
  EXPECT_EQ (out, "\xEF\xBB\xBF<r>\xC3\xA9</r>");
  EXPECT_EQ (table_valid_prefix (out), out.size ());
}

TEST_F (
    XmlEncodingTest,
    GivenEveryEncodingOfADocumentWithReferences_WhenLoaded_ThenOnlyTheCharsExpand)
{
  // The same document in UTF-8, UTF-16 and UTF-32: a reference is read after
  // the conversion, so the encoding of the document changes nothing.
  std::u32string const document
      = U"<r a=\"&#xD800;&#x41;&#x110000;&#0;&#xFFFF;\">p&#xD800;q&#xDFFF;r"
        U"&#x110000;s&#x7FFFFFFF;t&#0;u&#x1;v&#xFFFE;w&#x10FFFF;x&#xFFFD;y"
        U"&#x9;z&#xD7FF;&#xE000;&#x10000;</r>";
  std::string const expected_text
      = "p&#xD800;q&#xDFFF;r&#x110000;s&#x7FFFFFFF;t&#0;u&#x1;v&#xFFFE;w"
        "\xF4\x8F\xBF\xBF"
        "x\xEF\xBF\xBD"
        "y\tz\xED\x9F\xBF\xEE\x80\x80\xF0\x90\x80\x80";
  std::vector<unicode_encoding_t> const encodings = unicode_encodings ();
  for (std::size_t i = 0; i < encodings.size (); ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = load (encode (document, encodings[i].encoding, false),
                  encodings[i].encoding);
      ASSERT_TRUE (result) << encodings[i].name << ": "
                           << result.description ();
      EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                 "&#xD800;A&#x110000;&#0;&#xFFFF;")
          << encodings[i].name;
      EXPECT_EQ (std::string (doc.child ("r").text ().get ()), expected_text)
          << encodings[i].name;
    }
}

TEST_F (XmlEncodingTest,
        GivenAWideStreamWithAReferenceThatIsNotAChar_WhenLoaded_ThenItStays)
{
  std::wistringstream in (L"<r a=\"&#xD800;\">p&#xD800;q&#x41;</r>");
  ASSERT_TRUE (doc.load (in));
  EXPECT_STREQ (doc.child ("r").attribute ("a").value (), "&#xD800;");
  EXPECT_STREQ (doc.child ("r").text ().get (), "p&#xD800;qA");
}

TEST (XmlUtf8Conversion,
      GivenEveryShortByteString_WhenChecked_ThenTheReferenceAgrees)
{
  // The bytes at the edges of every range of Table 3-7 and a few outside of
  // it, in every string up to four bytes: the validator and the conversion of
  // a UTF-8 buffer (copying and in place) against the reference.
  unsigned char const alphabet[]
      = { 0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF,
          0xC0, 0xC1, 0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE,
          0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xF8, 0xFF };
  std::size_t const letters = sizeof (alphabet) / sizeof (alphabet[0]);
  std::size_t checked = 0;
  for (std::size_t length = 0; length <= 4; ++length)
    {
      std::size_t total = 1;
      for (std::size_t k = 0; k < length; ++k)
        total *= letters;
      for (std::size_t n = 0; n < total; ++n)
        {
          std::string text;
          std::size_t rest = n;
          for (std::size_t k = 0; k < length; ++k)
            {
              text.push_back (static_cast<char> (alphabet[rest % letters]));
              rest /= letters;
            }
          expect_conversion_matches_reference (text);
          ++checked;
        }
    }
  EXPECT_EQ (checked, 475255U);
}

TEST (
    XmlUtf8Conversion,
    GivenAnIllFormedSequenceAtEveryPlaceOfAnAsciiRun_WhenChecked_ThenTheReferenceAgrees)
{
  // The validator reads ASCII eight bytes at a time: put each pattern at
  // every offset of runs of every length around the word size, in the
  // middle and at the end.
  std::vector<bad_utf8_t> const bad = bad_utf8_cases ();
  for (std::size_t i = 0; i < bad.size (); ++i)
    for (std::size_t before = 0; before <= 20; ++before)
      for (std::size_t after = 0; after <= 20; ++after)
        {
          std::string const text = std::string (before, 'a') + bad[i].bytes
                                   + std::string (after, 'b');
          expect_conversion_matches_reference (text);
          // a valid character instead of the pattern
          expect_conversion_matches_reference (std::string (before, 'a')
                                               + "\xE2\x82\xAC"
                                               + std::string (after, 'b'));
        }
}

TEST (XmlUtf8Conversion,
      GivenPseudoRandomText_WhenChecked_ThenTheReferenceAgrees)
{
  // Mostly ASCII and valid sequences, with an invalid byte now and then.
  unsigned int seed = 12345;
  std::string const valid[] = { "a",
                                "b",
                                " ",
                                "\xC3\xA9",
                                "\xDF\xBF",
                                "\xE2\x82\xAC",
                                "\xED\x9F\xBF",
                                "\xEE\x80\x80",
                                "\xEF\xBF\xBD",
                                "\xF0\x90\x80\x80",
                                "\xF4\x8F\xBF\xBF" };
  std::string const invalid[] = { "\xC0",
                                  "\x80",
                                  "\xBF",
                                  "\xC1\xBF",
                                  "\xE0\x80\x80",
                                  "\xED\xA0\x80",
                                  "\xF4\x90\x80\x80",
                                  "\xF5",
                                  "\xFF",
                                  "\xE2\x82",
                                  "\xF0\x9F",
                                  "\xC3" };
  for (std::size_t round = 0; round < 3000; ++round)
    {
      std::string text;
      std::size_t const pieces = 1 + round % 90;
      for (std::size_t k = 0; k < pieces; ++k)
        {
          seed = seed * 1664525U + 1013904223U;
          unsigned int const pick = (seed >> 8) % 100;
          if (pick < 3)
            text += invalid[(seed >> 16)
                            % (sizeof (invalid) / sizeof (invalid[0]))];
          else
            text += valid[(seed >> 16) % (sizeof (valid) / sizeof (valid[0]))];
        }
      expect_conversion_matches_reference (text);
    }
}

// ------------------------------------------- what is cleaned: the declaration
//
// Only a document that declares UTF-8 or declares no encoding is cleaned. One
// that declares another encoding is loaded byte for byte: the library does not
// convert it, and the bytes of an 8-bit text (windows-1251, KOI8-R) are not
// errors. The declaration decides also when the caller names encoding_utf8.

namespace
{
struct declaration_case_t
{
  char const *name;
  std::string before;    // what stands in front of the root element
  bool cleaned;          // whether the bytes of the text are dropped
  bool converts_in_auto; // the encoding_auto path converts the document
};

std::vector<declaration_case_t>
declaration_cases ()
{
  std::vector<declaration_case_t> cases;
  cases.push_back (
      { "UTF-8", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>", true, false });
  cases.push_back (
      { "utf-8", "<?xml version=\"1.0\" encoding=\"utf-8\"?>", true, false });
  cases.push_back (
      { "Utf-8", "<?xml version=\"1.0\" encoding=\"Utf-8\"?>", true, false });
  cases.push_back ({ "single quotes", "<?xml version='1.0' encoding='UTF-8'?>",
                     true, false });
  cases.push_back (
      { "spaces around =", "<?xml version=\"1.0\" encoding = \"UTF-8\" ?>",
        true, false });
  cases.push_back ({ "no encoding in the declaration",
                     "<?xml version=\"1.0\"?>", true, false });
  cases.push_back ({ "standalone only",
                     "<?xml version=\"1.0\" standalone=\"yes\"?>", true,
                     false });
  cases.push_back ({ "no declaration", "", true, false });
  cases.push_back ({ "BOM, no declaration", "\xEF\xBB\xBF", true, false });
  cases.push_back ({ "BOM, UTF-8",
                     std::string ("\xEF\xBB\xBF")
                         + "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
                     true, false });
  cases.push_back ({ "windows-1251",
                     "<?xml version=\"1.0\" encoding=\"windows-1251\"?>",
                     false, false });
  cases.push_back ({ "Windows-1251 in capitals",
                     "<?xml version=\"1.0\" encoding=\"WINDOWS-1251\"?>",
                     false, false });
  cases.push_back ({ "KOI8-R", "<?xml version=\"1.0\" encoding=\"KOI8-R\"?>",
                     false, false });
  cases.push_back (
      { "cp866", "<?xml version=\"1.0\" encoding='cp866'?>", false, false });
  cases.push_back ({ "utf8 without the dash",
                     "<?xml version=\"1.0\" encoding=\"utf8\"?>", false,
                     false });
  cases.push_back ({ "UTF-80", "<?xml version=\"1.0\" encoding=\"UTF-80\"?>",
                     false, false });
  cases.push_back ({ "BOM, windows-1251",
                     std::string ("\xEF\xBB\xBF")
                         + "<?xml version=\"1.0\" encoding=\"windows-1251\"?>",
                     false, false });
  cases.push_back ({ "an encoding the parser does not read",
                     "<?xml version=\"1.0\" encoding=windows-1251?>", false,
                     false });
  cases.push_back ({ "ISO-8859-1",
                     "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>", false,
                     true });
  // the declaration the parser takes is not only the one at the first byte
  std::string const w1251
      = "<?xml version=\"1.0\" encoding=\"windows-1251\"?>";
  std::string const u8decl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  std::string const stylesheet
      = "<?xml-stylesheet href=\"encoding.xsl\" type=\"text/xsl\"?>";
  cases.push_back ({ "LF, then windows-1251", "\n" + w1251, false, false });
  cases.push_back ({ "space, then windows-1251", " " + w1251, false, false });
  cases.push_back (
      { "CR LF and TAB, then windows-1251", "\r\n\t" + w1251, false, false });
  cases.push_back ({ "BOM, LF, then windows-1251",
                     std::string ("\xEF\xBB\xBF\n") + w1251, false, false });
  cases.push_back (
      { "comment, then windows-1251", "<!--c-->" + w1251, false, false });
  cases.push_back (
      { "instruction, then windows-1251", "<?pi x?>" + w1251, false, false });
  cases.push_back ({ "xml-stylesheet, then windows-1251", stylesheet + w1251,
                     false, false });
  cases.push_back ({ "document type, then windows-1251",
                     "<!DOCTYPE r [<!ENTITY a \"x>\"><!-- > -->]>" + w1251,
                     false, false });
  cases.push_back ({ "uppercase target",
                     "<?XML version=\"1.0\" encoding=\"windows-1251\"?>",
                     false, false });
  cases.push_back ({ "mixed case target, UTF-8",
                     "<?Xml version=\"1.0\" encoding=\"UTF-8\"?>", true,
                     false });
  cases.push_back ({ "LF, then UTF-8", "\n" + u8decl, true, false });
  cases.push_back (
      { "LF, then no encoding", "\n<?xml version=\"1.0\"?>", true, false });
  cases.push_back (
      { "xml-stylesheet, no declaration", stylesheet, true, false });
  cases.push_back (
      { "xml-stylesheet, then UTF-8", stylesheet + u8decl, true, false });
  cases.push_back ({ "a comment that holds the word, no declaration",
                     "<!-- encoding=\"windows-1251\" -->", true, false });
  cases.push_back ({ "an instruction that holds the word, no declaration",
                     "<?pi encoding=\"windows-1251\"?>", true, false });
  return cases;
}

// the text of a document in windows-1251: "Privet" in Cyrillic
std::string
windows_1251_text ()
{
  return std::string ("\xCF\xF0\xE8\xE2\xE5\xF2");
}
} // namespace

TEST_F (
    XmlUtf8LoadTest,
    GivenADeclaration_WhenLoadedByAnyPath_ThenOnlyUtf8OrNoEncodingIsCleaned)
{
  std::vector<declaration_case_t> const cases = declaration_cases ();
  std::vector<load_path_t> const paths = load_paths ();
  std::string const bytes = "x" + windows_1251_text () + "y";
  for (std::size_t c = 0; c < cases.size (); ++c)
    for (std::size_t p = 0; p < paths.size (); ++p)
      {
        // encoding_auto converts a document that declares ISO-8859-1
        if (cases[c].converts_in_auto
            && paths[p] == load_path_t::auto_detected)
          continue;
        std::string const label
            = std::string (load_path_name (paths[p])) + ", " + cases[c].name;
        std::string const document = cases[c].before + "<r>" + bytes + "</r>";
        lumex::xml::text::xml_parse_result_t const result
            = load_by (paths[p], document);
        ASSERT_TRUE (result) << label << ": " << result.description ();
        EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
                   cases[c].cleaned ? table_clean (bytes) : bytes)
            << label;
      }
}

TEST_F (
    XmlUtf8LoadTest,
    GivenADeclarationOfAnotherEncoding_WhenLoadedWithEncodingUtf8_ThenTheDeclarationDecides)
{
  // The caller names UTF-8 and the document says windows-1251: the bytes of
  // the document are kept; with no declaration the same call cleans them.
  std::string const bytes = "x" + windows_1251_text () + "y";
  std::string const declared
      = "<?xml version=\"1.0\" encoding=\"windows-1251\"?><r>" + bytes
        + "</r>";
  ASSERT_TRUE (doc.load_buffer (declared.data (), declared.size (),
                                kparse_default, xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("r").text ().get ()), bytes);
  EXPECT_EQ (saved (xml_encoding::encoding_utf8,
                    kformat_raw | kformat_no_declaration),
             "<r>" + bytes + "</r>");

  std::string const undeclared = "<r>" + bytes + "</r>";
  ASSERT_TRUE (doc.load_buffer (undeclared.data (), undeclared.size (),
                                kparse_default, xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("r").text ().get ()), "xy");
}

TEST_F (XmlUtf8LoadTest,
        GivenAFragmentWithADeclaration_WhenAppended_ThenTheFragmentDecides)
{
  // append_buffer into a tree that already holds a node: the fragment decides
  // by its own declaration, as a document does.
  ASSERT_TRUE (doc.load_string ("<root/>"));
  std::string const kept
      = "<?xml version=\"1.0\" encoding=\"KOI8-R\"?><a>\xC0\xBC</a>";
  ASSERT_TRUE (doc.child ("root").append_buffer (kept.data (), kept.size (),
                                                 kparse_default,
                                                 xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("root").child ("a").text ().get ()),
             "\xC0\xBC");
  std::string const cleaned = "<b>\xC0\xBC</b>";
  ASSERT_TRUE (doc.child ("root").append_buffer (
      cleaned.data (), cleaned.size (), kparse_default,
      xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("root").child ("b").text ().get ()), "");
}

TEST (
    XmlUtf8Declaration,
    GivenTheStartOfADocument_WhenAskedWhetherItIsCleaned_ThenTheDeclarationAnswers)
{
  struct case_t
  {
    std::string start;
    bool cleaned;
  };
  std::vector<case_t> cases;
  cases.push_back ({ "", true });
  cases.push_back ({ "<", true });
  cases.push_back ({ "<r/>", true });
  cases.push_back ({ "<?xml", true });
  cases.push_back ({ "<?xml?>", true });
  cases.push_back ({ "<?xml version=\"1.0\"?><r/>", true });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"UTF-8\"?>", true });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"uTf-8\"?>", true });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"UTF-16\"?>", false });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"UTF-8x\"?>", false });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"UTF_8\"?>", false });
  cases.push_back (
      { "<?xml version=\"1.0\" encoding=\"windows-1251\"?>", false });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=windows-1251?>", false });
  cases.push_back ({ std::string ("\xEF\xBB\xBF"), true });
  cases.push_back (
      { std::string (
            "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"KOI8-R\"?>"),
        false });
  // the declaration of the prolog, as the parser takes it
  std::string const w1251
      = "<?xml version=\"1.0\" encoding=\"windows-1251\"?>";
  cases.push_back ({ "\n" + w1251, false });
  cases.push_back ({ " " + w1251, false });
  cases.push_back ({ "\r\n\t " + w1251, false });
  cases.push_back ({ std::string ("\xEF\xBB\xBF\n") + w1251, false });
  cases.push_back ({ "<!--c-->" + w1251, false });
  cases.push_back ({ "<!-- a --> <!-- b -->\n" + w1251, false });
  cases.push_back ({ "<?pi x?>" + w1251, false });
  cases.push_back ({ "<?xml-stylesheet href=\"a\"?>" + w1251, false });
  cases.push_back ({ "<!DOCTYPE r>" + w1251, false });
  cases.push_back ({ "<!DOCTYPE r [<!ENTITY a \"]>\">]>" + w1251, false });
  cases.push_back (
      { "<?XML version=\"1.0\" encoding=\"windows-1251\"?>", false });
  cases.push_back ({ "<?xMl version=\"1.0\" encoding=\"UTF-8\"?>", true });
  cases.push_back ({ "<?xml-stylesheet href=\"encoding.xsl\"?><r/>", true });
  cases.push_back ({ "<?xml-stylesheet encoding=\"koi8-r\"?>", true });
  cases.push_back ({ "<?xmlx encoding=\"koi8-r\"?>", true });
  cases.push_back ({ "<?xm encoding=\"koi8-r\"?>", true });
  cases.push_back ({ "<!-- <?xml encoding=\"koi8-r\"?> -->", true });
  // the prolog ends at the first element or text
  cases.push_back ({ "<r/>" + w1251, true });
  cases.push_back ({ "text" + w1251, true });
  cases.push_back ({ "<![CDATA[x]]>" + w1251, true });
  // unreadable and cut short
  cases.push_back ({ "<?xml version=\"1.0\" encoding=\"UTF-", false });
  cases.push_back ({ "<?xml version=\"1.0\" encod", true });
  cases.push_back ({ "<?xml version=\"1.0\" encoding", false });
  cases.push_back ({ "<?xml version=\"1.0\" encoding=", false });
  cases.push_back ({ "<?xml version=\"1.0\" encoding='UTF-8", false });
  cases.push_back (
      { "<?xml version=\"1.0\" encoding = 'UTF-8' standalone=yes?>", true });
  cases.push_back (
      { "<?xml version=\"1.0\" standalone=yes encoding=\"koi8-r\"?>", false });
  cases.push_back ({ "<!--", true });
  cases.push_back ({ "<!-- x", true });
  cases.push_back ({ "<!DOCTYPE", true });
  cases.push_back ({ "<!DOCTYPE r [", true });
  cases.push_back ({ "<?", true });
  cases.push_back ({ "<?pi", true });
  // a mention of the word outside the declaration decides nothing
  cases.push_back ({ "<r encoding=\"windows-1251\"/>", true });
  cases.push_back (
      { "<?xml version=\"1.0\"?><!-- encoding=\"koi8-r\" -->", true });
  for (std::size_t i = 0; i < cases.size (); ++i)
    EXPECT_EQ (
        lumex::xml::utility::is_utf8_cleaned_on_load (
            reinterpret_cast<std::uint8_t const *> (cases[i].start.data ()),
            cases[i].start.size ()),
        cases[i].cleaned)
        << cases[i].start;
}

TEST (
    XmlUtf8Declaration,
    GivenEveryPrefixOfADocument_WhenAskedWhetherItIsCleaned_ThenNothingBehindTheSizeIsRead)
{
  // The answer for a prefix must not depend on the bytes behind it: the same
  // prefix is given with two different tails, one that would complete a
  // declaration and one that would end the prolog. An exact-size heap block
  // makes an address checker report a read past the end as well.
  std::vector<std::string> documents;
  documents.push_back ("\xEF\xBB\xBF\n<!-- c --><?pi x?><?XML version=\"1.0\" "
                       "encoding=\"windows-1251\"?><r/>");
  documents.push_back ("<!DOCTYPE r [<!ENTITY a \"x>\"><!-- > -->]>\n"
                       "<?xml version='1.0' encoding='UTF-8'?><r/>");
  documents.push_back ("<?xml-stylesheet href=\"encoding.xsl\"?><r/>");
  std::string const tails[] = { std::string ("?> encoding=\"koi8-r\"?><?xml"),
                                std::string ("<r/>   ") };
  for (std::size_t k = 0; k < documents.size (); ++k)
    for (std::size_t length = 0; length <= documents[k].size (); ++length)
      {
        bool answers[2] = { false, false };
        for (std::size_t t = 0; t < 2; ++t)
          {
            std::vector<std::uint8_t> block (
                documents[k].begin (),
                documents[k].begin () + static_cast<std::ptrdiff_t> (length));
            std::vector<std::uint8_t> const tail (tails[t].begin (),
                                                  tails[t].end ());
            std::vector<std::uint8_t> padded (block);
            padded.insert (padded.end (), tail.begin (), tail.end ());
            std::uint8_t *const exact
                = new std::uint8_t[length == 0 ? 1 : length];
            std::copy (block.begin (), block.end (), exact);
            bool const with_tail
                = lumex::xml::utility::is_utf8_cleaned_on_load (
                    padded.empty () ? nullptr : &padded[0], length);
            bool const exact_answer
                = lumex::xml::utility::is_utf8_cleaned_on_load (exact, length);
            delete[] exact;
            EXPECT_EQ (with_tail, exact_answer) << k << " length " << length;
            answers[t] = with_tail;
          }
        EXPECT_EQ (answers[0], answers[1]) << k << " length " << length;
      }
}

// ---------------------------------------- the end of the in-place buffer

TEST_F (
    XmlUtf8LoadTest,
    GivenAFragmentWithoutATerminator_WhenLoadedInPlaceWithBytesDropped_ThenNoValidByteIsLost)
{
  // The parser takes the last byte of the buffer for the end and writes a NUL
  // over it: after the cleaning that byte is the freed one, not the last byte
  // of the text. "ab" + FF used to give "a".
  unsigned int const options
      = kparse_default | lumex::xml::constants::Constants::kparse_fragment;
  std::string const without = std::string ("ab\xFF");
  std::string const with_terminator = std::string ("ab\xFF\0", 4);
  load_path_t const paths[] = { load_path_t::inplace, load_path_t::inplace_own,
                                load_path_t::buffer };
  for (std::size_t p = 0; p < sizeof (paths) / sizeof (paths[0]); ++p)
    {
      ASSERT_TRUE (load_by (paths[p], without, options))
          << load_path_name (paths[p]);
      EXPECT_STREQ (doc.child_value (), "ab") << load_path_name (paths[p]);
      ASSERT_TRUE (load_by (paths[p], with_terminator, options))
          << load_path_name (paths[p]);
      EXPECT_STREQ (doc.child_value (), "ab") << load_path_name (paths[p]);
    }
}

TEST_F (
    XmlUtf8LoadTest,
    GivenALessThanSignLeftInTheFreedBytes_WhenLoadedInPlace_ThenNoErrorIsReported)
{
  // The parser saves the last byte of the buffer and reports a '<' there as a
  // stray tag. The bytes behind the cleaned text are the old ones: here the
  // freed bytes start with the '<' of "<-->" that the comment still holds
  // further down, so the byte after the cleaned text must be made a NUL.
  std::string const cleaned = "<a/><!--x<-->";
  std::string const text = std::string ("\xFF\xFF\xFF\xFF") + cleaned;
  load_path_t const paths[]
      = { load_path_t::inplace, load_path_t::inplace_own };
  for (std::size_t p = 0; p < sizeof (paths) / sizeof (paths[0]); ++p)
    {
      lumex::xml::text::xml_parse_result_t const result
          = load_by (paths[p], text, kFullOptions);
      EXPECT_TRUE (result) << load_path_name (paths[p]) << ": "
                           << result.description ();
      EXPECT_TRUE (doc.child ("a")) << load_path_name (paths[p]);
    }
}

// ------------------------------------------------------------- offsets

TEST_F (
    XmlUtf8LoadTest,
    GivenDroppedBytes_WhenAnErrorOrANodeIsLocated_ThenTheOffsetCountsTheCleanedText)
{
  // The offsets are those of the text that the parser read: the document
  // without the dropped bytes. The same document written without them gives
  // the same numbers on the same path; on the base the bytes were part of the
  // count.
  std::string const dropped = "\xC0\xBC\xC0\xBC";
  std::string const bad_error = "<r>" + dropped + "</x>";
  std::string const clean_error = "<r></x>";
  std::string const bad_node = "<r>" + dropped + "<x/></r>";
  std::string const clean_node = "<r><x/></r>";
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      std::string const label = load_path_name (paths[p]);

      lumex::xml::text::xml_parse_result_t const bad
          = load_by (paths[p], bad_error, kparse_default);
      lumex::xml::text::xml_parse_result_t const clean
          = load_by (paths[p], clean_error, kparse_default);
      EXPECT_FALSE (bad) << label;
      EXPECT_EQ (bad.status, clean.status) << label;
      EXPECT_EQ (bad.offset, clean.offset) << label;

      // append_buffer parses into a tree that has a root: no node offset
      if (paths[p] == load_path_t::append_buffer)
        continue;
      ASSERT_TRUE (load_by (paths[p], bad_node, kparse_default)) << label;
      ptrdiff_t const bad_at = doc.child ("r").child ("x").offset_debug ();
      ASSERT_TRUE (load_by (paths[p], clean_node, kparse_default)) << label;
      ptrdiff_t const clean_at = doc.child ("r").child ("x").offset_debug ();
      EXPECT_EQ (bad_at, clean_at) << label;
      EXPECT_EQ (bad_at, 4) << label;
    }
  EXPECT_EQ (load_by (load_path_t::buffer, bad_error, kparse_default).offset,
             5);
}

TEST_F (
    XmlUtf8LoadTest,
    GivenAnErrorAtTheEndOfTheInput_WhenBytesAreDropped_ThenTheOffsetIsThatOfTheCleanInputOnEveryPath)
{
  // A file and a stream end with the NUL that is read into the buffer: the
  // cleaned text must not get a second one, or the parser's roll-back of an
  // error at the terminator does not fire and the offset is one too high.
  std::string const documents[][2] = {
    { std::string ("<r>\xFF"), "<r>" },
    { std::string ("<r/><\xFF"), "<r/><" },
    { std::string ("<r>t\xC0\xBC"), "<r>t" },
    { std::string ("<r a=\"x\xFF"), "<r a=\"x" },
  };
  std::vector<load_path_t> const paths = load_paths ();
  for (std::size_t c = 0; c < sizeof (documents) / sizeof (documents[0]); ++c)
    for (std::size_t p = 0; p < paths.size (); ++p)
      {
        std::string const label
            = std::string (load_path_name (paths[p])) + ", " + documents[c][1];
        lumex::xml::text::xml_parse_result_t const clean
            = load_by (paths[p], documents[c][1], kparse_default);
        lumex::xml::text::xml_parse_result_t const bad
            = load_by (paths[p], documents[c][0], kparse_default);
        EXPECT_FALSE (clean) << label;
        EXPECT_FALSE (bad) << label;
        EXPECT_EQ (bad.status, clean.status) << label;
        EXPECT_EQ (bad.offset, clean.offset) << label;
      }
  EXPECT_EQ (load_by (load_path_t::file, "<r>\xFF", kparse_default).offset, 2);
  EXPECT_EQ (load_by (load_path_t::stream, "<r>\xFF", kparse_default).offset,
             2);
  EXPECT_EQ (load_by (load_path_t::buffer, "<r/><\xFF", kparse_default).offset,
             4);
  EXPECT_EQ (load_by (load_path_t::file, "<r/><\xFF", kparse_default).offset,
             4);
}

// ------------------------------------------------------------- Latin-1

TEST_F (
    XmlUtf8LoadTest,
    GivenACleanedTree_WhenSavedAsLatin1_ThenWhatLatin1CannotHoldIsAQuestionMark)
{
  // The saves in UTF-8, UTF-16 and UTF-32 write the same text; Latin-1 writes
  // '?' above U+00FF, for a cleaned tree as for any other.
  std::string const text
      = std::string ("<r>\xC3\xA9\xC0\xBC") + "\xE2\x82\xAC</r>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (), kparse_default,
                                xml_encoding::encoding_utf8));
  EXPECT_EQ (saved (xml_encoding::encoding_latin1,
                    kformat_raw | kformat_no_declaration),
             "<r>\xE9?</r>");
  EXPECT_EQ (saved (xml_encoding::encoding_utf8,
                    kformat_raw | kformat_no_declaration),
             "<r>\xC3\xA9\xE2\x82\xAC</r>");
}

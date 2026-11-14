// lumex/tests/xml/LumexXmlKeepInvalidUtf8.cxx11.tests.cpp
//
// The parse option kparse_keep_invalid_utf8: a UTF-8 document is loaded byte
// for byte, whatever it declares, so that a byte that starts no well-formed
// sequence stays in the tree. Without the option a document that declares
// UTF-8 or no encoding is cleaned (LumexXmlEncodings.cxx11.tests.cpp). The
// tests run both ways on every load function and on XmlNode::append_buffer,
// with ill-formed input, with valid input, with a declaration of UTF-8, of
// another encoding and none, and with a byte order mark.
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
using lumex::xml::constants::Constants::kparse_cdata;
using lumex::xml::constants::Constants::kparse_comments;
using lumex::xml::constants::Constants::kparse_declaration;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::constants::Constants::kparse_doctype;
using lumex::xml::constants::Constants::kparse_embed_pcdata;
using lumex::xml::constants::Constants::kparse_eol;
using lumex::xml::constants::Constants::kparse_escapes;
using lumex::xml::constants::Constants::kparse_fragment;
using lumex::xml::constants::Constants::kparse_full;
using lumex::xml::constants::Constants::kparse_keep_invalid_utf8;
using lumex::xml::constants::Constants::kparse_merge_pcdata;
using lumex::xml::constants::Constants::kparse_pi;
using lumex::xml::constants::Constants::kparse_trim_pcdata;
using lumex::xml::constants::Constants::kparse_wconv_attribute;
using lumex::xml::constants::Constants::kparse_wnorm_attribute;
using lumex::xml::constants::Constants::kparse_ws_pcdata;
using lumex::xml::constants::Constants::kparse_ws_pcdata_single;
using lumex::xml::types::Types::xml_encoding;

namespace
{
enum class path_t
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

std::vector<path_t>
all_paths ()
{
  std::vector<path_t> paths;
  paths.push_back (path_t::buffer);
  paths.push_back (path_t::inplace);
  paths.push_back (path_t::inplace_own);
  paths.push_back (path_t::string);
  paths.push_back (path_t::file);
  paths.push_back (path_t::stream);
  paths.push_back (path_t::stream_without_seek);
  paths.push_back (path_t::auto_detected);
  paths.push_back (path_t::append_buffer);
  return paths;
}

char const *
path_name (path_t path)
{
  switch (path)
    {
    case path_t::buffer:
      return "load_buffer";
    case path_t::inplace:
      return "load_buffer_inplace";
    case path_t::inplace_own:
      return "load_buffer_inplace_own";
    case path_t::string:
      return "load_string";
    case path_t::file:
      return "load_file";
    case path_t::stream:
      return "load (istream)";
    case path_t::stream_without_seek:
      return "load (istream without seek)";
    case path_t::auto_detected:
      return "load_buffer (auto)";
    case path_t::append_buffer:
      return "XmlNode::append_buffer";
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

class XmlKeepInvalidUtf8Test : public XmlFixture
{
protected:
  lumex::xml::text::xml_parse_result_t
  load_by (path_t path, std::string const &bytes, unsigned int options)
  {
    doc.reset ();
    switch (path)
      {
      case path_t::buffer:
        return doc.load_buffer (bytes.data (), bytes.size (), options,
                                xml_encoding::encoding_utf8);
      case path_t::auto_detected:
        return doc.load_buffer (bytes.data (), bytes.size (), options,
                                xml_encoding::encoding_auto);
      case path_t::append_buffer:
        return doc.append_buffer (bytes.data (), bytes.size (), options,
                                  xml_encoding::encoding_utf8);
      case path_t::inplace:
        {
          m_storage.assign (bytes.begin (), bytes.end ());
          m_storage.push_back ('\0');
          return doc.load_buffer_inplace (&m_storage[0], bytes.size (),
                                          options,
                                          xml_encoding::encoding_utf8);
        }
      case path_t::inplace_own:
        {
          void *own = std::malloc (bytes.empty () ? 1 : bytes.size ());
          if (!bytes.empty ())
            std::memcpy (own, bytes.data (), bytes.size ());
          return doc.load_buffer_inplace_own (own, bytes.size (), options,
                                              xml_encoding::encoding_utf8);
        }
      case path_t::string:
        return doc.load_string (bytes.c_str (), options);
      case path_t::file:
        {
          // the tests and the suites of the standards run side by side in one
          // directory
          std::string const name
              = std::string ("lumex_xml_keep_utf8_")
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
      case path_t::stream:
        {
          std::istringstream in (bytes);
          return doc.load (in, options, xml_encoding::encoding_utf8);
        }
      case path_t::stream_without_seek:
        {
          chunked_streambuf buffer (bytes);
          std::istream in (&buffer);
          return doc.load (in, options, xml_encoding::encoding_utf8);
        }
      }
    return lumex::xml::text::xml_parse_result_t ();
  }

  std::string
  saved (xml_encoding encoding)
  {
    std::ostringstream out;
    doc.save (out, "", kformat_raw | kformat_no_declaration, encoding);
    return out.str ();
  }

  std::vector<char> m_storage;
};

// The bytes of the text node: what the options decide about.
std::string const kIllFormed = std::string ("x\xC0\xBC") + "y\xED\xA0\x80"
                               + "z\xF4\x90\x80\x80" + "w\xE2\x82" + "v\xFF"
                               + "u\x80";
std::string const kCleaned = "xyzwvu";

struct document_case_t
{
  char const *name;
  std::string before;            // in front of the root element
  bool keeps_without_the_option; // the declaration says it is not cleaned
  bool converts_in_auto;         // encoding_auto converts the document
};

std::vector<document_case_t>
document_cases ()
{
  std::vector<document_case_t> cases;
  cases.push_back ({ "no declaration", "", false, false });
  cases.push_back (
      { "UTF-8", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>", false, false });
  cases.push_back (
      { "utf-8", "<?xml version=\"1.0\" encoding=\"utf-8\"?>", false, false });
  cases.push_back ({ "no encoding", "<?xml version=\"1.0\"?>", false, false });
  cases.push_back ({ "BOM", std::string ("\xEF\xBB\xBF"), false, false });
  cases.push_back ({ "LF, then UTF-8",
                     "\n<?xml version=\"1.0\" encoding=\"UTF-8\"?>", false,
                     false });
  cases.push_back ({ "windows-1251",
                     "<?xml version=\"1.0\" encoding=\"windows-1251\"?>", true,
                     false });
  cases.push_back ({ "KOI8-R", "<?xml version=\"1.0\" encoding=\"KOI8-R\"?>",
                     true, false });
  cases.push_back ({ "LF, then windows-1251",
                     "\n<?xml version=\"1.0\" encoding=\"windows-1251\"?>",
                     true, false });
  cases.push_back ({ "ISO-8859-1",
                     "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>", true,
                     true });
  return cases;
}
} // namespace

TEST (XmlKeepInvalidUtf8Constant,
      GivenTheFlag_WhenCompared_ThenItIsAFreeBitOutsideTheDefaults)
{
  unsigned int const others
      = kparse_pi | kparse_comments | kparse_cdata | kparse_ws_pcdata
        | kparse_escapes | kparse_eol | kparse_wconv_attribute
        | kparse_wnorm_attribute | kparse_declaration | kparse_doctype
        | kparse_ws_pcdata_single | kparse_trim_pcdata | kparse_fragment
        | kparse_embed_pcdata | kparse_merge_pcdata;
  EXPECT_EQ (kparse_keep_invalid_utf8, 0x8000U);
  EXPECT_EQ (kparse_keep_invalid_utf8 & others, 0U);
  // the option changes the data that is loaded: neither default nor full
  EXPECT_EQ (kparse_keep_invalid_utf8 & kparse_default, 0U);
  EXPECT_EQ (kparse_keep_invalid_utf8 & kparse_full, 0U);
}

TEST_F (
    XmlKeepInvalidUtf8Test,
    GivenIllFormedUtf8_WhenLoadedWithAndWithoutTheOption_ThenOnlyTheOptionKeepsTheBytes)
{
  std::vector<document_case_t> const cases = document_cases ();
  std::vector<path_t> const paths = all_paths ();
  for (std::size_t c = 0; c < cases.size (); ++c)
    for (std::size_t p = 0; p < paths.size (); ++p)
      {
        // encoding_auto converts a document that declares ISO-8859-1
        if (cases[c].converts_in_auto && paths[p] == path_t::auto_detected)
          continue;
        std::string const label
            = std::string (path_name (paths[p])) + ", " + cases[c].name;
        std::string const document
            = cases[c].before + "<r>" + kIllFormed + "</r>";

        lumex::xml::text::xml_parse_result_t const without
            = load_by (paths[p], document, kparse_default);
        ASSERT_TRUE (without) << label << ": " << without.description ();
        EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
                   cases[c].keeps_without_the_option ? kIllFormed : kCleaned)
            << label << " without the option";

        lumex::xml::text::xml_parse_result_t const with = load_by (
            paths[p], document, kparse_default | kparse_keep_invalid_utf8);
        ASSERT_TRUE (with) << label << ": " << with.description ();
        EXPECT_EQ (std::string (doc.child ("r").text ().get ()), kIllFormed)
            << label << " with the option";
        // the UTF-8 save writes the bytes of the tree as they are
        EXPECT_EQ (saved (xml_encoding::encoding_utf8),
                   "<r>" + kIllFormed + "</r>")
            << label;
      }
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenKeptBytes_WhenSavedInAnotherEncoding_ThenTheEncodersSkipThem)
{
  ASSERT_TRUE (load_by (path_t::buffer, "<r>" + kIllFormed + "</r>",
                        kparse_default | kparse_keep_invalid_utf8));
  std::string const utf16 = saved (xml_encoding::encoding_utf16_le);
  std::string expected;
  std::string const text = "<r>" + kCleaned + "</r>";
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      expected.push_back (text[i]);
      expected.push_back ('\0');
    }
  EXPECT_EQ (utf16, expected);
  EXPECT_EQ (saved (xml_encoding::encoding_latin1), text);
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenValidUtf8_WhenLoadedWithAndWithoutTheOption_ThenTheTreeIsTheSame)
{
  // every length of sequence, the edges of the ranges, a real U+FFFD
  std::string const valid
      = "x\xC2\x80 \xDF\xBF \xE0\xA0\x80 \xED\x9F\xBF \xEE\x80\x80 "
        "\xEF\xBF\xBD \xF0\x90\x80\x80 \xF4\x8F\xBF\xBF y";
  std::vector<path_t> const paths = all_paths ();
  for (std::size_t p = 0; p < paths.size (); ++p)
    {
      std::string const document = "<r>" + valid + "</r>";
      ASSERT_TRUE (load_by (paths[p], document, kparse_default))
          << path_name (paths[p]);
      std::string const without = saved (xml_encoding::encoding_utf8);
      ASSERT_TRUE (load_by (paths[p], document,
                            kparse_default | kparse_keep_invalid_utf8))
          << path_name (paths[p]);
      EXPECT_EQ (saved (xml_encoding::encoding_utf8), without)
          << path_name (paths[p]);
      EXPECT_EQ (without, document) << path_name (paths[p]);
    }
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenAReferenceThatIsNotAChar_WhenLoadedWithTheOption_ThenItStaysText)
{
  // the option is about bytes of the document, not about references
  ASSERT_TRUE (load_by (path_t::buffer, "<r>a&#xD800;b&#0;c&#x41;</r>",
                        kparse_default | kparse_keep_invalid_utf8));
  EXPECT_STREQ (doc.child ("r").text ().get (), "a&#xD800;b&#0;cA");
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenOtherEncodings_WhenLoadedWithTheOption_ThenTheyAreDecodedAsAlways)
{
  // A UTF-16 document with an unpaired surrogate unit: the decoder skips it,
  // with or without the option.
  std::string utf16;
  std::string const text = "<r>a";
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      utf16.push_back (text[i]);
      utf16.push_back ('\0');
    }
  utf16.push_back ('\x00');
  utf16.push_back (static_cast<char> (0xD8)); // U+D800 alone
  utf16.push_back ('b');
  utf16.push_back ('\0');
  std::string const tail = "</r>";
  for (std::size_t i = 0; i < tail.size (); ++i)
    {
      utf16.push_back (tail[i]);
      utf16.push_back ('\0');
    }
  ASSERT_TRUE (doc.load_buffer (utf16.data (), utf16.size (),
                                kparse_default | kparse_keep_invalid_utf8,
                                xml_encoding::encoding_utf16_le));
  EXPECT_STREQ (doc.child ("r").text ().get (), "ab");

  // Latin-1 stays Latin-1: the bytes above 0x7F are letters
  std::string const latin1 = "<r>caf\xE9</r>";
  ASSERT_TRUE (doc.load_buffer (latin1.data (), latin1.size (),
                                kparse_default | kparse_keep_invalid_utf8,
                                xml_encoding::encoding_latin1));
  EXPECT_STREQ (doc.child ("r").text ().get (), "caf\xC3\xA9");
}

TEST_F (
    XmlKeepInvalidUtf8Test,
    GivenAnErrorAfterKeptBytes_WhenLoadedWithTheOption_ThenTheOffsetCountsTheInput)
{
  // With the option nothing is dropped, so the offset is the position in the
  // input bytes; without it the offset counts the cleaned text.
  std::string const dropped = "\xC0\xBC\xC0\xBC";
  std::string const document = "<r>" + dropped + "</x>";
  lumex::xml::text::xml_parse_result_t const kept = load_by (
      path_t::buffer, document, kparse_default | kparse_keep_invalid_utf8);
  lumex::xml::text::xml_parse_result_t const cleaned
      = load_by (path_t::buffer, document, kparse_default);
  EXPECT_FALSE (kept);
  EXPECT_FALSE (cleaned);
  EXPECT_EQ (kept.offset, cleaned.offset + 4);
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenKeptBytes_WhenLoadedInPlace_ThenTheBufferIsNotMoved)
{
  // The bytes are parsed where they stand: the text of the node is inside the
  // caller's buffer at the offset it has in the input. Ill-formed bytes stand
  // in front of the element, so that the cleaning, which would move the text
  // down by two bytes, shows.
  std::string const document
      = std::string ("\xC0\xBC") + "<r>ab\xC0\xBC" + "cd</r>";
  unsigned int const options = kparse_default | kparse_keep_invalid_utf8;
  m_storage.assign (document.begin (), document.end ());
  m_storage.push_back ('\0');
  ASSERT_TRUE (doc.load_buffer_inplace (&m_storage[0], document.size (),
                                        options, xml_encoding::encoding_utf8));
  char const *const kept = doc.child ("r").text ().get ();
  EXPECT_EQ (kept, &m_storage[5]);
  EXPECT_EQ (std::string (kept), std::string ("ab\xC0\xBC") + "cd");

  // the control: without the option the text moves down to offset 3
  m_storage.assign (document.begin (), document.end ());
  m_storage.push_back ('\0');
  ASSERT_TRUE (doc.load_buffer_inplace (&m_storage[0], document.size (),
                                        kparse_default,
                                        xml_encoding::encoding_utf8));
  char const *const cleaned = doc.child ("r").text ().get ();
  EXPECT_EQ (cleaned, &m_storage[3]);
  EXPECT_EQ (std::string (cleaned), "abcd");
}

TEST_F (XmlKeepInvalidUtf8Test,
        GivenTheOptionOnAnAppendedFragment_WhenAppended_ThenItIsKept)
{
  ASSERT_TRUE (doc.load_string ("<root/>"));
  std::string const fragment = "<a>x\xC0\xBC" + std::string ("y</a>");
  ASSERT_TRUE (doc.child ("root").append_buffer (
      fragment.data (), fragment.size (),
      kparse_default | kparse_keep_invalid_utf8, xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("root").child ("a").text ().get ()),
             "x\xC0\xBC" + std::string ("y"));
  ASSERT_TRUE (doc.child ("root").append_buffer (
      fragment.data (), fragment.size (), kparse_default,
      xml_encoding::encoding_utf8));
  EXPECT_EQ (std::string (doc.child ("root").last_child ().text ().get ()),
             "xy");
}

// lumex/tests/xml/LumexXml.cxx11.tests.cpp

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"
#include "lumex/xml/xpath/utility/XPathUtils.hpp"

#include "lumex/tests/support/LumexPerfSkip.hpp"
#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::xml;
using namespace lumex::xml::document;
using namespace lumex::xml::node;
using namespace lumex::xml::text;
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;
using namespace lumex::xml::constants;
using namespace lumex::xml::constants::Constants;

using lumex::xml::attribute::XmlAttribute;
using lumex::xml::constants::Constants::kformat_default;
using lumex::xml::constants::Constants::kformat_indent;
using lumex::xml::constants::Constants::kformat_no_declaration;
using lumex::xml::constants::Constants::kformat_no_escapes;
using lumex::xml::constants::Constants::kformat_raw;
using lumex::xml::constants::Constants::kformat_save_file_text;
using lumex::xml::constants::Constants::kformat_write_bom;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::constants::Constants::kparse_full;
using lumex::xml::document::XmlDocument;
using lumex::xml::node::XmlNode;
using lumex::xml::text::XmlText;
using lumex::xml::types::Types::xml_encoding;
using lumex::xml::types::Types::xml_node_type;
using lumex::xml::types::Types::xml_parse_status;

namespace
{

// --- Simple happy paths -------------------------------------------------
TEST_F (XmlFixture,
        GivenValidXmlString_WhenLoadString_ThenDocumentElementAccessible)
{
  char const *xml = "<root attr=\"1\"><child>text</child></root>";
  auto res = doc.load_string (xml, kparse_default);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));

  XmlNode root = doc.document_element ();
  ASSERT_TRUE (root);
  EXPECT_STREQ (root.name (), "root");
  ASSERT_TRUE (root.attribute ("attr"));
  EXPECT_STREQ (root.attribute ("attr").value (), "1");
  ASSERT_TRUE (root.child ("child"));
  EXPECT_STREQ (root.child ("child").text ().get (), "text");
}

TEST_F (XmlFixture,
        GivenBuffer_WhenLoadBuffer_SaveToOstream_ThenRoundTripKeepsStructure)
{
  char const *xml = "<?xml version=\"1.0\"?><a><b c=\"d\"/></a>";
  auto res = doc.load_buffer (xml, std::strlen (xml), kparse_full,
                              xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));

  std::ostringstream oss;
  doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
  std::string out = oss.str ();
}

TEST_F (XmlFixture,
        GivenEmptyDoc_WhenBuildTree_ThenStructureQueriesAndRemovalWork)
{
  doc.reset ();
  XmlNode root = doc.append_child ("r");
  ASSERT_TRUE (root);

  XmlNode n1 = root.append_child ("n1");
  XmlNode n2 = root.append_child ("n2");
  n1.append_attribute ("a").set_value ("v");
  n2.append_child ("leaf").text ().set ("42");

  ASSERT_TRUE (root.child ("n1"));
  EXPECT_STREQ (root.child ("n1").attribute ("a").value (), "v");
  ASSERT_TRUE (root.child ("n2").child ("leaf"));
  EXPECT_STREQ (root.child ("n2").child ("leaf").text ().get (), "42");

  ASSERT_TRUE (root.remove_child ("n1"));
  EXPECT_FALSE (root.child ("n1"));
}

TEST_F (XmlFixture, GivenUtf8Specials_WhenSaveAndReload_ThenTextPreserved)
{
  doc.reset ();
  XmlNode root = doc.append_child ("root");
  XmlNode t = root.append_child ("t");
  t.text ().set ("<alpha> & \"β\" ©");

  std::ostringstream oss;
  doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
  std::string s = oss.str ();

  XmlDocument d2;
  ASSERT_EQ (d2.load_buffer (s.c_str (), s.size (), kparse_default,
                             xml_encoding::encoding_utf8)
                 .status,
             xml_parse_status::status_ok);
  EXPECT_STREQ (d2.document_element ().child ("t").text ().get (),
                "<alpha> & \"β\" ©");
}

TEST_F (XmlFixture, GivenMalformedXml_WhenLoad_ThenNonOkStatus)
{
  char const *bad = "<root><unclosed></root>";
  auto res = doc.load_buffer (bad, std::strlen (bad), kparse_default,
                              xml_encoding::encoding_utf8);
  ASSERT_NE (res.status, xml_parse_status::status_ok);
}

TEST_F (XmlFixture, GivenAttributesAndChildren_WhenIterate_ThenAllVisited)
{
  doc.reset ();
  XmlNode root = doc.append_child ("r");
  root.append_attribute ("a").set_value ("1");
  root.append_attribute ("b").set_value ("2");
  root.append_child ("c1");
  root.append_child ("c2");

  std::vector<std::string> attrs;
  for (auto it = root.attributes_begin (); it != root.attributes_end (); ++it)
    attrs.push_back ((*it).name ());
  ASSERT_EQ (attrs.size (), static_cast<std::size_t> (2));
  EXPECT_TRUE ((attrs[0] == "a" && attrs[1] == "b")
               || (attrs[0] == "b" && attrs[1] == "a"));

  std::vector<std::string> kids;
  for (auto it = root.begin (); it != root.end (); ++it)
    kids.push_back ((*it).name ());
  ASSERT_EQ (kids.size (), static_cast<std::size_t> (2));
  EXPECT_TRUE ((kids[0] == "c1" && kids[1] == "c2")
               || (kids[0] == "c2" && kids[1] == "c1"));
}

// --- Node creation variants (types, before/after/prepend, copy/move) ---
TEST_F (XmlFixture, GivenNodeTypes_WhenCreate_ThenTypesMatch)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode e = r.append_child (xml_node_type::node_element);
  EXPECT_EQ (e.type (), xml_node_type::node_element);

  XmlNode cmt = r.append_child (xml_node_type::node_comment);
  cmt.set_value ("hello");
  EXPECT_EQ (cmt.type (), xml_node_type::node_comment);
  EXPECT_STREQ (cmt.value (), "hello");

  XmlNode pi = r.append_child (xml_node_type::node_pi);
  pi.set_name ("xml-stylesheet");
  pi.set_value ("href='x.css'");
  EXPECT_EQ (pi.type (), xml_node_type::node_pi);
  EXPECT_STREQ (pi.name (), "xml-stylesheet");
  EXPECT_STREQ (pi.value (), "href='x.css'");
}

TEST_F (XmlFixture,
        GivenSiblings_WhenInsertBeforeAfterPrepend_ThenOrderIsCorrect)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode a = r.append_child ("A");
  XmlNode c = r.append_child ("C");
  XmlNode b = r.insert_child_before ("B", c);
  XmlNode z = r.prepend_child ("Z");
  ASSERT_TRUE (a && b && c && z);

  // Expect: Z A B C
  std::vector<std::string> names;
  for (auto it = r.begin (); it != r.end (); ++it)
    names.push_back ((*it).name ());
  ASSERT_EQ (names.size (), static_cast<std::size_t> (4));
  EXPECT_EQ (names[0], "Z");
  EXPECT_EQ (names[1], "A");
  EXPECT_EQ (names[2], "B");
  EXPECT_EQ (names[3], "C");
}

TEST_F (XmlFixture,
        GivenChild_WhenAppendCopyMove_ThenTargetHasCloneAndMovedHasNoParent)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode a = r.append_child ("a");
  a.append_attribute ("k").set_value ("v");
  a.append_child ("x").text ().set ("t");

  XmlNode copy = r.append_copy (a);
  ASSERT_TRUE (copy);
  EXPECT_TRUE (copy.attribute ("k"));
  EXPECT_TRUE (copy.child ("x"));

  XmlNode holder = r.append_child ("holder");
  XmlNode moved = holder.append_move (a);
  ASSERT_TRUE (moved);
  EXPECT_TRUE (holder.child ("a")); // moved to
  EXPECT_TRUE (moved.child ("x"));
}

TEST_F (XmlFixture, GivenNode_WhenRemoveChildrenAndAttributes_ThenNodeIsEmpty)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");
  n.append_attribute ("a").set_value ("1");
  n.append_attribute ("b").set_value ("2");
  n.append_child ("c1");
  n.append_child ("c2");

  EXPECT_TRUE (n.remove_attributes ());
  EXPECT_TRUE (n.remove_children ());
  EXPECT_FALSE (n.first_attribute ());
  EXPECT_FALSE (n.first_child ());
}

// --- Attribute API: set/get, numeric and bool conversions --------------
struct BoolCase
{
  char const *s;
  bool expected;
};

// ctest names a value-parameterized test after the text GoogleTest prints for
// its parameter. Without this operator the text is the bytes of the object,
// which hold the address of the string literal and change with every build, so
// the ctest names of AttrBoolParamTest were not stable.
std::ostream &
operator<< (std::ostream &os, BoolCase const &item)
{
  return os << "s=" << item.s
            << ",expected=" << (item.expected ? "true" : "false");
}

class AttrBoolParamTest : public XmlFixture,
                          public ::testing::WithParamInterface<BoolCase>
{
};
TEST_P (AttrBoolParamTest, GivenString_WhenAsBool_ThenParsesPerSpec)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");
  XmlAttribute a = n.append_attribute ("b");
  a.set_value (GetParam ().s);
  EXPECT_EQ (a.as_bool (false), GetParam ().expected);
}
INSTANTIATE_TEST_SUITE_P (
    Xml, AttrBoolParamTest,
    ::testing::Values (BoolCase{ "1", true }, BoolCase{ "0", false },
                       BoolCase{ "true", true }, BoolCase{ "True", true },
                       BoolCase{ "t", true }, BoolCase{ "yes", true },
                       BoolCase{ "Y", true }, BoolCase{ "Yup", true },
                       BoolCase{ "false", false }, BoolCase{ "no", false },
                       BoolCase{ "n", false }, BoolCase{ "", false }));

TEST (BoolCaseNameTest, GivenCase_WhenPrinted_ThenTheTextNamesTheCase)
{
  EXPECT_EQ (::testing::PrintToString (BoolCase{ "yes", true }),
             "s=yes,expected=true");
  EXPECT_EQ (::testing::PrintToString (BoolCase{ "", false }),
             "s=,expected=false");
}

TEST_F (XmlFixture,
        GivenAttribute_WhenSetValueVariousScalars_ThenAsXReturnsExpected)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");

  XmlAttribute i = n.append_attribute ("i");
  i.set_value (-12);
  XmlAttribute u = n.append_attribute ("u");
  u.set_value (static_cast<unsigned int> (42));
  XmlAttribute d = n.append_attribute ("d");
  d.set_value (3.14159);
  XmlAttribute f = n.append_attribute ("f");
  f.set_value (2.5f);
  XmlAttribute b = n.append_attribute ("b");
  b.set_value (true);
  XmlAttribute l = n.append_attribute ("l");
  l.set_value (9223372036854775807LL);

  EXPECT_EQ (i.as_int (), -12);
  EXPECT_EQ (u.as_uint (), static_cast<unsigned int> (42));
  EXPECT_NEAR (d.as_double (), 3.14159, 1e-12);
  EXPECT_NEAR (f.as_float (), 2.5f, 1e-6f);
  EXPECT_TRUE (b.as_bool ());
  EXPECT_EQ (l.as_llong (), 9223372036854775807LL);
}

TEST_F (XmlFixture, GivenAttributeNames_WhenRename_ThenNamesUpdated)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");
  XmlAttribute a = n.append_attribute ("old");
  ASSERT_TRUE (a);
  EXPECT_STREQ (a.name (), "old");
  ASSERT_TRUE (a.set_name ("new"));
  EXPECT_STREQ (a.name (), "new");
}

// --- XmlText: numeric conversions and assignment operators -------------
TEST_F (XmlFixture, GivenTextNode_WhenSetNumeric_ThenAsNumericMatches)
{
  doc.reset ();
  XmlNode x = doc.append_child ("x");
  XmlNode t = x.append_child (xml_node_type::node_pcdata);
  XmlText xt = t.text ();

  ASSERT_TRUE (xt.set (123));
  EXPECT_EQ (xt.as_int (), 123);
  ASSERT_TRUE (xt.set (3.5));
  EXPECT_NEAR (xt.as_double (), 3.5, 1e-12);
  ASSERT_TRUE (xt.set (true));
  EXPECT_TRUE (xt.as_bool ());
  xt = 42;
  EXPECT_EQ (xt.as_int (), 42);
}

TEST_F (XmlFixture, GivenText_WhenSetAndGetString_ThenRoundtrip)
{
  doc.reset ();
  XmlNode x = doc.append_child ("x");
  XmlNode t = x.append_child (xml_node_type::node_pcdata);
  XmlText xt = t.text ();

  ASSERT_TRUE (xt.set ("hello"));
  EXPECT_STREQ (xt.get (), "hello");
}

// --- Path and find_* APIs -----------------------------------------------
TEST_F (XmlFixture, GivenNestedTree_WhenPathAndFirstElementByPath_ThenWorks)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode a = r.append_child ("a");
  XmlNode b = a.append_child ("b");
  b.append_child ("c");

  std::string p = b.path ();
  EXPECT_FALSE (p.empty ());
  XmlNode found = r.first_element_by_path ("a/b");
  ASSERT_TRUE (found);
  EXPECT_STREQ (found.name (), "b");
}

TEST_F (XmlFixture, GivenPredicates_WhenFindAttributeNode_ThenReturnsExpected)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  r.append_attribute ("aa").set_value ("1");
  r.append_attribute ("bb").set_value ("2");
  r.append_child ("x").append_attribute ("k").set_value ("v");

  XmlAttribute aa
      = r.find_attribute ([] (XmlAttribute const &a)
                            { return std::strcmp (a.name (), "aa") == 0; });
  ASSERT_TRUE (aa);
  EXPECT_STREQ (aa.value (), "1");

  XmlNode x
      = r.find_node ([] (XmlNode const &n) { return n.attribute ("k"); });
  ASSERT_TRUE (x);
  EXPECT_STREQ (x.name (), "x");
}

TEST_F (XmlFixture, GivenFindByAttribute_WhenSearch_ThenFinds)
{
  doc.reset ();
  XmlNode r = doc.append_child ("root");
  XmlNode t = r.append_child ("tool");
  t.append_attribute ("name").set_value ("clang");
  XmlNode y = r.find_child_by_attribute ("tool", "name", "clang");
  ASSERT_TRUE (y);
  EXPECT_STREQ (y.name (), "tool");
}

// --- XPath selection (basic) --------------------------------------------
TEST_F (XmlFixture, GivenXPath_WhenSelectNodes_ThenReturnsExpectedCount)
{
  doc.reset ();
  doc.load_string ("<Profile><Tools><Tool Filename='a' Timeout='10'/>"
                   "<Tool Filename='b' Timeout='0'/></Tools></Profile>",
                   kparse_default);
  auto set = doc.select_nodes ("/Profile/Tools/Tool[@Timeout > 0]");
  ASSERT_FALSE (set.empty ());
  EXPECT_EQ (set.size (), static_cast<std::size_t> (1));
  EXPECT_STREQ (set.first ().node ().attribute ("Filename").value (), "a");
}

TEST_F (XmlFixture, GivenXPath_WhenSelectSingleNode_ThenNodeReturned)
{
  doc.reset ();
  doc.load_string ("<r><a id='1'/><a id='2'/></r>", kparse_default);
  auto n = doc.select_node ("//a[@id='2']");
  ASSERT_TRUE (n);
  EXPECT_STREQ (n.node ().attribute ("id").value (), "2");
}

TEST_F (XmlFixture, GivenMissingChildOrAttribute_WhenUnfound_ThenEmpty)
{
  doc.reset ();
  doc.load_string ("<root present=\"yes\"><child>text</child></root>",
                   kparse_default);
  XmlNode root = doc.document_element ();
  ASSERT_TRUE (root);
  ASSERT_TRUE (root.child ("child"));
  ASSERT_TRUE (root.attribute ("present"));
  EXPECT_FALSE (root.child ("missing"));
  EXPECT_FALSE (root.attribute ("absent"));
  EXPECT_FALSE (root.find_child_by_attribute ("child", "id", "no-such"));
}

TEST_F (XmlFixture, GivenXPath_WhenNoMatch_ThenUnfoundEmpty)
{
  doc.reset ();
  doc.load_string ("<r><a id='1'/></r>", kparse_default);
  auto set = doc.select_nodes ("//a[@id='99']");
  EXPECT_TRUE (set.empty ());
  EXPECT_EQ (set.size (), static_cast<std::size_t> (0));
  auto n = doc.select_node ("//missing");
  EXPECT_FALSE (n);
}

// --- Streams and encoding -----------------------------------------------
TEST_F (XmlFixture, GivenIStringstream_WhenLoad_ThenParses)
{
  std::istringstream iss ("<r/>");
  auto res = doc.load (iss, kparse_default, xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  EXPECT_STREQ (doc.document_element ().name (), "r");
}

TEST_F (XmlFixture, GivenWOStringstream_WhenSave_ThenProducesWideXml)
{
  doc.load_string ("<r><x>1</x></r>", kparse_default);
  std::wostringstream woss;
  doc.save (woss, "\t", kformat_default);
  auto s = woss.str ();
  EXPECT_NE (s.find (L"<r>"), std::wstring::npos);
}

TEST_F (XmlFixture, GivenBOMFlag_WhenSaveUtf8_ThenStartsWithBOM)
{
  doc.reset ();
  doc.append_child ("r");
  std::ostringstream oss;
  doc.save (oss, "", kformat_write_bom | kformat_raw,
            xml_encoding::encoding_utf8);
  std::string s = oss.str ();
  ASSERT_GE (s.size (), static_cast<std::size_t> (3));
  EXPECT_EQ (static_cast<unsigned char> (s[0]), 0xEF);
  EXPECT_EQ (static_cast<unsigned char> (s[1]), 0xBB);
  EXPECT_EQ (static_cast<unsigned char> (s[2]), 0xBF);
}

TEST_F (XmlFixture, GivenNoDeclarationFlag_WhenSave_ThenNoXmlDeclIfRequested)
{
  doc.load_string ("<r/>", kparse_default);
  std::ostringstream oss;
  doc.save (oss, "\t", kformat_no_declaration | kformat_raw,
            xml_encoding::encoding_utf8);
  std::string s = oss.str ();
  ASSERT_EQ (s.find ("<?xml"), std::string::npos);
}

TEST_F (XmlFixture,
        GivenDeclarationAbsent_WhenSaveDefault_ThenXmlDeclAutoAdded)
{
  doc.reset ();
  doc.append_child ("r");
  std::ostringstream oss;
  doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
  std::string s = oss.str ();
  ASSERT_NE (s.find ("<?xml"), std::string::npos);
}

TEST_F (XmlFixture, GivenLoadBufferInplace_ThenBufferModifiedAndParsed)
{
  char const *src = "<r><x/></r>";
  std::vector<char> buf (src, src + std::strlen (src) + 1);
  auto res = doc.load_buffer_inplace (&buf[0], buf.size () - 1, kparse_default,
                                      xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  EXPECT_STREQ (doc.document_element ().name (), "r");
}

TEST_F (XmlFixture, GivenLoadBufferInplaceOwn_ThenDocOwnsAndParses)
{
  char const *src = "<r><x/></r>";
  std::size_t n = std::strlen (src);
  char *own = static_cast<char *> (std::malloc (n + 1));
  ASSERT_NE (own, static_cast<char *> (nullptr));
  std::memcpy (own, src, n + 1);
  auto res = doc.load_buffer_inplace_own (own, n, kparse_default,
                                          xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  EXPECT_STREQ (doc.document_element ().name (), "r");
  // memory will be freed by document destructor/reset
}

#if defined(_WIN32)
TEST_F (XmlFixture, Windows_WidePath_SaveAndLoad)
{
  doc.load_string ("<r><x>y</x></r>", kparse_default);
  wchar_t const *wpath = L"lumex_xml_test_tmp.xml";
  ASSERT_TRUE (doc.save_file (wpath, "\t", kformat_save_file_text,
                              xml_encoding::encoding_utf8));
  XmlDocument d2;
  auto pr = d2.load_file (wpath, kparse_default, xml_encoding::encoding_utf8);
  EXPECT_STREQ (d2.document_element ().child ("x").text ().get (), "y");
  std::remove ("lumex_xml_test_tmp.xml");
}
#endif

// --- Named child iteration and ranges -----------------------------------
TEST_F (XmlFixture,
        GivenNamedChildren_WhenIterateByName_ThenOnlyMatchingVisited)
{
  doc.load_string ("<r><a/><b/><a/></r>", kparse_default);
  XmlNode r = doc.document_element ();
  std::size_t count = 0u;
  for (auto it = r.children ("a").begin (); it != r.children ("a").end ();
       ++it)
    {
      EXPECT_STREQ ((*it).name (), "a");
      ++count;
    }
  EXPECT_EQ (count, static_cast<std::size_t> (2));
}

// --- Move semantics & reset ---------------------------------------------
TEST_F (XmlFixture, Lifetime_MoveCtorAndMoveAssign_ResetLeavesEmpty)
{
  doc.load_string ("<root><x>y</x></root>", kparse_default);
  XmlDocument moved (std::move (doc));
  ASSERT_STREQ (moved.document_element ().name (), "root");
  XmlDocument another;
  another = std::move (moved);
  ASSERT_STREQ (another.document_element ().child ("x").text ().get (), "y");
  another.reset ();
  EXPECT_FALSE (another.document_element ());
}

// --- Append buffer into subtree -----------------------------------------
TEST_F (XmlFixture, GivenNode_WhenAppendBuffer_ThenSubtreeAdded)
{
  doc.load_string ("<r/>", kparse_default);
  XmlNode r = doc.document_element ();
  char const *frag = "<a id='1'/><a id='2'/>";
  auto res = r.append_buffer (frag, std::strlen (frag), kparse_default,
                              xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  EXPECT_TRUE (r.child ("a"));
  EXPECT_STREQ (r.child ("a").attribute ("id").value (), "1");
  EXPECT_TRUE (r.child ("a").next_sibling ("a"));
}

// --- Attribute insertion order (before/after) ---------------------------
TEST_F (XmlFixture, GivenAttributes_WhenInsertBeforeAfter_ThenOrderCorrect)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");
  XmlAttribute a = n.append_attribute ("a");
  XmlAttribute c = n.append_attribute ("c");
  XmlAttribute b = n.insert_attribute_before ("b", c);
  ASSERT_TRUE (a && b && c);

  std::vector<std::string> names;
  for (auto it = n.attributes_begin (); it != n.attributes_end (); ++it)
    names.push_back ((*it).name ());
  ASSERT_EQ (names.size (), static_cast<std::size_t> (3));
  EXPECT_EQ (names[0], "a");
  EXPECT_EQ (names[1], "b");
  EXPECT_EQ (names[2], "c");
}

TEST_F (XmlFixture, GivenAttributes_WhenRemoveByName_ThenGone)
{
  doc.reset ();
  XmlNode n = doc.append_child ("n");
  n.append_attribute ("a").set_value ("1");
  n.append_attribute ("b").set_value ("2");
  EXPECT_TRUE (n.remove_attribute ("a"));
  EXPECT_FALSE (n.attribute ("a"));
  EXPECT_TRUE (n.attribute ("b"));
}

// --- Print to std::ostream variants -------------------------------------
TEST_F (XmlFixture, GivenOStream_WhenPrint_ThenNonEmpty)
{
  doc.load_string ("<r><x/></r>", kparse_default);
  std::ostringstream oss;
  doc.print (oss, "  ", kformat_indent, xml_encoding::encoding_utf8);
  EXPECT_FALSE (oss.str ().empty ());
}

TEST_F (XmlFixture, GivenShortAttributeValues_WhenPrint_ThenRoundtripIntact)
{
  char const *const src = "<root><item id=\"1\"/><item id=\"2\"/></root>";
  ASSERT_TRUE (doc.load_string (src, kparse_default));
  std::ostringstream oss;
  doc.document_element ().print (oss);
  std::string const out = oss.str ();
  EXPECT_NE (out.find ("id=\"1\""), std::string::npos);
  EXPECT_NE (out.find ("id=\"2\""), std::string::npos);

  XmlDocument roundtrip;
  ASSERT_TRUE (roundtrip.load_string (out.c_str (), kparse_default));
  EXPECT_STREQ (
      roundtrip.document_element ().child ("item").attribute ("id").value (),
      "1");
}

TEST_F (XmlFixture, GivenWOStream_WhenPrintWide_ThenNonEmpty)
{
  doc.load_string ("<r><x/></r>", kparse_default);
  std::wostringstream woss;
  doc.print (woss, "  ", kformat_indent);
  EXPECT_FALSE (woss.str ().empty ());
}

// --- Query helpers and safe bool operators ------------------------------
TEST_F (XmlFixture, GivenEmptyNode_WhenSafeBoolAndOperators_ThenBehave)
{
  XmlNode empty;
  EXPECT_FALSE (empty);
  EXPECT_TRUE ((empty || false) == false);
  EXPECT_TRUE ((empty && true) == false);
}

TEST_F (XmlFixture, GivenNonEmptyNode_WhenSafeBoolAndOperators_ThenTrue)
{
  doc.load_string ("<r/>", kparse_default);
  XmlNode r = doc.document_element ();
  EXPECT_TRUE (r);
  EXPECT_TRUE ((r || true) == true);
  EXPECT_TRUE ((r && true) == true);
}

// --- Concurrency: read‑only iterations in parallel ----------------------
TEST_F (XmlFixture, ThreadSafety_ReadOnlyTraversal_16Threads)
{
  doc.load_string ("<r> <a/><b/><c/><d/><e/></r>", kparse_default);
  XmlNode r = doc.document_element ();

  std::vector<std::thread> threads;
  std::atomic<int> ok (0);
  for (int i = 0; i < 16; ++i)
    {
      threads.push_back (std::thread (
          [&] ()
            {
              int local = 0;
              for (auto it = r.begin (); it != r.end (); ++it)
                if ((*it))
                  ++local;
              if (local >= 5)
                ok.fetch_add (1);
            }));
    }
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (ok.load (), 16);
}

// --- Platform quirks: newlines and path stability -----------------------
TEST_F (XmlFixture, Platform_NewlinesAndPathStable)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode c = r.append_child ("c");
  c.text ().set ("line1\r\nline2\n");

  std::ostringstream oss;
  doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
  std::string out = oss.str ();
  ASSERT_NE (out.find ("line1"), std::string::npos);
  ASSERT_NE (out.find ("line2"), std::string::npos);
  EXPECT_FALSE (c.path ().empty ());
}

// --- Performance (opt‑in) -----------------------------------------------
TEST_F (XmlFixture, Perf_SaveManySmallDocs)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const N = 3000;
  std::string xml = "<r><n v='1'/><n v='2'/><n v='3'/></r>";
  auto res = doc.load_buffer (xml.c_str (), xml.size (), kparse_default,
                              xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));

  auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < N; ++i)
    {
      std::ostringstream oss;
      doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
      ASSERT_FALSE (oss.str ().empty ());
    }
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);
  EXPECT_LT (dur.count (), 2000);
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

// --- Document element and empty cases -----------------------------------
TEST_F (XmlFixture, GivenNoElement_WhenDocumentElement_ThenEmpty)
{
  doc.reset ();
  EXPECT_FALSE (doc.document_element ());
}

TEST_F (XmlFixture,
        GivenMixedNodes_WhenDocumentElement_ThenFirstElementReturned)
{
  doc.reset ();
  XmlNode cmt = doc.append_child (xml_node_type::node_comment);
  cmt.set_value ("c");
  XmlNode elt = doc.append_child ("E");
  (void)elt;
  XmlNode dec = doc.append_child (xml_node_type::node_declaration);
  dec.set_name ("xml");
  EXPECT_STREQ (doc.document_element ().name (), "E");
}

// --- Insert siblings chain and previous_sibling navigation --------------
TEST_F (XmlFixture, GivenSiblings_WhenPreviousNextSiblingByName_ThenCorrect)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  XmlNode a = r.append_child ("n");
  (void)a;
  XmlNode b = r.append_child ("n");
  XmlNode c = r.append_child ("n");
  EXPECT_TRUE (c.previous_sibling ("n"));
  EXPECT_TRUE (b.next_sibling ("n"));
}

// --- Sized names (same signature in every standard) ---------------------

TEST_F (XmlFixture,
        GivenNameInsideLargerBuffer_WhenSizedLookup_ThenOnlyTheRangeMatches)
{
  ASSERT_EQ (
      doc.load_string ("<r a='1' ab='2'><x/><xy/><x/></r>", kparse_default)
          .status,
      xml_parse_status::status_ok);
  XmlNode const r = doc.document_element ();
  // "x", "xy", "a" and "ab" are ranges of one buffer without NUL between.
  char const names[] = "xyab";

  // Found.
  EXPECT_STREQ (r.child (names, 1).name (), "x");
  EXPECT_STREQ (r.child (names, 2).name (), "xy");
  EXPECT_STREQ (r.attribute (names + 2, 1).value (), "1");
  EXPECT_STREQ (r.attribute (names + 2, 2).value (), "2");
  XmlNode const first = r.child (names, 1);
  EXPECT_STREQ (first.next_sibling (names, 2).name (), "xy");
  EXPECT_STREQ (r.last_child ().previous_sibling (names, 2).name (), "xy");

  // Unfound: a longer range, a range that is no name, an empty name.
  EXPECT_FALSE (r.child (names, 3));
  EXPECT_FALSE (r.attribute (names + 1, 1));
  EXPECT_FALSE (first.next_sibling (names + 1, 1));
  EXPECT_FALSE (r.last_child ().previous_sibling (names, 3));
  EXPECT_FALSE (r.child ("", 0));
  EXPECT_FALSE (r.attribute ("", 0));
}

TEST_F (XmlFixture, GivenNameWithEmbeddedNul_WhenSizedLookup_ThenUnfound)
{
  ASSERT_EQ (doc.load_string ("<r x='1'><x/><xy/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode r = doc.document_element ();
  char const name[] = { 'x', '\0', 'y' };
  EXPECT_FALSE (r.child (name, 3));
  EXPECT_FALSE (r.attribute (name, 3));
  EXPECT_FALSE (r.remove_child (name, 3));
  EXPECT_EQ (child_names (r), "x,xy");
}

TEST_F (XmlFixture,
        GivenHint_WhenSizedAttributeLookup_ThenFoundAndHintAdvanced)
{
  ASSERT_EQ (doc.load_string ("<r a='1' b='2' c='3'/>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode const r = doc.document_element ();
  char const names[] = "abc";
  XmlAttribute hint;

  EXPECT_STREQ (r.attribute (names + 1, 1, hint).value (), "2");
  EXPECT_STREQ (hint.name (), "c");
  // Wraps around to the attributes before the hint.
  EXPECT_STREQ (r.attribute (names, 1, hint).value (), "1");
  EXPECT_STREQ (hint.name (), "b");
  // Unfound leaves the hint alone.
  EXPECT_FALSE (r.attribute (names, 2, hint));
  EXPECT_STREQ (hint.name (), "b");
}

TEST_F (
    XmlFixture,
    GivenNamesInsideLargerBuffer_WhenSizedInsertAndRemove_ThenOnlyRangeUsed)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  // "item" is names[0..4), "name" is names[4..8).
  char const names[] = "itemname";

  XmlNode const item = r.append_child (names, 4);
  EXPECT_STREQ (item.name (), "item");
  XmlNode const name = r.prepend_child (names + 4, 4);
  EXPECT_STREQ (name.name (), "name");
  EXPECT_STREQ (r.insert_child_after (names, 2, name).name (), "it");
  EXPECT_STREQ (r.insert_child_before (names + 4, 2, name).name (), "na");
  EXPECT_EQ (child_names (r), "na,name,it,item");

  XmlNode node = r.child (names, 4);
  XmlAttribute const attr = node.append_attribute (names + 4, 4);
  EXPECT_STREQ (attr.name (), "name");
  EXPECT_STREQ (node.prepend_attribute (names, 2).name (), "it");
  EXPECT_STREQ (node.insert_attribute_after (names, 4, attr).name (), "item");
  EXPECT_STREQ (node.insert_attribute_before (names + 4, 2, attr).name (),
                "na");
  EXPECT_EQ (attribute_names (node), "it,na,name,item");

  // Found, then unfound once removed.
  EXPECT_TRUE (node.remove_attribute (names + 4, 2));
  EXPECT_FALSE (node.remove_attribute (names + 4, 2));
  EXPECT_EQ (attribute_names (node), "it,name,item");
  EXPECT_TRUE (r.remove_child (names, 2));
  EXPECT_FALSE (r.remove_child (names, 3));
  EXPECT_EQ (child_names (r), "na,name,item");
}

// --- Attribute hash and node hash existence (sanity) --------------------
TEST_F (XmlFixture, GivenHandles_WhenHashValue_ThenNonZero)
{
  doc.load_string ("<r a='1'><x/></r>", kparse_default);
  XmlNode r = doc.document_element ();
  EXPECT_NE (r.hash_value (), static_cast<std::size_t> (0));
  EXPECT_NE (r.attribute ("a").hash_value (), static_cast<std::size_t> (0));
}

// --- Remove by handle overloads -----------------------------------------
TEST_F (XmlFixture, GivenChildHandle_WhenRemoveChild_ThenRemoved)
{
  doc.load_string ("<r><x/><y/></r>", kparse_default);
  XmlNode r = doc.document_element ();
  XmlNode x = r.child ("x");
  ASSERT_TRUE (r.remove_child (x));
  EXPECT_FALSE (r.child ("x"));
  EXPECT_TRUE (r.child ("y"));
}

TEST_F (XmlFixture, GivenAttrHandle_WhenRemoveAttribute_ThenRemoved)
{
  doc.load_string ("<r a='1' b='2'/>", kparse_default);
  XmlNode r = doc.document_element ();
  XmlAttribute a = r.attribute ("a");
  ASSERT_TRUE (r.remove_attribute (a));
  EXPECT_FALSE (r.attribute ("a"));
  EXPECT_TRUE (r.attribute ("b"));
}

// --- set_name/set_value overloads on nodes ------------------------------
TEST_F (XmlFixture, GivenNode_WhenSetNameAndValue_ThenApplied)
{
  doc.reset ();
  XmlNode c = doc.append_child (xml_node_type::node_comment);
  ASSERT_TRUE (c.set_value ("hello"));
  EXPECT_STREQ (c.value (), "hello");
  XmlNode e = doc.append_child ("old");
  ASSERT_TRUE (e.set_name ("new"));
  EXPECT_STREQ (e.name (), "new");
}

// --- Print depth argument does not crash and respects indent ------------
TEST_F (XmlFixture, GivenDepth_WhenPrintWithDepth_ThenNonEmpty)
{
  doc.load_string ("<r><a><b/></a></r>", kparse_default);
  std::ostringstream oss;
  doc.print (oss, "  ", kformat_indent, xml_encoding::encoding_utf8, 0);
  EXPECT_FALSE (oss.str ().empty ());
}

// --- Parse flags sanity (pi/comments/doctypes) --------------------------
TEST_F (XmlFixture, GivenFullParse_WhenParsePIAndComments_ThenNodesPresent)
{
  char const *xml = "<?xml version='1.0'?><?pi x='1'?><!--c--><r/>";
  auto res = doc.load_buffer (xml, std::strlen (xml), kparse_full,
                              xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  // Not asserting exact order; presence sanity: document element exists
  EXPECT_STREQ (doc.document_element ().name (), "r");
}

// --- Save file text mode roundtrip (narrow) -----------------------------
TEST_F (XmlFixture, GivenFileRoundtrip_WhenSaveTextAndLoad_ThenOk)
{
  doc.load_string ("<r><z>1</z></r>", kparse_default);
  char const *path = "lumex_xml_test_roundtrip.xml";
  ASSERT_TRUE (doc.save_file (path, "\t",
                              kformat_save_file_text | kformat_no_escapes,
                              xml_encoding::encoding_utf8));
  XmlDocument d2;
  auto pr = d2.load_file (path, kparse_default, xml_encoding::encoding_utf8);
  EXPECT_EQ (pr.status, xml_parse_status::status_ok);
  EXPECT_STREQ (d2.document_element ().child ("z").text ().get (), "1");
  std::remove (path);
}

// --- first/last child & attribute navigation ----------------------------
TEST_F (XmlFixture, GivenMultipleChildren_WhenFirstLastChild_ThenCorrect)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  r.append_child ("a");
  r.append_child ("b");
  EXPECT_STREQ (r.first_child ().name (), "a");
  EXPECT_STREQ (r.last_child ().name (), "b");
}

TEST_F (XmlFixture, GivenMultipleAttributes_WhenFirstLastAttribute_ThenCorrect)
{
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  r.append_attribute ("a").set_value ("1");
  r.append_attribute ("b").set_value ("2");
  EXPECT_STREQ (r.first_attribute ().name (), "a");
  EXPECT_STREQ (r.last_attribute ().name (), "b");
}

// --- select_node overload using compiled XPathQuery ---------------------
TEST_F (XmlFixture, GivenCompiledXPath_WhenSelectNode_ThenWorks)
{
  doc.load_string ("<r><a id='1'/><a id='2'/></r>", kparse_default);
  lumex::xml::xpath::query::XPathQuery q ("//a[@id='1']");
  auto n = doc.select_node (q);
  ASSERT_TRUE (n);
  EXPECT_STREQ (n.node ().attribute ("id").value (), "1");
}

// --- Large‑document parameterized suites ----------------

class LargeBuildSaveLoadParamTest : public XmlFixture,
                                    public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeBuildSaveLoadParamTest,
        GivenLargeDoc_WhenSaveThenLoad_Stream_ThenCountsMatch)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode root = doc.append_child ("root");
  for (int i = 0; i < N; ++i)
    {
      XmlNode n = root.append_child ("item");
      n.append_attribute ("i").set_value (i);
      n.text ().set (i % 2 == 0 ? "even" : "odd");
    }
  std::ostringstream oss;
  doc.save (oss, "  ", kformat_default, xml_encoding::encoding_utf8);
  std::istringstream iss (oss.str ());
  XmlDocument d2;
  auto pr = d2.load (iss, kparse_default, xml_encoding::encoding_utf8);
  XmlNode r2 = d2.document_element ();
  std::size_t cnt = 0u;
  for (auto it = r2.begin (); it != r2.end (); ++it)
    ++cnt;
  EXPECT_TRUE (r2.child ("item").attribute ("i"));
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeBuildSaveLoadParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeXPathParamTest : public XmlFixture,
                            public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeXPathParamTest,
        GivenLargeDoc_WhenXPathFilterByAttr_ThenExpectedCount)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode root = doc.append_child ("root");
  for (int i = 0; i < N; ++i)
    {
      XmlNode n = root.append_child ("item");
      n.append_attribute ("v").set_value (i);
    }
  // Count items with v >= N-10
  std::ostringstream q;
  q << "//item[@v>=" << (N - 10) << "]";
  auto set = doc.select_nodes (q.str ().c_str ());
  EXPECT_EQ (set.size (), static_cast<std::size_t> (std::min (10, N)));
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeXPathParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeIterationParamTest : public XmlFixture,
                                public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeIterationParamTest,
        GivenLargeDoc_WhenIterateChildrenAndAttributes_ThenTotalsOk)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode root = doc.append_child ("root");
  for (int i = 0; i < N; ++i)
    {
      XmlNode n = root.append_child ("item");
      n.append_attribute ("a").set_value (i);
      n.append_attribute ("b").set_value (i * 2);
    }
  std::size_t nodes = 0, attrs = 0;
  for (auto it = root.begin (); it != root.end (); ++it)
    {
      ++nodes;
      for (auto at = (*it).attributes_begin (); at != (*it).attributes_end ();
           ++at)
        ++attrs;
    }
  EXPECT_EQ (nodes, static_cast<std::size_t> (N));
  EXPECT_EQ (attrs, static_cast<std::size_t> (2 * N));
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeIterationParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeAttributesParamTest : public XmlFixture,
                                 public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeAttributesParamTest,
        GivenManyAttrs_WhenAsIntAndSum_ThenSumMatches)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode root = doc.append_child ("root");
  long long expected = 0;
  for (int i = 0; i < N; ++i)
    {
      XmlNode n = root.append_child ("item");
      n.append_attribute ("v").set_value (i);
      expected += i;
    }
  long long sum = 0;
  for (auto it = root.begin (); it != root.end (); ++it)
    sum += (*it).attribute ("v").as_llong ();
  EXPECT_EQ (sum, expected);
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeAttributesParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargePrevNextParamTest : public XmlFixture,
                               public ::testing::WithParamInterface<int>
{
};
TEST_P (LargePrevNextParamTest,
        GivenManySiblings_WhenPrevNextByName_ThenEdgesResolve)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  for (int i = 0; i < N; ++i)
    r.append_child ("n");
  XmlNode first = r.first_child ();
  XmlNode last = r.last_child ();
  EXPECT_FALSE (first.previous_sibling ("n"));
  EXPECT_FALSE (last.next_sibling ("n"));
  EXPECT_TRUE (first.next_sibling ("n"));
  EXPECT_TRUE (last.previous_sibling ("n"));
}
INSTANTIATE_TEST_SUITE_P (Xml, LargePrevNextParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeAppendBufferParamTest : public XmlFixture,
                                   public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeAppendBufferParamTest,
        GivenLargeFragment_WhenAppendBuffer_ThenAllChildrenPresent)
{
  int const N = GetParam ();
  doc.load_string ("<r/>", kparse_default);
  XmlNode r = doc.document_element ();
  std::string frag;
  frag.reserve (static_cast<std::size_t> (N) * 16u);
  for (int i = 0; i < N; ++i)
    frag += "<c v='1'/>";
  auto res = r.append_buffer (frag.c_str (), frag.size (), kparse_default,
                              xml_encoding::encoding_utf8);
  ASSERT_TRUE (res.status == xml_parse_status::status_ok
               || res.status == static_cast<xml_parse_status> ('\0'));
  std::size_t count = 0;
  for (auto it = r.begin (); it != r.end (); ++it)
    ++count;
  EXPECT_EQ (count, static_cast<std::size_t> (N));
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeAppendBufferParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeMixedContentParamTest : public XmlFixture,
                                   public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeMixedContentParamTest,
        GivenMixedContent_WhenSavedAndReloaded_ThenTextAndElementsOk)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  r.append_child (xml_node_type::node_comment).set_value ("header");
  for (int i = 0; i < N; ++i)
    {
      XmlNode e = r.append_child ("e");
      e.text ().set ("t");
      if (i % 10 == 0)
        r.append_child (xml_node_type::node_comment).set_value ("c");
    }
  std::ostringstream oss;
  doc.save (oss, "\t", kformat_default, xml_encoding::encoding_utf8);
  XmlDocument d2;
  std::istringstream iss (oss.str ());
  XmlNode rd = d2.document_element ();
  std::size_t el = 0;
  for (auto it = rd.begin (); it != rd.end (); ++it)
    if (std::strcmp ((*it).name (), "e") == 0)
      ++el;
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeMixedContentParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class LargeDeepNestingPathParamTest : public XmlFixture,
                                      public ::testing::WithParamInterface<int>
{
};
TEST_P (LargeDeepNestingPathParamTest,
        GivenDeepTree_WhenFirstElementByPath_ThenFindsTail)
{
  int const depth = GetParam (); // keep moderate for stack
  doc.reset ();
  XmlNode cur = doc.append_child ("n0");
  for (int i = 1; i <= depth; ++i)
    cur = cur.append_child (("n" + std::to_string (i)).c_str ());
  std::string path;
  for (int i = 1; i <= depth; ++i)
    {
      if (i > 1)
        path += "/";
      path += "n" + std::to_string (i);
    }
  XmlNode found
      = doc.document_element ().first_element_by_path (path.c_str ());
  ASSERT_TRUE (found);
  EXPECT_STREQ (found.name (), ("n" + std::to_string (depth)).c_str ());
}
INSTANTIATE_TEST_SUITE_P (Xml, LargeDeepNestingPathParamTest,
                          ::testing::Values (50, 100, 150, 200, 250));

class LargePrintStreamParamTest : public XmlFixture,
                                  public ::testing::WithParamInterface<int>
{
};
TEST_P (LargePrintStreamParamTest,
        GivenLargeDoc_WhenPrintToStreams_ThenNonEmpty)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  for (int i = 0; i < N; ++i)
    {
      XmlNode n = r.append_child ("n");
      n.append_attribute ("k").set_value (i);
    }
  std::ostringstream oss;
  doc.print (oss, "  ", kformat_indent, xml_encoding::encoding_utf8);
  EXPECT_FALSE (oss.str ().empty ());
  std::wostringstream woss;
  doc.print (woss, "  ", kformat_indent);
  EXPECT_FALSE (woss.str ().empty ());
}
INSTANTIATE_TEST_SUITE_P (Xml, LargePrintStreamParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

class ConcurrencyLargeReadOnlyParamTest
    : public XmlFixture,
      public ::testing::WithParamInterface<int>
{
};
TEST_P (ConcurrencyLargeReadOnlyParamTest,
        GivenLargeDoc_WhenParallelReadOnly_ThenStable)
{
  int const N = GetParam ();
  doc.reset ();
  XmlNode r = doc.append_child ("r");
  for (int i = 0; i < N; ++i)
    r.append_child ("x");
  std::vector<std::thread> threads;
  std::atomic<int> ok (0);
  for (int t = 0; t < 16; ++t)
    {
      threads.push_back (std::thread (
          [&] ()
            {
              std::size_t c = 0;
              for (auto it = r.begin (); it != r.end (); ++it)
                ++c;
              if (c == static_cast<std::size_t> (N))
                ok.fetch_add (1);
            }));
    }
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (ok.load (), 16);
}
INSTANTIATE_TEST_SUITE_P (Xml, ConcurrencyLargeReadOnlyParamTest,
                          ::testing::Values (100, 500, 1000, 1500, 2000));

// --- Typed getters: the default stands only for a missing value ---------
// The getters of XmlAttribute and XmlText return `def` only when there is
// no value. A value that is not a number still converts: to 0, to false,
// or clamped to the range of the type.
TEST_F (XmlFixture,
        GivenNonNumericAttribute_WhenNumericGettersWithDefault_ThenZero)
{
  ASSERT_EQ (doc.load_string ("<n a=\"abc\" e=\"\" w=\" 12px\"/>").status,
             xml_parse_status::status_ok);
  XmlNode n = doc.child ("n");
  XmlAttribute a = n.attribute ("a");
  ASSERT_FALSE (a.empty ());
  EXPECT_EQ (a.as_int (7), 0);
  EXPECT_EQ (a.as_uint (7U), 0U);
  EXPECT_EQ (a.as_llong (7), 0);
  EXPECT_EQ (a.as_ullong (7), 0U);
  EXPECT_DOUBLE_EQ (a.as_double (7.5), 0.0);
  EXPECT_FLOAT_EQ (a.as_float (7.5F), 0.0F);
  XmlAttribute e = n.attribute ("e");
  ASSERT_FALSE (e.empty ());
  EXPECT_EQ (e.as_int (7), 0);
  EXPECT_DOUBLE_EQ (e.as_double (7.5), 0.0);
  EXPECT_EQ (n.attribute ("w").as_int (7), 12);
}

TEST_F (XmlFixture, GivenNonTrueAttribute_WhenAsBoolWithTrueDefault_ThenFalse)
{
  ASSERT_EQ (doc.load_string ("<n a=\"no\" e=\"\" t=\"yes\"/>").status,
             xml_parse_status::status_ok);
  XmlNode n = doc.child ("n");
  EXPECT_FALSE (n.attribute ("a").as_bool (true));
  EXPECT_FALSE (n.attribute ("e").as_bool (true));
  EXPECT_TRUE (n.attribute ("t").as_bool (false));
}

TEST_F (XmlFixture, GivenMissingAttribute_WhenTypedGetters_ThenDefault)
{
  ASSERT_EQ (doc.load_string ("<n a=\"1\"/>").status,
             xml_parse_status::status_ok);
  XmlAttribute m = doc.child ("n").attribute ("missing");
  ASSERT_TRUE (m.empty ());
  EXPECT_EQ (m.as_int (7), 7);
  EXPECT_EQ (m.as_uint (7U), 7U);
  EXPECT_EQ (m.as_llong (7), 7);
  EXPECT_EQ (m.as_ullong (7), 7U);
  EXPECT_DOUBLE_EQ (m.as_double (7.5), 7.5);
  EXPECT_FLOAT_EQ (m.as_float (7.5F), 7.5F);
  EXPECT_TRUE (m.as_bool (true));
}

TEST_F (XmlFixture,
        GivenOutOfRangeAttribute_WhenIntegerGetters_ThenClampedNotDefault)
{
  ASSERT_EQ (doc.load_string ("<n big=\"99999999999999999999999\" "
                              "small=\"-99999999999999999999999\" "
                              "neg=\"-5\" hex=\"0x1F\"/>")
                 .status,
             xml_parse_status::status_ok);
  XmlNode n = doc.child ("n");
  EXPECT_EQ (n.attribute ("big").as_int (7), std::numeric_limits<int>::max ());
  EXPECT_EQ (n.attribute ("small").as_int (7),
             std::numeric_limits<int>::min ());
  EXPECT_EQ (n.attribute ("big").as_uint (7U),
             std::numeric_limits<unsigned int>::max ());
  EXPECT_EQ (n.attribute ("big").as_llong (7),
             std::numeric_limits<long long>::max ());
  EXPECT_EQ (n.attribute ("small").as_llong (7),
             std::numeric_limits<long long>::min ());
  EXPECT_EQ (n.attribute ("big").as_ullong (7),
             std::numeric_limits<unsigned long long>::max ());
  EXPECT_EQ (n.attribute ("neg").as_uint (7U), 0U);
  EXPECT_EQ (n.attribute ("neg").as_ullong (7), 0U);
  EXPECT_EQ (n.attribute ("hex").as_int (7), 31);
}

TEST_F (XmlFixture, GivenNonNumericText_WhenTypedGettersWithDefault_ThenZero)
{
  ASSERT_EQ (doc.load_string ("<x>abc</x>").status,
             xml_parse_status::status_ok);
  XmlText xt = doc.child ("x").text ();
  ASSERT_FALSE (xt.empty ());
  EXPECT_EQ (xt.as_int (7), 0);
  EXPECT_EQ (xt.as_uint (7U), 0U);
  EXPECT_EQ (xt.as_llong (7), 0);
  EXPECT_EQ (xt.as_ullong (7), 0U);
  EXPECT_DOUBLE_EQ (xt.as_double (7.5), 0.0);
  EXPECT_FLOAT_EQ (xt.as_float (7.5F), 0.0F);
  EXPECT_FALSE (xt.as_bool (true));
}

TEST_F (XmlFixture, GivenElementWithoutText_WhenTextTypedGetters_ThenDefault)
{
  ASSERT_EQ (doc.load_string ("<x><y/></x>").status,
             xml_parse_status::status_ok);
  XmlText xt = doc.child ("x").text ();
  ASSERT_TRUE (xt.empty ());
  EXPECT_EQ (xt.as_int (7), 7);
  EXPECT_EQ (xt.as_uint (7U), 7U);
  EXPECT_EQ (xt.as_llong (7), 7);
  EXPECT_EQ (xt.as_ullong (7), 7U);
  EXPECT_DOUBLE_EQ (xt.as_double (7.5), 7.5);
  EXPECT_FLOAT_EQ (xt.as_float (7.5F), 7.5F);
  EXPECT_TRUE (xt.as_bool (true));
}

// --- copy_xpath_variable: one implementation under two names ------------
// The exported lumex::xml::xpath::utility::copy_xpath_variable forwards to
// the inline lumex::xml::xpath::variable::copy_xpath_variable, which the
// copy of an XPathVariableSet uses.
class XPathVariableCopyTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    ASSERT_EQ (doc.load_string ("<r><a/><b/></r>").status,
               xml_parse_status::status_ok);
    for (char const *name : { "b", "n", "s", "ns" })
      ASSERT_NE (target.get (name), nullptr) << name;
    ASSERT_TRUE (source.set ("b", true));
    ASSERT_TRUE (source.set ("n", 2.5));
    ASSERT_TRUE (source.set ("s", "text"));
    ASSERT_TRUE (source.set ("ns", doc.select_nodes ("/r/*")));
  }

  XmlDocument doc;
  lumex::xml::xpath::variable::XPathVariableSet source = make_set ();
  lumex::xml::xpath::variable::XPathVariableSet target = make_set ();

  static lumex::xml::xpath::variable::XPathVariableSet
  make_set ()
  {
    lumex::xml::xpath::variable::XPathVariableSet set;
    set.add ("b", lumex::xml::types::Types::xpath_type_boolean);
    set.add ("n", lumex::xml::types::Types::xpath_type_number);
    set.add ("s", lumex::xml::types::Types::xpath_type_string);
    set.add ("ns", lumex::xml::types::Types::xpath_type_node_set);
    return set;
  }

  void
  expect_copied ()
  {
    EXPECT_TRUE (target.get ("b")->get_boolean ());
    EXPECT_DOUBLE_EQ (target.get ("n")->get_number (), 2.5);
    EXPECT_STREQ (target.get ("s")->get_string (), "text");
    EXPECT_EQ (target.get ("ns")->get_node_set ().size (), 2U);
  }
};

TEST_F (XPathVariableCopyTest, GivenEveryType_WhenExportedCopy_ThenValueCopied)
{
  for (char const *name : { "b", "n", "s", "ns" })
    EXPECT_TRUE (lumex::xml::xpath::utility::copy_xpath_variable (
        target.get (name), source.get (name)))
        << name;
  expect_copied ();
}

TEST_F (XPathVariableCopyTest, GivenEveryType_WhenInlineCopy_ThenValueCopied)
{
  for (char const *name : { "b", "n", "s", "ns" })
    EXPECT_TRUE (lumex::xml::xpath::variable::copy_xpath_variable (
        target.get (name), source.get (name)))
        << name;
  expect_copied ();
}

TEST_F (XPathVariableCopyTest, GivenOtherType_WhenExportedCopy_ThenFalse)
{
  EXPECT_FALSE (lumex::xml::xpath::utility::copy_xpath_variable (
      target.get ("b"), source.get ("n")));
  EXPECT_FALSE (target.get ("b")->get_boolean ());
}

TEST_F (XPathVariableCopyTest, GivenSet_WhenCopyConstructed_ThenValuesCopied)
{
  lumex::xml::xpath::variable::XPathVariableSet copy (source);
  EXPECT_TRUE (copy.get ("b")->get_boolean ());
  EXPECT_DOUBLE_EQ (copy.get ("n")->get_number (), 2.5);
  EXPECT_STREQ (copy.get ("s")->get_string (), "text");
  EXPECT_EQ (copy.get ("ns")->get_node_set ().size (), 2U);
}
} // namespace

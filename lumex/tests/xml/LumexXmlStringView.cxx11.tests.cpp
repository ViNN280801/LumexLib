// lumex/tests/xml/LumexXmlStringView.cxx11.tests.cpp
//
// XML tests of the string_view_t overloads (every standard): inline wrappers
// over the sized (pointer and length) functions the library exports.
// string_view_t is std::basic_string_view<char_t> from C++17 and the
// lumex_string_view of lumex::string_view below it, so a C++11 program passes
// a std::string, a char const * and a sized view the way a C++17 one does.
// The C++17 and C++20 suites compile this file too (LumexXml.cxx17.tests.cpp
// holds the tests that name std::string_view).

#include <cstddef>
#include <string>
#include <type_traits>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::attribute::XmlAttribute;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::node::XmlNode;
using lumex::xml::text::XmlText;
using lumex::xml::types::Types::string_view_t;
using lumex::xml::types::Types::xml_node_type;
using lumex::xml::types::Types::xml_parse_status;
using lumex::xml::utility::stringview_equal;

#if __cplusplus >= 201703L
static_assert (std::is_same<string_view_t, std::string_view>::value,
               "from C++17 string_view_t is std::basic_string_view<char_t>");
#else
static_assert (
    std::is_same<string_view_t,
                 lumex::core::string_view::view::lumex_string_view>::value,
    "below C++17 string_view_t is the lumex_string_view");
#endif
static_assert (std::is_convertible<char const *, string_view_t>::value,
               "a char const * is accepted as a name");
static_assert (std::is_convertible<std::string const &, string_view_t>::value,
               "a std::string is accepted as a name");
static_assert (std::is_convertible<char const (&)[4], string_view_t>::value,
               "a literal is accepted as a name");

namespace
{

TEST_F (XmlFixture, GivenStdStringNames_WhenLookup_ThenSameAsThePointerForms)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/><xy/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode r = doc.document_element ();
  std::string const xy = "xy";
  std::string const x = "x";
  std::string const a = "a";
  std::string const missing = "missing";

  EXPECT_STREQ (r.child (xy).name (), "xy");
  EXPECT_EQ (r.child (xy), r.child ("xy"));
  EXPECT_EQ (r.child (xy), r.child (xy.data (), xy.size ()));
  EXPECT_STREQ (r.attribute (a).value (), "1");
  EXPECT_EQ (r.attribute (a), r.attribute ("a"));
  EXPECT_FALSE (r.child (missing));
  EXPECT_FALSE (r.attribute (missing));
  XmlNode const first = r.child (x);
  EXPECT_STREQ (first.next_sibling (xy).name (), "xy");
  EXPECT_FALSE (first.next_sibling (x));
  EXPECT_STREQ (r.last_child ().previous_sibling (x).name (), "x");
  EXPECT_FALSE (r.last_child ().previous_sibling (xy));
  XmlAttribute hint;
  EXPECT_STREQ (r.attribute (a, hint).value (), "1");
  EXPECT_FALSE (r.attribute (missing, hint));
}

TEST_F (XmlFixture, GivenStdStringNames_WhenEdit_ThenSameAsThePointerForms)
{
  XmlNode r = doc.append_child ("r");
  XmlNode const x = r.append_child (std::string ("x"));
  EXPECT_STREQ (x.name (), "x");
  XmlNode added = r.append_child (std::string ("z"));
  EXPECT_TRUE (added.set_name (std::string ("renamed")));
  EXPECT_STREQ (added.name (), "renamed");
  EXPECT_STREQ (r.prepend_child (std::string ("p")).name (), "p");
  EXPECT_STREQ (r.insert_child_after (std::string ("after"), x).name (),
                "after");
  EXPECT_STREQ (r.insert_child_before (std::string ("before"), x).name (),
                "before");
  EXPECT_EQ (child_names (r), "p,before,x,after,renamed");

  XmlAttribute attr = added.append_attribute (std::string ("key"));
  EXPECT_STREQ (attr.name (), "key");
  EXPECT_TRUE (attr.set_value (std::string ("value")));
  EXPECT_STREQ (attr.value (), "value");
  attr = std::string ("other");
  EXPECT_STREQ (attr.value (), "other");
  EXPECT_TRUE (attr.set_name (std::string ("k2")));
  EXPECT_STREQ (attr.name (), "k2");
  EXPECT_STREQ (added.prepend_attribute (std::string ("k1")).name (), "k1");
  EXPECT_STREQ (
      added.insert_attribute_after (std::string ("k3"), attr).name (), "k3");
  EXPECT_STREQ (
      added.insert_attribute_before (std::string ("k1b"), attr).name (),
      "k1b");
  EXPECT_EQ (attribute_names (added), "k1,k1b,k2,k3");

  XmlText text = added.text ();
  EXPECT_TRUE (text.set (std::string ("body")));
  EXPECT_STREQ (added.text ().get (), "body");
  text = std::string ("next");
  EXPECT_STREQ (added.text ().get (), "next");

  XmlNode pcdata = r.append_child (xml_node_type::node_pcdata);
  EXPECT_TRUE (pcdata.set_value (std::string ("abc")));
  EXPECT_STREQ (pcdata.value (), "abc");

  EXPECT_TRUE (added.remove_attribute (std::string ("k2")));
  EXPECT_FALSE (added.remove_attribute (std::string ("k2")));
  EXPECT_TRUE (r.remove_child (std::string ("renamed")));
  EXPECT_FALSE (r.child (std::string ("renamed")));
}

TEST_F (XmlFixture, GivenSizedViews_WhenLookupAndEdit_ThenOnlyTheViewIsUsed)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/><xy/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode r = doc.document_element ();
  // A name inside a longer buffer: the sized view stops where the name does.
  std::string const names = "xyab";
  string_view_t const xy (names.data (), 2);
  string_view_t const x (names.data (), 1);
  string_view_t const a (names.data () + 2, 1);
  string_view_t const xya (names.data (), 3);

  EXPECT_STREQ (r.child (xy).name (), "xy");
  EXPECT_STREQ (r.attribute (a).value (), "1");
  EXPECT_FALSE (r.child (xya));
  EXPECT_STREQ (r.child (x).next_sibling (xy).name (), "xy");
  EXPECT_STREQ (r.last_child ().previous_sibling (x).name (), "x");
  XmlAttribute hint;
  EXPECT_STREQ (r.attribute (a, hint).value (), "1");

  string_view_t const renamed ("renamed!", 7);
  XmlNode added = r.append_child (string_view_t ("zz", 1));
  EXPECT_STREQ (added.name (), "z");
  EXPECT_TRUE (added.set_name (renamed));
  EXPECT_STREQ (added.name (), "renamed");
  EXPECT_STREQ (r.prepend_child (string_view_t ("pq", 1)).name (), "p");

  XmlAttribute attr = added.append_attribute (string_view_t ("key=", 3));
  EXPECT_STREQ (attr.name (), "key");
  EXPECT_TRUE (attr.set_value (string_view_t ("value;", 5)));
  EXPECT_STREQ (attr.value (), "value");
  attr = string_view_t ("other;", 5);
  EXPECT_STREQ (attr.value (), "other");
  EXPECT_TRUE (attr.set_name (string_view_t ("k2;", 2)));
  EXPECT_STREQ (attr.name (), "k2");

  XmlText text = added.text ();
  EXPECT_TRUE (text.set (string_view_t ("body!", 4)));
  EXPECT_STREQ (added.text ().get (), "body");
  text = string_view_t ("next!", 4);
  EXPECT_STREQ (added.text ().get (), "next");

  XmlNode pcdata = r.append_child (xml_node_type::node_pcdata);
  EXPECT_TRUE (pcdata.set_value (string_view_t ("abc!", 3)));
  EXPECT_STREQ (pcdata.value (), "abc");
  EXPECT_TRUE (added.remove_attribute (string_view_t ("k2;", 2)));
  EXPECT_TRUE (r.remove_child (renamed));
}

TEST_F (XmlFixture, GivenEmbeddedZero_WhenLookup_ThenTheStdStringIsSized)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode const r = doc.document_element ();
  // The std::string is sized: "x\0y" is not "x". A char const * is
  // NUL-terminated: the literal "x\0y" is "x".
  EXPECT_FALSE (r.child (std::string ("x\0y", 3)));
  EXPECT_TRUE (r.child ("x\0y"));
  EXPECT_FALSE (r.attribute (std::string ("a\0", 2)));
  EXPECT_TRUE (r.attribute ("a\0"));
  EXPECT_FALSE (r.child (string_view_t ("x\0", 2)));
}

TEST_F (XmlFixture, GivenEmptyViews_WhenLookup_ThenUnfound)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode const r = doc.document_element ();
  string_view_t const empty;
  EXPECT_FALSE (r.child (empty));
  EXPECT_FALSE (r.attribute (empty));
  EXPECT_FALSE (r.child ("x").next_sibling (empty));
  EXPECT_FALSE (r.child ("x").previous_sibling (empty));
  EXPECT_FALSE (r.child (std::string ()));
  EXPECT_FALSE (r.child (string_view_t ("x", 0)));
  XmlAttribute hint;
  EXPECT_FALSE (r.attribute (empty, hint));
}

TEST_F (XmlFixture,
        GivenOverloads_WhenCallWithPointerSizeAndView_ThenNoAmbiguity)
{
  XmlNode r = doc.append_child ("r");
  std::string const name = "n";
  // Each argument kind picks its own overload; none of the calls is
  // ambiguous and all give the same node.
  XmlNode const by_pointer = r.append_child ("n");
  EXPECT_EQ (r.child ("n"), by_pointer);
  EXPECT_EQ (r.child (name), by_pointer);
  EXPECT_EQ (r.child (name.c_str ()), by_pointer);
  EXPECT_EQ (r.child (name.data (), name.size ()), by_pointer);
  EXPECT_EQ (r.child (string_view_t (name.data (), name.size ())), by_pointer);
  // The node type overload stays the better match for an enumerator.
  XmlNode const text_node = r.append_child (xml_node_type::node_pcdata);
  EXPECT_EQ (text_node.type (), xml_node_type::node_pcdata);
  // The value assignments keep their meaning: a literal is text, a number is
  // a number, a bool is a bool.
  XmlAttribute attr = r.append_attribute ("a");
  attr = "text";
  EXPECT_STREQ (attr.value (), "text");
  attr = 42;
  EXPECT_STREQ (attr.value (), "42");
  attr = true;
  EXPECT_STREQ (attr.value (), "true");
  attr = std::string ("string");
  EXPECT_STREQ (attr.value (), "string");
  XmlText text = r.text ();
  text = "text";
  EXPECT_STREQ (r.text ().get (), "text");
  text = 7;
  EXPECT_STREQ (r.text ().get (), "7");
  text = std::string ("string");
  EXPECT_STREQ (r.text ().get (), "string");
}

TEST (XmlStringViewEqual, GivenViewAndText_WhenCompare_ThenTheWholeViewMatches)
{
  EXPECT_TRUE (stringview_equal ("abc", "abc"));
  EXPECT_TRUE (stringview_equal (std::string ("abc"), "abc"));
  EXPECT_TRUE (stringview_equal (string_view_t ("abcd", 3), "abc"));
  EXPECT_FALSE (stringview_equal (string_view_t ("abcd", 4), "abc"));
  EXPECT_FALSE (stringview_equal (string_view_t ("abc", 2), "abc"));
  EXPECT_FALSE (stringview_equal (string_view_t ("abc", 3), "abd"));
  EXPECT_TRUE (stringview_equal (string_view_t (), ""));
  EXPECT_TRUE (stringview_equal (string_view_t ("abc", 0), ""));
  EXPECT_FALSE (stringview_equal (string_view_t (), "a"));
  // A NUL inside the view never matches the end of the text.
  EXPECT_FALSE (stringview_equal (std::string ("ab\0", 3), "ab"));
  EXPECT_FALSE (stringview_equal (std::string ("a\0b", 3), "a"));
}
} // namespace

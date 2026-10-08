/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// lumex/tests/xml/LumexXml.cxx17.tests.cpp
//
// XML tests of the std::string_view overloads (C++17): inline wrappers over
// the sized (pointer and length) functions the library exports. The C++17
// and C++20 suites compile this file together with LumexXml.cxx11.tests.cpp
// and LumexXmlGlobalNames.cxx11.tests.cpp.

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"
#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::attribute::XmlAttribute;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::node::XmlNode;
using lumex::xml::text::XmlText;
using lumex::xml::types::Types::xml_node_type;
using lumex::xml::types::Types::xml_parse_status;

namespace
{

TEST_F (XmlFixture,
        GivenStringViewNames_WhenLookupAndEdit_ThenWrappersUseTheViewOnly)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/><xy/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode r = doc.document_element ();
  std::string_view const names = "xyab";

  EXPECT_STREQ (r.child (names.substr (0, 2)).name (), "xy");
  EXPECT_STREQ (r.attribute (names.substr (2, 1)).value (), "1");
  EXPECT_FALSE (r.child (names.substr (0, 3)));
  XmlNode const x = r.child (names.substr (0, 1));
  EXPECT_STREQ (x.next_sibling (names.substr (0, 2)).name (), "xy");
  EXPECT_STREQ (r.last_child ().previous_sibling (names.substr (0, 1)).name (),
                "x");
  XmlAttribute hint;
  EXPECT_STREQ (r.attribute (names.substr (2, 1), hint).value (), "1");

  XmlNode added = r.append_child (std::string_view ("zz").substr (0, 1));
  EXPECT_STREQ (added.name (), "z");
  EXPECT_TRUE (added.set_name (std::string_view ("renamed!").substr (0, 7)));
  EXPECT_STREQ (added.name (), "renamed");
  EXPECT_STREQ (r.prepend_child (std::string_view ("p")).name (), "p");
  EXPECT_STREQ (r.insert_child_after (std::string_view ("after"), x).name (),
                "after");
  EXPECT_STREQ (r.insert_child_before (std::string_view ("before"), x).name (),
                "before");
  EXPECT_EQ (child_names (r), "p,before,x,after,xy,renamed");

  XmlAttribute attr
      = added.append_attribute (std::string_view ("key=").substr (0, 3));
  EXPECT_STREQ (attr.name (), "key");
  EXPECT_TRUE (attr.set_value (std::string_view ("value;").substr (0, 5)));
  EXPECT_STREQ (attr.value (), "value");
  attr = std::string_view ("other;").substr (0, 5);
  EXPECT_STREQ (attr.value (), "other");
  EXPECT_TRUE (attr.set_name (std::string_view ("k2;").substr (0, 2)));
  EXPECT_STREQ (attr.name (), "k2");
  EXPECT_STREQ (added.prepend_attribute (std::string_view ("k1")).name (),
                "k1");
  EXPECT_STREQ (
      added.insert_attribute_after (std::string_view ("k3"), attr).name (),
      "k3");
  EXPECT_STREQ (
      added.insert_attribute_before (std::string_view ("k1b"), attr).name (),
      "k1b");
  EXPECT_EQ (attribute_names (added), "k1,k1b,k2,k3");

  XmlText text = added.text ();
  EXPECT_TRUE (text.set (std::string_view ("body!").substr (0, 4)));
  EXPECT_STREQ (added.text ().get (), "body");
  text = std::string_view ("next!").substr (0, 4);
  EXPECT_STREQ (added.text ().get (), "next");

  XmlNode pcdata = r.append_child (xml_node_type::node_pcdata);
  EXPECT_TRUE (pcdata.set_value (std::string_view ("abc!").substr (0, 3)));
  EXPECT_STREQ (pcdata.value (), "abc");

  EXPECT_TRUE (added.remove_attribute (std::string_view ("k2")));
  EXPECT_FALSE (added.remove_attribute (std::string_view ("k2")));
  EXPECT_TRUE (r.remove_child (std::string_view ("renamed")));
  EXPECT_FALSE (r.child (std::string_view ("renamed")));
}

TEST_F (XmlFixture, GivenDefaultConstructedStringView_WhenLookup_ThenUnfound)
{
  ASSERT_EQ (doc.load_string ("<r a='1'><x/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode const r = doc.document_element ();
  std::string_view const empty;
  EXPECT_FALSE (r.child (empty));
  EXPECT_FALSE (r.attribute (empty));
  EXPECT_FALSE (r.child ("x").next_sibling (empty));
}
} // namespace

TEST_F (XmlFixture,
        GivenLumexStringView_WhenLookupAndEdit_ThenConvertsToStdView)
{
  // The view of the library converts to std::string_view, which is what the
  // string_view_t overloads take from C++17.
  ASSERT_EQ (doc.load_string ("<r a='1'><x/><xy/></r>", kparse_default).status,
             xml_parse_status::status_ok);
  XmlNode r = doc.document_element ();
  lumex_string_view const names ("xyab");
  EXPECT_STREQ (r.child (names.substr (0, 2)).name (), "xy");
  EXPECT_STREQ (r.attribute (names.substr (2, 1)).value (), "1");
  EXPECT_FALSE (r.child (names.substr (0, 3)));
  XmlAttribute hint;
  EXPECT_STREQ (r.attribute (names.substr (2, 1), hint).value (), "1");
  XmlNode added = r.append_child (lumex_string_view ("zz").substr (0, 1));
  EXPECT_STREQ (added.name (), "z");
  EXPECT_TRUE (added.set_name (lumex_string_view ("renamed!").substr (0, 7)));
  EXPECT_STREQ (added.name (), "renamed");
  XmlAttribute attr = added.append_attribute (lumex_string_view ("key"));
  attr = lumex_string_view ("other;").substr (0, 5);
  EXPECT_STREQ (attr.value (), "other");
  XmlText text = added.text ();
  EXPECT_TRUE (text.set (lumex_string_view ("body!").substr (0, 4)));
  EXPECT_STREQ (added.text ().get (), "body");
  EXPECT_TRUE (r.remove_child (lumex_string_view ("renamed")));
  EXPECT_FALSE (r.child ("renamed"));
  EXPECT_TRUE (
      lumex::xml::utility::stringview_equal (lumex_string_view ("ab"), "ab"));
  // The pointer, std::string and C string forms keep their calls.
  EXPECT_STREQ (r.child ("x").name (), "x");
  EXPECT_STREQ (r.child (std::string ("xy")).name (), "xy");
}

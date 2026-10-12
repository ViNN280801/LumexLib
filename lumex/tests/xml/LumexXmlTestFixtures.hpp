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

#ifndef LUMEX_TESTS_XML_HPP
#define LUMEX_TESTS_XML_HPP

#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

// Fixture and helpers of the XML tests. The test sources of every standard
// (LumexXml.cxx11.tests.cpp, .cxx17) add tests to the same GoogleTest
// suites, and every test of a suite must use one fixture class.

class XmlFixture : public ::testing::Test
{
protected:
  lumex::xml::document::XmlDocument doc;
};

// Children of `node` or its attributes by name, joined with ','.
inline std::string
child_names (lumex::xml::node::XmlNode const &node)
{
  std::string names;
  for (lumex::xml::node::XmlNode c = node.first_child (); c;
       c = c.next_sibling ())
    {
      if (!names.empty ())
        names += ',';
      names += c.name ();
    }
  return names;
}

inline std::string
attribute_names (lumex::xml::node::XmlNode const &node)
{
  std::string names;
  for (lumex::xml::attribute::XmlAttribute a = node.first_attribute (); a;
       a = a.next_attribute ())
    {
      if (!names.empty ())
        names += ',';
      names += a.name ();
    }
  return names;
}

#endif // !LUMEX_TESTS_XML_HPP

// lumex/tests/xml/LumexXmlParserOptions.cxx11.tests.cpp
//
// The parse options that select a string conversion of the parser: escapes
// (`&amp;`, `&#65;`), end-of-line normalization, whitespace conversion and
// normalization of attribute values, and trimming of text. The parser picks
// one of several instantiations of its conversion templates from these bits
// (lumex/xml/text/XmlParser.cpp), so a swapped flag gives wrong text for one
// combination only. The table runs every combination of the five options
// over four documents; the expected strings were recorded from the parser
// before its option parameters changed from types to bool values, and they
// follow the pugixml option semantics. The semantic tests below the table
// state the rules in words.
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::constants::Constants::kparse_eol;
using lumex::xml::constants::Constants::kparse_escapes;
using lumex::xml::constants::Constants::kparse_minimal;
using lumex::xml::constants::Constants::kparse_trim_pcdata;
using lumex::xml::constants::Constants::kparse_wconv_attribute;
using lumex::xml::constants::Constants::kparse_wnorm_attribute;

namespace
{
struct parse_case_t
{
  unsigned int options;
  char const *xml;
  char const *attribute;
  char const *text;
};

parse_case_t const kCases[] = {
  { 0x0000,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  ",
    "  t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x0000, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0000, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0000, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\r\ry\n\nz\r",
    "x\r\ry\n\nz\r" },
  { 0x0010,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\r\nd  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "  t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x0010, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0010, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0010, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\r\ry\n\nz\r",
    "x\r\ry\n\nz\r" },
  { 0x0020,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\nd  e&amp;f&lt;&#65;&#x20AC;g  ",
    "  t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x0020, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0020, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0020, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\n\ny\n\nz\n",
    "x\n\ny\n\nz\n" },
  { 0x0030,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\nd  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "  t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x0030, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0030, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0030, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\n\ny\n\nz\n",
    "x\n\ny\n\nz\n" },
  { 0x0040,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&amp;f&lt;&#65;&#x20AC;g  ",
    "  t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x0040, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0040, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0040, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\r\ry\n\nz\r" },
  { 0x0050,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "  t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x0050, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0050, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0050, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\r\ry\n\nz\r" },
  { 0x0060,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&amp;f&lt;&#65;&#x20AC;g  ",
    "  t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x0060, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0060, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0060, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\n\ny\n\nz\n" },
  { 0x0070,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "  t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x0070, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0070, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0070, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\n\ny\n\nz\n" },
  { 0x0080,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "  t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x0080, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0080, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0080, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz\r" },
  { 0x0090,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "  t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x0090, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0090, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0090, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz\r" },
  { 0x00A0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "  t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x00A0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00A0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x00A0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz\n" },
  { 0x00B0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "  t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x00B0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00B0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x00B0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz\n" },
  { 0x00C0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "  t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x00C0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00C0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x00C0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz\r" },
  { 0x00D0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "  t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x00D0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00D0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x00D0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz\r" },
  { 0x00E0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "  t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x  " },
  { 0x00E0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00E0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x00E0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz\n" },
  { 0x00F0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "  t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x  " },
  { 0x00F0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x00F0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x00F0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz\n" },
  { 0x0800,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  ",
    "t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x0800, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0800, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0800, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\r\ry\n\nz\r",
    "x\r\ry\n\nz" },
  { 0x0810,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\r\nd  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x0810, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0810, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0810, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\r\ry\n\nz\r",
    "x\r\ry\n\nz" },
  { 0x0820,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\nd  e&amp;f&lt;&#65;&#x20AC;g  ",
    "t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x0820, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0820, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0820, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\n\ny\n\nz\n",
    "x\n\ny\n\nz" },
  { 0x0830,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a\tb\nc\nd  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x0830, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0830, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0830, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x\n\ny\n\nz\n",
    "x\n\ny\n\nz" },
  { 0x0840,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&amp;f&lt;&#65;&#x20AC;g  ",
    "t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x0840, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0840, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0840, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\r\ry\n\nz" },
  { 0x0850,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x0850, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0850, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0850, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\r\ry\n\nz" },
  { 0x0860,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&amp;f&lt;&#65;&#x20AC;g  ",
    "t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x0860, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0860, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0860, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\n\ny\n\nz" },
  { 0x0870,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "  a b c d  e&f<A\xE2"
    "\x82"
    "\xAC"
    "g  ",
    "t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x0870, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0870, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0870, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x  y  z ",
    "x\n\ny\n\nz" },
  { 0x0880,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x0880, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0880, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x0880, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz" },
  { 0x0890,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x0890, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x0890, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x0890, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz" },
  { 0x08A0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x08A0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08A0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x08A0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz" },
  { 0x08B0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x08B0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08B0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x08B0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz" },
  { 0x08C0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "t\tu\r\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x08C0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08C0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x08C0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz" },
  { 0x08D0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "t\tu\r\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x08D0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08D0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x08D0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\r\ry\n\nz" },
  { 0x08E0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&amp;f&lt;&#65;&#x20AC;g",
    "t\tu\nv  &amp;w&lt;&#66;&#x20AC;  x" },
  { 0x08E0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08E0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "&quot;&apos;&gt;&amp;amp;", "&quot;&apos;&gt;&amp;amp;" },
  { 0x08E0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz" },
  { 0x08F0,
    "<r a=\"  a\tb\nc\r\nd  e&amp;f&lt;&#65;&#x20AC;g  \">  t\tu\r\nv  "
    "&amp;w&lt;&#66;&#x20AC;  x  </r>",
    "a b c d e&f<A\xE2"
    "\x82"
    "\xAC"
    "g",
    "t\tu\nv  &w<B\xE2"
    "\x82"
    "\xAC"
    "  x" },
  { 0x08F0, "<r a=\"plain\">plain</r>", "plain", "plain" },
  { 0x08F0, "<r a=\"&quot;&apos;&gt;&amp;amp;\">&quot;&apos;&gt;&amp;amp;</r>",
    "\"'>&amp;", "\"'>&amp;" },
  { 0x08F0, "<r a=\"x\r\ry\n\nz\r\">x\r\ry\n\nz\r</r>", "x y z",
    "x\n\ny\n\nz" },
};

class XmlParserOptionsTest : public XmlFixture
{
protected:
  std::string
  attribute_of (char const *xml, unsigned int options)
  {
    EXPECT_TRUE (doc.load_string (xml, options));
    return doc.child ("r").attribute ("a").value ();
  }

  std::string
  text_of (char const *xml, unsigned int options)
  {
    EXPECT_TRUE (doc.load_string (xml, options));
    return doc.child ("r").text ().get ();
  }
};
} // namespace

TEST_F (
    XmlParserOptionsTest,
    GivenEveryCombinationOfTheFiveOptions_WhenParsed_ThenTheRecordedTextComesBack)
{
  for (std::size_t i = 0; i < sizeof (kCases) / sizeof (kCases[0]); ++i)
    {
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = doc.load_string (kCases[i].xml, kCases[i].options);
      ASSERT_TRUE (result) << "case " << i << " options " << std::hex
                           << kCases[i].options << ": "
                           << result.description ();
      EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                 kCases[i].attribute)
          << "case " << std::dec << i << " options " << std::hex
          << kCases[i].options << " xml " << kCases[i].xml;
      EXPECT_EQ (std::string (doc.child ("r").text ().get ()), kCases[i].text)
          << "case " << std::dec << i << " options " << std::hex
          << kCases[i].options << " xml " << kCases[i].xml;
    }
}

TEST_F (XmlParserOptionsTest,
        GivenEscapesOff_WhenParsed_ThenEntitiesStayAsWritten)
{
  char const *const xml = "<r a=\"&amp;&#65;\">&lt;&#x42;</r>";
  EXPECT_EQ (attribute_of (xml, kparse_minimal), "&amp;&#65;");
  EXPECT_EQ (text_of (xml, kparse_minimal), "&lt;&#x42;");
}

TEST_F (XmlParserOptionsTest, GivenEscapesOn_WhenParsed_ThenEntitiesAreDecoded)
{
  char const *const xml = "<r a=\"&amp;&#65;\">&lt;&#x42;</r>";
  EXPECT_EQ (attribute_of (xml, kparse_escapes), "&A");
  EXPECT_EQ (text_of (xml, kparse_escapes), "<B");
}

TEST_F (
    XmlParserOptionsTest,
    GivenWhitespaceConversion_WhenParsed_ThenEachWhitespaceCharacterOfAnAttributeIsASpace)
{
  char const *const xml = "<r a=\"p\tq\nr\">x</r>";
  EXPECT_EQ (attribute_of (xml, kparse_minimal), "p\tq\nr");
  EXPECT_EQ (attribute_of (xml, kparse_wconv_attribute), "p q r");
}

TEST_F (
    XmlParserOptionsTest,
    GivenWhitespaceNormalization_WhenParsed_ThenRunsCollapseAndEndsAreTrimmed)
{
  char const *const xml = "<r a=\"  p \t\n q  \">x</r>";
  EXPECT_EQ (attribute_of (xml, kparse_wnorm_attribute), "p q");
}

TEST_F (
    XmlParserOptionsTest,
    GivenEndOfLineNormalization_WhenParsed_ThenCarriageReturnPairsBecomeNewlines)
{
  char const *const xml = "<r a=\"x\">p\r\nq\rr</r>";
  EXPECT_EQ (text_of (xml, kparse_minimal), "p\r\nq\rr");
  EXPECT_EQ (text_of (xml, kparse_eol), "p\nq\nr");
}

TEST_F (XmlParserOptionsTest,
        GivenTrimming_WhenParsed_ThenTheEndsOfTextAreRemoved)
{
  char const *const xml = "<r a=\"x\">  inner  </r>";
  EXPECT_EQ (text_of (xml, kparse_minimal), "  inner  ");
  EXPECT_EQ (text_of (xml, kparse_trim_pcdata), "inner");
}

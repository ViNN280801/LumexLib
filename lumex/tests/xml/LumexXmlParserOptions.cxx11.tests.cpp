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
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::constants::Constants::kformat_no_declaration;
using lumex::xml::constants::Constants::kformat_raw;
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

// ------------------------------------------------ character references
//
// A numeric character reference expands only to a Char of XML 1.0 (section
// 2.2: TAB, LF, CR, U+0020..U+D7FF, U+E000..U+FFFD, U+10000..U+10FFFF). Any
// other value stays in the text as written, `&#xD800;` included, and the
// document still loads. The expected bytes below are written out by hand.

namespace
{
struct reference_t
{
  char const *source; // the reference as it stands in the document
  char const *bytes;  // what it expands to (empty when it stays as written)
};

// Not a Char: the text keeps the reference.
reference_t const kIllegalReferences[] = {
  { "&#xD800;", "" },
  { "&#xDBFF;", "" },
  { "&#xDC00;", "" },
  { "&#xDFFF;", "" },
  { "&#x110000;", "" },
  { "&#x1FFFFF;", "" },
  { "&#x7FFFFFFF;", "" },
  { "&#xFFFFFFFF;", "" },
  { "&#xFFFFFFFFFFFFFFFF;", "" },
  { "&#x0;", "" },
  { "&#x00;", "" },
  { "&#x1;", "" },
  { "&#x8;", "" },
  { "&#xB;", "" },
  { "&#xC;", "" },
  { "&#xE;", "" },
  { "&#x1F;", "" },
  { "&#xFFFE;", "" },
  { "&#xFFFF;", "" },
  // 2^32 plus a legal value: it must not wrap around to the legal value
  { "&#x100000041;", "" },
  { "&#x10000000A;", "" },
  { "&#x100000000000041;", "" },
  { "&#55296;", "" },
  { "&#56319;", "" },
  { "&#57343;", "" },
  { "&#1114112;", "" },
  { "&#2147483647;", "" },
  { "&#4294967295;", "" },
  { "&#0;", "" },
  { "&#1;", "" },
  { "&#8;", "" },
  { "&#11;", "" },
  { "&#12;", "" },
  { "&#31;", "" },
  { "&#65534;", "" },
  { "&#65535;", "" },
  { "&#4294967361;", "" },
  { "&#4294967306;", "" },
  { "&#99999999999999999999;", "" },
};

// A Char: the reference becomes the UTF-8 bytes of the code point.
reference_t const kLegalReferences[] = {
  { "&#x9;", "\x09" },
  { "&#xA;", "\x0A" },
  { "&#xD;", "\x0D" },
  { "&#x20;", " " },
  { "&#x41;", "A" },
  { "&#x7F;", "\x7F" },
  { "&#x80;", "\xC2\x80" },
  { "&#x7FF;", "\xDF\xBF" },
  { "&#x800;", "\xE0\xA0\x80" },
  { "&#xD7FF;", "\xED\x9F\xBF" },
  { "&#xE000;", "\xEE\x80\x80" },
  { "&#xFFFD;", "\xEF\xBF\xBD" },
  { "&#x10000;", "\xF0\x90\x80\x80" },
  { "&#x10FFFF;", "\xF4\x8F\xBF\xBF" },
  { "&#x0000000041;", "A" },
  { "&#9;", "\x09" },
  { "&#10;", "\x0A" },
  { "&#13;", "\x0D" },
  { "&#55295;", "\xED\x9F\xBF" },
  { "&#57344;", "\xEE\x80\x80" },
  { "&#65533;", "\xEF\xBF\xBD" },
  { "&#65536;", "\xF0\x90\x80\x80" },
  { "&#1114111;", "\xF4\x8F\xBF\xBF" },
  { "&#0000000065;", "A" },
};

// The options that change which conversion of the parser reads a reference;
// every set has the escapes on.
unsigned int
reference_options (unsigned int subset)
{
  unsigned int options = kparse_escapes;
  if ((subset & 1U) != 0)
    options |= kparse_eol;
  if ((subset & 2U) != 0)
    options |= kparse_wconv_attribute;
  if ((subset & 4U) != 0)
    options |= kparse_wnorm_attribute;
  if ((subset & 8U) != 0)
    options |= kparse_trim_pcdata;
  return options;
}
} // namespace

TEST_F (
    XmlParserOptionsTest,
    GivenAReferenceThatIsNotAChar_WhenParsedInTextAndAttribute_ThenItStaysAsWritten)
{
  for (std::size_t i = 0;
       i < sizeof (kIllegalReferences) / sizeof (kIllegalReferences[0]); ++i)
    for (unsigned int subset = 0; subset < 16; ++subset)
      {
        std::string const reference = kIllegalReferences[i].source;
        std::string const xml
            = "<r a=\"p" + reference + "q\">p" + reference + "q</r>";
        doc.reset ();
        lumex::xml::text::xml_parse_result_t const result
            = doc.load_string (xml.c_str (), reference_options (subset));
        ASSERT_TRUE (result) << reference << " subset " << subset << ": "
                             << result.description ();
        EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                   "p" + reference + "q")
            << reference << " subset " << subset;
        EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
                   "p" + reference + "q")
            << reference << " subset " << subset;
      }
}

TEST_F (XmlParserOptionsTest,
        GivenAReferenceThatIsAChar_WhenParsedInTextAndAttribute_ThenItExpands)
{
  // Without the whitespace options: they decide what happens to a TAB or a
  // carriage return that the source holds, not to one a reference produces
  // (the next test runs the default options).
  for (std::size_t i = 0;
       i < sizeof (kLegalReferences) / sizeof (kLegalReferences[0]); ++i)
    {
      std::string const reference = kLegalReferences[i].source;
      std::string const xml
          = "<r a=\"p" + reference + "q\">p" + reference + "q</r>";
      std::string const expected
          = std::string ("p") + kLegalReferences[i].bytes + "q";
      doc.reset ();
      lumex::xml::text::xml_parse_result_t const result
          = doc.load_string (xml.c_str (), kparse_escapes);
      ASSERT_TRUE (result) << reference << ": " << result.description ();
      EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                 expected)
          << reference;
      EXPECT_EQ (std::string (doc.child ("r").text ().get ()), expected)
          << reference;
    }
}

TEST_F (
    XmlParserOptionsTest,
    GivenAReferenceThatIsAChar_WhenParsedWithTheDefaultOptions_ThenItExpands)
{
  // kparse_default adds eol and the whitespace conversion of attributes: the
  // result of a reference is not converted again.
  for (std::size_t i = 0;
       i < sizeof (kLegalReferences) / sizeof (kLegalReferences[0]); ++i)
    {
      std::string const reference = kLegalReferences[i].source;
      std::string const xml
          = "<r a=\"p" + reference + "q\">p" + reference + "q</r>";
      std::string const expected
          = std::string ("p") + kLegalReferences[i].bytes + "q";
      doc.reset ();
      ASSERT_TRUE (doc.load_string (xml.c_str ())) << reference;
      EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
                 expected)
          << reference;
      EXPECT_EQ (std::string (doc.child ("r").text ().get ()), expected)
          << reference;
    }
}

TEST_F (XmlParserOptionsTest,
        GivenReferencesNextToEachOther_WhenParsed_ThenEachIsJudgedOnItsOwn)
{
  // The text is moved down behind every expanded reference; a reference that
  // stays must not disturb that.
  char const *const xml
      = "<r a=\"&lt;&#xD800;&amp;&#x41;&#0;&gt;\">&lt;&#xD800;&amp;&#x41;&#0;"
        "&gt;&#x110000;&#x10FFFF;&#xFFFF;&#xE000;</r>";
  doc.reset ();
  ASSERT_TRUE (doc.load_string (xml, kparse_escapes));
  EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
             "<&#xD800;&A&#0;>");
  EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
             "<&#xD800;&A&#0;>&#x110000;\xF4\x8F\xBF\xBF&#xFFFF;\xEE\x80\x80");
}

TEST_F (XmlParserOptionsTest,
        GivenAMalformedReference_WhenParsed_ThenItStaysAsWritten)
{
  // The cases that were left alone before the range check, next to it.
  char const *const xml = "<r>a&#;b&#x;c&#xZ;d&#12a;e&#x41</r>";
  doc.reset ();
  ASSERT_TRUE (doc.load_string (xml, kparse_escapes));
  EXPECT_EQ (std::string (doc.child ("r").text ().get ()),
             "a&#;b&#x;c&#xZ;d&#12a;e&#x41");
}

TEST_F (XmlParserOptionsTest,
        GivenEscapesOff_WhenAReferenceIsNotAChar_ThenItStaysAsWritten)
{
  doc.reset ();
  ASSERT_TRUE (doc.load_string ("<r a=\"&#xD800;\">&#xD800;&#x41;</r>",
                                kparse_minimal));
  EXPECT_EQ (std::string (doc.child ("r").attribute ("a").value ()),
             "&#xD800;");
  EXPECT_EQ (std::string (doc.child ("r").text ().get ()), "&#xD800;&#x41;");
}

TEST_F (XmlParserOptionsTest,
        GivenAReferenceThatStayed_WhenSavedAndLoadedAgain_ThenTheTextIsTheSame)
{
  // The reference is text now, so a save writes its ampersand as &amp; and the
  // file Lumex writes holds no reference that a strict parser refuses.
  doc.reset ();
  ASSERT_TRUE (doc.load_string ("<r a=\"x&#xD800;y\">p&#0;q&#x110000;r</r>",
                                kparse_escapes));
  std::ostringstream out;
  doc.save (out, "", kformat_raw | kformat_no_declaration,
            lumex::xml::types::Types::xml_encoding::encoding_utf8);
  EXPECT_EQ (out.str (),
             "<r a=\"x&amp;#xD800;y\">p&amp;#0;q&amp;#x110000;r</r>");

  lumex::xml::document::XmlDocument again;
  ASSERT_TRUE (again.load_string (out.str ().c_str (), kparse_escapes));
  EXPECT_EQ (std::string (again.child ("r").attribute ("a").value ()),
             "x&#xD800;y");
  EXPECT_EQ (std::string (again.child ("r").text ().get ()),
             "p&#0;q&#x110000;r");
}

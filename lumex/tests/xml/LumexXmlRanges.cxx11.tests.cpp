// lumex/tests/xml/LumexXmlRanges.cxx11.tests.cpp
//
// XmlNode::children () and attributes () return an iterator_range of the core
// utility module (it was XmlObjectRange of the xml module): the return types,
// the loops over children, named children and attributes, and an empty range.
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::core::utility::ranges::iterator_range;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::node::XmlNode;

namespace
{
class XmlRangeTest : public XmlFixture
{
protected:
  void
  SetUp () override
  {
    ASSERT_TRUE (
        doc.load_string ("<r a=\"1\" b=\"2\" c=\"3\"><x/><y/><x/><z/><x/></r>",
                         kparse_default));
  }
};
} // namespace

TEST_F (
    XmlRangeTest,
    GivenANode_WhenChildrenAttributesAndNamedChildrenCalled_ThenTheyReturnIteratorRanges)
{
  XmlNode const root = doc.child ("r");
  static_assert (
      std::is_same<decltype (root.children ()),
                   iterator_range<lumex::xml::node::XmlNodeIterator>>::value,
      "children () returns iterator_range<XmlNodeIterator>");
  static_assert (
      std::is_same<
          decltype (root.attributes ()),
          iterator_range<lumex::xml::attribute::XmlAttributeIterator>>::value,
      "attributes () returns iterator_range<XmlAttributeIterator>");
  static_assert (
      std::is_same<
          decltype (root.children ("x")),
          iterator_range<lumex::xml::node::XmlNamedNodeIterator>>::value,
      "children (name) returns iterator_range<XmlNamedNodeIterator>");
  SUCCEED ();
}

TEST_F (
    XmlRangeTest,
    GivenAnElement_WhenChildrenRangeFor_ThenEveryChildIsVisitedInDocumentOrder)
{
  std::string names;
  for (XmlNode child : doc.child ("r").children ())
    names += child.name ();
  EXPECT_EQ (names, "xyxzx");
}

TEST_F (XmlRangeTest,
        GivenAName_WhenNamedChildrenRangeFor_ThenOnlyThoseChildrenAreVisited)
{
  int visits = 0;
  for (XmlNode child : doc.child ("r").children ("x"))
    {
      EXPECT_STREQ (child.name (), "x");
      ++visits;
    }
  EXPECT_EQ (visits, 3);
}

TEST_F (XmlRangeTest,
        GivenAnElement_WhenAttributesRangeFor_ThenEveryAttributeIsVisited)
{
  std::string joined;
  for (lumex::xml::attribute::XmlAttribute attribute :
       doc.child ("r").attributes ())
    joined += std::string (attribute.name ()) + attribute.value ();
  EXPECT_EQ (joined, "a1b2c3");
}

TEST_F (XmlRangeTest,
        GivenALeafAndAMissingName_WhenRangesBuilt_ThenTheyAreEmpty)
{
  XmlNode const leaf = doc.child ("r").child ("y");
  EXPECT_TRUE (leaf.children ().empty ());
  EXPECT_TRUE (leaf.attributes ().empty ());
  EXPECT_TRUE (doc.child ("r").children ("missing").empty ());
  EXPECT_FALSE (doc.child ("r").children ().empty ());
  EXPECT_FALSE (doc.child ("r").attributes ().empty ());
}

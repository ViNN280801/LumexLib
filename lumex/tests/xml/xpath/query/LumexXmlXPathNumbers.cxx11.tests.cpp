// lumex/tests/xml/xpath/query/LumexXmlXPathNumbers.cxx11.tests.cpp
//
// The rules of XPath 1.0 that compare doubles exactly: the number to boolean
// and number to string conversions (zero, NaN, infinity), floor and ceiling
// of NaN, the numeric predicate (`item[2]`, `item[last()]`, `item[1.5]`) and
// the = and != operators on numbers. The library writes these comparisons
// through one helper (lumex::core::math::ops::exactly_equal) so the
// compiler's -Wfloat-equal stays quiet; these tests fail when that helper or
// one of its callers changes what a comparison means.
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::xpath::node::XPathNode;
using lumex::xml::xpath::query::XPathQuery;

namespace
{
class XPathNumbersTest : public XmlFixture
{
protected:
  void
  SetUp () override
  {
    ASSERT_TRUE (
        doc.load_string ("<r><i>a</i><i>b</i><i>c</i></r>",
                         lumex::xml::constants::Constants::kparse_default));
  }

  bool
  boolean_of (char const *expression)
  {
    XPathQuery query (expression);
    return query.evaluate_boolean (XPathNode (doc));
  }

  double
  number_of (char const *expression)
  {
    XPathQuery query (expression);
    return query.evaluate_number (XPathNode (doc));
  }

  std::string
  string_of (char const *expression)
  {
    XPathQuery query (expression);
    return query.evaluate_string (XPathNode (doc));
  }

  // The text of the nodes a path selects, joined without a separator.
  std::string
  texts_of (char const *path)
  {
    std::string joined;
    lumex::xml::xpath::node::XPathNodeSet const set = doc.select_nodes (path);
    for (std::size_t i = 0; i < set.size (); ++i)
      joined += set[i].node ().child_value ();
    return joined;
  }
};

TEST_F (XPathNumbersTest, GivenZero_WhenBoolean_ThenFalse)
{
  EXPECT_FALSE (boolean_of ("boolean(0)"));
  EXPECT_FALSE (boolean_of ("boolean(-0)"));
}

TEST_F (XPathNumbersTest, GivenNonZeroNumber_WhenBoolean_ThenTrue)
{
  EXPECT_TRUE (boolean_of ("boolean(1)"));
  EXPECT_TRUE (boolean_of ("boolean(-0.5)"));
  EXPECT_TRUE (boolean_of ("boolean(1 div 0)"));
}

TEST_F (XPathNumbersTest, GivenNaN_WhenBoolean_ThenFalse)
{
  EXPECT_FALSE (boolean_of ("boolean(0 div 0)"));
}

TEST_F (XPathNumbersTest, GivenSpecialNumbers_WhenString_ThenXPathSpelling)
{
  EXPECT_EQ ("0", string_of ("string(0)"));
  EXPECT_EQ ("NaN", string_of ("string(0 div 0)"));
  EXPECT_EQ ("Infinity", string_of ("string(1 div 0)"));
  EXPECT_EQ ("-Infinity", string_of ("string(-1 div 0)"));
}

TEST_F (XPathNumbersTest, GivenOrdinaryNumbers_WhenString_ThenDigits)
{
  EXPECT_EQ ("1", string_of ("string(1)"));
  EXPECT_EQ ("-2.5", string_of ("string(-2.5)"));
}

TEST_F (XPathNumbersTest, GivenNumbers_WhenFloorAndCeiling_ThenRounded)
{
  EXPECT_DOUBLE_EQ (2.0, number_of ("floor(2.7)"));
  EXPECT_DOUBLE_EQ (3.0, number_of ("ceiling(2.1)"));
  EXPECT_DOUBLE_EQ (-3.0, number_of ("floor(-2.1)"));
  EXPECT_DOUBLE_EQ (-2.0, number_of ("ceiling(-2.7)"));
}

TEST_F (XPathNumbersTest, GivenNaN_WhenFloorAndCeiling_ThenStaysNaN)
{
  EXPECT_TRUE (std::isnan (number_of ("floor(0 div 0)")));
  EXPECT_TRUE (std::isnan (number_of ("ceiling(0 div 0)")));
}

TEST_F (XPathNumbersTest, GivenIntegerPredicate_WhenSelect_ThenThatPosition)
{
  EXPECT_EQ ("a", texts_of ("//i[1]"));
  EXPECT_EQ ("b", texts_of ("//i[2]"));
  EXPECT_EQ ("b", texts_of ("//i[2.0]"));
  EXPECT_EQ ("c", texts_of ("//i[last()]"));
  EXPECT_EQ ("c", texts_of ("//i[1 + 2]"));
}

TEST_F (XPathNumbersTest, GivenOtherPredicate_WhenSelect_ThenNothing)
{
  EXPECT_EQ ("", texts_of ("//i[1.5]"));
  EXPECT_EQ ("", texts_of ("//i[0]"));
  EXPECT_EQ ("", texts_of ("//i[4]"));
  EXPECT_EQ ("", texts_of ("//i[0 div 0]"));
}

TEST_F (XPathNumbersTest, GivenNumbers_WhenEqualOrNotEqual_ThenIeeeRules)
{
  EXPECT_TRUE (boolean_of ("1 = 1"));
  EXPECT_FALSE (boolean_of ("1 = 2"));
  EXPECT_FALSE (boolean_of ("1 != 1"));
  EXPECT_TRUE (boolean_of ("1 != 2"));
}

TEST_F (XPathNumbersTest, GivenNaN_WhenEqualOrNotEqual_ThenNeverEqual)
{
  EXPECT_FALSE (boolean_of ("(0 div 0) = (0 div 0)"));
  EXPECT_TRUE (boolean_of ("(0 div 0) != (0 div 0)"));
}
} // namespace

// lumex/tests/xml/xpath/query/LumexXmlXPathRelational.cxx11.tests.cpp
//
// The relational operators of XPath 1.0 (<, <=, >, >=) on numbers, on strings
// converted to numbers, on booleans and on node-sets, with the rules for NaN.
// The engine evaluates them with the standard comparison functors on doubles
// and swaps the operands for > and >=, so each operator and each operand
// order has its own case. The expected values come from the XPath 1.0
// specification (section 3.4), not from the engine.
#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::xpath::node::XPathNode;
using lumex::xml::xpath::query::XPathQuery;

namespace
{
class XPathRelationalTest : public XmlFixture
{
protected:
  void
  SetUp () override
  {
    ASSERT_TRUE (doc.load_string (
        "<r><n>3</n><n>7</n><n>12</n><s>abc</s><e/></r>", kparse_default));
  }

  bool
  holds (char const *expression)
  {
    XPathQuery query (expression);
    return query.evaluate_boolean (XPathNode (doc));
  }
};
} // namespace

TEST_F (XPathRelationalTest,
        GivenTwoNumbers_WhenLessOrGreater_ThenTheStrictOrder)
{
  EXPECT_TRUE (holds ("1 < 2"));
  EXPECT_FALSE (holds ("2 < 1"));
  EXPECT_FALSE (holds ("2 < 2"));
  EXPECT_TRUE (holds ("2 > 1"));
  EXPECT_FALSE (holds ("1 > 2"));
  EXPECT_FALSE (holds ("2 > 2"));
}

TEST_F (
    XPathRelationalTest,
    GivenTwoNumbers_WhenLessOrEqualOrGreaterOrEqual_ThenTheOrderIncludesEquality)
{
  EXPECT_TRUE (holds ("1 <= 2"));
  EXPECT_TRUE (holds ("2 <= 2"));
  EXPECT_FALSE (holds ("3 <= 2"));
  EXPECT_TRUE (holds ("2 >= 1"));
  EXPECT_TRUE (holds ("2 >= 2"));
  EXPECT_FALSE (holds ("1 >= 2"));
}

TEST_F (XPathRelationalTest,
        GivenNegativeAndFractionalNumbers_WhenCompared_ThenNumericOrder)
{
  EXPECT_TRUE (holds ("-3 < -2"));
  EXPECT_TRUE (holds ("-0.5 < 0"));
  EXPECT_TRUE (holds ("0.25 <= 0.25"));
  EXPECT_TRUE (holds ("10 > 9.99"));
  EXPECT_FALSE (holds ("-1 >= 0"));
}

TEST_F (XPathRelationalTest,
        GivenStrings_WhenCompared_ThenTheyAreComparedAsNumbers)
{
  // '10' < '9' is false as numbers (it would be true as text).
  EXPECT_FALSE (holds ("'10' < '9'"));
  EXPECT_TRUE (holds ("'9' < '10'"));
  EXPECT_TRUE (holds ("'2' >= '2'"));
  EXPECT_TRUE (holds ("' 5 ' > '4'"));
}

TEST_F (XPathRelationalTest,
        GivenBooleans_WhenCompared_ThenTheyAreComparedAsNumbers)
{
  EXPECT_TRUE (holds ("false() < true()"));
  EXPECT_FALSE (holds ("true() < false()"));
  EXPECT_TRUE (holds ("true() >= true()"));
  EXPECT_TRUE (holds ("true() > false()"));
}

TEST_F (XPathRelationalTest, GivenNaN_WhenAnyRelationalOperator_ThenFalse)
{
  EXPECT_FALSE (holds ("number('x') < 1"));
  EXPECT_FALSE (holds ("number('x') <= 1"));
  EXPECT_FALSE (holds ("number('x') > 1"));
  EXPECT_FALSE (holds ("number('x') >= 1"));
  EXPECT_FALSE (holds ("1 < number('x')"));
  EXPECT_FALSE (holds ("1 >= number('x')"));
  EXPECT_FALSE (holds ("number('x') <= number('x')"));
  EXPECT_FALSE (holds ("number('x') >= number('x')"));
}

TEST_F (XPathRelationalTest,
        GivenANodeSetAndANumber_WhenCompared_ThenTrueIfAnyNodeSatisfies)
{
  // The values are 3, 7 and 12.
  EXPECT_TRUE (holds ("r/n < 4"));
  EXPECT_FALSE (holds ("r/n < 3"));
  EXPECT_TRUE (holds ("r/n <= 3"));
  EXPECT_TRUE (holds ("r/n > 11"));
  EXPECT_FALSE (holds ("r/n > 12"));
  EXPECT_TRUE (holds ("r/n >= 12"));
  EXPECT_TRUE (holds ("5 < r/n"));
  EXPECT_FALSE (holds ("12 < r/n"));
  EXPECT_TRUE (holds ("12 <= r/n"));
  EXPECT_TRUE (holds ("4 > r/n"));
  EXPECT_FALSE (holds ("3 > r/n"));
  EXPECT_TRUE (holds ("3 >= r/n"));
}

TEST_F (XPathRelationalTest,
        GivenANodeSetThatIsEmptyOrNotNumeric_WhenCompared_ThenFalse)
{
  EXPECT_FALSE (holds ("r/missing < 100"));
  EXPECT_FALSE (holds ("100 > r/missing"));
  EXPECT_FALSE (holds ("r/s < 100"));
  EXPECT_FALSE (holds ("r/s >= 0"));
  EXPECT_FALSE (holds ("r/e <= 0"));
}

TEST_F (XPathRelationalTest,
        GivenTwoNodeSets_WhenCompared_ThenTrueIfSomePairSatisfies)
{
  EXPECT_TRUE (holds ("r/n < r/n"));
  EXPECT_FALSE (holds ("r/n < r/missing"));
  EXPECT_TRUE (holds ("r/n[1] < r/n[3]"));
  EXPECT_FALSE (holds ("r/n[3] < r/n[1]"));
  EXPECT_TRUE (holds ("r/n[3] > r/n[1]"));
  EXPECT_TRUE (holds ("r/n[2] >= r/n[2]"));
  EXPECT_TRUE (holds ("r/n[2] <= r/n[2]"));
  EXPECT_FALSE (holds ("r/n[1] >= r/n[3]"));
}

TEST_F (XPathRelationalTest, GivenInfinity_WhenCompared_ThenOrdered)
{
  EXPECT_TRUE (holds ("1 div 0 > 1000000"));
  EXPECT_TRUE (holds ("-1 div 0 < -1000000"));
  EXPECT_TRUE (holds ("1 div 0 >= 1 div 0"));
  EXPECT_TRUE (holds ("-1 div 0 <= -1 div 0"));
  EXPECT_FALSE (holds ("1 div 0 < 1 div 0"));
}

TEST_F (
    XPathRelationalTest,
    GivenARelationalOperatorInAPredicate_WhenSelected_ThenTheMatchingNodesAreCounted)
{
  XPathQuery below ("count(r/n[. < 8])");
  XPathQuery above ("count(r/n[. > 8])");
  XPathQuery upto ("count(r/n[. <= 7])");
  XPathQuery from ("count(r/n[. >= 7])");
  EXPECT_EQ (below.evaluate_number (XPathNode (doc)), 2.0);
  EXPECT_EQ (above.evaluate_number (XPathNode (doc)), 1.0);
  EXPECT_EQ (upto.evaluate_number (XPathNode (doc)), 2.0);
  EXPECT_EQ (from.evaluate_number (XPathNode (doc)), 2.0);
}

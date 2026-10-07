// lumex/tests/xml/xpath/query/LumexXmlXPathNaN.cxx11.tests.cpp
//
// The places of the XPath engine that produce a NaN: the number () of a string
// that is not an XPath number, an XPathQuery without a compiled expression,
// and the number of a variable that does not hold one. The XPath rules of
// NaN itself (boolean, string, floor, ceiling, equality) are in
// LumexXmlXPathNumbers.cxx11.tests.cpp.
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::node::XmlNode;
using lumex::xml::xpath::node::XPathNode;
using lumex::xml::xpath::query::XPathQuery;
using lumex::xml::xpath::variable::XPathVariableSet;

namespace
{
class XPathNaNTest : public XmlFixture
{
protected:
  void
  SetUp () override
  {
    ASSERT_TRUE (doc.load_string ("<r><v>12</v><w>x</w></r>", kparse_default));
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
};
} // namespace

TEST_F (XPathNaNTest, GivenStringsThatAreNotXPathNumbers_WhenNumber_ThenNaN)
{
  char const *const expressions[]
      = { "number('abc')", "number('')",    "number('1e5')",
          "number('--1')", "number('+1')",  "number('1.2.3')",
          "number('.')",   "number('-')",   "number('0x10')",
          "number('1 2')", "number('NaN')", "number('Infinity')" };
  for (std::size_t i = 0; i < sizeof (expressions) / sizeof (expressions[0]);
       ++i)
    EXPECT_TRUE (std::isnan (number_of (expressions[i]))) << expressions[i];
}

TEST_F (XPathNaNTest, GivenStringsThatAreXPathNumbers_WhenNumber_ThenTheValue)
{
  EXPECT_DOUBLE_EQ (number_of ("number(' 12 ')"), 12.0);
  EXPECT_DOUBLE_EQ (number_of ("number('-.5')"), -0.5);
  EXPECT_DOUBLE_EQ (number_of ("number('007')"), 7.0);
  EXPECT_DOUBLE_EQ (number_of ("number('3.')"), 3.0);
  EXPECT_DOUBLE_EQ (number_of ("number(r/v)"), 12.0);
}

TEST_F (XPathNaNTest, GivenANodeThatIsNotANumber_WhenNumberAndString_ThenNaN)
{
  EXPECT_TRUE (std::isnan (number_of ("number(r/w)")));
  EXPECT_EQ (string_of ("string(number(r/w))"), "NaN");
  EXPECT_TRUE (std::isnan (number_of ("number(r/missing)")));
  EXPECT_EQ (string_of ("string(number(r/missing))"), "NaN");
}

TEST_F (XPathNaNTest, GivenAnXPathNaN_WhenArithmetic_ThenNaNPropagates)
{
  EXPECT_TRUE (std::isnan (number_of ("number('x') + 1")));
  EXPECT_TRUE (std::isnan (number_of ("2 * number('x')")));
  EXPECT_TRUE (std::isnan (number_of ("sum(r/w)")));
  EXPECT_EQ (string_of ("string(number('x') < 1)"), "false");
  EXPECT_EQ (string_of ("string(number('x') >= 1)"), "false");
}

TEST_F (XPathNaNTest, GivenANaNResult_WhenCompared_ThenItIsEqualToNothing)
{
  // The sign of a NaN is not part of XPath: whatever it is, the value is
  // neither less than, greater than nor equal to anything, itself included.
  double const nan = number_of ("number('x')");
  EXPECT_TRUE (std::isnan (nan));
  EXPECT_FALSE (nan == nan);
  EXPECT_FALSE (nan < 0 || nan > 0 || nan == 0);
}

TEST_F (XPathNaNTest,
        GivenAQueryWithoutAnExpression_WhenEvaluateNumber_ThenNaN)
{
  XPathQuery const empty;
  EXPECT_TRUE (std::isnan (empty.evaluate_number (XPathNode (doc))));
  EXPECT_FALSE (empty.evaluate_boolean (XPathNode (doc)));
}

TEST_F (XPathNaNTest, GivenAVariableThatIsNotANumber_WhenGetNumber_ThenNaN)
{
  XPathVariableSet variables;
  ASSERT_TRUE (variables.set ("text", "abc"));
  ASSERT_TRUE (variables.set ("flag", true));
  ASSERT_TRUE (variables.set ("count", 5.0));
  EXPECT_TRUE (std::isnan (variables.get ("text")->get_number ()));
  EXPECT_TRUE (std::isnan (variables.get ("flag")->get_number ()));
  EXPECT_DOUBLE_EQ (variables.get ("count")->get_number (), 5.0);
}

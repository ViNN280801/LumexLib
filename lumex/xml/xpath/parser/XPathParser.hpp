#ifndef LUMEX_XML_XPATH_PARSER_HPP
#define LUMEX_XML_XPATH_PARSER_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

#include "lumex/xml/xpath/ast/XPathAstNode.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"
#include "lumex/xml/xpath/parser/XPathParseResult.hpp"
#include "lumex/xml/xpath/parser/lexer/XPathLexer.hpp"
#include "lumex/xml/xpath/variable/XPathVariableSet.hpp"

using namespace Lumex::Xml::Types;

using namespace Lumex::Xml::XPath::Ast;
using namespace Lumex::Xml::XPath::Memory;
using namespace Lumex::Xml::XPath::Variable;
using namespace Lumex::Xml::XPath::Parser::Lexer;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Parser
      {
        struct LUMEX_API XPathParser {
          XPathAllocator *m_alloc;       // NOLINT(misc-non-private-member-variables-in-classes)
          XPathLexer m_lexer;            // NOLINT(misc-non-private-member-variables-in-classes)
          char_t const *m_query;         // NOLINT(misc-non-private-member-variables-in-classes)
          XPathVariableSet *m_variables; // NOLINT(misc-non-private-member-variables-in-classes)
          XPathParseResult *m_result;    // NOLINT(misc-non-private-member-variables-in-classes)
          char_t m_scratch[32];          // NOLINT(misc-non-private-member-variables-in-classes,
                                         // cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
          size_t m_depth{};              // NOLINT(misc-non-private-member-variables-in-classes)

          XPathAstNode *error(char const *message) const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of error_oom() means ignoring that an out-of-memory condition has occurred. "
            "This critical information should be handled to prevent further issues or terminate execution gracefully.")
          XPathAstNode *error_oom() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of error_rec() means ignoring that the maximum recursion depth has been "
            "exceeded. This indicates a potential infinite loop or an overly complex XPath query that could lead to a "
            "stack overflow.")
          XPathAstNode *error_rec() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which "
            "could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.")
          void *alloc_node() const;

          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, char_t const *value) const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which "
            "could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.")
          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, double value) const;

          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, XPathVariable *value) const;

          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, XPathAstNode *left = nullptr,
                                   XPathAstNode *right = nullptr) const;

          XPathAstNode *
          alloc_node(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents) const;

          XPathAstNode *alloc_node(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test) const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned string pointer means ignoring the allocated string, leading to a memory leak "
            "and potential data loss or corruption if the string content is needed later.")
          char_t const *alloc_string(XPathLexerString const &value) const;

          XPathAstNode *parse_function(
            XPathLexerString const &name, size_t argc,
            XPathAstNode *args[2]) const; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

          axis_t parse_axis_name(XPathLexerString const &name, bool &specified);

          nodetest_t parse_node_test_type(XPathLexerString const &name);

          // PrimaryExpr ::= VariableReference | '(' Expr ')' | Literal | Number | FunctionCall
          XPathAstNode *parse_primary_expression();

          // FilterExpr ::= PrimaryExpr | FilterExpr Predicate
          // Predicate ::= '[' PredicateExpr ']'
          // PredicateExpr ::= Expr
          XPathAstNode *parse_filter_expression();

          // Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
          // AxisSpecifier ::= AxisName '::' | '@'?
          // NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
          // NameTest ::= '*' | NCName ':' '*' | QName
          // AbbreviatedStep ::= '.' | '..'
          XPathAstNode *parse_step(XPathAstNode *set);

          // RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
          XPathAstNode *parse_relative_location_path(XPathAstNode *set);

          // LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
          // AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
          XPathAstNode *parse_location_path();

          // PathExpr ::= LocationPath
          //				| FilterExpr
          //				| FilterExpr '/' RelativeLocationPath
          //				| FilterExpr '//' RelativeLocationPath
          // UnionExpr ::= PathExpr | UnionExpr '|' PathExpr
          // UnaryExpr ::= UnionExpr | '-' UnaryExpr
          XPathAstNode *parse_path_or_unary_expression();

          XPathAstNode *parse_expression_rec(XPathAstNode *lhs, int limit);

          // Expr ::= OrExpr
          // OrExpr ::= AndExpr | OrExpr 'or' AndExpr
          // AndExpr ::= EqualityExpr | AndExpr 'and' EqualityExpr
          // EqualityExpr ::= RelationalExpr
          //					| EqualityExpr '=' RelationalExpr
          //					| EqualityExpr '!=' RelationalExpr
          // RelationalExpr ::= AdditiveExpr
          //					  | RelationalExpr '<' AdditiveExpr
          //					  | RelationalExpr '>' AdditiveExpr
          //					  | RelationalExpr '<=' AdditiveExpr
          //					  | RelationalExpr '>=' AdditiveExpr
          // AdditiveExpr ::= MultiplicativeExpr
          //					| AdditiveExpr '+' MultiplicativeExpr
          //					| AdditiveExpr '-' MultiplicativeExpr
          // MultiplicativeExpr ::= UnaryExpr
          //						  | MultiplicativeExpr '*' UnaryExpr
          //						  | MultiplicativeExpr 'div' UnaryExpr
          //						  | MultiplicativeExpr 'mod' UnaryExpr
          XPathAstNode *parse_expression(int limit = 0);

          XPathParser(char_t const *query, XPathVariableSet *variables, XPathAllocator *alloc,
                      XPathParseResult *result);

          XPathAstNode *parse();

          static XPathAstNode *
          parse(char_t const *query, XPathVariableSet *variables, XPathAllocator *alloc, XPathParseResult *result);
        };
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSER_HPP

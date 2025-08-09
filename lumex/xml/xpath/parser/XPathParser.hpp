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
        /**
         * @brief Parses XPath expressions into an Abstract Syntax Tree (AST).
         * @details This class implements a recursive descent parser for XPath 1.0 expressions.
         *          It takes an XPath query string, a set of variables, and a memory allocator
         *          to construct an AST that can then be evaluated. It handles various XPath
         *          constructs, including location paths, expressions, functions, and predicates.
         *
         * @note The parser relies on `XPathLexer` for tokenization and `XPathAllocator`
         *       for memory management of AST nodes. Error reporting is handled via `XPathParseResult`.
         * @warning The parser uses recursion; very deep or complex expressions might lead to
         *          stack overflow if `kxpath_ast_depth_limit` is exceeded.
         */
        struct LUMEX_API XPathParser {
          /// @brief Pointer to the allocator used for creating AST nodes and strings.
          XPathAllocator *m_alloc; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The lexer instance used to tokenize the XPath query.
          XPathLexer m_lexer; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The original XPath query string.
          char_t const *m_query; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Optional set of XPath variables available during parsing and evaluation.
          XPathVariableSet *m_variables; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Pointer to the result structure where parsing errors are reported.
          XPathParseResult *m_result; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Small scratch buffer for temporary string operations.
          char_t m_scratch[32]; // NOLINT(misc-non-private-member-variables-in-classes,
                                // cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
          /// @brief Current recursion depth for parsing expressions.
          size_t m_depth{}; // NOLINT(misc-non-private-member-variables-in-classes)

          /**
           * @brief Reports a parsing error.
           * @details Sets the error message and offset in the `m_result` object and returns `nullptr`.
           * @param message A C-style string describing the error.
           * @return Always returns `nullptr` to indicate a parsing failure.
           */
          XPathAstNode *error(char const *message) const;

          /**
           * @brief Reports an out-of-memory error.
           * @details Sets the `oom` flag in the allocator's error pointer (if available) and returns `nullptr`.
           * @return Always returns `nullptr`.
           */
          // Discarding the return value of error_oom() means ignoring that an out-of-memory condition has occurred.
          // This critical information should be handled to prevent further issues or terminate execution gracefully.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of error_oom() means ignoring that an out-of-memory condition has occurred. "
            "This critical information should be handled to prevent further issues or terminate execution gracefully.")
          XPathAstNode *error_oom() const;

          /**
           * @brief Reports a recursion depth limit error.
           * @details This function is called when the parser's recursion depth exceeds
           *          `kxpath_ast_depth_limit`, indicating a potentially overly complex or
           *          malformed XPath expression.
           * @return Always returns `nullptr` to signal a parsing error.
           */
          // Discarding the return value of error_rec() means ignoring that the maximum recursion depth has been
          // exceeded. This indicates a potential infinite loop or an overly complex XPath query that could lead to a
          // stack overflow.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of error_rec() means ignoring that the maximum recursion depth has been "
            "exceeded. This indicates a potential infinite loop or an overly complex XPath query that could lead to a "
            "stack overflow.")
          XPathAstNode *error_rec() const;

          /**
           * @brief Allocates raw memory for an `XPathAstNode`.
           * @details A helper function to allocate memory for a new `XPathAstNode` using the internal allocator.
           * @return A `void*` pointer to the allocated memory, or `nullptr` on failure.
           */
          // Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which
          // could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which "
            "could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.")
          void *alloc_node() const;

          /**
           * @brief Allocates and constructs an `XPathAstNode` for a string constant.
           * @param type The AST node type, must be `ast_string_constant`.
           * @param rettype The return type of the expression.
           * @param value The string constant value.
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, char_t const *value) const;

          /**
           * @brief Allocates and constructs an `XPathAstNode` for a numeric constant.
           * @param type The AST node type, must be `ast_number_constant`.
           * @param rettype The return type of the expression.
           * @param value The numeric constant value.
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          // Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which
          // could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the return value of alloc_node() means ignoring the result of a memory allocation, which "
            "could lead to use of uninitialized memory or null pointer dereferences if the allocation failed.")
          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, double value) const;

          /**
           * @brief Allocates and constructs an `XPathAstNode` for a variable reference.
           * @param type The AST node type, must be `ast_variable`.
           * @param rettype The return type of the variable.
           * @param value Pointer to the `XPathVariable` object.
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, XPathVariable *value) const;

          /**
           * @brief Allocates and constructs a generic `XPathAstNode` with optional children.
           * @param type The AST node type.
           * @param rettype The return type of the expression.
           * @param left Pointer to the left child node (optional, defaults to `nullptr`).
           * @param right Pointer to the right child node (optional, defaults to `nullptr`).
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          XPathAstNode *alloc_node(ast_type_t type, xpath_value_type rettype, XPathAstNode *left = nullptr,
                                   XPathAstNode *right = nullptr) const;

          /**
           * @brief Allocates and constructs an `XPathAstNode` for a step in a location path.
           * @param type The AST node type, must be `ast_step`.
           * @param left Pointer to the left child node (context node).
           * @param axis The XPath axis of the step.
           * @param test The node test of the step.
           * @param contents Additional contents for the node test (e.g., node name).
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          XPathAstNode *
          alloc_node(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents) const;

          /**
           * @brief Allocates and constructs an `XPathAstNode` for a predicate or filter.
           * @param type The AST node type (`ast_filter` or `ast_predicate`).
           * @param left Pointer to the left child node (expression being filtered).
           * @param right Pointer to the right child node (predicate expression).
           * @param test The predicate type.
           * @return A pointer to the newly created `XPathAstNode`, or `nullptr` on allocation failure.
           */
          XPathAstNode *alloc_node(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test) const;

          /**
           * @brief Allocates a new string and copies contents from an `XPathLexerString`.
           * @details This function is used to create persistent copies of string literals or
           *          names extracted by the lexer, storing them in the `XPathAllocator`.
           * @param value The `XPathLexerString` containing the string data to copy.
           * @return A `char_t const*` pointer to the newly allocated and copied string,
           *         or `nullptr` if allocation fails. The string is null-terminated.
           */
          // Discarding the returned string pointer means ignoring the allocated string, leading to a memory leak
          // and potential data loss or corruption if the string content is needed later.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned string pointer means ignoring the allocated string, leading to a memory leak "
            "and potential data loss or corruption if the string content is needed later.")
          char_t const *alloc_string(XPathLexerString const &value) const;

          /**
           * @brief Parses an XPath function call.
           * @details This function identifies standard XPath functions based on `name` and `argc`,
           *          and constructs the appropriate `XPathAstNode` for the function call.
           *          It performs basic argument type validation.
           * @param name The `XPathLexerString` representing the function name.
           * @param argc The number of arguments passed to the function.
           * @param args An array of `XPathAstNode` pointers representing the function arguments.
           *             Only the first two arguments are directly accessed; subsequent arguments
           *             are linked via `XPathAstNode::m_next`.
           * @return A pointer to the root `XPathAstNode` for the function call, or `nullptr` on error.
           * @throws `LUMEX_ASSERT` if the function has too many arguments for direct array access.
           */
          XPathAstNode *parse_function(
            XPathLexerString const &name, size_t argc,
            XPathAstNode *args[2]) const; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

          /**
           * @brief Parses an XPath axis name.
           * @details Converts a string representation of an axis name (e.g., "child", "ancestor")
           *          into its corresponding `axis_t` enumeration value.
           * @param name The `XPathLexerString` representing the axis name.
           * @param specified A boolean reference that will be set to `true` if the name
           *                  corresponds to a known axis, `false` otherwise.
           * @return The `axis_t` value for the recognized axis, or `axis_child` (default)
           *         if the name is not a recognized axis. The `specified` flag should be used
           *         to check for actual recognition.
           */
          axis_t parse_axis_name(XPathLexerString const &name, bool &specified);

          /**
           * @brief Parses an XPath node test type.
           * @details Converts a string representation of a node test type (e.g., "comment", "text")
           *          into its corresponding `nodetest_t` enumeration value.
           * @param name The `XPathLexerString` representing the node test type name.
           * @return The `nodetest_t` value for the recognized node test type, or `nodetest_none`
           *         if the name is not recognized.
           */
          nodetest_t parse_node_test_type(XPathLexerString const &name);

          /**
           * @brief Parses a PrimaryExpr production in the XPath grammar.
           * @details A PrimaryExpr can be a VariableReference, an expression in parentheses,
           *          a Literal (string), a Number, or a FunctionCall.
           * @return A pointer to the root `XPathAstNode` of the parsed primary expression,
           *         or `nullptr` on parsing error.
           * @throws `LUMEX_ASSERT` if an unrecognized primary expression token is encountered.
           * @note This function handles the recursive parsing of sub-expressions.
           */
          // PrimaryExpr ::= VariableReference | '(' Expr ')' | Literal | Number | FunctionCall
          XPathAstNode *parse_primary_expression();

          /**
           * @brief Parses a FilterExpr production in the XPath grammar.
           * @details A FilterExpr is a PrimaryExpr optionally followed by one or more Predicates.
           * @return A pointer to the root `XPathAstNode` of the parsed filter expression,
           *         or `nullptr` on parsing error.
           * @note This function handles the recursive parsing of predicates.
           * @warning Returns error if predicate is applied to non-node-set expression.
           */
          // FilterExpr ::= PrimaryExpr | FilterExpr Predicate
          // Predicate ::= '[' PredicateExpr ']'
          // PredicateExpr ::= Expr
          XPathAstNode *parse_filter_expression();

          /**
           * @brief Parses a Step production in the XPath grammar.
           * @details A Step consists of an AxisSpecifier, a NodeTest, and zero or more Predicates,
           *          or it can be an AbbreviatedStep ('.' or '..').
           * @param set An optional `XPathAstNode` representing the context node set for this step.
           * @return A pointer to the root `XPathAstNode` of the parsed step, or `nullptr` on parsing error.
           * @note This function handles implied child axis, abbreviated steps, and various node tests.
           * @warning Returns error if predicates are applied to abbreviated steps.
           */
          // Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
          // AxisSpecifier ::= AxisName '::' | '@'?
          // NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
          // NameTest ::= '*' | NCName ':' '*' | QName
          // AbbreviatedStep ::= '.' | '..'
          XPathAstNode *parse_step(XPathAstNode *set);

          /**
           * @brief Parses a RelativeLocationPath production in the XPath grammar.
           * @details A RelativeLocationPath is a sequence of Steps separated by '/' or '//'.
           * @param set An optional `XPathAstNode` representing the initial context node set for the path.
           * @return A pointer to the root `XPathAstNode` of the parsed relative location path,
           *         or `nullptr` on parsing error.
           * @note This function handles the "descendant-or-self" implied step for '//'.
           */
          // RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
          XPathAstNode *parse_relative_location_path(XPathAstNode *set);

          /**
           * @brief Parses a LocationPath production in the XPath grammar.
           * @details A LocationPath can be either a RelativeLocationPath or an AbsoluteLocationPath.
           *          AbsoluteLocationPath starts with '/' or '//'.
           * @return A pointer to the root `XPathAstNode` of the parsed location path,
           *         or `nullptr` on parsing error.
           */
          // LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
          // AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
          XPathAstNode *parse_location_path();

          /**
           * @brief Parses a PathExpr, UnionExpr, or UnaryExpr production.
           * @details This function acts as an entry point for expressions that can
           *          start with a location path, a filter expression, or a unary minus.
           *          It resolves ambiguities related to `FilterExpr` vs. `LocationPath`
           *          and handles path expressions that append steps to filter expressions.
           * @return A pointer to the root `XPathAstNode` of the parsed expression,
           *         or `nullptr` on parsing error.
           */
          // PathExpr ::= LocationPath
          //				| FilterExpr
          //				| FilterExpr '/' RelativeLocationPath
          //				| FilterExpr '//' RelativeLocationPath
          // UnionExpr ::= PathExpr | UnionExpr '|' PathExpr
          // UnaryExpr ::= UnionExpr | '-' UnaryExpr
          XPathAstNode *parse_path_or_unary_expression();

          /**
           * @brief Recursively parses binary expressions based on operator precedence.
           * @details This is a core part of the shunting-yard algorithm (or operator-precedence parsing)
           *          that handles binary operators like `+`, `-`, `*`, `div`, `mod`, `=`, `!=`, `<`, `>`,
           *          `<=`, `>=`, `and`, `or`, and `|` (union). It takes into account operator precedence
           *          to build the correct AST.
           * @param lhs The left-hand side `XPathAstNode` already parsed.
           * @param limit The minimum precedence level for operators to consider at this level of recursion.
           * @return A pointer to the root `XPathAstNode` of the parsed binary expression chain,
           *         or `nullptr` on parsing error.
           * @note This function is highly recursive and manages `m_depth`.
           * @warning Ensures that the union operator (`|`) is only applied to node sets.
           */
          XPathAstNode *parse_expression_rec(XPathAstNode *lhs, int limit);

          /**
           * @brief Parses a general XPath expression (`Expr`).
           * @details This function is the main entry point for parsing any XPath expression.
           *          It handles the full hierarchy of XPath expressions, from `OrExpr` down
           *          to `UnaryExpr`, respecting operator precedence and associativity.
           * @param limit The minimum precedence level for operators to consider (defaults to 0 for full expression).
           * @return A pointer to the root `XPathAstNode` of the parsed expression, or `nullptr` on parsing error.
           * @note This function manages recursion depth using `m_depth`.
           */
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

          /**
           * @brief Constructs an `XPathParser` object.
           * @details Initializes the parser with the XPath query string, optional variable set,
           *          memory allocator, and a result structure for error reporting.
           * @param query The XPath query string to parse.
           * @param variables A pointer to an `XPathVariableSet` for resolving variable references, or `nullptr`.
           * @param alloc A pointer to the `XPathAllocator` to use for AST node and string allocations.
           * @param result A pointer to an `XPathParseResult` structure to store parsing outcome and errors.
           * @note The constructor initializes the internal `XPathLexer` and `m_depth`.
           */
          XPathParser(char_t const *query, XPathVariableSet *variables, XPathAllocator *alloc,
                      XPathParseResult *result);

          /**
           * @brief Parses the XPath query.
           * @details This is the primary method to start the parsing process. It calls
           *          `parse_expression()` and then checks if the entire query has been consumed.
           * @return A pointer to the root `XPathAstNode` of the parsed expression tree if successful,
           *         or `nullptr` if a parsing error occurred.
           * @throws `LUMEX_ASSERT` if `m_depth` is not 0 after parsing, indicating a potential
           *         recursion imbalance.
           * @warning Returns error if unparsed tokens remain after expression.
           */
          XPathAstNode *parse();

          /**
           * @brief Static helper to parse an XPath query.
           * @details Provides a convenient static interface for parsing an XPath expression
           *          without explicitly creating an `XPathParser` object. It internally
           *          creates a parser and calls its `parse()` method.
           * @param query The XPath query string to parse.
           * @param variables A pointer to an `XPathVariableSet` for resolving variable references, or `nullptr`.
           * @param alloc A pointer to the `XPathAllocator` to use for AST node and string allocations.
           * @param result A pointer to an `XPathParseResult` structure to store parsing outcome and errors.
           * @return A pointer to the root `XPathAstNode` of the parsed expression tree if successful,
           *         or `nullptr` if a parsing error occurred.
           */
          static XPathAstNode *
          parse(char_t const *query, XPathVariableSet *variables, XPathAllocator *alloc, XPathParseResult *result);
        };
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSER_HPP

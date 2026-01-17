#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/xpath/variable/XPathVariableSet.hpp"

#include "XPathParser.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Parser;

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::error(char const *message) const
{
  m_result->error  = message;
  m_result->offset = m_lexer.current_pos() - m_query;

  return nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::error_oom() const
{
  LUMEX_ASSERT(m_alloc->m_error);
  *m_alloc->m_error = true;

  return nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::error_rec() const
{
  return error("Exceeded maximum allowed query depth");
}

LUMEX_PUBLIC_API
void *
XPathParser::alloc_node() const
{
  return m_alloc->allocate(sizeof(XPathAstNode));
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, xpath_value_type rettype, char_t const *value) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, xpath_value_type rettype, double value) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, xpath_value_type rettype, XPathVariable *value) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, xpath_value_type rettype, XPathAstNode *left, XPathAstNode *right) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, rettype, left, right) : nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, left, axis, test, contents) : nullptr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::alloc_node(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test) const
{
  void *memory = alloc_node();
  return (memory != nullptr) ? new(memory) XPathAstNode(type, left, right, test) : nullptr;
}

LUMEX_PUBLIC_API
char_t const *
XPathParser::alloc_string(XPathLexerString const &value) const
{
  if(value.begin == nullptr) return LUMEX_XML_TEXT("");

  auto length = static_cast<size_t>(value.end - value.begin);
  auto *chr   = static_cast<char_t *>(m_alloc->allocate((length + 1) * sizeof(char_t)));
  if(chr == nullptr) return nullptr;

  std::memcpy(chr, value.begin, length * sizeof(char_t));
  chr[length] = 0;

  return chr;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_function( // NOLINT(readability-function-cognitive-complexity)
  XPathLexerString const &name, size_t argc,
  XPathAstNode *args[2]) const // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
{
  switch(name.begin[0])
  {
  case 'b':
    if(name == LUMEX_XML_TEXT("boolean") && argc == 1) return alloc_node(ast_func_boolean, xpath_type_boolean, args[0]);

    break;

  case 'c':
    if(name == LUMEX_XML_TEXT("count") && argc == 1)
    {
      if(args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
      return alloc_node(ast_func_count, xpath_type_number, args[0]);
    }
    else if(name == LUMEX_XML_TEXT("contains") && argc == 2)
      return alloc_node(ast_func_contains, xpath_type_boolean, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("concat") && argc >= 2)
      return alloc_node(ast_func_concat, xpath_type_string, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("ceiling") && argc == 1)
      return alloc_node(ast_func_ceiling, xpath_type_number, args[0]);

    break;

  case 'f':
    if(name == LUMEX_XML_TEXT("false") && argc == 0)
      return alloc_node(ast_func_false, xpath_type_boolean);
    else if(name == LUMEX_XML_TEXT("floor") && argc == 1)
      return alloc_node(ast_func_floor, xpath_type_number, args[0]);

    break;

  case 'i':
    if(name == LUMEX_XML_TEXT("id") && argc == 1) return alloc_node(ast_func_id, xpath_type_node_set, args[0]);

    break;

  case 'l':
    if(name == LUMEX_XML_TEXT("last") && argc == 0)
      return alloc_node(ast_func_last, xpath_type_number);
    else if(name == LUMEX_XML_TEXT("lang") && argc == 1)
      return alloc_node(ast_func_lang, xpath_type_boolean, args[0]);
    else if(name == LUMEX_XML_TEXT("local-name") && argc <= 1)
    {
      if(argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
      return alloc_node(argc == 0 ? ast_func_local_name_0 : ast_func_local_name_1, xpath_type_string, args[0]);
    }

    break;

  case 'n':
    if(name == LUMEX_XML_TEXT("name") && argc <= 1)
    {
      if(argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
      return alloc_node(argc == 0 ? ast_func_name_0 : ast_func_name_1, xpath_type_string, args[0]);
    }
    else if(name == LUMEX_XML_TEXT("namespace-uri") && argc <= 1)
    {
      if(argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
      return alloc_node(argc == 0 ? ast_func_namespace_uri_0 : ast_func_namespace_uri_1, xpath_type_string, args[0]);
    }
    else if(name == LUMEX_XML_TEXT("normalize-space") && argc <= 1)
      return alloc_node(argc == 0 ? ast_func_normalize_space_0 : ast_func_normalize_space_1, xpath_type_string, args[0],
                        args[1]);
    else if(name == LUMEX_XML_TEXT("not") && argc == 1)
      return alloc_node(ast_func_not, xpath_type_boolean, args[0]);
    else if(name == LUMEX_XML_TEXT("number") && argc <= 1)
      return alloc_node(argc == 0 ? ast_func_number_0 : ast_func_number_1, xpath_type_number, args[0]);

    break;

  case 'p':
    if(name == LUMEX_XML_TEXT("position") && argc == 0) return alloc_node(ast_func_position, xpath_type_number);

    break;

  case 'r':
    if(name == LUMEX_XML_TEXT("round") && argc == 1) return alloc_node(ast_func_round, xpath_type_number, args[0]);

    break;

  case 's':
    if(name == LUMEX_XML_TEXT("string") && argc <= 1)
      return alloc_node(argc == 0 ? ast_func_string_0 : ast_func_string_1, xpath_type_string, args[0]);
    else if(name == LUMEX_XML_TEXT("string-length") && argc <= 1)
      return alloc_node(argc == 0 ? ast_func_string_length_0 : ast_func_string_length_1, xpath_type_number, args[0]);
    else if(name == LUMEX_XML_TEXT("starts-with") && argc == 2)
      return alloc_node(ast_func_starts_with, xpath_type_boolean, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("substring-before") && argc == 2)
      return alloc_node(ast_func_substring_before, xpath_type_string, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("substring-after") && argc == 2)
      return alloc_node(ast_func_substring_after, xpath_type_string, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("substring") && (argc == 2 || argc == 3))
      return alloc_node(argc == 2 ? ast_func_substring_2 : ast_func_substring_3, xpath_type_string, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("sum") && argc == 1)
    {
      if(args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
      return alloc_node(ast_func_sum, xpath_type_number, args[0]);
    }

    break;

  case 't':
    if(name == LUMEX_XML_TEXT("translate") && argc == 3)
      return alloc_node(ast_func_translate, xpath_type_string, args[0], args[1]);
    else if(name == LUMEX_XML_TEXT("true") && argc == 0)
      return alloc_node(ast_func_true, xpath_type_boolean);

    break;

  default: break;
  }

  return error("Unrecognized function or wrong parameter count");
}

LUMEX_PUBLIC_API
axis_t
XPathParser::parse_axis_name(XPathLexerString const &name, bool &specified)
{
  specified = true;

  switch(name.begin[0])
  {
  case 'a':
    if(name == LUMEX_XML_TEXT("ancestor"))
      return axis_ancestor;
    else if(name == LUMEX_XML_TEXT("ancestor-or-self"))
      return axis_ancestor_or_self;
    else if(name == LUMEX_XML_TEXT("attribute"))
      return axis_attribute;

    break;

  case 'c':
    if(name == LUMEX_XML_TEXT("child")) return axis_child;

    break;

  case 'd':
    if(name == LUMEX_XML_TEXT("descendant"))
      return axis_descendant;
    else if(name == LUMEX_XML_TEXT("descendant-or-self"))
      return axis_descendant_or_self;

    break;

  case 'f':
    if(name == LUMEX_XML_TEXT("following"))
      return axis_following;
    else if(name == LUMEX_XML_TEXT("following-sibling"))
      return axis_following_sibling;

    break;

  case 'n':
    if(name == LUMEX_XML_TEXT("namespace")) return axis_namespace;

    break;

  case 'p':
    if(name == LUMEX_XML_TEXT("parent"))
      return axis_parent;
    else if(name == LUMEX_XML_TEXT("preceding"))
      return axis_preceding;
    else if(name == LUMEX_XML_TEXT("preceding-sibling"))
      return axis_preceding_sibling;

    break;

  case 's':
    if(name == LUMEX_XML_TEXT("self")) return axis_self;

    break;

  default: break;
  }

  specified = false;
  return axis_child;
}

LUMEX_PUBLIC_API
nodetest_t
XPathParser::parse_node_test_type(XPathLexerString const &name)
{
  switch(name.begin[0])
  {
  case 'c':
    if(name == LUMEX_XML_TEXT("comment")) return nodetest_type_comment;

    break;

  case 'n':
    if(name == LUMEX_XML_TEXT("node")) return nodetest_type_node;

    break;

  case 'p':
    if(name == LUMEX_XML_TEXT("processing-instruction")) return nodetest_type_pi;

    break;

  case 't':
    if(name == LUMEX_XML_TEXT("text")) return nodetest_type_text;

    break;

  default: break;
  }

  return nodetest_none;
}

// PrimaryExpr ::= VariableReference | '(' Expr ')' | Literal | Number | FunctionCall
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_primary_expression() // NOLINT(misc-no-recursion, readability-function-cognitive-complexity)
{
  switch(m_lexer.current())
  {
  case lex_var_ref: {
    XPathLexerString name = m_lexer.contents();

    if(m_variables == nullptr) return error("Unknown variable: variable set is not provided");

    XPathVariable *var = nullptr;
    if(!Lumex::Xml::XPath::Variable::get_variable_scratch(m_scratch, m_variables, name.begin, name.end, &var))
      return error_oom();

    if(var == nullptr) return error("Unknown variable: variable set does not contain the given name");

    m_lexer.next();

    return alloc_node(ast_variable, var->type(), var);
  }

  case lex_open_brace: {
    m_lexer.next();

    XPathAstNode *ast_node = parse_expression();
    if(ast_node == nullptr) return nullptr;

    if(m_lexer.current() != lex_close_brace) return error("Expected ')' to match an opening '('");

    m_lexer.next();

    return ast_node;
  }

  case lex_quoted_string: {
    char_t const *value = alloc_string(m_lexer.contents());
    if(value == nullptr) return nullptr;

    m_lexer.next();

    return alloc_node(ast_string_constant, xpath_type_string, value);
  }

  case lex_number: {
    double value = 0;

    if(!Utility::convert_string_to_number_scratch(m_scratch, m_lexer.contents().begin, m_lexer.contents().end, &value))
      return error_oom();

    m_lexer.next();

    return alloc_node(ast_number_constant, xpath_type_number, value);
  }

  case lex_string: {
    XPathAstNode *args[2] // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
      = {nullptr};
    size_t argc               = 0;

    XPathLexerString function = m_lexer.contents();
    m_lexer.next();

    XPathAstNode *last_arg = nullptr;

    if(m_lexer.current() != lex_open_brace) return error("Unrecognized function call");
    m_lexer.next();

    size_t old_depth = m_depth;

    while(m_lexer.current() != lex_close_brace)
    {
      if(argc > 0)
      {
        if(m_lexer.current() != lex_comma) return error("No comma between function arguments");
        m_lexer.next();
      }

      if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

      XPathAstNode *astNode = parse_expression();
      if(astNode == nullptr) return nullptr;

      if(argc < 2)
        args[argc] = astNode; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      else
        last_arg->set_next(astNode);

      argc++;
      last_arg = astNode;
    }

    m_lexer.next();

    m_depth = old_depth;

    return parse_function(function, argc,
                          args); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  }

  default: return error("Unrecognizable primary expression");
  }
}

// FilterExpr ::= PrimaryExpr | FilterExpr Predicate
// Predicate ::= '[' PredicateExpr ']'
// PredicateExpr ::= Expr
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_filter_expression() // NOLINT(misc-no-recursion)
{
  XPathAstNode *astNode = parse_primary_expression();
  if(astNode == nullptr) return nullptr;

  size_t old_depth = m_depth;

  while(m_lexer.current() == lex_open_square_brace)
  {
    m_lexer.next();

    if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

    if(astNode->rettype() != xpath_type_node_set) return error("Predicate has to be applied to node set");

    XPathAstNode *expr = parse_expression();
    if(expr == nullptr) return nullptr;

    astNode = alloc_node(ast_filter, astNode, expr, predicate_default);
    if(astNode == nullptr) return nullptr;

    if(m_lexer.current() != lex_close_square_brace) return error("Expected ']' to match an opening '['");

    m_lexer.next();
  }

  m_depth = old_depth;

  return astNode;
}

// Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
// AxisSpecifier ::= AxisName '::' | '@'?
// NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
// NameTest ::= '*' | NCName ':' '*' | QName
// AbbreviatedStep ::= '.' | '..'
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_step(XPathAstNode *set) // NOLINT(misc-no-recursion, readability-function-cognitive-complexity)
{
  if((set != nullptr) && set->rettype() != xpath_type_node_set) return error("Step has to be applied to node set");

  bool axis_specified = false;
  axis_t axis         = axis_child; // implied child axis

  if(m_lexer.current() == lex_axis_attribute)
  {
    axis           = axis_attribute;
    axis_specified = true;

    m_lexer.next();
  }
  else if(m_lexer.current() == lex_dot)
  {
    m_lexer.next();

    if(m_lexer.current() == lex_open_square_brace) return error("Predicates are not allowed after an abbreviated step");

    return alloc_node(ast_step, set, axis_self, nodetest_type_node, nullptr);
  }
  else if(m_lexer.current() == lex_double_dot)
  {
    m_lexer.next();

    if(m_lexer.current() == lex_open_square_brace) return error("Predicates are not allowed after an abbreviated step");

    return alloc_node(ast_step, set, axis_parent, nodetest_type_node, nullptr);
  }

  nodetest_t nt_type = nodetest_none;
  XPathLexerString nt_name;

  if(m_lexer.current() == lex_string)
  {
    // node name test
    nt_name = m_lexer.contents();
    m_lexer.next();

    // was it an axis name?
    if(m_lexer.current() == lex_double_colon)
    {
      // parse axis name
      if(axis_specified) return error("Two axis specifiers in one step");

      axis = parse_axis_name(nt_name, axis_specified);

      if(!axis_specified) return error("Unknown axis");

      // read actual node test
      m_lexer.next();

      if(m_lexer.current() == lex_multiply)
      {
        nt_type = nodetest_all;
        nt_name = XPathLexerString();
        m_lexer.next();
      }
      else if(m_lexer.current() == lex_string)
      {
        nt_name = m_lexer.contents();
        m_lexer.next();
      }
      else { return error("Unrecognized node test"); }
    }

    if(nt_type == nodetest_none)
    {
      // node type test or processing-instruction
      if(m_lexer.current() == lex_open_brace)
      {
        m_lexer.next();

        if(m_lexer.current() == lex_close_brace)
        {
          m_lexer.next();

          nt_type = parse_node_test_type(nt_name);

          if(nt_type == nodetest_none) return error("Unrecognized node type");

          nt_name = XPathLexerString();
        }
        else if(nt_name == LUMEX_XML_TEXT("processing-instruction"))
        {
          if(m_lexer.current() != lex_quoted_string)
            return error("Only literals are allowed as arguments to processing-instruction()");

          nt_type = nodetest_pi;
          nt_name = m_lexer.contents();
          m_lexer.next();

          if(m_lexer.current() != lex_close_brace) return error("Unmatched brace near processing-instruction()");
          m_lexer.next();
        }
        else { return error("Unmatched brace near node type test"); }
      }
      // QName or NCName:*
      else if(nt_name.end - nt_name.begin > 2 && nt_name.end[-2] == ':' && nt_name.end[-1] == '*') // NCName:*
      {
        nt_name.end--; // erase *

        nt_type = nodetest_all_in_namespace;
      }
      else { nt_type = nodetest_name; }
    }
  }
  else if(m_lexer.current() == lex_multiply)
  {
    nt_type = nodetest_all;
    m_lexer.next();
  }
  else { return error("Unrecognized node test"); }

  char_t const *nt_name_copy = alloc_string(nt_name);
  if(nt_name_copy == nullptr) return nullptr;

  XPathAstNode *astNode = alloc_node(ast_step, set, axis, nt_type, nt_name_copy);
  if(astNode == nullptr) return nullptr;

  size_t old_depth   = m_depth;

  XPathAstNode *last = nullptr;

  while(m_lexer.current() == lex_open_square_brace)
  {
    m_lexer.next();

    if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

    XPathAstNode *expr = parse_expression();
    if(expr == nullptr) return nullptr;

    XPathAstNode *pred = alloc_node(ast_predicate, nullptr, expr, predicate_default);
    if(pred == nullptr) return nullptr;

    if(m_lexer.current() != lex_close_square_brace) return error("Expected ']' to match an opening '['");
    m_lexer.next();

    if(last != nullptr)
      last->set_next(pred);
    else
      astNode->set_right(pred);

    last = pred;
  }

  m_depth = old_depth;

  return astNode;
}

// RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_relative_location_path(XPathAstNode *set) // NOLINT(misc-no-recursion)
{
  XPathAstNode *astNode = parse_step(set);
  if(astNode == nullptr) return nullptr;

  size_t old_depth = m_depth;

  while(m_lexer.current() == lex_slash || m_lexer.current() == lex_double_slash)
  {
    lexeme_t lexeme = m_lexer.current();
    m_lexer.next();

    if(lexeme == lex_double_slash)
    {
      astNode = alloc_node(ast_step, astNode, axis_descendant_or_self, nodetest_type_node, nullptr);
      if(astNode == nullptr) return nullptr;

      ++m_depth;
    }

    if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

    astNode = parse_step(astNode);
    if(astNode == nullptr) return nullptr;
  }

  m_depth = old_depth;

  return astNode;
}

// LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
// AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_location_path() // NOLINT(misc-no-recursion)
{
  if(m_lexer.current() == lex_slash)
  {
    m_lexer.next();

    XPathAstNode *astNode = alloc_node(ast_step_root, xpath_type_node_set);
    if(astNode == nullptr) return nullptr;

    // relative location path can start from axis_attribute, dot, double_dot, multiply and string lexemes; any
    // other lexeme means standalone root path
    lexeme_t lexeme = m_lexer.current();

    if(lexeme == lex_string || lexeme == lex_axis_attribute || lexeme == lex_dot || lexeme == lex_double_dot
       || lexeme == lex_multiply)
      return parse_relative_location_path(astNode);

    return astNode;
  }
  if(m_lexer.current() == lex_double_slash)
  {
    m_lexer.next();

    XPathAstNode *astNode = alloc_node(ast_step_root, xpath_type_node_set);
    if(astNode == nullptr) return nullptr;

    astNode = alloc_node(ast_step, astNode, axis_descendant_or_self, nodetest_type_node, nullptr);
    if(astNode == nullptr) return nullptr;

    return parse_relative_location_path(astNode);
  }

  // else clause moved outside of if because of bogus warning 'control may reach end of non-void function
  // being inlined' in gcc 4.0.1
  return parse_relative_location_path(nullptr);
}

// PathExpr ::= LocationPath
//				| FilterExpr
//				| FilterExpr '/' RelativeLocationPath
//				| FilterExpr '//' RelativeLocationPath
// UnionExpr ::= PathExpr | UnionExpr '|' PathExpr
// UnaryExpr ::= UnionExpr | '-' UnaryExpr
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_path_or_unary_expression() // NOLINT(misc-no-recursion, readability-function-cognitive-complexity)
{
  // Clarification.
  // PathExpr begins with either LocationPath or FilterExpr.
  // FilterExpr begins with PrimaryExpr
  // PrimaryExpr begins with '$' in case of it being a variable reference,
  // '(' in case of it being an expression, string literal, number constant or
  // function call.
  if(m_lexer.current() == lex_var_ref || m_lexer.current() == lex_open_brace || m_lexer.current() == lex_quoted_string
     || m_lexer.current() == lex_number || m_lexer.current() == lex_string)
  {
    if(m_lexer.current() == lex_string)
    {
      // This is either a function call, or not - if not, we shall proceed with location path
      char_t const *state = m_lexer.state();

      while(LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        *state, ct_space))
        ++state;

      if(*state != '(') return parse_location_path();

      // This looks like a function call; however this still can be a node-test. Check it.
      if(parse_node_test_type(m_lexer.contents()) != nodetest_none) return parse_location_path();
    }

    XPathAstNode *astNode = parse_filter_expression();
    if(astNode == nullptr) return nullptr;

    if(m_lexer.current() == lex_slash || m_lexer.current() == lex_double_slash)
    {
      lexeme_t lexeme = m_lexer.current();
      m_lexer.next();

      if(lexeme == lex_double_slash)
      {
        if(astNode->rettype() != xpath_type_node_set) return error("Step has to be applied to node set");

        astNode = alloc_node(ast_step, astNode, axis_descendant_or_self, nodetest_type_node, nullptr);
        if(astNode == nullptr) return nullptr;
      }

      // select from location path
      return parse_relative_location_path(astNode);
    }

    return astNode;
  }
  if(m_lexer.current() == lex_minus)
  {
    m_lexer.next();

    // precedence 7+ - only parses union expressions
    XPathAstNode *astNode = parse_expression(7);
    if(astNode == nullptr) return nullptr;

    return alloc_node(ast_op_negate, xpath_type_number, astNode);
  }

  return parse_location_path();
}

struct binary_op_t {
  ast_type_t asttype;       // NOLINT(misc-non-private-member-variables-in-classes)
  xpath_value_type rettype; // NOLINT(misc-non-private-member-variables-in-classes)
  int precedence;           // NOLINT(misc-non-private-member-variables-in-classes)

  binary_op_t() : asttype(ast_unknown), rettype(xpath_type_none), precedence(0) {}

  binary_op_t(ast_type_t asttype_, xpath_value_type rettype_, int precedence_)
      : asttype(asttype_), rettype(rettype_), precedence(precedence_)
  {}

  static binary_op_t
  parse(XPathLexer &lexer)
  {
    switch(lexer.current())
    {
    case lex_string:
      if(lexer.contents() == LUMEX_XML_TEXT("or"))
        return {ast_op_or, xpath_type_boolean, 1};
      else if(lexer.contents() == LUMEX_XML_TEXT("and"))
        return {ast_op_and, xpath_type_boolean, 2};
      else if(lexer.contents() == LUMEX_XML_TEXT("div"))
        return {ast_op_divide, xpath_type_number, 6};
      else if(lexer.contents() == LUMEX_XML_TEXT("mod"))
        return {ast_op_mod, xpath_type_number, 6};
      else
        return {};

    case lex_equal: return {ast_op_equal, xpath_type_boolean, 3};
    case lex_not_equal: return {ast_op_not_equal, xpath_type_boolean, 3};
    case lex_less: return {ast_op_less, xpath_type_boolean, 4};
    case lex_greater: return {ast_op_greater, xpath_type_boolean, 4};
    case lex_less_or_equal: return {ast_op_less_or_equal, xpath_type_boolean, 4};
    case lex_greater_or_equal: return {ast_op_greater_or_equal, xpath_type_boolean, 4};
    case lex_plus: return {ast_op_add, xpath_type_number, 5};
    case lex_minus: return {ast_op_subtract, xpath_type_number, 5};
    case lex_multiply: return {ast_op_multiply, xpath_type_number, 6};
    case lex_union: return {ast_op_union, xpath_type_node_set, 7};
    default: return {};
    }
  }
};

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_expression_rec(XPathAstNode *lhs, int limit) // NOLINT(misc-no-recursion)
{
  binary_op_t bin_op = binary_op_t::parse(m_lexer);

  while(bin_op.asttype != ast_unknown && bin_op.precedence >= limit)
  {
    m_lexer.next();

    if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

    XPathAstNode *rhs = parse_path_or_unary_expression();
    if(rhs == nullptr) return nullptr;

    binary_op_t next_op = binary_op_t::parse(m_lexer);

    while(next_op.asttype != ast_unknown && next_op.precedence > bin_op.precedence)
    {
      rhs = parse_expression_rec(rhs, next_op.precedence);
      if(rhs == nullptr) return nullptr;

      next_op = binary_op_t::parse(m_lexer);
    }

    if(bin_op.asttype == ast_op_union
       && (lhs->rettype() != xpath_type_node_set || rhs->rettype() != xpath_type_node_set))
      return error("Union operator has to be applied to node sets");

    lhs = alloc_node(bin_op.asttype, bin_op.rettype, lhs, rhs);
    if(lhs == nullptr) return nullptr;

    bin_op = binary_op_t::parse(m_lexer);
  }

  return lhs;
}

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
LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse_expression(int limit) // NOLINT(misc-no-recursion)
{
  size_t old_depth = m_depth;

  if(++m_depth > Constants::kxpath_ast_depth_limit) return error_rec();

  XPathAstNode *astNode = parse_path_or_unary_expression();
  if(astNode == nullptr) return nullptr;

  astNode = parse_expression_rec(astNode, limit);
  m_depth = old_depth;

  return astNode;
}

LUMEX_PUBLIC_API
XPathParser::XPathParser(char_t const *query, // NOLINT(cppcoreguidelines-pro-type-member-init)
                         XPathVariableSet *variables, XPathAllocator *alloc, XPathParseResult *result)
    : m_alloc(alloc), m_lexer(query), m_query(query), m_variables(variables), m_result(result)
{}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse()
{
  XPathAstNode *astNode = parse_expression();
  if(astNode == nullptr) return nullptr;

  LUMEX_ASSERT(m_depth == 0);

  // check if there are unparsed tokens left
  if(m_lexer.current() != lex_eof) return error("Incorrect query");

  return astNode;
}

LUMEX_PUBLIC_API
XPathAstNode *
XPathParser::parse(char_t const *query, XPathVariableSet *variables, XPathAllocator *alloc, XPathParseResult *result)
{
  XPathParser parser(query, variables, alloc, result);
  return parser.parse();
}

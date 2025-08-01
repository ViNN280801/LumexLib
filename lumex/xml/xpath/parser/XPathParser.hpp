#ifndef LUMEX_XML_XPATH_PARSER_HPP
#define LUMEX_XML_XPATH_PARSER_HPP

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/xpath/XPathVariable.hpp"
#include "lumex/xml/xpath/ast/XPathAst.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Memory;
using namespace Lumex::Xml::XPath::Ast;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Parser
      {
        struct xpath_parser {
          XPathAllocator *_alloc;
          xpath_lexer _lexer;

          char_t const *_query;
          xpath_variable_set *_variables;

          xpath_parse_result *_result;

          char_t _scratch[32];

          size_t _depth;

          XPathAstNode *
          error(char const *message)
          {
            _result->error  = message;
            _result->offset = _lexer.current_pos() - _query;

            return nullptr;
          }

          XPathAstNode *
          error_oom()
          {
            LUMEX_ASSERT(_alloc->_error);
            *_alloc->_error = true;

            return nullptr;
          }

          XPathAstNode *
          error_rec()
          {
            return error("Exceeded maximum allowed query depth");
          }

          void *
          alloc_node()
          {
            return _alloc->allocate(sizeof(XPathAstNode));
          }

          XPathAstNode *
          alloc_node(ast_type_t type, xpath_value_type rettype, char_t const *value)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
          }

          XPathAstNode *
          alloc_node(ast_type_t type, xpath_value_type rettype, double value)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
          }

          XPathAstNode *
          alloc_node(ast_type_t type, xpath_value_type rettype, xpath_variable *value)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, rettype, value) : nullptr;
          }

          XPathAstNode *
          alloc_node(ast_type_t type, xpath_value_type rettype, XPathAstNode *left = nullptr,
                     XPathAstNode *right = nullptr)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, rettype, left, right) : nullptr;
          }

          XPathAstNode *
          alloc_node(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, left, axis, test, contents) : nullptr;
          }

          XPathAstNode *
          alloc_node(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test)
          {
            void *memory = alloc_node();
            return memory ? new(memory) XPathAstNode(type, left, right, test) : nullptr;
          }

          char_t const *
          alloc_string(xpath_lexer_string const &value)
          {
            if(!value.begin) return LUMEX_XML_TEXT("");

            size_t length = static_cast<size_t>(value.end - value.begin);

            char_t *c     = static_cast<char_t *>(_alloc->allocate((length + 1) * sizeof(char_t)));
            if(!c) return nullptr;

            memcpy(c, value.begin, length * sizeof(char_t));
            c[length] = 0;

            return c;
          }

          XPathAstNode *
          parse_function(xpath_lexer_string const &name, size_t argc, XPathAstNode *args[2])
          {
            switch(name.begin[0])
            {
            case 'b':
              if(name == LUMEX_XML_TEXT("boolean") && argc == 1)
                return alloc_node(ast_func_boolean, xpath_type_boolean, args[0]);

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
              if(name == LUMEX_XML_TEXT("id") && argc == 1)
                return alloc_node(ast_func_id, xpath_type_node_set, args[0]);

              break;

            case 'l':
              if(name == LUMEX_XML_TEXT("last") && argc == 0)
                return alloc_node(ast_func_last, xpath_type_number);
              else if(name == LUMEX_XML_TEXT("lang") && argc == 1)
                return alloc_node(ast_func_lang, xpath_type_boolean, args[0]);
              else if(name == LUMEX_XML_TEXT("local-name") && argc <= 1)
              {
                if(argc == 1 && args[0]->rettype() != xpath_type_node_set)
                  return error("Function has to be applied to node set");
                return alloc_node(argc == 0 ? ast_func_local_name_0 : ast_func_local_name_1, xpath_type_string,
                                  args[0]);
              }

              break;

            case 'n':
              if(name == LUMEX_XML_TEXT("name") && argc <= 1)
              {
                if(argc == 1 && args[0]->rettype() != xpath_type_node_set)
                  return error("Function has to be applied to node set");
                return alloc_node(argc == 0 ? ast_func_name_0 : ast_func_name_1, xpath_type_string, args[0]);
              }
              else if(name == LUMEX_XML_TEXT("namespace-uri") && argc <= 1)
              {
                if(argc == 1 && args[0]->rettype() != xpath_type_node_set)
                  return error("Function has to be applied to node set");
                return alloc_node(argc == 0 ? ast_func_namespace_uri_0 : ast_func_namespace_uri_1, xpath_type_string,
                                  args[0]);
              }
              else if(name == LUMEX_XML_TEXT("normalize-space") && argc <= 1)
                return alloc_node(argc == 0 ? ast_func_normalize_space_0 : ast_func_normalize_space_1,
                                  xpath_type_string, args[0], args[1]);
              else if(name == LUMEX_XML_TEXT("not") && argc == 1)
                return alloc_node(ast_func_not, xpath_type_boolean, args[0]);
              else if(name == LUMEX_XML_TEXT("number") && argc <= 1)
                return alloc_node(argc == 0 ? ast_func_number_0 : ast_func_number_1, xpath_type_number, args[0]);

              break;

            case 'p':
              if(name == LUMEX_XML_TEXT("position") && argc == 0)
                return alloc_node(ast_func_position, xpath_type_number);

              break;

            case 'r':
              if(name == LUMEX_XML_TEXT("round") && argc == 1)
                return alloc_node(ast_func_round, xpath_type_number, args[0]);

              break;

            case 's':
              if(name == LUMEX_XML_TEXT("string") && argc <= 1)
                return alloc_node(argc == 0 ? ast_func_string_0 : ast_func_string_1, xpath_type_string, args[0]);
              else if(name == LUMEX_XML_TEXT("string-length") && argc <= 1)
                return alloc_node(argc == 0 ? ast_func_string_length_0 : ast_func_string_length_1, xpath_type_number,
                                  args[0]);
              else if(name == LUMEX_XML_TEXT("starts-with") && argc == 2)
                return alloc_node(ast_func_starts_with, xpath_type_boolean, args[0], args[1]);
              else if(name == LUMEX_XML_TEXT("substring-before") && argc == 2)
                return alloc_node(ast_func_substring_before, xpath_type_string, args[0], args[1]);
              else if(name == LUMEX_XML_TEXT("substring-after") && argc == 2)
                return alloc_node(ast_func_substring_after, xpath_type_string, args[0], args[1]);
              else if(name == LUMEX_XML_TEXT("substring") && (argc == 2 || argc == 3))
                return alloc_node(argc == 2 ? ast_func_substring_2 : ast_func_substring_3, xpath_type_string, args[0],
                                  args[1]);
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

          axis_t
          parse_axis_name(xpath_lexer_string const &name, bool &specified)
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

          nodetest_t
          parse_node_test_type(xpath_lexer_string const &name)
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
          XPathAstNode *
          parse_primary_expression()
          {
            switch(_lexer.current())
            {
            case lex_var_ref: {
              xpath_lexer_string name = _lexer.contents();

              if(!_variables) return error("Unknown variable: variable set is not provided");

              xpath_variable *var = nullptr;
              if(!get_variable_scratch(_scratch, _variables, name.begin, name.end, &var)) return error_oom();

              if(!var) return error("Unknown variable: variable set does not contain the given name");

              _lexer.next();

              return alloc_node(ast_variable, var->type(), var);
            }

            case lex_open_brace: {
              _lexer.next();

              XPathAstNode *n = parse_expression();
              if(!n) return nullptr;

              if(_lexer.current() != lex_close_brace) return error("Expected ')' to match an opening '('");

              _lexer.next();

              return n;
            }

            case lex_quoted_string: {
              char_t const *value = alloc_string(_lexer.contents());
              if(!value) return nullptr;

              _lexer.next();

              return alloc_node(ast_string_constant, xpath_type_string, value);
            }

            case lex_number: {
              double value = 0;

              if(!convert_string_to_number_scratch(_scratch, _lexer.contents().begin, _lexer.contents().end, &value))
                return error_oom();

              _lexer.next();

              return alloc_node(ast_number_constant, xpath_type_number, value);
            }

            case lex_string: {
              XPathAstNode *args[2] = {nullptr};
              size_t argc                   = 0;

              xpath_lexer_string function   = _lexer.contents();
              _lexer.next();

              XPathAstNode *last_arg = nullptr;

              if(_lexer.current() != lex_open_brace) return error("Unrecognized function call");
              _lexer.next();

              size_t old_depth = _depth;

              while(_lexer.current() != lex_close_brace)
              {
                if(argc > 0)
                {
                  if(_lexer.current() != lex_comma) return error("No comma between function arguments");
                  _lexer.next();
                }

                if(++_depth > xpath_ast_depth_limit) return error_rec();

                XPathAstNode *n = parse_expression();
                if(!n) return nullptr;

                if(argc < 2)
                  args[argc] = n;
                else
                  last_arg->set_next(n);

                argc++;
                last_arg = n;
              }

              _lexer.next();

              _depth = old_depth;

              return parse_function(function, argc, args);
            }

            default: return error("Unrecognizable primary expression");
            }
          }

          // FilterExpr ::= PrimaryExpr | FilterExpr Predicate
          // Predicate ::= '[' PredicateExpr ']'
          // PredicateExpr ::= Expr
          XPathAstNode *
          parse_filter_expression()
          {
            XPathAstNode *n = parse_primary_expression();
            if(!n) return nullptr;

            size_t old_depth = _depth;

            while(_lexer.current() == lex_open_square_brace)
            {
              _lexer.next();

              if(++_depth > xpath_ast_depth_limit) return error_rec();

              if(n->rettype() != xpath_type_node_set) return error("Predicate has to be applied to node set");

              XPathAstNode *expr = parse_expression();
              if(!expr) return nullptr;

              n = alloc_node(ast_filter, n, expr, predicate_default);
              if(!n) return nullptr;

              if(_lexer.current() != lex_close_square_brace) return error("Expected ']' to match an opening '['");

              _lexer.next();
            }

            _depth = old_depth;

            return n;
          }

          // Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
          // AxisSpecifier ::= AxisName '::' | '@'?
          // NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
          // NameTest ::= '*' | NCName ':' '*' | QName
          // AbbreviatedStep ::= '.' | '..'
          XPathAstNode *
          parse_step(XPathAstNode *set)
          {
            if(set && set->rettype() != xpath_type_node_set) return error("Step has to be applied to node set");

            bool axis_specified = false;
            axis_t axis         = axis_child; // implied child axis

            if(_lexer.current() == lex_axis_attribute)
            {
              axis           = axis_attribute;
              axis_specified = true;

              _lexer.next();
            }
            else if(_lexer.current() == lex_dot)
            {
              _lexer.next();

              if(_lexer.current() == lex_open_square_brace)
                return error("Predicates are not allowed after an abbreviated step");

              return alloc_node(ast_step, set, axis_self, nodetest_type_node, nullptr);
            }
            else if(_lexer.current() == lex_double_dot)
            {
              _lexer.next();

              if(_lexer.current() == lex_open_square_brace)
                return error("Predicates are not allowed after an abbreviated step");

              return alloc_node(ast_step, set, axis_parent, nodetest_type_node, nullptr);
            }

            nodetest_t nt_type = nodetest_none;
            xpath_lexer_string nt_name;

            if(_lexer.current() == lex_string)
            {
              // node name test
              nt_name = _lexer.contents();
              _lexer.next();

              // was it an axis name?
              if(_lexer.current() == lex_double_colon)
              {
                // parse axis name
                if(axis_specified) return error("Two axis specifiers in one step");

                axis = parse_axis_name(nt_name, axis_specified);

                if(!axis_specified) return error("Unknown axis");

                // read actual node test
                _lexer.next();

                if(_lexer.current() == lex_multiply)
                {
                  nt_type = nodetest_all;
                  nt_name = xpath_lexer_string();
                  _lexer.next();
                }
                else if(_lexer.current() == lex_string)
                {
                  nt_name = _lexer.contents();
                  _lexer.next();
                }
                else { return error("Unrecognized node test"); }
              }

              if(nt_type == nodetest_none)
              {
                // node type test or processing-instruction
                if(_lexer.current() == lex_open_brace)
                {
                  _lexer.next();

                  if(_lexer.current() == lex_close_brace)
                  {
                    _lexer.next();

                    nt_type = parse_node_test_type(nt_name);

                    if(nt_type == nodetest_none) return error("Unrecognized node type");

                    nt_name = xpath_lexer_string();
                  }
                  else if(nt_name == LUMEX_XML_TEXT("processing-instruction"))
                  {
                    if(_lexer.current() != lex_quoted_string)
                      return error("Only literals are allowed as arguments to processing-instruction()");

                    nt_type = nodetest_pi;
                    nt_name = _lexer.contents();
                    _lexer.next();

                    if(_lexer.current() != lex_close_brace)
                      return error("Unmatched brace near processing-instruction()");
                    _lexer.next();
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
            else if(_lexer.current() == lex_multiply)
            {
              nt_type = nodetest_all;
              _lexer.next();
            }
            else { return error("Unrecognized node test"); }

            char_t const *nt_name_copy = alloc_string(nt_name);
            if(!nt_name_copy) return nullptr;

            XPathAstNode *n = alloc_node(ast_step, set, axis, nt_type, nt_name_copy);
            if(!n) return nullptr;

            size_t old_depth           = _depth;

            XPathAstNode *last = nullptr;

            while(_lexer.current() == lex_open_square_brace)
            {
              _lexer.next();

              if(++_depth > xpath_ast_depth_limit) return error_rec();

              XPathAstNode *expr = parse_expression();
              if(!expr) return nullptr;

              XPathAstNode *pred = alloc_node(ast_predicate, nullptr, expr, predicate_default);
              if(!pred) return nullptr;

              if(_lexer.current() != lex_close_square_brace) return error("Expected ']' to match an opening '['");
              _lexer.next();

              if(last)
                last->set_next(pred);
              else
                n->set_right(pred);

              last = pred;
            }

            _depth = old_depth;

            return n;
          }

          // RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
          XPathAstNode *
          parse_relative_location_path(XPathAstNode *set)
          {
            XPathAstNode *n = parse_step(set);
            if(!n) return nullptr;

            size_t old_depth = _depth;

            while(_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
            {
              lexeme_t l = _lexer.current();
              _lexer.next();

              if(l == lex_double_slash)
              {
                n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, nullptr);
                if(!n) return nullptr;

                ++_depth;
              }

              if(++_depth > xpath_ast_depth_limit) return error_rec();

              n = parse_step(n);
              if(!n) return nullptr;
            }

            _depth = old_depth;

            return n;
          }

          // LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
          // AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
          XPathAstNode *
          parse_location_path()
          {
            if(_lexer.current() == lex_slash)
            {
              _lexer.next();

              XPathAstNode *n = alloc_node(ast_step_root, xpath_type_node_set);
              if(!n) return nullptr;

              // relative location path can start from axis_attribute, dot, double_dot, multiply and string lexemes; any
              // other lexeme means standalone root path
              lexeme_t l = _lexer.current();

              if(l == lex_string || l == lex_axis_attribute || l == lex_dot || l == lex_double_dot || l == lex_multiply)
                return parse_relative_location_path(n);
              else
                return n;
            }
            else if(_lexer.current() == lex_double_slash)
            {
              _lexer.next();

              XPathAstNode *n = alloc_node(ast_step_root, xpath_type_node_set);
              if(!n) return nullptr;

              n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, nullptr);
              if(!n) return nullptr;

              return parse_relative_location_path(n);
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
          XPathAstNode *
          parse_path_or_unary_expression()
          {
            // Clarification.
            // PathExpr begins with either LocationPath or FilterExpr.
            // FilterExpr begins with PrimaryExpr
            // PrimaryExpr begins with '$' in case of it being a variable reference,
            // '(' in case of it being an expression, string literal, number constant or
            // function call.
            if(_lexer.current() == lex_var_ref || _lexer.current() == lex_open_brace
               || _lexer.current() == lex_quoted_string || _lexer.current() == lex_number
               || _lexer.current() == lex_string)
            {
              if(_lexer.current() == lex_string)
              {
                // This is either a function call, or not - if not, we shall proceed with location path
                char_t const *state = _lexer.state();

                while(PUGI_IMPL_IS_CHARTYPE(*state, ct_space)) ++state;

                if(*state != '(') return parse_location_path();

                // This looks like a function call; however this still can be a node-test. Check it.
                if(parse_node_test_type(_lexer.contents()) != nodetest_none) return parse_location_path();
              }

              XPathAstNode *n = parse_filter_expression();
              if(!n) return nullptr;

              if(_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
              {
                lexeme_t l = _lexer.current();
                _lexer.next();

                if(l == lex_double_slash)
                {
                  if(n->rettype() != xpath_type_node_set) return error("Step has to be applied to node set");

                  n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, nullptr);
                  if(!n) return nullptr;
                }

                // select from location path
                return parse_relative_location_path(n);
              }

              return n;
            }
            else if(_lexer.current() == lex_minus)
            {
              _lexer.next();

              // precedence 7+ - only parses union expressions
              XPathAstNode *n = parse_expression(7);
              if(!n) return nullptr;

              return alloc_node(ast_op_negate, xpath_type_number, n);
            }
            else { return parse_location_path(); }
          }

          struct binary_op_t {
            ast_type_t asttype;
            xpath_value_type rettype;
            int precedence;

            binary_op_t() : asttype(ast_unknown), rettype(xpath_type_none), precedence(0) {}

            binary_op_t(ast_type_t asttype_, xpath_value_type rettype_, int precedence_)
                : asttype(asttype_), rettype(rettype_), precedence(precedence_)
            {}

            static binary_op_t
            parse(xpath_lexer &lexer)
            {
              switch(lexer.current())
              {
              case lex_string:
                if(lexer.contents() == LUMEX_XML_TEXT("or"))
                  return binary_op_t(ast_op_or, xpath_type_boolean, 1);
                else if(lexer.contents() == LUMEX_XML_TEXT("and"))
                  return binary_op_t(ast_op_and, xpath_type_boolean, 2);
                else if(lexer.contents() == LUMEX_XML_TEXT("div"))
                  return binary_op_t(ast_op_divide, xpath_type_number, 6);
                else if(lexer.contents() == LUMEX_XML_TEXT("mod"))
                  return binary_op_t(ast_op_mod, xpath_type_number, 6);
                else
                  return binary_op_t();

              case lex_equal: return binary_op_t(ast_op_equal, xpath_type_boolean, 3);

              case lex_not_equal: return binary_op_t(ast_op_not_equal, xpath_type_boolean, 3);

              case lex_less: return binary_op_t(ast_op_less, xpath_type_boolean, 4);

              case lex_greater: return binary_op_t(ast_op_greater, xpath_type_boolean, 4);

              case lex_less_or_equal: return binary_op_t(ast_op_less_or_equal, xpath_type_boolean, 4);

              case lex_greater_or_equal: return binary_op_t(ast_op_greater_or_equal, xpath_type_boolean, 4);

              case lex_plus: return binary_op_t(ast_op_add, xpath_type_number, 5);

              case lex_minus: return binary_op_t(ast_op_subtract, xpath_type_number, 5);

              case lex_multiply: return binary_op_t(ast_op_multiply, xpath_type_number, 6);

              case lex_union: return binary_op_t(ast_op_union, xpath_type_node_set, 7);

              default: return binary_op_t();
              }
            }
          };

          XPathAstNode *
          parse_expression_rec(XPathAstNode *lhs, int limit)
          {
            binary_op_t op = binary_op_t::parse(_lexer);

            while(op.asttype != ast_unknown && op.precedence >= limit)
            {
              _lexer.next();

              if(++_depth > xpath_ast_depth_limit) return error_rec();

              XPathAstNode *rhs = parse_path_or_unary_expression();
              if(!rhs) return nullptr;

              binary_op_t nextop = binary_op_t::parse(_lexer);

              while(nextop.asttype != ast_unknown && nextop.precedence > op.precedence)
              {
                rhs = parse_expression_rec(rhs, nextop.precedence);
                if(!rhs) return nullptr;

                nextop = binary_op_t::parse(_lexer);
              }

              if(op.asttype == ast_op_union
                 && (lhs->rettype() != xpath_type_node_set || rhs->rettype() != xpath_type_node_set))
                return error("Union operator has to be applied to node sets");

              lhs = alloc_node(op.asttype, op.rettype, lhs, rhs);
              if(!lhs) return nullptr;

              op = binary_op_t::parse(_lexer);
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
          XPathAstNode *
          parse_expression(int limit = 0)
          {
            size_t old_depth = _depth;

            if(++_depth > xpath_ast_depth_limit) return error_rec();

            XPathAstNode *n = parse_path_or_unary_expression();
            if(!n) return nullptr;

            n      = parse_expression_rec(n, limit);

            _depth = old_depth;

            return n;
          }

          xpath_parser(char_t const *query, xpath_variable_set *variables, XPathAllocator *alloc,
                       xpath_parse_result *result)
              : _alloc(alloc), _lexer(query), _query(query), _variables(variables), _result(result), _depth(0)
          {}

          XPathAstNode *
          parse()
          {
            XPathAstNode *n = parse_expression();
            if(!n) return nullptr;

            LUMEX_ASSERT(_depth == 0);

            // check if there are unparsed tokens left
            if(_lexer.current() != lex_eof) return error("Incorrect query");

            return n;
          }

          static XPathAstNode *
          parse(char_t const *query, xpath_variable_set *variables, XPathAllocator *alloc,
                xpath_parse_result *result)
          {
            xpath_parser parser(query, variables, alloc, result);

            return parser.parse();
          }
        };
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSER_HPP

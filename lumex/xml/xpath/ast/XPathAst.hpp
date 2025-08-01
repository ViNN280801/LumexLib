#ifndef LUMEX_XML_XPATH_AST_HPP
#define LUMEX_XML_XPATH_AST_HPP

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/xpath/context/XPathContext.hpp"
#include "lumex/xml/xpath/memory/XPathStack.hpp"
#include "lumex/xml/xpath/node/XPathNodeSet.hpp"
#include "lumex/xml/xpath/variable/XPathVariable.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Variable;
using namespace Lumex::Xml::XPath::Memory;
using namespace Lumex::Xml::XPath::Context;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Ast
      {
        class XPathAstNode
        {
        private:
          // node type
          char m_type;
          char _rettype;

          // for ast_step
          char _axis;

          // for ast_step/ast_predicate/ast_filter
          char _test;

          // tree node structure
          XPathAstNode *_left;
          XPathAstNode *_right;
          XPathAstNode *_next;

          union {
            char_t const *string;
            double number;
            XPathVariable *variable;
            char_t const *nodetest;
            unsigned char const *table;
          } _data;

          XPathAstNode(XPathAstNode const &);
          XPathAstNode &operator=(XPathAstNode const &);

          template <class Comp>
          static bool
          compare_eq(XPathAstNode *lhs, XPathAstNode *rhs, XPathContext const &c, XPathStack const &stack,
                     Comp const &comp)
          {
            xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

            if(lt != xpath_type_node_set && rt != xpath_type_node_set)
            {
              if(lt == xpath_type_boolean || rt == xpath_type_boolean)
                return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
              else if(lt == xpath_type_number || rt == xpath_type_number)
                return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
              else if(lt == xpath_type_string || rt == xpath_type_string)
              {
                XPathAllocatorCapture cr(stack.result);

                xpath_string ls = lhs->eval_string(c, stack);
                xpath_string rs = rhs->eval_string(c, stack);

                return comp(ls, rs);
              }
            }
            else if(lt == xpath_type_node_set && rt == xpath_type_node_set)
            {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
              XPathNodeSetRaw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

              for(XPathNode const *li = ls.begin(); li != ls.end(); ++li)
                for(XPathNode const *ri = rs.begin(); ri != rs.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);

                  if(comp(string_value(*li, stack.result), string_value(*ri, stack.result))) return true;
                }

              return false;
            }
            else
            {
              if(lt == xpath_type_node_set)
              {
                swap(lhs, rhs);
                swap(lt, rt);
              }

              if(lt == xpath_type_boolean)
                return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
              else if(lt == xpath_type_number)
              {
                XPathAllocatorCapture cr(stack.result);

                double l           = lhs->eval_number(c, stack);
                XPathNodeSetRaw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                for(XPathNode const *ri = rs.begin(); ri != rs.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);

                  if(comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
                }

                return false;
              }
              else if(lt == xpath_type_string)
              {
                XPathAllocatorCapture cr(stack.result);

                xpath_string l     = lhs->eval_string(c, stack);
                XPathNodeSetRaw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                for(XPathNode const *ri = rs.begin(); ri != rs.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);

                  if(comp(l, string_value(*ri, stack.result))) return true;
                }

                return false;
              }
            }

            LUMEX_ASSERT(false && "Wrong types"); // unreachable
            return false;
          }

          static bool
          eval_once(XPathNodeSet::type_t type, Types::nodeset_eval_t eval)
          {
            return type == XPathNodeSet::type_sorted ? eval != nodeset_eval_all : eval == nodeset_eval_any;
          }

          template <class Comp>
          static bool
          compare_rel(XPathAstNode *lhs, XPathAstNode *rhs, XPathContext const &c, XPathStack const &stack,
                      Comp const &comp)
          {
            xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

            if(lt != xpath_type_node_set && rt != xpath_type_node_set)
              return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
            else if(lt == xpath_type_node_set && rt == xpath_type_node_set)
            {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
              XPathNodeSetRaw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

              for(XPathNode const *li = ls.begin(); li != ls.end(); ++li)
              {
                XPathAllocatorCapture cri(stack.result);

                double l = convert_string_to_number(string_value(*li, stack.result).c_str());

                for(XPathNode const *ri = rs.begin(); ri != rs.end(); ++ri)
                {
                  XPathAllocatorCapture crii(stack.result);

                  if(comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
                }
              }

              return false;
            }
            else if(lt != xpath_type_node_set && rt == xpath_type_node_set)
            {
              XPathAllocatorCapture cr(stack.result);

              double l           = lhs->eval_number(c, stack);
              XPathNodeSetRaw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

              for(XPathNode const *ri = rs.begin(); ri != rs.end(); ++ri)
              {
                XPathAllocatorCapture cri(stack.result);

                if(comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
              }

              return false;
            }
            else if(lt == xpath_type_node_set && rt != xpath_type_node_set)
            {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
              double r           = rhs->eval_number(c, stack);

              for(XPathNode const *li = ls.begin(); li != ls.end(); ++li)
              {
                XPathAllocatorCapture cri(stack.result);

                if(comp(convert_string_to_number(string_value(*li, stack.result).c_str()), r)) return true;
              }

              return false;
            }
            else
            {
              LUMEX_ASSERT(false && "Wrong types"); // unreachable
              return false;
            }
          }

          static void
          apply_predicate_boolean(XPathNodeSetRaw &ns, size_t first, XPathAstNode *expr, XPathStack const &stack,
                                  bool once)
          {
            LUMEX_ASSERT(ns.size() >= first);
            LUMEX_ASSERT(expr->rettype() != xpath_type_number);

            size_t i         = 1;
            size_t size      = ns.size() - first;

            XPathNode *last = ns.begin() + first;

            // remove_if... or well, sort of
            for(XPathNode *it = last; it != ns.end(); ++it, ++i)
            {
              XPathContext c(*it, i, size);

              if(expr->eval_boolean(c, stack))
              {
                *last++ = *it;

                if(once) break;
              }
            }

            ns.truncate(last);
          }

          static void
          apply_predicate_number(XPathNodeSetRaw &ns, size_t first, XPathAstNode *expr, XPathStack const &stack,
                                 bool once)
          {
            LUMEX_ASSERT(ns.size() >= first);
            LUMEX_ASSERT(expr->rettype() == xpath_type_number);

            size_t i         = 1;
            size_t size      = ns.size() - first;

            XPathNode *last = ns.begin() + first;

            // remove_if... or well, sort of
            for(XPathNode *it = last; it != ns.end(); ++it, ++i)
            {
              XPathContext c(*it, i, size);

              if(expr->eval_number(c, stack) == static_cast<double>(i))
              {
                *last++ = *it;

                if(once) break;
              }
            }

            ns.truncate(last);
          }

          static void
          apply_predicate_number_const(XPathNodeSetRaw &ns, size_t first, XPathAstNode *expr, XPathStack const &stack)
          {
            LUMEX_ASSERT(ns.size() >= first);
            LUMEX_ASSERT(expr->rettype() == xpath_type_number);

            size_t size      = ns.size() - first;

            XPathNode *last = ns.begin() + first;

            XPathNode cn;
            XPathContext c(cn, 1, size);

            double er = expr->eval_number(c, stack);

            if(er >= 1.0 && er <= static_cast<double>(size))
            {
              size_t eri = static_cast<size_t>(er);

              if(er == static_cast<double>(eri))
              {
                XPathNode r = last[eri - 1];

                *last++      = r;
              }
            }

            ns.truncate(last);
          }

          void
          apply_predicate(XPathNodeSetRaw &ns, size_t first, XPathStack const &stack, bool once)
          {
            if(ns.size() == first) return;

            LUMEX_ASSERT(m_type == ast_filter || m_type == ast_predicate);

            if(_test == predicate_constant || _test == predicate_constant_one)
              apply_predicate_number_const(ns, first, _right, stack);
            else if(_right->rettype() == xpath_type_number)
              apply_predicate_number(ns, first, _right, stack, once);
            else
              apply_predicate_boolean(ns, first, _right, stack, once);
          }

          void
          apply_predicates(XPathNodeSetRaw &ns, size_t first, XPathStack const &stack, Types::nodeset_eval_t eval)
          {
            if(ns.size() == first) return;

            bool last_once = eval_once(ns.type(), eval);

            for(XPathAstNode *pred = _right; pred; pred = pred->_next)
              pred->apply_predicate(ns, first, stack, !pred->_next && last_once);
          }

          bool
          step_push(XPathNodeSetRaw &ns, xml_attribute_struct *a, xml_node_struct *parent, xpath_allocator *alloc)
          {
            LUMEX_ASSERT(a);

            char_t const *name = a->name ? a->name + 0 : LUMEX_XML_TEXT("");

            switch(_test)
            {
            case nodetest_name:
              if(strequal(name, _data.nodetest) && is_xpath_attribute(name))
              {
                ns.push_back(XPathNode(xml_attribute(a), xml_node(parent)), alloc);
                return true;
              }
              break;

            case nodetest_type_node:
            case nodetest_all:
              if(is_xpath_attribute(name))
              {
                ns.push_back(XPathNode(xml_attribute(a), xml_node(parent)), alloc);
                return true;
              }
              break;

            case nodetest_all_in_namespace:
              if(starts_with(name, _data.nodetest) && is_xpath_attribute(name))
              {
                ns.push_back(XPathNode(xml_attribute(a), xml_node(parent)), alloc);
                return true;
              }
              break;

            default:;
            }

            return false;
          }

          bool
          step_push(XPathNodeSetRaw &ns, xml_node_struct *n, xpath_allocator *alloc)
          {
            LUMEX_ASSERT(n);

            xml_node_type type = PUGI_IMPL_NODETYPE(n);

            switch(_test)
            {
            case nodetest_name:
              if(type == node_element && n->name && strequal(n->name, _data.nodetest))
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_type_node: ns.push_back(xml_node(n), alloc); return true;

            case nodetest_type_comment:
              if(type == node_comment)
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_type_text:
              if(type == node_pcdata || type == node_cdata)
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_type_pi:
              if(type == node_pi)
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_pi:
              if(type == node_pi && n->name && strequal(n->name, _data.nodetest))
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_all:
              if(type == node_element)
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            case nodetest_all_in_namespace:
              if(type == node_element && n->name && starts_with(n->name, _data.nodetest))
              {
                ns.push_back(xml_node(n), alloc);
                return true;
              }
              break;

            default: LUMEX_ASSERT(false && "Unknown axis"); // unreachable
            }

            return false;
          }

          template <class T>
          void
          step_fill(XPathNodeSetRaw &ns, xml_node_struct *n, xpath_allocator *alloc, bool once, T)
          {
            axis_t const axis = T::axis;

            switch(axis)
            {
            case axis_attribute: {
              for(xml_attribute_struct *a = n->first_attribute; a; a = a->next_attribute)
                if(step_push(ns, a, n, alloc) & once) return;

              break;
            }

            case axis_child: {
              for(xml_node_struct *c = n->first_child; c; c = c->next_sibling)
                if(step_push(ns, c, alloc) & once) return;

              break;
            }

            case axis_descendant:
            case axis_descendant_or_self: {
              if(axis == axis_descendant_or_self)
                if(step_push(ns, n, alloc) & once) return;

              xml_node_struct *cur = n->first_child;

              while(cur)
              {
                if(step_push(ns, cur, alloc) & once) return;

                if(cur->first_child)
                  cur = cur->first_child;
                else
                {
                  while(!cur->next_sibling)
                  {
                    cur = cur->parent;

                    if(cur == n) return;
                  }

                  cur = cur->next_sibling;
                }
              }

              break;
            }

            case axis_following_sibling: {
              for(xml_node_struct *c = n->next_sibling; c; c = c->next_sibling)
                if(step_push(ns, c, alloc) & once) return;

              break;
            }

            case axis_preceding_sibling: {
              for(xml_node_struct *c = n->prev_sibling_c; c->next_sibling; c = c->prev_sibling_c)
                if(step_push(ns, c, alloc) & once) return;

              break;
            }

            case axis_following: {
              xml_node_struct *cur = n;

              // exit from this node so that we don't include descendants
              while(!cur->next_sibling)
              {
                cur = cur->parent;

                if(!cur) return;
              }

              cur = cur->next_sibling;

              while(cur)
              {
                if(step_push(ns, cur, alloc) & once) return;

                if(cur->first_child)
                  cur = cur->first_child;
                else
                {
                  while(!cur->next_sibling)
                  {
                    cur = cur->parent;

                    if(!cur) return;
                  }

                  cur = cur->next_sibling;
                }
              }

              break;
            }

            case axis_preceding: {
              xml_node_struct *cur = n;

              // exit from this node so that we don't include descendants
              while(!cur->prev_sibling_c->next_sibling)
              {
                cur = cur->parent;

                if(!cur) return;
              }

              cur = cur->prev_sibling_c;

              while(cur)
              {
                if(cur->first_child)
                  cur = cur->first_child->prev_sibling_c;
                else
                {
                  // leaf node, can't be ancestor
                  if(step_push(ns, cur, alloc) & once) return;

                  while(!cur->prev_sibling_c->next_sibling)
                  {
                    cur = cur->parent;

                    if(!cur) return;

                    if(!node_is_ancestor(cur, n))
                      if(step_push(ns, cur, alloc) & once) return;
                  }

                  cur = cur->prev_sibling_c;
                }
              }

              break;
            }

            case axis_ancestor:
            case axis_ancestor_or_self: {
              if(axis == axis_ancestor_or_self)
                if(step_push(ns, n, alloc) & once) return;

              xml_node_struct *cur = n->parent;

              while(cur)
              {
                if(step_push(ns, cur, alloc) & once) return;

                cur = cur->parent;
              }

              break;
            }

            case axis_self: {
              step_push(ns, n, alloc);

              break;
            }

            case axis_parent: {
              if(n->parent) step_push(ns, n->parent, alloc);

              break;
            }

            default: LUMEX_ASSERT(false && "Unimplemented axis"); // unreachable
            }
          }

          template <class T>
          void
          step_fill(XPathNodeSetRaw &ns, xml_attribute_struct *a, xml_node_struct *p, xpath_allocator *alloc, bool once,
                    T v)
          {
            axis_t const axis = T::axis;

            switch(axis)
            {
            case axis_ancestor:
            case axis_ancestor_or_self: {
              if(axis == axis_ancestor_or_self
                 && _test == nodetest_type_node) // reject attributes based on principal node type test
                if(step_push(ns, a, p, alloc) & once) return;

              xml_node_struct *cur = p;

              while(cur)
              {
                if(step_push(ns, cur, alloc) & once) return;

                cur = cur->parent;
              }

              break;
            }

            case axis_descendant_or_self:
            case axis_self: {
              if(_test == nodetest_type_node) // reject attributes based on principal node type test
                step_push(ns, a, p, alloc);

              break;
            }

            case axis_following: {
              xml_node_struct *cur = p;

              while(cur)
              {
                if(cur->first_child)
                  cur = cur->first_child;
                else
                {
                  while(!cur->next_sibling)
                  {
                    cur = cur->parent;

                    if(!cur) return;
                  }

                  cur = cur->next_sibling;
                }

                if(step_push(ns, cur, alloc) & once) return;
              }

              break;
            }

            case axis_parent: {
              step_push(ns, p, alloc);

              break;
            }

            case axis_preceding: {
              // preceding:: axis does not include attribute nodes and attribute ancestors (they are the same as
              // parent's ancestors), so we can reuse node preceding
              step_fill(ns, p, alloc, once, v);
              break;
            }

            default: LUMEX_ASSERT(false && "Unimplemented axis"); // unreachable
            }
          }

          template <class T>
          void
          step_fill(XPathNodeSetRaw &ns, XPathNode const &xn, xpath_allocator *alloc, bool once, T v)
          {
            axis_t const axis = T::axis;
            bool const axis_has_attributes
              = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_descendant_or_self
                 || axis == axis_following || axis == axis_parent || axis == axis_preceding || axis == axis_self);

            if(xn.node())
              step_fill(ns, xn.node().internal_object(), alloc, once, v);
            else if(axis_has_attributes && xn.attribute() && xn.parent())
              step_fill(ns, xn.attribute().internal_object(), xn.parent().internal_object(), alloc, once, v);
          }

          template <class T>
          XPathNodeSetRaw
          step_do(XPathContext const &c, XPathStack const &stack, Types::nodeset_eval_t eval, T v)
          {
            axis_t const axis       = T::axis;
            bool const axis_reverse = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_preceding
                                       || axis == axis_preceding_sibling);
            XPathNodeSet::type_t const axis_type
              = axis_reverse ? XPathNodeSet::type_sorted_reverse : XPathNodeSet::type_sorted;

            bool once = (axis == axis_attribute && _test == nodetest_name) || (!_right && eval_once(axis_type, eval)) ||
                        // coverity[mixed_enums]
                        (_right && !_right->_next && _right->_test == predicate_constant_one);

            XPathNodeSetRaw ns;
            ns.set_type(axis_type);

            if(_left)
            {
              XPathNodeSetRaw s = _left->eval_node_set(c, stack, nodeset_eval_all);

              // self axis preserves the original order
              if(axis == axis_self) ns.set_type(s.type());

              for(XPathNode const *it = s.begin(); it != s.end(); ++it)
              {
                size_t size = ns.size();

                // in general, all axes generate elements in a particular order, but there is no order guarantee if axis
                // is applied to two nodes
                if(axis != axis_self && size != 0) ns.set_type(XPathNodeSet::type_unsorted);

                step_fill(ns, *it, stack.result, once, v);
                if(_right) apply_predicates(ns, size, stack, eval);
              }
            }
            else
            {
              step_fill(ns, c.n, stack.result, once, v);
              if(_right) apply_predicates(ns, 0, stack, eval);
            }

            // child, attribute and self axes always generate unique set of nodes
            // for other axis, if the set stayed sorted, it stayed unique because the traversal algorithms do not visit
            // the same node twice
            if(axis != axis_child && axis != axis_attribute && axis != axis_self
               && ns.type() == XPathNodeSet::type_unsorted)
              ns.remove_duplicates(stack.temp);

            return ns;
          }

        public:
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, char_t const *value)
              : m_type(static_cast<char>(type)),
                _rettype(static_cast<char>(rettype_)),
                _axis(0),
                _test(0),
                _left(nullptr),
                _right(nullptr),
                _next(nullptr)
          {
            LUMEX_ASSERT(type == ast_string_constant);
            _data.string = value;
          }

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, double value)
              : m_type(static_cast<char>(type)),
                _rettype(static_cast<char>(rettype_)),
                _axis(0),
                _test(0),
                _left(nullptr),
                _right(nullptr),
                _next(nullptr)
          {
            LUMEX_ASSERT(type == ast_number_constant);
            _data.number = value;
          }

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathVariable *value)
              : m_type(static_cast<char>(type)),
                _rettype(static_cast<char>(rettype_)),
                _axis(0),
                _test(0),
                _left(nullptr),
                _right(nullptr),
                _next(nullptr)
          {
            LUMEX_ASSERT(type == ast_variable);
            _data.variable = value;
          }

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathAstNode *left = nullptr,
                       XPathAstNode *right = nullptr)
              : m_type(static_cast<char>(type)),
                _rettype(static_cast<char>(rettype_)),
                _axis(0),
                _test(0),
                _left(left),
                _right(right),
                _next(nullptr)
          {}

          XPathAstNode(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents)
              : m_type(static_cast<char>(type)),
                _rettype(xpath_type_node_set),
                _axis(static_cast<char>(axis)),
                _test(static_cast<char>(test)),
                _left(left),
                _right(nullptr),
                _next(nullptr)
          {
            LUMEX_ASSERT(type == ast_step);
            _data.nodetest = contents;
          }

          XPathAstNode(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test)
              : m_type(static_cast<char>(type)),
                _rettype(xpath_type_node_set),
                _axis(0),
                _test(static_cast<char>(test)),
                _left(left),
                _right(right),
                _next(nullptr)
          {
            LUMEX_ASSERT(type == ast_filter || type == ast_predicate);
          }

          void
          set_next(XPathAstNode *value)
          {
            _next = value;
          }

          void
          set_right(XPathAstNode *value)
          {
            _right = value;
          }

          bool
          eval_boolean(XPathContext const &c, XPathStack const &stack)
          {
            switch(m_type)
            {
            case ast_op_or: return _left->eval_boolean(c, stack) || _right->eval_boolean(c, stack);

            case ast_op_and: return _left->eval_boolean(c, stack) && _right->eval_boolean(c, stack);

            case ast_op_equal: return compare_eq(_left, _right, c, stack, equal_to());

            case ast_op_not_equal: return compare_eq(_left, _right, c, stack, not_equal_to());

            case ast_op_less: return compare_rel(_left, _right, c, stack, less());

            case ast_op_greater: return compare_rel(_right, _left, c, stack, less());

            case ast_op_less_or_equal: return compare_rel(_left, _right, c, stack, less_equal());

            case ast_op_greater_or_equal: return compare_rel(_right, _left, c, stack, less_equal());

            case ast_func_starts_with: {
              XPathAllocatorCapture cr(stack.result);

              xpath_string lr = _left->eval_string(c, stack);
              xpath_string rr = _right->eval_string(c, stack);

              return starts_with(lr.c_str(), rr.c_str());
            }

            case ast_func_contains: {
              XPathAllocatorCapture cr(stack.result);

              xpath_string lr = _left->eval_string(c, stack);
              xpath_string rr = _right->eval_string(c, stack);

              return find_substring(lr.c_str(), rr.c_str()) != nullptr;
            }

            case ast_func_boolean: return _left->eval_boolean(c, stack);

            case ast_func_not: return !_left->eval_boolean(c, stack);

            case ast_func_true: return true;

            case ast_func_false: return false;

            case ast_func_lang: {
              if(c.n.attribute()) return false;

              XPathAllocatorCapture cr(stack.result);

              xpath_string lang = _left->eval_string(c, stack);

              for(xml_node n = c.n.node(); n; n = n.parent())
              {
                xml_attribute a = n.attribute(LUMEX_XML_TEXT("xml:lang"));

                if(a)
                {
                  char_t const *value = a.value();

                  // strnicmp / strncasecmp is not portable
                  for(char_t const *lit = lang.c_str(); *lit; ++lit)
                  {
                    if(tolower_ascii(*lit) != tolower_ascii(*value)) return false;
                    ++value;
                  }

                  return *value == 0 || *value == '-';
                }
              }

              return false;
            }

            case ast_opt_compare_attribute: {
              char_t const *value
                = (_right->m_type == ast_string_constant) ? _right->_data.string : _right->_data.variable->get_string();

              xml_attribute attr = c.n.node().attribute(_left->_data.nodetest);

              return attr && strequal(attr.value(), value) && is_xpath_attribute(attr.name());
            }

            case ast_variable: {
              LUMEX_ASSERT(_rettype == _data.variable->type());

              if(_rettype == xpath_type_boolean) return _data.variable->get_boolean();

              // variable needs to be converted to the correct type, this is handled by the fallthrough block below
              break;
            }

            default:;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch(_rettype)
            {
            case xpath_type_number: return convert_number_to_boolean(eval_number(c, stack));

            case xpath_type_string: {
              XPathAllocatorCapture cr(stack.result);

              return !eval_string(c, stack).empty();
            }

            case xpath_type_node_set: {
              XPathAllocatorCapture cr(stack.result);

              return !eval_node_set(c, stack, nodeset_eval_any).empty();
            }

            default:
              LUMEX_ASSERT(false && "Wrong expression for return type boolean"); // unreachable
              return false;
            }
          }

          double
          eval_number(XPathContext const &c, XPathStack const &stack)
          {
            switch(m_type)
            {
            case ast_op_add: return _left->eval_number(c, stack) + _right->eval_number(c, stack);

            case ast_op_subtract: return _left->eval_number(c, stack) - _right->eval_number(c, stack);

            case ast_op_multiply: return _left->eval_number(c, stack) * _right->eval_number(c, stack);

            case ast_op_divide: return _left->eval_number(c, stack) / _right->eval_number(c, stack);

            case ast_op_mod: return fmod(_left->eval_number(c, stack), _right->eval_number(c, stack));

            case ast_op_negate: return -_left->eval_number(c, stack);

            case ast_number_constant: return _data.number;

            case ast_func_last: return static_cast<double>(c.size);

            case ast_func_position: return static_cast<double>(c.position);

            case ast_func_count: {
              XPathAllocatorCapture cr(stack.result);

              return static_cast<double>(_left->eval_node_set(c, stack, nodeset_eval_all).size());
            }

            case ast_func_string_length_0: {
              XPathAllocatorCapture cr(stack.result);

              return static_cast<double>(string_value(c.n, stack.result).length());
            }

            case ast_func_string_length_1: {
              XPathAllocatorCapture cr(stack.result);

              return static_cast<double>(_left->eval_string(c, stack).length());
            }

            case ast_func_number_0: {
              XPathAllocatorCapture cr(stack.result);

              return convert_string_to_number(string_value(c.n, stack.result).c_str());
            }

            case ast_func_number_1: return _left->eval_number(c, stack);

            case ast_func_sum: {
              XPathAllocatorCapture cr(stack.result);

              double r           = 0;

              XPathNodeSetRaw ns = _left->eval_node_set(c, stack, nodeset_eval_all);

              for(XPathNode const *it = ns.begin(); it != ns.end(); ++it)
              {
                XPathAllocatorCapture cri(stack.result);

                r += convert_string_to_number(string_value(*it, stack.result).c_str());
              }

              return r;
            }

            case ast_func_floor: {
              double r = _left->eval_number(c, stack);

              return r == r ? floor(r) : r;
            }

            case ast_func_ceiling: {
              double r = _left->eval_number(c, stack);

              return r == r ? ceil(r) : r;
            }

            case ast_func_round: return round_nearest_nzero(_left->eval_number(c, stack));

            case ast_variable: {
              LUMEX_ASSERT(_rettype == _data.variable->type());

              if(_rettype == xpath_type_number) return _data.variable->get_number();

              // variable needs to be converted to the correct type, this is handled by the fallthrough block below
              break;
            }

            default:;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch(_rettype)
            {
            case xpath_type_boolean: return eval_boolean(c, stack) ? 1 : 0;

            case xpath_type_string:
            case xpath_type_node_set: // implicit conversion to string
            {
              XPathAllocatorCapture cr(stack.result);

              return convert_string_to_number(eval_string(c, stack).c_str());
            }

            default:
              LUMEX_ASSERT(false && "Wrong expression for return type number"); // unreachable
              return 0;
            }
          }

          xpath_string
          eval_string_concat(XPathContext const &c, XPathStack const &stack)
          {
            LUMEX_ASSERT(m_type == ast_func_concat);

            XPathAllocatorCapture ct(stack.temp);

            // count the string number
            size_t count = 1;
            for(XPathAstNode *nc = _right; nc; nc = nc->_next) count++;

            // allocate a buffer for temporary string objects
            xpath_string *buffer = static_cast<xpath_string *>(stack.temp->allocate(count * sizeof(xpath_string)));
            if(!buffer) return xpath_string();

            // evaluate all strings to temporary stack
            XPathStack swapped_stack = {stack.temp, stack.result};

            buffer[0]                = _left->eval_string(c, swapped_stack);

            size_t pos               = 1;
            for(XPathAstNode *n = _right; n; n = n->_next, ++pos) buffer[pos] = n->eval_string(c, swapped_stack);
            LUMEX_ASSERT(pos == count);

            // get total length
            size_t length = 0;
            for(size_t i = 0; i < count; ++i) length += buffer[i].length();

            // create final string
            char_t *result = static_cast<char_t *>(stack.result->allocate((length + 1) * sizeof(char_t)));
            if(!result) return xpath_string();

            char_t *ri = result;

            for(size_t j = 0; j < count; ++j)
              for(char_t const *bi = buffer[j].c_str(); *bi; ++bi) *ri++ = *bi;

            *ri = 0;

            return xpath_string::from_heap_preallocated(result, ri);
          }

          xpath_string
          eval_string(XPathContext const &c, XPathStack const &stack)
          {
            switch(m_type)
            {
            case ast_string_constant: return xpath_string::from_const(_data.string);

            case ast_func_local_name_0: {
              XPathNode na = c.n;

              return xpath_string::from_const(local_name(na));
            }

            case ast_func_local_name_1: {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
              XPathNode na      = ns.first();

              return xpath_string::from_const(local_name(na));
            }

            case ast_func_name_0: {
              XPathNode na = c.n;

              return xpath_string::from_const(qualified_name(na));
            }

            case ast_func_name_1: {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
              XPathNode na      = ns.first();

              return xpath_string::from_const(qualified_name(na));
            }

            case ast_func_namespace_uri_0: {
              XPathNode na = c.n;

              return xpath_string::from_const(namespace_uri(na));
            }

            case ast_func_namespace_uri_1: {
              XPathAllocatorCapture cr(stack.result);

              XPathNodeSetRaw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
              XPathNode na      = ns.first();

              return xpath_string::from_const(namespace_uri(na));
            }

            case ast_func_string_0: return string_value(c.n, stack.result);

            case ast_func_string_1: return _left->eval_string(c, stack);

            case ast_func_concat: return eval_string_concat(c, stack);

            case ast_func_substring_before: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              xpath_string s           = _left->eval_string(c, swapped_stack);
              xpath_string p           = _right->eval_string(c, swapped_stack);

              char_t const *pos        = find_substring(s.c_str(), p.c_str());

              return pos ? xpath_string::from_heap(s.c_str(), pos, stack.result) : xpath_string();
            }

            case ast_func_substring_after: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              xpath_string s           = _left->eval_string(c, swapped_stack);
              xpath_string p           = _right->eval_string(c, swapped_stack);

              char_t const *pos        = find_substring(s.c_str(), p.c_str());
              if(!pos) return xpath_string();

              char_t const *rbegin = pos + p.length();
              char_t const *rend   = s.c_str() + s.length();

              return s.uses_heap() ? xpath_string::from_heap(rbegin, rend, stack.result)
                                   : xpath_string::from_const(rbegin);
            }

            case ast_func_substring_2: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              xpath_string s           = _left->eval_string(c, swapped_stack);
              size_t s_length          = s.length();

              double first             = round_nearest(_right->eval_number(c, stack));

              if(is_nan(first))
                return xpath_string(); // NaN
              else if(first >= static_cast<double>(s_length + 1))
                return xpath_string();

              size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
              LUMEX_ASSERT(1 <= pos && pos <= s_length + 1);

              char_t const *rbegin = s.c_str() + (pos - 1);
              char_t const *rend   = s.c_str() + s.length();

              return s.uses_heap() ? xpath_string::from_heap(rbegin, rend, stack.result)
                                   : xpath_string::from_const(rbegin);
            }

            case ast_func_substring_3: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              xpath_string s           = _left->eval_string(c, swapped_stack);
              size_t s_length          = s.length();

              double first             = round_nearest(_right->eval_number(c, stack));
              double last              = first + round_nearest(_right->_next->eval_number(c, stack));

              if(is_nan(first) || is_nan(last))
                return xpath_string();
              else if(first >= static_cast<double>(s_length + 1))
                return xpath_string();
              else if(first >= last)
                return xpath_string();
              else if(last < 1)
                return xpath_string();

              size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
              size_t end = last >= static_cast<double>(s_length + 1) ? s_length + 1 : static_cast<size_t>(last);

              LUMEX_ASSERT(1 <= pos && pos <= end && end <= s_length + 1);
              char_t const *rbegin = s.c_str() + (pos - 1);
              char_t const *rend   = s.c_str() + (end - 1);

              return (end == s_length + 1 && !s.uses_heap()) ? xpath_string::from_const(rbegin)
                                                             : xpath_string::from_heap(rbegin, rend, stack.result);
            }

            case ast_func_normalize_space_0: {
              xpath_string s = string_value(c.n, stack.result);

              char_t *begin  = s.data(stack.result);
              if(!begin) return xpath_string();

              char_t *end = normalize_space(begin);

              return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_func_normalize_space_1: {
              xpath_string s = _left->eval_string(c, stack);

              char_t *begin  = s.data(stack.result);
              if(!begin) return xpath_string();

              char_t *end = normalize_space(begin);

              return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_func_translate: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              xpath_string s           = _left->eval_string(c, stack);
              xpath_string from        = _right->eval_string(c, swapped_stack);
              xpath_string to          = _right->_next->eval_string(c, swapped_stack);

              char_t *begin            = s.data(stack.result);
              if(!begin) return xpath_string();

              char_t *end = translate(begin, from.c_str(), to.c_str(), to.length());

              return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_opt_translate_table: {
              xpath_string s = _left->eval_string(c, stack);

              char_t *begin  = s.data(stack.result);
              if(!begin) return xpath_string();

              char_t *end = translate_table(begin, _data.table);

              return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_variable: {
              LUMEX_ASSERT(_rettype == _data.variable->type());

              if(_rettype == xpath_type_string) return xpath_string::from_const(_data.variable->get_string());

              // variable needs to be converted to the correct type, this is handled by the fallthrough block below
              break;
            }

            default:;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch(_rettype)
            {
            case xpath_type_boolean:
              return xpath_string::from_const(eval_boolean(c, stack) ? LUMEX_XML_TEXT("true")
                                                                     : LUMEX_XML_TEXT("false"));

            case xpath_type_number: return convert_number_to_string(eval_number(c, stack), stack.result);

            case xpath_type_node_set: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              XPathNodeSetRaw ns       = eval_node_set(c, swapped_stack, nodeset_eval_first);
              return ns.empty() ? xpath_string() : string_value(ns.first(), stack.result);
            }

            default:
              LUMEX_ASSERT(false && "Wrong expression for return type string"); // unreachable
              return xpath_string();
            }
          }

          XPathNodeSetRaw
          eval_node_set(XPathContext const &c, XPathStack const &stack, Types::nodeset_eval_t eval)
          {
            switch(m_type)
            {
            case ast_op_union: {
              XPathAllocatorCapture cr(stack.temp);

              XPathStack swapped_stack = {stack.temp, stack.result};

              XPathNodeSetRaw ls       = _left->eval_node_set(c, stack, eval);
              XPathNodeSetRaw rs       = _right->eval_node_set(c, swapped_stack, eval);

              // we can optimize merging two sorted sets, but this is a very rare operation, so don't bother
              ls.set_type(XPathNodeSet::type_unsorted);

              ls.append(rs.begin(), rs.end(), stack.result);
              ls.remove_duplicates(stack.temp);

              return ls;
            }

            case ast_filter: {
              XPathNodeSetRaw set = _left->eval_node_set(
                c, stack, _test == predicate_constant_one ? nodeset_eval_first : nodeset_eval_all);

              // either expression is a number or it contains position() call; sort by document order
              if(_test != predicate_posinv) set.sort_do();

              bool once = eval_once(set.type(), eval);

              apply_predicate(set, 0, stack, once);

              return set;
            }

            case ast_func_id: return XPathNodeSetRaw();

            case ast_step: {
              switch(_axis)
              {
              case axis_ancestor: return step_do(c, stack, eval, axis_to_type<axis_ancestor>());

              case axis_ancestor_or_self: return step_do(c, stack, eval, axis_to_type<axis_ancestor_or_self>());

              case axis_attribute: return step_do(c, stack, eval, axis_to_type<axis_attribute>());

              case axis_child: return step_do(c, stack, eval, axis_to_type<axis_child>());

              case axis_descendant: return step_do(c, stack, eval, axis_to_type<axis_descendant>());

              case axis_descendant_or_self: return step_do(c, stack, eval, axis_to_type<axis_descendant_or_self>());

              case axis_following: return step_do(c, stack, eval, axis_to_type<axis_following>());

              case axis_following_sibling: return step_do(c, stack, eval, axis_to_type<axis_following_sibling>());

              case axis_namespace:
                // namespaced axis is not supported
                return XPathNodeSetRaw();

              case axis_parent: return step_do(c, stack, eval, axis_to_type<axis_parent>());

              case axis_preceding: return step_do(c, stack, eval, axis_to_type<axis_preceding>());

              case axis_preceding_sibling: return step_do(c, stack, eval, axis_to_type<axis_preceding_sibling>());

              case axis_self: return step_do(c, stack, eval, axis_to_type<axis_self>());

              default:
                LUMEX_ASSERT(false && "Unknown axis"); // unreachable
                return XPathNodeSetRaw();
              }
            }

            case ast_step_root: {
              LUMEX_ASSERT(!_right); // root step can't have any predicates

              XPathNodeSetRaw ns;

              ns.set_type(XPathNodeSet::type_sorted);

              if(c.n.node())
                ns.push_back(c.n.node().root(), stack.result);
              else if(c.n.attribute())
                ns.push_back(c.n.parent().root(), stack.result);

              return ns;
            }

            case ast_variable: {
              LUMEX_ASSERT(_rettype == _data.variable->type());

              if(_rettype == xpath_type_node_set)
              {
                XPathNodeSet const &s = _data.variable->get_node_set();

                XPathNodeSetRaw ns;

                ns.set_type(s.type());
                ns.append(s.begin(), s.end(), stack.result);

                return ns;
              }

              // variable needs to be converted to the correct type, this is handled by the fallthrough block below
              break;
            }

            default:;
            }

            // none of the ast types that return the value directly matched, but conversions to node set are invalid
            LUMEX_ASSERT(false && "Wrong expression for return type node set"); // unreachable
            return XPathNodeSetRaw();
          }

          void
          optimize(xpath_allocator *alloc)
          {
            if(_left) _left->optimize(alloc);

            if(_right) _right->optimize(alloc);

            if(_next) _next->optimize(alloc);

            // coverity[var_deref_model]
            optimize_self(alloc);
          }

          void
          optimize_self(xpath_allocator *alloc)
          {
            // Rewrite [position()=expr] with [expr]
            // Note that this step has to go before classification to recognize [position()=1]
            if((m_type == ast_filter || m_type == ast_predicate) && _right
               && // workaround for clang static analyzer (_right is never null for ast_filter/ast_predicate)
               _right->m_type == ast_op_equal && _right->_left->m_type == ast_func_position
               && _right->_right->_rettype == xpath_type_number)
            {
              _right = _right->_right;
            }

            // Classify filter/predicate ops to perform various optimizations during evaluation
            if((m_type == ast_filter || m_type == ast_predicate)
               && _right) // workaround for clang static analyzer (_right is never null for ast_filter/ast_predicate)
            {
              LUMEX_ASSERT(_test == predicate_default);

              if(_right->m_type == ast_number_constant && _right->_data.number == 1.0)
                _test = predicate_constant_one;
              else if(_right->_rettype == xpath_type_number
                      && (_right->m_type == ast_number_constant || _right->m_type == ast_variable
                          || _right->m_type == ast_func_last))
                _test = predicate_constant;
              else if(_right->_rettype != xpath_type_number && _right->is_posinv_expr())
                _test = predicate_posinv;
            }

            // Rewrite descendant-or-self::node()/child::foo with descendant::foo
            // The former is a full form of //foo, the latter is much faster since it executes the node test immediately
            // Do a similar kind of rewrite for self/descendant/descendant-or-self axes
            // Note that we only rewrite positionally invariant steps (//foo[1] != /descendant::foo[1])
            if(m_type == ast_step
               && (_axis == axis_child || _axis == axis_self || _axis == axis_descendant
                   || _axis == axis_descendant_or_self)
               && _left && _left->m_type == ast_step && _left->_axis == axis_descendant_or_self
               && _left->_test == nodetest_type_node && !_left->_right && is_posinv_step())
            {
              if(_axis == axis_child || _axis == axis_descendant)
                _axis = axis_descendant;
              else
                _axis = axis_descendant_or_self;

              _left = _left->_left;
            }

            // Use optimized lookup table implementation for translate() with constant arguments
            if(m_type == ast_func_translate && _right
               && // workaround for clang static analyzer (_right is never null for ast_func_translate)
               _right->m_type == ast_string_constant && _right->_next->m_type == ast_string_constant)
            {
              unsigned char *table = translate_table_generate(alloc, _right->_data.string, _right->_next->_data.string);

              if(table)
              {
                m_type      = ast_opt_translate_table;
                _data.table = table;
              }
            }

            // Use optimized path for @attr = 'value' or @attr = $value
            if(m_type == ast_op_equal && _left && _right
               && // workaround for clang static analyzer and Coverity (_left and
                  // _right are never null for ast_op_equal) coverity[mixed_enums]
               _left->m_type == ast_step && _left->_axis == axis_attribute && _left->_test == nodetest_name
               && !_left->_left && !_left->_right
               && (_right->m_type == ast_string_constant
                   || (_right->m_type == ast_variable && _right->_rettype == xpath_type_string)))
            {
              m_type = ast_opt_compare_attribute;
            }
          }

          bool
          is_posinv_expr() const
          {
            switch(m_type)
            {
            case ast_func_position:
            case ast_func_last: return false;

            case ast_string_constant:
            case ast_number_constant:
            case ast_variable: return true;

            case ast_step:
            case ast_step_root: return true;

            case ast_predicate:
            case ast_filter: return true;

            default:
              if(_left && !_left->is_posinv_expr()) return false;

              for(XPathAstNode *n = _right; n; n = n->_next)
                if(!n->is_posinv_expr()) return false;

              return true;
            }
          }

          bool
          is_posinv_step() const
          {
            LUMEX_ASSERT(m_type == ast_step);

            for(XPathAstNode *n = _right; n; n = n->_next)
            {
              LUMEX_ASSERT(n->m_type == ast_predicate);

              if(n->_test != predicate_posinv) return false;
            }

            return true;
          }

          xpath_value_type
          rettype() const
          {
            return static_cast<xpath_value_type>(_rettype);
          }
        };
      } // namespace Ast
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_AST_HPP

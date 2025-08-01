#ifndef LUMEX_XML_XPATH_AST_HPP
#define LUMEX_XML_XPATH_AST_HPP

#include "lumex/xml/types/XmlTypes.hpp"

#include "lumex/xml/xpath/context/XPathContext.hpp"
#include "lumex/xml/xpath/memory/XPathStack.hpp"
#include "lumex/xml/xpath/node/XPathNodeSet.hpp"
#include "lumex/xml/xpath/string/XPathString.hpp"
#include "lumex/xml/xpath/variable/XPathVariable.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Variable;
using namespace Lumex::Xml::XPath::Memory;
using namespace Lumex::Xml::XPath::Context;
using namespace Lumex::Xml::XPath::String;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Ast
      {
        template <axis_t N> struct axis_to_type {
          static axis_t const axis;
        };

        template <axis_t N> axis_t const axis_to_type<N>::axis = N;

        class XPathAstNode
        {
        public:
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, char_t const *value);

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, double value);

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathVariable *value);

          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathAstNode *left = nullptr,
                       XPathAstNode *right = nullptr);

          XPathAstNode(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents);

          XPathAstNode(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test);

          void set_next(XPathAstNode *value);

          void set_right(XPathAstNode *value);

          bool eval_boolean(XPathContext const &ctx, XPathStack const &stack);

          double eval_number(XPathContext const &ctx, XPathStack const &stack);

          XPathString eval_string_concat(XPathContext const &ctx, XPathStack const &stack);

          XPathString eval_string(XPathContext const &ctx, XPathStack const &stack);

          XPathNodeSetRaw eval_node_set(XPathContext const &ctx, XPathStack const &stack, Types::nodeset_eval_t eval);

          void optimize(XPathAllocator *alloc);

          void optimize_self(XPathAllocator *alloc);

          bool is_posinv_expr() const;

          bool is_posinv_step() const;

          xpath_value_type rettype() const;

        private:
          // node type
          char m_type;
          char m_rettype;

          // for ast_step
          char m_axis;

          // for ast_step/ast_predicate/ast_filter
          char m_test;

          // tree node structure
          XPathAstNode *m_left;
          XPathAstNode *m_right;
          XPathAstNode *m_next;

          union {
            char_t const *string;
            double number;
            XPathVariable *variable;
            char_t const *nodetest;
            unsigned char const *table;
          } m_data;

          XPathAstNode(XPathAstNode const &)                = default;
          XPathAstNode &operator=(XPathAstNode const &)     = default;
          XPathAstNode(XPathAstNode &&) noexcept            = default;
          XPathAstNode &operator=(XPathAstNode &&) noexcept = default;
          ~XPathAstNode() noexcept                          = default;

          template <class Comp>
          static bool
          compare_eq(XPathAstNode *lhs, // NOLINT(readability-function-cognitive-complexity)
                     XPathAstNode *rhs, XPathContext const &ctx, XPathStack const &stack, Comp const &comp)
          {
            xpath_value_type lt_ = lhs->rettype();
            xpath_value_type rt_ = rhs->rettype();

            if(lt_ != xpath_type_node_set && rt_ != xpath_type_node_set)
            {
              if(lt_ == xpath_type_boolean || rt_ == xpath_type_boolean)
                return comp(lhs->eval_boolean(ctx, stack), rhs->eval_boolean(ctx, stack));
              if(lt_ == xpath_type_number || rt_ == xpath_type_number)
                return comp(lhs->eval_number(ctx, stack), rhs->eval_number(ctx, stack));
              if(lt_ == xpath_type_string || rt_ == xpath_type_string)
              {
                XPathAllocatorCapture capture(stack.result);

                XPathString ls_ = lhs->eval_string(ctx, stack);
                XPathString rs_ = rhs->eval_string(ctx, stack);

                return comp(ls_, rs_);
              }
            }
            else if(lt_ == xpath_type_node_set && rt_ == xpath_type_node_set)
            {
              XPathAllocatorCapture capture(stack.result);

              XPathNodeSetRaw ls_ = lhs->eval_node_set(ctx, stack, nodeset_eval_all);
              XPathNodeSetRaw rs_ = rhs->eval_node_set(ctx, stack, nodeset_eval_all);

              for(XPathNode const *li = ls_.begin(); li != ls_.end(); ++li)
                for(XPathNode const *ri = rs_.begin(); ri != rs_.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);

                  if(comp(string_value(*li, stack.result), string_value(*ri, stack.result))) return true;
                }

              return false;
            }
            else
            {
              if(lt_ == xpath_type_node_set)
              {
                swap(lhs, rhs);
                swap(lt_, rt_);
              }

              if(lt_ == xpath_type_boolean) return comp(lhs->eval_boolean(ctx, stack), rhs->eval_boolean(ctx, stack));
              if(lt_ == xpath_type_number)
              {
                XPathAllocatorCapture capture(stack.result);

                double tmp          = lhs->eval_number(ctx, stack);
                XPathNodeSetRaw rs_ = rhs->eval_node_set(ctx, stack, nodeset_eval_all);

                for(XPathNode const *ri = rs_.begin(); ri != rs_.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);

                  if(comp(tmp, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
                }

                return false;
              }
              if(lt_ == xpath_type_string)
              {
                XPathAllocatorCapture capture(stack.result);

                XPathString tmp     = lhs->eval_string(ctx, stack);
                XPathNodeSetRaw rs_ = rhs->eval_node_set(ctx, stack, nodeset_eval_all);

                for(XPathNode const *ri = rs_.begin(); ri != rs_.end(); ++ri)
                {
                  XPathAllocatorCapture cri(stack.result);
                  if(comp(tmp, string_value(*ri, stack.result))) return true;
                }

                return false;
              }
            }

            LUMEX_ASSERT(false && "Wrong types"); // unreachable
            return false;
          }

          static bool eval_once(XPathNodeSet::type_t type, Types::nodeset_eval_t eval);

          template <class Comp>
          static bool
          compare_rel(XPathAstNode *lhs, // NOLINT(readability-function-cognitive-complexity)
                      XPathAstNode *rhs, XPathContext const &ctx, XPathStack const &stack, Comp const &comp)
          {
            xpath_value_type lt_ = lhs->rettype();
            xpath_value_type rt_ = rhs->rettype();

            if(lt_ != xpath_type_node_set && rt_ != xpath_type_node_set)
              return comp(lhs->eval_number(ctx, stack), rhs->eval_number(ctx, stack));
            if(lt_ == xpath_type_node_set && rt_ == xpath_type_node_set)
            {
              XPathAllocatorCapture capture(stack.result);

              XPathNodeSetRaw ls_ = lhs->eval_node_set(ctx, stack, nodeset_eval_all);
              XPathNodeSetRaw rs_ = rhs->eval_node_set(ctx, stack, nodeset_eval_all);

              for(XPathNode const *li = ls_.begin(); li != ls_.end(); ++li)
              {
                XPathAllocatorCapture cri(stack.result);

                double tmp = convert_string_to_number(string_value(*li, stack.result).c_str());

                for(XPathNode const *ri = rs_.begin(); ri != rs_.end(); ++ri)
                {
                  XPathAllocatorCapture crii(stack.result);

                  if(comp(tmp, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
                }
              }

              return false;
            }
            if(lt_ != xpath_type_node_set && rt_ == xpath_type_node_set)
            {
              XPathAllocatorCapture capture(stack.result);

              double tmp          = lhs->eval_number(ctx, stack);
              XPathNodeSetRaw rs_ = rhs->eval_node_set(ctx, stack, nodeset_eval_all);

              for(XPathNode const *ri = rs_.begin(); ri != rs_.end(); ++ri)
              {
                XPathAllocatorCapture cri(stack.result);

                if(comp(tmp, convert_string_to_number(string_value(*ri, stack.result).c_str()))) return true;
              }

              return false;
            }
            if(lt_ == xpath_type_node_set && rt_ != xpath_type_node_set)
            {
              XPathAllocatorCapture capture(stack.result);

              XPathNodeSetRaw ls_ = lhs->eval_node_set(ctx, stack, nodeset_eval_all);
              double tmp          = rhs->eval_number(ctx, stack);

              for(XPathNode const *li = ls_.begin(); li != ls_.end(); ++li)
              {
                XPathAllocatorCapture cri(stack.result);

                if(comp(convert_string_to_number(string_value(*li, stack.result).c_str()), tmp)) return true;
              }

              return false;
            }

            LUMEX_ASSERT(false && "Wrong types"); // unreachable
            return false;
          }

          static void apply_predicate_boolean(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr,
                                              XPathStack const &stack, bool once);

          static void apply_predicate_number(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr,
                                             XPathStack const &stack, bool once);

          static void
          apply_predicate_number_const(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr, XPathStack const &stack);

          void apply_predicate(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, bool once);

          void
          apply_predicates(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, Types::nodeset_eval_t eval);

          bool
          step_push(XPathNodeSetRaw &nsr, XmlAttributeBase *attr, XmlNodeBase *parent, XPathAllocator *alloc) const;

          bool step_push(XPathNodeSetRaw &nsr, XmlNodeBase *node, XPathAllocator *alloc) const;

          template <class T>
          void
          step_fill(XPathNodeSetRaw &nsr, // NOLINT(readability-function-cognitive-complexity)
                    XmlNodeBase *node, XPathAllocator *alloc, bool once, T /*val*/)
          {
            axis_t const axis = T::axis;

            switch(axis)
            {
            case axis_attribute: {
              for(XmlAttributeBase *attr = node->first_attribute; attr; attr = attr->next_attribute)
                if(step_push(nsr, attr, node, alloc) & once) return;

              break;
            }

            case axis_child: {
              for(XmlNodeBase *ctx = node->first_child; ctx; ctx = ctx->next_sibling)
                if(step_push(nsr, ctx, alloc) & once) return;

              break;
            }

            case axis_descendant:
            case axis_descendant_or_self: {
              if(axis == axis_descendant_or_self)
                if(step_push(nsr, node, alloc) & once) return;

              XmlNodeBase *cur = node->first_child;

              while(cur)
              {
                if(step_push(nsr, cur, alloc) & once) return;

                if(cur->first_child)
                  cur = cur->first_child;
                else
                {
                  while(!cur->next_sibling)
                  {
                    cur = cur->parent;

                    if(cur == node) return;
                  }

                  cur = cur->next_sibling;
                }
              }

              break;
            }

            case axis_following_sibling: {
              for(XmlNodeBase *ctx = node->next_sibling; ctx; ctx = ctx->next_sibling)
                if(step_push(nsr, ctx, alloc) & once) return;

              break;
            }

            case axis_preceding_sibling: {
              for(XmlNodeBase *ctx = node->prev_sibling_c; ctx->next_sibling; ctx = ctx->prev_sibling_c)
                if(step_push(nsr, ctx, alloc) & once) return;

              break;
            }

            case axis_following: {
              XmlNodeBase *cur = node;

              // exit from this node so that we don't include descendants
              while(!cur->next_sibling)
              {
                cur = cur->parent;

                if(!cur) return;
              }

              cur = cur->next_sibling;

              while(cur)
              {
                if(step_push(nsr, cur, alloc) & once) return;

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
              XmlNodeBase *cur = node;

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
                  if(step_push(nsr, cur, alloc) & once) return;

                  while(!cur->prev_sibling_c->next_sibling)
                  {
                    cur = cur->parent;

                    if(!cur) return;

                    if(!node_is_ancestor(cur, node))
                      if(step_push(nsr, cur, alloc) & once) return;
                  }

                  cur = cur->prev_sibling_c;
                }
              }

              break;
            }

            case axis_ancestor:
            case axis_ancestor_or_self: {
              if(axis == axis_ancestor_or_self)
                if(step_push(nsr, node, alloc) & once) return;

              XmlNodeBase *cur = node->parent;

              while(cur)
              {
                if(step_push(nsr, cur, alloc) & once) return;

                cur = cur->parent;
              }

              break;
            }

            case axis_self: {
              step_push(nsr, node, alloc);

              break;
            }

            case axis_parent: {
              if(node->parent) step_push(nsr, node->parent, alloc);

              break;
            }

            default: LUMEX_ASSERT(false && "Unimplemented axis"); // unreachable
            }
          }

          template <class T>
          void
          step_fill(XPathNodeSetRaw &nsr, // NOLINT(readability-function-cognitive-complexity)
                    XmlAttributeBase *attr, XmlNodeBase *node, XPathAllocator *alloc, bool once, T val)
          {
            axis_t const axis = T::axis;

            switch(axis)
            {
            case axis_ancestor:
            case axis_ancestor_or_self: {
              if(axis == axis_ancestor_or_self
                 && m_test == nodetest_type_node) // reject attributes based on principal node type test
                if(step_push(nsr, attr, node, alloc) & once) return;

              XmlNodeBase *cur = node;

              while(cur)
              {
                if(step_push(nsr, cur, alloc) & once) return;

                cur = cur->parent;
              }

              break;
            }

            case axis_descendant_or_self:
            case axis_self: {
              if(m_test == nodetest_type_node) // reject attributes based on principal node type test
                step_push(nsr, attr, node, alloc);

              break;
            }

            case axis_following: {
              XmlNodeBase *cur = node;

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

                if(step_push(nsr, cur, alloc) & once) return;
              }

              break;
            }

            case axis_parent: {
              step_push(nsr, node, alloc);

              break;
            }

            case axis_preceding: {
              // preceding:: axis does not include attribute nodes and attribute ancestors (they are the same as
              // parent's ancestors), so we can reuse node preceding
              step_fill(nsr, node, alloc, once, val);
              break;
            }

            default: LUMEX_ASSERT(false && "Unimplemented axis"); // unreachable
            }
          }

          template <class T>
          void
          step_fill(XPathNodeSetRaw &nsr, XPathNode const &path_node, XPathAllocator *alloc, bool once, T val)
          {
            axis_t const axis = T::axis;
            bool const axis_has_attributes
              = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_descendant_or_self
                 || axis == axis_following || axis == axis_parent || axis == axis_preceding || axis == axis_self);

            if(path_node.node())
              step_fill(nsr, path_node.node().get(), alloc, once, val);
            else if(axis_has_attributes && path_node.attribute() && path_node.parent())
              step_fill(nsr, path_node.attribute().get(), path_node.parent().get(), alloc, once, val);
          }

          template <class T>
          XPathNodeSetRaw
          step_do(XPathContext const &ctx, XPathStack const &stack, Types::nodeset_eval_t eval, T val)
          {
            axis_t const axis       = T::axis;
            bool const axis_reverse = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_preceding
                                       || axis == axis_preceding_sibling);
            XPathNodeSet::type_t const axis_type
              = axis_reverse ? XPathNodeSet::type_sorted_reverse : XPathNodeSet::type_sorted;

            bool once = (axis == axis_attribute && m_test == nodetest_name) || (!m_right && eval_once(axis_type, eval))
                        ||
                        // coverity[mixed_enums]
                        (m_right && !m_right->m_next && m_right->m_test == predicate_constant_one);

            XPathNodeSetRaw nsr;
            nsr.set_type(axis_type);

            if(m_left)
            {
              XPathNodeSetRaw tmp = m_left->eval_node_set(ctx, stack, nodeset_eval_all);

              // self axis preserves the original order
              if(axis == axis_self) nsr.set_type(tmp.type());

              for(XPathNode const *it = tmp.begin(); it != tmp.end(); ++it)
              {
                size_t size = nsr.size();

                // in general, all axes generate elements in a particular order, but there is no order guarantee if axis
                // is applied to two nodes
                if(axis != axis_self && size != 0) nsr.set_type(XPathNodeSet::type_unsorted);

                step_fill(nsr, *it, stack.result, once, val);
                if(m_right) apply_predicates(nsr, size, stack, eval);
              }
            }
            else
            {
              step_fill(nsr, ctx.node, stack.result, once, val);
              if(m_right) apply_predicates(nsr, 0, stack, eval);
            }

            // child, attribute and self axes always generate unique set of nodes
            // for other axis, if the set stayed sorted, it stayed unique because the traversal algorithms do not visit
            // the same node twice
            if(axis != axis_child && axis != axis_attribute && axis != axis_self
               && nsr.type() == XPathNodeSet::type_unsorted)
              nsr.remove_duplicates(stack.temp);

            return nsr;
          }
        };
      } // namespace Ast
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_AST_HPP

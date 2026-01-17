#ifndef LUMEX_XML_XPATH_AST_HPP
#define LUMEX_XML_XPATH_AST_HPP

#include "lumex/LumexExport.hpp"

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
        /**
         * @brief Provides a static member `axis` for mapping an `axis_t` enumeration value
         *        to a type.
         * @details This template struct is used to facilitate type-based dispatch for XPath axis
         *          operations, allowing axis-specific logic to be associated with concrete types
         *          rather than just enum values.
         * @tparam N The `axis_t` enumeration value representing the specific XPath axis.
         */
        template <axis_t N> struct axis_to_type {
          static axis_t const axis;
        };

        template <axis_t N> axis_t const axis_to_type<N>::axis = N;

        /**
         * @brief Represents a node in the XPath Abstract Syntax Tree (AST).
         * @details This class is the fundamental building block for representing parsed XPath
         *          expressions. Each `XPathAstNode` encapsulates a specific operation,
         *          constant, or step within an XPath query, along with its associated data
         *          and references to child nodes. It supports various XPath evaluation
         *          operations (boolean, number, string, node set) and optimizations.
         *
         * The node types (`m_type`) cover a wide range of XPath constructs, including:
         * - Arithmetic and logical operators (e.g., `ast_op_add`, `ast_op_and`)
         * - Function calls (e.g., `ast_func_string`, `ast_func_concat`)
         * - Path steps and filters (e.g., `ast_step`, `ast_filter`)
         * - Constants and variables (e.g., `ast_string_constant`, `ast_variable`)
         *
         * @note This class is not thread-safe and should be used within a single execution
         *       context or with external synchronization.
         * @warning The internal `m_data` union access requires careful handling to ensure
         *          the correct member is accessed based on `m_type`.
         */
        class LUMEX_API XPathAstNode
        {
        public:
          /**
           * @brief Constructs an XPath AST node for a string constant.
           * @details Initializes an AST node representing a string literal in an XPath expression.
           * @param type The AST node type, must be `ast_string_constant`.
           * @param rettype_ The return type of the expression, typically `xpath_type_string`.
           * @param value A null-terminated C-string representing the constant string value.
           */
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, char_t const *value);

          /**
           * @brief Constructs an XPath AST node for a number constant.
           * @details Initializes an AST node representing a numeric literal in an XPath expression.
           * @param type The AST node type, must be `ast_number_constant`.
           * @param rettype_ The return type of the expression, typically `xpath_type_number`.
           * @param value The double-precision floating-point constant numeric value.
           */
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, double value);

          /**
           * @brief Constructs an XPath AST node for a variable reference.
           * @details Initializes an AST node representing a reference to an XPath variable.
           * @param type The AST node type, must be `ast_variable`.
           * @param rettype_ The expected return type of the variable.
           * @param value A pointer to the `XPathVariable` object. Ownership is not transferred.
           * @warning `value` must point to a valid `XPathVariable` instance throughout the
           *          lifetime of this `XPathAstNode`.
           */
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathVariable *value);

          /**
           * @brief Constructs a general XPath AST node with optional left and right children.
           * @details This constructor is used for most binary and unary operations, functions,
           *          and filters that involve child expressions.
           * @param type The AST node type (e.g., `ast_op_add`, `ast_func_boolean`, `ast_filter`).
           * @param rettype_ The expected return type of the expression represented by this node.
           * @param left A pointer to the left child AST node, or `nullptr` if not applicable.
           *             Ownership is not transferred.
           * @param right A pointer to the right child AST node, or `nullptr` if not applicable.
           *              Ownership is not transferred.
           */
          XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathAstNode *left = nullptr,
                       XPathAstNode *right = nullptr);

          /**
           * @brief Constructs an XPath AST node specifically for a path step.
           * @details This constructor initializes an AST node representing a step in an XPath path,
           *          including its axis, node test, and content (e.g., node name).
           * @param type The AST node type, must be `ast_step`.
           * @param left A pointer to the left child AST node, representing the preceding step
           *             or context node, or `nullptr` for an absolute path. Ownership is not transferred.
           * @param axis The XPath axis to apply (e.g., `axis_child`, `axis_attribute`).
           * @param test The node test to apply (e.g., `nodetest_name`, `nodetest_type_node`).
           * @param contents A null-terminated C-string for node test contents (e.g., element name),
           *                 or `nullptr` if not applicable.
           */
          XPathAstNode(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents);

          /**
           * @brief Constructs an XPath AST node for a predicate or filter.
           * @details This constructor is used for nodes that apply predicates to a node-set,
           *          modifying the filtered node-set based on an expression.
           * @param type The AST node type, must be `ast_filter` or `ast_predicate`.
           * @param left A pointer to the left child AST node, representing the expression
           *             producing the node-set to be filtered. Ownership is not transferred.
           * @param right A pointer to the right child AST node, representing the predicate
           *              expression (e.g., `position() = 1`). Ownership is not transferred.
           * @param test The predicate type, used for optimization.
           */
          XPathAstNode(ast_type_t type, XPathAstNode *left, XPathAstNode *right, predicate_t test);

          /**
           * @brief Sets the `m_next` pointer of this AST node.
           * @details The `m_next` pointer is typically used to link nodes in a sequence,
           *          such as multiple predicates in a step or arguments in a function call.
           * @param value A pointer to the next `XPathAstNode` in the sequence, or `nullptr`.
           *              Ownership is not transferred.
           */
          void set_next(XPathAstNode *value);

          /**
           * @brief Sets the `m_right` pointer of this AST node.
           * @details The `m_right` pointer typically points to the second operand in a
           *          binary operation or a subsequent argument in a function call.
           * @param value A pointer to the right `XPathAstNode`, or `nullptr`. Ownership is not transferred.
           */
          void set_right(XPathAstNode *value);

          /**
           * @brief Evaluates the XPath expression represented by this AST node to a boolean value.
           * @details This function recursively evaluates the expression tree rooted at this node
           *          and converts the result to a boolean value according to XPath conversion rules.
           * @param ctx The XPath context (current node, position, size) for evaluation.
           * @param stack The XPath stack, providing memory allocation for intermediate results.
           * @return `true` or `false` based on the evaluation result.
           * @note This function can be computationally expensive for complex expressions
           *       or large node sets.
           */
          bool eval_boolean(XPathContext const &ctx, XPathStack const &stack);

          /**
           * @brief Evaluates the XPath expression represented by this AST node to a numeric value.
           * @details This function recursively evaluates the expression tree rooted at this node
           *          and converts the result to a double-precision floating-point number.
           * @param ctx The XPath context (current node, position, size) for evaluation.
           * @param stack The XPath stack, providing memory allocation for intermediate results.
           * @return A `double` representing the numeric evaluation result. Returns NaN for invalid
           *         numeric conversions.
           * @note This function can be computationally expensive for complex expressions
           *       or large node sets.
           */
          double eval_number(XPathContext const &ctx, XPathStack const &stack);

          /**
           * @brief Evaluates a 'concat' XPath function expression, concatenating multiple strings.
           * @details This is a specialized evaluation function specifically for `ast_func_concat`
           *          node types. It efficiently concatenates the string values of all
           *          arguments.
           * @param ctx The XPath context for evaluation.
           * @param stack The XPath stack for memory allocation.
           * @return An `XPathString` containing the concatenated string. This string is
           *         allocated on the `stack.result` allocator.
           */
          XPathString eval_string_concat(XPathContext const &ctx, XPathStack const &stack);

          /**
           * @brief Evaluates the XPath expression represented by this AST node to a string value.
           * @details This function recursively evaluates the expression tree rooted at this node
           *          and converts the result to an `XPathString`.
           * @param ctx The XPath context (current node, position, size) for evaluation.
           * @param stack The XPath stack, providing memory allocation for intermediate results.
           * @return An `XPathString` representing the string evaluation result. The string
           *         data is managed by the `stack.result` allocator.
           * @note This function can be computationally expensive for complex expressions
           *       or large node sets.
           */
          XPathString eval_string(XPathContext const &ctx, XPathStack const &stack);

          /**
           * @brief Evaluates the XPath expression represented by this AST node to a node-set.
           * @details This function recursively evaluates the expression tree rooted at this node
           *          and collects all resulting nodes into an `XPathNodeSetRaw`.
           * @param ctx The XPath context (current node, position, size) for evaluation.
           * @param stack The XPath stack, providing memory allocation for intermediate results.
           * @param eval The evaluation mode for the node set (e.g., `nodeset_eval_all`,
           *             `nodeset_eval_first`).
           * @return An `XPathNodeSetRaw` containing the nodes resulting from the evaluation.
           *         The nodes are managed by the `stack.result` allocator.
           * @note The order of nodes in the returned set depends on the specific axis and
           *       evaluation strategy.
           */
          XPathNodeSetRaw eval_node_set(XPathContext const &ctx, XPathStack const &stack, Types::nodeset_eval_t eval);

          /**
           * @brief Optimizes the XPath AST by applying various rewrite rules recursively.
           * @details This function traverses the AST from this node downwards, applying
           *          optimizations to improve evaluation performance. Optimizations include
           *          predicate rewriting, axis simplification, and pre-calculating translation tables.
           * @param alloc A pointer to the `XPathAllocator` used for any new allocations during optimization.
           *              Ownership is not transferred.
           * @note This function modifies the AST in-place.
           */
          void optimize(XPathAllocator *alloc);

          /**
           * @brief Optimizes this specific XPath AST node by applying various rewrite rules.
           * @details This function applies local optimizations to the current node based on its
           *          type and the properties of its children. This includes classifying predicates
           *          and rewriting common XPath patterns (e.g., `//foo` to `descendant::foo`).
           * @param alloc A pointer to the `XPathAllocator` used for any new allocations during optimization.
           *              Ownership is not transferred.
           * @note This function modifies the AST in-place.
           */
          void optimize_self(XPathAllocator *alloc);

          /**
           * @brief Checks if the expression represented by this AST node is positionally invariant.
           * @details A positionally invariant expression is one whose result does not depend
           *          on the current context position or size (i.e., it does not use `position()`
           *          or `last()` functions). This property is crucial for certain XPath
           *          optimizations.
           * @return `true` if the expression is positionally invariant, `false` otherwise.
           */
          bool is_posinv_expr() const;

          /**
           * @brief Checks if this XPath step is positionally invariant.
           * @details A step is positionally invariant if none of its predicates depend on
           *          the current context position or size. This is used in optimizations
           *          like rewriting `//foo[1]` which is NOT positionally invariant.
           * @return `true` if the step and its predicates are positionally invariant, `false` otherwise.
           */
          bool is_posinv_step() const;

          /**
           * @brief Retrieves the XPath value type that this AST node is expected to return.
           * @details This is determined during parsing and reflects the primary type of the
           *          expression (e.g., `xpath_type_boolean`, `xpath_type_number`, `xpath_type_string`,
           *          `xpath_type_node_set`).
           * @return The `xpath_value_type` of this AST node.
           */
          xpath_value_type rettype() const;

        private:
          char m_type;    ///< The AST node type (e.g., ast_op_add, ast_func_string).
          char m_rettype; ///< The XPath value type returned by this expression.

          char m_axis; ///< The XPath axis (e.g., axis_child, axis_attribute) for `ast_step` nodes.
          char m_test; ///< The node test or predicate type (e.g., nodetest_name, predicate_constant_one).

          XPathAstNode *m_left;  ///< Pointer to the left child AST node.
          XPathAstNode *m_right; ///< Pointer to the right child AST node.
          XPathAstNode *m_next;  ///< Pointer to the next AST node in a sequence (e.g., function arguments, predicates).

          union {
            char_t const *string;    ///< Used for `ast_string_constant` and `ast_variable` when string type.
            double number;           ///< Used for `ast_number_constant` and `ast_variable` when number type.
            XPathVariable *variable; ///< Used for `ast_variable` to store the variable object.
            char_t const *nodetest;  ///< Used for `ast_step` and `ast_opt_compare_attribute` for node name or prefix.
            unsigned char const *table; ///< Used for `ast_opt_translate_table` to store translation lookup table.
          } m_data;                     ///< Union holding data specific to the node type.

          // Disable copy and move constructors/assignment operators for this class
          // as AST nodes are typically managed by an allocator and not copied.
          XPathAstNode(XPathAstNode const &)                = default;
          XPathAstNode &operator=(XPathAstNode const &)     = default;
          XPathAstNode(XPathAstNode &&) noexcept            = default;
          XPathAstNode &operator=(XPathAstNode &&) noexcept = default;
          ~XPathAstNode() noexcept                          = default;

          /**
           * @brief Compares two XPath expressions for equality or inequality.
           * @details This static helper function implements the XPath equality/inequality
           *          comparison rules, handling type conversions between boolean, number,
           *          string, and node-set types.
           * @tparam Comp A comparator functor (e.g., `equal_to`, `not_equal_to`) that defines
           *              the specific comparison operation.
           * @param lhs Pointer to the left-hand side AST node.
           * @param rhs Pointer to the right-hand side AST node.
           * @param ctx The XPath context for evaluation.
           * @param stack The XPath stack for memory allocation.
           * @param comp The comparator functor to use for the actual comparison.
           * @return `true` if the comparison holds, `false` otherwise.
           * @note This function can involve recursive evaluation of child nodes and temporary
           *       string/node-set allocations.
           * @throws `LUMEX_ASSERT` if an unsupported or wrong type combination is encountered.
           */
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
                std::swap(lhs, rhs);
                std::swap(lt_, rt_);
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

          /**
           * @brief Determines if `nodeset_eval_t` can be optimized to evaluate only a single node.
           * @details This static helper checks if the evaluation strategy for a node set allows
           *          for short-circuiting once the first matching node is found, based on
           *          the node set type (sorted/unsorted) and the desired evaluation mode.
           * @param type The type of the XPath node set (`XPathNodeSet::type_sorted`, `type_sorted_reverse`,
           * `type_unsorted`).
           * @param eval The desired evaluation mode (`nodeset_eval_all`, `nodeset_eval_first`, `nodeset_eval_any`).
           * @return `true` if only a single node evaluation is sufficient, `false` otherwise.
           */
          static bool eval_once(XPathNodeSet::type_t type, Types::nodeset_eval_t eval);

          /**
           * @brief Compares two XPath expressions for relational operators (less than, greater than, etc.).
           * @details This static helper function implements the XPath relational comparison rules,
           *          primarily converting operands to numbers for comparison. It handles
           *          node-set conversions according to XPath specification.
           * @tparam Comp A comparator functor (e.g., `less`, `less_equal`) that defines
           *              the specific relational comparison operation.
           * @param lhs Pointer to the left-hand side AST node.
           * @param rhs Pointer to the right-hand side AST node.
           * @param ctx The XPath context for evaluation.
           * @param stack The XPath stack for memory allocation.
           * @param comp The comparator functor to use for the actual comparison.
           * @return `true` if the comparison holds, `false` otherwise.
           * @note This function can involve recursive evaluation of child nodes and temporary
           *       string/node-set allocations.
           * @throws `LUMEX_ASSERT` if an unsupported or wrong type combination is encountered.
           */
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

          /**
           * @brief Applies a boolean predicate to a raw node set.
           * @details This function filters a portion of `nsr` (from `first` index) based on
           *          the boolean evaluation of `expr` for each node. Nodes for which `expr`
           *          evaluates to `true` are retained.
           * @param nsr The raw node set to be filtered. Modified in-place.
           * @param first The starting index in `nsr` from which to apply the predicate.
           * @param expr The AST node representing the boolean predicate expression.
           * @param stack The XPath stack for memory allocation during evaluation.
           * @param once If `true`, stops filtering after the first matching node is found.
           * @note Assumes `expr->rettype()` is not `xpath_type_number`.
           * @warning This function modifies the `nsr` in-place by compacting matching elements
           *          to the beginning of the filtered range.
           */
          static void apply_predicate_boolean(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr,
                                              XPathStack const &stack, bool once);

          /**
           * @brief Applies a numeric predicate to a raw node set.
           * @details This function filters a portion of `nsr` (from `first` index) based on
           *          whether the numeric evaluation of `expr` matches the context position
           *          (`idx`) for each node. Nodes for which `expr` evaluates to `idx` are retained.
           * @param nsr The raw node set to be filtered. Modified in-place.
           * @param first The starting index in `nsr` from which to apply the predicate.
           * @param expr The AST node representing the numeric predicate expression.
           * @param stack The XPath stack for memory allocation during evaluation.
           * @param once If `true`, stops filtering after the first matching node is found.
           * @note Assumes `expr->rettype()` is `xpath_type_number`.
           * @warning This function modifies the `nsr` in-place by compacting matching elements
           *          to the beginning of the filtered range.
           */
          static void apply_predicate_number(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr,
                                             XPathStack const &stack, bool once);

          /**
           * @brief Applies a constant numeric predicate to a raw node set.
           * @details This function filters a portion of `nsr` (from `first` index) based on
           *          whether the constant numeric evaluation of `expr` exactly matches a
           *          position within the node set. Only the node at that specific position
           *          (if valid) is retained.
           * @param nsr The raw node set to be filtered. Modified in-place.
           * @param first The starting index in `nsr` from which to apply the predicate.
           * @param expr The AST node representing the constant numeric predicate expression.
           * @param stack The XPath stack for memory allocation during evaluation.
           * @note Assumes `expr->rettype()` is `xpath_type_number` and `expr` is a constant.
           * @warning This function modifies the `nsr` in-place.
           */
          static void
          apply_predicate_number_const(XPathNodeSetRaw &nsr, size_t first, XPathAstNode *expr, XPathStack const &stack);

          /**
           * @brief Applies a single predicate to a raw node set.
           * @details This function dispatches to the appropriate predicate application
           *          method (`apply_predicate_boolean`, `apply_predicate_number`, or
           *          `apply_predicate_number_const`) based on the predicate expression's
           *          return type and its optimized `m_test` value.
           * @param nsr The raw node set to be filtered. Modified in-place.
           * @param first The starting index in `nsr` from which to apply the predicate.
           * @param stack The XPath stack for memory allocation.
           * @param once If `true`, indicates that only the first match is needed.
           * @note The `m_type` of this `XPathAstNode` must be `ast_filter` or `ast_predicate`.
           */
          void apply_predicate(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, bool once);

          /**
           * @brief Applies all predicates associated with this AST node to a raw node set.
           * @details This function iterates through a linked list of predicates (starting from `m_right`)
           *          and applies each one sequentially to the provided node set, refining the set.
           * @param nsr The raw node set to be filtered. Modified in-place.
           * @param first The starting index in `nsr` from which to apply the predicates.
           * @param stack The XPath stack for memory allocation.
           * @param eval The evaluation mode for the node set, used to determine if a "once" optimization can be
           * applied.
           */
          void
          apply_predicates(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, Types::nodeset_eval_t eval);

          /**
           * @brief Attempts to push an XML attribute node onto the node set if it matches the node test.
           * @details This function checks if a given `XmlAttributeBase` matches the `m_test` and
           *          `m_data.nodetest` of this `XPathAstNode` (which must be an `ast_step` node).
           *          If a match occurs, the attribute is added to the `XPathNodeSetRaw`.
           * @param nsr The `XPathNodeSetRaw` to which the node might be added.
           * @param attr Pointer to the `XmlAttributeBase` to test.
           * @param parent Pointer to the parent `XmlNodeBase` of the attribute.
           * @param alloc The `XPathAllocator` for allocating `XPathNode` objects within `nsr`.
           * @return `true` if the attribute was pushed (matched the test), `false` otherwise.
           */
          bool
          step_push(XPathNodeSetRaw &nsr, XmlAttributeBase *attr, XmlNodeBase *parent, XPathAllocator *alloc) const;

          /**
           * @brief Attempts to push an XML node onto the node set if it matches the node test.
           * @details This function checks if a given `XmlNodeBase` matches the `m_test` and
           *          `m_data.nodetest` of this `XPathAstNode` (which must be an `ast_step` node).
           *          If a match occurs, the node is added to the `XPathNodeSetRaw`.
           * @param nsr The `XPathNodeSetRaw` to which the node might be added.
           * @param node Pointer to the `XmlNodeBase` to test.
           * @param alloc The `XPathAllocator` for allocating `XPathNode` objects within `nsr`.
           * @return `true` if the node was pushed (matched the test), `false` otherwise.
           */
          bool step_push(XPathNodeSetRaw &nsr, XmlNodeBase *node, XPathAllocator *alloc) const;

          /**
           * @brief Fills a node set with nodes from a specific axis relative to a given XML node.
           * @details This template function traverses the XML document along the specified XPath `axis`
           *          starting from `node`, and adds all matching nodes (according to this AST node's
           *          `m_test` and `m_data.nodetest`) to the `nsr`. It handles various axis types
           *          (e.g., child, descendant, following-sibling, ancestor).
           * @tparam T An `axis_to_type` specialization indicating the axis to traverse.
           * @param nsr The `XPathNodeSetRaw` to fill with results. Modified in-place.
           * @param node The starting `XmlNodeBase` for the axis traversal.
           * @param alloc The `XPathAllocator` for node allocations.
           * @param once If `true`, stops filling after the first matching node is found.
           * @param val An unused placeholder argument to deduce the axis type `T`.
           * @note This function is a core part of XPath step evaluation.
           * @throws `LUMEX_ASSERT` if an unimplemented axis is encountered.
           */
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

          /**
           * @brief Fills a node set with nodes from a specific axis relative to an XML attribute.
           * @details This template function traverses the XML document along the specified XPath `axis`
           *          starting from `attr` (and its `node` parent), and adds all matching nodes
           *          to the `nsr`. It primarily handles axes that are relevant to attributes
           *          (e.g., ancestor, self, following, parent, preceding).
           * @tparam T An `axis_to_type` specialization indicating the axis to traverse.
           * @param nsr The `XPathNodeSetRaw` to fill with results. Modified in-place.
           * @param attr The starting `XmlAttributeBase` for the axis traversal.
           * @param node The parent `XmlNodeBase` of the attribute.
           * @param alloc The `XPathAllocator` for node allocations.
           * @param once If `true`, stops filling after the first matching node is found.
           * @param val An unused placeholder argument to deduce the axis type `T`.
           * @note This function handles special considerations for attributes, e.g.,
           *       `preceding::` axis does not include attribute nodes themselves.
           * @throws `LUMEX_ASSERT` if an unimplemented axis is encountered.
           */
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

          /**
           * @brief Fills a node set with nodes from a specific axis relative to a generic XPath node.
           * @details This overloaded template function dispatches to the appropriate `step_fill`
           *          overload based on whether the `path_node` represents an XML element node or
           *          an attribute node, considering which axes are applicable to attributes.
           * @tparam T An `axis_to_type` specialization indicating the axis to traverse.
           * @param nsr The `XPathNodeSetRaw` to fill with results. Modified in-place.
           * @param path_node The starting `XPathNode` (which can be either an element or an attribute).
           * @param alloc The `XPathAllocator` for node allocations.
           * @param once If `true`, stops filling after the first matching node is found.
           * @param val An unused placeholder argument to deduce the axis type `T`.
           */
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

          /**
           * @brief Performs the core evaluation for an XPath step.
           * @details This template function is responsible for evaluating a single XPath step,
           *          including traversing the specified axis from the context node(s),
           *          applying node tests, and then applying any associated predicates.
           *          It handles sorting and duplicate removal for the resulting node set.
           * @tparam T An `axis_to_type` specialization indicating the axis to traverse.
           * @param ctx The XPath context for evaluation.
           * @param stack The XPath stack for memory allocation.
           * @param eval The evaluation mode for the node set.
           * @param val An unused placeholder argument to deduce the axis type `T`.
           * @return An `XPathNodeSetRaw` containing the results of this step.
           */
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

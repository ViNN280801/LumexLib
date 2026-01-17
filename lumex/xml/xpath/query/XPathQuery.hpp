#ifndef LUMEX_XML_XPATH_QUERY_HPP
#define LUMEX_XML_XPATH_QUERY_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"
#include "lumex/xml/xpath/memory/XPathMemoryBlock.hpp"
#include "lumex/xml/xpath/parser/XPathParseResult.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Memory;
using namespace Lumex::Xml::XPath::Parser;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Ast
      {
        class XPathAstNode;
      }
      namespace Node
      {
        class XPathNode;
        class XPathNodeSet;
      }
      namespace Variable
      {
        class XPathVariableSet;
      }

      namespace Query
      {
        /**
         * @brief Represents a compiled XPath query ready for evaluation.
         * @details This class encapsulates a parsed and optimized XPath expression, allowing
         *          it to be evaluated against different XML nodes. It manages the underlying
         *          Abstract Syntax Tree (AST) and the memory associated with it.
         *          `XPathQuery` objects are immutable once constructed and can be
         *          reused for multiple evaluations.
         *
         * @note This class utilizes an internal `XPathQueryImpl` to manage its resources
         *       and provides a safe boolean conversion to check for successful parsing.
         *       It supports evaluating the query to various XPath value types (boolean, number,
         *       string, node-set).
         * @warning The query parsing and initial optimization occur during construction,
         *          which might throw `std::bad_alloc` or `XPathException` on failure.
         */
        class LUMEX_API XPathQuery
        {
        private:
          /// @brief Opaque pointer to the internal implementation details (`XPathQueryImpl`).
          void *m_impl;
          /// @brief The result of the XPath parsing operation, including any errors.
          XPathParseResult m_result;

          // Private copy constructor and assignment operator to prevent copying
          // and enforce move-only semantics, as the internal m_impl is a raw pointer.
          XPathQuery(XPathQuery const &);
          XPathQuery &operator=(XPathQuery const &);

        public:
          /**
           * @brief Type definition for safe boolean conversion.
           * @details This is a standard C++ idiom for enabling safe boolean conversions,
           *          preventing accidental implicit conversions to integral types.
           */
          using unspecified_bool_type = void (*)(XPathQuery ***);

          /**
           * @brief Constructs an XPathQuery by parsing the given query string.
           * @details This constructor parses the `query` string, builds the XPath AST,
           *          and optimizes it. If parsing or memory allocation fails, it throws
           *          an exception.
           * @param query A null-terminated C-style string containing the XPath expression.
           * @param variables An optional pointer to an `XPathVariableSet` for resolving
           *                  variable references in the XPath query. Defaults to `nullptr`.
           * @throws `std::bad_alloc` if memory allocation fails during parsing or AST construction.
           * @throws `XPathException` if the `query` string contains a syntax error or is otherwise invalid.
           */
          explicit XPathQuery(char_t const *query, Variable::XPathVariableSet *variables = nullptr);

          /**
           * @brief Default constructor. Constructs an empty (invalid) XPathQuery object.
           * @details An XPathQuery constructed with this default constructor will evaluate
           *          to `false` in boolean contexts and its evaluation methods will return
           *          default/empty values.
           */
          XPathQuery();

          /**
           * @brief Destructor.
           * @details Releases all memory associated with the compiled XPath query, including
           *          the AST and any internal buffers managed by `XPathQueryImpl`.
           */
          ~XPathQuery();

          /**
           * @brief Move constructor.
           * @details Efficiently transfers the ownership of the internal `XPathQueryImpl`
           *          from `rhs` to this object, leaving `rhs` in a valid but empty state.
           * @param rhs The `XPathQuery` object to move resources from.
           */
          XPathQuery(XPathQuery &&rhs) noexcept;
          /**
           * @brief Move assignment operator.
           * @details Efficiently transfers the ownership of the internal `XPathQueryImpl`
           *          from `rhs` to this object. Any resources currently held by this
           *          object are released before the transfer. `rhs` is left in a valid
           *          but empty state.
           * @param rhs The `XPathQuery` object to move resources from.
           * @return A reference to this `XPathQuery` object after the move.
           */
          XPathQuery &operator=(XPathQuery &&rhs) noexcept;

          /**
           * @brief Returns the expected return type of the compiled XPath query.
           * @details This indicates the type of value (boolean, number, string, node-set)
           *          that the XPath expression is designed to return.
           * @return An `xpath_value_type` enum value. Returns `xpath_type_none` if the
           *         query is invalid or empty.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Return type of XPath query should not be discarded.")
          xpath_value_type return_type() const;

          /**
           * @brief Evaluates the XPath query to a boolean value.
           * @details Evaluates the compiled XPath expression against the provided context node
           *          and converts the result to a boolean according to XPath conversion rules.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return The boolean result of the evaluation.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during evaluation.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Boolean evaluation result should not be discarded.")
          bool evaluate_boolean(Node::XPathNode const &n) const;

          /**
           * @brief Evaluates the XPath query to a numeric value.
           * @details Evaluates the compiled XPath expression against the provided context node
           *          and converts the result to a double-precision floating-point number
           *          according to XPath conversion rules.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return The numeric result of the evaluation. Returns NaN if the conversion is not possible.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during evaluation.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Numerical evaluation result should not be discarded.")
          double evaluate_number(Node::XPathNode const &n) const;

          /**
           * @brief Evaluates the XPath query to a string value.
           * @details Evaluates the compiled XPath expression against the provided context node
           *          and converts the result to a string according to XPath conversion rules.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return An `string_t` (string view) containing the result. The string data is
           *         managed internally and valid only for the duration of the evaluation call.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during evaluation.
           */
          LUMEX_ATTRIBUTE_NODISCARD("String evaluation result should not be discarded.")
          string_t evaluate_string(Node::XPathNode const &n) const;

          /**
           * @brief Evaluates the XPath query to a string value, writing to a provided buffer.
           * @details Evaluates the compiled XPath expression against the context node and
           *          writes the resulting string into the provided `buffer` up to `capacity`.
           *          The buffer will be null-terminated if `capacity > 0`.
           * @param buffer A character buffer where the string result will be written.
           * @param capacity The maximum number of characters (including null terminator) that can be written to
           * `buffer`.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return The total length of the string result (including null terminator) if there was enough capacity,
           *         or the required capacity if the buffer was too small.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during internal evaluation.
           * @note If `capacity` is 0, the function returns the required size without writing anything.
           */
          size_t evaluate_string(char_t *buffer, size_t capacity, Node::XPathNode const &n) const;

          /**
           * @brief Evaluates the XPath query to a node-set.
           * @details Evaluates the compiled XPath expression against the provided context node
           *          and returns a `XPathNodeSet` containing the resulting nodes.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return A `Node::XPathNodeSet` containing the nodes that match the query.
           * @throws `XPathException` if the expression does not evaluate to a node set.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during evaluation.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Node set evaluation result should not be discarded.")
          Node::XPathNodeSet evaluate_node_set(Node::XPathNode const &n) const;

          /**
           * @brief Evaluates the XPath query to a single node.
           * @details Evaluates the compiled XPath expression, which must return a node-set,
           *          and retrieves the first node from that set in document order.
           * @param n The `XPathNode` representing the context item for evaluation.
           * @return The first `Node::XPathNode` in the result set, or an empty `XPathNode` if the set is empty.
           * @throws `XPathException` if the expression does not evaluate to a node set.
           * @throws `std::bad_alloc` if an out-of-memory condition occurs during evaluation.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Single node evaluation result should not be discarded.")
          Node::XPathNode evaluate_node(Node::XPathNode const &n) const;

          /**
           * @brief Returns the parsing result of the XPath query.
           * @details Provides access to the `XPathParseResult` object which contains
           *          details about whether the query was successfully parsed and, if not,
           *          what error occurred and at what position.
           * @return A constant reference to the `Parser::XPathParseResult` object.
           */
          LUMEX_ATTRIBUTE_NODISCARD("Query result set should not be discarded.")
          Parser::XPathParseResult const &result() const;

          /**
           * @brief Safe boolean conversion operator.
           * @details Allows an `XPathQuery` object to be used in boolean contexts (e.g., `if (xpathQuery)`).
           *          It evaluates to `true` if the query was successfully parsed and compiled (`m_impl` is not
           * `nullptr`), and `false` otherwise. This prevents unintended implicit conversions.
           * @return A pointer to a dummy function if the query is valid, `nullptr` otherwise.
           */
          operator unspecified_bool_type() const;

          /**
           * @brief Logical NOT operator.
           * @details Returns `true` if this `XPathQuery` object is invalid (e.g., failed to parse),
           *          and `false` if it is a valid, compiled query.
           * @return `true` if the query is invalid, `false` otherwise.
           */
          bool operator!() const;
        };
      } // namespace Query
      /**
       * @brief Internal implementation structure for XPathQuery.
       * @details This struct holds the core components of a compiled XPath query,
       *          including the root of the Abstract Syntax Tree (AST), the memory
       *          allocator used for AST nodes, and a flag for out-of-memory conditions.
       *          It is managed by `XPathQuery` via an opaque pointer.
       *
       * @note This separation of interface (`XPathQuery`) from implementation (`XPathQueryImpl`)
       *       is a Pimpl (Pointer to Implementation) idiom, often used to hide implementation
       *       details, reduce compile-time dependencies, and provide binary compatibility.
       */
      struct XPathQueryImpl {
        /**
         * @brief Factory method to create a new `XPathQueryImpl` instance.
         * @details Allocates memory for a new `XPathQueryImpl` object and constructs it.
         * @return A pointer to the newly created `XPathQueryImpl` on success, or `nullptr` on memory allocation
         * failure.
         */
        static XPathQueryImpl *create();

        /**
         * @brief Destroys an `XPathQueryImpl` instance and releases its resources.
         * @details This function is responsible for freeing all memory associated with the
         *          `XPathQueryImpl`, including the AST nodes managed by its allocator.
         * @param impl A pointer to the `XPathQueryImpl` object to destroy.
         */
        static void destroy(XPathQueryImpl *impl);

        /**
         * @brief Constructs an `XPathQueryImpl` object.
         * @details Initializes the internal allocator with a memory block and sets up
         *          the out-of-memory flag.
         */
        XPathQueryImpl();

        /// @brief The root node of the XPath Abstract Syntax Tree. `nullptr` if parsing failed.
        Ast::XPathAstNode *root{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief The allocator used for managing memory for the AST nodes and related data.
        XPathAllocator alloc; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief The initial memory block used by the allocator.
        XPathMemoryBlock block{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Flag indicating if an out-of-memory condition occurred during parsing or optimization.
        bool oom{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_QUERY_HPP

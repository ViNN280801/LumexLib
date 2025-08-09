#ifndef LUMEX_XML_XPATH_NODE_HPP
#define LUMEX_XML_XPATH_NODE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/node/XmlNode.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Attribute;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Node
      {
        /**
         * @brief Represents a node in an XPath node-set, which can be either an XML element/document node or an XML
         * attribute.
         * @details This class provides a unified wrapper for `Lumex::Xml::Node::XmlNode` and
         *          `Lumex::Xml::Attribute::XmlAttribute`, allowing XPath expressions to operate
         *          on both types of nodes seamlessly. It maintains internal pointers to either
         *          an `XmlNode` or an `XmlAttribute` (and its parent `XmlNode` for attributes).
         *          It supports safe boolean conversion and comparison operations.
         *
         * @note An `XPathNode` is considered "empty" or "null" if it does not contain either
         *       a valid `XmlNode` or `XmlAttribute`.
         * @warning This class does not manage the lifetime of the underlying `XmlNode` or
         *          `XmlAttribute` objects; it only holds references. These underlying objects
         *          must be managed externally.
         */
        class LUMEX_API XPathNode
        {
        public:
          /**
           * @brief Type definition for safe boolean conversion.
           * @details This is a standard C++ idiom for enabling safe boolean conversions,
           *          preventing accidental implicit conversions to integral types.
           */
          using unspecified_bool_type = void (*)(XPathNode ***);

          /**
           * @brief Default constructor; constructs an empty XPath node.
           * @details Initializes an `XPathNode` instance that does not refer to any
           *          XML element, document, or attribute.
           */
          XPathNode() = default;

          /**
           * @brief Constructs an XPath node from an XML node.
           * @details Initializes an `XPathNode` that wraps a `Lumex::Xml::Node::XmlNode`.
           * @param node The `Lumex::Xml::Node::XmlNode` to wrap.
           */
          XPathNode(Lumex::Xml::Node::XmlNode const &node);
          /**
           * @brief Constructs an XPath node from an XML attribute and its parent.
           * @details Initializes an `XPathNode` that wraps a `Lumex::Xml::Attribute::XmlAttribute`.
           *          The parent `XmlNode` is also provided as attributes do not have direct
           *          parent pointers but are associated with an element.
           * @param attribute The `Lumex::Xml::Attribute::XmlAttribute` to wrap.
           * @param parent The `Lumex::Xml::Node::XmlNode` that is the parent of the attribute.
           *               This is stored internally to allow `parent()` queries for attributes.
           */
          XPathNode(Lumex::Xml::Attribute::XmlAttribute const &attribute, Lumex::Xml::Node::XmlNode const &parent);

          /**
           * @brief Gets the wrapped `XmlNode` if this `XPathNode` represents an element/document node.
           * @details If this `XPathNode` holds an `XmlAttribute`, this function returns an
           *          empty `XmlNode`.
           * @return The `Lumex::Xml::Node::XmlNode` if present, otherwise an empty `XmlNode`.
           */
          // The returned XML node from the XPath evaluation should be used; discarding it
          // negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD("The returned XML node from the XPath evaluation should be used; discarding it "
                                    "negates the purpose of the getter.")
          Lumex::Xml::Node::XmlNode node() const;

          /**
           * @brief Gets the wrapped `XmlAttribute` if this `XPathNode` represents an attribute.
           * @details If this `XPathNode` holds an `XmlNode`, this function returns an
           *          empty `XmlAttribute`.
           * @return The `Lumex::Xml::Attribute::XmlAttribute` if present, otherwise an empty `XmlAttribute`.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned XML attribute from the XPath evaluation should be used; discarding it "
            "negates the purpose of the getter.")
          Lumex::Xml::Attribute::XmlAttribute attribute() const;

          /**
           * @brief Gets the parent `XmlNode` of the wrapped node or attribute.
           * @details If this `XPathNode` wraps an `XmlNode`, it returns that node's parent.
           *          If it wraps an `XmlAttribute`, it returns the parent `XmlNode` that
           *          was provided during construction.
           * @return The parent `Lumex::Xml::Node::XmlNode`. Returns an empty `XmlNode`
           *         if the current node is a document root or if no parent exists.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned parent XML node should be used; discarding it negates the purpose of the getter.")
          Lumex::Xml::Node::XmlNode parent() const;

          /**
           * @brief Safe boolean conversion operator.
           * @details Allows an `XPathNode` object to be used in boolean contexts (e.g., `if (xpathNode)`).
           *          It evaluates to `true` if the `XPathNode` wraps a valid `XmlNode` or `XmlAttribute`,
           *          and `false` otherwise. This prevents unintended implicit conversions.
           * @return A pointer to a dummy function if the node is valid, `nullptr` otherwise.
           */
          operator unspecified_bool_type() const;

          /**
           * @brief Logical NOT operator.
           * @details Returns `true` if this `XPathNode` is empty (does not wrap any XML node or attribute),
           *          and `false` otherwise.
           * @return `true` if the node is empty, `false` otherwise.
           */
          bool operator!() const;

          /**
           * @brief Equality comparison operator.
           * @details Compares two `XPathNode` objects for equality based on their
           *          wrapped `XmlNode` and `XmlAttribute` instances.
           * @param n The `XPathNode` to compare with.
           * @return `true` if both `m_node` and `m_attribute` members are equal, `false` otherwise.
           */
          bool operator==(XPathNode const &n) const;
          /**
           * @brief Inequality comparison operator.
           * @details Compares two `XPathNode` objects for inequality.
           * @param n The `XPathNode` to compare with.
           * @return `true` if either `m_node` or `m_attribute` members are not equal, `false` otherwise.
           */
          bool operator!=(XPathNode const &n) const;

        private:
          /// @brief Internal storage for the wrapped XML node. Used when representing an element or document.
          Lumex::Xml::Node::XmlNode m_node;
          /// @brief Internal storage for the wrapped XML attribute. Used when representing an attribute.
          Lumex::Xml::Attribute::XmlAttribute m_attribute;
        };

        /**
         * @brief Logical AND operator for `XPathNode` and boolean.
         * @details Enables the use of `XPathNode` in logical AND expressions (`&&`)
         *          with a boolean right-hand side, treating the `XPathNode` as a boolean.
         * @param lhs The left-hand side `XPathNode`.
         * @param rhs The right-hand side boolean value.
         * @return The result of the logical AND operation.
         */
        LUMEX_API
        bool operator&&(XPathNode const &lhs, bool rhs);

        /**
         * @brief Logical OR operator for `XPathNode` and boolean.
         * @details Enables the use of `XPathNode` in logical OR expressions (`||`)
         *          with a boolean right-hand side, treating the `XPathNode` as a boolean.
         * @param lhs The left-hand side `XPathNode`.
         * @param rhs The right-hand side boolean value.
         * @return The result of the logical OR operation.
         */
        LUMEX_API
        bool operator||(XPathNode const &lhs, bool rhs);
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_HPP

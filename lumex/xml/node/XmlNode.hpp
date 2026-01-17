#ifndef LUMEX_XML_NODE_HPP
#define LUMEX_XML_NODE_HPP

#include "lumex/LumexExport.hpp"

#include <iterator>

#include "lumex/core/utility/LumexUtility"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/range/XmlObjectRange.hpp"
#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/writer/IXmlWriter.hpp"
#include "lumex/xml/writer/XmlBufferedWriter.hpp"

#include "XmlNodeBase.hpp"

// Forward declarations for iterators - definitions after XmlNode class
namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      class XmlNodeIterator;
      class XmlNamedNodeIterator;
    }
    namespace Attribute
    {
      class XmlAttributeIterator;
    }
  }
}

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Writer;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    // Forward declarations
    namespace XPath // NOLINT(modernize-concat-nested-namespaces)
    {
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
        class XPathQuery;
      }
      namespace Node
      {
        class XPathNode;
        class XPathNodeSet;
      }
    }
    namespace Attribute
    {
      class XmlAttributeIterator;
    }
    namespace Text
    {
      class XmlText;
    }
    namespace Tree
    {
      class XmlTreeWalker;
    }

    using namespace XPath::Node;
    using namespace XPath::Variable;
    using namespace XPath::Query;
    using namespace Attribute;
    using namespace Text;
    using namespace Tree;
    // End of forward declarations

    namespace Node
    {
      /**
       * @brief Represents a single XML node in the DOM tree, providing a high-level interface for
       *        accessing and manipulating its properties, children, and attributes.
       * @details This class is a lightweight wrapper around an internal `XmlNodeBase` pointer. It offers
       *          methods for traversal, content access (name, value), modification (setting name/value,
       *          adding/removing children/attributes), and advanced features like XPath queries and tree traversal.
       *          It implements value-like semantics for safety and ease of use, while internally operating on
       *          raw pointers managed by `XmlDocument`'s allocator.
       * @note `XmlNode` objects do not own the memory they point to; their lifetime is tied to the `XmlDocument`
       *       from which they originate. Accessing a node after its owning document is destroyed leads to undefined
       * behavior.
       * @see XmlNodeBase
       * @see XmlAttribute
       * @see XmlDocument
       */
      class LUMEX_API XmlNode
      {
        friend class Tree::XmlTreeWalker;
        friend class Attribute::XmlAttribute;
        friend class Attribute::XmlAttributeIterator;
        friend class XmlNodeIterator;
        friend class XmlNamedNodeIterator;

      public:
        using unspecified_bool_type = void (*)(XmlNode ***);

        /**
         * @brief Default constructor. Constructs an empty (null) node.
         * @details An empty node does not point to any valid XML element and its methods will generally return
         *          default values or empty objects.
         */
        XmlNode();

        /**
         * @brief Constructs an `XmlNode` from an internal `XmlNodeBase` pointer.
         * @param[in] ptr A pointer to the underlying `XmlNodeBase` object. This pointer is not owned by the `XmlNode`
         * instance.
         * @details This constructor is typically used internally to wrap raw node pointers obtained from parsing or
         * tree manipulation functions.
         * @note This constructor is explicit to prevent unintended conversions from raw pointers.
         */
        explicit XmlNode(XmlNodeBase *ptr);

        /**
         * @brief Provides safe boolean conversion for `XmlNode` objects.
         * @details Allows an `XmlNode` object to be used in boolean contexts (e.g., `if (node)`).
         *          It evaluates to `true` if the node points to a valid `XmlNodeBase` object (i.e., not null), and
         * `false` otherwise.
         * @return A pointer to a dummy function if the internal node pointer is not null, otherwise `nullptr`.
         * @note This conversion prevents problematic implicit conversions to arithmetic types.
         */
        operator unspecified_bool_type() const;

        // Borland C++ workaround
        /**
         * @brief Overloads the logical NOT operator.
         * @details Provides a convenient way to check if an `XmlNode` object is empty.
         * @return `true` if the node is empty (internal pointer is null), `false` otherwise.
         * @see empty()
         */
        bool operator!() const;

        // Comparison operators (compares wrapped node pointers)
        /**
         * @brief Compares two `XmlNode` objects for equality.
         * @details Two `XmlNode` objects are considered equal if they wrap the same underlying `XmlNodeBase` pointer.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if both nodes point to the same internal `XmlNodeBase`, `false` otherwise.
         * @note This performs a pointer comparison, not a value comparison of the node's name or value.
         */
        bool operator==(XmlNode const &other) const;
        /**
         * @brief Compares two `XmlNode` objects for inequality.
         * @details Two `XmlNode` objects are considered unequal if they wrap different underlying `XmlNodeBase`
         * pointers.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if the nodes point to different internal `XmlNodeBase` objects, `false` otherwise.
         * @note This performs a pointer comparison, not a value comparison of the node's name or value.
         */
        bool operator!=(XmlNode const &other) const;
        /**
         * @brief Compares two `XmlNode` objects using the less-than operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlNodeBase` pointers.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if the internal pointer of this node is less than that of the `other` node, `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator<(XmlNode const &other) const;
        /**
         * @brief Compares two `XmlNode` objects using the greater-than operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlNodeBase` pointers.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if the internal pointer of this node is greater than that of the `other` node, `false`
         * otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator>(XmlNode const &other) const;
        /**
         * @brief Compares two `XmlNode` objects using the less-than-or-equal-to operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlNodeBase` pointers.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if the internal pointer of this node is less than or equal to that of the `other` node,
         * `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator<=(XmlNode const &other) const;
        /**
         * @brief Compares two `XmlNode` objects using the greater-than-or-equal-to operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlNodeBase` pointers.
         * @param[in] other The `XmlNode` object to compare with.
         * @return `true` if the internal pointer of this node is greater than or equal to that of the `other` node,
         * `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator>=(XmlNode const &other) const;

        // Check if node is empty (null)
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned boolean indicates whether the node is empty; discarding it negates the purpose of the getter.")
        /**
         * @brief Checks if the `XmlNode` object is empty.
         * @details A node is considered empty if its internal `m_root` pointer is `nullptr`.
         * @return `true` if the node is empty, `false` otherwise.
         * @note This method is equivalent to `!operator bool()`.
         */
        bool empty() const;

        // Get node type
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned node type should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the type of the XML node.
         * @details Returns an `xml_node_type` enumeration value indicating whether the node is an element, text,
         * comment, etc. If the node is empty (`nullptr`), `node_null` is returned.
         * @return An `xml_node_type` value.
         * @see xml_node_type
         */
        xml_node_type type() const;

        // Get node name, or "" if node is empty or it has no name
        // Note: For <node>text</node> node.value() does not return "text"! Use child_value() or text() methods to
        // access text inside nodes.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string name should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the name of the XML node.
         * @details If the node is valid and has a name (e.g., element, processing instruction, declaration),
         *          this returns a C-style string pointer to it. Returns `LUMEX_XML_TEXT("")` for empty nodes or node
         * types without names (e.g., PCDATA, CDATA).
         * @return A null-terminated C-style string representing the node's name.
         * @note The returned pointer points to internal memory and should not be deallocated or modified. Its lifetime
         * is tied to the XML document.
         * @warning This method does NOT return the text content of element nodes (e.g., for `<tag>text</tag>`, `name()`
         * returns `tag`, not `text`). Use `child_value()` or `text()` for text content.
         */
        char_t const *name() const;

        // Get node value, or "" if node is empty or it has no value
        // Note: For <node>text</node> node.value() does not return "text"! Use child_value() or text() methods to
        // access text inside nodes.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string value should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the value of the XML node.
         * @details Returns a C-style string pointer to the node's value. Valid for node types like `PCDATA`, `CDATA`,
         *          `comment`, `doctype`, `pi` (processing instruction). For element nodes, this typically returns
         * `nullptr` unless `kparse_embed_pcdata` was used during parsing. Returns `LUMEX_XML_TEXT("")` for empty nodes
         * or node types without a direct value.
         * @return A null-terminated C-style string representing the node's value.
         * @note The returned pointer points to internal memory and should not be deallocated or modified. Its lifetime
         * is tied to the XML document.
         * @warning For element nodes (`<node>text</node>`), `value()` generally returns an empty string unless parsing
         * options were used to embed PCDATA. Use `child_value()` or `text()` to access contained text.
         */
        char_t const *value() const;

        // Get attribute list
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the first attribute of the node.
         * @details If the node is valid and has attributes, returns an `XmlAttribute` object representing the first
         * attribute. Otherwise, returns an empty `XmlAttribute`.
         * @return The first `XmlAttribute` of the node, or an empty `XmlAttribute` if none exists or the node is empty.
         * @see last_attribute()
         * @see attributes()
         */
        XmlAttribute first_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the last attribute of the node.
         * @details If the node is valid and has attributes, returns an `XmlAttribute` object representing the last
         * attribute. Otherwise, returns an empty `XmlAttribute`. This uses the circular `prev_attribute_c` pointer from
         * `first_attribute`.
         * @return The last `XmlAttribute` of the node, or an empty `XmlAttribute` if none exists or the node is empty.
         * @see first_attribute()
         * @see attributes()
         */
        XmlAttribute last_attribute() const;

        // Get children list
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the first child node of the current node.
         * @details If the node is valid and has children, returns an `XmlNode` object representing its first child.
         *          Otherwise, returns an empty `XmlNode`.
         * @return The first `XmlNode` child, or an empty `XmlNode` if no children exist or the node is empty.
         * @see last_child()
         * @see children()
         */
        XmlNode first_child() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the last child node of the current node.
         * @details If the node is valid and has children, returns an `XmlNode` object representing its last child.
         *          Otherwise, returns an empty `XmlNode`. This uses the circular `prev_sibling_c` pointer from
         * `first_child`.
         * @return The last `XmlNode` child, or an empty `XmlNode` if no children exist or the node is empty.
         * @see first_child()
         * @see children()
         */
        XmlNode last_child() const;

        // Get next/previous sibling in the children list of the parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the next sibling node of the current node.
         * @details If the node is valid and has a next sibling, returns an `XmlNode` object representing it.
         *          Otherwise, returns an empty `XmlNode`.
         * @return The next `XmlNode` sibling, or an empty `XmlNode` if no next sibling exists or the node is empty.
         * @see previous_sibling()
         */
        XmlNode next_sibling() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the previous sibling node of the current node.
         * @details If the node is valid and has a previous sibling, returns an `XmlNode` object representing it.
         *          Otherwise, returns an empty `XmlNode`.
         * @return The previous `XmlNode` sibling, or an empty `XmlNode` if no previous sibling exists or the node is
         * empty.
         * @see next_sibling()
         */
        XmlNode previous_sibling() const;

        // Get parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the parent node of the current node.
         * @details If the node is valid and has a parent, returns an `XmlNode` object representing it.
         *          The document node itself has no parent.
         * @return The parent `XmlNode`, or an empty `XmlNode` if no parent exists (e.g., for the document node or an
         * empty node).
         */
        XmlNode parent() const;

        // Get root of DOM tree this node belongs to
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the root of the DOM tree to which this node belongs.
         * @details For any valid node, this method returns an `XmlNode` object representing the `XmlDocument`
         *          that owns the node. If the current node is empty, an empty `XmlNode` is returned.
         * @return An `XmlNode` representing the owning document, or an empty `XmlNode` if this node is invalid.
         * @note The `XmlDocument` object itself is a special type of node (`node_document`).
         */
        XmlNode root() const;

        // Get text object for the current node
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned text object should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves a text object (`XmlText`) associated with the current node.
         * @details The `XmlText` object provides methods to access and modify the text content of a node,
         *          especially for PCDATA and CDATA nodes. For element nodes, it simplifies accessing child text.
         * @return An `XmlText` object that can be used to query or manipulate the node's text.
         * @note The `XmlText` object is a wrapper and does not own the text data; its lifetime is tied to the node.
         */
        XmlText text() const;

        // Get child, attribute or next/previous sibling with the specified name
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the first child node with the specified name.
         * @param[in] name A null-terminated C-style string representing the name of the child node to find.
         * @return An `XmlNode` object representing the first child found with the matching name, or an empty `XmlNode`
         * if no such child exists or the current node is empty.
         * @note The search is case-sensitive.
         * @see children(char_t const*)
         */
        XmlNode child(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the first attribute with the specified name.
         * @param[in] name A null-terminated C-style string representing the name of the attribute to find.
         * @return An `XmlAttribute` object representing the first attribute found with the matching name, or an empty
         * `XmlAttribute` if no such attribute exists or the current node is empty.
         * @note The search is case-sensitive.
         */
        XmlAttribute attribute(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the next sibling node with the specified name.
         * @param[in] name A null-terminated C-style string representing the name of the sibling node to find.
         * @return An `XmlNode` object representing the next sibling found with the matching name, or an empty `XmlNode`
         * if no such sibling exists or the current node is empty.
         * @note The search is case-sensitive and starts from the current node's next sibling.
         */
        XmlNode next_sibling(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the previous sibling node with the specified name.
         * @param[in] name A null-terminated C-style string representing the name of the sibling node to find.
         * @return An `XmlNode` object representing the previous sibling found with the matching name, or an empty
         * `XmlNode` if no such sibling exists or the current node is empty.
         * @note The search is case-sensitive and starts from the current node's previous sibling.
         */
        XmlNode previous_sibling(char_t const *name) const;

#if __cplusplus >= 201703L
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the first child node with the specified name using `std::string_view`.
         * @param[in] name A `std::string_view` representing the name of the child node to find.
         * @return An `XmlNode` object representing the first child found with the matching name, or an empty `XmlNode`
         * if no such child exists or the current node is empty.
         * @note The search is case-sensitive. Available only when compiled with C++17 or later.
         */
        XmlNode child(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the first attribute with the specified name using `std::string_view`.
         * @param[in] name A `std::string_view` representing the name of the attribute to find.
         * @return An `XmlAttribute` object representing the first attribute found with the matching name, or an empty
         * `XmlAttribute` if no such attribute exists or the current node is empty.
         * @note The search is case-sensitive. Available only when compiled with C++17 or later.
         */
        XmlAttribute attribute(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the next sibling node with the specified name using `std::string_view`.
         * @param[in] name A `std::string_view` representing the name of the sibling node to find.
         * @return An `XmlNode` object representing the next sibling found with the matching name, or an empty `XmlNode`
         * if no such sibling exists or the current node is empty.
         * @note The search is case-sensitive and starts from the current node's next sibling. Available only when
         * compiled with C++17 or later.
         */
        XmlNode next_sibling(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Finds the previous sibling node with the specified name using `std::string_view`.
         * @param[in] name A `std::string_view` representing the name of the sibling node to find.
         * @return An `XmlNode` object representing the previous sibling found with the matching name, or an empty
         * `XmlNode` if no such sibling exists or the current node is empty.
         * @note The search is case-sensitive and starts from the current node's previous sibling. Available only when
         * compiled with C++17 or later.
         */
        XmlNode previous_sibling(string_view_t name) const;
#endif

        // Get attribute, starting the search from a hint (and updating hint so that searching for a sequence of
        // attributes is fast)
        /**
         * @brief Finds an attribute by name, optimizing search with a hint.
         * @param[in] name A null-terminated C-style string representing the name of the attribute to find.
         * @param[in,out] hint An `XmlAttribute` object that serves as a starting point for the search. If found,
         *                     `hint` is updated to point to the *next* attribute, optimizing subsequent searches for
         *                     attributes in sequence. `hint` should ideally be an attribute of the current node.
         * @return An `XmlAttribute` object representing the found attribute, or an empty `XmlAttribute` if not found.
         * @note The search starts from `hint` and wraps around to the beginning of the attribute list if not found
         * initially.
         * @warning If `hint` is not an attribute of `this` node, the behavior is undefined in release builds (asserts
         * in debug).
         */
        XmlAttribute attribute(char_t const *name, XmlAttribute &hint) const;
#if __cplusplus >= 201703L
        /**
         * @brief Finds an attribute by name using `std::string_view`, optimizing search with a hint.
         * @param[in] name A `std::string_view` representing the name of the attribute to find.
         * @param[in,out] hint An `XmlAttribute` object serving as a search starting point, updated to the next
         * attribute on success.
         * @return An `XmlAttribute` object representing the found attribute, or an empty `XmlAttribute` if not found.
         * @note Available only when compiled with C++17 or later.
         * @warning If `hint` is not an attribute of `this` node, the behavior is undefined in release builds (asserts
         * in debug).
         */
        XmlAttribute attribute(string_view_t name, XmlAttribute &hint) const;
#endif

        // Get child value of current node; that is, value of the first child node of type PCDATA/CDATA
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string child value should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the text content of the first child node that is of type `PCDATA` or `CDATA`.
         * @details For an element node, this is the most common way to get its contained text.
         *          If the current node is an element and `kparse_embed_pcdata` was used, it might return the `value()`
         * of the element itself. Returns `LUMEX_XML_TEXT("")` if no such child exists or the node is empty.
         * @return A null-terminated C-style string representing the child's value.
         * @note The returned pointer points to internal memory and should not be deallocated or modified.
         */
        char_t const *child_value() const;

        // Get child value of child with specified name. Equivalent to child(name).child_value().
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string child value should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the text content of a named child node.
         * @param[in] name A null-terminated C-style string representing the name of the child element.
         * @return The text content of the first child element matching `name`, or `LUMEX_XML_TEXT("")` if not found or
         * no text content exists.
         * @details This is a convenience method equivalent to `child(name).child_value()`.
         */
        char_t const *child_value(char_t const *name) const;

        // Set node name/value (returns false if node is empty, there is not enough memory, or node can not have
        // name/value)
        /**
         * @brief Sets the name of the XML node.
         * @param[in] rhs The new name for the node as a null-terminated C-style string.
         * @return `true` if the name was successfully set, `false` otherwise (e.g., if the node is empty,
         *         does not support names, or there is insufficient memory).
         * @details This function is applicable to node types such as `node_element`, `node_pi`, and `node_declaration`.
         *          It internally handles memory allocation/reallocation for the name string.
         * @note The `rhs` string is copied into the node's internal memory.
         */
        bool set_name(char_t const *rhs);

        /**
         * @brief Sets the name of the XML node from a C-style string with a specified size.
         * @param[in] rhs The new name for the node.
         * @param[in] size The number of characters to copy from `rhs`.
         * @return `true` if the name was successfully set, `false` otherwise.
         * @details Similar to `set_name(char_t const*)` but allows specifying the length, which can be useful for
         *          non-null-terminated strings or when only a prefix is desired.
         */
        bool set_name(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        /**
         * @brief Sets the name of the XML node from a `std::string_view`.
         * @param[in] rhs The new name for the node as a `std::string_view`.
         * @return `true` if the name was successfully set, `false` otherwise.
         * @details Provides a modern C++ interface for setting node names, leveraging `std::string_view` for
         * efficiency.
         * @note Available only when compiled with C++17 or later.
         */
        bool set_name(string_view_t rhs);
#endif

        /**
         * @brief Sets the value of the XML node.
         * @param[in] rhs The new value for the node as a null-terminated C-style string.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the node is empty,
         *         does not support values, or there is insufficient memory).
         * @details This function is applicable to node types like `node_pcdata`, `node_cdata`, `node_comment`,
         * `node_pi`, and `node_doctype`. It internally handles memory allocation/reallocation for the value string.
         * @note The `rhs` string is copied into the node's internal memory.
         */
        bool set_value(char_t const *rhs);

        /**
         * @brief Sets the value of the XML node from a C-style string with a specified size.
         * @param[in] rhs The new value for the node.
         * @param[in] size The number of characters to copy from `rhs`.
         * @return `true` if the value was successfully set, `false` otherwise.
         * @details Similar to `set_value(char_t const*)` but allows specifying the length.
         */
        bool set_value(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        /**
         * @brief Sets the value of the XML node from a `std::string_view`.
         * @param[in] rhs The new value for the node as a `std::string_view`.
         * @return `true` if the value was successfully set, `false` otherwise.
         * @details Provides a modern C++ interface for setting node values, leveraging `std::string_view`.
         * @note Available only when compiled with C++17 or later.
         */
        bool set_value(string_view_t rhs);
#endif

        // Add attribute with specified name. Returns added attribute, or empty attribute on errors.
        /**
         * @brief Appends a new attribute with the specified name to the current node.
         * @param[in] name A null-terminated C-style string for the new attribute's name.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` if the
         * current node cannot have attributes or memory allocation fails.
         * @details This function allocates a new `XmlAttributeBase` structure and links it to the end of the current
         * node's attribute list. The name is then set using `XmlAttribute::set_name`.
         */
        XmlAttribute append_attribute(char_t const *name);

        /**
         * @brief Prepends a new attribute with the specified name to the current node.
         * @param[in] name A null-terminated C-style string for the new attribute's name.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` if the
         * current node cannot have attributes or memory allocation fails.
         * @details This function allocates a new `XmlAttributeBase` structure and links it to the beginning of the
         * current node's attribute list. The name is then set using `XmlAttribute::set_name`.
         */
        XmlAttribute prepend_attribute(char_t const *name);

        /**
         * @brief Inserts a new attribute with the specified name after a given existing attribute.
         * @param[in] name A null-terminated C-style string for the new attribute's name.
         * @param[in] attr The existing `XmlAttribute` after which the new attribute will be inserted. This attribute
         * must belong to the current node.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @details Returns an empty `XmlAttribute` if the current node cannot have attributes, `attr` does not belong
         *          to this node, or memory allocation fails.
         */
        XmlAttribute insert_attribute_after(char_t const *name, XmlAttribute const &attr);

        /**
         * @brief Inserts a new attribute with the specified name before a given existing attribute.
         * @param[in] name A null-terminated C-style string for the new attribute's name.
         * @param[in] attr The existing `XmlAttribute` before which the new attribute will be inserted. This attribute
         * must belong to the current node.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @details Returns an empty `XmlAttribute` if the current node cannot have attributes, `attr` does not belong
         *          to this node, or memory allocation fails.
         */
        XmlAttribute insert_attribute_before(char_t const *name, XmlAttribute const &attr);

#if __cplusplus >= 201703L
        /**
         * @brief Appends a new attribute with the specified name (`std::string_view`) to the current node.
         * @param[in] name A `std::string_view` for the new attribute's name.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlAttribute append_attribute(string_view_t name);

        /**
         * @brief Prepends a new attribute with the specified name (`std::string_view`) to the current node.
         * @param[in] name A `std::string_view` for the new attribute's name.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlAttribute prepend_attribute(string_view_t name);

        /**
         * @brief Inserts a new attribute with the specified name (`std::string_view`) after a given existing attribute.
         * @param[in] name A `std::string_view` for the new attribute's name.
         * @param[in] attr The existing `XmlAttribute` after which the new attribute will be inserted.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlAttribute insert_attribute_after(string_view_t name, XmlAttribute const &attr);

        /**
         * @brief Inserts a new attribute with the specified name (`std::string_view`) before a given existing
         * attribute.
         * @param[in] name A `std::string_view` for the new attribute's name.
         * @param[in] attr The existing `XmlAttribute` before which the new attribute will be inserted.
         * @return An `XmlAttribute` object representing the newly added attribute, or an empty `XmlAttribute` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlAttribute insert_attribute_before(string_view_t name, XmlAttribute const &attr);
#endif

        // Add a copy of the specified attribute. Returns added attribute, or empty attribute on errors.
        /**
         * @brief Appends a copy of an existing attribute (`proto`) to the current node.
         * @param[in] proto The `XmlAttribute` to be copied. Must be a valid attribute.
         * @return An `XmlAttribute` object representing the newly added copy, or an empty `XmlAttribute` on error.
         * @details Returns an empty `XmlAttribute` if `proto` is invalid, the current node cannot have attributes,
         *          or memory allocation fails during the copy.
         */
        XmlAttribute append_copy(XmlAttribute const &proto);

        /**
         * @brief Prepends a copy of an existing attribute (`proto`) to the current node.
         * @param[in] proto The `XmlAttribute` to be copied. Must be a valid attribute.
         * @return An `XmlAttribute` object representing the newly added copy, or an empty `XmlAttribute` on error.
         */
        XmlAttribute prepend_copy(XmlAttribute const &proto);

        /**
         * @brief Inserts a copy of an existing attribute (`proto`) after a given existing attribute (`attr`).
         * @param[in] proto The `XmlAttribute` to be copied. Must be a valid attribute.
         * @param[in] attr The existing `XmlAttribute` after which the copy will be inserted. This attribute must belong
         * to the current node.
         * @return An `XmlAttribute` object representing the newly added copy, or an empty `XmlAttribute` on error.
         */
        XmlAttribute insert_copy_after(XmlAttribute const &proto, XmlAttribute const &attr);

        /**
         * @brief Inserts a copy of an existing attribute (`proto`) before a given existing attribute (`attr`).
         * @param[in] proto The `XmlAttribute` to be copied. Must be a valid attribute.
         * @param[in] attr The existing `XmlAttribute` before which the copy will be inserted. This attribute must
         * belong to the current node.
         * @return An `XmlAttribute` object representing the newly added copy, or an empty `XmlAttribute` on error.
         */
        XmlAttribute insert_copy_before(XmlAttribute const &proto, XmlAttribute const &attr);

        // Add child node with specified type. Returns added node, or empty node on errors.
        /**
         * @brief Appends a new child node of a specified type to the current node.
         * @param[in] type The `xml_node_type` of the new child node (e.g., `node_element`, `node_pcdata`). Defaults to
         * `node_element`.
         * @return An `XmlNode` object representing the newly added child node, or an empty `XmlNode` on error.
         * @details Returns an empty `XmlNode` if the current node cannot have children of the specified `type` or
         * memory allocation fails.
         */
        XmlNode append_child(xml_node_type type = node_element);

        /**
         * @brief Prepends a new child node of a specified type to the current node.
         * @param[in] type The `xml_node_type` of the new child node. Defaults to `node_element`.
         * @return An `XmlNode` object representing the newly added child node, or an empty `XmlNode` on error.
         */
        XmlNode prepend_child(xml_node_type type = node_element);

        /**
         * @brief Inserts a new child node of a specified type before a given existing child node.
         * @param[in] type The `xml_node_type` of the new child node.
         * @param[in] node The existing `XmlNode` child before which the new node will be inserted. This node must be a
         * direct child of the current node.
         * @return An `XmlNode` object representing the newly added child node, or an empty `XmlNode` on error.
         */
        XmlNode insert_child_after(xml_node_type type, XmlNode const &node);

        /**
         * @brief Inserts a new child node of a specified type after a given existing child node.
         * @param[in] type The `xml_node_type` of the new child node.
         * @param[in] node The existing `XmlNode` child after which the new node will be inserted. This node must be a
         * direct child of the current node.
         * @return An `XmlNode` object representing the newly added child node, or an empty `XmlNode` on error.
         */
        XmlNode insert_child_before(xml_node_type type, XmlNode const &node);

        // Add child element with specified name. Returns added node, or empty node on errors.
        /**
         * @brief Appends a new child element with the specified name to the current node.
         * @param[in] name A null-terminated C-style string for the new child element's name.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @details This is a convenience method that calls `append_child(node_element)` and then `set_name(name)`.
         */
        XmlNode append_child(char_t const *name);

        /**
         * @brief Prepends a new child element with the specified name to the current node.
         * @param[in] name A null-terminated C-style string for the new child element's name.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @details This is a convenience method that calls `prepend_child(node_element)` and then `set_name(name)`.
         */
        XmlNode prepend_child(char_t const *name);

        /**
         * @brief Inserts a new child element with the specified name after a given existing child node.
         * @param[in] name A null-terminated C-style string for the new child element's name.
         * @param[in] node The existing `XmlNode` child after which the new element will be inserted.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @details This is a convenience method that calls `insert_child_after(node_element, node)` and then
         * `set_name(name)`.
         */
        XmlNode insert_child_after(char_t const *name, XmlNode const &node);

        /**
         * @brief Inserts a new child element with the specified name before a given existing child node.
         * @param[in] name A null-terminated C-style string for the new child element's name.
         * @param[in] node The existing `XmlNode` child before which the new element will be inserted.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @details This is a convenience method that calls `insert_child_before(node_element, node)` and then
         * `set_name(name)`.
         */
        XmlNode insert_child_before(char_t const *name, XmlNode const &node);

#if __cplusplus >= 201703L
        /**
         * @brief Appends a new child element with the specified name (`std::string_view`) to the current node.
         * @param[in] name A `std::string_view` for the new child element's name.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlNode append_child(string_view_t name);

        /**
         * @brief Prepends a new child element with the specified name (`std::string_view`) to the current node.
         * @param[in] name A `std::string_view` for the new child element's name.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlNode prepend_child(string_view_t name);

        /**
         * @brief Inserts a new child element with the specified name (`std::string_view`) after a given existing child
         * node.
         * @param[in] name A `std::string_view` for the new child element's name.
         * @param[in] node The existing `XmlNode` child after which the new element will be inserted.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlNode insert_child_after(string_view_t, XmlNode const &node);

        /**
         * @brief Inserts a new child element with the specified name (`std::string_view`) before a given existing child
         * node.
         * @param[in] name A `std::string_view` for the new child element's name.
         * @param[in] node The existing `XmlNode` child before which the new element will be inserted.
         * @return An `XmlNode` object representing the newly added element, or an empty `XmlNode` on error.
         * @note Available only when compiled with C++17 or later.
         */
        XmlNode insert_child_before(string_view_t name, XmlNode const &node);
#endif

        // Add a copy of the specified node as a child. Returns added node, or empty node on errors.
        /**
         * @brief Appends a deep copy of an existing `XmlNode` (`proto`) as a child of the current node.
         * @param[in] proto The `XmlNode` to be copied. Its entire subtree (children and attributes) will be duplicated.
         * @return An `XmlNode` object representing the newly added copy, or an empty `XmlNode` on error.
         * @details Returns an empty `XmlNode` if the current node cannot have children of `proto`'s type or memory
         * allocation fails.
         * @note The copied node and its subtree will be allocated from the current document's allocator.
         */
        XmlNode append_copy(XmlNode const &proto);

        /**
         * @brief Prepends a deep copy of an existing `XmlNode` (`proto`) as a child of the current node.
         * @param[in] proto The `XmlNode` to be copied. Its entire subtree will be duplicated.
         * @return An `XmlNode` object representing the newly added copy, or an empty `XmlNode` on error.
         */
        XmlNode prepend_copy(XmlNode const &proto);

        /**
         * @brief Inserts a deep copy of an existing `XmlNode` (`proto`) after a given existing child node.
         * @param[in] proto The `XmlNode` to be copied.
         * @param[in] node The existing `XmlNode` child after which the copy will be inserted. Must be a direct child of
         * the current node.
         * @return An `XmlNode` object representing the newly added copy, or an empty `XmlNode` on error.
         */
        XmlNode insert_copy_after(XmlNode const &proto, XmlNode const &node);

        /**
         * @brief Inserts a deep copy of an existing `XmlNode` (`proto`) before a given existing child node.
         * @param[in] proto The `XmlNode` to be copied.
         * @param[in] node The existing `XmlNode` child before which the copy will be inserted. Must be a direct child
         * of the current node.
         * @return An `XmlNode` object representing the newly added copy, or an empty `XmlNode` on error.
         */
        XmlNode insert_copy_before(XmlNode const &proto, XmlNode const &node);

        // Move the specified node to become a child of this node. Returns moved node, or empty node on errors.
        /**
         * @brief Moves an existing `XmlNode` (`moved`) to become the last child of the current node.
         * @param[in] moved The `XmlNode` to be moved. It must belong to the same `XmlDocument` as the current node.
         * @return The `XmlNode` object that was moved (now a child of `this`), or an empty `XmlNode` on error.
         * @details This operation re-parents `moved` to the current node. It's an efficient pointer manipulation,
         *          not a deep copy. Returns an empty `XmlNode` if the move is disallowed (e.g., `moved` is an ancestor
         * of `this`), or if `moved` belongs to a different document.
         * @note Moving nodes invalidates the "document buffer order" optimization flag on the owning document.
         */
        XmlNode append_move(XmlNode const &moved);

        /**
         * @brief Moves an existing `XmlNode` (`moved`) to become the first child of the current node.
         * @param[in] moved The `XmlNode` to be moved.
         * @return The `XmlNode` object that was moved, or an empty `XmlNode` on error.
         * @details Similar to `append_move`, but places `moved` at the beginning of the children list.
         */
        XmlNode prepend_move(XmlNode const &moved);

        /**
         * @brief Moves an existing `XmlNode` (`moved`) to be inserted after a specified child node.
         * @param[in] moved The `XmlNode` to be moved.
         * @param[in] node The existing `XmlNode` child after which `moved` will be inserted. This `node` must be a
         * direct child of the current node.
         * @return The `XmlNode` object that was moved, or an empty `XmlNode` on error.
         */
        XmlNode insert_move_after(XmlNode const &moved, XmlNode const &node);

        /**
         * @brief Moves an existing `XmlNode` (`moved`) to be inserted before a specified child node.
         * @param[in] moved The `XmlNode` to be moved.
         * @param[in] node The existing `XmlNode` child before which `moved` will be inserted. This `node` must be a
         * direct child of the current node.
         * @return The `XmlNode` object that was moved, or an empty `XmlNode` on error.
         */
        XmlNode insert_move_before(XmlNode const &moved, XmlNode const &node);

        // Remove specified attribute
        /**
         * @brief Removes a specified `XmlAttribute` from the current node.
         * @param[in] attr The `XmlAttribute` to remove. It must be an attribute of the current node.
         * @return `true` if the attribute was successfully removed, `false` otherwise.
         * @details Returns `false` if the node is empty, `attr` is invalid, or `attr` is not an attribute of this node.
         *          The memory associated with the attribute is deallocated.
         */
        bool remove_attribute(XmlAttribute const &attr);

        /**
         * @brief Removes an attribute with the specified name from the current node.
         * @param[in] name A null-terminated C-style string representing the name of the attribute to remove.
         * @return `true` if an attribute with the matching name was found and removed, `false` otherwise.
         * @details This is a convenience method that calls `attribute(name)` followed by `remove_attribute(XmlAttribute
         * const&)`.
         */
        bool remove_attribute(char_t const *name);

#if __cplusplus >= 201703L
        /**
         * @brief Removes an attribute with the specified name (`std::string_view`) from the current node.
         * @param[in] name A `std::string_view` representing the name of the attribute to remove.
         * @return `true` if an attribute with the matching name was found and removed, `false` otherwise.
         * @note Available only when compiled with C++17 or later.
         */
        bool remove_attribute(string_view_t name);
#endif

        // Remove all attributes
        /**
         * @brief Removes all attributes from the current node.
         * @return `true` if attributes were removed (or if the node was already empty of attributes), `false` if the
         * node is empty.
         * @details All attributes attached to this node are deallocated, and the node's `first_attribute` pointer is
         * set to `nullptr`.
         */
        bool remove_attributes();

        // Remove specified child
        /**
         * @brief Removes a specified child node from the current node.
         * @param[in] n The `XmlNode` child to remove. It must be a direct child of the current node.
         * @return `true` if the child was successfully removed, `false` otherwise.
         * @details Returns `false` if the parent node is empty, `n` is invalid, or `n` is not a direct child of this
         * node. The memory associated with the child node and its entire subtree is deallocated.
         */
        bool remove_child(XmlNode const &n);

        /**
         * @brief Removes the first child node with the specified name from the current node.
         * @param[in] name A null-terminated C-style string representing the name of the child to remove.
         * @return `true` if a child with the matching name was found and removed, `false` otherwise.
         * @details This is a convenience method that calls `child(name)` followed by `remove_child(XmlNode const&)`.
         */
        bool remove_child(char_t const *name);

#if __cplusplus >= 201703L
        /**
         * @brief Removes the first child node with the specified name (`std::string_view`) from the current node.
         * @param[in] name A `std::string_view` representing the name of the child to remove.
         * @return `true` if a child with the matching name was found and removed, `false` otherwise.
         * @note Available only when compiled with C++17 or later.
         */
        bool remove_child(string_view_t name);
#endif

        // Remove all children
        /**
         * @brief Removes all child nodes from the current node.
         * @return `true` if children were removed (or if the node was already empty of children), `false` if the node
         * is empty.
         * @details All child nodes and their subtrees are deallocated, and the node's `first_child` pointer is set to
         * `nullptr`.
         */
        bool remove_children();

        // Parses buffer as an XML document fragment and appends all nodes as children of the current node.
        // Copies/converts the buffer, so it may be deleted or changed after the function returns.
        // Note: append_buffer allocates memory that has the lifetime of the owning document; removing the appended
        // nodes does not immediately reclaim that memory.
        /**
         * @brief Parses an XML buffer as a document fragment and appends its root-level nodes as children to the
         * current node.
         * @param[in] contents A pointer to the memory buffer containing the XML fragment.
         * @param[in] size The size of the buffer in bytes.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags controlling parsing behavior. Defaults to
         * `kparse_default`.
         * @param[in] encoding The expected character encoding of the buffer. Defaults to `encoding_auto`.
         * @return An `XmlParseResult` object indicating the parsing outcome.
         * @details This function copies the provided `contents` buffer into a new internal buffer associated with the
         * owning document, parses it, and then appends the top-level nodes of the parsed fragment as children of `this`
         * node. The original `contents` buffer can be safely deleted or changed after this call.
         * @note The memory allocated for the fragment has the lifetime of the *owning document*. Removing the appended
         *       nodes later does not immediately reclaim this fragment's buffer memory; it will only be freed when the
         *       entire `XmlDocument` is destroyed or reset. This function is only valid for element/document nodes as
         * targets.
         * @throws `status_append_invalid_root` if target node cannot have children or if `kparse_merge_pcdata` is used
         * with existing PCDATA.
         */
        XmlParseResult append_buffer(void const *contents, size_t size, unsigned int options = kparse_default,
                                     xml_encoding encoding = encoding_auto);

        // Find attribute using predicate. Returns first attribute for which predicate returned true.
        /**
         * @brief Finds the first attribute for which the provided predicate returns `true`.
         * @tparam Predicate A callable type (function object, lambda) that takes an `XmlAttribute` as its argument
         *                   and returns a `bool`.
         * @param[in] pred The predicate to apply to each attribute.
         * @return An `XmlAttribute` object representing the first attribute for which `pred` is `true`,
         *         or an empty `XmlAttribute` if no such attribute is found or the node is empty.
         * @details This function iterates through all attributes of the current node, applying `pred` to each one.
         */
        template <typename Predicate>
        XmlAttribute
        find_attribute(Predicate pred) const
        {
          if(!m_root) return {};

          for(XmlAttribute attrib = first_attribute(); attrib; attrib = attrib.next_attribute())
            if(pred(attrib)) return attrib;

          return {};
        }

        // Find child node using predicate. Returns first child for which predicate returned true.
        /**
         * @brief Finds the first child node for which the provided predicate returns `true`.
         * @tparam Predicate A callable type (function object, lambda) that takes an `XmlNode` as its argument
         *                   and returns a `bool`.
         * @param[in] pred The predicate to apply to each direct child node.
         * @return An `XmlNode` object representing the first child for which `pred` is `true`,
         *         or an empty `XmlNode` if no such child is found or the node is empty.
         * @details This function iterates through all direct child nodes of the current node, applying `pred` to each
         * one.
         */
        template <typename Predicate>
        XmlNode
        find_child(Predicate pred) const
        {
          if(!m_root) return {};

          for(XmlNode node = first_child(); node; node = node.next_sibling())
            if(pred(node)) return node;

          return {};
        }

        // Find node from subtree using predicate. Returns first node from subtree (depth-first), for which predicate
        // returned true.
        /**
         * @brief Finds the first node within the current node's subtree (including itself) for which the provided
         * predicate returns `true`.
         * @tparam Predicate A callable type (function object, lambda) that takes an `XmlNode` as its argument
         *                   and returns a `bool`.
         * @param[in] pred The predicate to apply to nodes in the subtree.
         * @return An `XmlNode` object representing the first node (depth-first traversal) for which `pred` is `true`,
         *         or an empty `XmlNode` if no such node is found or the current node is empty.
         * @details This function performs a depth-first traversal of the current node's subtree (starting from its
         * first child) and applies `pred` to each visited node.
         * @note The current node itself is *not* checked by the predicate in this function; only its children and their
         * descendants.
         */
        template <typename Predicate>
        XmlNode
        find_node(Predicate pred) const
        {
          if(!m_root) return {};

          XmlNode cur = first_child();

          while(cur.m_root && cur.m_root != m_root)
          {
            if(pred(cur)) return cur;

            if(cur.first_child())
              cur = cur.first_child();
            else if(cur.next_sibling())
              cur = cur.next_sibling();
            else
            {
              while(!cur.next_sibling() && cur.m_root != m_root) cur = cur.parent();

              if(cur.m_root != m_root) cur = cur.next_sibling();
            }
          }

          return {};
        }

        // Find child node by attribute name/value
        /**
         * @brief Finds the first child element with a specific attribute name and value.
         * @param[in] name A null-terminated C-style string representing the name of the child element to search for.
         * @param[in] attr_name A null-terminated C-style string representing the name of the attribute.
         * @param[in] attr_value A null-terminated C-style string representing the expected value of the attribute.
         * @return An `XmlNode` object representing the first child element that matches both the name and has the
         * specified attribute with the given value. Returns an empty `XmlNode` if no match is found or the current node
         * is empty.
         * @note The search is case-sensitive for both node and attribute names/values.
         */
        XmlNode find_child_by_attribute(char_t const *name, char_t const *attr_name, char_t const *attr_value) const;

        /**
         * @brief Finds the first child element with a specific attribute name and value, regardless of the child's
         * name.
         * @param[in] attr_name A null-terminated C-style string representing the name of the attribute.
         * @param[in] attr_value A null-terminated C-style string representing the expected value of the attribute.
         * @return An `XmlNode` object representing the first child element that has the specified attribute with the
         * given value. Returns an empty `XmlNode` if no match is found or the current node is empty.
         * @note The search is case-sensitive for attribute names/values.
         */
        XmlNode find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const;

        // Get the absolute node path from root as a text string.
        /**
         * @brief Generates the absolute path of the current node from the document root as a string.
         * @param[in] delimiter The character used to separate path segments. Defaults to `/`.
         * @return A `string_t` (e.g., `std::string` or `std::wstring`) representing the absolute path. Returns an empty
         * string for an empty node.
         * @details The path includes the names of all ancestor nodes up to the document root, separated by the
         * `delimiter`.
         */
        string_t path(char_t delimiter = '/') const;

        // Search for a node by path consisting of node names and . or .. elements.
        /**
         * @brief Searches for the first element node by a specified path.
         * @param[in] path A null-terminated C-style string representing the path to the desired node.
         *                 Path segments are node names. `.` refers to the current context, `..` refers to the parent.
         *                 A path starting with `/` is absolute (from document root).
         * @param[in] delimiter The character used to separate path segments. Defaults to `/`.
         * @return An `XmlNode` object representing the found element, or an empty `XmlNode` if no element matches the
         * path.
         * @note This function is recursive.
         */
        XmlNode first_element_by_path(char_t const *path, char_t delimiter = '/') const;

        // Recursively traverse subtree with XmlTreeWalker
        /**
         * @brief Traverses the subtree rooted at the current node using a custom `XmlTreeWalker`.
         * @param[in,out] walker An `XmlTreeWalker` object whose `begin`, `for_each`, and `end` methods will be called
         *                       during traversal.
         * @return `true` if the traversal completes without `walker.begin` or `walker.for_each` returning `false`,
         * `false` otherwise.
         * @details This function implements a depth-first traversal algorithm. The `walker` object can control the
         *          flow of traversal (e.g., stopping early, skipping subtrees).
         * @note The `walker`'s depth is managed internally by this function.
         * @see XmlTreeWalker
         */
        bool traverse(XmlTreeWalker &walker);

        // Select single node by evaluating XPath query. Returns first node from the resulting node set.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Selects a single node by evaluating an XPath query.
         * @param[in] query A null-terminated C-style string containing the XPath query.
         * @param[in] variables An optional pointer to an `XPathVariableSet` to provide custom variables for the query.
         * Defaults to `nullptr`.
         * @return An `XPathNode` object representing the first node found by the query, or an empty `XPathNode` if no
         * match.
         * @details This function compiles and executes the XPath query against the current node's document context.
         * @note This is a convenience method that creates a temporary `XPathQuery` object. For multiple queries with
         * the same expression, compile the query once using `XPathQuery` directly.
         * @see XPathQuery
         * @see XPathNode
         */
        XPathNode select_node(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Selects a single node using a pre-compiled XPath query.
         * @param[in] query The `XPathQuery` object to evaluate.
         * @return An `XPathNode` object representing the first node found by the query, or an empty `XPathNode` if no
         * match.
         * @details This method is more efficient than `select_node(char_t const*, ...)` if the same query is to be run
         * multiple times.
         * @see XPathQuery
         * @see XPathNode
         */
        XPathNode select_node(XPathQuery const &query) const;

        // Select node set by evaluating XPath query
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Selects a set of nodes by evaluating an XPath query.
         * @param[in] query A null-terminated C-style string containing the XPath query.
         * @param[in] variables An optional pointer to an `XPathVariableSet` to provide custom variables for the query.
         * Defaults to `nullptr`.
         * @return An `XPathNodeSet` object containing all nodes matching the query, or an empty set if no matches.
         * @details This function compiles and executes the XPath query, returning all matching nodes.
         * @note This is a convenience method that creates a temporary `XPathQuery` object.
         * @see XPathQuery
         * @see XPathNodeSet
         */
        XPathNodeSet select_nodes(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Selects a set of nodes using a pre-compiled XPath query.
         * @param[in] query The `XPathQuery` object to evaluate.
         * @return An `XPathNodeSet` object containing all nodes matching the query, or an empty set if no matches.
         * @details This method is more efficient than `select_nodes(char_t const*, ...)` if the same query is to be run
         * multiple times.
         * @see XPathQuery
         * @see XPathNodeSet
         */
        XPathNodeSet select_nodes(XPathQuery const &query) const;

        // (deprecated: use select_node instead) Select single node by evaluating XPath query.
        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") XPathNode
          select_single_node(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") XPathNode
          select_single_node(XPathQuery const &query) const;

        // Print subtree using a writer object
        /**
         * @brief Prints the XML subtree rooted at the current node to a custom writer interface.
         * @param[in,out] writer An `IXmlWriter` implementation to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string used for indentation. Defaults to a tab character
         * `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags controlling output formatting. Defaults to
         * `kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto`.
         * @param[in] depth The initial indentation level (used for recursive calls). Defaults to `0`.
         * @details This method serializes the node and its children (but not attributes of the current node unless it's
         * an element being opened) to the writer. It handles indentation and character escaping based on the provided
         * flags.
         * @note For printing an entire document, use `XmlDocument::save` instead, as it handles XML declarations and
         * BOM.
         */
        void
        print(IXmlWriter &writer, char_t const *indent = LUMEX_XML_TEXT("\t"), unsigned int flags = kformat_default,
              xml_encoding encoding = encoding_auto, unsigned int depth = 0) const;

        // Print subtree to stream
        /**
         * @brief Prints the XML subtree rooted at the current node to a C-style character output stream.
         * @param[in,out] ostream The output stream to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto`.
         * @param[in] depth The initial indentation level. Defaults to `0`.
         * @details This is a convenience wrapper around `print(IXmlWriter&, ...)`, using an internal `XmlWriterStream`.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         */
        void print(std::basic_ostream<char> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, xml_encoding encoding = encoding_auto,
                   unsigned int depth = 0) const;
        /**
         * @brief Prints the XML subtree rooted at the current node to a wide character output stream.
         * @param[in,out] ostream The output stream to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `kformat_default`.
         * @param[in] depth The initial indentation level. Defaults to `0`.
         * @details This is a convenience wrapper around `print(IXmlWriter&, ...)`, with output encoding fixed to
         * `encoding_wchar`.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         */
        void print(std::basic_ostream<wchar_t> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, unsigned int depth = 0) const;

        // Child nodes iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for traversing children; discarding it negates "
                                  "the purpose of iteration.")
        /**
         * @brief Returns an iterator to the first child node of the current node.
         * @return An `XmlNodeIterator` pointing to the first child, or `end()` if no children exist.
         * @details This function enables range-based for loops over direct children.
         * @see end()
         * @see children()
         */
        XmlNodeIterator begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for delimiting children iteration; discarding "
                                  "it negates the purpose of iteration.")
        /**
         * @brief Returns an iterator to the past-the-end child node of the current node.
         * @return An `XmlNodeIterator` representing the end of the children sequence.
         * @details This function is used to delimit range-based for loops over direct children. It does not point to a
         * valid node.
         * @see begin()
         * @see children()
         */
        XmlNodeIterator end() const;

        // Attribute iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for traversing attributes; "
                                  "discarding it negates the purpose of iteration.")
        /**
         * @brief Returns an iterator to the first attribute of the current node.
         * @return An `XmlAttributeIterator` pointing to the first attribute, or `attributes_end()` if no attributes
         * exist.
         * @details This function enables range-based for loops over attributes.
         * @see attributes_end()
         * @see attributes()
         */
        XmlAttributeIterator attributes_begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for delimiting attribute iteration; "
                                  "discarding it negates the purpose of iteration.")
        /**
         * @brief Returns an iterator to the past-the-end attribute of the current node.
         * @return An `XmlAttributeIterator` representing the end of the attribute sequence.
         * @details This function is used to delimit range-based for loops over attributes. It does not point to a valid
         * attribute.
         * @see attributes_begin()
         * @see attributes()
         */
        XmlAttributeIterator attributes_end() const;

        // Range-based for support - declarations only, definitions in XmlNodeImpl.hpp
        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over children; discarding it "
                                  "negates the purpose of iteration.")
        /**
         * @brief Provides a range object for iterating over all direct child nodes.
         * @return An `XmlObjectRange` suitable for use in a range-based for loop, iterating over `XmlNodeIterator`.
         * @details This method simplifies iterating through all children: `for (XmlNode child : node.children()) { ...
         * }`.
         */
        Range::XmlObjectRange<XmlNodeIterator> children() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over attributes; discarding "
                                  "it negates the purpose of iteration.")
        /**
         * @brief Provides a range object for iterating over all attributes.
         * @return An `XmlObjectRange` suitable for use in a range-based for loop, iterating over
         * `XmlAttributeIterator`.
         * @details This method simplifies iterating through all attributes: `for (XmlAttribute attr :
         * node.attributes()) { ... }`.
         */
        Range::XmlObjectRange<XmlAttributeIterator> attributes() const;

        // Range-based for support for all children with the specified name
        // Note: name pointer must have a longer lifetime than the returned object; be careful with passing
        // temporaries!
        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over named children; "
                                  "discarding it negates the purpose of iteration.")
        /**
         * @brief Provides a range object for iterating over direct child nodes with a specific name.
         * @param[in] name A null-terminated C-style string representing the name of the children to iterate over.
         * @return An `XmlObjectRange` suitable for use in a range-based for loop, iterating over
         * `XmlNamedNodeIterator`.
         * @details This method simplifies iterating through specific named children: `for (XmlNode child :
         * node.children("tag")) { ... }`.
         * @note The `name` pointer must have a longer lifetime than the returned `XmlObjectRange` object, as it is
         * stored by the iterator.
         */
        Range::XmlObjectRange<XmlNamedNodeIterator> children(char_t const *name) const;

        // Get node offset in parsed file/string (in char_t units) for debugging purposes
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned offset should be used for debugging; discarding it negates the purpose of the getter.")
        /**
         * @brief Retrieves the byte offset of the node's name or value within the original parsed buffer.
         * @return A `ptrdiff_t` representing the offset in `char_t` units, or `-1` if the offset cannot be determined
         *         (e.g., if the document was not parsed from a single contiguous buffer, or if the node type does not
         * have a relevant offset).
         * @details This function is primarily for debugging and error reporting, allowing identification of the exact
         *          location of a node within the source XML text. It works reliably only if the document was loaded
         *          from a single, contiguous buffer (e.g., via `load_file` or `load_buffer`).
         * @note The offset refers to the start of the node's *name* for elements/PIs/declarations, and the start
         *       of the *value* for PCDATA/CDATA/comments/doctypes. For `node_document`, it returns `0`.
         */
        ptrdiff_t offset_debug() const;

        // Get hash value (unique for handles to the same object)
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned hash value should be used; discarding it negates the purpose of the getter.")
        /**
         * @brief Calculates a hash value for the `XmlNode` object.
         * @return A `size_t` value representing the hash of the node's internal pointer (`m_root`). Returns `0` if the
         * node is empty.
         * @details The hash is computed from the memory address of the underlying `XmlNodeBase`, suitable for use in
         * hash-based containers.
         */
        size_t hash_value() const;

        // Get internal pointer
        LUMEX_ATTRIBUTE_NODISCARD("The returned internal pointer should be used for low-level access; discarding it "
                                  "negates the purpose of the getter.")
        /**
         * @brief Retrieves the raw internal `XmlNodeBase` pointer wrapped by this `XmlNode` object.
         * @warning Direct manipulation of the returned pointer is generally discouraged as it bypasses the safe and
         *          convenient interface of `XmlNode`. Incorrect usage can lead to memory corruption or undefined
         * behavior.
         * @return A pointer to the internal `XmlNodeBase` structure. Can be `nullptr` if the `XmlNode` is empty.
         */
        XmlNodeBase *get() const;

      protected:
        XmlNodeBase
          *m_root{}; // NOLINT(misc-non-private-member-variables-in-classes,
                     // cppcoreguidelines-non-private-member-variables-in-classes) => need to use in XmlDocument
      };

      /**
       * @brief Checks if a given `XmlNodeBase` represents a text-like node.
       * @param[in] node A pointer to the `XmlNodeBase` to check.
       * @return `true` if the node is of type `node_pcdata` or `node_cdata`, `false` otherwise.
       * @note This is an internal utility function.
       */
      LUMEX_API
      bool is_text_node(XmlNodeBase *node);

      /**
       * @brief Checks if a move operation between `parent` and `child` nodes is allowed.
       * @param[in] parent The potential new parent node.
       * @param[in] child The node intended to be moved.
       * @return `true` if the move is semantically valid, `false` otherwise.
       * @details A move is allowed if:
       *          - `child` can legally be a child of `parent` (e.g., an attribute cannot be a child).
       *          - Both `parent` and `child` belong to the same XML document.
       *          - `parent` is not `child` itself, nor is it a descendant of `child` (to prevent creating cycles).
       * @note This is an internal utility function.
       */
      LUMEX_API
      bool allow_move(XmlNode parent, XmlNode child);

      /**
       * @brief Outputs the XML subtree rooted at `root` to a buffered writer.
       * @param[in,out] writer The `XmlBufferedWriter` to write the XML to.
       * @param[in] root The root `XmlNodeBase` of the subtree to print.
       * @param[in] indent A null-terminated C-style string for indentation (e.g., `"\t"` or `"  "`).
       * @param[in] flags A bitmask of `Constants::xml_format_option` flags controlling output format.
       * @param[in] depth The current indentation depth for the `root` node.
       * @details This recursive function serializes the XML tree, handling different node types (elements, text,
       * comments, etc.), attributes, indentation, and character escaping based on the provided flags. It uses an
       * internal `indent_flags` variable to manage newline and indentation rules during traversal.
       * @note This function is a core part of the XML serialization process. It's marked for its cognitive complexity
       * due to nested logic.
       */
      LUMEX_API
      void node_output( // NOLINT(misc-use-internal-linkage, readability-function-cognitive-complexity)
        XmlBufferedWriter &writer, XmlNodeBase *root, char_t const *indent, unsigned int flags, unsigned int depth);

      /**
       * @brief Overloads the logical AND operator for `XmlNode` and boolean types.
       * @param[in] lhs The left-hand side `XmlNode` object.
       * @param[in] rhs The right-hand side boolean value.
       * @return `true` if both the `lhs` node is valid (not empty) and `rhs` is `true`, `false` otherwise.
       * @details This operator enables short-circuit evaluation in boolean expressions involving `XmlNode` objects.
       *          It first converts `lhs` to a boolean (using its safe bool conversion operator).
       */
      LUMEX_API
      bool operator&&(XmlNode const &lhs, bool rhs);

      /**
       * @brief Overloads the logical OR operator for `XmlNode` and boolean types.
       * @param[in] lhs The left-hand side `XmlNode` object.
       * @param[in] rhs The right-hand side boolean value.
       * @return `true` if either the `lhs` node is valid (not empty) or `rhs` is `true`, `false` otherwise.
       * @details This operator enables short-circuit evaluation. It first converts `lhs` to a boolean.
       */
      LUMEX_API
      bool operator||(XmlNode const &lhs, bool rhs);

      // Iterator definitions - now that XmlNode is fully defined

      // Node iterator (bidirectional iterator over child nodes)
      /**
       * @brief A bidirectional iterator for traversing direct child nodes of an `XmlNode`.
       * @details This iterator allows standard C++ algorithm usage and range-based for loops over a node's children.
       *          It wraps an `XmlNode` object (`m_wrap`) and keeps a reference to its parent for reverse iteration
       *          and boundary checks.
       * @note The iterator holds a mutable `XmlNode` to allow its internal pointer to be updated during traversal
       * (`operator++`, `operator--`).
       * @see XmlNode::begin()
       * @see XmlNode::end()
       */
      class LUMEX_API XmlNodeIterator
      {
        friend class XmlNode;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = XmlNode;
        using pointer           = XmlNode *;
        using reference         = XmlNode &;
        using iterator_category = std::bidirectional_iterator_tag;

        /**
         * @brief Default constructor. Constructs a singular (invalid) iterator.
         */
        XmlNodeIterator() = default;

        /**
         * @brief Constructs an iterator which points to the specified node.
         * @param[in] node The `XmlNode` object to which the iterator should point.
         * @details This constructor initializes the iterator to point to `node` and stores its parent for bidirectional
         * traversal.
         */
        XmlNodeIterator(XmlNode const &node);

        // Iterator operators
        /**
         * @brief Compares two `XmlNodeIterator` objects for equality.
         * @param[in] rhs The `XmlNodeIterator` to compare with.
         * @return `true` if both iterators point to the same node within the same parent, `false` otherwise.
         */
        bool operator==(XmlNodeIterator const &rhs) const;
        /**
         * @brief Compares two `XmlNodeIterator` objects for inequality.
         * @param[in] rhs The `XmlNodeIterator` to compare with.
         * @return `true` if the iterators point to different nodes or belong to different parents, `false` otherwise.
         */
        bool operator!=(XmlNodeIterator const &rhs) const;

        /**
         * @brief Dereferences the iterator to provide access to the current `XmlNode` object.
         * @return A non-const reference to the `XmlNode` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlNode &operator*() const;
        /**
         * @brief Dereferences the iterator to provide pointer-like access to the current `XmlNode` object.
         * @return A pointer to the `XmlNode` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlNode *operator->() const;

        /**
         * @brief Pre-increments the iterator to move to the next child node.
         * @return A reference to the incremented `XmlNodeIterator` object (`*this`).
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` before increment (debug
         * builds only).
         * @note If the iterator is at the last child, it will become a singular (end) iterator after this operation.
         */
        XmlNodeIterator &operator++();
        /**
         * @brief Post-increments the iterator to move to the next child node.
         * @return A copy of the iterator's state BEFORE it was incremented.
         * @details Creates a temporary copy of the current iterator, then increments the current iterator, and returns
         * the temporary.
         * @note This operation is generally less efficient than pre-increment (`operator++()`) due to the creation of a
         * temporary object.
         */
        XmlNodeIterator operator++(int);

        /**
         * @brief Pre-decrements the iterator to move to the previous child node.
         * @return A reference to the decremented `XmlNodeIterator` object (`*this`).
         * @details If the iterator is singular (points to `nullptr`), this operation moves it to the last child of its
         * parent. Otherwise, it moves to the previous sibling.
         */
        XmlNodeIterator &operator--();
        /**
         * @brief Post-decrements the iterator to move to the previous child node.
         * @return A copy of the iterator's state BEFORE it was decremented.
         * @details Creates a temporary copy of the current iterator, then decrements the current iterator, and returns
         * the temporary.
         * @note This operation is generally less efficient than pre-decrement (`operator--()`) due to the creation of a
         * temporary object.
         */
        XmlNodeIterator operator--(int);

      private:
        mutable XmlNode m_wrap;
        XmlNode m_parent;

        /**
         * @brief Private constructor for internal use, allowing initialization with raw `XmlNodeBase` pointers.
         * @param[in] ref A raw pointer to the `XmlNodeBase` object.
         * @param[in] parent A raw pointer to the parent `XmlNodeBase` object.
         * @note This constructor is used by `XmlNode::begin()` and `XmlNode::end()` to create iterators.
         */
        XmlNodeIterator(XmlNodeBase *ref, XmlNodeBase *parent);
      };

      // Named node iterator (bidirectional iterator over child nodes with specific name)
      /**
       * @brief A bidirectional iterator for traversing direct child nodes of an `XmlNode` that have a specific name.
       * @details This iterator filters child nodes, returning only those whose names match a provided string.
       *          It allows standard C++ algorithm usage and range-based for loops over named children.
       * @note The `m_name` pointer is stored directly and must have a longer lifetime than the iterator itself.
       * @see XmlNode::children(char_t const*)
       */
      class LUMEX_API XmlNamedNodeIterator
      {
        friend class XmlNode;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = XmlNode;
        using pointer           = XmlNode *;
        using reference         = XmlNode &;
        using iterator_category = std::bidirectional_iterator_tag;

        /**
         * @brief Default constructor. Constructs a singular (invalid) named node iterator.
         */
        XmlNamedNodeIterator();

        /**
         * @brief Constructs an iterator which points to the specified node and is configured to search for a specific
         * name.
         * @param[in] node The `XmlNode` object that this iterator will initially point to.
         * @param[in] name A null-terminated C-style string representing the name that nodes must match.
         * @details This constructor initializes the iterator to point to `node`, stores its parent, and the name to
         * filter by.
         * @note The `name` pointer is stored in the iterator and must have a longer lifetime than the iterator itself.
         */
        XmlNamedNodeIterator(XmlNode const &node, char_t const *name);

        // Iterator operators
        /**
         * @brief Compares two `XmlNamedNodeIterator` objects for equality.
         * @param[in] rhs The `XmlNamedNodeIterator` to compare with.
         * @return `true` if both iterators point to the same node and belong to the same document root and parent,
         * `false` otherwise.
         */
        bool operator==(XmlNamedNodeIterator const &rhs) const;
        /**
         * @brief Compares two `XmlNamedNodeIterator` objects for inequality.
         * @param[in] rhs The `XmlNamedNodeIterator` to compare with.
         * @return `true` if the iterators point to different nodes or documents/parents, `false` otherwise.
         */
        bool operator!=(XmlNamedNodeIterator const &rhs) const;

        /**
         * @brief Dereferences the iterator to provide access to the current `XmlNode` object.
         * @return A non-const reference to the `XmlNode` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlNode &operator*() const;
        /**
         * @brief Dereferences the iterator to provide pointer-like access to the current `XmlNode` object.
         * @return A pointer to the `XmlNode` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlNode *operator->() const;

        /**
         * @brief Pre-increments the iterator to move to the next child node with the matching name.
         * @return A reference to the incremented `XmlNamedNodeIterator` object (`*this`).
         * @throws `LUMEX_ASSERT` if the internal node pointer (`m_wrap.m_root`) is `nullptr` before increment (debug
         * builds only).
         * @details The iterator advances to the next sibling that matches the stored name filter. If no more matches
         *          are found, it becomes a singular (end) iterator.
         */
        XmlNamedNodeIterator &operator++();
        /**
         * @brief Post-increments the iterator to move to the next child node with the matching name.
         * @return A copy of the iterator's state BEFORE it was incremented.
         */
        XmlNamedNodeIterator operator++(int);

        /**
         * @brief Pre-decrements the iterator to move to the previous child node with the matching name.
         * @return A reference to the decremented `XmlNamedNodeIterator` object (`*this`).
         * @details If the iterator is singular (points to `nullptr`), this operation first attempts to move it to
         *          the last child of its parent, and then searches backwards for a matching named node.
         *          Otherwise, it searches backward from the current position for the previous sibling that matches the
         * stored name filter.
         */
        XmlNamedNodeIterator &operator--();
        /**
         * @brief Post-decrements the iterator to move to the previous child node with the matching name.
         * @return A copy of the iterator's state BEFORE it was decremented.
         */
        XmlNamedNodeIterator operator--(int);

      private:
        mutable XmlNode m_wrap;
        XmlNode m_parent;
        char_t const *m_name;

        /**
         * @brief Private constructor for internal use, allowing initialization with raw `XmlNodeBase` pointers and a
         * name filter.
         * @param[in] ref A raw pointer to the `XmlNodeBase` object.
         * @param[in] parent A raw pointer to the parent `XmlNodeBase` object.
         * @param[in] name A null-terminated C-style string name filter.
         * @note This constructor is used by `XmlNode::children(char_t const*)` to create iterators.
         */
        XmlNamedNodeIterator(XmlNodeBase *ref, XmlNodeBase *parent, char_t const *name);
      };
    } // namespace Node

    namespace Attribute
    {
      // Attribute iterator (bidirectional iterator over attributes)
      /**
       * @brief A bidirectional iterator for traversing attributes of an `XmlNode`.
       * @details This iterator allows standard C++ algorithm usage and range-based for loops over a node's attributes.
       *          It wraps an `XmlAttribute` object (`m_wrap`) and keeps a reference to its parent `XmlNode` for reverse
       *          iteration and boundary checks (e.g., finding the last attribute of the parent from a singular
       * iterator).
       * @note The iterator holds a mutable `XmlAttribute` to allow its internal pointer to be updated during traversal.
       * @see XmlNode::attributes_begin()
       * @see XmlNode::attributes_end()
       */
      class LUMEX_API XmlAttributeIterator
      {
        friend class Node::XmlNode;

      private:
        mutable XmlAttribute m_wrap;
        Node::XmlNode m_parent;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = XmlAttribute;
        using pointer           = XmlAttribute *;
        using reference         = XmlAttribute &;
        using iterator_category = std::bidirectional_iterator_tag;

        /**
         * @brief Default constructor. Constructs a singular (invalid) attribute iterator.
         */
        XmlAttributeIterator() = default;

        /**
         * @brief Constructs an attribute iterator from a raw `XmlAttributeBase` pointer and its parent `XmlNodeBase`
         * pointer.
         * @param[in] ref A raw pointer to the `XmlAttributeBase` object that this iterator will initially point to.
         * @param[in] parent A raw pointer to the `XmlNodeBase` that owns the attribute list being iterated.
         * @details This constructor is primarily for internal use, allowing initialization from low-level pointers.
         * @note The raw pointers are not owned by the iterator.
         */
        XmlAttributeIterator(XmlAttributeBase *ref, Node::XmlNodeBase *parent);

        /**
         * @brief Constructs an iterator which points to the specified attribute and knows its parent node.
         * @param[in] attr The `XmlAttribute` object that this iterator will initially point to.
         * @param[in] parent The `XmlNode` that owns the attribute list being iterated. This is used for operations like
         * `operator--` that might need to find the last attribute of the parent.
         * @details This constructor initializes the iterator to wrap a specific attribute and keeps a reference to its
         * parent node.
         */
        XmlAttributeIterator(XmlAttribute const &attr, Node::XmlNode const &parent);

        // Iterator operators
        /**
         * @brief Compares two `XmlAttributeIterator` objects for equality.
         * @param[in] rhs The `XmlAttributeIterator` to compare with.
         * @return `true` if both iterators point to the same attribute within the same document root and parent,
         * `false` otherwise.
         */
        bool operator==(XmlAttributeIterator const &rhs) const;
        /**
         * @brief Compares two `XmlAttributeIterator` objects for inequality.
         * @param[in] rhs The `XmlAttributeIterator` to compare with.
         * @return `true` if the iterators point to different attributes or different documents/parents, `false`
         * otherwise.
         */
        bool operator!=(XmlAttributeIterator const &rhs) const;

        /**
         * @brief Dereferences the iterator to provide access to the current `XmlAttribute` object.
         * @return A non-const reference to the `XmlAttribute` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal attribute pointer (`m_wrap.m_attr`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlAttribute &operator*() const;
        /**
         * @brief Dereferences the iterator to provide pointer-like access to the current `XmlAttribute` object.
         * @return A pointer to the `XmlAttribute` object the iterator currently points to.
         * @throws `LUMEX_ASSERT` if the internal attribute pointer (`m_wrap.m_attr`) is `nullptr` (debug builds only).
         * @warning Dereferencing a singular (end) or invalid iterator leads to undefined behavior.
         */
        XmlAttribute *operator->() const;

        /**
         * @brief Pre-increments the iterator to move to the next attribute in the list.
         * @return A reference to the incremented `XmlAttributeIterator` object (`*this`).
         * @throws `LUMEX_ASSERT` if the internal attribute pointer (`m_wrap.m_attr`) is `nullptr` before increment
         * (debug builds only).
         * @note If the iterator is at the last attribute, it will become a singular (end) iterator after this
         * operation.
         */
        XmlAttributeIterator &operator++();
        /**
         * @brief Post-increments the iterator to move to the next attribute in the list.
         * @return A copy of the iterator's state BEFORE it was incremented.
         * @details Creates a temporary copy of the current iterator, then increments the current iterator, and returns
         * the temporary.
         * @note This operation is generally less efficient than pre-increment (`operator++()`) due to the creation of a
         * temporary object.
         */
        XmlAttributeIterator operator++(int);

        /**
         * @brief Pre-decrements the iterator to move to the previous attribute in the list.
         * @return A reference to the decremented `XmlAttributeIterator` object (`*this`).
         * @details If the iterator is singular (points to `nullptr`), this operation moves it to the last attribute of
         * its parent. Otherwise, it moves to the previous attribute.
         */
        XmlAttributeIterator &operator--();
        /**
         * @brief Post-decrements the iterator to move to the previous attribute in the list.
         * @return A copy of the iterator's state BEFORE it was decremented.
         * @details Creates a temporary copy of the current iterator, then decrements the current iterator, and returns
         * the temporary.
         * @note This operation is generally less efficient than pre-decrement (`operator--()`) due to the creation of a
         * temporary object.
         */
        XmlAttributeIterator operator--(int);
      };
    } // namespace Attribute

    // Inline implementations of template methods - now that all types are defined
    namespace Node
    {
      inline Range::XmlObjectRange<XmlNodeIterator>
      XmlNode::children() const
      {
        return Range::XmlObjectRange<XmlNodeIterator>(begin(), end());
      }

      inline Range::XmlObjectRange<XmlNamedNodeIterator>
      XmlNode::children(char_t const *name_) const
      {
        return Range::XmlObjectRange<XmlNamedNodeIterator>(XmlNamedNodeIterator(child(name_).m_root, m_root, name_),
                                                           XmlNamedNodeIterator(nullptr, m_root, name_));
      }

      inline Range::XmlObjectRange<Attribute::XmlAttributeIterator>
      XmlNode::attributes() const
      {
        return Range::XmlObjectRange<Attribute::XmlAttributeIterator>(attributes_begin(), attributes_end());
      }
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_HPP

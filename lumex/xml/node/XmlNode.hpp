#ifndef LUMEX_XML_NODE_HPP
#define LUMEX_XML_NODE_HPP

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/writer/IXmlWriter.hpp"

#include "XmlNodeBase.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Constants;

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
      class XmlNode
      {
        friend class Attribute::XmlAttribute;
        friend class Attribute::XmlAttributeIterator;
        friend class XmlNodeIterator;
        friend class XmlNamedNodeIterator;

      public:
        using unspecified_bool_type = void (*)(XmlNode ***);

        // Default constructor. Constructs an empty node.
        XmlNode();

        // Constructs node from internal pointer
        explicit XmlNode(XmlNodeBase *ptr);

        // Safe bool conversion operator
        operator unspecified_bool_type() const;

        // Borland C++ workaround
        bool operator!() const;

        // Comparison operators (compares wrapped node pointers)
        bool operator==(XmlNode const &other) const;
        bool operator!=(XmlNode const &other) const;
        bool operator<(XmlNode const &other) const;
        bool operator>(XmlNode const &other) const;
        bool operator<=(XmlNode const &other) const;
        bool operator>=(XmlNode const &other) const;

        // Check if node is empty (null)
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned boolean indicates whether the node is empty; discarding it negates the purpose of the getter.")
        bool empty() const;

        // Get node type
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned node type should be used; discarding it negates the purpose of the getter.")
        xml_node_type type() const;

        // Get node name, or "" if node is empty or it has no name
        // Note: For <node>text</node> node.value() does not return "text"! Use child_value() or text() methods to
        // access text inside nodes.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string name should be used; discarding it negates the purpose of the getter.")
        char_t const *name() const;

        // Get node value, or "" if node is empty or it has no value
        // Note: For <node>text</node> node.value() does not return "text"! Use child_value() or text() methods to
        // access text inside nodes.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string value should be used; discarding it negates the purpose of the getter.")
        char_t const *value() const;

        // Get attribute list
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        XmlAttribute first_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        XmlAttribute last_attribute() const;

        // Get children list
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode first_child() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode last_child() const;

        // Get next/previous sibling in the children list of the parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode next_sibling() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode previous_sibling() const;

        // Get parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode parent() const;

        // Get root of DOM tree this node belongs to
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        XmlNode root() const;

        // Get text object for the current node
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned text object should be used; discarding it negates the purpose of the getter.")
        XmlText text() const;

        // Get child, attribute or next/previous sibling with the specified name
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        XmlNode child(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        XmlAttribute attribute(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        XmlNode next_sibling(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        XmlNode previous_sibling(char_t const *name) const;

#if __cplusplus >= 201703L
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        XmlNode child(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        XmlAttribute attribute(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        XmlNode next_sibling(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        XmlNode previous_sibling(string_view_t name) const;
#endif

        // Get attribute, starting the search from a hint (and updating hint so that searching for a sequence of
        // attributes is fast)
        XmlAttribute attribute(char_t const *name, XmlAttribute &hint) const;
#if __cplusplus >= 201703L
        XmlAttribute attribute(string_view_t name, XmlAttribute &hint) const;
#endif

        // Get child value of current node; that is, value of the first child node of type PCDATA/CDATA
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string child value should be used; discarding it negates the purpose of the getter.")
        char_t const *child_value() const;

        // Get child value of child with specified name. Equivalent to child(name).child_value().
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string child value should be used; discarding it negates the purpose of the getter.")
        char_t const *child_value(char_t const *name) const;

        // Set node name/value (returns false if node is empty, there is not enough memory, or node can not have
        // name/value)
        bool set_name(char_t const *rhs);

        bool set_name(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        bool set_name(string_view_t rhs);
#endif

        bool set_value(const char_t *rhs);

        bool set_value(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        bool set_value(string_view_t rhs);
#endif

        // Add attribute with specified name. Returns added attribute, or empty attribute on errors.
        XmlAttribute append_attribute(char_t const *name);

        XmlAttribute prepend_attribute(char_t const *name);

        XmlAttribute insert_attribute_after(char_t const *name, XmlAttribute const &attr);

        XmlAttribute insert_attribute_before(char_t const *name, XmlAttribute const &attr);

#if __cplusplus >= 201703L
        XmlAttribute append_attribute(string_view_t name);

        XmlAttribute prepend_attribute(string_view_t name);

        XmlAttribute insert_attribute_after(string_view_t name, XmlAttribute const &attr);

        XmlAttribute insert_attribute_before(string_view_t name, XmlAttribute const &attr);
#endif

        // Add a copy of the specified attribute. Returns added attribute, or empty attribute on errors.
        XmlAttribute append_copy(XmlAttribute const &proto);

        XmlAttribute prepend_copy(XmlAttribute const &proto);

        XmlAttribute insert_copy_after(XmlAttribute const &proto, XmlAttribute const &attr);

        XmlAttribute insert_copy_before(XmlAttribute const &proto, XmlAttribute const &attr);

        // Add child node with specified type. Returns added node, or empty node on errors.
        XmlNode append_child(xml_node_type type = node_element);

        XmlNode prepend_child(xml_node_type type = node_element);

        XmlNode insert_child_after(xml_node_type type, XmlNode const &node);

        XmlNode insert_child_before(xml_node_type type, XmlNode const &node);

        // Add child element with specified name. Returns added node, or empty node on errors.
        XmlNode append_child(char_t const *name);

        XmlNode prepend_child(char_t const *name);

        XmlNode insert_child_after(char_t const *name, XmlNode const &node);

        XmlNode insert_child_before(char_t const *name, XmlNode const &node);

#if __cplusplus >= 201703L
        XmlNode append_child(string_view_t name);

        XmlNode prepend_child(string_view_t name);

        XmlNode insert_child_after(string_view_t, XmlNode const &node);

        XmlNode insert_child_before(string_view_t name, XmlNode const &node);
#endif

        // Add a copy of the specified node as a child. Returns added node, or empty node on errors.
        XmlNode append_copy(XmlNode const &proto);

        XmlNode prepend_copy(XmlNode const &proto);

        XmlNode insert_copy_after(XmlNode const &proto, XmlNode const &node);

        XmlNode insert_copy_before(XmlNode const &proto, XmlNode const &node);

        // Move the specified node to become a child of this node. Returns moved node, or empty node on errors.
        XmlNode append_move(XmlNode const &moved);

        XmlNode prepend_move(XmlNode const &moved);

        XmlNode insert_move_after(XmlNode const &moved, XmlNode const &node);

        XmlNode insert_move_before(XmlNode const &moved, XmlNode const &node);

        // Remove specified attribute
        bool remove_attribute(XmlAttribute const &attr);

        bool remove_attribute(char_t const *name);

#if __cplusplus >= 201703L
        bool remove_attribute(string_view_t name);
#endif

        // Remove all attributes
        bool remove_attributes();

        // Remove specified child
        bool remove_child(XmlNode const &n);

        bool remove_child(char_t const *name);

#if __cplusplus >= 201703L
        bool remove_child(string_view_t name);
#endif

        // Remove all children
        bool remove_children();

        // Parses buffer as an XML document fragment and appends all nodes as children of the current node.
        // Copies/converts the buffer, so it may be deleted or changed after the function returns.
        // Note: append_buffer allocates memory that has the lifetime of the owning document; removing the appended
        // nodes does not immediately reclaim that memory.
        XmlParseResult append_buffer(void const *contents, size_t size, unsigned int options = kparse_default,
                                     xml_encoding encoding = encoding_auto);

        // Find attribute using predicate. Returns first attribute for which predicate returned true.
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
        XmlNode find_child_by_attribute(char_t const *name, char_t const *attr_name, char_t const *attr_value) const;

        XmlNode find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const;

        // Get the absolute node path from root as a text string.
        string_t path(char_t delimiter = '/') const;

        // Search for a node by path consisting of node names and . or .. elements.
        XmlNode first_element_by_path(char_t const *path, char_t delimiter = '/') const;

        // Recursively traverse subtree with XmlTreeWalker
        bool traverse(XmlTreeWalker &walker);

        // Select single node by evaluating XPath query. Returns first node from the resulting node set.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        XPathNode select_node(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        XPathNode select_node(XPathQuery const &query) const;

        // Select node set by evaluating XPath query
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        XPathNodeSet select_nodes(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        XPathNodeSet select_nodes(XPathQuery const &query) const;

        // (deprecated: use select_node instead) Select single node by evaluating XPath query.
        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") XPathNode
          select_single_node(char_t const *query, XPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") XPathNode
          select_single_node(XPathQuery const &query) const;

        // Print subtree using a writer object
        void
        print(IXmlWriter &writer, char_t const *indent = LUMEX_XML_TEXT("\t"), unsigned int flags = kformat_default,
              xml_encoding encoding = encoding_auto, unsigned int depth = 0) const;

        // Print subtree to stream
        void print(std::basic_ostream<char> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, xml_encoding encoding = encoding_auto,
                   unsigned int depth = 0) const;
        void print(std::basic_ostream<wchar_t> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, unsigned int depth = 0) const;

        // Child nodes iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for traversing children; discarding it negates "
                                  "the purpose of iteration.")
        XmlNodeIterator begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for delimiting children iteration; discarding "
                                  "it negates the purpose of iteration.")
        XmlNodeIterator end() const;

        // Attribute iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for traversing attributes; "
                                  "discarding it negates the purpose of iteration.")
        XmlAttributeIterator attributes_begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for delimiting attribute iteration; "
                                  "discarding it negates the purpose of iteration.")
        XmlAttributeIterator attributes_end() const;

        // TODO: Resolve circular dependency for these methods
        // // Range-based for support
        // LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over children; discarding
        // it "
        //                           "negates the purpose of iteration.")
        // XmlObjectRange<XmlNodeIterator> children() const;

        // LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over attributes; discarding
        // "
        //                           "it negates the purpose of iteration.")
        // XmlObjectRange<XmlAttributeIterator> attributes() const;

        // // Range-based for support for all children with the specified name
        // // Note: name pointer must have a longer lifetime than the returned object; be careful with passing
        // // temporaries!
        // LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over named children; "
        //                           "discarding it negates the purpose of iteration.")
        // XmlObjectRange<XmlNamedNodeIterator> children(char_t const *name) const;

        // Get node offset in parsed file/string (in char_t units) for debugging purposes
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned offset should be used for debugging; discarding it negates the purpose of the getter.")
        ptrdiff_t offset_debug() const;

        // Get hash value (unique for handles to the same object)
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned hash value should be used; discarding it negates the purpose of the getter.")
        size_t hash_value() const;

        // Get internal pointer
        LUMEX_ATTRIBUTE_NODISCARD("The returned internal pointer should be used for low-level access; discarding it "
                                  "negates the purpose of the getter.")
        XmlNodeBase *get() const;

      protected:
        XmlNodeBase
          *m_root{}; // NOLINT(misc-non-private-member-variables-in-classes,
                     // cppcoreguidelines-non-private-member-variables-in-classes) => need to use in XmlDocument
      };

      inline bool
      is_text_node(XmlNodeBase *node)
      {
        auto type = LUMEX_XML_NODETYPE(node);
        return type == node_pcdata || type == node_cdata;
      }

      bool allow_move(XmlNode parent, XmlNode child);
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_HPP

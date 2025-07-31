#ifndef LUMEX_XML_NODE_HPP
#define LUMEX_XML_NODE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/attribute/LumexXmlAttribute.hpp"
#include "lumex/xml/constants/LumexXmlConstants.hpp"
#include "lumex/xml/memory/LumexXmlMemoryPage.hpp"
#include "lumex/xml/range/LumexXmlObjectRange.hpp"
#include "lumex/xml/text/LumexXmlParseResult.hpp"
#include "lumex/xml/text/LumexXmlText.hpp"
#include "lumex/xml/tree/LumexXmlTreeWalker.hpp"
#include "lumex/xml/types/LumexXmlTypes.hpp"
#include "lumex/xml/utility/LumexXmlMacros.hpp"
#include "lumex/xml/writer/ILumexXmlWriter.hpp"
#include "lumex/xml/xpath/node/LumexXmlXPathNode.hpp"
#include "lumex/xml/xpath/node/LumexXmlXPathNodeSet.hpp"
#include "lumex/xml/xpath/query/LumexXmlXPathQuery.hpp"

using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Tree;
using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Range;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Query;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      struct LUMEX_API xml_node_t {
        xml_node_t(xml_mem_page_t *page, xml_node_type type) : header(LUMEX_XML_GETHEADER_IMPL(this, page, type)) {}

        uintptr_t header; // NOLINT(misc-non-private-member-variables-in-classes)

        char_t *name{};  // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)

        xml_node_t *parent{}; // NOLINT(misc-non-private-member-variables-in-classes)

        xml_node_t *first_child{}; // NOLINT(misc-non-private-member-variables-in-classes)

        xml_node_t *prev_sibling_c{}; // NOLINT(misc-non-private-member-variables-in-classes)
        xml_node_t *next_sibling{};   // NOLINT(misc-non-private-member-variables-in-classes)

        xml_attr_t *first_attribute{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      class LUMEX_API LumexXmlNode
      {
        friend class Lumex::Xml::Attribute::LumexXmlAttributeIterator;
        friend class LumexXmlNodeIterator;
        friend class LumexXmlNamedNodeIterator;

      public:
        using unspecified_bool_type = void (*)(LumexXmlNode ***);

        using iterator              = LumexXmlNodeIterator;
        using attribute_iterator    = LumexXmlAttributeIterator;

        // Default constructor. Constructs an empty node.
        LumexXmlNode();

        // Constructs node from internal pointer
        explicit LumexXmlNode(xml_node_t *ptr);

        // Safe bool conversion operator
        operator unspecified_bool_type() const;

        // Borland C++ workaround
        bool operator!() const;

        // Comparison operators (compares wrapped node pointers)
        bool operator==(LumexXmlNode const &other) const;
        bool operator!=(LumexXmlNode const &other) const;
        bool operator<(LumexXmlNode const &other) const;
        bool operator>(LumexXmlNode const &other) const;
        bool operator<=(LumexXmlNode const &other) const;
        bool operator>=(LumexXmlNode const &other) const;

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
        LumexXmlAttribute first_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        LumexXmlAttribute last_attribute() const;

        // Get children list
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode first_child() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode last_child() const;

        // Get next/previous sibling in the children list of the parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode next_sibling() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode previous_sibling() const;

        // Get parent node
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode parent() const;

        // Get root of DOM tree this node belongs to
        LUMEX_ATTRIBUTE_NODISCARD("The returned node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode root() const;

        // Get text object for the current node
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned text object should be used; discarding it negates the purpose of the getter.")
        LumexXmlText text() const;

        // Get child, attribute or next/previous sibling with the specified name
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode child(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        LumexXmlAttribute attribute(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode next_sibling(char_t const *name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode previous_sibling(char_t const *name) const;

#if __cplusplus >= 201703L
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned child node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode child(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter.")
        LumexXmlAttribute attribute(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned next sibling node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode next_sibling(string_view_t name) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned previous sibling node should be used; discarding it negates the purpose of the getter.")
        LumexXmlNode previous_sibling(string_view_t name) const;
#endif

        // Get attribute, starting the search from a hint (and updating hint so that searching for a sequence of
        // attributes is fast)
        LumexXmlAttribute attribute(char_t const *name, LumexXmlAttribute &hint) const;
#if __cplusplus >= 201703L
        LumexXmlAttribute attribute(string_view_t name, LumexXmlAttribute &hint) const;
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
        LumexXmlAttribute append_attribute(char_t const *name);

        LumexXmlAttribute prepend_attribute(char_t const *name);

        LumexXmlAttribute insert_attribute_after(char_t const *name, LumexXmlAttribute const &attr);

        LumexXmlAttribute insert_attribute_before(char_t const *name, LumexXmlAttribute const &attr);

#if __cplusplus >= 201703L
        LumexXmlAttribute append_attribute(string_view_t name);

        LumexXmlAttribute prepend_attribute(string_view_t name);

        LumexXmlAttribute insert_attribute_after(string_view_t name, LumexXmlAttribute const &attr);

        LumexXmlAttribute insert_attribute_before(string_view_t name, LumexXmlAttribute const &attr);
#endif

        // Add a copy of the specified attribute. Returns added attribute, or empty attribute on errors.
        LumexXmlAttribute append_copy(LumexXmlAttribute const &proto);

        LumexXmlAttribute prepend_copy(LumexXmlAttribute const &proto);

        LumexXmlAttribute insert_copy_after(LumexXmlAttribute const &proto, LumexXmlAttribute const &attr);

        LumexXmlAttribute insert_copy_before(LumexXmlAttribute const &proto, LumexXmlAttribute const &attr);

        // Add child node with specified type. Returns added node, or empty node on errors.
        LumexXmlNode append_child(xml_node_type type = node_element);

        LumexXmlNode prepend_child(xml_node_type type = node_element);

        LumexXmlNode insert_child_after(xml_node_type type, LumexXmlNode const &node);

        LumexXmlNode insert_child_before(xml_node_type type, LumexXmlNode const &node);

        // Add child element with specified name. Returns added node, or empty node on errors.
        LumexXmlNode append_child(char_t const *name);

        LumexXmlNode prepend_child(char_t const *name);

        LumexXmlNode insert_child_after(char_t const *name, LumexXmlNode const &node);

        LumexXmlNode insert_child_before(char_t const *name, LumexXmlNode const &node);

#if __cplusplus >= 201703L
        LumexXmlNode append_child(string_view_t name);

        LumexXmlNode prepend_child(string_view_t name);

        LumexXmlNode insert_child_after(string_view_t, LumexXmlNode const &node);

        LumexXmlNode insert_child_before(string_view_t name, LumexXmlNode const &node);
#endif

        // Add a copy of the specified node as a child. Returns added node, or empty node on errors.
        LumexXmlNode append_copy(LumexXmlNode const &proto);

        LumexXmlNode prepend_copy(LumexXmlNode const &proto);

        LumexXmlNode insert_copy_after(LumexXmlNode const &proto, LumexXmlNode const &node);

        LumexXmlNode insert_copy_before(LumexXmlNode const &proto, LumexXmlNode const &node);

        // Move the specified node to become a child of this node. Returns moved node, or empty node on errors.
        LumexXmlNode append_move(LumexXmlNode const &moved);

        LumexXmlNode prepend_move(LumexXmlNode const &moved);

        LumexXmlNode insert_move_after(LumexXmlNode const &moved, LumexXmlNode const &node);

        LumexXmlNode insert_move_before(LumexXmlNode const &moved, LumexXmlNode const &node);

        // Remove specified attribute
        bool remove_attribute(LumexXmlAttribute const &attr);

        bool remove_attribute(char_t const *name);

#if __cplusplus >= 201703L
        bool remove_attribute(string_view_t name);
#endif

        // Remove all attributes
        bool remove_attributes();

        // Remove specified child
        bool remove_child(LumexXmlNode const &n);

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
        LumexXmlParseResult append_buffer(void const *contents, size_t size, unsigned int options = kparse_default,
                                          xml_encoding encoding = encoding_auto);

        // Find attribute using predicate. Returns first attribute for which predicate returned true.
        template <typename Predicate>
        LumexXmlAttribute
        find_attribute(Predicate pred) const
        {
          if(!m_root) return {};

          for(LumexXmlAttribute attrib = first_attribute(); attrib; attrib = attrib.next_attribute())
            if(pred(attrib)) return attrib;

          return {};
        }

        // Find child node using predicate. Returns first child for which predicate returned true.
        template <typename Predicate>
        LumexXmlNode
        find_child(Predicate pred) const
        {
          if(!m_root) return {};

          for(LumexXmlNode node = first_child(); node; node = node.next_sibling())
            if(pred(node)) return node;

          return {};
        }

        // Find node from subtree using predicate. Returns first node from subtree (depth-first), for which predicate
        // returned true.
        template <typename Predicate>
        LumexXmlNode
        find_node(Predicate pred) const
        {
          if(!m_root) return {};

          LumexXmlNode cur = first_child();

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
        LumexXmlNode
        find_child_by_attribute(char_t const *name, char_t const *attr_name, char_t const *attr_value) const;

        LumexXmlNode find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const;

        // Get the absolute node path from root as a text string.
        string_t path(char_t delimiter = '/') const;

        // Search for a node by path consisting of node names and . or .. elements.
        LumexXmlNode first_element_by_path(char_t const *path, char_t delimiter = '/') const;

        // Recursively traverse subtree with LumexXmlTreeWalker
        bool traverse(LumexXmlTreeWalker &walker);

        // Select single node by evaluating XPath query. Returns first node from the resulting node set.
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        LumexXmlXPathNode select_node(char_t const *query, LumexXmlXPathVariableSet *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.")
        LumexXmlXPathNode select_node(LumexXmlXPathQuery const &query) const;

        // Select node set by evaluating XPath query
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        LumexXmlXPathNodeSet select_nodes(char_t const *query, xpath_variable_set *variables = nullptr) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node set should be used; discarding it negates the purpose of the getter.")
        LumexXmlXPathNodeSet select_nodes(LumexXmlXPathQuery const &query) const;

        // (deprecated: use select_node instead) Select single node by evaluating XPath query.
        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") LumexXmlXPathNode
          select_single_node(char_t const *query, xpath_variable_set *variables = nullptr) const;

        LUMEX_ATTRIBUTE_DEPRECATED LUMEX_ATTRIBUTE_NODISCARD(
          "The returned XPath node should be used; discarding it negates the purpose of the getter.") LumexXmlXPathNode
          select_single_node(LumexXmlXPathQuery const &query) const;

        // Print subtree using a writer object
        void print(ILumexXmlWriter &writer, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, xml_encoding encoding = encoding_auto,
                   unsigned int depth = 0) const;

        // Print subtree to stream
        void print(std::basic_ostream<char> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, xml_encoding encoding = encoding_auto,
                   unsigned int depth = 0) const;
        void print(std::basic_ostream<wchar_t> &ostream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                   unsigned int flags = kformat_default, unsigned int depth = 0) const;

        // Child nodes iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for traversing children; discarding it negates "
                                  "the purpose of iteration.")
        iterator begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for delimiting children iteration; discarding "
                                  "it negates the purpose of iteration.")
        iterator end() const;

        // Attribute iterators
        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for traversing attributes; "
                                  "discarding it negates the purpose of iteration.")
        attribute_iterator attributes_begin() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute iterator should be used for delimiting attribute iteration; "
                                  "discarding it negates the purpose of iteration.")
        attribute_iterator attributes_end() const;

        // Range-based for support
        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over children; discarding it "
                                  "negates the purpose of iteration.")
        LumexXmlObjectRange<LumexXmlNodeIterator> children() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over attributes; discarding "
                                  "it negates the purpose of iteration.")
        LumexXmlObjectRange<LumexXmlAttributeIterator> attributes() const;

        // Range-based for support for all children with the specified name
        // Note: name pointer must have a longer lifetime than the returned object; be careful with passing
        // temporaries!
        LUMEX_ATTRIBUTE_NODISCARD("The returned range object should be used for iterating over named children; "
                                  "discarding it negates the purpose of iteration.")
        LumexXmlObjectRange<LumexXmlNamedNodeIterator> children(char_t const *name) const;

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
        xml_node_t *get() const;

      private:
        xml_node_t *m_root{};
      };

      inline bool
      is_text_node(xml_node_t *node)
      {
        auto type = LUMEX_XML_NODETYPE(node);
        return type == node_pcdata || type == node_cdata;
      }
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_HPP

#ifndef LUMEX_XML_NODE_HPP
#define LUMEX_XML_NODE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/writer/IXmlWriter.hpp"
#include "lumex/xml/writer/XmlBufferedWriter.hpp"

#include "XmlNodeBase.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Utility;

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
      class LUMEX_API XmlNode
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

      LUMEX_API
      inline bool
      is_text_node(XmlNodeBase *node)
      {
        auto type = LUMEX_XML_NODETYPE(node);
        return type == node_pcdata || type == node_cdata;
      }

      LUMEX_API
      bool allow_move(XmlNode parent, XmlNode child);

      namespace
      {
        inline void
        text_output_escaped( // NOLINT(misc-use-internal-linkage, readability-function-cognitive-complexity)
          XmlBufferedWriter &writer, char_t const *str, chartypex_t type, unsigned int flags)
        {
          while(*str != 0)
          {
            char_t const *prev = str;

            // While *s is a usual symbol
            LUMEX_XML_SCANWHILE_UNROLL( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,
                                        // readability-identifier-length)
              !LUMEX_XML_IS_CHARTYPEX(*str, type));

            writer.write_buffer(prev, static_cast<size_t>(str - prev));

            switch(*str)
            {
            case 0: break;
            case '&':
              writer.write('&', 'a', 'm', 'p', ';');
              ++str;
              break;
            case '<':
              writer.write('&', 'l', 't', ';');
              ++str;
              break;
            case '>':
              writer.write('&', 'g', 't', ';');
              ++str;
              break;
            case '"':
              if((flags & Constants::kformat_attribute_single_quote) != 0)
                writer.write('"');
              else
                writer.write('&', 'q', 'u', 'o', 't', ';');
              ++str;
              break;
            case '\'':
              if((flags & Constants::kformat_attribute_single_quote) != 0)
                writer.write('&', 'a', 'p', 'o', 's', ';');
              else
                writer.write('\'');
              ++str;
              break;
            default: // s is not a usual symbol
            {
              auto chr = static_cast<unsigned int>(*str++); // NOLINT(bugprone-signed-char-misuse)
              LUMEX_ASSERT(chr < 32);

              if((flags & Constants::kformat_skip_control_chars) == 0)
                writer.write('&', '#', static_cast<char_t>((chr / 10) + '0'), static_cast<char_t>((chr % 10) + '0'),
                             ';');
            }
            }
          }
        }

        inline void
        text_output(XmlBufferedWriter &writer, char_t const *str, // NOLINT(misc-use-internal-linkage)
                    chartypex_t type, unsigned int flags)
        {
          if((flags & Constants::kformat_no_escapes) != 0)
            writer.write_string(str);
          else
            text_output_escaped(writer, str, type, flags);
        }

        inline void
        text_output_cdata(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
        {
          do { // NOLINT(cppcoreguidelines-avoid-do-while)
            writer.write('<', '!', '[', 'C', 'D');
            writer.write('A', 'T', 'A', '[');

            char_t const *prev = str;

            // look for ]]> sequence - we can't output it as is since it terminates CDATA
            while((*str != 0) && (str[0] != ']' || str[1] != ']' || str[2] != '>')) ++str;

            // skip ]] if we stopped at ]]>, > will go to the next CDATA section
            if(*str != 0) str += 2;

            writer.write_buffer(prev, static_cast<size_t>(str - prev));

            writer.write(']', ']', '>');
          } while(*str != 0);
        }

        inline void
        text_output_indent(XmlBufferedWriter &writer,                  // NOLINT(misc-use-internal-linkage)
                           char_t const *indent, size_t indent_length, // NOLINT(bugprone-easily-swappable-parameters)
                           unsigned int depth)
        {
          switch(indent_length)
          {
          case 1: {
            for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0]);
            break;
          }

          case 2: {
            for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1]);
            break;
          }

          case 3: {
            for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1], indent[2]);
            break;
          }

          case 4: {
            for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1], indent[2], indent[3]);
            break;
          }

          default: {
            for(unsigned int i = 0; i < depth; ++i) writer.write_buffer(indent, indent_length);
          }
          }
        }

        inline void
        node_output_comment(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
        {
          writer.write('<', '!', '-', '-');

          while(*str != 0)
          {
            char_t const *prev = str;

            // look for -\0 or -- sequence - we can't output it since -- is illegal in comment body
            while((*str != 0) && (str[0] != '-' || (str[1] != '-' && str[1] != 0))) ++str;

            writer.write_buffer(prev, static_cast<size_t>(str - prev));

            if(*str != 0)
            {
              LUMEX_ASSERT(*str == '-');

              writer.write('-', ' ');
              ++str;
            }
          }

          writer.write('-', '-', '>');
        }

        inline void
        node_output_pi_value(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
        {
          while(*str != 0)
          {
            char_t const *prev = str;

            // look for ?> sequence - we can't output it since ?> terminates PI
            while((*str != 0) && (str[0] != '?' || str[1] != '>')) ++str;

            writer.write_buffer(prev, static_cast<size_t>(str - prev));

            if(*str != 0)
            {
              LUMEX_ASSERT(str[0] == '?' && str[1] == '>');

              writer.write('?', ' ', '>');
              str += 2;
            }
          }
        }

        inline void
        node_output_attributes(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                               XmlNodeBase *node, char_t const *indent,
                               size_t indent_length, // NOLINT(bugprone-easily-swappable-parameters)
                               unsigned int flags, unsigned int depth)
        {
          char_t const *default_name    = LUMEX_XML_TEXT(":anonymous");
          char_t const enquotation_char = ((flags & Constants::kformat_attribute_single_quote) != 0) ? '\'' : '"';

          for(XmlAttributeBase *attr = node->first_attribute; attr != nullptr; attr = attr->next_attribute)
          {
            if((flags & (Constants::kformat_indent_attributes | Constants::kformat_raw))
               == Constants::kformat_indent_attributes)
            {
              writer.write('\n');

              text_output_indent(writer, indent, indent_length, depth + 1);
            }
            else { writer.write(' '); }

            writer.write_string((attr->name != nullptr) ? attr->name + 0 : default_name);
            writer.write('=', enquotation_char);

            if(attr->value != nullptr) text_output(writer, attr->value, ctx_special_attr, flags);

            writer.write(enquotation_char);
          }
        }

        inline bool
        node_output_start(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                          XmlNodeBase *node, char_t const *indent, size_t indent_length, unsigned int flags,
                          unsigned int depth)
        {
          char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
          char_t const *name         = (node->name != nullptr) ? node->name + 0 : default_name;

          writer.write('<');
          writer.write_string(name);

          if(node->first_attribute != nullptr)
            node_output_attributes(writer, node, indent, indent_length, flags, depth);

          // element nodes can have value if parse_embed_pcdata was used
          if(node->value == nullptr)
          {
            if(node->first_child == nullptr)
            {
              if((flags & Constants::kformat_no_empty_element_tags) != 0)
              {
                writer.write('>', '<', '/');
                writer.write_string(name);
                writer.write('>');

                return false;
              }

              if((flags & Constants::kformat_raw) == 0) writer.write(' ');

              writer.write('/', '>');

              return false;
            }
            writer.write('>');
            return true;
          }

          writer.write('>');

          text_output(writer, node->value, ctx_special_pcdata, flags);

          if(node->first_child == nullptr)
          {
            writer.write('<', '/');
            writer.write_string(name);
            writer.write('>');

            return false;
          }
          return true;
        }

        inline void
        node_output_end(XmlBufferedWriter &writer, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
        {
          char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
          char_t const *name         = (node->name != nullptr) ? node->name + 0 : default_name;

          writer.write('<', '/');
          writer.write_string(name);
          writer.write('>');
        }

        inline void
        node_output_simple(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                           XmlNodeBase *node, unsigned int flags)
        {
          char_t const *default_name = LUMEX_XML_TEXT(":anonymous");

          switch(LUMEX_XML_NODETYPE(node))
          {
          case node_pcdata:
            text_output(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT(""), ctx_special_pcdata,
                        flags);
            break;

          case node_cdata:
            text_output_cdata(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT(""));
            break;

          case node_comment:
            node_output_comment(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT(""));
            break;

          case node_pi:
            writer.write('<', '?');
            writer.write_string((node->name != nullptr) ? node->name + 0 : default_name);

            if(node->value != nullptr)
            {
              writer.write(' ');
              node_output_pi_value(writer, node->value);
            }

            writer.write('?', '>');
            break;

          case node_declaration:
            writer.write('<', '?');
            writer.write_string((node->name != nullptr) ? node->name + 0 : default_name);
            node_output_attributes(writer, node, LUMEX_XML_TEXT(""), 0, flags | Constants::kformat_raw, 0);
            writer.write('?', '>');
            break;

          case node_doctype:
            writer.write('<', '!', 'D', 'O', 'C');
            writer.write('T', 'Y', 'P', 'E');

            if(node->value != nullptr)
            {
              writer.write(' ');
              writer.write_string(node->value);
            }

            writer.write('>');
            break;

          default: LUMEX_ASSERT(false && "Invalid node type"); // unreachable
          }
        }
      } // anonymous namespace (internal functions)

      LUMEX_API
      inline void
      node_output( // NOLINT(misc-use-internal-linkage, readability-function-cognitive-complexity)
        XmlBufferedWriter &writer, XmlNodeBase *root, char_t const *indent, unsigned int flags, unsigned int depth)
      {
        size_t indent_length      = (((flags & (Constants::kformat_indent | Constants::kformat_indent_attributes)) != 0)
                                && (flags & Constants::kformat_raw) == 0)
                                      ? Utility::strlength(indent)
                                      : 0;
        unsigned int indent_flags = indent_indent;

        XmlNodeBase *node         = root;

        do { // NOLINT(cppcoreguidelines-avoid-do-while)
          LUMEX_ASSERT(node);

          // begin writing current node
          if(LUMEX_XML_NODETYPE(node) == node_pcdata || LUMEX_XML_NODETYPE(node) == node_cdata)
          {
            node_output_simple(writer, node, flags);

            indent_flags = 0;
          }
          else
          {
            if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');

            if(((indent_flags & indent_indent) != 0) && (indent_length != 0))
              text_output_indent(writer, indent, indent_length, depth);

            if(LUMEX_XML_NODETYPE(node) == node_element)
            {
              indent_flags = indent_newline | indent_indent;

              if(node_output_start(writer, node, indent, indent_length, flags, depth))
              {
                // element nodes can have value if parse_embed_pcdata was used
                if(node->value != nullptr) indent_flags = 0;

                node = node->first_child;
                depth++;
                continue;
              }
            }
            else if(LUMEX_XML_NODETYPE(node) == node_document)
            {
              indent_flags = indent_indent;

              if(node->first_child != nullptr)
              {
                node = node->first_child;
                continue;
              }
            }
            else
            {
              node_output_simple(writer, node, flags);

              indent_flags = indent_newline | indent_indent;
            }
          }

          // continue to the next node
          while(node != root)
          {
            if(node->next_sibling != nullptr)
            {
              node = node->next_sibling;
              break;
            }

            node = node->parent;

            // write closing node
            if(LUMEX_XML_NODETYPE(node) == node_element)
            {
              depth--;

              if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');

              if(((indent_flags & indent_indent) != 0) && (indent_length != 0))
                text_output_indent(writer, indent, indent_length, depth);

              node_output_end(writer, node);

              indent_flags = indent_newline | indent_indent;
            }
          }
        } while(node != root);

        if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');
      }

      LUMEX_API
      bool operator&&(XmlNode const &lhs, bool rhs);

      LUMEX_API
      bool operator||(XmlNode const &lhs, bool rhs);
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_HPP

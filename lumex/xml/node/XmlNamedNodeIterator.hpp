#ifndef LUMEX_XML_NAMED_NODE_ITERATOR_HPP
#define LUMEX_XML_NAMED_NODE_ITERATOR_HPP

#include <iterator>

#include "lumex/xml/node/XmlNode.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      class XmlNamedNodeIterator
      {
        friend class XmlNode;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = XmlNode;
        using pointer           = XmlNode *;
        using reference         = XmlNode &;

        using iterator_category = std::bidirectional_iterator_tag;

        // Default constructor
        XmlNamedNodeIterator();

        // Construct an iterator which points to the specified node
        // Note: name pointer is stored in the iterator and must have a longer lifetime than iterator itself
        XmlNamedNodeIterator(XmlNode const &node, char_t const *name);

        // Iterator operators
        bool operator==(XmlNamedNodeIterator const &rhs) const;
        bool operator!=(XmlNamedNodeIterator const &rhs) const;

        XmlNode &operator*() const;
        XmlNode *operator->() const;

        XmlNamedNodeIterator &operator++();
        XmlNamedNodeIterator operator++(int);

        XmlNamedNodeIterator &operator--();
        XmlNamedNodeIterator operator--(int);

      private:
        mutable XmlNode m_wrap;
        XmlNode m_parent;
        char_t const *m_name;

        XmlNamedNodeIterator(XmlNodeBase *ref, XmlNodeBase *parent, char_t const *name);
      };
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NAMED_NODE_ITERATOR_HPP

#ifndef LUMEX_XML_NODE_ITERATOR_HPP
#define LUMEX_XML_NODE_ITERATOR_HPP

#include <iterator>

#include "lumex/xml/node/XmlNode.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      class XmlNodeIterator
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
        XmlNodeIterator() = default;

        // Construct an iterator which points to the specified node
        XmlNodeIterator(XmlNode const &node);

        // Iterator operators
        bool operator==(XmlNodeIterator const &rhs) const;
        bool operator!=(XmlNodeIterator const &rhs) const;

        XmlNode &operator*() const;
        XmlNode *operator->() const;

        XmlNodeIterator &operator++();
        XmlNodeIterator operator++(int);

        XmlNodeIterator &operator--();
        XmlNodeIterator operator--(int);

      private:
        mutable XmlNode m_wrap;
        XmlNode m_parent;

        XmlNodeIterator(XmlNodeBase *ref, XmlNodeBase *parent);
      };
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_ITERATOR_HPP

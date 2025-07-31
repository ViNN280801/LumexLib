#ifndef LUMEX_XML_NODE_ITERATOR_HPP
#define LUMEX_XML_NODE_ITERATOR_HPP

#include <iterator>

#include "lumex/LumexExport.hpp"

#include "lumex/xml/node/LumexXmlNode.hpp"

using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      class LUMEX_API LumexXmlNodeIterator
      {
        friend class LumexXmlNode;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = LumexXmlNode;
        using pointer           = LumexXmlNode *;
        using reference         = LumexXmlNode &;

        using iterator_category = std::bidirectional_iterator_tag;

        // Default constructor
        LumexXmlNodeIterator() = default;

        // Construct an iterator which points to the specified node
        LumexXmlNodeIterator(LumexXmlNode const &node);

        // Iterator operators
        bool operator==(LumexXmlNodeIterator const &rhs) const;
        bool operator!=(LumexXmlNodeIterator const &rhs) const;

        LumexXmlNode &operator*() const;
        LumexXmlNode *operator->() const;

        LumexXmlNodeIterator &operator++();
        LumexXmlNodeIterator operator++(int);

        LumexXmlNodeIterator &operator--();
        LumexXmlNodeIterator operator--(int);

      private:
        mutable LumexXmlNode m_wrap;
        LumexXmlNode m_parent;

        LumexXmlNodeIterator(xml_node_t *ref, xml_node_t *parent);
      };
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_ITERATOR_HPP

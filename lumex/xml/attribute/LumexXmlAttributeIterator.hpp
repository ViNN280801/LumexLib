#ifndef LUMEX_XML_ATTRIBUTE_ITERATOR_HPP
#define LUMEX_XML_ATTRIBUTE_ITERATOR_HPP

#include "lumex/xml/attribute/LumexXmlAttribute.hpp"
#include "lumex/xml/node/LumexXmlNode.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Attribute
    {
      class LUMEX_API LumexXmlAttributeIterator
      {
        friend class Lumex::Xml::Node::LumexXmlNode;

      private:
        mutable LumexXmlAttribute m_wrap;
        Lumex::Xml::Node::LumexXmlNode m_parent;

        LumexXmlAttributeIterator(xml_attr_t *ref, xml_node_t *parent);

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = LumexXmlAttribute;
        using pointer           = LumexXmlAttribute *;
        using reference         = LumexXmlAttribute &;

        using iterator_category = std::bidirectional_iterator_tag;

        // Default constructor
        LumexXmlAttributeIterator() = default;

        // Construct an iterator which points to the specified attribute
        LumexXmlAttributeIterator(LumexXmlAttribute const &attr, Lumex::Xml::Node::LumexXmlNode const &parent);

        // Iterator operators
        bool operator==(LumexXmlAttributeIterator const &rhs) const;
        bool operator!=(LumexXmlAttributeIterator const &rhs) const;

        LumexXmlAttribute &operator*() const;
        LumexXmlAttribute *operator->() const;

        LumexXmlAttributeIterator &operator++();
        LumexXmlAttributeIterator operator++(int);

        LumexXmlAttributeIterator &operator--();
        LumexXmlAttributeIterator operator--(int);
      };
    } // namespace Attribute
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ATTRIBUTE_ITERATOR_HPP

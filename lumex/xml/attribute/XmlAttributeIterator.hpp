#ifndef LUMEX_XML_ATTRIBUTE_ITERATOR_HPP
#define LUMEX_XML_ATTRIBUTE_ITERATOR_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/node/XmlNode.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Attribute
    {
      class LUMEX_API XmlAttributeIterator
      {
      private:
        mutable XmlAttribute m_wrap;
        XmlNode m_parent;

      public:
        // Iterator traits
        using difference_type   = ptrdiff_t;
        using value_type        = XmlAttribute;
        using pointer           = XmlAttribute *;
        using reference         = XmlAttribute &;

        using iterator_category = std::bidirectional_iterator_tag;

        // Default constructor
        XmlAttributeIterator() = default;

        XmlAttributeIterator(XmlAttributeBase *ref, XmlNodeBase *parent);

        // Construct an iterator which points to the specified attribute
        XmlAttributeIterator(XmlAttribute const &attr, XmlNode const &parent);

        // Iterator operators
        bool operator==(XmlAttributeIterator const &rhs) const;
        bool operator!=(XmlAttributeIterator const &rhs) const;

        XmlAttribute &operator*() const;
        XmlAttribute *operator->() const;

        XmlAttributeIterator &operator++();
        XmlAttributeIterator operator++(int);

        XmlAttributeIterator &operator--();
        XmlAttributeIterator operator--(int);
      };
    } // namespace Attribute
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ATTRIBUTE_ITERATOR_HPP

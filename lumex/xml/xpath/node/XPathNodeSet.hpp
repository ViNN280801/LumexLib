#ifndef LUMEX_XML_XPATH_NODE_SET_HPP
#define LUMEX_XML_XPATH_NODE_SET_HPP

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstdint>

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/xpath/memory/XPathAllocator.hpp"

#include "XPathNode.hpp"

using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Node
      {
        class LUMEX_API XPathNodeSet
        {
        public:
          using const_iterator = XPathNode const *;
          using iterator       = XPathNode const *;

          enum type_t : std::uint8_t
          {
            type_unsorted,      // Not ordered
            type_sorted,        // Sorted by document order (ascending)
            type_sorted_reverse // Sorted by document order (descending)
          };

          // Default constructor. Constructs empty set.
          XPathNodeSet();

          // Constructs a set from iterator range; data is not checked for duplicates and is not sorted according to
          // provided type, so be careful
          XPathNodeSet(const_iterator begin, const_iterator end, type_t type = type_unsorted);

          // Destructor
          ~XPathNodeSet();

          // Copy constructor/assignment operator
          XPathNodeSet(XPathNodeSet const &rhs);

          XPathNodeSet &operator=(XPathNodeSet const &rhs);

          // Move semantics support
          XPathNodeSet(XPathNodeSet &&rhs) noexcept;

          XPathNodeSet &operator=(XPathNodeSet &&rhs) noexcept;

          // Get collection type
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          type_t type() const;

          // Get collection size
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          // Indexing operator
          XPathNode const &operator[](size_t index) const;

          // Collection iterators
          LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for traversing the node set; discarding it "
                                    "negates the purpose of iteration.")
          const_iterator begin() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          const_iterator end() const;

          // Sort the collection in ascending/descending order by document order
          void sort(bool reverse = false);

          // Get first node in the collection by document order
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned first node should be used; discarding it negates the purpose of the getter.")
          XPathNode first() const;

          // Check if collection is empty
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

        private:
          type_t m_type;

          std::array<XPathNode, 1> m_storage;

          XPathNode *m_begin{};
          XPathNode *m_end{};

          void _assign(const_iterator begin, const_iterator end, type_t type);
          void _move(XPathNodeSet &rhs) noexcept;
        };

        class LUMEX_API XPathNodeSetRaw
        {
        public:
          XPathNodeSetRaw() = default;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          XPathNode *begin() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          XPathNode *end() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned first node should be used; discarding it negates the purpose of the getter.")
          XPathNode first() const;

          void push_back_grow(XPathNode const &node, XPathAllocator *alloc);

          void push_back(XPathNode const &node, XPathAllocator *alloc);

          void append(XPathNode const *begin_, XPathNode const *end_, XPathAllocator *alloc);

          void sort_do();

          void truncate(XPathNode *pos);

          void remove_duplicates(XPathAllocator *alloc);

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          XPathNodeSet::type_t type() const;

          void set_type(XPathNodeSet::type_t value);

        private:
          XPathNodeSet::type_t m_type{};

          XPathNode *m_begin{};
          XPathNode *m_end{};
          XPathNode *m_eos{};
        };

        static XPathNodeSet const dummy_node_set;
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_SET_HPP

#ifndef LUMEX_XML_XPATH_NODE_SET_HPP
#define LUMEX_XML_XPATH_NODE_SET_HPP

#include <array>
#include <cstdint>

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/xpath/memory/LumexXmlXPathAllocator.hpp"

#include "LumexXmlXPathNode.hpp"

using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Node
      {
        class LUMEX_API LumexXmlXPathNodeSet
        {
        public:
          using const_iterator = LumexXmlXPathNode const *;
          using iterator       = LumexXmlXPathNode const *;

          enum type_t : std::uint8_t
          {
            type_unsorted,      // Not ordered
            type_sorted,        // Sorted by document order (ascending)
            type_sorted_reverse // Sorted by document order (descending)
          };

          // Default constructor. Constructs empty set.
          LumexXmlXPathNodeSet();

          // Constructs a set from iterator range; data is not checked for duplicates and is not sorted according to
          // provided type, so be careful
          LumexXmlXPathNodeSet(const_iterator begin, const_iterator end, type_t type = type_unsorted);

          // Destructor
          ~LumexXmlXPathNodeSet();

          // Copy constructor/assignment operator
          LumexXmlXPathNodeSet(LumexXmlXPathNodeSet const &rhs);

          LumexXmlXPathNodeSet &operator=(LumexXmlXPathNodeSet const &rhs);

          // Move semantics support
          LumexXmlXPathNodeSet(LumexXmlXPathNodeSet &&rhs) noexcept;

          LumexXmlXPathNodeSet &operator=(LumexXmlXPathNodeSet &&rhs) noexcept;

          // Get collection type
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          type_t type() const;

          // Get collection size
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          // Indexing operator
          LumexXmlXPathNode const &operator[](size_t index) const;

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
          LumexXmlXPathNode first() const;

          // Check if collection is empty
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

        private:
          type_t m_type;

          std::array<LumexXmlXPathNode, 1> m_storage;

          LumexXmlXPathNode *m_begin{};
          LumexXmlXPathNode *m_end{};

          void _assign(const_iterator begin, const_iterator end, type_t type);
          void _move(LumexXmlXPathNodeSet &rhs) noexcept;
        };

        class LumexXmlXPathNodeSetRaw
        {
        public:
          LumexXmlXPathNodeSetRaw() = default;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          LumexXmlXPathNode *begin() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          LumexXmlXPathNode *end() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned first node should be used; discarding it negates the purpose of the getter.")
          LumexXmlXPathNode first() const;

          void push_back_grow(LumexXmlXPathNode const &node, LumexXmlXPathAllocator *alloc);

          void push_back(LumexXmlXPathNode const &node, LumexXmlXPathAllocator *alloc);

          void append(LumexXmlXPathNode const *begin_, LumexXmlXPathNode const *end_, LumexXmlXPathAllocator *alloc);

          void sort_do();

          void truncate(LumexXmlXPathNode *pos);

          void remove_duplicates(LumexXmlXPathAllocator *alloc);

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          LumexXmlXPathNodeSet::type_t type() const;

          void set_type(LumexXmlXPathNodeSet::type_t value);

        private:
          LumexXmlXPathNodeSet::type_t m_type{};

          LumexXmlXPathNode *m_begin{};
          LumexXmlXPathNode *m_end{};
          LumexXmlXPathNode *m_eos{};
        };

        static LumexXmlXPathNodeSet const dummy_node_set;
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_SET_HPP

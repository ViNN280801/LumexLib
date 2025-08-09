#ifndef LUMEX_XML_XPATH_UTILS_HPP
#define LUMEX_XML_XPATH_UTILS_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/xpath/node/XPathNode.hpp"
#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class XPathVariable;
      }

      namespace Utility
      {
        using namespace Variable;

        /**
         * @brief Determines the current document order of a range of XPath nodes.
         * @details This function efficiently checks if a given range of `XPathNode`s is
         *          sorted in ascending, descending, or unsorted document order. It is an
         *          optimization to avoid unnecessary sorting.
         * @param begin A pointer to the first `XPathNode` in the range.
         * @param end A pointer to one past the last `XPathNode` in the range.
         * @return An `XPathNodeSet::type_t` value indicating the order of the nodes in the range.
         * @note This function performs a linear scan and is primarily used internally by `xpath_sort`.
         */
        LUMEX_API
        XPathNodeSet::type_t xpath_get_order(XPathNode const *begin, XPathNode const *end);

        /**
         * @brief Sorts a range of XPath nodes by document order.
         * @details This function sorts the nodes in the specified range `[begin, end)`
         *          according to their XML document order. It can sort in ascending or
         *          descending order. It first checks the current order using `xpath_get_order`
         *          to avoid redundant sorting.
         * @param begin A pointer to the first `XPathNode` in the mutable range to sort.
         * @param end A pointer to one past the last `XPathNode` in the mutable range to sort.
         * @param type The current `XPathNodeSet::type_t` of the range before sorting.
         * @param rev If `true`, sort in descending document order; otherwise, ascending.
         * @return The new `XPathNodeSet::type_t` indicating the sorted order of the nodes.
         */
        LUMEX_API
        XPathNodeSet::type_t xpath_sort(XPathNode *begin, XPathNode *end, XPathNodeSet::type_t type, bool rev);

        /**
         * @brief Retrieves the first node in document order from a range of XPath nodes.
         * @details This function efficiently finds the first `XPathNode` in XML document order
         *          within the specified range `[begin, end)`. If the range is already sorted,
         *          it directly returns the first or last element. If unsorted, it performs a
         *          linear scan using `std::min_element` with a document order comparator.
         * @param begin A pointer to the first `XPathNode` in the range.
         * @param end A pointer to one past the last `XPathNode` in the range.
         * @param type The `XPathNodeSet::type_t` indicating the current order of the nodes.
         * @return The `XPathNode` that appears first in document order, or an empty `XPathNode` if the range is empty.
         * @throws `LUMEX_ASSERT` if an invalid `type` is provided.
         */
        LUMEX_API
        XPathNode xpath_first(XPathNode const *begin, XPathNode const *end, XPathNodeSet::type_t type);

        /**
         * @brief Copies the value of one XPath variable to another.
         * @details This function performs a deep copy of the value from `rhs` (source variable)
         *          to `lhs` (destination variable), handling different XPath variable types
         *          (node-set, number, string, boolean).
         * @param lhs A pointer to the destination `XPathVariable` to which the value will be copied.
         * @param rhs A constant pointer to the source `XPathVariable` from which the value will be copied.
         * @return `true` if the copy was successful, `false` otherwise (e.g., if memory allocation fails for node-set).
         * @throws `LUMEX_ASSERT` if an invalid variable type is encountered.
         * @note This function is typically used in XPath variable assignment or initialization scenarios.
         */
        LUMEX_API
        bool copy_xpath_variable(XPathVariable *lhs, XPathVariable const *rhs);
      } // namespace Utility
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_UTILS_HPP

#ifndef LUMEXXML_OBJECT_RANGE_HPP
#define LUMEXXML_OBJECT_RANGE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexUtility"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Range
    {
      /**
       * @brief Range of XML objects, providing iteration over a collection of elements.
       *
       * The `xml_object_range` class provides a lightweight view on a sequence of
       * elements, defined by a pair of iterators `begin` and `end`. It is intended for
       * use in scenarios where access to the range of data is required without copying
       * or modifying the underlying collection.
       *
       * @tparam Iterator Type of iterator that must conform to the `std::forward_iterator`
       *                  (C++20). This ensures that the iterator is copyable, comparable
       *                  for equality/inequality, and can be dereferenced.
       *                  For earlier C++ standards (before C++20), similar checks would
       *                  be implemented using SFINAE and `std::iterator_traits`,
       *                  for example, through `std::enable_if_t<std::is_base_of_v<std::input_iterator_tag,
       *                  typename std::iterator_traits<Iterator>::iterator_category>>`.
       *                  Using `std::forward_iterator` in C++20 is a more explicit and
       *                  clear way to express these requirements.
       *
       * @warning The class does not own the data pointed to by the iterators. The lifecycle
       *          of the underlying collection must be managed by the calling code.
       */
      template <typename Iterator>
      class XmlObjectRange
      {
      public:
        /**
         * @brief Alias for the type of the constant iterator.
         *
         * Used for uniformity, since the range does not provide methods
         * for non-constant modification.
         */
        using const_iterator = Iterator;

        /**
         * @brief Alias for the type of the iterator.
         *
         * Used for uniformity, since the range does not provide methods
         * for non-constant modification.
         */
        using iterator = Iterator;

        /**
         * @brief Constructor of the XML object range.
         *
         * Initializes the range with a pair of iterators pointing to the beginning and end of the collection.
         *
         * @param begin Iterator pointing to the first element of the range.
         * @param end Iterator pointing to the position after the last element of the range.
         */
        XmlObjectRange(Iterator begin, Iterator end) : m_begin(begin), m_end(end) {}

        /**
         * @brief Returns the iterator to the beginning of the range.
         *
         * @return Iterator pointing to the first element of the range.
         */
        Iterator
        begin() const
        {
          return m_begin;
        }

        /**
         * @brief Returns the iterator to the end of the range.
         *
         * @return Iterator pointing to the position after the last element of the range.
         */
        Iterator
        end() const
        {
          return m_end;
        }

        /**
         * @brief Checks if the range is empty.
         *
         * The range is considered empty if the beginning iterator is equal to the end iterator.
         *
         * @return `true` if the range is empty; `false` otherwise.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the same as the one returned by the begin() method; discarding it "
          "negates the purpose of the getter.")
        bool
        empty() const
        {
          return m_begin == m_end;
        }

      private:
        Iterator m_begin; ///< Iterator pointing to the beginning of the range.
        Iterator m_end;   ///< Iterator pointing to the end of the range (position after the last element).
      };
    } // namespace Range
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_OBJECT_RANGE_HPP

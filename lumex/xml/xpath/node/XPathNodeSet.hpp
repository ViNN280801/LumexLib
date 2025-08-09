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
        /**
         * @brief Represents a collection of XPath nodes, managing their order and uniqueness.
         * @details This class provides a container for `XPathNode` objects, supporting
         *          different ordering types (unsorted, sorted, reverse sorted by document order).
         *          It internally manages a buffer for storing nodes, using a small array
         *          optimization for 0 or 1 elements and heap allocation for larger sets.
         *          It offers iterators, size queries, and sorting capabilities.
         *
         * @note This class manages its own memory for storing `XPathNode` objects (not the
         *       underlying XML node/attribute data, which is externally managed).
         * @warning Copying large `XPathNodeSet` objects can be expensive due to deep copying.
         *          Move semantics are provided for efficient transfers.
         */
        class LUMEX_API XPathNodeSet
        {
        public:
          /// @brief Type alias for a constant iterator over `XPathNode` objects.
          using const_iterator = XPathNode const *;
          /// @brief Type alias for a mutable iterator over `XPathNode` objects (though typically constant access is
          /// preferred).
          using iterator = XPathNode const *;

          /**
           * @brief Enumeration defining the ordering type of the node set.
           */
          enum type_t : std::uint8_t
          {
            type_unsorted,      ///< The node set is not ordered.
            type_sorted,        ///< The node set is sorted in ascending document order.
            type_sorted_reverse ///< The node set is sorted in descending document order.
          };

          /**
           * @brief Default constructor. Constructs an empty node set.
           * @details Initializes an `XPathNodeSet` with no nodes and an `type_unsorted` type.
           */
          XPathNodeSet();

          /**
           * @brief Constructs a node set from an iterator range.
           * @details Initializes an `XPathNodeSet` by copying nodes from the specified range.
           *          The uniqueness and order of the nodes are *not* guaranteed by this constructor;
           *          it's the caller's responsibility to ensure data integrity if needed.
           * @param begin An iterator pointing to the beginning of the range of `XPathNode`s to copy.
           * @param end An iterator pointing to the end of the range of `XPathNode`s to copy.
           * @param type The initial ordering type of the collection (defaults to `type_unsorted`).
           * @throws `std::bad_alloc` if memory allocation fails for the internal buffer.
           */
          XPathNodeSet(const_iterator begin, const_iterator end, type_t type = type_unsorted);

          /**
           * @brief Destructor.
           * @details Frees any dynamically allocated memory used by the node set's internal buffer.
           */
          ~XPathNodeSet();

          /**
           * @brief Copy constructor.
           * @details Creates a new `XPathNodeSet` by performing a deep copy of the `rhs` node set's contents.
           * @param rhs The `XPathNodeSet` to copy from.
           * @throws `std::bad_alloc` if memory allocation fails during the copy.
           */
          XPathNodeSet(XPathNodeSet const &rhs);

          /**
           * @brief Copy assignment operator.
           * @details Assigns the contents of `rhs` to this `XPathNodeSet` by performing a deep copy.
           * @param rhs The `XPathNodeSet` to copy from.
           * @return A reference to this `XPathNodeSet` after assignment.
           * @throws `std::bad_alloc` if memory allocation fails during the copy.
           */
          XPathNodeSet &operator=(XPathNodeSet const &rhs);

          /**
           * @brief Move constructor.
           * @details Constructs a new `XPathNodeSet` by efficiently moving resources from `rhs`,
           *          leaving `rhs` in a valid but unspecified (typically empty) state.
           * @param rhs The `XPathNodeSet` to move from.
           */
          XPathNodeSet(XPathNodeSet &&rhs) noexcept;

          /**
           * @brief Move assignment operator.
           * @details Moves the contents of `rhs` to this `XPathNodeSet`, efficiently transferring
           *          resource ownership. Any resources held by this object prior to the move
           *          are released. `rhs` is left in a valid but unspecified (typically empty) state.
           * @param rhs The `XPathNodeSet` to move from.
           * @return A reference to this `XPathNodeSet` after assignment.
           */
          XPathNodeSet &operator=(XPathNodeSet &&rhs) noexcept;

          /**
           * @brief Gets the current ordering type of the node set.
           * @return The `type_t` enumeration value indicating the current order.
           */
          // The returned collection type should be used; discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          type_t type() const;

          /**
           * @brief Gets the number of nodes in the set.
           * @return The `size_t` count of nodes.
           */
          // The returned size should be used; discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          /**
           * @brief Accesses a node by index.
           * @details Provides direct, constant access to the `XPathNode` at the specified `index`.
           * @param index The zero-based index of the node to access.
           * @return A constant reference to the `XPathNode` at `index`.
           * @throws `LUMEX_ASSERT` if `index` is out of bounds.
           */
          XPathNode const &operator[](size_t index) const;

          /**
           * @brief Returns a constant iterator to the beginning of the node set.
           * @return A `const_iterator` pointing to the first `XPathNode`.
           */
          // The returned iterator should be used for traversing the node set; discarding it
          // negates the purpose of iteration.
          LUMEX_ATTRIBUTE_NODISCARD("The returned iterator should be used for traversing the node set; discarding it "
                                    "negates the purpose of iteration.")
          const_iterator begin() const;

          /**
           * @brief Returns a constant iterator to the end of the node set.
           * @details This iterator points one past the last element, following standard
           *          C++ iterator conventions.
           * @return A `const_iterator` pointing to the end of the `XPathNodeSet`.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          const_iterator end() const;

          /**
           * @brief Sorts the collection by document order.
           * @details Arranges the nodes in the set according to XML document order.
           *          The order can be ascending (default) or descending.
           * @param reverse If `true`, sorts in descending document order; otherwise, ascending.
           * @note This operation modifies the internal order of nodes and updates the `m_type`.
           * @see Lumex::Xml::XPath::Utility::xpath_sort for the underlying sorting algorithm.
           */
          void sort(bool reverse = false);

          /**
           * @brief Gets the first node in the collection based on document order.
           * @details If the set is already sorted, it returns the first element. If unsorted,
           *          it finds the first node in document order without fully sorting the set.
           * @return The first `XPathNode` in document order. Returns an empty `XPathNode` if the set is empty.
           * @see Lumex::Xml::XPath::Utility::xpath_first for the underlying logic.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned first node should be used; discarding it negates the purpose of the getter.")
          XPathNode first() const;

          /**
           * @brief Checks if the node set is empty.
           * @return `true` if the set contains no nodes, `false` otherwise.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

        private:
          /// @brief The current ordering type of the node set.
          type_t m_type;
          /// @brief Small internal buffer for optimizing storage of 0 or 1 nodes.
          std::array<XPathNode, 1> m_storage;
          /// @brief Pointer to the beginning of the node data (either `m_storage.data()` or a heap-allocated buffer).
          XPathNode *m_begin{};
          /// @brief Pointer to one past the last node in the data.
          XPathNode *m_end{};

          /**
           * @brief Internal helper to assign node data from a range.
           * @details Copies nodes from the specified range into the internal buffer,
           *          managing memory allocation (using `m_storage` or heap).
           * @param begin An iterator to the start of the source range.
           * @param end An iterator to the end of the source range.
           * @param type The type of the node set to be assigned.
           * @throws `std::bad_alloc` if memory allocation fails.
           * @warning This function does not perform duplicate checking or sorting.
           */
          void _assign(const_iterator begin, const_iterator end, type_t type);
          /**
           * @brief Internal helper to move resources from another `XPathNodeSet`.
           * @details Performs a shallow copy of pointers and capacity from `rhs`,
           *          leaving `rhs` in an empty state. Used for move constructors/assignments.
           * @param rhs The `XPathNodeSet` to move from.
           */
          void _move(XPathNodeSet &rhs) noexcept;
        };

        /**
         * @brief Represents a raw, mutable node set used primarily during XPath evaluation.
         * @details This class provides a low-level, mutable view of a collection of `XPathNode`s.
         *          It's designed for efficient modification during XPath evaluation, allowing
         *          nodes to be added, sorted, truncated, and duplicates removed. It directly
         *          interacts with an `XPathAllocator` for its memory management.
         *
         * @note This class does not own the `XPathAllocator` it uses, nor does it typically
         *       own the memory blocks unless they are specifically reallocated by its `push_back_grow`
         *       or `append` methods. Its lifetime is often tied to a temporary `XPathStack` or `XPathAllocator`.
         * @warning Due to its low-level nature and direct pointer manipulation, careful usage
         *          and interaction with `XPathAllocator` are required to avoid memory errors.
         */
        class LUMEX_API XPathNodeSetRaw
        {
        public:
          /**
           * @brief Default constructor. Constructs an empty raw node set.
           * @details Initializes the raw node set with null pointers, indicating no nodes.
           */
          XPathNodeSetRaw() = default;

          /**
           * @brief Returns a mutable pointer to the beginning of the node data.
           * @return A `XPathNode*` pointer to the first node in the set.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          XPathNode *begin() const;

          /**
           * @brief Returns a mutable pointer to one past the last node in the data.
           * @return A `XPathNode*` pointer to the end of the node set.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned iterator should be used for delimiting node set iteration; discarding "
            "it negates the purpose of iteration.")
          XPathNode *end() const;

          /**
           * @brief Checks if the raw node set is empty.
           * @return `true` if the set contains no nodes, `false` otherwise.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned boolean indicates whether the node set is empty; discarding it negates "
            "the purpose of the getter.")
          bool empty() const;

          /**
           * @brief Gets the number of nodes currently in the raw node set.
           * @return The `size_t` count of nodes.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned size should be used; discarding it negates the purpose of the getter.")
          size_t size() const;

          /**
           * @brief Gets the first node in the collection by document order.
           * @details Finds the first `XPathNode` in document order within the raw set.
           *          This may involve a linear scan if the set is unsorted.
           * @return The first `XPathNode` in document order. Returns an empty `XPathNode` if the set is empty.
           * @see Lumex::Xml::XPath::Utility::xpath_first for the underlying logic.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned first node should be used; discarding it negates the purpose of the getter.")
          XPathNode first() const;

          /**
           * @brief Adds a node to the end of the set, growing the underlying buffer if necessary.
           * @details This function is called internally by `push_back` when the current
           *          capacity of the `XPathNodeSetRaw` is exhausted. It reallocates the
           *          underlying buffer to accommodate more nodes.
           * @param node The `XPathNode` to add.
           * @param alloc The `XPathAllocator` to use for memory reallocation.
           * @note This function has `NOINLINE` attribute to prevent inlining for size optimization.
           */
          void push_back_grow(XPathNode const &node, XPathAllocator *alloc);

          /**
           * @brief Adds a node to the end of the set.
           * @details Appends a new `XPathNode` to the end of the raw node set. If the
           *          current capacity is not sufficient, it calls `push_back_grow` to
           *          reallocate the internal buffer.
           * @param node The `XPathNode` to add.
           * @param alloc The `XPathAllocator` to use for potential memory allocation.
           */
          void push_back(XPathNode const &node, XPathAllocator *alloc);

          /**
           * @brief Appends a range of nodes to the end of the set.
           * @details Copies nodes from the specified range (`begin_` to `end_`) to the
           *          end of the current raw node set. Reallocates the internal buffer
           *          if the combined size exceeds current capacity.
           * @param begin_ A pointer to the beginning of the range of `XPathNode`s to append.
           * @param end_ A pointer to the end of the range of `XPathNode`s to append.
           * @param alloc The `XPathAllocator` to use for potential memory allocation.
           */
          void append(XPathNode const *begin_, XPathNode const *end_, XPathAllocator *alloc);

          /**
           * @brief Sorts the raw node set in ascending document order.
           * @details Applies an in-place sort to the nodes within the raw node set
           *          based on XML document order.
           * @note This operation updates the internal `m_type` to `XPathNodeSet::type_sorted`.
           * @see Lumex::Xml::XPath::Utility::xpath_sort for the underlying sorting algorithm.
           */
          void sort_do();

          /**
           * @brief Truncates the raw node set to a new end position.
           * @details Resizes the raw node set, effectively removing all nodes from `pos`
           *          to the original end. The memory is not deallocated but marked as unused.
           * @param pos A `XPathNode*` pointer specifying the new logical end of the set.
           *            Must be within the valid range of `[begin(), end()]`.
           * @throws `LUMEX_ASSERT` if `pos` is out of bounds.
           */
          void truncate(XPathNode *pos);

          /**
           * @brief Removes duplicate nodes from the raw node set.
           * @details Iterates through the raw node set and removes any `XPathNode`s that
           *          are duplicates. If the set is unsorted and large, it uses a hash-based
           *          approach for efficiency; otherwise, it falls back to `std::unique`.
           * @param alloc The `XPathAllocator` to use for temporary memory (e.g., for hash table).
           * @note This operation may change the order of nodes if the set was unsorted
           *       and the hash-based method is used.
           */
          void remove_duplicates(XPathAllocator *alloc);

          /**
           * @brief Gets the current ordering type of the raw node set.
           * @return The `XPathNodeSet::type_t` enumeration value indicating the current order.
           */
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned collection type should be used; discarding it negates the purpose of the getter.")
          XPathNodeSet::type_t type() const;

          /**
           * @brief Sets the ordering type of the raw node set.
           * @details Explicitly sets the `m_type` member. This does not perform any sorting;
           *          it merely updates the metadata about the set's order.
           * @param value The `XPathNodeSet::type_t` value to set.
           */
          void set_type(XPathNodeSet::type_t value);

        private:
          /// @brief The current ordering type of the raw node set.
          XPathNodeSet::type_t m_type{};
          /// @brief Pointer to the beginning of the raw node data.
          XPathNode *m_begin{};
          /// @brief Pointer to one past the last valid node in the raw data.
          XPathNode *m_end{};
          /// @brief Pointer to one past the end of allocated storage (end of storage).
          XPathNode *m_eos{};
        };

        /// @brief A global, constant empty `XPathNodeSet` for convenience.
        static XPathNodeSet const dummy_node_set;
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_SET_HPP

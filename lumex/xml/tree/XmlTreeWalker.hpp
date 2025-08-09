#ifndef LUMEX_XML_TREE_WALKER_HPP
#define LUMEX_XML_TREE_WALKER_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Tree
    {
      /**
       * @brief Abstract base class for implementing custom XML tree traversal logic.
       * @details `XmlTreeWalker` defines a customizable interface for walking an XML document tree.
       *          Users can derive from this class and override `begin`, `for_each`, and `end`
       *          methods to implement specific behaviors during traversal, such as filtering nodes,
       *          collecting data, or performing modifications.
       * @note This class is intended to be used with `XmlNode::traverse()`.
       * @see XmlNode::traverse()
       */
      class LUMEX_API XmlTreeWalker // NOLINT(cppcoreguidelines-special-member-functions)
      {
        friend class XmlNode;

      public:
        /**
         * @brief Default constructor for `XmlTreeWalker`.
         * @details Initializes the walker.
         */
        XmlTreeWalker() = default;
        /**
         * @brief Virtual destructor for `XmlTreeWalker`.
         * @details Ensures proper cleanup of derived classes.
         */
        virtual ~XmlTreeWalker() = default;

        /**
         * @brief Callback function invoked when the tree traversal begins for a node.
         * @param[in,out] node The `XmlNode` at which the traversal is beginning.
         * @return `true` to continue traversal, `false` to stop.
         * @details This method can be overridden to perform actions or checks before visiting a node's children.
         */
        virtual bool
        begin(XmlNode & /* unused */)
        {
          return true;
        }

        /**
         * @brief Pure virtual callback function invoked for each node traversed.
         * @param[in,out] node The `XmlNode` currently being visited.
         * @return `true` to continue traversal, `false` to stop.
         * @details This method must be implemented by derived classes to define the core logic of the tree walk.
         */
        virtual bool for_each(XmlNode &node) = 0;

        /**
         * @brief Callback function invoked when the tree traversal ends for a node.
         * @param[in,out] node The `XmlNode` for which traversal is ending (i.e., all its children and their subtrees
         * have been visited).
         * @return `true` to continue traversal, `false` to stop.
         * @details This method can be overridden to perform actions or cleanup after visiting a node's children.
         */
        virtual bool
        end(XmlNode & /* unused */)
        {
          return true;
        }

        /**
         * @brief Retrieves the current traversal depth.
         * @return An integer representing the current depth in the XML tree (root is depth 0).
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned integer indicates the current traversal depth; discarding it negates the "
          "purpose of the getter")
        int
        depth() const
        {
          return m_depth;
        }

        /**
         * @brief Sets the current traversal depth.
         * @param[in] depth The new depth value.
         */
        void
        set_depth(int depth)
        {
          m_depth = depth;
        }

        /**
         * @brief Increments the current traversal depth.
         * @details Used internally during tree descent.
         */
        void
        increment_depth()
        {
          ++m_depth;
        }

        /**
         * @brief Decrements the current traversal depth.
         * @details Used internally during tree ascent.
         */
        void
        decrement_depth()
        {
          --m_depth;
        }

      private:
        /// @brief Internal member to store the current traversal depth.
        int m_depth{};
      };
    } // namespace Tree
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TREE_WALKER_HPP

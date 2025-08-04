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
      class LUMEX_API XmlTreeWalker // NOLINT(cppcoreguidelines-special-member-functions)
      {
        friend class XmlNode;

      public:
        XmlTreeWalker()          = default;
        virtual ~XmlTreeWalker() = default;

        // Callback that is called when traversal begins
        virtual bool
        begin(LUMEX_ATTRIBUTE_MAYBE_UNUSED XmlNode &node)
        {
          return true;
        }

        // Callback that is called for each node traversed
        virtual bool for_each(XmlNode &node) = 0;

        // Callback that is called when traversal ends
        virtual bool
        end(LUMEX_ATTRIBUTE_MAYBE_UNUSED XmlNode &node)
        {
          return true;
        }

      protected:
        // Get current traversal depth
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned integer indicates the current traversal depth; discarding it negates the "
          "purpose of the getter")
        int
        depth() const
        {
          return m_depth;
        }

      private:
        int m_depth{};
      };
    } // namespace Tree
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TREE_WALKER_HPP

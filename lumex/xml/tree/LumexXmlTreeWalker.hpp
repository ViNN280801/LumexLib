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
      class LUMEX_API LumexXmlTreeWalker // NOLINT(cppcoreguidelines-special-member-functions)
      {
        friend class LumexXmlNode;

      public:
        LumexXmlTreeWalker();
        virtual ~LumexXmlTreeWalker();

        // Callback that is called when traversal begins
        virtual bool begin(LumexXmlNode &node);

        // Callback that is called for each node traversed
        virtual bool for_each(LumexXmlNode &node) = 0;

        // Callback that is called when traversal ends
        virtual bool end(LumexXmlNode &node);

      protected:
        // Get current traversal depth
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned integer indicates the current traversal depth; discarding it negates the "
          "purpose of the getter")
        int depth() const;

      private:
        int m_depth;
      };
    } // namespace Tree
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TREE_WALKER_HPP

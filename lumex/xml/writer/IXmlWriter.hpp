#ifndef ILUMEX_XML_IWRITER_HPP
#define ILUMEX_XML_IWRITER_HPP

#include "lumex/LumexExport.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      class LUMEX_API IXmlWriter // NOLINT(cppcoreguidelines-special-member-functions)
      {
      public:
        virtual ~IXmlWriter()                             = default;

        virtual void write(void const *data, size_t size) = 0;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !ILUMEX_XML_IWRITER_HPP

#ifndef LUMEX_XML_XPATH_STACK_HPP
#define LUMEX_XML_XPATH_STACK_HPP

#include "LumexXmlXPathAllocator.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Memory
      {
        struct LumexXmlXPathStack {
          LumexXmlXPathAllocator *result;
          LumexXmlXPathAllocator *temp;
        };

        struct LumexXmlXPathStackData {                     // NOLINT(cppcoreguidelines-special-member-functions)
          std::array<LumexXmlXPathMemoryBlock, 2> blocks{}; // NOLINT(misc-non-private-member-variables-in-classes)
          LumexXmlXPathAllocator result;                    // NOLINT(misc-non-private-member-variables-in-classes)
          LumexXmlXPathAllocator temp;                      // NOLINT(misc-non-private-member-variables-in-classes)
          LumexXmlXPathStack stack{};                       // NOLINT(misc-non-private-member-variables-in-classes)
          bool oom{};                                       // NOLINT(misc-non-private-member-variables-in-classes)

          LumexXmlXPathStackData();
          ~LumexXmlXPathStackData();
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_STACK_HPP

#ifndef LUMEX_XML_XPATH_STACK_HPP
#define LUMEX_XML_XPATH_STACK_HPP

#include "XPathAllocator.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Memory
      {
        struct XPathStack {
          XPathAllocator *result;
          XPathAllocator *temp;
        };

        struct XPathStackData {                     // NOLINT(cppcoreguidelines-special-member-functions)
          std::array<XPathMemoryBlock, 2> blocks{}; // NOLINT(misc-non-private-member-variables-in-classes)
          XPathAllocator result;                    // NOLINT(misc-non-private-member-variables-in-classes)
          XPathAllocator temp;                      // NOLINT(misc-non-private-member-variables-in-classes)
          XPathStack stack{};                       // NOLINT(misc-non-private-member-variables-in-classes)
          bool oom{};                                       // NOLINT(misc-non-private-member-variables-in-classes)

          XPathStackData();
          ~XPathStackData();
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_STACK_HPP

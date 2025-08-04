#ifndef LUMEX_XML_XPATH_EXCEPTION_HPP
#define LUMEX_XML_XPATH_EXCEPTION_HPP

#include <exception>

#include "lumex/core/utility/LumexAssert.hpp"
#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/xpath/parser/XPathParseResult.hpp"

using namespace Lumex::Xml::XPath::Parser;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Exception
      {
        class XPathException : public std::exception
        {
        public:
          // Construct exception from parse result
          explicit XPathException(XPathParseResult const &result_) : m_result(result_) { LUMEX_ASSERT(m_result.error); }

          // Get error message
          LUMEX_ATTRIBUTE_NODISCARD("Error message should not be discarded.")
          char const *
          what() const noexcept override
          {
            return m_result.error;
          }

          // Get parse result
          LUMEX_ATTRIBUTE_NODISCARD("Parse result should not be discarded.")
          XPathParseResult const &
          result() const
          {
            return m_result;
          }

        private:
          XPathParseResult m_result;
        };
      } // namespace Exception
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_EXCEPTION_HPP

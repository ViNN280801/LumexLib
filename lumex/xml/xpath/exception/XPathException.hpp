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
        /**
         * @brief Exception class for XPath parsing and evaluation errors.
         * @details This class inherits from `std::exception` and provides detailed
         *          information about errors encountered during XPath operations,
         *          including the error message and the `XPathParseResult` which
         *          contains context about the error location.
         *
         * @note This exception is thrown when XPath parsing fails or when
         *       XPath evaluation encounters an unrecoverable error.
         * @see Lumex::Xml::XPath::Parser::XPathParseResult for detailed error information.
         */
        class XPathException : public std::exception
        {
        public:
          /**
           * @brief Constructs an XPathException from a parse result.
           * @details Initializes the exception with an `XPathParseResult` that
           *          describes the XPath error. An assertion ensures that the
           *          provided `result_` indicates an actual error.
           * @param result_ A constant reference to an `XPathParseResult` object
           *                containing the error details.
           * @throws `LUMEX_ASSERT` if `result_.error` is `nullptr`.
           */
          explicit XPathException(XPathParseResult const &result_) : m_result(result_) { LUMEX_ASSERT(m_result.error); }

          /**
           * @brief Returns a null-terminated character string describing the exception.
           * @details This function overrides `std::exception::what()` to provide
           *          the error message from the encapsulated `XPathParseResult`.
           * @return A C-style string containing the error message. The pointer
           *         is guaranteed to be valid as long as the exception object exists.
           * @note The returned string is owned by the exception object and should not be freed.
           */
          // Error message should not be discarded.
          LUMEX_ATTRIBUTE_NODISCARD("Error message should not be discarded.")
          char const *
          what() const noexcept override
          {
            return m_result.error;
          }

          /**
           * @brief Retrieves the parse result associated with this exception.
           * @details Provides access to the `XPathParseResult` object that was
           *          used to construct this exception, allowing callers to
           *          inspect more detailed error information, such as the error
           *          offset within the XPath expression.
           * @return A constant reference to the `XPathParseResult` object.
           */
          // Parse result should not be discarded.
          LUMEX_ATTRIBUTE_NODISCARD("Parse result should not be discarded.")
          XPathParseResult const &
          result() const
          {
            return m_result;
          }

        private:
          /// @brief The parse result containing details about the XPath error.
          XPathParseResult m_result;
        };
      } // namespace Exception
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_EXCEPTION_HPP

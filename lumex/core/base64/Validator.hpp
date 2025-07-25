#ifndef LUMREPORTGEN_BASE64_VALIDATOR_HPP
#define LUMREPORTGEN_BASE64_VALIDATOR_HPP

#include "lumex/LumexExport.hpp"
#include "lumex/core/base64/Base64.hpp"

using namespace Lumex::Core::Base64::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Base64
    {
      class LUMEX_API Validator final
      {
      public:
        /**
         * @brief Checks if a given string is a valid Base64 encoded string.
         *
         * This function verifies the integrity of a Base64 string by checking its length,
         * character set, and padding. A valid Base64 string must have a length that is
         * a multiple of 4, consist only of Base64 alphabet characters and padding characters ('='),
         * and have correctly placed padding at the end if present.
         *
         * @param[in] str The string to be checked for Base64 validity.
         * @return `true` if the string adheres to Base64 formatting rules, `false` otherwise.
         */
        static bool is_valid_base64(string_type_t str);
      };
    } // namespace Base64
  } // namespace Utility
} // namespace Lumex

#endif // !LUMREPORTGEN_BASE64_VALIDATOR_HPP

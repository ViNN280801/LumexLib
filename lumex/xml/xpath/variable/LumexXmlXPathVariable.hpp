#ifndef LUMEX_XML_XPATH_VARIABLE_HPP
#define LUMEX_XML_XPATH_VARIABLE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/LumexXmlTypes.hpp"

#include "lumex/xml/xpath/node/LumexXmlXPathNodeSet.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class LUMEX_API LumexXmlXPathVariable // NOLINT(cppcoreguidelines-special-member-functions)
        {
          friend class xpath_variable_set;
          friend struct xpath_variable_boolean;
          friend struct xpath_variable_number;
          friend struct xpath_variable_string;
          friend struct xpath_variable_node_set;

        public:
          // Get variable name
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the name() method; "
                                    "discarding it negates the purpose of the getter.")
          char_t const *name() const;

          // Get variable type
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the type() method; "
                                    "discarding it negates the purpose of the getter.")
          xpath_value_type type() const;

          // Get variable value; no type conversion is performed, default value (false, NaN, empty string, empty node
          // set) is returned on type mismatch error
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_boolean() method; "
                                    "discarding it negates the purpose of the getter.")
          bool get_boolean() const;

          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_number() method; "
                                    "discarding it negates the purpose of the getter.")
          double get_number() const;

          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_string() method; "
                                    "discarding it negates the purpose of the getter.")
          char_t const *get_string() const;

          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_node_set() method; "
                                    "discarding it negates the purpose of the getter.")
          LumexXmlXPathNodeSet const &get_node_set() const;

          // Set variable value; no type conversion is performed, false is returned on type mismatch error
          bool set(bool value);

          bool set(double value);

          bool set(char_t const *value);

          bool set(LumexXmlXPathNodeSet const &value);

        private:
          Types::xpath_value_type m_type;
          LumexXmlXPathVariable *m_next;

          LumexXmlXPathVariable(xpath_value_type type);

          // Non-copyable semantics
          LumexXmlXPathVariable(LumexXmlXPathVariable const &);

          LumexXmlXPathVariable &operator=(LumexXmlXPathVariable const &);
        };

        struct xpath_variable_boolean : LumexXmlXPathVariable {
          xpath_variable_boolean() : LumexXmlXPathVariable(xpath_type_boolean) {}

          bool value{};     // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
                            // modernize-avoid-c-arrays)
        };

        struct xpath_variable_number : LumexXmlXPathVariable {
          xpath_variable_number() : LumexXmlXPathVariable(xpath_type_number) {}

          double value{};   // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        struct xpath_variable_string : LumexXmlXPathVariable { // NOLINT(cppcoreguidelines-special-member-functions)
          xpath_variable_string() : LumexXmlXPathVariable(xpath_type_string) {}

          ~xpath_variable_string()
          {
            if(value != nullptr) free(value); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
          }

          char_t *value{};  // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        struct xpath_variable_node_set : LumexXmlXPathVariable {
          xpath_variable_node_set() : LumexXmlXPathVariable(xpath_type_node_set) {}

          LumexXmlXPathNodeSet value; // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_HPP

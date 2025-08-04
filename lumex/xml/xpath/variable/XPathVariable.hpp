#ifndef LUMEX_XML_XPATH_VARIABLE_HPP
#define LUMEX_XML_XPATH_VARIABLE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class LUMEX_API XPathVariable // NOLINT(cppcoreguidelines-special-member-functions)
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
          XPathNodeSet const &get_node_set() const;

          // Set variable value; no type conversion is performed, false is returned on type mismatch error
          bool set(bool value);

          bool set(double value);

          bool set(char_t const *value);

          bool set(XPathNodeSet const &value);

          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the next() method; "
                                    "discarding it negates the purpose of the getter.")
          XPathVariable *
          next() const
          {
            return m_next;
          }

          void
          set_next(XPathVariable *next)
          {
            m_next = next;
          }

        private:
          Types::xpath_value_type m_type;
          XPathVariable *m_next;

          XPathVariable(xpath_value_type type);

          // Non-copyable semantics
          XPathVariable(XPathVariable const &);

          XPathVariable &operator=(XPathVariable const &);
        };

        struct xpath_variable_boolean : XPathVariable {
          xpath_variable_boolean() : XPathVariable(xpath_type_boolean) {}

          bool value{};     // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
                            // modernize-avoid-c-arrays)
        };

        struct xpath_variable_number : XPathVariable {
          xpath_variable_number() : XPathVariable(xpath_type_number) {}

          double value{};   // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        struct xpath_variable_string : XPathVariable { // NOLINT(cppcoreguidelines-special-member-functions)
          xpath_variable_string() : XPathVariable(xpath_type_string) {}

          ~xpath_variable_string()
          {
            if(value != nullptr) free(value); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
          }

          char_t *value{};  // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        struct xpath_variable_node_set : XPathVariable {
          xpath_variable_node_set() : XPathVariable(xpath_type_node_set) {}

          XPathNodeSet value; // NOLINT(misc-non-private-member-variables-in-classes)
          char_t name[1]{};   // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        template <typename T>
        LUMEX_API inline T *
        new_xpath_variable(char_t const *name)
        {
          size_t length = Utility::strlength(name);
          if(length == 0) return nullptr; // empty variable names are invalid

          // $$ we can't use offsetof(T, name) because T is non-POD, so we just allocate additional length characters
          void *memory         // NOLINT(cppcoreguidelines-owning-memory)
            = malloc(sizeof(T) // NOLINT(cppcoreguidelines-no-malloc)
                     + (length * sizeof(char_t)));
          if(!memory) return nullptr;

          T *result = new(memory) T(); // NOLINT(cppcoreguidelines-owning-memory)

          std::memcpy(result->name, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                      name, (length + 1) * sizeof(char_t));

          return result;
        }

        LUMEX_API
        inline XPathVariable *
        new_xpath_variable(xpath_value_type type, char_t const *name)
        {
          switch(type)
          {
          case xpath_type_node_set: return new_xpath_variable<xpath_variable_node_set>(name);
          case xpath_type_number: return new_xpath_variable<xpath_variable_number>(name);
          case xpath_type_string: return new_xpath_variable<xpath_variable_string>(name);
          case xpath_type_boolean: return new_xpath_variable<xpath_variable_boolean>(name);
          default: return nullptr;
          }
        }

        template <typename T>
        LUMEX_API inline void
        delete_xpath_variable(T *var)
        {
          var->~T();
          free(var); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
        }

        LUMEX_API
        inline void
        delete_xpath_variable(xpath_value_type type, XPathVariable *var)
        {
          switch(type)
          {
          case xpath_type_node_set:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_node_set *>(var));
            break;
          case xpath_type_number:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_number *>(var));
            break;
          case xpath_type_string:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_string *>(var));
            break;
          case xpath_type_boolean:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_boolean *>(var));
            break;

          default: LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
          }
        }

        LUMEX_API
        inline bool
        copy_xpath_variable(XPathVariable *lhs, XPathVariable const *rhs)
        {
          switch(rhs->type())
          {
          case xpath_type_node_set:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_node_set const *>(rhs)
                              ->value);
          case xpath_type_number:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_number const *>(rhs)
                              ->value);
          case xpath_type_string:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_string const *>(rhs)
                              ->value);
          case xpath_type_boolean:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_boolean const *>(rhs)
                              ->value);
          default:
            LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
            return false;
          }
        }
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_HPP

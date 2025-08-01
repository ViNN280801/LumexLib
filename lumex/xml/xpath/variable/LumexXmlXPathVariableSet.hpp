#ifndef LUMEX_XML_XPATH_VARIABLE_SET_HPP
#define LUMEX_XML_XPATH_VARIABLE_SET_HPP

#include "lumex/xml/xpath/node/LumexXmlXPathNodeSet.hpp"

#include "LumexXmlXPathVariable.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class LumexXmlXPathVariableSet // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
          // Default constructor/destructor
          LumexXmlXPathVariableSet();
          ~LumexXmlXPathVariableSet();

          // Copy constructor/assignment operator
          LumexXmlXPathVariableSet(LumexXmlXPathVariableSet const &rhs);
          LumexXmlXPathVariableSet &operator=(LumexXmlXPathVariableSet const &rhs);

          // Move semantics support
          LumexXmlXPathVariableSet(LumexXmlXPathVariableSet &&rhs) noexcept;
          LumexXmlXPathVariableSet &operator=(LumexXmlXPathVariableSet &&rhs) noexcept;

          // Add a new variable or get the existing one, if the types match
          LumexXmlXPathVariable *add(char_t const *name, xpath_value_type type);

          // Set value of an existing variable; no type conversion is performed, false is returned if there is no such
          // variable or if types mismatch
          bool set(char_t const *name, bool value);
          bool set(char_t const *name, double value);
          bool set(char_t const *name, char_t const *value);
          bool set(char_t const *name, LumexXmlXPathNodeSet const &value);

          // Get existing variable by name
          LumexXmlXPathVariable *get(char_t const *name);
          LumexXmlXPathVariable const *get(char_t const *name) const;

        private:
          constexpr static size_t _max_variables = 64;
          std::array<LumexXmlXPathVariable *, _max_variables> _data{};

          void _assign(LumexXmlXPathVariableSet const &rhs);
          void _swap(LumexXmlXPathVariableSet &rhs);

          LumexXmlXPathVariable *_find(char_t const *name) const;

          static bool _clone(LumexXmlXPathVariable *var, LumexXmlXPathVariable **out_result);
          static void _destroy(LumexXmlXPathVariable *var);
        };
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_SET_HPP

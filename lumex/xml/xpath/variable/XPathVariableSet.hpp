#ifndef LUMEX_XML_XPATH_VARIABLE_SET_HPP
#define LUMEX_XML_XPATH_VARIABLE_SET_HPP

#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

#include "XPathVariable.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class XPathVariableSet // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
          // Default constructor/destructor
          XPathVariableSet();
          ~XPathVariableSet();

          // Copy constructor/assignment operator
          XPathVariableSet(XPathVariableSet const &rhs);
          XPathVariableSet &operator=(XPathVariableSet const &rhs);

          // Move semantics support
          XPathVariableSet(XPathVariableSet &&rhs) noexcept;
          XPathVariableSet &operator=(XPathVariableSet &&rhs) noexcept;

          // Add a new variable or get the existing one, if the types match
          XPathVariable *add(char_t const *name, xpath_value_type type);

          // Set value of an existing variable; no type conversion is performed, false is returned if there is no such
          // variable or if types mismatch
          bool set(char_t const *name, bool value);
          bool set(char_t const *name, double value);
          bool set(char_t const *name, char_t const *value);
          bool set(char_t const *name, XPathNodeSet const &value);

          // Get existing variable by name
          XPathVariable *get(char_t const *name);
          XPathVariable const *get(char_t const *name) const;

        private:
          constexpr static size_t _max_variables = 64;
          std::array<XPathVariable *, _max_variables> _data{};

          void _assign(XPathVariableSet const &rhs);
          void _swap(XPathVariableSet &rhs);

          XPathVariable *_find(char_t const *name) const;

          static bool _clone(XPathVariable *var, XPathVariable **out_result);
          static void _destroy(XPathVariable *var);
        };

        inline bool
        get_variable_scratch(char_t (&buffer)[32], // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
                             XPathVariableSet *set, char_t const *begin, char_t const *end, XPathVariable **out_result)
        {
          auto length     = static_cast<size_t>(end - begin);
          char_t *scratch = buffer; // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

          if(length >= sizeof(buffer) / sizeof(buffer[0]))
          {
            // need to make dummy on-heap copy
            scratch = static_cast<char_t *>(          // NOLINT(cppcoreguidelines-owning-memory)
              malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
            if(scratch == nullptr) return false;
          }

          // copy string to zero-terminated buffer and perform lookup
          memcpy(scratch, begin, length * sizeof(char_t));
          scratch[length] = 0;

          *out_result     = set->get(scratch);

          // free dummy buffer
          if(scratch != buffer) // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            free(scratch);      // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

          return true;
        }
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_SET_HPP

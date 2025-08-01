#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathVariable.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Variable;

inline XPathVariable::XPathVariable(xpath_value_type type_) : m_type(type_), m_next(nullptr) {}

inline char_t const *
XPathVariable::name() const
{
  switch(m_type)
  {
  case xpath_type_node_set:
    return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                        // cppcoreguidelines-pro-type-static-cast-downcast)
             xpath_variable_node_set const *>(this)
      ->name;
  case xpath_type_number:
    return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                        // cppcoreguidelines-pro-type-static-cast-downcast)
             xpath_variable_number const *>(this)
      ->name;
  case xpath_type_string:
    return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                        // cppcoreguidelines-pro-type-static-cast-downcast)
             xpath_variable_string const *>(this)
      ->name;
  case xpath_type_boolean:
    return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                        // cppcoreguidelines-pro-type-static-cast-downcast)
             xpath_variable_boolean const *>(this)
      ->name;

  default:
    LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
    return nullptr;
  }
}

inline xpath_value_type
XPathVariable::type() const
{
  return m_type;
}

inline bool
XPathVariable::get_boolean() const
{
  return (m_type == xpath_type_boolean) ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                            xpath_variable_boolean const *>(this)
                                            ->value
                                        : false;
}

inline double
XPathVariable::get_number() const
{
  return (m_type == xpath_type_number) ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                           xpath_variable_number const *>(this)
                                           ->value
                                       : Utility::gen_nan();
}

inline char_t const *
XPathVariable::get_string() const
{
  char_t const *value = (m_type == xpath_type_string)
                          ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_string const *>(this)
                              ->value
                          : nullptr;
  return value != nullptr ? value : LUMEX_XML_TEXT("");
}

inline XPathNodeSet const &
XPathVariable::get_node_set() const
{
  return (m_type == xpath_type_node_set) ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                             xpath_variable_node_set const *>(this)
                                             ->value
                                         : XPath::Node::dummy_node_set;
}

inline bool
XPathVariable::set(bool value)
{
  if(m_type != xpath_type_boolean) return false;

  static_cast<xpath_variable_boolean *>(this)->value = value; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
  return true;
}

inline bool
XPathVariable::set(double value)
{
  if(m_type != xpath_type_number) return false;

  static_cast<xpath_variable_number *>(this)->value = value; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
  return true;
}

inline bool
XPathVariable::set(char_t const *value)
{
  if(m_type != xpath_type_string) return false;

  auto *var   = static_cast<xpath_variable_string *>(this); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

  size_t size = (Utility::strlength(value) + 1) * sizeof(char_t);

  auto *copy                               // NOLINT(cppcoreguidelines-owning-memory)
    = static_cast<char_t *>(malloc(size)); // NOLINT(cppcoreguidelines-no-malloc)
  if(copy == nullptr) return false;

  memcpy(copy, value, size);

  // replace old string
  if(var->value != nullptr) free(var->value); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
  var->value = copy;

  return true;
}

inline bool
XPathVariable::set(XPathNodeSet const &value)
{
  if(m_type != xpath_type_node_set) return false;

  static_cast<xpath_variable_node_set *>(this)->value // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    = value;
  return true;
}

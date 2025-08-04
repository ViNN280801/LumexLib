#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathVariableSet.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Variable;

LUMEX_PUBLIC_API
inline XPathVariableSet::XPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
    _data.at(i) = nullptr;
}

LUMEX_PUBLIC_API
inline XPathVariableSet::~XPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
    _destroy(_data.at(i));
}

LUMEX_PUBLIC_API
inline XPathVariableSet::XPathVariableSet(XPathVariableSet const &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
    _data.at(i) = nullptr;

  _assign(rhs);
}

LUMEX_PUBLIC_API
inline XPathVariableSet &
XPathVariableSet::operator=(XPathVariableSet const &rhs)
{
  if(this == &rhs) return *this;

  _assign(rhs);

  return *this;
}

LUMEX_PUBLIC_API
inline XPathVariableSet::XPathVariableSet(XPathVariableSet &&rhs) noexcept
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
  {
    _data.at(i)     = rhs._data.at(i);
    rhs._data.at(i) = nullptr;
  }
}

LUMEX_PUBLIC_API
inline XPathVariableSet &
XPathVariableSet::operator=(XPathVariableSet &&rhs) noexcept
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
  {
    _destroy(_data.at(i));

    _data.at(i)     = rhs._data.at(i);
    rhs._data.at(i) = nullptr;
  }

  return *this;
}

LUMEX_PUBLIC_API
inline void
XPathVariableSet::_assign(XPathVariableSet const &rhs)
{
  XPathVariableSet temp;

  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
    if((rhs._data.at(i) != nullptr) && !_clone(rhs._data.at(i), &temp._data.at(i))) return;

  _swap(temp);
}

LUMEX_PUBLIC_API
inline void
XPathVariableSet::_swap(XPathVariableSet &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data.at(0)); ++i) // NOLINT(bugprone-sizeof-expression)
  {
    XPathVariable *chain = _data.at(i);

    _data.at(i)          = rhs._data.at(i);
    rhs._data.at(i)      = chain;
  }
}

LUMEX_PUBLIC_API
inline XPathVariable *
XPathVariableSet::_find(char_t const *name) const
{
  size_t const hash_size = sizeof(_data) / sizeof(_data.at(0)); // NOLINT(bugprone-sizeof-expression)
  size_t hash            = Utility::hash_string(name) % hash_size;

  // look for existing variable
  for(XPathVariable *var = _data.at(hash); var != nullptr; var = var->next())
  {
    char_t const *tmp = var->name();
    if((tmp != nullptr) && Utility::strequal(tmp, name)) return var;
  }

  return nullptr;
}

LUMEX_PUBLIC_API
inline bool
XPathVariableSet::_clone(XPathVariable *var, XPathVariable **out_result)
{
  XPathVariable *last = nullptr;

  while(var != nullptr)
  {
    // allocate storage for new variable
    XPathVariable *nvar = new_xpath_variable(var->type(), var->name());
    if(nvar == nullptr) return false;

    // link the variable to the result immediately to handle failures gracefully
    if(last != nullptr)
      last->set_next(nvar);
    else
      *out_result = nvar;

    last = nvar;

    // copy the value; this can fail due to out-of-memory conditions
    if(!copy_xpath_variable(nvar, var)) return false;

    var = var->next();
  }

  return true;
}

LUMEX_PUBLIC_API
inline void
XPathVariableSet::_destroy(XPathVariable *var)
{
  while(var != nullptr)
  {
    XPathVariable *next = var->next();

    delete_xpath_variable(var->type(), var);

    var = next;
  }
}

LUMEX_PUBLIC_API
inline XPathVariable *
XPathVariableSet::add(char_t const *name, xpath_value_type type)
{
  size_t const hash_size = sizeof(_data) / sizeof(_data.at(0)); // NOLINT(bugprone-sizeof-expression)
  size_t hash            = Utility::hash_string(name) % hash_size;

  // look for existing variable
  for(XPathVariable *var = _data.at(hash); var != nullptr; var = var->next())
  {
    char_t const *tmp = var->name();
    if((tmp != nullptr) && Utility::strequal(tmp, name)) return var->type() == type ? var : nullptr;
  }

  // add new variable
  XPathVariable *result = new_xpath_variable(type, name);

  if(result != nullptr)
  {
    result->set_next(_data.at(hash));

    _data.at(hash) = result;
  }

  return result;
}

LUMEX_PUBLIC_API
inline bool
XPathVariableSet::set(char_t const *name, bool value)
{
  XPathVariable *var = add(name, xpath_type_boolean);
  return (var != nullptr) ? var->set(value) : false;
}

LUMEX_PUBLIC_API
inline bool
XPathVariableSet::set(char_t const *name, double value)
{
  XPathVariable *var = add(name, xpath_type_number);
  return (var != nullptr) ? var->set(value) : false;
}

LUMEX_PUBLIC_API
inline bool
XPathVariableSet::set(char_t const *name, char_t const *value) // NOLINT(bugprone-easily-swappable-parameters)
{
  XPathVariable *var = add(name, xpath_type_string);
  return (var != nullptr) ? var->set(value) : false;
}

LUMEX_PUBLIC_API
inline bool
XPathVariableSet::set(char_t const *name, XPathNodeSet const &value)
{
  XPathVariable *var = add(name, xpath_type_node_set);
  return (var != nullptr) ? var->set(value) : false;
}

LUMEX_PUBLIC_API
inline XPathVariable *
XPathVariableSet::get(char_t const *name)
{
  return _find(name);
}

LUMEX_PUBLIC_API
inline XPathVariable const *
XPathVariableSet::get(char_t const *name) const
{
  return _find(name);
}

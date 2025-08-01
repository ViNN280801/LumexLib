#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathVariableSet.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Variable;

inline XPathVariableSet::XPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _data[i] = nullptr;
}

inline XPathVariableSet::~XPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _destroy(_data[i]);
}

inline XPathVariableSet::XPathVariableSet(XPathVariableSet const &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _data[i] = nullptr;

  _assign(rhs);
}

inline XPathVariableSet &
XPathVariableSet::operator=(XPathVariableSet const &rhs)
{
  if(this == &rhs) return *this;

  _assign(rhs);

  return *this;
}

inline XPathVariableSet::XPathVariableSet(XPathVariableSet &&rhs) noexcept
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
  {
    _data[i]     = rhs._data[i];
    rhs._data[i] = nullptr;
  }
}

inline XPathVariableSet &
XPathVariableSet::operator=(XPathVariableSet &&rhs) noexcept
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
  {
    _destroy(_data[i]);

    _data[i]     = rhs._data[i];
    rhs._data[i] = nullptr;
  }

  return *this;
}

inline void
XPathVariableSet::_assign(XPathVariableSet const &rhs)
{
  XPathVariableSet temp;

  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
    if(rhs._data[i] && !_clone(rhs._data[i], &temp._data[i])) return;

  _swap(temp);
}

inline void
XPathVariableSet::_swap(XPathVariableSet &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
  {
    XPathVariable *chain = _data[i];

    _data[i]                     = rhs._data[i];
    rhs._data[i]                 = chain;
  }
}

inline XPathVariable *
XPathVariableSet::_find(char_t const *name) const
{
  size_t const hash_size = sizeof(_data) / sizeof(_data[0]);
  size_t hash            = Utility::hash_string(name) % hash_size;

  // look for existing variable
  for(XPathVariable *var = _data[hash]; var; var = var->_next)
  {
    char_t const *vn = var->name();
    if(vn && impl::strequal(vn, name)) return var;
  }

  return nullptr;
}

inline bool
XPathVariableSet::_clone(XPathVariable *var, XPathVariable **out_result)
{
  XPathVariable *last = nullptr;

  while(var)
  {
    // allocate storage for new variable
    XPathVariable *nvar = impl::new_xpath_variable(var->m_type, var->name());
    if(!nvar) return false;

    // link the variable to the result immediately to handle failures gracefully
    if(last)
      last->_next = nvar;
    else
      *out_result = nvar;

    last = nvar;

    // copy the value; this can fail due to out-of-memory conditions
    if(!impl::copy_xpath_variable(nvar, var)) return false;

    var = var->_next;
  }

  return true;
}

inline void
XPathVariableSet::_destroy(XPathVariable *var)
{
  while(var)
  {
    XPathVariable *next = var->_next;

    impl::delete_xpath_variable(var->m_type, var);

    var = next;
  }
}

inline XPathVariable *
XPathVariableSet::add(char_t const *name, xpath_value_type type)
{
  size_t const hash_size = sizeof(_data) / sizeof(_data[0]);
  size_t hash            = impl::hash_string(name) % hash_size;

  // look for existing variable
  for(XPathVariable *var = _data[hash]; var; var = var->_next)
  {
    char_t const *vn = var->name();
    if(vn && impl::strequal(vn, name)) return var->type() == type ? var : nullptr;
  }

  // add new variable
  XPathVariable *result = impl::new_xpath_variable(type, name);

  if(result)
  {
    result->_next = _data[hash];

    _data[hash]   = result;
  }

  return result;
}

inline bool
XPathVariableSet::set(char_t const *name, bool value)
{
  XPathVariable *var = add(name, xpath_type_boolean);
  return var ? var->set(value) : false;
}

inline bool
XPathVariableSet::set(char_t const *name, double value)
{
  XPathVariable *var = add(name, xpath_type_number);
  return var ? var->set(value) : false;
}

inline bool
XPathVariableSet::set(char_t const *name, char_t const *value)
{
  XPathVariable *var = add(name, xpath_type_string);
  return var ? var->set(value) : false;
}

inline bool
XPathVariableSet::set(char_t const *name, xpath_node_set const &value)
{
  XPathVariable *var = add(name, xpath_type_node_set);
  return var ? var->set(value) : false;
}

inline XPathVariable *
XPathVariableSet::get(char_t const *name)
{
  return _find(name);
}

inline XPathVariable const *
XPathVariableSet::get(char_t const *name) const
{
  return _find(name);
}

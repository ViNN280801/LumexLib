#include "lumex/xml/utility/LumexXmlUtils.hpp"

#include "LumexXmlXPathVariableSet.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Variable;

inline LumexXmlXPathVariableSet::LumexXmlXPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _data[i] = nullptr;
}

inline LumexXmlXPathVariableSet::~LumexXmlXPathVariableSet()
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _destroy(_data[i]);
}

inline LumexXmlXPathVariableSet::LumexXmlXPathVariableSet(LumexXmlXPathVariableSet const &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _data[i] = nullptr;

  _assign(rhs);
}

inline LumexXmlXPathVariableSet &
LumexXmlXPathVariableSet::operator=(LumexXmlXPathVariableSet const &rhs)
{
  if(this == &rhs) return *this;

  _assign(rhs);

  return *this;
}

inline LumexXmlXPathVariableSet::LumexXmlXPathVariableSet(LumexXmlXPathVariableSet &&rhs) noexcept
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
  {
    _data[i]     = rhs._data[i];
    rhs._data[i] = nullptr;
  }
}

inline LumexXmlXPathVariableSet &
LumexXmlXPathVariableSet::operator=(LumexXmlXPathVariableSet &&rhs) noexcept
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
LumexXmlXPathVariableSet::_assign(LumexXmlXPathVariableSet const &rhs)
{
  LumexXmlXPathVariableSet temp;

  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
    if(rhs._data[i] && !_clone(rhs._data[i], &temp._data[i])) return;

  _swap(temp);
}

inline void
LumexXmlXPathVariableSet::_swap(LumexXmlXPathVariableSet &rhs)
{
  for(size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
  {
    LumexXmlXPathVariable *chain = _data[i];

    _data[i]                     = rhs._data[i];
    rhs._data[i]                 = chain;
  }
}

inline LumexXmlXPathVariable *
LumexXmlXPathVariableSet::_find(char_t const *name) const
{
  size_t const hash_size = sizeof(_data) / sizeof(_data[0]);
  size_t hash            = Utility::hash_string(name) % hash_size;

  // look for existing variable
  for(LumexXmlXPathVariable *var = _data[hash]; var; var = var->_next)
  {
    char_t const *vn = var->name();
    if(vn && impl::strequal(vn, name)) return var;
  }

  return nullptr;
}

inline bool
LumexXmlXPathVariableSet::_clone(LumexXmlXPathVariable *var, LumexXmlXPathVariable **out_result)
{
  LumexXmlXPathVariable *last = nullptr;

  while(var)
  {
    // allocate storage for new variable
    LumexXmlXPathVariable *nvar = impl::new_xpath_variable(var->m_type, var->name());
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
LumexXmlXPathVariableSet::_destroy(LumexXmlXPathVariable *var)
{
  while(var)
  {
    LumexXmlXPathVariable *next = var->_next;

    impl::delete_xpath_variable(var->m_type, var);

    var = next;
  }
}

inline LumexXmlXPathVariable *
LumexXmlXPathVariableSet::add(char_t const *name, xpath_value_type type)
{
  size_t const hash_size = sizeof(_data) / sizeof(_data[0]);
  size_t hash            = impl::hash_string(name) % hash_size;

  // look for existing variable
  for(LumexXmlXPathVariable *var = _data[hash]; var; var = var->_next)
  {
    char_t const *vn = var->name();
    if(vn && impl::strequal(vn, name)) return var->type() == type ? var : nullptr;
  }

  // add new variable
  LumexXmlXPathVariable *result = impl::new_xpath_variable(type, name);

  if(result)
  {
    result->_next = _data[hash];

    _data[hash]   = result;
  }

  return result;
}

inline bool
LumexXmlXPathVariableSet::set(char_t const *name, bool value)
{
  LumexXmlXPathVariable *var = add(name, xpath_type_boolean);
  return var ? var->set(value) : false;
}

inline bool
LumexXmlXPathVariableSet::set(char_t const *name, double value)
{
  LumexXmlXPathVariable *var = add(name, xpath_type_number);
  return var ? var->set(value) : false;
}

inline bool
LumexXmlXPathVariableSet::set(char_t const *name, char_t const *value)
{
  LumexXmlXPathVariable *var = add(name, xpath_type_string);
  return var ? var->set(value) : false;
}

inline bool
LumexXmlXPathVariableSet::set(char_t const *name, xpath_node_set const &value)
{
  LumexXmlXPathVariable *var = add(name, xpath_type_node_set);
  return var ? var->set(value) : false;
}

inline LumexXmlXPathVariable *
LumexXmlXPathVariableSet::get(char_t const *name)
{
  return _find(name);
}

inline LumexXmlXPathVariable const *
LumexXmlXPathVariableSet::get(char_t const *name) const
{
  return _find(name);
}

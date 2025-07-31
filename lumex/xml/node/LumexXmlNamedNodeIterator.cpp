
inline xml_named_node_iterator::xml_named_node_iterator() : _name(nullptr) {}

inline xml_named_node_iterator::xml_named_node_iterator(xml_node const &node, char_t const *name)
    : _wrap(node), _parent(node.parent()), _name(name)
{}

inline xml_named_node_iterator::xml_named_node_iterator(xml_node_struct *ref, xml_node_struct *parent,
                                                        char_t const *name)
    : _wrap(ref), _parent(parent), _name(name)
{}

inline bool
xml_named_node_iterator::operator==(xml_named_node_iterator const &rhs) const
{
  return _wrap._root == rhs._wrap._root && _parent._root == rhs._parent._root;
}

inline bool
xml_named_node_iterator::operator!=(xml_named_node_iterator const &rhs) const
{
  return _wrap._root != rhs._wrap._root || _parent._root != rhs._parent._root;
}

inline xml_node &
xml_named_node_iterator::operator*() const
{
  LUMEX_ASSERT(_wrap._root);
  return _wrap;
}

inline xml_node *
xml_named_node_iterator::operator->() const
{
  LUMEX_ASSERT(_wrap._root);
  return &_wrap;
}

inline xml_named_node_iterator &
xml_named_node_iterator::operator++()
{
  LUMEX_ASSERT(_wrap._root);
  _wrap = _wrap.next_sibling(_name);
  return *this;
}

inline xml_named_node_iterator
xml_named_node_iterator::operator++(int)
{
  xml_named_node_iterator temp = *this;
  ++*this;
  return temp;
}

inline xml_named_node_iterator &
xml_named_node_iterator::operator--()
{
  if(_wrap._root)
    _wrap = _wrap.previous_sibling(_name);
  else
  {
    _wrap = _parent.last_child();

    if(!impl::strequal(_wrap.name(), _name)) _wrap = _wrap.previous_sibling(_name);
  }

  return *this;
}

inline xml_named_node_iterator
xml_named_node_iterator::operator--(int)
{
  xml_named_node_iterator temp = *this;
  --*this;
  return temp;
}

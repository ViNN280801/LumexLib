#ifndef LUMEX_XML_NAMED_NODE_ITERATOR_HPP
#define LUMEX_XML_NAMED_NODE_ITERATOR_HPP

class xml_named_node_iterator
{
  friend class xml_node;

public:
  // Iterator traits
  typedef ptrdiff_t difference_type;
  typedef xml_node value_type;
  typedef xml_node *pointer;
  typedef xml_node &reference;

  typedef std::bidirectional_iterator_tag iterator_category;

  // Default constructor
  xml_named_node_iterator();

  // Construct an iterator which points to the specified node
  // Note: name pointer is stored in the iterator and must have a longer lifetime than iterator itself
  xml_named_node_iterator(xml_node const &node, char_t const *name);

  // Iterator operators
  bool operator==(xml_named_node_iterator const &rhs) const;
  bool operator!=(xml_named_node_iterator const &rhs) const;

  xml_node &operator*() const;
  xml_node *operator->() const;

  xml_named_node_iterator &operator++();
  xml_named_node_iterator operator++(int);

  xml_named_node_iterator &operator--();
  xml_named_node_iterator operator--(int);

private:
  mutable xml_node _wrap;
  xml_node _parent;
  char_t const *_name;

  xml_named_node_iterator(xml_node_struct *ref, xml_node_struct *parent, char_t const *name);
};

#endif // !LUMEX_XML_NAMED_NODE_ITERATOR_HPP

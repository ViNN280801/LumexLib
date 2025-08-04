#define LUMEX_IMPLEMENTATION

#include "XPathAstNode.hpp"

using namespace Lumex::Xml::XPath::Ast;

namespace
{
  struct equal_to {
    template <typename T>
    bool
    operator()(T const &lhs, T const &rhs) const
    {
      return lhs == rhs;
    }
  };

  struct not_equal_to {
    template <typename T>
    bool
    operator()(T const &lhs, T const &rhs) const
    {
      return lhs != rhs;
    }
  };

  struct less {
    template <typename T>
    bool
    operator()(T const &lhs, T const &rhs) const
    {
      return lhs < rhs;
    }
  };

  struct less_equal {
    template <typename T>
    bool
    operator()(T const &lhs, T const &rhs) const
    {
      return lhs <= rhs;
    }
  };
  struct namespace_uri_predicate {
    char_t const *prefix{}; // NOLINT(misc-non-private-member-variables-in-classes)
    size_t prefix_length{}; // NOLINT(misc-non-private-member-variables-in-classes)

    namespace_uri_predicate(char_t const *name)
    {
      char_t const *pos = find_char(name, ':');

      prefix            = (pos != nullptr) ? name : nullptr;
      prefix_length     = (pos != nullptr) ? static_cast<size_t>(pos - name) : 0;
    }

    bool
    operator()(XmlAttribute attr) const
    {
      char_t const *name = attr.name();

      if(!starts_with(name, LUMEX_XML_TEXT("xmlns"))) return false;

      return (prefix != nullptr) ? name[5] == ':' && strequalrange(name + 6, prefix, prefix_length) : name[5] == 0;
    }
  };

  inline char_t const *
  namespace_uri(XmlNode node)
  {
    namespace_uri_predicate pred = node.name();

    XmlNode p_node               = node;

    while(p_node != nullptr)
    {
      XmlAttribute attr = p_node.find_attribute(pred);
      if(attr != nullptr) return attr.value();

      p_node = p_node.parent();
    }

    return LUMEX_XML_TEXT("");
  }

  inline char_t const *
  namespace_uri(XmlAttribute attr, XmlNode parent)
  {
    namespace_uri_predicate pred = attr.name();

    // Default namespace does not apply to attributes
    if(!pred.prefix) return LUMEX_XML_TEXT("");

    XmlNode pNode = parent;

    while(pNode)
    {
      XmlAttribute attrToFind = pNode.find_attribute(pred);
      if(attrToFind) return attrToFind.value();
      pNode = pNode.parent();
    }

    return LUMEX_XML_TEXT("");
  }

  inline char_t const *
  namespace_uri(XPathNode const &node)
  {
    return (node.attribute() != nullptr) ? namespace_uri(node.attribute(), node.parent()) : namespace_uri(node.node());
  }

  inline char_t const *
  qualified_name(XPathNode const &node)
  {
    return (node.attribute() != nullptr) ? node.attribute().name() : node.node().name();
  }

  inline char_t const *
  local_name(XPathNode const &node)
  {
    char_t const *name = qualified_name(node);
    char_t const *ptr  = find_char(name, ':');

    return (ptr != nullptr) ? ptr + 1 : name;
  }
}

LUMEX_PUBLIC_API
bool
XPathAstNode::eval_once(XPathNodeSet::type_t type, Types::nodeset_eval_t eval) // NOLINT(misc-use-anonymous-namespace)
{
  return type == XPathNodeSet::type_sorted ? eval != nodeset_eval_all : eval == nodeset_eval_any;
}

LUMEX_PUBLIC_API
void
XPathAstNode::apply_predicate_boolean(XPathNodeSetRaw &nsr, // NOLINT(misc-no-recursion)
                                      size_t first, XPathAstNode *expr, XPathStack const &stack, bool once)
{
  LUMEX_ASSERT(nsr.size() >= first);
  LUMEX_ASSERT(expr->rettype() != xpath_type_number);

  size_t idx      = 1;
  size_t size     = nsr.size() - first;

  XPathNode *last = nsr.begin() + first;

  // remove_if... or well, sort of
  for(XPathNode *it = last; it != nsr.end(); ++it, ++idx)
  {
    XPathContext ctx(*it, idx, size);

    if(expr->eval_boolean(ctx, stack))
    {
      *last++ = *it;

      if(once) break;
    }
  }

  nsr.truncate(last);
}

LUMEX_PUBLIC_API
void
XPathAstNode::apply_predicate_number(XPathNodeSetRaw &nsr, // NOLINT(misc-no-recursion)
                                     size_t first, XPathAstNode *expr, XPathStack const &stack, bool once)
{
  LUMEX_ASSERT(nsr.size() >= first);
  LUMEX_ASSERT(expr->rettype() == xpath_type_number);

  size_t idx      = 1;
  size_t size     = nsr.size() - first;

  XPathNode *last = nsr.begin() + first;

  // remove_if... or well, sort of
  for(XPathNode *it = last; it != nsr.end(); ++it, ++idx)
  {
    XPathContext ctx(*it, idx, size);

    if(expr->eval_number(ctx, stack) == static_cast<double>(idx))
    {
      *last++ = *it;

      if(once) break;
    }
  }

  nsr.truncate(last);
}

LUMEX_PUBLIC_API
void
XPathAstNode::apply_predicate_number_const(XPathNodeSetRaw &nsr, // NOLINT(misc-no-recursion)
                                           size_t first, XPathAstNode *expr, XPathStack const &stack)
{
  LUMEX_ASSERT(nsr.size() >= first);
  LUMEX_ASSERT(expr->rettype() == xpath_type_number);

  size_t size     = nsr.size() - first;

  XPathNode *last = nsr.begin() + first;

  XPathNode cn_node;
  XPathContext ctx(cn_node, 1, size);

  double er_ = expr->eval_number(ctx, stack);

  if(er_ >= 1.0 && er_ <= static_cast<double>(size))
  {
    auto eri = static_cast<size_t>(er_);

    if(er_ == static_cast<double>(eri))
    {
      XPathNode r_node = last[eri - 1];
      *last++          = r_node;
    }
  }

  nsr.truncate(last);
}

LUMEX_PUBLIC_API
void
XPathAstNode::apply_predicate(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, // NOLINT(misc-no-recursion)
                              bool once)
{
  if(nsr.size() == first) return;

  LUMEX_ASSERT(m_type == ast_filter || m_type == ast_predicate);

  if(m_test == predicate_constant || m_test == predicate_constant_one)
    apply_predicate_number_const(nsr, first, m_right, stack);
  else if(m_right->rettype() == xpath_type_number)
    apply_predicate_number(nsr, first, m_right, stack, once);
  else
    apply_predicate_boolean(nsr, first, m_right, stack, once);
}

LUMEX_PUBLIC_API
void
XPathAstNode::apply_predicates(XPathNodeSetRaw &nsr, size_t first, XPathStack const &stack, Types::nodeset_eval_t eval)
{
  if(nsr.size() == first) return;

  bool last_once = eval_once(nsr.type(), eval);

  for(XPathAstNode *pred = m_right; pred != nullptr; pred = pred->m_next)
    pred->apply_predicate(nsr, first, stack, (pred->m_next == nullptr) && last_once);
}

LUMEX_PUBLIC_API
bool
XPathAstNode::step_push(XPathNodeSetRaw &nsr, // NOLINT(readability-make-member-function-const)
                        XmlAttributeBase *attr, XmlNodeBase *parent, XPathAllocator *alloc) const
{
  LUMEX_ASSERT(attr);

  char_t const *name = (attr->name != nullptr) ? attr->name + 0 : LUMEX_XML_TEXT("");

  switch(m_test)
  {
  case nodetest_name:
    if(Utility::strequal(name, m_data.nodetest) // NOLINT(cppcoreguidelines-pro-type-union-access)
       && is_xpath_attribute(name))
    {
      nsr.push_back(XPathNode(XmlAttribute(attr), XmlNode(parent)), alloc);
      return true;
    }
    break;

  case nodetest_type_node:
  case nodetest_all:
    if(is_xpath_attribute(name))
    {
      nsr.push_back(XPathNode(XmlAttribute(attr), XmlNode(parent)), alloc);
      return true;
    }
    break;

  case nodetest_all_in_namespace:
    if(starts_with(name, m_data.nodetest) // NOLINT(cppcoreguidelines-pro-type-union-access)
       && is_xpath_attribute(name))
    {
      nsr.push_back(XPathNode(XmlAttribute(attr), XmlNode(parent)), alloc);
      return true;
    }
    break;

  default:;
  }

  return false;
}

LUMEX_PUBLIC_API
bool
XPathAstNode::step_push(XPathNodeSetRaw &nsr, XmlNodeBase *node, XPathAllocator *alloc) const
{
  LUMEX_ASSERT(node);

  auto type = LUMEX_XML_NODETYPE(node);

  switch(m_test)
  {
  case nodetest_name:
    if(type == node_element && (node->name != nullptr)
       && Utility::strequal(node->name,
                            m_data.nodetest)) // NOLINT(cppcoreguidelines-pro-type-union-access)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_type_node: nsr.push_back(XmlNode(node), alloc); return true;

  case nodetest_type_comment:
    if(type == node_comment)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_type_text:
    if(type == node_pcdata || type == node_cdata)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_type_pi:
    if(type == node_pi)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_pi:
    if(type == node_pi && (node->name != nullptr)
       && Utility::strequal(node->name,
                            m_data.nodetest)) // NOLINT(cppcoreguidelines-pro-type-union-access)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_all:
    if(type == node_element)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  case nodetest_all_in_namespace:
    if(type == node_element && (node->name != nullptr)
       && starts_with(node->name,
                      m_data.nodetest)) // NOLINT(cppcoreguidelines-pro-type-union-access)
    {
      nsr.push_back(XmlNode(node), alloc);
      return true;
    }
    break;

  default: LUMEX_ASSERT(false && "Unknown axis"); // unreachable
  }

  return false;
}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, xpath_value_type rettype_, char_t const *value)
    : m_type(static_cast<char>(type)),
      m_rettype(static_cast<char>(rettype_)),
      m_axis(0),
      m_test(0),
      m_left(nullptr),
      m_right(nullptr),
      m_next(nullptr),
      m_data{}
{
  LUMEX_ASSERT(type == ast_string_constant);
  m_data.string = value; // NOLINT(cppcoreguidelines-pro-type-union-access)
}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, xpath_value_type rettype_, double value)
    : m_type(static_cast<char>(type)),
      m_rettype(static_cast<char>(rettype_)),
      m_axis(0),
      m_test(0),
      m_left(nullptr),
      m_right(nullptr),
      m_next(nullptr),
      m_data{}
{
  LUMEX_ASSERT(type == ast_number_constant);
  m_data.number = value; // NOLINT(cppcoreguidelines-pro-type-union-access)
}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, xpath_value_type rettype_, XPathVariable *value)
    : m_type(static_cast<char>(type)),
      m_rettype(static_cast<char>(rettype_)),
      m_axis(0),
      m_test(0),
      m_left(nullptr),
      m_right(nullptr),
      m_next(nullptr),
      m_data{}
{
  LUMEX_ASSERT(type == ast_variable);
  m_data.variable = value; // NOLINT(cppcoreguidelines-pro-type-union-access)
}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, xpath_value_type rettype_,
                           XPathAstNode *left, // NOLINT(bugprone-easily-swappable-parameters)
                           XPathAstNode *right)
    : m_type(static_cast<char>(type)),
      m_rettype(static_cast<char>(rettype_)),
      m_axis(0),
      m_test(0),
      m_left(left),
      m_right(right),
      m_next(nullptr),
      m_data{}
{}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, XPathAstNode *left, axis_t axis, nodetest_t test, char_t const *contents)
    : m_type(static_cast<char>(type)),
      m_rettype(xpath_type_node_set),
      m_axis(static_cast<char>(axis)),
      m_test(static_cast<char>(test)),
      m_left(left),
      m_right(nullptr),
      m_next(nullptr),
      m_data{}
{
  LUMEX_ASSERT(type == ast_step);
  m_data.nodetest = contents; // NOLINT(cppcoreguidelines-pro-type-union-access)
}

LUMEX_PUBLIC_API
XPathAstNode::XPathAstNode(ast_type_t type, XPathAstNode *left, // NOLINT(bugprone-easily-swappable-parameters)
                           XPathAstNode *right, predicate_t test)
    : m_type(static_cast<char>(type)),
      m_rettype(xpath_type_node_set),
      m_axis(0),
      m_test(static_cast<char>(test)),
      m_left(left),
      m_right(right),
      m_next(nullptr),
      m_data{}
{
  LUMEX_ASSERT(type == ast_filter || type == ast_predicate);
}

LUMEX_PUBLIC_API
void
XPathAstNode::set_next(XPathAstNode *value)
{
  m_next = value;
}

LUMEX_PUBLIC_API
void
XPathAstNode::set_right(XPathAstNode *value)
{
  m_right = value;
}

LUMEX_PUBLIC_API
bool
XPathAstNode::eval_boolean( // NOLINT(misc-no-recursion, readability-function-cognitive-complexity)
  XPathContext const &ctx, XPathStack const &stack)
{
  switch(m_type)
  {
  case ast_op_or: return m_left->eval_boolean(ctx, stack) || m_right->eval_boolean(ctx, stack);
  case ast_op_and: return m_left->eval_boolean(ctx, stack) && m_right->eval_boolean(ctx, stack);
  case ast_op_equal: return compare_eq(m_left, m_right, ctx, stack, equal_to());
  case ast_op_not_equal: return compare_eq(m_left, m_right, ctx, stack, not_equal_to());
  case ast_op_less: return compare_rel(m_left, m_right, ctx, stack, less());
  case ast_op_greater: return compare_rel(m_right, m_left, ctx, stack, less());
  case ast_op_less_or_equal: return compare_rel(m_left, m_right, ctx, stack, std::less_equal());
  case ast_op_greater_or_equal: return compare_rel(m_right, m_left, ctx, stack, std::less_equal());

  case ast_func_starts_with: {
    XPathAllocatorCapture capture(stack.result);

    XPathString lr_str = m_left->eval_string(ctx, stack);
    XPathString rr_str = m_right->eval_string(ctx, stack);

    return starts_with(lr_str.c_str(), rr_str.c_str());
  }

  case ast_func_contains: {
    XPathAllocatorCapture capture(stack.result);

    XPathString lr_str = m_left->eval_string(ctx, stack);
    XPathString rr_str = m_right->eval_string(ctx, stack);

    return find_substring(lr_str.c_str(), rr_str.c_str()) != nullptr;
  }

  case ast_func_boolean: return m_left->eval_boolean(ctx, stack);

  case ast_func_not: return !m_left->eval_boolean(ctx, stack);

  case ast_func_true: return true;

  case ast_func_false: return false;

  case ast_func_lang: {
    if(ctx.node.attribute() != nullptr) return false;

    XPathAllocatorCapture capture(stack.result);

    XPathString lang = m_left->eval_string(ctx, stack);

    for(XmlNode node = ctx.node.node(); node != nullptr; node = node.parent())
    {
      XmlAttribute attr = node.attribute(LUMEX_XML_TEXT("xml:lang"));

      if(attr != nullptr)
      {
        char_t const *value = attr.value();

        // strnicmp / strncasecmp is not portable
        for(char_t const *lit = lang.c_str(); *lit != 0; ++lit)
        {
          if(tolower_ascii(*lit) != tolower_ascii(*value)) return false;
          ++value;
        }

        return *value == 0 || *value == '-';
      }
    }

    return false;
  }

  case ast_opt_compare_attribute: {
    char_t const *value = (m_right->m_type == ast_string_constant)
                            ? m_right->m_data.string                  // NOLINT(cppcoreguidelines-pro-type-union-access)
                            : m_right->m_data.variable->get_string(); // NOLINT(cppcoreguidelines-pro-type-union-access)

    XmlAttribute attr
      = ctx.node.node().attribute(m_left->m_data.nodetest); // NOLINT(cppcoreguidelines-pro-type-union-access)

    return (attr != nullptr) && Utility::strequal(attr.value(), value) && is_xpath_attribute(attr.name());
  }

  case ast_variable: {
    LUMEX_ASSERT(m_rettype == m_data.variable->type()); // NOLINT(cppcoreguidelines-pro-type-union-access)

    if(m_rettype == xpath_type_boolean)
      return m_data.variable->get_boolean(); // NOLINT(cppcoreguidelines-pro-type-union-access)

    // variable needs to_ be converted to_ the correct type, this is handled by the fallthrough block below
    break;
  }

  default:;
  }

  // none of the ast types that return the value directly matched, we need to_ perform type conversion
  switch(m_rettype)
  {
  case xpath_type_number: return convert_number_to_boolean(eval_number(ctx, stack));

  case xpath_type_string: {
    XPathAllocatorCapture capture(stack.result);

    return !eval_string(ctx, stack).empty();
  }

  case xpath_type_node_set: {
    XPathAllocatorCapture capture(stack.result);

    return !eval_node_set(ctx, stack, nodeset_eval_any).empty();
  }

  default:
    LUMEX_ASSERT(false && "Wrong expression for return type boolean"); // unreachable
    return false;
  }
}

LUMEX_PUBLIC_API
double
XPathAstNode::eval_number(XPathContext const &ctx, XPathStack const &stack) // NOLINT(misc-no-recursion)
{
  switch(m_type)
  {
  case ast_op_add: return m_left->eval_number(ctx, stack) + m_right->eval_number(ctx, stack);
  case ast_op_subtract: return m_left->eval_number(ctx, stack) - m_right->eval_number(ctx, stack);
  case ast_op_multiply: return m_left->eval_number(ctx, stack) * m_right->eval_number(ctx, stack);
  case ast_op_divide: return m_left->eval_number(ctx, stack) / m_right->eval_number(ctx, stack);
  case ast_op_mod: return fmod(m_left->eval_number(ctx, stack), m_right->eval_number(ctx, stack));
  case ast_op_negate: return -m_left->eval_number(ctx, stack);
  case ast_number_constant: return m_data.number; // NOLINT(cppcoreguidelines-pro-type-union-access)
  case ast_func_last: return static_cast<double>(ctx.size);
  case ast_func_position: return static_cast<double>(ctx.position);
  case ast_func_count: {
    XPathAllocatorCapture capture(stack.result);
    return static_cast<double>(m_left->eval_node_set(ctx, stack, nodeset_eval_all).size());
  }

  case ast_func_string_length_0: {
    XPathAllocatorCapture capture(stack.result);

    return static_cast<double>(string_value(ctx.node, stack.result).length());
  }

  case ast_func_string_length_1: {
    XPathAllocatorCapture capture(stack.result);

    return static_cast<double>(m_left->eval_string(ctx, stack).length());
  }

  case ast_func_number_0: {
    XPathAllocatorCapture capture(stack.result);

    return convert_string_to_number(string_value(ctx.node, stack.result).c_str());
  }

  case ast_func_number_1: return m_left->eval_number(ctx, stack);

  case ast_func_sum: {
    XPathAllocatorCapture capture(stack.result);

    double r_val        = 0;

    XPathNodeSetRaw nsr = m_left->eval_node_set(ctx, stack, nodeset_eval_all);

    for(XPathNode const *it = nsr.begin(); it != nsr.end(); ++it)
    {
      XPathAllocatorCapture cri(stack.result);

      r_val += convert_string_to_number(string_value(*it, stack.result).c_str());
    }

    return r_val;
  }

  case ast_func_floor: {
    double r_val = m_left->eval_number(ctx, stack);
    return r_val == r_val ? floor(r_val) : r_val;
  }

  case ast_func_ceiling: {
    double r_val = m_left->eval_number(ctx, stack);
    return r_val == r_val ? ceil(r_val) : r_val;
  }

  case ast_func_round: return round_nearest_nzero(m_left->eval_number(ctx, stack));

  case ast_variable: {
    LUMEX_ASSERT(m_rettype == m_data.variable->type()); // NOLINT(cppcoreguidelines-pro-type-union-access)

    if(m_rettype == xpath_type_number)
      return m_data.variable->get_number(); // NOLINT(cppcoreguidelines-pro-type-union-access)

    // variable needs to_ be converted to_ the correct type, this is handled by the fallthrough block below
    break;
  }

  default:;
  }

  // none of the ast types that return the value directly matched, we need to_ perform type conversion
  switch(m_rettype)
  {
  case xpath_type_boolean: return eval_boolean(ctx, stack) ? 1 : 0;

  case xpath_type_string:
  case xpath_type_node_set: // implicit conversion to_ string
  {
    XPathAllocatorCapture capture(stack.result);

    return convert_string_to_number(eval_string(ctx, stack).c_str());
  }

  default:
    LUMEX_ASSERT(false && "Wrong expression for return type number"); // unreachable
    return 0;
  }
}

LUMEX_PUBLIC_API
XPathString
XPathAstNode::eval_string_concat(XPathContext const &ctx, XPathStack const &stack) // NOLINT(misc-no-recursion)
{
  LUMEX_ASSERT(m_type == ast_func_concat);

  XPathAllocatorCapture capture(stack.temp);

  // count the string number
  size_t count = 1;
  for(XPathAstNode *nc_node = m_right; nc_node != nullptr; nc_node = nc_node->m_next) count++;

  // allocate a buffer for temporary string objects
  auto *buffer = static_cast<XPathString *>(stack.temp->allocate(count * sizeof(XPathString)));
  if(buffer == nullptr) return {};

  // evaluate all strings to_ temporary stack
  XPathStack swapped_stack = {stack.temp, stack.result};

  buffer[0]                = m_left->eval_string(ctx, swapped_stack);

  size_t pos               = 1;
  for(XPathAstNode *node = m_right; node != nullptr; node = node->m_next, ++pos)
    buffer[pos] = node->eval_string(ctx, swapped_stack);
  LUMEX_ASSERT(pos == count);

  // get total length
  size_t length = 0;
  for(size_t i = 0; i < count; ++i) length += buffer[i].length();

  // create final string
  auto *result = static_cast<char_t *>(stack.result->allocate((length + 1) * sizeof(char_t)));
  if(result == nullptr) return {};

  char_t *ri_chr = result;

  for(size_t j = 0; j < count; ++j)
    for(char_t const *bi = buffer[j].c_str(); *bi != 0; ++bi) *ri_chr++ = *bi;

  *ri_chr = 0;

  return XPathString::from_heap_preallocated(result, ri_chr);
}

LUMEX_PUBLIC_API
XPathString
XPathAstNode::eval_string( // NOLINT(misc-no-recursion, readability-function-cognitive-complexity)
  XPathContext const &ctx, XPathStack const &stack)
{
  switch(m_type)
  {
  case ast_string_constant:
    return XPathString::from_const(m_data.string); // NOLINT(cppcoreguidelines-pro-type-union-access)

  case ast_func_local_name_0: {
    XPathNode na_node = ctx.node;

    return XPathString::from_const(local_name(na_node));
  }

  case ast_func_local_name_1: {
    XPathAllocatorCapture capture(stack.result);

    XPathNodeSetRaw nsr = m_left->eval_node_set(ctx, stack, nodeset_eval_first);
    XPathNode na_node   = nsr.first();

    return XPathString::from_const(local_name(na_node));
  }

  case ast_func_name_0: {
    XPathNode na_node = ctx.node;

    return XPathString::from_const(qualified_name(na_node));
  }

  case ast_func_name_1: {
    XPathAllocatorCapture capture(stack.result);

    XPathNodeSetRaw nsr = m_left->eval_node_set(ctx, stack, nodeset_eval_first);
    XPathNode na_node   = nsr.first();

    return XPathString::from_const(qualified_name(na_node));
  }

  case ast_func_namespace_uri_0: {
    XPathNode na_node = ctx.node;

    return XPathString::from_const(namespace_uri(na_node));
  }

  case ast_func_namespace_uri_1: {
    XPathAllocatorCapture capture(stack.result);

    XPathNodeSetRaw nsr = m_left->eval_node_set(ctx, stack, nodeset_eval_first);
    XPathNode na_node   = nsr.first();

    return XPathString::from_const(namespace_uri(na_node));
  }

  case ast_func_string_0: return string_value(ctx.node, stack.result);

  case ast_func_string_1: return m_left->eval_string(ctx, stack);

  case ast_func_concat: return eval_string_concat(ctx, stack);

  case ast_func_substring_before: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathString s_str        = m_left->eval_string(ctx, swapped_stack);
    XPathString p_str        = m_right->eval_string(ctx, swapped_stack);

    char_t const *pos        = find_substring(s_str.c_str(), p_str.c_str());

    return (pos != nullptr) ? XPathString::from_heap(s_str.c_str(), pos, stack.result) : XPathString();
  }

  case ast_func_substring_after: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathString s_str        = m_left->eval_string(ctx, swapped_stack);
    XPathString p_str        = m_right->eval_string(ctx, swapped_stack);

    char_t const *pos        = find_substring(s_str.c_str(), p_str.c_str());
    if(pos == nullptr) return {};

    char_t const *rbegin = pos + p_str.length();
    char_t const *rend   = s_str.c_str() + s_str.length();

    return s_str.uses_heap() ? XPathString::from_heap(rbegin, rend, stack.result) : XPathString::from_const(rbegin);
  }

  case ast_func_substring_2: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathString s_str        = m_left->eval_string(ctx, swapped_stack);
    size_t s_length          = s_str.length();

    double first             = round_nearest(m_right->eval_number(ctx, stack));

    if(is_nan(first)) return {}; // NaN // NOLINT(bugprone-branch-clone)
    if(first >= static_cast<double>(s_length + 1)) return {};

    size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
    LUMEX_ASSERT(1 <= pos && pos <= s_length + 1);

    char_t const *rbegin = s_str.c_str() + (pos - 1);
    char_t const *rend   = s_str.c_str() + s_str.length();

    return s_str.uses_heap() ? XPathString::from_heap(rbegin, rend, stack.result) : XPathString::from_const(rbegin);
  }

  case ast_func_substring_3: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathString s_str        = m_left->eval_string(ctx, swapped_stack);
    size_t s_length          = s_str.length();

    double first             = round_nearest(m_right->eval_number(ctx, stack));
    double last              = first + round_nearest(m_right->m_next->eval_number(ctx, stack));

    if(is_nan(first) || is_nan(last)) return {};
    if(first >= static_cast<double>(s_length + 1)) return {};
    if(first >= last) return {};
    if(last < 1) return {};

    size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
    size_t end = last >= static_cast<double>(s_length + 1) ? s_length + 1 : static_cast<size_t>(last);

    LUMEX_ASSERT(1 <= pos && pos <= end && end <= s_length + 1);
    char_t const *rbegin = s_str.c_str() + (pos - 1);
    char_t const *rend   = s_str.c_str() + (end - 1);

    return (end == s_length + 1 && !s_str.uses_heap()) ? XPathString::from_const(rbegin)
                                                       : XPathString::from_heap(rbegin, rend, stack.result);
  }

  case ast_func_normalize_space_0: {
    XPathString s_str = string_value(ctx.node, stack.result);

    char_t *begin     = s_str.data(stack.result);
    if(begin == nullptr) return {};

    char_t *end = normalize_space(begin);

    return XPathString::from_heap_preallocated(begin, end);
  }

  case ast_func_normalize_space_1: {
    XPathString s_str = m_left->eval_string(ctx, stack);

    char_t *begin     = s_str.data(stack.result);
    if(begin == nullptr) return {};

    char_t *end = normalize_space(begin);

    return XPathString::from_heap_preallocated(begin, end);
  }

  case ast_func_translate: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathString s_str        = m_left->eval_string(ctx, stack);
    XPathString from         = m_right->eval_string(ctx, swapped_stack);
    XPathString to_          = m_right->m_next->eval_string(ctx, swapped_stack);

    char_t *begin            = s_str.data(stack.result);
    if(begin == nullptr) return {};

    char_t *end = translate(begin, from.c_str(), to_.c_str(), to_.length());

    return XPathString::from_heap_preallocated(begin, end);
  }

  case ast_opt_translate_table: {
    XPathString s_str = m_left->eval_string(ctx, stack);

    char_t *begin     = s_str.data(stack.result);
    if(begin == nullptr) return {};

    char_t *end = translate_table(begin, m_data.table); // NOLINT(cppcoreguidelines-pro-type-union-access)

    return XPathString::from_heap_preallocated(begin, end);
  }

  case ast_variable: {
    LUMEX_ASSERT(m_rettype == m_data.variable->type()); // NOLINT(cppcoreguidelines-pro-type-union-access)

    if(m_rettype == xpath_type_string)
      return XPathString::from_const(m_data.variable->get_string()); // NOLINT(cppcoreguidelines-pro-type-union-access)

    // variable needs to_ be converted to_ the correct type, this is handled by the fallthrough block below
    break;
  }

  default:;
  }

  // none of the ast types that return the value directly matched, we need to_ perform type conversion
  switch(m_rettype)
  {
  case xpath_type_boolean:
    return XPathString::from_const(eval_boolean(ctx, stack) ? LUMEX_XML_TEXT("true") : LUMEX_XML_TEXT("false"));

  case xpath_type_number: return convert_number_to_string(eval_number(ctx, stack), stack.result);

  case xpath_type_node_set: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathNodeSetRaw nsr      = eval_node_set(ctx, swapped_stack, nodeset_eval_first);
    return nsr.empty() ? XPathString() : string_value(nsr.first(), stack.result);
  }

  default:
    LUMEX_ASSERT(false && "Wrong expression for return type string"); // unreachable
    return {};
  }
}

LUMEX_PUBLIC_API
XPathNodeSetRaw
XPathAstNode::eval_node_set( // NOLINT(misc-no-recursion)
  XPathContext const &ctx, XPathStack const &stack, Types::nodeset_eval_t eval)
{
  switch(m_type)
  {
  case ast_op_union: {
    XPathAllocatorCapture capture(stack.temp);

    XPathStack swapped_stack = {stack.temp, stack.result};

    XPathNodeSetRaw ls_      = m_left->eval_node_set(ctx, stack, eval);
    XPathNodeSetRaw rs_      = m_right->eval_node_set(ctx, swapped_stack, eval);

    // we can optimize merging two sorted sets, but this is a very rare operation, so don't bother
    ls_.set_type(XPathNodeSet::type_unsorted);

    ls_.append(rs_.begin(), rs_.end(), stack.result);
    ls_.remove_duplicates(stack.temp);

    return ls_;
  }

  case ast_filter: {
    XPathNodeSetRaw set
      = m_left->eval_node_set(ctx, stack, m_test == predicate_constant_one ? nodeset_eval_first : nodeset_eval_all);

    // either expression is a number or it contains position() call; sort by document order
    if(m_test != predicate_posinv) set.sort_do();

    bool once = eval_once(set.type(), eval);

    apply_predicate(set, 0, stack, once);

    return set;
  }

  case ast_func_id: return {};

  case ast_step: {
    switch(m_axis)
    {
    case axis_ancestor: return step_do(ctx, stack, eval, axis_to_type<axis_ancestor>());
    case axis_ancestor_or_self: return step_do(ctx, stack, eval, axis_to_type<axis_ancestor_or_self>());
    case axis_attribute: return step_do(ctx, stack, eval, axis_to_type<axis_attribute>());
    case axis_child: return step_do(ctx, stack, eval, axis_to_type<axis_child>());
    case axis_descendant: return step_do(ctx, stack, eval, axis_to_type<axis_descendant>());
    case axis_descendant_or_self: return step_do(ctx, stack, eval, axis_to_type<axis_descendant_or_self>());
    case axis_following: return step_do(ctx, stack, eval, axis_to_type<axis_following>());
    case axis_following_sibling: return step_do(ctx, stack, eval, axis_to_type<axis_following_sibling>());
    case axis_namespace:
      // namespaced axis is not supported
      return {};

    case axis_parent: return step_do(ctx, stack, eval, axis_to_type<axis_parent>());
    case axis_preceding: return step_do(ctx, stack, eval, axis_to_type<axis_preceding>());
    case axis_preceding_sibling: return step_do(ctx, stack, eval, axis_to_type<axis_preceding_sibling>());
    case axis_self: return step_do(ctx, stack, eval, axis_to_type<axis_self>());

    default:
      LUMEX_ASSERT(false && "Unknown axis"); // unreachable
      return {};
    }
  }

  case ast_step_root: {
    LUMEX_ASSERT(!m_right); // root step can't have any predicates

    XPathNodeSetRaw nsr;

    nsr.set_type(XPathNodeSet::type_sorted);

    if(ctx.node.node() != nullptr)
      nsr.push_back(ctx.node.node().root(), stack.result);
    else if(ctx.node.attribute() != nullptr)
      nsr.push_back(ctx.node.parent().root(), stack.result);

    return nsr;
  }

  case ast_variable: {
    LUMEX_ASSERT(m_rettype == m_data.variable->type()); // NOLINT(cppcoreguidelines-pro-type-union-access)

    if(m_rettype == xpath_type_node_set)
    {
      XPathNodeSet const &s_set = m_data.variable->get_node_set(); // NOLINT(cppcoreguidelines-pro-type-union-access)

      XPathNodeSetRaw nsr;

      nsr.set_type(s_set.type());
      nsr.append(s_set.begin(), s_set.end(), stack.result);

      return nsr;
    }

    // variable needs to_ be converted to_ the correct type, this is handled by the fallthrough block below
    break;
  }

  default:;
  }

  // none of the ast types that return the value directly matched, but conversions to_ node set are invalid
  LUMEX_ASSERT(false && "Wrong expression for return type node set"); // unreachable
  return {};
}

LUMEX_PUBLIC_API
void
XPathAstNode::optimize(XPathAllocator *alloc) // NOLINT(misc-no-recursion)
{
  if(m_left != nullptr) m_left->optimize(alloc);
  if(m_right != nullptr) m_right->optimize(alloc);
  if(m_next != nullptr) m_next->optimize(alloc);

  // coverity[var_deref_model]
  optimize_self(alloc);
}

LUMEX_PUBLIC_API
void
XPathAstNode::optimize_self(XPathAllocator *alloc) // NOLINT(readability-function-cognitive-complexity)
{
  // Rewrite [position()=expr] with [expr]
  // Note that this step has to_ go before classification to_ recognize [position()=1]
  if((m_type == ast_filter || m_type == ast_predicate) && (m_right != nullptr)
     && // workaround for clang static analyzer (m_right is never null for ast_filter/ast_predicate)
     m_right->m_type == ast_op_equal && m_right->m_left->m_type == ast_func_position
     && m_right->m_right->m_rettype == xpath_type_number)
  {
    m_right = m_right->m_right;
  }

  // Classify filter/predicate ops to_ perform various optimizations during evaluation
  if((m_type == ast_filter || m_type == ast_predicate)
     && (m_right
         != nullptr)) // workaround for clang static analyzer (m_right is never null for ast_filter/ast_predicate)
  {
    LUMEX_ASSERT(m_test == predicate_default);

    if(m_right->m_type == ast_number_constant
       && m_right->m_data.number == 1.0) // NOLINT(cppcoreguidelines-pro-type-union-access)
      m_test = predicate_constant_one;
    else if(m_right->m_rettype == xpath_type_number
            && (m_right->m_type == ast_number_constant || m_right->m_type == ast_variable
                || m_right->m_type == ast_func_last))
      m_test = predicate_constant;
    else if(m_right->m_rettype != xpath_type_number && m_right->is_posinv_expr())
      m_test = predicate_posinv;
  }

  // Rewrite descendant-or-self::node()/child::foo with descendant::foo
  // The former is a full form of //foo, the latter is much faster since it executes the node test immediately
  // Do a similar kind of rewrite for self/descendant/descendant-or-self axes
  // Note that we only rewrite positionally invariant steps (//foo[1] != /descendant::foo[1])
  if(m_type == ast_step
     && (m_axis == axis_child || m_axis == axis_self || m_axis == axis_descendant || m_axis == axis_descendant_or_self)
     && (m_left != nullptr) && m_left->m_type == ast_step && m_left->m_axis == axis_descendant_or_self
     && m_left->m_test == nodetest_type_node && (m_left->m_right == nullptr) && is_posinv_step())
  {
    if(m_axis == axis_child || m_axis == axis_descendant)
      m_axis = axis_descendant;
    else
      m_axis = axis_descendant_or_self;

    m_left = m_left->m_left;
  }

  // Use optimized lookup table implementation for translate() with constant arguments
  if(m_type == ast_func_translate && (m_right != nullptr)
     && // workaround for clang static analyzer (m_right is never null for ast_func_translate)
     m_right->m_type == ast_string_constant && m_right->m_next->m_type == ast_string_constant)
  {
    unsigned char *table
      = translate_table_generate(alloc,
                                 m_right->m_data.string,          // NOLINT(cppcoreguidelines-pro-type-union-access)
                                 m_right->m_next->m_data.string); // NOLINT(cppcoreguidelines-pro-type-union-access)

    if(table != nullptr)
    {
      m_type       = ast_opt_translate_table;
      m_data.table = table; // NOLINT(cppcoreguidelines-pro-type-union-access)
    }
  }

  // Use optimized path for @attr = 'value' or @attr = $value
  if(m_type == ast_op_equal && (m_left != nullptr) && (m_right != nullptr)
     && // workaround for clang static analyzer and Coverity (m_left and
        // m_right are never null for ast_op_equal) coverity[mixed_enums]
     m_left->m_type == ast_step && m_left->m_axis == axis_attribute && m_left->m_test == nodetest_name
     && (m_left->m_left == nullptr) && (m_left->m_right == nullptr)
     && (m_right->m_type == ast_string_constant
         || (m_right->m_type == ast_variable && m_right->m_rettype == xpath_type_string)))
  {
    m_type = ast_opt_compare_attribute;
  }
}

LUMEX_PUBLIC_API
bool
XPathAstNode::is_posinv_expr() const // NOLINT(misc-no-recursion)
{
  switch(m_type)
  {
  case ast_func_position:
  case ast_func_last: return false;

  case ast_string_constant: // NOLINT(bugprone-branch-clone)
  case ast_number_constant:
  case ast_variable: return true;

  case ast_step:
  case ast_step_root: return true;

  case ast_predicate:
  case ast_filter: return true;

  default:
    if((m_left != nullptr) && !m_left->is_posinv_expr()) return false;

    for(XPathAstNode *node = m_right; node != nullptr; node = node->m_next)
      if(!node->is_posinv_expr()) return false;

    return true;
  }
}

LUMEX_PUBLIC_API
bool
XPathAstNode::is_posinv_step() const
{
  LUMEX_ASSERT(m_type == ast_step);

  for(XPathAstNode *node = m_right; node != nullptr; node = node->m_next)
  {
    LUMEX_ASSERT(node->m_type == ast_predicate);

    if(node->m_test != predicate_posinv) return false;
  }

  return true;
}

LUMEX_PUBLIC_API
xpath_value_type
XPathAstNode::rettype() const
{
  return static_cast<xpath_value_type>(m_rettype);
}

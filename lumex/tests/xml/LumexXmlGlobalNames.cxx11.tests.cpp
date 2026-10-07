// lumex/tests/xml/LumexXmlGlobalNames.cxx11.tests.cpp
//
// Including the XML and base64 umbrellas adds no name to the global
// namespace. This file declares global names spelled like names of the two
// modules and uses them unqualified. A header that writes `using namespace
// lumex::...;` (or `using lumex::...::name;`) at file scope makes every such
// use ambiguous, and this file stops compiling. Each probe below names the
// namespace it guards; the run-time checks only confirm that the global name
// is this file's own. The probes stand in for a consumer's own declarations,
// so they carry the library's spellings rather than its naming rules.

#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/xml/LumexXml"

/// @brief Base of every global type probe of this file.
struct own_name_t
{
};

/// @brief Type of every global object probe of this file.
struct own_value_t
{
};

// lumex::xml::types::Types (also the old global `using ...::char_t;`).
struct char_t : own_name_t
{
};
struct string_t : own_name_t
{
};
struct xml_node_type : own_name_t
{
};
own_value_t const node_element{};
own_value_t const status_ok{};

// lumex::xml::constants::Constants and
// lumex::xml::xpath::constants::Constants.
own_value_t const kparse_default{};
own_value_t const kxpath_memory_page_size{};

// lumex::xml::memory, attribute, node, text, writer, tree.
struct XmlAllocator : own_name_t
{
};
struct XmlAttribute : own_name_t
{
};
struct XmlNode : own_name_t
{
};
struct XmlText : own_name_t
{
};
struct xml_parse_result_t : own_name_t
{
};
struct XmlBufferedWriter : own_name_t
{
};
struct XmlTreeWalker : own_name_t
{
};

// lumex::xml::xpath::memory, node, variable, query, parser.
struct XPathAllocator : own_name_t
{
};
struct XPathNode : own_name_t
{
};
struct XPathNodeSet : own_name_t
{
};
struct XPathVariableSet : own_name_t
{
};
struct XPathQuery : own_name_t
{
};
struct xpath_parse_result_t : own_name_t
{
};

// The namespaces of lumex::xml itself.
namespace document
{
struct probe_t : own_name_t
{
};
}
namespace xpath
{
struct probe_t : own_name_t
{
};
}

// lumex::core::base64::codec::Types.
struct byte_type : own_name_t
{
};
struct string_type_t : own_name_t
{
};

namespace
{
template <typename T>
bool
is_own_name ()
{
  return std::is_base_of<own_name_t, T>::value;
}

template <typename T>
bool
is_own_value (T const & /* value */)
{
  return std::is_same<T, own_value_t>::value;
}

TEST (XmlGlobalNames, GivenUmbrellasIncluded_WhenXmlTypeNamesUsed_ThenOwnTypes)
{
  EXPECT_TRUE (is_own_name<char_t> ());
  EXPECT_TRUE (is_own_name<string_t> ());
  EXPECT_TRUE (is_own_name<xml_node_type> ());
  EXPECT_TRUE (is_own_value (node_element));
  EXPECT_TRUE (is_own_value (status_ok));
}

TEST (XmlGlobalNames,
      GivenUmbrellasIncluded_WhenConstantNamesUsed_ThenOwnValues)
{
  EXPECT_TRUE (is_own_value (kparse_default));
  EXPECT_TRUE (is_own_value (kxpath_memory_page_size));
}

TEST (XmlGlobalNames,
      GivenUmbrellasIncluded_WhenDomClassNamesUsed_ThenOwnTypes)
{
  EXPECT_TRUE (is_own_name<XmlAllocator> ());
  EXPECT_TRUE (is_own_name<XmlAttribute> ());
  EXPECT_TRUE (is_own_name<XmlNode> ());
  EXPECT_TRUE (is_own_name<XmlText> ());
  EXPECT_TRUE (is_own_name<xml_parse_result_t> ());
  EXPECT_TRUE (is_own_name<XmlBufferedWriter> ());
  EXPECT_TRUE (is_own_name<XmlTreeWalker> ());
}

TEST (XmlGlobalNames, GivenUmbrellasIncluded_WhenXPathNamesUsed_ThenOwnTypes)
{
  EXPECT_TRUE (is_own_name<XPathAllocator> ());
  EXPECT_TRUE (is_own_name<XPathNode> ());
  EXPECT_TRUE (is_own_name<XPathNodeSet> ());
  EXPECT_TRUE (is_own_name<XPathVariableSet> ());
  EXPECT_TRUE (is_own_name<XPathQuery> ());
  EXPECT_TRUE (is_own_name<xpath_parse_result_t> ());
}

TEST (XmlGlobalNames,
      GivenUmbrellasIncluded_WhenNamespaceNamesUsed_ThenOwnNamespaces)
{
  EXPECT_TRUE (is_own_name<document::probe_t> ());
  EXPECT_TRUE (is_own_name<xpath::probe_t> ());
}

TEST (XmlGlobalNames,
      GivenUmbrellasIncluded_WhenBase64TypeNamesUsed_ThenOwnTypes)
{
  EXPECT_TRUE (is_own_name<byte_type> ());
  EXPECT_TRUE (is_own_name<string_type_t> ());
}

TEST (XmlGlobalNames,
      GivenUmbrellasIncluded_WhenLibraryNamesQualified_ThenLibraryEntities)
{
  EXPECT_TRUE (
      (std::is_same<lumex::xml::types::Types::char_t, LUMEX_XML_CHAR>::value));
  EXPECT_FALSE (is_own_name<lumex::xml::node::XmlNode> ());
  EXPECT_TRUE (lumex::xml::node::XmlNode ().empty ());
  EXPECT_TRUE (lumex::xml::attribute::XmlAttribute ().empty ());
  EXPECT_TRUE ((std::is_same<lumex::core::base64::codec::Types::byte_type,
                             unsigned char>::value));
}
} // namespace

// Consumer of LumexCMake.consumer_standard_mismatch_*: compiled at
// CONSUMER_STD against Lumex libraries compiled at LIB_STD. Each call uses the
// overload of the consumer's own standard: std::string_view / std::span from
// C++17 / C++20, std::string const & and the pointer and size functions
// below. Returns the number of failed checks.
#include <cstdio>
#include <string>
#include <vector>
#if __cplusplus >= 201703L
#include <string_view>
#endif
#if __cplusplus >= 202002L
#include <span>
#endif

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/exceptions/LumexException"
#include "lumex/xml/LumexXml"

namespace
{
int
check (bool ok, char const *what)
{
  if (!ok)
    std::fprintf (stderr, "standard_mismatch: %s failed\n", what);
  return ok ? 0 : 1;
}

int
check_base64 ()
{
  using lumex::core::base64::codec::Types::byte_type;
  using lumex::core::base64::decode::Decoder;
  using lumex::core::base64::encode::Encoder;
  using lumex::core::base64::validate::Validator;

  int failures = 0;
  std::string const encoded = "SGVsbG8=";
  std::vector<byte_type> bytes;
  failures += check (Decoder::decode (encoded, bytes),
                     "Decoder::decode (text, out)");
  failures += check (std::string (bytes.begin (), bytes.end ()) == "Hello",
                     "decoded bytes");
  failures += check (Decoder::decode (encoded).size () == 5u,
                     "Decoder::decode (text)");
  failures += check (Validator::is_valid_base64 (encoded),
                     "Validator::is_valid_base64 (text)");
  failures += check (!Validator::is_valid_base64 (nullptr, 0),
                     "Validator::is_valid_base64 (nullptr, 0)");
  failures += check (Encoder::encode ("Hello", 5) == encoded,
                     "Encoder::encode (pointer, size)");
  failures += check (Encoder::encode (bytes) == encoded,
                     "Encoder::encode (vector)");
#if __cplusplus >= 201703L
  failures += check (Encoder::encode (std::string_view ("Hello")) == encoded,
                     "Encoder::encode (string_view)");
#endif
#if __cplusplus >= 202002L
  failures += check (Encoder::encode (std::span<byte_type const> (bytes))
                         == encoded,
                     "Encoder::encode (span)");
#endif
  return failures;
}

int
check_xml ()
{
  using lumex::xml::attribute::XmlAttribute;
  using lumex::xml::document::XmlDocument;
  using lumex::xml::node::XmlNode;

  int failures = 0;
  XmlDocument doc;
  XmlNode root = doc.append_child ("root");
  // "item" and "key" are ranges of one buffer without a NUL between them.
  char const names[] = "itemkey";
  XmlNode item = root.append_child (names, 4);
  XmlAttribute key = item.append_attribute (names + 4, 3);
  failures += check (key.set_value ("v"), "XmlAttribute::set_value");
  failures += check (root.child (names, 4) == item,
                     "XmlNode::child (pointer, size)");
  failures += check (!item.attribute (names + 4, 3).empty (),
                     "XmlNode::attribute (pointer, size)");
#if __cplusplus >= 201703L
  std::string_view const view = names;
  failures += check (root.child (view.substr (0, 4)) == item,
                     "XmlNode::child (string_view)");
  failures += check (!item.attribute (view.substr (4, 3)).empty (),
                     "XmlNode::attribute (string_view)");
  failures += check (key.set_value (std::string_view ("value;").substr (0, 5)),
                     "XmlAttribute::set_value (string_view)");
  failures += check (std::string (key.value ()) == "value", "attribute value");
  failures += check (root.remove_child (view.substr (0, 4)),
                     "XmlNode::remove_child (string_view)");
#else
  failures += check (root.remove_child (names, 4),
                     "XmlNode::remove_child (pointer, size)");
#endif
  failures += check (root.child ("item").empty (), "child removed");
  return failures;
}

int
check_exceptions ()
{
  using lumex::core::exceptions::exception::LumexBaseException;

  int failures = 0;
#if __cplusplus >= 201703L
  LumexBaseException const ex (std::string_view ("boom!").substr (0, 4));
#else
  LumexBaseException const ex (std::string ("boom"));
#endif
  failures += check (std::string (ex.what ()) == "boom",
                     "LumexBaseException (text)");
  return failures;
}
} // namespace

int
main ()
{
  int const failures = check_base64 () + check_xml () + check_exceptions ();
  std::printf ("standard_mismatch: C++%ld consumer, %d failed checks\n",
               static_cast<long> (__cplusplus), failures);
  return failures == 0 ? 0 : 1;
}

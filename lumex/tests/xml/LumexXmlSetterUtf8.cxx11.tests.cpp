// lumex/tests/xml/LumexXmlSetterUtf8.cxx11.tests.cpp
//
// The setters that copy a string of the caller into the tree as a name or a
// value drop the bytes that start no well-formed UTF-8 sequence (Unicode
// Table 3-7), as a load of a UTF-8 document does, and return true: set_name
// and set_value of nodes and attributes (with and without a size, and with a
// string_view_t), XmlText::set, the assignments of attributes and text, and
// every function that creates a child or an attribute from a name. Valid text
// is stored byte for byte, a real U+FFFD included. The numeric setters are not
// touched. The expected text of every case comes from a reference written from
// the table, not from the decoder of the unicode module.
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/xml/LumexXml"

#include "lumex/tests/xml/LumexXmlTestFixtures.hpp"

using lumex::xml::attribute::XmlAttribute;
using lumex::xml::constants::Constants::kformat_no_declaration;
using lumex::xml::constants::Constants::kformat_raw;
using lumex::xml::constants::Constants::kparse_default;
using lumex::xml::constants::Constants::kparse_keep_invalid_utf8;
using lumex::xml::document::XmlDocument;
using lumex::xml::node::XmlNode;
using lumex::xml::text::XmlText;
using lumex::xml::types::Types::node_element;
using lumex::xml::types::Types::node_pcdata;
using lumex::xml::types::Types::string_view_t;
using lumex::xml::types::Types::xml_encoding;

namespace
{
unsigned char
byte_at (std::string const &s, std::size_t i)
{
  return static_cast<unsigned char> (s[i]);
}

bool
byte_in (unsigned char b, unsigned int low, unsigned int high)
{
  return b >= low && b <= high;
}

// Table 3-7: the length of the well-formed sequence at s[i], or 0.
std::size_t
table_sequence_length (std::string const &s, std::size_t i)
{
  std::size_t const left = s.size () - i;
  unsigned char const b0 = byte_at (s, i);
  if (b0 <= 0x7F)
    return 1;
  if (byte_in (b0, 0xC2, 0xDF))
    return (left >= 2 && byte_in (byte_at (s, i + 1), 0x80, 0xBF)) ? 2 : 0;
  if (byte_in (b0, 0xE0, 0xEF))
    {
      unsigned int low = (b0 == 0xE0) ? 0xA0 : 0x80;
      unsigned int high = (b0 == 0xED) ? 0x9F : 0xBF;
      return (left >= 3 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF))
                 ? 3
                 : 0;
    }
  if (byte_in (b0, 0xF0, 0xF4))
    {
      unsigned int low = (b0 == 0xF0) ? 0x90 : 0x80;
      unsigned int high = (b0 == 0xF4) ? 0x8F : 0xBF;
      return (left >= 4 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF)
              && byte_in (byte_at (s, i + 3), 0x80, 0xBF))
                 ? 4
                 : 0;
    }
  return 0;
}

std::string
table_clean (std::string const &s)
{
  std::string out;
  std::size_t i = 0;
  while (i < s.size ())
    {
      std::size_t const length = table_sequence_length (s, i);
      if (length == 0)
        ++i;
      else
        {
          out.append (s, i, length);
          i += length;
        }
    }
  return out;
}

// What a setter did: the value it returned and the text now in the tree.
struct outcome_t
{
  bool ok;
  std::string stored;
};

using apply_t = outcome_t (*) (XmlDocument &doc, std::string const &text);

struct setter_t
{
  char const *name;
  apply_t apply;
};

std::string
text_of (char const *s)
{
  return s != nullptr ? std::string (s) : std::string ();
}

// Every setter that copies a name or a value of the caller. The text has no
// NUL: the setters with a size are given its exact size.
std::vector<setter_t>
setters ()
{
  std::vector<setter_t> list;

  // ---- nodes
  list.push_back ({ "XmlNode::set_name",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child ("e");
                        bool const ok = n.set_name (s.c_str ());
                        return outcome_t{ ok, text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::set_name (size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child ("e");
                        bool const ok = n.set_name (s.data (), s.size ());
                        return outcome_t{ ok, text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::set_name (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child ("e");
                        bool const ok = n.set_name (
                            string_view_t (s.data (), s.size ()));
                        return outcome_t{ ok, text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::set_value",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child (node_pcdata);
                        bool const ok = n.set_value (s.c_str ());
                        return outcome_t{ ok, text_of (n.value ()) };
                      } });
  list.push_back ({ "XmlNode::set_value (size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child (node_pcdata);
                        bool const ok = n.set_value (s.data (), s.size ());
                        return outcome_t{ ok, text_of (n.value ()) };
                      } });
  list.push_back ({ "XmlNode::set_value (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child (node_pcdata);
                        bool const ok = n.set_value (
                            string_view_t (s.data (), s.size ()));
                        return outcome_t{ ok, text_of (n.value ()) };
                      } });

  // ---- attributes
  list.push_back (
      { "XmlAttribute::set_name", [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a = d.append_child ("r").append_attribute ("k");
            bool const ok = a.set_name (s.c_str ());
            return outcome_t{ ok, text_of (a.name ()) };
          } });
  list.push_back ({ "XmlAttribute::set_name (size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlAttribute a
                            = d.append_child ("r").append_attribute ("k");
                        bool const ok = a.set_name (s.data (), s.size ());
                        return outcome_t{ ok, text_of (a.name ()) };
                      } });
  list.push_back (
      { "XmlAttribute::set_name (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a = d.append_child ("r").append_attribute ("k");
            bool const ok = a.set_name (string_view_t (s.data (), s.size ()));
            return outcome_t{ ok, text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlAttribute::set_value", [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a = d.append_child ("r").append_attribute ("k");
            bool const ok = a.set_value (s.c_str ());
            return outcome_t{ ok, text_of (a.value ()) };
          } });
  list.push_back ({ "XmlAttribute::set_value (size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlAttribute a
                            = d.append_child ("r").append_attribute ("k");
                        bool const ok = a.set_value (s.data (), s.size ());
                        return outcome_t{ ok, text_of (a.value ()) };
                      } });
  list.push_back (
      { "XmlAttribute::set_value (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a = d.append_child ("r").append_attribute ("k");
            bool const ok = a.set_value (string_view_t (s.data (), s.size ()));
            return outcome_t{ ok, text_of (a.value ()) };
          } });
  list.push_back ({ "XmlAttribute::operator= (char_t const *)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlAttribute a
                            = d.append_child ("r").append_attribute ("k");
                        a = s.c_str ();
                        return outcome_t{ true, text_of (a.value ()) };
                      } });
  list.push_back ({ "XmlAttribute::operator= (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlAttribute a
                            = d.append_child ("r").append_attribute ("k");
                        a = string_view_t (s.data (), s.size ());
                        return outcome_t{ true, text_of (a.value ()) };
                      } });

  // ---- text
  list.push_back ({ "XmlText::set", [] (XmlDocument &d, std::string const &s)
                      {
                        XmlText t = d.append_child ("r").text ();
                        bool const ok = t.set (s.c_str ());
                        return outcome_t{ ok, text_of (t.get ()) };
                      } });
  list.push_back ({ "XmlText::set (size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlText t = d.append_child ("r").text ();
                        bool const ok = t.set (s.data (), s.size ());
                        return outcome_t{ ok, text_of (t.get ()) };
                      } });
  list.push_back ({ "XmlText::set (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlText t = d.append_child ("r").text ();
                        bool const ok
                            = t.set (string_view_t (s.data (), s.size ()));
                        return outcome_t{ ok, text_of (t.get ()) };
                      } });
  list.push_back ({ "XmlText::operator= (char_t const *)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlText t = d.append_child ("r").text ();
                        t = s.c_str ();
                        return outcome_t{ true, text_of (t.get ()) };
                      } });
  list.push_back ({ "XmlText::operator= (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlText t = d.append_child ("r").text ();
                        t = string_view_t (s.data (), s.size ());
                        return outcome_t{ true, text_of (t.get ()) };
                      } });

  // ---- children made from a name
  list.push_back ({ "XmlNode::append_child (name)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child (s.c_str ());
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::append_child (name, size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode n = d.append_child (s.data (), s.size ());
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back (
      { "XmlNode::append_child (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode n = d.append_child (string_view_t (s.data (), s.size ()));
            return outcome_t{ static_cast<bool> (n), text_of (n.name ()) };
          } });
  list.push_back ({ "XmlNode::prepend_child (name)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        d.append_child ("first");
                        XmlNode n = d.prepend_child (s.c_str ());
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::prepend_child (name, size)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        d.append_child ("first");
                        XmlNode n = d.prepend_child (s.data (), s.size ());
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back (
      { "XmlNode::prepend_child (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            d.append_child ("first");
            XmlNode n = d.prepend_child (string_view_t (s.data (), s.size ()));
            return outcome_t{ static_cast<bool> (n), text_of (n.name ()) };
          } });
  list.push_back ({ "XmlNode::insert_child_before (name)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode ref = d.append_child ("ref");
                        XmlNode n = d.insert_child_before (s.c_str (), ref);
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back (
      { "XmlNode::insert_child_before (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode ref = d.append_child ("ref");
            XmlNode n = d.insert_child_before (s.data (), s.size (), ref);
            return outcome_t{ static_cast<bool> (n), text_of (n.name ()) };
          } });
  list.push_back ({ "XmlNode::insert_child_before (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode ref = d.append_child ("ref");
                        XmlNode n = d.insert_child_before (
                            string_view_t (s.data (), s.size ()), ref);
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back ({ "XmlNode::insert_child_after (name)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode ref = d.append_child ("ref");
                        XmlNode n = d.insert_child_after (s.c_str (), ref);
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });
  list.push_back (
      { "XmlNode::insert_child_after (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode ref = d.append_child ("ref");
            XmlNode n = d.insert_child_after (s.data (), s.size (), ref);
            return outcome_t{ static_cast<bool> (n), text_of (n.name ()) };
          } });
  list.push_back ({ "XmlNode::insert_child_after (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode ref = d.append_child ("ref");
                        XmlNode n = d.insert_child_after (
                            string_view_t (s.data (), s.size ()), ref);
                        return outcome_t{ static_cast<bool> (n),
                                          text_of (n.name ()) };
                      } });

  // ---- attributes made from a name
  list.push_back (
      { "XmlNode::append_attribute (name)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a
                = d.append_child ("r").append_attribute (s.c_str ());
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::append_attribute (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a
                = d.append_child ("r").append_attribute (s.data (), s.size ());
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::append_attribute (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlAttribute a = d.append_child ("r").append_attribute (
                string_view_t (s.data (), s.size ()));
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back ({ "XmlNode::prepend_attribute (name)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode r = d.append_child ("r");
                        r.append_attribute ("first");
                        XmlAttribute a = r.prepend_attribute (s.c_str ());
                        return outcome_t{ static_cast<bool> (a),
                                          text_of (a.name ()) };
                      } });
  list.push_back (
      { "XmlNode::prepend_attribute (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            r.append_attribute ("first");
            XmlAttribute a = r.prepend_attribute (s.data (), s.size ());
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::prepend_attribute (string_view_t)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            r.append_attribute ("first");
            XmlAttribute a
                = r.prepend_attribute (string_view_t (s.data (), s.size ()));
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::insert_attribute_before (name)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            XmlAttribute ref = r.append_attribute ("ref");
            XmlAttribute a = r.insert_attribute_before (s.c_str (), ref);
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::insert_attribute_before (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            XmlAttribute ref = r.append_attribute ("ref");
            XmlAttribute a
                = r.insert_attribute_before (s.data (), s.size (), ref);
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back ({ "XmlNode::insert_attribute_before (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode r = d.append_child ("r");
                        XmlAttribute ref = r.append_attribute ("ref");
                        XmlAttribute a = r.insert_attribute_before (
                            string_view_t (s.data (), s.size ()), ref);
                        return outcome_t{ static_cast<bool> (a),
                                          text_of (a.name ()) };
                      } });
  list.push_back (
      { "XmlNode::insert_attribute_after (name)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            XmlAttribute ref = r.append_attribute ("ref");
            XmlAttribute a = r.insert_attribute_after (s.c_str (), ref);
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back (
      { "XmlNode::insert_attribute_after (name, size)",
        [] (XmlDocument &d, std::string const &s)
          {
            XmlNode r = d.append_child ("r");
            XmlAttribute ref = r.append_attribute ("ref");
            XmlAttribute a
                = r.insert_attribute_after (s.data (), s.size (), ref);
            return outcome_t{ static_cast<bool> (a), text_of (a.name ()) };
          } });
  list.push_back ({ "XmlNode::insert_attribute_after (string_view_t)",
                    [] (XmlDocument &d, std::string const &s)
                      {
                        XmlNode r = d.append_child ("r");
                        XmlAttribute ref = r.append_attribute ("ref");
                        XmlAttribute a = r.insert_attribute_after (
                            string_view_t (s.data (), s.size ()), ref);
                        return outcome_t{ static_cast<bool> (a),
                                          text_of (a.name ()) };
                      } });
  return list;
}

// Texts with ill-formed UTF-8 of every kind, alone and between valid letters.
std::vector<std::string>
ill_formed_texts ()
{
  std::vector<std::string> texts;
  texts.push_back (std::string ("x\xC0\xBC") + "y");
  texts.push_back (std::string ("x\xC1\xBF") + "y");
  texts.push_back (std::string ("x\xE0\x80\xBC") + "y");
  texts.push_back (std::string ("x\xF0\x80\x80\xBC") + "y");
  texts.push_back (std::string ("x\xED\xA0\x80") + "y");
  texts.push_back (std::string ("x\xED\xBF\xBF") + "y");
  texts.push_back (std::string ("x\xF4\x90\x80\x80") + "y");
  texts.push_back (std::string ("x\xF8\x88\x80\x80\x80") + "y");
  texts.push_back (std::string ("x\xFF") + "y");
  texts.push_back (std::string ("x\x80") + "y");
  texts.push_back (std::string ("x\xE2\x82") + "y");
  texts.push_back (std::string ("x\xF0\x9F\x98") + "y");
  texts.push_back (std::string ("x\xC3\xA9\xC0\xBC\xE2\x82\xAC") + "y");
  texts.push_back (std::string ("\xC0\xBC") + "y");
  texts.push_back (std::string ("x\xC0\xBC"));
  texts.push_back (std::string ("x\xE2\x82"));
  // longer than the 32 characters below which the storage is reused
  texts.push_back (std::string ("0123456789abcdefghij\xC0\xBC")
                   + "0123456789abcdefghij\xFF" + "0123456789");
  return texts;
}

std::vector<std::string>
valid_texts ()
{
  std::vector<std::string> texts;
  texts.push_back (std::string ("x"));
  texts.push_back (std::string ("plain ascii text"));
  texts.push_back (std::string ("caf\xC3\xA9"));
  texts.push_back (std::string ("\xE2\x82\xAC"));
  texts.push_back (std::string ("\xEF\xBF\xBD")); // a real U+FFFD
  texts.push_back (std::string ("a\xEF\xBF\xBD") + "b");
  texts.push_back (std::string ("\xF0\x9F\x98\x80"));
  texts.push_back (std::string ("\xF4\x8F\xBF\xBF"));
  texts.push_back (std::string ("\xED\x9F\xBF") + "\xEE\x80\x80");
  texts.push_back (std::string ("\xC2\x80") + "\xDF\xBF");
  texts.push_back (std::string ("0123456789abcdefghij0123456789abcdefghij"
                                "0123456789 and a longer text"));
  return texts;
}

// UTF-16 and UTF-32 little endian back to UTF-8, for text that holds valid
// scalar values only.
void
append_utf8 (std::string &out, std::uint32_t cp)
{
  if (cp < 0x80)
    out.push_back (static_cast<char> (cp));
  else if (cp < 0x800)
    {
      out.push_back (static_cast<char> (0xC0 | (cp >> 6)));
      out.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
    }
  else if (cp < 0x10000)
    {
      out.push_back (static_cast<char> (0xE0 | (cp >> 12)));
      out.push_back (static_cast<char> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
    }
  else
    {
      out.push_back (static_cast<char> (0xF0 | (cp >> 18)));
      out.push_back (static_cast<char> (0x80 | ((cp >> 12) & 0x3F)));
      out.push_back (static_cast<char> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
    }
}

std::string
utf8_of_utf16_le (std::string const &bytes)
{
  std::string out;
  for (std::size_t i = 0; i + 1 < bytes.size (); i += 2)
    {
      std::uint32_t unit
          = static_cast<std::uint32_t> (byte_at (bytes, i))
            | (static_cast<std::uint32_t> (byte_at (bytes, i + 1)) << 8);
      if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < bytes.size ())
        {
          std::uint32_t const low
              = static_cast<std::uint32_t> (byte_at (bytes, i + 2))
                | (static_cast<std::uint32_t> (byte_at (bytes, i + 3)) << 8);
          unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
          i += 2;
        }
      append_utf8 (out, unit);
    }
  return out;
}

std::string
utf8_of_utf32_le (std::string const &bytes)
{
  std::string out;
  for (std::size_t i = 0; i + 3 < bytes.size (); i += 4)
    append_utf8 (
        out,
        static_cast<std::uint32_t> (byte_at (bytes, i))
            | (static_cast<std::uint32_t> (byte_at (bytes, i + 1)) << 8)
            | (static_cast<std::uint32_t> (byte_at (bytes, i + 2)) << 16)
            | (static_cast<std::uint32_t> (byte_at (bytes, i + 3)) << 24));
  return out;
}

class XmlSetterUtf8Test : public XmlFixture
{
protected:
  std::string
  saved (xml_encoding encoding)
  {
    std::ostringstream out;
    doc.save (out, "", kformat_raw | kformat_no_declaration, encoding);
    return out.str ();
  }

  // The UTF-8, UTF-16 and UTF-32 saves of the tree hold the same text.
  void
  expect_equal_saves (std::string const &label)
  {
    std::string const utf8 = saved (xml_encoding::encoding_utf8);
    EXPECT_EQ (utf8_of_utf16_le (saved (xml_encoding::encoding_utf16_le)),
               utf8)
        << label;
    EXPECT_EQ (utf8_of_utf32_le (saved (xml_encoding::encoding_utf32_le)),
               utf8)
        << label;
  }
};
} // namespace

TEST_F (
    XmlSetterUtf8Test,
    GivenIllFormedUtf8_WhenAnySetterStoresIt_ThenTheBytesAreDroppedAndTheCallSucceeds)
{
  std::vector<setter_t> const list = setters ();
  std::vector<std::string> const texts = ill_formed_texts ();
  for (std::size_t s = 0; s < list.size (); ++s)
    for (std::size_t t = 0; t < texts.size (); ++t)
      {
        doc.reset ();
        std::string const label
            = std::string (list[s].name) + ", text " + std::to_string (t);
        outcome_t const result = list[s].apply (doc, texts[t]);
        EXPECT_TRUE (result.ok) << label;
        EXPECT_EQ (result.stored, table_clean (texts[t])) << label;
        // the UTF-8, UTF-16 and UTF-32 saves are the same text
        expect_equal_saves (label);
      }
}

TEST_F (XmlSetterUtf8Test,
        GivenValidUtf8_WhenAnySetterStoresIt_ThenTheBytesAreStoredAsTheyAre)
{
  std::vector<setter_t> const list = setters ();
  std::vector<std::string> const texts = valid_texts ();
  for (std::size_t s = 0; s < list.size (); ++s)
    for (std::size_t t = 0; t < texts.size (); ++t)
      {
        doc.reset ();
        std::string const label
            = std::string (list[s].name) + ", text " + std::to_string (t);
        outcome_t const result = list[s].apply (doc, texts[t]);
        EXPECT_TRUE (result.ok) << label;
        EXPECT_EQ (result.stored, texts[t]) << label;
        expect_equal_saves (label);
      }
}

TEST_F (XmlSetterUtf8Test,
        GivenNothingButIllFormedBytes_WhenAValueIsSet_ThenTheValueIsEmpty)
{
  XmlNode r = doc.append_child ("r");
  XmlAttribute a = r.append_attribute ("k");
  ASSERT_TRUE (a.set_value ("kept"));
  EXPECT_TRUE (a.set_value ("\xFF\xC0\xBC"));
  EXPECT_STREQ (a.value (), "");
  XmlText t = r.text ();
  ASSERT_TRUE (t.set ("kept"));
  EXPECT_TRUE (t.set ("\x80\x80"));
  EXPECT_STREQ (t.get (), "");
  // a name that is left empty
  XmlNode n = doc.append_child ("\xFF");
  EXPECT_TRUE (n);
  EXPECT_STREQ (n.name (), "");
}

TEST_F (
    XmlSetterUtf8Test,
    GivenAnOldValue_WhenIllFormedValuesAreSet_ThenTheReuseAndTheAllocationPathsBothClean)
{
  // The storage of a value is reused when the new text fits and is not less
  // than half of it (below 32 characters, when it fits); the pointer tells
  // which path a call took.
  XmlAttribute a = doc.append_child ("r").append_attribute ("k");
  std::string const long_text (80, 'a');
  ASSERT_TRUE (a.set_value (long_text.c_str ()));
  char const *const storage = a.value ();

  // 60 characters after the cleaning: reuse, with the old tail overwritten
  std::string const reuse
      = std::string (30, 'b') + "\xC0\xBC" + std::string (30, 'c') + "\xFF";
  EXPECT_TRUE (a.set_value (reuse.c_str ()));
  EXPECT_EQ (a.value (), storage);
  EXPECT_EQ (std::string (a.value ()),
             std::string (30, 'b') + std::string (30, 'c'));

  // the same through the size form
  std::string const reuse_size
      = std::string (45, 'd') + "\xED\xA0\x80" + std::string (5, 'e');
  EXPECT_TRUE (a.set_value (reuse_size.data (), reuse_size.size ()));
  EXPECT_EQ (a.value (), storage);
  EXPECT_EQ (std::string (a.value ()),
             std::string (45, 'd') + std::string (5, 'e'));

  // 6 characters: the storage is more than twice too large, another one is
  // allocated
  std::string const shorter = std::string ("ab\xC0\xBC") + "cd\xFF" + "ef";
  EXPECT_TRUE (a.set_value (shorter.c_str ()));
  EXPECT_NE (a.value (), storage);
  EXPECT_STREQ (a.value (), "abcdef");
  char const *const small = a.value ();

  // longer than the storage has: allocated again
  std::string const longer
      = std::string (60, 'b') + "\xED\xA0\x80" + std::string (60, 'c');
  EXPECT_TRUE (a.set_value (longer.c_str ()));
  EXPECT_NE (a.value (), small);
  EXPECT_EQ (std::string (a.value ()),
             std::string (60, 'b') + std::string (60, 'c'));

  // and back to valid text
  EXPECT_TRUE (a.set_value ("plain"));
  EXPECT_STREQ (a.value (), "plain");
}

TEST_F (
    XmlSetterUtf8Test,
    GivenASizeThatCutsASequence_WhenTheValueIsSet_ThenTheCutBytesAreDropped)
{
  XmlAttribute a = doc.append_child ("r").append_attribute ("k");
  std::string const text = "ab\xE2\x82\xAC" + std::string ("cd");
  EXPECT_TRUE (a.set_value (text.data (), 4)); // "ab" and two bytes of EURO
  EXPECT_STREQ (a.value (), "ab");
  EXPECT_TRUE (a.set_value (text.data (), 5));
  EXPECT_EQ (std::string (a.value ()), "ab\xE2\x82\xAC");
  EXPECT_TRUE (a.set_value (text.data (), 0));
  EXPECT_STREQ (a.value (), "");
}

TEST_F (XmlSetterUtf8Test,
        GivenAValueWithNulAndASize_WhenSet_ThenTheNulIsAValidByte)
{
  // a NUL is valid UTF-8: the size form stores it, as before
  XmlAttribute a = doc.append_child ("r").append_attribute ("k");
  std::string const text ("a\0b\xC0\xBC"
                          "c",
                          6);
  EXPECT_TRUE (a.set_value (text.data (), text.size ()));
  EXPECT_STREQ (a.value (), "a");
}

TEST_F (
    XmlSetterUtf8Test,
    GivenAnEmptyHandleOrAWrongNodeType_WhenAnIllFormedValueIsSet_ThenItIsStillFalse)
{
  XmlNode empty;
  EXPECT_FALSE (empty.set_value ("x\xFF"));
  EXPECT_FALSE (empty.set_name ("x\xFF"));
  XmlAttribute no_attribute;
  EXPECT_FALSE (no_attribute.set_value ("x\xFF"));
  EXPECT_FALSE (no_attribute.set_name ("x\xFF"));
  // the document node has no name and no value
  EXPECT_FALSE (doc.set_name ("x\xFF"));
  EXPECT_FALSE (doc.set_value ("x\xFF"));
  // an element has a name and no value
  XmlNode e = doc.append_child ("e");
  EXPECT_FALSE (e.set_value ("x\xFF"));
}

TEST_F (XmlSetterUtf8Test,
        GivenANumericSetter_WhenItStoresAValue_ThenTheTextIsTheNumber)
{
  XmlNode r = doc.append_child ("r");
  XmlAttribute a = r.append_attribute ("k");
  ASSERT_TRUE (a.set_value (-42));
  EXPECT_STREQ (a.value (), "-42");
  ASSERT_TRUE (a.set_value (2.5));
  EXPECT_STREQ (a.value (), "2.5");
  ASSERT_TRUE (a.set_value (true));
  EXPECT_STREQ (a.value (), "true");
  XmlText t = r.text ();
  ASSERT_TRUE (t.set (7U));
  EXPECT_STREQ (t.get (), "7");
}

TEST_F (XmlSetterUtf8Test,
        GivenATreeLoadedWithKeptBytes_WhenAValueIsSet_ThenTheNewValueIsClean)
{
  // the load keeps the bytes; a setter cleans what it stores, whatever the
  // tree
  std::string const text
      = std::string ("<r k=\"a\xC0\xBC") + "b\">x\xFF" + "y</r>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (),
                                kparse_default | kparse_keep_invalid_utf8,
                                xml_encoding::encoding_utf8));
  XmlNode r = doc.child ("r");
  EXPECT_EQ (std::string (r.attribute ("k").value ()),
             std::string ("a\xC0\xBC") + "b");
  EXPECT_TRUE (r.attribute ("k").set_value ("c\xFF"
                                            "d"));
  EXPECT_STREQ (r.attribute ("k").value (), "cd");
  // the kept text is still kept where no setter touched it
  EXPECT_EQ (std::string (r.text ().get ()), std::string ("x\xFF") + "y");
}

TEST_F (XmlSetterUtf8Test,
        GivenCopiedNodes_WhenTheyHoldKeptBytes_ThenTheCopyKeepsThem)
{
  // copying a node copies what the tree holds; only the setters clean
  std::string const text = std::string ("<r>x\xFF") + "y</r>";
  ASSERT_TRUE (doc.load_buffer (text.data (), text.size (),
                                kparse_default | kparse_keep_invalid_utf8,
                                xml_encoding::encoding_utf8));
  XmlDocument other;
  XmlNode copy = other.append_copy (doc.child ("r"));
  EXPECT_EQ (std::string (copy.text ().get ()), std::string ("x\xFF") + "y");
}

namespace
{
class XmlSetterOverlapTest : public XmlSetterUtf8Test
{
protected:
  // A tree whose strings are heap storage that holds ill-formed bytes: a copy
  // of nodes that a load with kparse_keep_invalid_utf8 made (the strings of
  // the load itself live in the shared buffer of the document and are never
  // reused).
  XmlNode
  kept_copy ()
  {
    std::string const text
        = std::string ("<abcdefgh\xC0\xBC") + "ij k=\"abcdefgh\xC0\xBC"
          + "ij\">abcdefgh\xC0\xBC" + "ij</abcdefgh\xC0\xBC" + "ij>";
    EXPECT_TRUE (source.load_buffer (text.data (), text.size (),
                                     kparse_default | kparse_keep_invalid_utf8,
                                     xml_encoding::encoding_utf8));
    XmlNode const copy = doc.append_copy (source.first_child ());
    EXPECT_EQ (std::string (copy.attribute ("k").value ()),
               std::string ("abcdefgh\xC0\xBC") + "ij");
    return copy;
  }

  XmlDocument source;
};
} // namespace

// A setter that is given a part of its own string reads and writes the same
// storage; the copy of the text must tolerate it.
TEST_F (XmlSetterOverlapTest,
        GivenAPartOfItsOwnString_WhenAValidTextIsSet_ThenItIsThatPart)
{
  XmlNode r = doc.append_child ("r");
  XmlAttribute a = r.append_attribute ("k");
  ASSERT_TRUE (a.set_value ("abcdefghijkl"));
  EXPECT_TRUE (a.set_value (a.value () + 1));
  EXPECT_STREQ (a.value (), "bcdefghijkl");
  EXPECT_TRUE (a.set_value (a.value () + 2, 5));
  EXPECT_STREQ (a.value (), "defgh");
  EXPECT_TRUE (a.set_name ("abcdefghijkl"));
  EXPECT_TRUE (a.set_name (a.name () + 3));
  EXPECT_STREQ (a.name (), "defghijkl");

  XmlNode p = r.append_child (node_pcdata);
  ASSERT_TRUE (p.set_value ("abcdefghijkl"));
  EXPECT_TRUE (p.set_value (p.value () + 4));
  EXPECT_STREQ (p.value (), "efghijkl");
  XmlNode e = r.append_child ("abcdefghijkl");
  EXPECT_TRUE (e.set_name (e.name () + 5));
  EXPECT_STREQ (e.name (), "fghijkl");

  XmlText t = r.text ();
  ASSERT_TRUE (t.set ("abcdefghijkl"));
  EXPECT_TRUE (t.set (t.get () + 6));
  EXPECT_STREQ (t.get (), "ghijkl");
}

TEST_F (XmlSetterOverlapTest,
        GivenAPartOfItsOwnIllFormedString_WhenItIsSet_ThenItIsThatPartCleaned)
{
  XmlNode r = kept_copy ();
  ASSERT_TRUE (r);
  std::string const raw = std::string ("abcdefgh\xC0\xBC") + "ij";

  // the valid prefix before the bad bytes is longer than the shift, so the
  // copy of the prefix overlaps its source
  XmlAttribute a = r.attribute ("k");
  ASSERT_EQ (std::string (a.value ()), raw);
  EXPECT_TRUE (a.set_value (a.value () + 1));
  EXPECT_STREQ (a.value (), "bcdefghij");

  XmlNode text = r.first_child ();
  ASSERT_EQ (std::string (text.value ()), raw);
  EXPECT_TRUE (text.set_value (text.value () + 2));
  EXPECT_STREQ (text.value (), "cdefghij");

  XmlText t = r.text ();
  EXPECT_TRUE (t.set (t.get () + 1));
  EXPECT_STREQ (t.get (), "defghij");

  ASSERT_EQ (std::string (r.name ()), raw);
  EXPECT_TRUE (r.set_name (r.name () + 3));
  EXPECT_STREQ (r.name (), "defghij");

  XmlAttribute second = r.append_copy (source.first_child ().attribute ("k"));
  ASSERT_EQ (std::string (second.value ()), raw);
  EXPECT_TRUE (second.set_value (second.value () + 1, 6));
  EXPECT_STREQ (second.value (), "bcdefg");
}

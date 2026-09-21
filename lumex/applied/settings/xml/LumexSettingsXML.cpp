#define LUMEX_IMPLEMENTATION

#include <string>
#include <unordered_map>

#include "lumex/core/filesystem/LumexFilesystem"

#if defined(LUMEX_SETTINGS_WITH_XML)
#include "lumex/xml/LumexXml"
#endif

#include "LumexSettingsXML.hpp"

namespace lumex
{
namespace applied
{
namespace settings
{
namespace xml
{
#if defined(LUMEX_SETTINGS_WITH_XML)
namespace
{
bool
has_element_child (::lumex::xml::node::XmlNode const &node)
{
  for (::lumex::xml::node::XmlNode child = node.first_child (); child;
       child = child.next_sibling ())
    {
      if (child.type () == ::lumex::xml::types::Types::node_element)
        return true;
    }
  return false;
}

void
load_keys_from_section (
    ::lumex::xml::node::XmlNode const &section,
    std::string const &section_name,
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>> &store,
    bool &settings_loaded)
{
  if (section_name.empty ())
    return;

  for (::lumex::xml::node::XmlNode key = section.first_child (); key;
       key = key.next_sibling ())
    {
      if (key.type () != ::lumex::xml::types::Types::node_element)
        continue;
      if (has_element_child (key))
        continue;

      char const *const name = key.name ();
      if (name == nullptr || name[0] == '\0')
        continue;

      char const *const value = key.text ().get ();
      store[section_name][name] = (value != nullptr) ? value : "";
      settings_loaded = true;
    }
}
} // namespace
#endif

LUMEX_PUBLIC_API
bool
LumexSettingsXML::is_xml_valid (std::string const &path)
{
#if !defined(LUMEX_SETTINGS_WITH_XML)
  (void)path;
  return false;
#else
  if (lumex::core::filesystem::fs::lumex_filesystem::is_directory (path))
    return false;
  if (!lumex::core::filesystem::fs::lumex_filesystem::is_readable (path))
    return false;

  ::lumex::xml::document::XmlDocument document;
  ::lumex::xml::text::xml_parse_result_t const result
      = document.load_file (path.c_str ());
  if (!result)
    return false;
  return static_cast<bool> (document.document_element ());
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::is_xml_valid (char const *path)
{
  if (path == nullptr)
    return false;
  return is_xml_valid (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::_load_with_parser (std::string const &path)
{
#if !defined(LUMEX_SETTINGS_WITH_XML)
  (void)path;
  return false;
#else
  ::lumex::xml::document::XmlDocument document;
  ::lumex::xml::text::xml_parse_result_t const result
      = document.load_file (path.c_str ());
  if (!result)
    return false;

  ::lumex::xml::node::XmlNode const root = document.document_element ();
  if (!root)
    return false;

  char const *const root_name = root.name ();
  if (root_name == nullptr || root_name[0] == '\0')
    return false;

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      loaded;
  bool settings_loaded = false;
  bool has_section_children = false;

  for (::lumex::xml::node::XmlNode child = root.first_child (); child;
       child = child.next_sibling ())
    {
      if (child.type () != ::lumex::xml::types::Types::node_element)
        continue;
      if (has_element_child (child))
        {
          has_section_children = true;
          break;
        }
    }

  if (has_section_children)
    {
      for (::lumex::xml::node::XmlNode child = root.first_child (); child;
           child = child.next_sibling ())
        {
          if (child.type () != ::lumex::xml::types::Types::node_element)
            continue;
          if (has_element_child (child))
            {
              char const *const section_name = child.name ();
              if (section_name == nullptr || section_name[0] == '\0')
                continue;
              load_keys_from_section (child, section_name, loaded,
                                      settings_loaded);
            }
          else
            {
              char const *const key_name = child.name ();
              if (key_name == nullptr || key_name[0] == '\0')
                continue;
              char const *const value = child.text ().get ();
              loaded[root_name][key_name] = (value != nullptr) ? value : "";
              settings_loaded = true;
            }
        }
    }
  else
    {
      load_keys_from_section (root, root_name, loaded, settings_loaded);
    }

  if (!settings_loaded)
    return false;

  _settings.swap (loaded);
  return true;
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::_save_with_parser (std::string const &path) const
{
#if !defined(LUMEX_SETTINGS_WITH_XML)
  (void)path;
  return false;
#else
  auto parent = lumex::path (path).parent_path ();
  if (!parent.empty ()
      && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
    lumex::core::filesystem::fs::lumex_filesystem::create_directories (parent);

  ::lumex::xml::document::XmlDocument document;
  ::lumex::xml::node::XmlNode root
      = document.append_child (Constants::SETTINGS_ROOT_NAME);
  if (!root)
    return false;

  for (auto const &sec_pair : _settings)
    {
      if (sec_pair.first.empty ())
        continue;
      ::lumex::xml::node::XmlNode section
          = root.append_child (sec_pair.first.c_str ());
      if (!section)
        continue;
      for (auto const &kv : sec_pair.second)
        {
          if (kv.first.empty ())
            continue;
          ::lumex::xml::node::XmlNode key
              = section.append_child (kv.first.c_str ());
          if (!key)
            continue;
          (void)key.text ().set (kv.second.c_str ());
        }
    }

  return document.save_file (path.c_str ());
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::load (std::string const &path)
{
  if (!is_xml_valid (path))
    return false;
  return _load_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::load (char const *path)
{
  if (path == nullptr)
    return false;
  return load (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::save (std::string const &path) const
{
  return _save_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsXML::save (char const *path) const
{
  if (path == nullptr)
    return false;
  return save (std::string (path));
}

LUMEX_PUBLIC_API
std::string
LumexSettingsXML::get (std::string const &section,
                       std::string const &key) const
{
  if (section.empty () || key.empty () || _settings.empty ())
    return "";

  auto section_it = _settings.find (section);
  if (section_it == _settings.end ())
    return "";

  auto key_it = section_it->second.find (key);
  if (key_it == section_it->second.end ())
    return "";

  return key_it->second;
}

LUMEX_PUBLIC_API
std::string
LumexSettingsXML::get (char const *section, char const *key) const
{
  if (section == nullptr || key == nullptr)
    return "";
  return get (std::string (section), std::string (key));
}

LUMEX_PUBLIC_API
void
LumexSettingsXML::add (std::string const &section, std::string const &key,
                       std::string const &value)
{
  if (section.empty () || key.empty () || value.empty ())
    return;
  _settings[section][key] = value;
}

LUMEX_PUBLIC_API
void
LumexSettingsXML::add (char const *section, char const *key, char const *value)
{
  if (section == nullptr || key == nullptr || value == nullptr)
    return;
  add (std::string (section), std::string (key), std::string (value));
}

LUMEX_PUBLIC_API
void
LumexSettingsXML::remove (std::string const &section, std::string const &key)
{
  if (section.empty () || key.empty () || _settings.empty ())
    return;

  auto section_it = _settings.find (section);
  if (section_it == _settings.end ())
    return;

  section_it->second.erase (key);
}

LUMEX_PUBLIC_API
void
LumexSettingsXML::remove (char const *section, char const *key)
{
  if (section == nullptr || key == nullptr)
    return;
  remove (std::string (section), std::string (key));
}
} // namespace xml
} // namespace settings
} // namespace applied
} // namespace lumex

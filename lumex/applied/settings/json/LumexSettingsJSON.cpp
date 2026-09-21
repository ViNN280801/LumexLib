#define LUMEX_IMPLEMENTATION

#include <fstream>
#include <string>
#include <unordered_map>

#if defined(LUMEX_SETTINGS_WITH_JSON)
#include <nlohmann/json.hpp>
#endif

#include "lumex/core/filesystem/LumexFilesystem"

#include "LumexSettingsJSON.hpp"

namespace lumex
{
namespace applied
{
namespace settings
{
namespace json
{
#if defined(LUMEX_SETTINGS_WITH_JSON)
namespace
{
bool
is_scalar_setting (::nlohmann::json const &value)
{
  return value.is_string () || value.is_number () || value.is_boolean ();
}

std::string
scalar_to_string (::nlohmann::json const &value)
{
  if (value.is_string ())
    return value.get<std::string> ();
  if (value.is_boolean ())
    return value.get<bool> () ? "true" : "false";
  if (value.is_number_unsigned ())
    return std::to_string (value.get<unsigned long long> ());
  if (value.is_number_integer ())
    return std::to_string (value.get<long long> ());
  if (value.is_number_float ())
    return std::to_string (value.get<double> ());
  return "";
}

void
load_keys_from_object (
    ::nlohmann::json const &section, std::string const &section_name,
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>> &store,
    bool &settings_loaded)
{
  if (section_name.empty () || !section.is_object ())
    return;

  for (auto const &item : section.items ())
    {
      if (item.key ().empty ())
        continue;
      if (!is_scalar_setting (item.value ()))
        continue;
      store[section_name][item.key ()] = scalar_to_string (item.value ());
      settings_loaded = true;
    }
}

bool
load_json_object (::nlohmann::json &root, std::string const &path)
{
  std::ifstream file (path.c_str ());
  if (!file.is_open ())
    return false;

  root = ::nlohmann::json::parse (file, nullptr, false);
  return !root.is_discarded () && root.is_object ();
}
} // namespace
#endif

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::is_json_valid (std::string const &path)
{
#if !defined(LUMEX_SETTINGS_WITH_JSON)
  (void)path;
  return false;
#else
  if (lumex::core::filesystem::fs::lumex_filesystem::is_directory (path))
    return false;
  if (!lumex::core::filesystem::fs::lumex_filesystem::is_readable (path))
    return false;

  ::nlohmann::json root;
  return load_json_object (root, path);
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::is_json_valid (char const *path)
{
  if (path == nullptr)
    return false;
  return is_json_valid (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::_load_with_parser (std::string const &path)
{
#if !defined(LUMEX_SETTINGS_WITH_JSON)
  (void)path;
  return false;
#else
  ::nlohmann::json root;
  if (!load_json_object (root, path))
    return false;

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      loaded;
  bool settings_loaded = false;
  bool has_section_children = false;

  for (auto const &item : root.items ())
    {
      if (item.value ().is_object ())
        {
          has_section_children = true;
          break;
        }
    }

  if (has_section_children)
    {
      for (auto const &item : root.items ())
        {
          if (item.key ().empty ())
            continue;
          if (item.value ().is_object ())
            {
              load_keys_from_object (item.value (), item.key (), loaded,
                                     settings_loaded);
            }
          else if (is_scalar_setting (item.value ()))
            {
              loaded[Constants::SETTINGS_ROOT_NAME][item.key ()]
                  = scalar_to_string (item.value ());
              settings_loaded = true;
            }
        }
    }
  else
    {
      load_keys_from_object (root, Constants::SETTINGS_ROOT_NAME, loaded,
                             settings_loaded);
    }

  if (!settings_loaded)
    return false;

  _settings.swap (loaded);
  return true;
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::_save_with_parser (std::string const &path) const
{
#if !defined(LUMEX_SETTINGS_WITH_JSON)
  (void)path;
  return false;
#else
  auto parent = lumex::path (path).parent_path ();
  if (!parent.empty ()
      && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
    lumex::core::filesystem::fs::lumex_filesystem::create_directories (parent);

  ::nlohmann::json root = ::nlohmann::json::object ();
  for (auto const &sec_pair : _settings)
    {
      if (sec_pair.first.empty ())
        continue;
      ::nlohmann::json section = ::nlohmann::json::object ();
      for (auto const &kv : sec_pair.second)
        {
          if (kv.first.empty ())
            continue;
          section[kv.first] = kv.second;
        }
      root[sec_pair.first] = section;
    }

  std::ofstream file (path.c_str ());
  if (!file.is_open ())
    return false;
  file << root.dump (2) << '\n';
  return file.good ();
#endif
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::load (std::string const &path)
{
  if (!is_json_valid (path))
    return false;
  return _load_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::load (char const *path)
{
  if (path == nullptr)
    return false;
  return load (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::save (std::string const &path) const
{
  return _save_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsJSON::save (char const *path) const
{
  if (path == nullptr)
    return false;
  return save (std::string (path));
}

LUMEX_PUBLIC_API
std::string
LumexSettingsJSON::get (std::string const &section,
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
LumexSettingsJSON::get (char const *section, char const *key) const
{
  if (section == nullptr || key == nullptr)
    return "";
  return get (std::string (section), std::string (key));
}

LUMEX_PUBLIC_API
void
LumexSettingsJSON::add (std::string const &section, std::string const &key,
                        std::string const &value)
{
  if (section.empty () || key.empty () || value.empty ())
    return;
  _settings[section][key] = value;
}

LUMEX_PUBLIC_API
void
LumexSettingsJSON::add (char const *section, char const *key,
                        char const *value)
{
  if (section == nullptr || key == nullptr || value == nullptr)
    return;
  add (std::string (section), std::string (key), std::string (value));
}

LUMEX_PUBLIC_API
void
LumexSettingsJSON::remove (std::string const &section, std::string const &key)
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
LumexSettingsJSON::remove (char const *section, char const *key)
{
  if (section == nullptr || key == nullptr)
    return;
  remove (std::string (section), std::string (key));
}
} // namespace json
} // namespace settings
} // namespace applied
} // namespace lumex

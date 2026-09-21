#define LUMEX_IMPLEMENTATION
#include <array>
#include <fstream>
#include <regex>

#include "lumex/core/filesystem/LumexFilesystem"

#include "LumexSettingsINI.hpp"

namespace lumex
{
namespace applied
{
namespace settings
{
namespace ini
{
namespace
{
// util function to trim
inline std::string
_trim (std::string const &str)
{
  auto left = str.find_first_not_of (" \t\r\n");
  if (left == std::string::npos)
    return "";
  auto right = str.find_last_not_of (" \t\r\n");
  return str.substr (left, right - left + 1);
}

// Helper to quote values for saving if they contain special characters or
// leading/trailing spaces
inline std::string
_quote_if_needed (std::string const &value)
{
  std::string result;
  // Determine if quoting is necessary:
  // 1. If it contains spaces or common INI delimiters (;, #, =, ,).
  // 2. If it contains a literal double quote (needs escaping and quoting).
  bool needs_quoting = (value.find_first_of (" ;#=,") != std::string::npos
                        || value.find ('"') != std::string::npos);

  if (needs_quoting)
    {
      result += '"'; // Add opening quote
      for (char chr : value)
        {
          if (chr == '"' || chr == '\\')
            { // Escape literal quotes and backslashes
              result += '\\';
            }
          result += chr;
        }
      result += '"'; // Add closing quote
    }
  else
    {
      result = value; // No quoting needed, use value as-is
    }
  return result;
}
} // anonymous namespace

#include <iostream>
LUMEX_PUBLIC_API
bool
LumexSettingsINI::is_ini_valid (std::string const &path)
{
  if (lumex::core::filesystem::fs::lumex_filesystem::is_directory (path))
    return false;
  if (!lumex::core::filesystem::fs::lumex_filesystem::is_readable (path))
    return false;

  std::ifstream file (path.c_str ());
  if (!file.is_open ())
    return false;

  // Skip BOM if present
  std::array<char, Constants::UTF8_BOM_SIZE> bom{};
  file.read (bom.data (), Constants::UTF8_BOM_SIZE);
  if (file.gcount () != Constants::UTF8_BOM_SIZE
      || static_cast<unsigned char> (bom[0]) != Constants::UTF8_BOM_0
      || static_cast<unsigned char> (bom[1]) != Constants::UTF8_BOM_1
      || static_cast<unsigned char> (bom[2]) != Constants::UTF8_BOM_2)
    {
      file.seekg (0);
    }

  std::string line;
  std::regex section_re (Constants::REGEX_SECTION);
  std::regex kv_re (Constants::REGEX_KEY_VALUE);
  int line_number = 0;

  while (std::getline (file, line))
    {
      line_number++;
      std::string trimmed_line = _trim (line);
      if (trimmed_line.empty () || trimmed_line[0] == ';'
          || trimmed_line[0] == '#')
        continue;

      if (std::regex_match (trimmed_line, section_re))
        continue;

      // Check for key-value, allowing for inline comments
      // REMOVED FIX: This logic incorrectly truncated lines with ';' or '#'
      // inside quoted values. The REGEX_KEY_VALUE already handles inline
      // comments at the end of the line. std::size_t comment_pos =
      // trimmed_line.find_first_of(";#"); if(comment_pos != std::string::npos)
      // trimmed_line = _trim(trimmed_line.substr(0, comment_pos));

      if (std::regex_match (trimmed_line, kv_re))
        continue;

      // DEBUG: Print the failing line
      std::cout << "Invalid line " << line_number << ": '" << trimmed_line
                << "'\n";
      return false; // Line is not a valid comment, section, or key-value pair
    }

  return true;
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::is_ini_valid (char const *path)
{
  if (path == nullptr)
    return false; // FIX(Test:
                  // LumexSettingsINITest.GivenNullFilePath_WhenIsIniValid_ThenReturnsFalse):
                  // Handle nullptr input. This line avoids exception when path
                  // is nullptr.
  return is_ini_valid (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::_load_with_parser (std::string const &path)
{
  std::ifstream file (path.c_str ());
  if (!file.is_open ())
    return false;

  // Skip BOM if present
  std::array<char, Constants::UTF8_BOM_SIZE> bom{};
  file.read (bom.data (), Constants::UTF8_BOM_SIZE);
  if (file.gcount () != Constants::UTF8_BOM_SIZE
      || static_cast<unsigned char> (bom[0]) != Constants::UTF8_BOM_0
      || static_cast<unsigned char> (bom[1]) != Constants::UTF8_BOM_1
      || static_cast<unsigned char> (bom[2]) != Constants::UTF8_BOM_2)
    {
      file.seekg (0);
    }

  m_settings.clear ();
  std::string currentSection;
  std::string line;
  bool settings_loaded = false;

  std::regex section_re (Constants::REGEX_SECTION);
  std::regex kv_re (Constants::REGEX_KEY_VALUE);
  std::smatch match;

  while (std::getline (file, line))
    {
      std::string trimmed_line = _trim (line);
      if (trimmed_line.empty () || trimmed_line[0] == ';'
          || trimmed_line[0] == '#')
        continue;

      if (std::regex_match (trimmed_line, match, section_re))
        {
          currentSection = match[1].str ();
          continue;
        }

      if (std::regex_match (trimmed_line, match, kv_re))
        {
          std::string key = match[1].str ();
          std::string value_str;

          if (match[2].matched)
            { // Group 2 for quoted values (e.g., "value" or "val\"ue")
              value_str = match[2].str ();
              // Unescape backslash-escaped characters within the quoted value.
              std::string unescaped_val;
              for (std::size_t i = 0; i < value_str.length (); ++i)
                {
                  if (value_str[i] == '\\' && i + 1 < value_str.length ())
                    {
                      // If it's an escaped quote or backslash, unescape it
                      if (value_str[i + 1] == '"' || value_str[i + 1] == '\\')
                        {
                          unescaped_val += value_str[i + 1];
                          i++; // Skip the escaped character
                        }
                      else
                        {
                          // Otherwise, just append the backslash itself (e.g.,
                          // for unknown escapes)
                          unescaped_val += value_str[i];
                        }
                    }
                  else
                    {
                      unescaped_val += value_str[i];
                    }
                }
              value_str = unescaped_val;
            }
          else if (match[3].matched)
            { // Group 3 for unquoted values (e.g., value)
              value_str = _trim (
                  match[3].str ()); // Apply trim only to unquoted content
            }

          if (!key.empty ())
            {
              m_settings[currentSection][key] = value_str;
              settings_loaded = true;
            }
          continue;
        }
    }
  return settings_loaded;
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::_save_with_parser (std::string const &path) const
{
  auto parent = lumex::path (path).parent_path ();
  if (!parent.empty ()
      && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
    lumex::core::filesystem::fs::lumex_filesystem::create_directories (parent);

  std::ofstream file (path.c_str ());
  if (!file.is_open ())
    return false;

  bool firstSection = true;
  for (auto const &secPair : m_settings)
    {
      if (!firstSection && !secPair.second.empty ())
        file << "\n";

      if (!secPair.first.empty ())
        file << "[" << secPair.first << "]\n";

      for (auto const &kvEntry : secPair.second)
        file << kvEntry.first << "=" << _quote_if_needed (kvEntry.second)
             << "\n";

      if (!secPair.second.empty ())
        firstSection = false;
    }
  file.close ();
  return file.good ();
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::load (std::string const &path)
{
  if (!is_ini_valid (path))
    return false;
  return _load_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::load (char const *path)
{
  if (path == nullptr)
    return false; // FIX(Test:
                  // LumexSettingsINITest.GivenNullFilePath_WhenLoad_ThenReturnsFalse):
                  // Handle nullptr
  return load (std::string (path));
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::save (std::string const &path) const
{
  return _save_with_parser (path);
}

LUMEX_PUBLIC_API
bool
LumexSettingsINI::save (char const *path) const
{
  if (path == nullptr)
    return false; // FIX(Test:
                  // LumexSettingsINITest.GivenNullFilePath_WhenSave_ThenReturnsFalse):
                  // Handle nullptr
  return save (std::string (path));
}

LUMEX_PUBLIC_API
std::string
LumexSettingsINI::get (std::string const &section,
                       std::string const &key) const
{
  if (section.empty () || key.empty () || m_settings.empty ())
    return "";

  auto sectionIt{ m_settings.find (section) };
  if (sectionIt == m_settings.end ())
    return "";

  auto keyIt{ sectionIt->second.find (key) };
  if (keyIt == sectionIt->second.end ())
    return "";

  return keyIt->second;
}

LUMEX_PUBLIC_API
std::string
LumexSettingsINI::get (char const *section, char const *key) const
{
  if (section == nullptr || key == nullptr)
    return ""; // FIX(Test:
               // LumexSettingsINITest.GivenNullArguments_WhenGet_ThenReturnsEmptyString):
               // Handle nullptr input. This line avoids exception when section
               // or key is nullptr.
  return get (std::string (section), std::string (key));
}

LUMEX_PUBLIC_API
void
LumexSettingsINI::add (std::string const &section, std::string const &key,
                       std::string const &value)
{
  if (section.empty () || key.empty () || value.empty ())
    return;
  m_settings[section][key] = value;
}

LUMEX_PUBLIC_API
void
LumexSettingsINI::add (char const *section, char const *key, char const *value)
{
  if (section == nullptr || key == nullptr || value == nullptr)
    return; // FIX(Test:
            // LumexSettingsINITest.GivenNullArguments_WhenAdd_ThenDoesNothing):
            // Handle nullptr input. This line avoids exception when section,
            // key, or value is nullptr.
  add (std::string (section), std::string (key), std::string (value));
}

LUMEX_PUBLIC_API
void
LumexSettingsINI::remove (std::string const &section, std::string const &key)
{
  if (section.empty () || key.empty () || m_settings.empty ())
    return;

  auto sectionIt{ m_settings.find (section) };
  if (sectionIt == m_settings.end ())
    return;

  auto &keyMap{ sectionIt->second };
  auto keyIt{ keyMap.find (key) };
  if (keyIt == keyMap.end ())
    return;

  std::size_t removedCount{ keyMap.erase (key) };
  if (removedCount == 0)
    return;
}

LUMEX_PUBLIC_API
void
LumexSettingsINI::remove (char const *section, char const *key)
{
  if (section == nullptr || key == nullptr)
    return; // FIX(Test:
            // LumexSettingsINITest.GivenNullArguments_WhenRemove_ThenDoesNothing):
            // Handle nullptr input. This line avoids exception when section or
            // key is nullptr.
  remove (std::string (section), std::string (key));
}
} // namespace ini
} // namespace settings
} // namespace applied
} // namespace lumex

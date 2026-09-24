#define LUMEX_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>

#include "LumexFilesystem.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include <sys/stat.h>

#if LUMEX_OS_WINDOWS
#include <Shlwapi.h>
#include <Windows.h>
#include <winnt.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h> // For PATH_MAX
#include <sys/stat.h>
#include <sys/statvfs.h> // For statvfs
#include <sys/types.h>
#include <unistd.h>
#include <utime.h> // For utime and utimbuf

#endif

namespace lumex
{
namespace core
{
namespace filesystem
{
namespace fs
{

namespace Detail
{
#if LUMEX_OS_WINDOWS
LUMEX_CONST_STR kForbiddenChars = "<>:\"/\\|*?";
static std::array<std::string, 22> const kReservedNames
    = { "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
        "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
        "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
LUMEX_CONST_NUM std::size_t kReservedNamesCount = kReservedNames.size ();

static inline bool
isForbidden (char chr) LUMEX_NOEXCEPT
{
  return std::string (kForbiddenChars).find (chr) != std::string::npos;
}

static inline bool
isReservedName (std::string const &name) LUMEX_NOEXCEPT
{
  std::string upperName = name;
  std::transform (
      upperName.begin (), upperName.end (), upperName.begin (),
      [] (unsigned char ch) { return static_cast<char> (::toupper (ch)); });

  for (std::size_t i = 0; i < kReservedNamesCount; ++i)
    if (upperName == kReservedNames.at (i))
      return true;
  return false;
}

static inline bool
hasInvalidEnding (std::string const &name) LUMEX_NOEXCEPT
{
  return !name.empty () && (name.back () == '.' || name.back () == ' ');
}

#elif LUMEX_OS_APPLE
LUMEX_CONST_STR kForbiddenChars = ":/";
LUMEX_CONST_STR kProblematicChars = "*?|\"'";

static inline bool
isForbidden (char chr) LUMEX_NOEXCEPT
{
  return std::string (kForbiddenChars).find (chr) != std::string::npos
         || std::string (kProblematicChars).find (chr) != std::string::npos;
}

static inline bool
isReservedName (std::string const &name) LUMEX_NOEXCEPT
{
  std::string upperName = name;
  std::transform (
      upperName.begin (), upperName.end (), upperName.begin (),
      [] (unsigned char ch) { return static_cast<char> (::toupper (ch)); });

  static std::array<std::string, 22> const reservedNames
      = { "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
          "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
          "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
  std::size_t const count = reservedNames.size ();

  for (std::size_t i = 0; i < count; ++i)
    if (upperName == reservedNames.at (i))
      return true;
  return false;
}

static inline bool
hasInvalidEnding (std::string const &) LUMEX_NOEXCEPT
{
  return false;
}

#elif LUMEX_OS_LINUX
LUMEX_CONST_STR kForbiddenChars = "/";
LUMEX_CONST_STR kProblematicChars = "&;|*?'\"`[]()$<>{}^#\\%!";

static inline bool
isForbidden (char chr) LUMEX_NOEXCEPT
{
  return std::string (kForbiddenChars).find (chr) != std::string::npos
         || std::string (kProblematicChars).find (chr) != std::string::npos;
}

static inline bool
isReservedName (std::string const & /*name*/) LUMEX_NOEXCEPT
{
  return false;
}

static inline bool
hasInvalidEnding (std::string const &name) LUMEX_NOEXCEPT
{
  return !name.empty () && name.front () == '-';
}

#else
LUMEX_CONST_STR kForbiddenChars = "/<>:\"\\|*?";

static inline bool
isForbidden (char chr) LUMEX_NOEXCEPT
{
  return std::string (kForbiddenChars).find (chr) != std::string::npos;
}

static inline bool
isReservedName (std::string const &) LUMEX_NOEXCEPT
{
  return false;
}
static inline bool
hasInvalidEnding (std::string const &) LUMEX_NOEXCEPT
{
  return false;
}
#endif
} // namespace Detail

LUMEX_PUBLIC_API
lumex::path::path (string_type source) : m_path (std::move (source)) {}

lumex::path::value_type const lumex::path::preferred_separator;

LUMEX_PUBLIC_API
bool
lumex::path::is_separator (value_type chr)
{
#if LUMEX_OS_WINDOWS
  return chr == '/' || chr == '\\';
#else
  return chr == '/';
#endif
}

LUMEX_PUBLIC_API
void
lumex::path::append_separator_if_needed ()
{
  if (!m_path.empty () && !is_separator (m_path.back ()))
    m_path += preferred_separator;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator/= (path const &path_arg)
{
  if (path_arg.empty () || path_arg.m_path == ".")
    return *this;

  // Native std::filesystem append: an absolute rhs, or a rhs with a different
  // root-name, replaces *this. A rhs that only has a root-directory (e.g.
  // "/abs" on Windows) keeps this root-name and replaces the rest.
  if (path_arg.is_absolute ()
      || (path_arg.has_root_name ()
          && path_arg.root_name ().string () != root_name ().string ()))
    {
      m_path = path_arg.m_path;
      return *this;
    }

  // Existing contract: path(".") / "file.txt" simplifies to "file.txt".
  if (m_path == ".")
    {
      m_path = path_arg.m_path;
      return *this;
    }

  if (path_arg.has_root_directory ())
    m_path = root_name ().m_path;
  else
    append_separator_if_needed ();

  std::string const rhs_root = path_arg.root_name ().string ();
  if (rhs_root.empty ()
      || path_arg.m_path.compare (0, rhs_root.size (), rhs_root) != 0)
    m_path += path_arg.m_path;
  else
    m_path += path_arg.m_path.substr (rhs_root.size ());
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator/= (string_type const &path_arg)
{
  return operator/= (path (path_arg));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator/= (char const *path_arg)
{
  return operator/= (path (path_arg));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator+= (path const &path_arg)
{
  m_path += path_arg.m_path;
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator+= (string_type const &path_arg)
{
  m_path += path_arg;
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator+= (char const *path_arg)
{
  m_path += path_arg != nullptr ? path_arg : "";
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::operator+= (value_type chr)
{
  m_path += chr;
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::make_preferred ()
{
#if LUMEX_OS_WINDOWS
  std::replace (m_path.begin (), m_path.end (), '/', '\\');
#else
  std::replace (m_path.begin (), m_path.end (), '\\', '/');
#endif
  return *this;
}

LUMEX_PUBLIC_API
std::size_t
lumex::path::find_filename_pos () const
{
  if (m_path.empty ())
    return string_type::npos;

  std::size_t pos = m_path.find_last_of ("/\\");
  if (static_cast<decltype (string_type::npos)> (pos) == string_type::npos)
    return 0;

  return pos + 1;
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::filename () const
{
  if (m_path.empty ())
    return path ();

  // Handle special cases
  if (m_path == "." || m_path == "..")
    return path ();

  // Handle paths ending with separator (directories)
  if (is_separator (m_path.back ()))
    {
      // Find the last non-separator character
      std::size_t end = m_path.find_last_not_of ("/\\");
      if (end == string_type::npos)
        return path ("/"); // Root path_arg

      // Find the separator before the last component
      std::size_t start = m_path.find_last_of ("/\\", end);
      if (start == string_type::npos)
        return path (m_path.substr (0, end + 1));
      return path (m_path.substr (start + 1, end - start));
    }

  std::size_t pos = find_filename_pos ();
  if (static_cast<decltype (string_type::npos)> (pos) == string_type::npos)
    return path ();

  return path (m_path.substr (pos));
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::parent_path () const
{
  if (m_path.empty () || m_path == "." || m_path == "..")
    {
      // Return a path_arg with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  // Handle root paths
  if (m_path == "/" || m_path == "\\")
    {
      // Return a path_arg with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  std::size_t pos = find_filename_pos ();
  if (static_cast<decltype (string_type::npos)> (pos) == string_type::npos)
    {
      // Return a path_arg with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  // If filename starts at position 0, it's a single file with no directory
  if (pos == 0)
    {
      // Return a path_arg with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  // Remove trailing separator
  std::size_t end = pos - 1;
  while (end > 0 && is_separator (m_path[end]))
    --end;

  // If we're at the root, return empty
  if (end == 0 && is_separator (m_path[0]))
    {
      // Return a path with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  return path (m_path.substr (0, end + 1));
}

LUMEX_PUBLIC_API
std::size_t
lumex::path::find_extension_pos () const
{
  std::size_t filename_pos = find_filename_pos ();
  if (static_cast<decltype (string_type::npos)> (filename_pos)
      == string_type::npos)
    return string_type::npos;

  std::size_t dot_pos = m_path.find_last_of ('.');
  if (static_cast<decltype (string_type::npos)> (dot_pos) == string_type::npos
      || dot_pos < filename_pos)
    return string_type::npos;

  // Don't count dot at beginning of filename
  if (dot_pos == filename_pos)
    return string_type::npos;

  return dot_pos;
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::extension () const
{
  std::size_t pos = find_extension_pos ();
  if (static_cast<decltype (string_type::npos)> (pos) == string_type::npos)
    {
      // Return a path with empty string so empty() returns true
      path result;
      result.m_path.clear ();
      return result;
    }

  return path (m_path.substr (pos));
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::stem () const
{
  std::size_t filename_pos = find_filename_pos ();
  if (static_cast<decltype (string_type::npos)> (filename_pos)
      == string_type::npos)
    return path ();

  std::size_t ext_pos = find_extension_pos ();
  if (static_cast<decltype (string_type::npos)> (ext_pos) == string_type::npos)
    return path (m_path.substr (filename_pos));

  return path (m_path.substr (filename_pos, ext_pos - filename_pos));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_extension (char const *ext)
{
  return replace_extension (path (ext));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_extension (std::string const &ext)
{
  return replace_extension (path (ext));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_extension (path const &ext)
{
  std::size_t pos = find_extension_pos ();
  if (static_cast<decltype (string_type::npos)> (pos) != string_type::npos)
    m_path.erase (pos);

  if (!ext.empty () && ext.m_path != ".")
    {
      if (ext.m_path[0] != '.')
        m_path += '.';
      m_path += ext.m_path;
    }

  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::remove_filename ()
{
  std::size_t pos = find_filename_pos ();
  if (static_cast<decltype (string_type::npos)> (pos) != string_type::npos
      && pos > 0)
    {
      // Remove the filename and any trailing separators
      m_path.erase (pos);
      // Remove trailing separators
      while (!m_path.empty () && is_separator (m_path.back ())
             && m_path != "/")
        m_path.pop_back ();
      // If we end up empty, set to current directory
      if (m_path.empty ())
        m_path = ".";
    }
  else if (pos == 0)
    {
      // Single file with no directory - set to current directory
      m_path = ".";
    }
  return *this;
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_filename (char const *filename)
{
  return replace_filename (path (filename));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_filename (std::string const &filename)
{
  return replace_filename (path (filename));
}

LUMEX_PUBLIC_API
lumex::path &
lumex::path::replace_filename (path const &replacement)
{
  remove_filename ();
  return operator/= (replacement);
}

LUMEX_PUBLIC_API
bool
lumex::path::is_absolute () const
{
#if LUMEX_OS_WINDOWS
  // Native Windows: both a root-name (C: or UNC) and a root-directory.
  // "/file.txt" has a root-directory but no root-name, so it is relative.
  return has_root_name () && has_root_directory ();
#else
  return !m_path.empty () && m_path[0] == '/';
#endif
}

LUMEX_PUBLIC_API
bool
lumex::path::has_filename () const
{
  if (m_path.empty () || m_path == "." || m_path == "..")
    return false;

  std::size_t pos = find_filename_pos ();
  return pos != string_type::npos && pos < m_path.length ();
}

LUMEX_PUBLIC_API
bool
lumex::path::has_extension () const
{
  return static_cast<decltype (string_type::npos)> (find_extension_pos ())
         != string_type::npos;
}

LUMEX_PUBLIC_API
bool
lumex::path::has_parent_path () const
{
  return !parent_path ().empty ();
}

LUMEX_PUBLIC_API
bool
lumex::path::has_root_directory () const
{
#if LUMEX_OS_WINDOWS
  if (m_path.length () >= 3 && std::isalpha (m_path[0]) != 0
      && m_path[1] == ':' && is_separator (m_path[2]))
    return true;
  if (m_path.length () >= 2 && m_path[0] == '\\' && m_path[1] == '\\')
    return true;
  // Generic-format root directory without a root-name ("/file.txt",
  // "\file.txt").
  return !m_path.empty () && is_separator (m_path[0])
         && !(m_path.length () >= 2 && is_separator (m_path[1]));
#else
  return !m_path.empty () && m_path[0] == '/';
#endif
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::root_directory () const
{
  if (!has_root_directory ())
    return {};

#if LUMEX_OS_WINDOWS
  if (m_path.length () >= 3 && std::isalpha (m_path[0]) != 0
      && m_path[1] == ':')
    return path ("\\");
  if (m_path.length () >= 2 && m_path[0] == '\\' && m_path[1] == '\\')
    {
      // Find next separator after \\server
      std::size_t pos = m_path.find_first_of ("/\\", 2);
      if (static_cast<decltype (string_type::npos)> (pos) != string_type::npos)
        {
          pos = m_path.find_first_of ("/\\", pos + 1);
          if (static_cast<decltype (string_type::npos)> (pos)
              != string_type::npos)
            return path ("\\");
        }
    }
  if (!m_path.empty () && is_separator (m_path[0]))
    return path (string_type (1, m_path[0]));
  return {};
#else
  return path ("/");
#endif
}

LUMEX_PUBLIC_API
std::wstring
lumex::path::wstring () const
{
  return lumex_filesystem::to_wide_string (m_path);
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::root_name () const
{
#if LUMEX_OS_WINDOWS
  // Windows: root name is drive letter or UNC server/share
  if (m_path.size () >= 2 && std::isalpha (m_path[0]) != 0 && m_path[1] == ':')
    return path (m_path.substr (0, 2));
  if (m_path.size () >= 2 && is_separator (m_path[0])
      && is_separator (m_path[1]))
    {
      // UNC path: \\server\share\...
      std::size_t pos = m_path.find_first_of ("/\\", 2);
      if (static_cast<decltype (string_type::npos)> (pos) != string_type::npos)
        {
          pos = m_path.find_first_of ("/\\", pos + 1);
          if (static_cast<decltype (string_type::npos)> (pos)
              != string_type::npos)
            return path (m_path.substr (0, pos));
        }
    }
  return {};
#else
  // POSIX: no root name
  return {};
#endif
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::root_path () const
{
  path rnp = root_name ();
  path rdp = root_directory ();
  if (!rnp.empty () && !rdp.empty ())
    return path (rnp.string () + rdp.string ());
  if (!rnp.empty ())
    return rnp;
  if (!rdp.empty ())
    return rdp;
  return {};
}

LUMEX_PUBLIC_API
lumex::path
lumex::path::relative_path () const
{
  path rpp = root_path ();
  if (rpp.empty ())
    return *this;
  return path (m_path.substr (rpp.string ().length ()));
}

LUMEX_PUBLIC_API
bool
lumex::path::has_root_name () const
{
  return !root_name ().empty ();
}

LUMEX_PUBLIC_API
bool
lumex::path::has_root_path () const
{
  return !root_path ().empty ();
}

LUMEX_PUBLIC_API
bool
lumex::path::has_relative_path () const
{
  return !relative_path ().empty ();
}

LUMEX_PUBLIC_API
bool
lumex::path::has_stem () const
{
  return !stem ().empty ();
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::exists (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA (path_arg.c_str ());
  return attrs != INVALID_FILE_ATTRIBUTES;
#else
  struct stat stt;
  return stat (path_arg.c_str (), std::addressof (stt)) == 0;
#endif
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_directory (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA (path_arg.c_str ());
  return attrs != INVALID_FILE_ATTRIBUTES
         && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  struct stat st;
  if (stat (path_arg.c_str (), &st) != 0)
    return false;
  return S_ISDIR (st.st_mode);
#endif
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_regular_file (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA (path_arg.c_str ());
  return attrs != INVALID_FILE_ATTRIBUTES
         && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0
         && (attrs & FILE_ATTRIBUTE_DEVICE) == 0;
#else
  struct stat stt;
  if (stat (path_arg.c_str (), std::addressof (stt)) != 0)
    return false;
  return S_ISREG (stt.st_mode);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<bool>
lumex_filesystem::create_directory (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  if (CreateDirectoryA (path_arg.c_str (), nullptr) != 0)
    return filesystem_result<bool>::ok (true);
  DWORD error = GetLastError ();
  if (error == ERROR_ALREADY_EXISTS && is_directory (path_arg))
    return filesystem_result<bool>::ok (false);
  return filesystem_result<bool>::err (static_cast<int> (error), false);
#else
  if (mkdir (path_arg.c_str (), 0755) == 0)
    return filesystem_result<bool>::ok (true);
  if (errno == EEXIST && is_directory (path_arg))
    return filesystem_result<bool>::ok (false);
  return filesystem_result<bool>::err (errno, false);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::path>
lumex_filesystem::current_path ()
{
#if LUMEX_OS_WINDOWS
  std::array<char, MAX_PATH> buffer;
  DWORD result = GetCurrentDirectoryA (MAX_PATH, buffer.data ());
  if (result == 0 || result > MAX_PATH)
    return filesystem_result<path>::err (static_cast<int> (GetLastError ()),
                                         path ());
  return filesystem_result<path>::ok (path (buffer.data ()));
#else
  std::array<char, PATH_MAX> buffer;
  if (getcwd (buffer.data (), PATH_MAX) != nullptr)
    return filesystem_result<path>::ok (path (buffer.data ()));
  return filesystem_result<path>::err (errno, path ());
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::current_path (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  if (SetCurrentDirectoryA (path_arg.c_str ()) != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  if (chdir (path_arg.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

namespace detail
{
// ---------- Additional filesystem helpers (private) ------------
#if LUMEX_OS_WINDOWS
// Renamed and updated to apply permissions rather than just convert
static DWORD
apply_perms_to_windows_attributes (DWORD existing_attrs, lumex::perms prms)
{
  DWORD new_attrs = existing_attrs;
  if ((prms & lumex::perms::owner_write) == lumex::perms::none)
    new_attrs |= FILE_ATTRIBUTE_READONLY; // If write not allowed, set readonly
  else
    new_attrs
        &= ~FILE_ATTRIBUTE_READONLY; // If write allowed, ensure not readonly
  return new_attrs;
}
#else
static mode_t
perms_to_posix_mode (lumex::perms prms)
{
  return static_cast<mode_t> (prms & lumex::perms::mask);
}
#endif
} // namespace detail

// ---------------- Low-level status helpers --------------------
#if LUMEX_OS_WINDOWS
LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::file_status>
lumex_filesystem::get_file_status_windows (path const &path_arg, bool follow)
{
  WIN32_FILE_ATTRIBUTE_DATA data;
  BOOL ok_
      = follow
            ? GetFileAttributesExA (path_arg.c_str (), GetFileExInfoStandard,
                                    std::addressof (data))
            : GetFileAttributesExA (path_arg.c_str (), GetFileExInfoStandard,
                                    std::addressof (data));
  if (!static_cast<bool> (ok_))
    return filesystem_result<file_status>::err (
        static_cast<int> (GetLastError ()),
        file_status (file_type::not_found));

  DWORD tmp = data.dwFileAttributes;
  file_type type = file_type::unknown;
  if ((tmp & FILE_ATTRIBUTE_DIRECTORY) != 0U)
    type = file_type::directory;
  else if ((tmp & FILE_ATTRIBUTE_REPARSE_POINT) != 0U)
    type = file_type::symlink;
  else
    type = file_type::regular;

  perms file_perms
      = ((tmp & FILE_ATTRIBUTE_READONLY) != 0U)
            ? (perms::owner_read | perms::group_read | perms::others_read)
            : perms::all;
  return filesystem_result<file_status>::ok (file_status (type, file_perms));
}
#else
LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::file_status>
lumex_filesystem::get_file_status_posix (path const &path_arg, bool follow)
{
  struct stat stt;
  int res = follow ? stat (path_arg.c_str (), std::addressof (stt))
                   : lstat (path_arg.c_str (), std::addressof (stt));
  if (res != 0)
    return filesystem_result<file_status>::err (
        errno, file_status (file_type::not_found));
  file_type type = file_type::unknown;
  if (S_ISREG (stt.st_mode))
    type = file_type::regular;
  else if (S_ISDIR (stt.st_mode))
    type = file_type::directory;
  else if (S_ISLNK (stt.st_mode))
    type = file_type::symlink;
  else if (S_ISCHR (stt.st_mode))
    type = file_type::character;
  else if (S_ISBLK (stt.st_mode))
    type = file_type::block;
  else if (S_ISFIFO (stt.st_mode))
    type = file_type::fifo;
  else if (S_ISSOCK (stt.st_mode))
    type = file_type::socket;

  auto perms = static_cast<perms> (stt.st_mode & 07777);
  return filesystem_result<file_status>::ok (file_status (type, perms));
}
#endif

// ---------------- public status helpers -----------------------
LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::file_status>
lumex_filesystem::status (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  return get_file_status_windows (path_arg, true);
#else
  return get_file_status_posix (path_arg, true);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::file_status>
lumex_filesystem::symlink_status (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  return get_file_status_windows (path_arg, false);
#else
  return get_file_status_posix (path_arg, false);
#endif
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_symlink (path const &path_arg)
{
  return symlink_status (path_arg).value ().type () == file_type::symlink;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_block_file (path const &path_arg)
{
  return status (path_arg).value ().type () == file_type::block;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_character_file (path const &path_arg)
{
  return status (path_arg).value ().type () == file_type::character;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_fifo (path const &path_arg)
{
  return status (path_arg).value ().type () == file_type::fifo;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_socket (path const &path_arg)
{
  return status (path_arg).value ().type () == file_type::socket;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_other (path const &path_arg)
{
  file_type fType = status (path_arg).value ().type ();
  return fType != file_type::none && fType != file_type::not_found
         && fType != file_type::regular && fType != file_type::directory;
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_empty (path const &path_arg)
{
  if (!exists (path_arg))
    return false;
  if (is_directory (path_arg))
    {
      directory_iterator iter (path_arg);
      return iter == directory_iterator ();
    }
  filesystem_result<std::uintmax_t> fsRes = file_size (path_arg);
  return fsRes.success () && fsRes.value () == 0;
}

// ---------------- copy operations -----------------------------
LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::copy (path const &from, path const &to_)
{
  // Step 1: Check if the source 'from' is a directory.
  if (is_directory (from))
    {
      // Step 2 (Recursive case - if 'from' is a directory):
      // Try to create the target directory 'to_'.
      filesystem_result<bool> created = create_directory (to_);

      // If creation fails and the error is not 'already exists', return the
      // error.
      if (!created && created.error_code () != 0)
        return filesystem_result<void>::err (created.error_code ());

      // Get all entries (files and subdirectories) within the source
      // directory.
      std::vector<directory_entry> entries = directory_contents (from);

      // Iterate over each entry in the source directory.
      for (directory_entry const &entry : entries)
        {
          // Form the new source path_arg for the current entry.
          path new_from = entry.path ();

          // Form the new target path_arg for the current entry within the
          // destination.
          path new_to = to_ / entry.path ().filename ();

          // Recursively call 'copy' for the current entry.
          filesystem_result<void> res = copy (new_from, new_to);

          // If the recursive copy fails, propagate the error up.
          if (!res)
            return res;
        }
      // If all entries were successfully copied, return success.
      return filesystem_result<void>::ok ();
    }
  // Step 3 (Non-recursive case - if 'from' is a regular file):
  // Directly copy the file using 'copy_file' function.
  // The result (success or error) of 'copy_file' is returned.
  return copy_file (from, to_);
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::copy_file (path const &from, path const &to_)
{
#if LUMEX_OS_WINDOWS
  if (CopyFileA (from.c_str (), to_.c_str (), FALSE) != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  std::size_t const bufSize = 16384;
  char buffer[bufSize];
  int inFd = open (from.c_str (), O_RDONLY);
  if (inFd < 0)
    return filesystem_result<void>::err (errno);
  int outFd = open (to_.c_str (), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (outFd < 0)
    {
      close (inFd);
      return filesystem_result<void>::err (errno);
    }
  ssize_t r;
  while ((r = read (inFd, buffer, bufSize)) > 0)
    {
      ssize_t w = write (outFd, buffer, static_cast<std::size_t> (r));
      if (w != r)
        {
          close (inFd);
          close (outFd);
          return filesystem_result<void>::err (errno);
        }
    }
  close (inFd);
  close (outFd);
  if (r < 0)
    return filesystem_result<void>::err (errno);
  return filesystem_result<void>::ok ();
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::copy_symlink (path const &from, path const &to_)
{
#if LUMEX_OS_WINDOWS
  // Windows requires knowing if link is file or dir
  DWORD attrs = GetFileAttributesA (from.c_str ());
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
  bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if (CreateSymbolicLinkA (to_.c_str (), from.c_str (),
                           isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)
      != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  if (symlink (from.c_str (), to_.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

// ---------------- directory helpers ---------------------------
LUMEX_PUBLIC_API
lumex::filesystem_result<bool>
lumex_filesystem::create_directories (path const &path_arg)
{
  if (path_arg.empty ())
    return filesystem_result<bool>::ok (false);
  if (exists (path_arg))
    return filesystem_result<bool>::ok (false);
  path parent = path_arg.parent_path ();
  if (!parent.empty ())
    {
      filesystem_result<bool> res = create_directories (parent);
      if (!res && res.error_code () != 0)
        return filesystem_result<bool>::err (res.error_code (), false);
    }
  return create_directory (path_arg);
}

LUMEX_PUBLIC_API
lumex::filesystem_result<bool>
lumex_filesystem::remove (path const &path_arg)
{
  // If file doesn't exist, return success with false
  if (!exists (path_arg))
    return filesystem_result<bool>::ok (false);

#if LUMEX_OS_WINDOWS
  // Native std::filesystem::remove on Windows clears FILE_ATTRIBUTE_READONLY
  // before DeleteFile/RemoveDirectory. Without that, remove_all fails after
  // permissions(..., owner_read) and leaves a leftover tree that poisons the
  // next gtest_discover_tests process for the same case.
  DWORD attrs = GetFileAttributesA (path_arg.c_str ());
  if (attrs != INVALID_FILE_ATTRIBUTES
      && (attrs & FILE_ATTRIBUTE_READONLY) != 0U)
    {
      SetFileAttributesA (path_arg.c_str (), attrs & ~FILE_ATTRIBUTE_READONLY);
    }
  if (is_directory (path_arg))
    {
      if (RemoveDirectoryA (path_arg.c_str ()) != 0)
        return filesystem_result<bool>::ok (true);
    }
  else
    {
      if (DeleteFileA (path_arg.c_str ()) != 0)
        return filesystem_result<bool>::ok (true);
    }
  // Return false with the last Windows error if removal failed.
  return filesystem_result<bool>::err (static_cast<int> (GetLastError ()),
                                       false);
#else
  if (is_directory (path_arg))
    {
      if (rmdir (path_arg.c_str ()) == 0)
        return filesystem_result<bool>::ok (true);
    }
  else
    {
      if (unlink (path_arg.c_str ()) == 0)
        return filesystem_result<bool>::ok (true);
    }
  // Return false with the errno if removal failed.
  return filesystem_result<bool>::err (errno, false);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<std::uintmax_t>
lumex_filesystem::remove_all (path const &path_arg)
{
  if (!exists (path_arg))
    return filesystem_result<std::uintmax_t>::ok (
        static_cast<std::uintmax_t> (0));
  std::uintmax_t count = 0;
  if (is_directory (path_arg))
    {
      for (directory_entry const &dirEntry : directory_contents (path_arg))
        {
          filesystem_result<std::uintmax_t> sub
              = remove_all (dirEntry.path ());
          if (!sub)
            return sub;
          count += sub.value ();
        }
    }
  filesystem_result<bool> self = remove (path_arg);
  if (!self)
    return filesystem_result<std::uintmax_t>::err (self.error_code (), count);
  return filesystem_result<std::uintmax_t>::ok (count + 1);
}

LUMEX_PUBLIC_API
lumex::filesystem_result<std::uintmax_t>
lumex_filesystem::file_size (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile
      = CreateFileA (path_arg.c_str (), GENERIC_READ, FILE_SHARE_READ, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return filesystem_result<std::uintmax_t>::err (
        static_cast<int> (GetLastError ()), 0);
  LARGE_INTEGER size;
  if (GetFileSizeEx (hFile, std::addressof (size)) == 0)
    {
      DWORD err = GetLastError ();
      CloseHandle (hFile);
      return filesystem_result<std::uintmax_t>::err (static_cast<int> (err),
                                                     0);
    }
  CloseHandle (hFile);
  return filesystem_result<std::uintmax_t>::ok (
      static_cast<std::uintmax_t> (size.QuadPart));
#else
  struct stat stt;
  if (stat (path_arg.c_str (), std::addressof (stt)) != 0)
    return filesystem_result<std::uintmax_t>::err (errno, 0);

  return filesystem_result<std::uintmax_t>::ok (
      static_cast<std::uintmax_t> (stt.st_size));
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<std::time_t>
lumex_filesystem::last_write_time (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile
      = CreateFileA (path_arg.c_str (), GENERIC_READ, FILE_SHARE_READ, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return filesystem_result<std::time_t>::err (
        static_cast<int> (GetLastError ()), 0);
  FILETIME fTime;
  if (GetFileTime (hFile, nullptr, nullptr, std::addressof (fTime)) == 0)
    {
      DWORD err = GetLastError ();
      CloseHandle (hFile);
      return filesystem_result<std::time_t>::err (static_cast<int> (err), 0);
    }
  CloseHandle (hFile);
  ULARGE_INTEGER ulInt;
  ulInt.LowPart = fTime.dwLowDateTime;
  ulInt.HighPart = fTime.dwHighDateTime;
  auto timeStruct = static_cast<std::time_t> (
      (ulInt.QuadPart - KDEFAULT_WINDOWS_FILETIME_TO_UNIX_EPOCH_INTERVALS)
      / KDEFAULT_HUNDRED_NANOSECONDS_PER_SECOND);
  return filesystem_result<std::time_t>::ok (timeStruct);
#else
  struct stat stt;
  if (stat (path_arg.c_str (), std::addressof (stt)) != 0)
    return filesystem_result<std::time_t>::err (errno, 0);

  return filesystem_result<std::time_t>::ok (stt.st_mtime);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::last_write_time (path const &path_arg, std::time_t new_time)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile
      = CreateFileA (path_arg.c_str (), GENERIC_WRITE, FILE_SHARE_WRITE,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
  ULARGE_INTEGER ulInt;
  ulInt.QuadPart = (static_cast<unsigned long long> (new_time)
                    * KDEFAULT_HUNDRED_NANOSECONDS_PER_SECOND)
                   + KDEFAULT_WINDOWS_FILETIME_TO_UNIX_EPOCH_INTERVALS;
  FILETIME fTime;
  fTime.dwLowDateTime = ulInt.LowPart;
  fTime.dwHighDateTime = ulInt.HighPart;
  BOOL ok_ = SetFileTime (hFile, nullptr, nullptr, std::addressof (fTime));
  DWORD err = (ok_ != 0) ? 0 : GetLastError ();
  CloseHandle (hFile);
  return (ok_ != 0) ? filesystem_result<void>::ok ()
                    : filesystem_result<void>::err (static_cast<int> (err));
#else
  struct utimbuf buf;
  buf.actime = new_time;
  buf.modtime = new_time;
  if (utime (path_arg.c_str (), std::addressof (buf)) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::permissions (path const &path_arg, perms prms)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA (path_arg.c_str ());
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return filesystem_result<void>::err (static_cast<int> (GetLastError ()));

  DWORD new_attrs = detail::apply_perms_to_windows_attributes (attrs, prms);

  if (SetFileAttributesA (path_arg.c_str (), new_attrs) != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  mode_t mode = detail::perms_to_posix_mode (prms);
  if (chmod (path_arg.c_str (), mode) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::path>
lumex_filesystem::read_symlink (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  std::vector<char> buf (MAX_PATH);
  DWORD len = GetFinalPathNameByHandleA (
      CreateFileA (path_arg.c_str (), 0, 0, nullptr, OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS, nullptr),
      buf.data (), MAX_PATH, FILE_NAME_NORMALIZED);
  if (len == 0)
    return filesystem_result<path>::err (static_cast<int> (GetLastError ()),
                                         path ());
  return filesystem_result<path>::ok (path (std::string (buf.data (), len)));
#else
  std::vector<char> buf (PATH_MAX);
  ssize_t len = readlink (path_arg.c_str (), buf.data (), buf.size () - 1);
  if (len < 0)
    return filesystem_result<path>::err (errno, path ());
  buf[len] = 0;
  return filesystem_result<path>::ok (path (buf.data ()));
#endif
}

// --------- space info & temp directory -----------------
LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::space_info>
lumex_filesystem::space (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  ULARGE_INTEGER freeBytesAvailable;
  ULARGE_INTEGER totalNumberOfBytes;
  ULARGE_INTEGER totalNumberOfFreeBytes;

  if (GetDiskFreeSpaceExA (path_arg.c_str (), &freeBytesAvailable,
                           &totalNumberOfBytes, &totalNumberOfFreeBytes)
      == 0)
    return filesystem_result<space_info>::err (
        static_cast<int> (GetLastError ()), space_info ());
  space_info info;
  info.available = static_cast<std::uintmax_t> (freeBytesAvailable.QuadPart);
  info.free = static_cast<std::uintmax_t> (totalNumberOfFreeBytes.QuadPart);
  info.capacity = static_cast<std::uintmax_t> (totalNumberOfBytes.QuadPart);
  return filesystem_result<space_info>::ok (info);
#else
  struct statvfs vfs;
  if (statvfs (path_arg.c_str (), std::addressof (vfs)) != 0)
    return filesystem_result<space_info>::err (errno, space_info ());
  space_info info;
  info.available = static_cast<std::uintmax_t> (vfs.f_bavail) * vfs.f_frsize;
  info.free = static_cast<std::uintmax_t> (vfs.f_bfree) * vfs.f_frsize;
  info.capacity = static_cast<std::uintmax_t> (vfs.f_blocks) * vfs.f_frsize;
  return filesystem_result<space_info>::ok (info);
#endif
}

// -------------- directory listing ----------------------
class lumex::directory_iterator::Impl
{
public:
#if LUMEX_OS_WINDOWS
  HANDLE handle;
  WIN32_FIND_DATAA data;
  bool first;
#else
  DIR *dirp;
  struct dirent *entry;
#endif
  path base;
  directory_entry current;
};

LUMEX_PUBLIC_API
lumex::directory_iterator::directory_iterator (path const &path_arg)
    : m_impl (new Impl)
{
  m_impl->base = path_arg;
#if LUMEX_OS_WINDOWS
  std::string pattern = path_arg.string ();
  if (!pattern.empty () && pattern.back () != '/')
    pattern += "/*";
  else
    pattern += "*";
  m_impl->handle = FindFirstFileA (pattern.c_str (), &m_impl->data);
  m_impl->first = true;
  if (m_impl->handle == INVALID_HANDLE_VALUE)
    m_impl.reset ();
#else
  m_impl->dirp = opendir (path_arg.c_str ());
  m_impl->entry = nullptr;
  if (!m_impl->dirp)
    m_impl.reset ();
#endif
  operator++ ();
}

LUMEX_PUBLIC_API
lumex::directory_iterator::directory_iterator (directory_iterator const &other)
    : m_impl (other.m_impl)
{
}

LUMEX_PUBLIC_API
lumex::directory_iterator &
lumex::directory_iterator::operator= (directory_iterator const &other)
{
  m_impl = other.m_impl;
  return *this;
}

LUMEX_PUBLIC_API
lumex::directory_iterator::directory_iterator (directory_iterator &&other)
    LUMEX_NOEXCEPT : m_impl (std::move (other.m_impl))
{
}

LUMEX_PUBLIC_API
lumex::directory_iterator &
lumex::directory_iterator::operator= (directory_iterator &&other)
    LUMEX_NOEXCEPT
{
  m_impl = std::move (other.m_impl);
  return *this;
}

LUMEX_PUBLIC_API lumex::directory_iterator::reference
lumex::directory_iterator::operator* () const
{
  return m_impl->current;
}

LUMEX_PUBLIC_API
lumex::directory_iterator::pointer
lumex::directory_iterator::operator->() const
{
  return &m_impl->current;
}

LUMEX_PUBLIC_API
lumex::directory_iterator &
lumex::directory_iterator::operator++ ()
{
  if (!m_impl)
    return *this;
#if LUMEX_OS_WINDOWS
  BOOL ok_;
  if (m_impl->first)
    {
      ok_ = TRUE;
      m_impl->first = false;
    }
  else
    {
      ok_ = FindNextFileA (m_impl->handle, &m_impl->data);
    }
  while (ok_ != 0)
    {
      std::string name (m_impl->data.cFileName);
      if (name != "." && name != "..")
        {
          m_impl->current = directory_entry (m_impl->base / path (name));
          return *this;
        }
      ok_ = FindNextFileA (m_impl->handle, &m_impl->data);
    }
  FindClose (m_impl->handle);
  m_impl.reset ();
#else
  while ((m_impl->entry = readdir (m_impl->dirp)) != nullptr)
    {
      std::string name (m_impl->entry->d_name);
      if (name != "." && name != "..")
        {
          m_impl->current = directory_entry (m_impl->base / path (name));
          return *this;
        }
    }
  closedir (m_impl->dirp);
  m_impl.reset ();
#endif
  return *this;
}

LUMEX_PUBLIC_API
lumex::directory_iterator
lumex::directory_iterator::operator++ (int)
{
  lumex::directory_iterator tmp (*this);
  ++*this;
  return tmp;
}

LUMEX_PUBLIC_API
bool
lumex::directory_iterator::operator== (directory_iterator const &rhs) const
{
  return m_impl == rhs.m_impl;
}

LUMEX_PUBLIC_API
bool
lumex::directory_iterator::operator!= (directory_iterator const &rhs) const
{
  return !(*this == rhs);
}

// directory_entry simple wrappers
LUMEX_PUBLIC_API
directory_entry::directory_entry (fs::path const &path_arg)
    : m_path (path_arg), m_status_known (false)
{
}

LUMEX_PUBLIC_API
void
lumex::directory_entry::refresh_status () const
{
  m_status = lumex_filesystem::status (m_path).value ();
  m_status_known = true;
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::exists () const
{
  return lumex_filesystem::exists (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_regular_file () const
{
  return lumex_filesystem::is_regular_file (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_directory () const
{
  return lumex_filesystem::is_directory (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_symlink () const
{
  return lumex_filesystem::is_symlink (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_block_file () const
{
  return lumex_filesystem::is_block_file (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_character_file () const
{
  return lumex_filesystem::is_character_file (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_fifo () const
{
  return lumex_filesystem::is_fifo (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_socket () const
{
  return lumex_filesystem::is_socket (m_path);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::is_other () const
{
  return lumex_filesystem::is_other (m_path);
}

LUMEX_PUBLIC_API
std::uintmax_t
lumex::directory_entry::file_size () const
{
  return lumex_filesystem::file_size (m_path).value ();
}

LUMEX_PUBLIC_API
lumex::file_status
lumex::directory_entry::status () const
{
  return lumex_filesystem::status (m_path).value ();
}

LUMEX_PUBLIC_API
lumex::file_status
lumex::directory_entry::symlink_status () const
{
  return lumex_filesystem::symlink_status (m_path).value ();
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator== (directory_entry const &rhs) const
{
  return m_path == rhs.m_path;
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator!= (directory_entry const &rhs) const
{
  return !(*this == rhs);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator< (directory_entry const &rhs) const
{
  return m_path < rhs.m_path;
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator<= (directory_entry const &rhs) const
{
  return !(rhs < *this);
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator> (directory_entry const &rhs) const
{
  return rhs < *this;
}

LUMEX_PUBLIC_API
bool
lumex::directory_entry::operator>= (directory_entry const &rhs) const
{
  return !(*this < rhs);
}

LUMEX_PUBLIC_API
std::vector<lumex::directory_entry>
lumex_filesystem::directory_contents (path const &path_arg)
{
  std::vector<lumex::directory_entry> out;
  for (directory_iterator it (path_arg); it != directory_iterator (); ++it)
    out.push_back (*it);
  return out;
}

LUMEX_PUBLIC_API
lumex::filesystem_result<std::vector<lumex::path>>
lumex_filesystem::directory_paths (path const &path_arg)
{
  std::vector<lumex::path> paths;

  // IMPORTANT FIX(Test: Filesystem_DirectoryPaths_PathIsFile):
  // Ensure the path_arg exists and is a directory BEFORE iterating.
  // The original logic was flawed because directory_iterator might not
  // immediately set errno or throw for non-directory paths, leading to an
  // empty 'paths' vector that then incorrectly passes the `if(!paths.empty()
  // || lumex_filesystem::exists(path_arg))` check.
  if (!lumex_filesystem::exists (path_arg))
    return filesystem_result<std::vector<path>>::err (
        ENOENT, {}); // No such file or directory

  if (!lumex_filesystem::is_directory (path_arg))
    {
      // If it exists but is not a directory, return an error like ENOTDIR (Not
      // a directory). On Windows, the equivalent might be different, but a
      // non-zero error is expected.
      return filesystem_result<std::vector<path>>::err (ENOTDIR, {});
    }

  // Use directory_contents to get entries, then extract paths
  // If the path_arg is a valid directory, the iterator should work.
  for (directory_iterator it (path_arg); it != directory_iterator (); ++it)
    paths.push_back (it->path ());

  // If we reach here, the path_arg exists and is a directory.
  // The result should always be successful, even if the directory is empty.
  return filesystem_result<std::vector<path>>::ok (paths);
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::equivalent (path const &path1, path const &path2)
{
  return status (path1).value ().type () == status (path2).value ().type ()
         && file_size (path1).value () == file_size (path2).value ();
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::absolute (path const &path_arg)
{
  if (path_arg.is_absolute ())
    return path_arg;
  path base = current_path ().value ();
  return base / path_arg;
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::create_symlink (path const &target, path const &link)
{
#if LUMEX_OS_WINDOWS
  // On Windows, CreateSymbolicLinkA requires admin rights for files, and needs
  // to know if target is a dir
  DWORD attrs = GetFileAttributesA (target.c_str ());
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
  bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if (CreateSymbolicLinkA (link.c_str (), target.c_str (),
                           isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)
      != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  if (symlink (target.c_str (), link.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::create_directory_symlink (path const &target,
                                            path const &link)
{
#if LUMEX_OS_WINDOWS
  // Always create as directory symlink
  if (CreateSymbolicLinkA (link.c_str (), target.c_str (),
                           SYMBOLIC_LINK_FLAG_DIRECTORY)
      != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  if (symlink (target.c_str (), link.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::canonical (path const &path_arg)
{
#if LUMEX_OS_WINDOWS
  std::array<char, MAX_PATH> buf;
  DWORD len
      = GetFullPathNameA (path_arg.c_str (), MAX_PATH, buf.data (), nullptr);
  if (len == 0 || len > MAX_PATH)
    return path ();
  return path (buf.data ());
#else
  std::array<char, PATH_MAX> buf;
  if (realpath (path_arg.c_str (), buf.data ()) == nullptr)
    return path ();
  return path (buf.data ());
#endif
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::weakly_canonical (path const &path_arg)
{
  // Try canonical, fallback to absolute if fails
  path can = canonical (path_arg);
  if (!can.empty ())
    return can;
  return absolute (path_arg);
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::relative (path const &path_arg, path const &base)
{
  // Simple implementation: if p is absolute and starts with base, strip base
  path abs_p = absolute (path_arg);
  path abs_base = absolute (base);
  std::string pstr = abs_p.string ();
  std::string bstr = abs_base.string ();
#if LUMEX_OS_WINDOWS
  std::transform (
      pstr.begin (), pstr.end (), pstr.begin (),
      [] (unsigned char ch) { return static_cast<char> (::tolower (ch)); });
  std::transform (
      bstr.begin (), bstr.end (), bstr.begin (),
      [] (unsigned char ch) { return static_cast<char> (::tolower (ch)); });
#endif
  if (pstr.find (bstr) == 0
      && (pstr.size () == bstr.size () || pstr[bstr.size ()] == '/'
          || pstr[bstr.size ()] == '\\'))
    {
      std::string rel = pstr.substr (bstr.size ());
      while (!rel.empty () && (rel[0] == '/' || rel[0] == '\\'))
        rel.erase (0, 1);
      // Ensure forward slashes for cross-platform compatibility
      std::replace (rel.begin (), rel.end (), '\\', '/');
      return path (rel);
    }
  // Fallback: just return p
  return path_arg;
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::proximate (path const &path_arg, path const &base)
{
  // proximate: like relative, but if not possible, return p
  path rel = relative (path_arg, base);
  if (!rel.empty () && rel != path_arg)
    return rel;
  return path_arg;
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::rename (path const &from, path const &to_)
{
#if LUMEX_OS_WINDOWS
  if (MoveFileExA (from.c_str (), to_.c_str (), MOVEFILE_REPLACE_EXISTING)
      != 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
#else
  if (::rename (from.c_str (), to_.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::resize_file (path const &path_arg, std::uintmax_t new_size)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile = CreateFileA (path_arg.c_str (), GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return filesystem_result<void>::err (static_cast<int> (GetLastError ()));
  LARGE_INTEGER lInt;
  lInt.QuadPart = static_cast<LONGLONG> (new_size);
  if (SetFilePointerEx (hFile, lInt, nullptr, FILE_BEGIN) == 0
      || SetEndOfFile (hFile) == 0)
    {
      DWORD err = GetLastError ();
      CloseHandle (hFile);
      return filesystem_result<void>::err (static_cast<int> (err));
    }
  CloseHandle (hFile);
  return filesystem_result<void>::ok ();
#else
  if (truncate (path_arg.c_str (), static_cast<off_t> (new_size)) == 0)
    return filesystem_result<void>::ok ();
  return filesystem_result<void>::err (errno);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::move_file (path const &from, path const &to_path)
{
#if LUMEX_OS_WINDOWS
  // Try native MoveFileEx with overwrite
  if (MoveFileExA (from.c_str (), to_path.c_str (),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED
                       | MOVEFILE_WRITE_THROUGH)
      != 0)
    return filesystem_result<void>::ok ();
  DWORD err = GetLastError ();
  // ERROR_NOT_SAME_DEVICE -> fallback copy+remove
  if (err == ERROR_NOT_SAME_DEVICE)
    {
      filesystem_result<void> copy_result = copy_file (from, to_path);
      if (!copy_result)
        return copy_result;

      filesystem_result<bool> remove_result = remove (from);
      return remove_result.success ()
                 ? filesystem_result<void>::ok ()
                 : filesystem_result<void>::err (remove_result.error_code ());
    }
  return filesystem_result<void>::err (static_cast<int> (err));
#else
  // POSIX rename first
  if (::rename (from.c_str (), to_path.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  int err = errno;
  if (err == EXDEV) // cross-device link
    {
      filesystem_result<void> copy_result = copy_file (from, to_path);
      if (!copy_result)
        return copy_result;

      filesystem_result<bool> remove_result = remove (from);
      return remove_result.success ()
                 ? filesystem_result<void>::ok ()
                 : filesystem_result<void>::err (remove_result.error_code ());
    }
  return filesystem_result<void>::err (err);
#endif
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
lumex_filesystem::move_directory (path const &from, path const &to_path)
{
  // Basic validation
  if (!is_directory (from))
    return filesystem_result<void>::err (ENOTDIR);
  if (equivalent (from, to_path))
    return filesystem_result<void>::ok ();

  // If destination exists, ensure it is empty or remove it
  if (exists (to_path))
    {
      if (!is_directory (to_path))
        return filesystem_result<void>::err (EEXIST);
      filesystem_result<std::uintmax_t> rem = remove_all (to_path);
      if (!rem)
        return filesystem_result<void>::err (rem.error_code ());
    }

#if LUMEX_OS_WINDOWS
  if (MoveFileExA (from.c_str (), to_path.c_str (),
                   MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)
      != 0)
    return filesystem_result<void>::ok ();
  DWORD err = GetLastError ();
  if (err != ERROR_NOT_SAME_DEVICE)
    return filesystem_result<void>::err (static_cast<int> (err));
#else
  if (::rename (from.c_str (), to_path.c_str ()) == 0)
    return filesystem_result<void>::ok ();
  if (errno != EXDEV)
    return filesystem_result<void>::err (errno);
#endif
  // Cross-device: manual copy then remove
  filesystem_result<void> copy_result = copy (from, to_path);
  if (!copy_result)
    return copy_result;
  filesystem_result<std::uintmax_t> remove_result = remove_all (from);
  return remove_result.success ()
             ? filesystem_result<void>::ok ()
             : filesystem_result<void>::err (remove_result.error_code ());
}

LUMEX_PUBLIC_API
lumex::filesystem_result<lumex::path>
lumex_filesystem::temp_directory_path ()
{
#if LUMEX_OS_WINDOWS
  char buf[MAX_PATH];
  DWORD len = GetTempPathA (MAX_PATH, buf);
  if (len == 0 || len > MAX_PATH)
    return filesystem_result<path>::err (static_cast<int> (GetLastError ()),
                                         path ());
  return filesystem_result<path>::ok (path (buf));
#else
  const char *tmp = getenv ("TMPDIR");
  if (!tmp)
    tmp = "/tmp";
  return filesystem_result<path>::ok (path (tmp));
#endif
}

LUMEX_PUBLIC_API
std::wstring
lumex_filesystem::to_wide_string (std::string const &str)
{
#if LUMEX_OS_WINDOWS
  if (str.empty ())
    return {};
  int size_needed = MultiByteToWideChar (CP_UTF8, 0, str.c_str (),
                                         (int)str.size (), nullptr, 0);
  std::wstring wstr (size_needed, 0);
  MultiByteToWideChar (CP_UTF8, 0, str.c_str (), (int)str.size (), &wstr[0],
                       size_needed);
  return wstr;
#else
  if (str.empty ())
    return {};
  std::size_t len = mbstowcs (nullptr, str.c_str (), 0);
  if (len == (std::size_t)-1)
    return {};
  std::wstring wstr (len, 0);
  mbstowcs (&wstr[0], str.c_str (), len);
  return wstr;
#endif
}

LUMEX_PUBLIC_API
std::string
lumex_filesystem::from_wide_string (std::wstring const &wstr)
{
#if LUMEX_OS_WINDOWS
  if (wstr.empty ())
    return {};
  int size_needed
      = WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.size (),
                             nullptr, 0, nullptr, nullptr);
  std::string str (size_needed, 0);
  WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.size (), &str[0],
                       size_needed, nullptr, nullptr);
  return str;
#else
  if (wstr.empty ())
    return {};
  std::size_t len = wcstombs (nullptr, wstr.c_str (), 0);
  if (len == (std::size_t)-1)
    return {};
  std::string str (len, 0);
  wcstombs (&str[0], wstr.c_str (), len);
  return str;
#endif
}

LUMEX_PUBLIC_API
lumex::path
lumex_filesystem::get_exe_path ()
{
  try
    {
#if LUMEX_OS_WINDOWS
      std::array<wchar_t, MAX_PATH> buf{};
      DWORD len
          = GetModuleFileNameW (nullptr, buf.data (), (DWORD)buf.size ());
      if (len == 0 || len > buf.size ())
        return {};
      std::wstring wpath (buf.data (), len);
      std::string spath = from_wide_string (wpath);
      return path (spath);
#else
      std::array<char, PATH_MAX> buf{};
      ssize_t len
          = ::readlink ("/proc/self/exe", buf.data (), buf.size () - 1);
      buf[len < 0 ? 0 : len] = '\0';
      return path (buf.data ());
#endif
    }
  catch (std::exception const &exc)
    {
      std::cerr << "Can't get executable path, reason: " << exc.what ()
                << ", falling back on empty path\n";
      return {};
    }
  catch (...)
    {
      std::cerr << "Can't get executable path by unknown reason (non-standard "
                   "exception raised)\n";
      return {};
    }
}

LUMEX_PUBLIC_API
void
lumex_filesystem::lock_directory (lumex::path const &path_arg)
{
  try
    {
#if LUMEX_OS_WINDOWS
      static HANDLE s_dirHandle = INVALID_HANDLE_VALUE;
      s_dirHandle = CreateFileW (
          path_arg.wstring ().c_str (), GENERIC_READ,
          FILE_SHARE_READ | FILE_SHARE_WRITE, // without FILE_SHARE_DELETE
          nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      if (s_dirHandle == INVALID_HANDLE_VALUE)
        std::cerr << "Warning: cannot lock \"" << path_arg << "\" directory\n";
#else
      static int s_lockFd = -1;
      auto lockPath = path_arg / ".lock";
      s_lockFd = ::open (lockPath.string ().c_str (), O_CREAT | O_RDWR,
                         S_IRUSR | S_IWUSR);
      if (s_lockFd >= 0)
        {
          struct flock fl{};
          fl.l_type = F_WRLCK;
          fl.l_whence = SEEK_SET;
          fl.l_start = 0;
          fl.l_len = 0; // the whole file
          if (fcntl (s_lockFd, F_SETLK, &fl) == -1)
            std::cerr << "Warning: cannot lock \"" << path_arg
                      << "\" directory\n";
        }
#endif
    }
  catch (std::exception const &exc)
    {
      std::cerr << "Exception while using " << LUMEX_FUNC_NAME << "\n";
      std::cerr << "Reason: " << exc.what ()
                << ". This function done nothing.\n";
    }
  catch (...)
    {
      std::cerr << "Exception while using " << LUMEX_FUNC_NAME << "\n";
      std::cerr << "Reason: unknown (non-standard exception raised). This "
                   "function done nothing.\n";
    }
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_readable (lumex::path const &path_arg)
{
  try
    {
      if (!lumex_filesystem::exists (path_arg))
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" does not exist, returning false\n";
          return false;
        }
      filesystem_result<file_status> statusRes
          = lumex_filesystem::status (path_arg);
      if (!statusRes)
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" status() failed, returning false\n";
          return false;
        }
      perms file_perms = statusRes.value ().permissions ();
      bool hasRead = (file_perms & perms::owner_read) != perms::none
                     || (file_perms & perms::group_read) != perms::none
                     || (file_perms & perms::others_read) != perms::none;
      if (!hasRead)
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" does not have read permissions, returning false\n";
          return false;
        }
      std::ifstream ifs (path_arg.c_str ());
      if (!ifs.is_open () || ifs.fail ())
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" cannot be opened, returning false\n";
          return false;
        }
      return true;
    }
  catch (std::exception const &exc)
    {
      std::cerr << "Exception in is_readable: " << exc.what ()
                << ". Returning false.\n";
      return false;
    }
  catch (...)
    {
      std::cerr << "Unknown exception in is_readable. Returning false.\n";
      return false;
    }
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_writable (lumex::path const &path_arg)
{
  try
    {
      if (!lumex_filesystem::exists (path_arg))
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" does not exist, returning false\n";
          return false;
        }
      filesystem_result<file_status> statusRes
          = lumex_filesystem::status (path_arg);
      if (!statusRes)
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" status() failed, returning false\n";
          return false;
        }
      perms file_perms = statusRes.value ().permissions ();
      bool hasWrite = (file_perms & perms::owner_write) != perms::none
                      || (file_perms & perms::group_write) != perms::none
                      || (file_perms & perms::others_write) != perms::none;
      if (!hasWrite)
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" does not have write permissions, returning false\n";
          return false;
        }
      std::ofstream ofs (path_arg.c_str (), std::ios::in | std::ios::out);
      if (!ofs.is_open () || ofs.fail ())
        {
          std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path_arg
                    << "\" cannot be opened, returning false\n";
          return false;
        }
      return true;
    }
  catch (std::exception const &exc)
    {
      std::cerr << "Exception in is_writable: " << exc.what ()
                << ". Returning false.\n";
      return false;
    }
  catch (...)
    {
      std::cerr << "Unknown exception in is_writable. Returning false.\n";
      return false;
    }
}

LUMEX_PUBLIC_API
bool
lumex_filesystem::is_accessible (lumex::path const &path_arg)
{
  std::clog << "Checking if path_arg '" << path_arg
            << "' is accessible by the current "
               "process\n";
  return lumex_filesystem::is_readable (path_arg)
         && lumex_filesystem::is_writable (path_arg);
}

LUMEX_PUBLIC_API
inline bool
isFileExists (std::string const &path_arg)
{
  std::ifstream ifs (path_arg.c_str ());
  return ifs.good ();
}

LUMEX_PUBLIC_API
void
checkName (std::string const &name)
{
  using namespace Detail;

  if (name.empty ())
    throw std::invalid_argument (
        "Name of the file or directory cannot be empty");

  // Checking forbidden characters (predicate)
  for (char chr : name)
    if (Detail::isForbidden (chr))
      throw std::invalid_argument (
          "Name of the file contains forbidden character: "
          + std::string (1, chr));

  // Checking reserved names
  if (Detail::isReservedName (name))
    throw std::invalid_argument ("Name of the file is reserved by the system: "
                                 + name);

  // Checking invalid endings/beginnings
  if (Detail::hasInvalidEnding (name))
    throw std::invalid_argument (
        "Name of the file has invalid ending/beginning");
}

LUMEX_PUBLIC_API std::string
sanitizeName (
    std::string const &name, // NOLINT(bugprone-easily-swappable-parameters)
    std::string const &defaultValue) LUMEX_NOEXCEPT
{
  using namespace Detail;

  if (name.empty ())
    return defaultValue;

  try
    {
      checkName (name);
      return name; // Already valid
    }
  catch (...)
    {
      std::string sanitized = name;

      // Replacing invalid characters through predicate
      for (char &chr : sanitized)
        if (Detail::isForbidden (chr))
          chr = '_';

      // Handling reserved names
      if (Detail::isReservedName (sanitized))
        return defaultValue + "_file";

      // Cleaning invalid endings/beginnings
#if LUMEX_OS_WINDOWS
      while (!sanitized.empty ()
             && (sanitized.back () == '.' || sanitized.back () == ' '))
        sanitized.pop_back ();
#elif LUMEX_OS_LINUX
      if (!sanitized.empty () && sanitized.front () == '-')
        sanitized.erase (0, 1);
#endif

      // Removing consecutive underscores
      sanitized.erase (
#if __cplusplus >= 202002L
          std::ranges::unique (
              sanitized,
              [] (char chr1, char chr2) { return chr1 == '_' && chr2 == '_'; })
              .begin (),
          sanitized.end ()
#else
          std::unique (sanitized.begin (), sanitized.end (),
                       [] (char chr1, char chr2) {
                         return chr1 == '_' && chr2 == '_';
                       }),
          sanitized.end ()
#endif
      );

      // Removing leading/trailing underscores
      if (!sanitized.empty () && sanitized.front () == '_')
        sanitized.erase (0, 1);
      if (!sanitized.empty () && sanitized.back () == '_')
        sanitized.pop_back ();

      return sanitized.empty () ? defaultValue : sanitized;
    }
}
} // namespace fs
} // namespace filesystem
} // namespace core
} // namespace lumex

#define LUMEX_IMPLEMENTATION
#include "LumexFilesystem.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

#if LUMEX_OS_WINDOWS
  #include <shlwapi.h>
  #include <windows.h>
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

namespace Detail
{
#if LUMEX_OS_WINDOWS
  static constexpr char const *kForbiddenChars = "<>:\"/\\|*?";
  static std::array<std::string, 22> const kReservedNames
    = {"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
       "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  static constexpr size_t kReservedNamesCount = kReservedNames.size();

  static inline bool
  isForbidden(char chr) noexcept
  {
    return std::string(kForbiddenChars).find(chr) != std::string::npos;
  }

  static inline bool
  isReservedName(std::string const &name) noexcept
  {
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    for(size_t i = 0; i < kReservedNamesCount; ++i)
      if(upperName == kReservedNames.at(i)) return true;
    return false;
  }

  static inline bool
  hasInvalidEnding(std::string const &name) noexcept
  {
    return !name.empty() && (name.back() == '.' || name.back() == ' ');
  }

#elif LUMEX_OS_APPLE
  static constexpr char const *kForbiddenChars   = ":/";
  static constexpr char const *kProblematicChars = "*?|\"'";

  static inline bool
  isForbidden(char chr) noexcept
  {
    return std::string(kForbiddenChars).find(chr) != std::string::npos
           || std::string(kProblematicChars).find(chr) != std::string::npos;
  }

  static inline bool
  isReservedName(std::string const &name) noexcept
  {
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    static std::array<std::string, 22> const reservedNames
      = {"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
         "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    static constexpr size_t count = reservedNames.size();

    for(size_t i = 0; i < count; ++i)
      if(upperName == reservedNames.at(i)) return true;
    return false;
  }

  static inline bool
  hasInvalidEnding(std::string const &) noexcept
  {
    return false;
  }

#elif LUMEX_OS_LINUX
  static constexpr char const *kForbiddenChars   = "/";
  static constexpr char const *kProblematicChars = "&;|*?'\"`[]()$<>{}^#\\%!";

  static inline bool
  isForbidden(char chr) noexcept
  {
    return std::string(kForbiddenChars).find(chr) != std::string::npos
           || std::string(kProblematicChars).find(chr) != std::string::npos;
  }

  static inline bool
  isReservedName(std::string const & /*name*/) noexcept
  {
    return false;
  }

  static inline bool
  hasInvalidEnding(std::string const &name) noexcept
  {
    return !name.empty() && name.front() == '-';
  }

#else
  static constexpr char const *kForbiddenChars = "/<>:\"\\|*?";

  static inline bool
  isForbidden(char chr) noexcept
  {
    return std::string(kForbiddenChars).find(chr) != std::string::npos;
  }

  static inline bool
  isReservedName(std::string const &) noexcept
  {
    return false;
  }
  static inline bool
  hasInvalidEnding(std::string const &) noexcept
  {
    return false;
  }
#endif
} // namespace Detail

LUMEX_PUBLIC_API
Lumex::Path::Path(string_type source) : m_path(std::move(source)) {}

LUMEX_PUBLIC_API
bool
Lumex::Path::is_separator(value_type chr)
{
#if LUMEX_OS_WINDOWS
  return chr == '/' || chr == '\\';
#else
  return chr == '/';
#endif
}

LUMEX_PUBLIC_API
void
Lumex::Path::append_separator_if_needed()
{
  if(!m_path.empty() && !is_separator(m_path.back())) m_path += '/';
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator/=(Path const &path)
{
  if(path.empty() || path.m_path == ".") return *this;

  if(path.is_absolute())
  {
    m_path = path.m_path;
    return *this;
  }

  // Simplified handling for current directory "."
  if(m_path == ".")
  {
    m_path = path.m_path;
    return *this;
  }

  append_separator_if_needed();
  m_path += path.m_path;
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator/=(string_type const &path)
{
  return operator/=(Path(path));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator/=(char const *path)
{
  return operator/=(Path(path));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator+=(Path const &path)
{
  m_path += path.m_path;
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator+=(string_type const &path)
{
  m_path += path;
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator+=(char const *path)
{
  m_path += path != nullptr ? path : "";
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::operator+=(value_type chr)
{
  m_path += chr;
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::make_preferred()
{
  // Always use forward slashes for cross-platform compatibility
  std::replace(m_path.begin(), m_path.end(), '\\', '/');
  return *this;
}

LUMEX_PUBLIC_API
size_t
Lumex::Path::find_filename_pos() const
{
  if(m_path.empty()) return string_type::npos;

  size_t pos = m_path.find_last_of("/\\");
  if(static_cast<decltype(string_type::npos)>(pos) == string_type::npos) return 0;

  return pos + 1;
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::filename() const
{
  if(m_path.empty()) return Path();

  // Handle special cases
  if(m_path == "." || m_path == "..") return Path();

  // Handle paths ending with separator (directories)
  if(is_separator(m_path.back()))
  {
    // Find the last non-separator character
    size_t end = m_path.find_last_not_of("/\\");
    if(end == string_type::npos) return Path("/"); // Root path

    // Find the separator before the last component
    size_t start = m_path.find_last_of("/\\", end);
    if(start == string_type::npos) return Path(m_path.substr(0, end + 1));
    return Path(m_path.substr(start + 1, end - start));
  }

  size_t pos = find_filename_pos();
  if(static_cast<decltype(string_type::npos)>(pos) == string_type::npos) return Path();

  return Path(m_path.substr(pos));
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::parent_path() const
{
  if(m_path.empty() || m_path == "." || m_path == "..")
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  // Handle root paths
  if(m_path == "/" || m_path == "\\")
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  size_t pos = find_filename_pos();
  if(static_cast<decltype(string_type::npos)>(pos) == string_type::npos)
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  // If filename starts at position 0, it's a single file with no directory
  if(pos == 0)
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  // Remove trailing separator
  size_t end = pos - 1;
  while(end > 0 && is_separator(m_path[end])) --end;

  // If we're at the root, return empty
  if(end == 0 && is_separator(m_path[0]))
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  return Path(m_path.substr(0, end + 1));
}

LUMEX_PUBLIC_API
size_t
Lumex::Path::find_extension_pos() const
{
  size_t filename_pos = find_filename_pos();
  if(static_cast<decltype(string_type::npos)>(filename_pos) == string_type::npos) return string_type::npos;

  size_t dot_pos = m_path.find_last_of('.');
  if(static_cast<decltype(string_type::npos)>(dot_pos) == string_type::npos || dot_pos < filename_pos)
    return string_type::npos;

  // Don't count dot at beginning of filename
  if(dot_pos == filename_pos) return string_type::npos;

  return dot_pos;
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::extension() const
{
  size_t pos = find_extension_pos();
  if(static_cast<decltype(string_type::npos)>(pos) == string_type::npos)
  {
    // Return a path with empty string so empty() returns true
    Path result;
    result.m_path.clear();
    return result;
  }

  return Path(m_path.substr(pos));
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::stem() const
{
  size_t filename_pos = find_filename_pos();
  if(static_cast<decltype(string_type::npos)>(filename_pos) == string_type::npos) return Path();

  size_t ext_pos = find_extension_pos();
  if(static_cast<decltype(string_type::npos)>(ext_pos) == string_type::npos) return Path(m_path.substr(filename_pos));

  return Path(m_path.substr(filename_pos, ext_pos - filename_pos));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_extension(char const *ext)
{
  return replace_extension(Path(ext));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_extension(std::string const &ext)
{
  return replace_extension(Path(ext));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_extension(Path const &ext)
{
  size_t pos = find_extension_pos();
  if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos) m_path.erase(pos);

  if(!ext.empty() && ext.m_path != ".")
  {
    if(ext.m_path[0] != '.') m_path += '.';
    m_path += ext.m_path;
  }

  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::remove_filename()
{
  size_t pos = find_filename_pos();
  if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos && pos > 0)
  {
    // Remove the filename and any trailing separators
    m_path.erase(pos);
    // Remove trailing separators
    while(!m_path.empty() && is_separator(m_path.back()) && m_path != "/") m_path.pop_back();
    // If we end up empty, set to current directory
    if(m_path.empty()) m_path = ".";
  }
  else if(pos == 0)
  {
    // Single file with no directory - set to current directory
    m_path = ".";
  }
  return *this;
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_filename(char const *filename)
{
  return replace_filename(Path(filename));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_filename(std::string const &filename)
{
  return replace_filename(Path(filename));
}

LUMEX_PUBLIC_API
Lumex::Path &
Lumex::Path::replace_filename(Path const &replacement)
{
  remove_filename();
  return operator/=(replacement);
}

LUMEX_PUBLIC_API
bool
Lumex::Path::is_absolute() const
{
#if LUMEX_OS_WINDOWS
  // Check for drive letter (C:) or UNC path (\\server)
  if(m_path.length() >= 2)
  {
    // Non-zero value if the character is an alphabetic character, zero otherwise.
    // @link https://en.cppreference.com/w/cpp/string/byte/isalpha
    if(std::isalpha(m_path[0]) != 0 && m_path[1] == ':') return true;
    if(m_path[0] == '\\' && m_path[1] == '\\') return true;
  }
  // Also check for Unix-style absolute paths
  return !m_path.empty() && m_path[0] == '/';
#else
  return !m_path.empty() && m_path[0] == '/';
#endif
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_filename() const
{
  if(m_path.empty() || m_path == "." || m_path == "..") return false;

  size_t pos = find_filename_pos();
  return pos != string_type::npos && pos < m_path.length();
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_extension() const
{
  return static_cast<decltype(string_type::npos)>(find_extension_pos()) != string_type::npos;
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_parent_path() const
{
  return !parent_path().empty();
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_root_directory() const
{
#if LUMEX_OS_WINDOWS
  if(m_path.length() >= 3 && std::isalpha(m_path[0]) != 0 && m_path[1] == ':' && is_separator(m_path[2])) return true;
  if(m_path.length() >= 2 && m_path[0] == '\\' && m_path[1] == '\\') return true;
  return false;
#else
  return !m_path.empty() && m_path[0] == '/';
#endif
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::root_directory() const
{
  if(!has_root_directory()) return {};

#if LUMEX_OS_WINDOWS
  if(m_path.length() >= 3 && std::isalpha(m_path[0]) != 0 && m_path[1] == ':') return Path("\\");
  if(m_path.length() >= 2 && m_path[0] == '\\' && m_path[1] == '\\')
  {
    // Find next separator after \\server
    size_t pos = m_path.find_first_of("/\\", 2);
    if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos)
    {
      pos = m_path.find_first_of("/\\", pos + 1);
      if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos) return Path("\\");
    }
  }
  return {};
#else
  return Path("/");
#endif
}

LUMEX_PUBLIC_API
std::wstring
Lumex::Path::wstring() const
{
  return Lumex::Filesystem::to_wide_string(m_path);
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::root_name() const
{
#if LUMEX_OS_WINDOWS
  // Windows: root name is drive letter or UNC server/share
  if(m_path.size() >= 2 && std::isalpha(m_path[0]) != 0 && m_path[1] == ':') return Path(m_path.substr(0, 2));
  if(m_path.size() >= 2 && is_separator(m_path[0]) && is_separator(m_path[1]))
  {
    // UNC path: \\server\share\...
    size_t pos = m_path.find_first_of("/\\", 2);
    if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos)
    {
      pos = m_path.find_first_of("/\\", pos + 1);
      if(static_cast<decltype(string_type::npos)>(pos) != string_type::npos) return Path(m_path.substr(0, pos));
    }
  }
  return {};
#else
  // POSIX: no root name
  return {};
#endif
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::root_path() const
{
  Path rnp = root_name();
  Path rdp = root_directory();
  if(!rnp.empty() && !rdp.empty()) return Path(rnp.string() + rdp.string());
  if(!rnp.empty()) return rnp;
  if(!rdp.empty()) return rdp;
  return {};
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Path::relative_path() const
{
  Path rpp = root_path();
  if(rpp.empty()) return *this;
  return Path(m_path.substr(rpp.string().length()));
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_root_name() const
{
  return !root_name().empty();
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_root_path() const
{
  return !root_path().empty();
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_relative_path() const
{
  return !relative_path().empty();
}

LUMEX_PUBLIC_API
bool
Lumex::Path::has_stem() const
{
  return !stem().empty();
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::exists(Path const &path)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES;
#else
  struct stat stt;
  return stat(path.c_str(), std::addressof(stt)) == 0;
#endif
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_directory(Path const &path)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  struct stat st;
  if(stat(path.c_str(), &st) != 0) return false;
  return S_ISDIR(st.st_mode);
#endif
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_regular_file(Path const &path)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0
         && (attrs & FILE_ATTRIBUTE_DEVICE) == 0;
#else
  struct stat stt;
  if(stat(path.c_str(), std::addressof(stt)) != 0) return false;
  return S_ISREG(stt.st_mode);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<bool>
Lumex::Filesystem::create_directory(Path const &path)
{
#if LUMEX_OS_WINDOWS
  if(CreateDirectoryA(path.c_str(), nullptr) != 0) return FilesystemResult<bool>::ok(true);
  DWORD error = GetLastError();
  if(error == ERROR_ALREADY_EXISTS && is_directory(path)) return FilesystemResult<bool>::ok(false);
  return FilesystemResult<bool>::err(static_cast<int>(error), false);
#else
  if(mkdir(path.c_str(), 0755) == 0) return FilesystemResult<bool>::ok(true);
  if(errno == EEXIST && is_directory(path)) return FilesystemResult<bool>::ok(false);
  return FilesystemResult<bool>::err(errno, false);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::Path>
Lumex::Filesystem::current_path()
{
#if LUMEX_OS_WINDOWS
  std::array<char, MAX_PATH> buffer;
  DWORD result = GetCurrentDirectoryA(MAX_PATH, buffer.data());
  if(result == 0 || result > MAX_PATH) return FilesystemResult<Path>::err(static_cast<int>(GetLastError()), Path());
  return FilesystemResult<Path>::ok(Path(buffer.data()));
#else
  std::array<char, PATH_MAX> buffer;
  if(getcwd(buffer.data(), PATH_MAX) != nullptr) return FilesystemResult<Path>::ok(Path(buffer.data()));
  return FilesystemResult<Path>::err(errno, Path());
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::current_path(Path const &path)
{
#if LUMEX_OS_WINDOWS
  if(SetCurrentDirectoryA(path.c_str()) != 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  if(chdir(path.c_str()) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

namespace Lumex
{
  namespace Core
  {
    namespace Filesystem
    {
      namespace detail
      {
// ---------- Additional filesystem helpers (private) ------------
#if LUMEX_OS_WINDOWS
        // Renamed and updated to apply permissions rather than just convert
        static DWORD
        apply_perms_to_windows_attributes(DWORD existing_attrs, Lumex::Perms prms)
        {
          DWORD new_attrs = existing_attrs;
          if((prms & Lumex::Perms::owner_write) == Lumex::Perms::none)
            new_attrs |= FILE_ATTRIBUTE_READONLY; // If write not allowed, set readonly
          else
            new_attrs &= ~FILE_ATTRIBUTE_READONLY; // If write allowed, ensure not readonly
          return new_attrs;
        }
#else
        static mode_t
        perms_to_posix_mode(Lumex::Perms prms)
        {
          return static_cast<mode_t>(prms & Lumex::Perms::mask);
        }
#endif
      } // namespace detail
    } // namespace Filesystem
  } // namespace Core
} // namespace Lumex

// ---------------- Low-level status helpers --------------------
#if LUMEX_OS_WINDOWS
LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::FileStatus>
Lumex::Filesystem::get_file_status_windows(Path const &path, bool follow)
{
  WIN32_FILE_ATTRIBUTE_DATA data;
  BOOL ok_ = follow ? GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, std::addressof(data))
                    : GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, std::addressof(data));
  if(!static_cast<bool>(ok_))
    return FilesystemResult<FileStatus>::err(static_cast<int>(GetLastError()), FileStatus(FileType::not_found));

  DWORD tmp     = data.dwFileAttributes;
  FileType type = FileType::unknown;
  if((tmp & FILE_ATTRIBUTE_DIRECTORY) != 0U)
    type = FileType::directory;
  else if((tmp & FILE_ATTRIBUTE_REPARSE_POINT) != 0U)
    type = FileType::symlink;
  else
    type = FileType::regular;

  Perms perms = ((tmp & FILE_ATTRIBUTE_READONLY) != 0U) ? (Perms::owner_read | Perms::group_read | Perms::others_read)
                                                        : Perms::all;
  return FilesystemResult<FileStatus>::ok(FileStatus(type, perms));
}
#else
LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::FileStatus>
Lumex::Filesystem::get_file_status_posix(Path const &path, bool follow)
{
  struct stat stt;
  int res = follow ? stat(path.c_str(), std::addressof(stt)) : lstat(path.c_str(), std::addressof(stt));
  if(res != 0) return FilesystemResult<FileStatus>::err(errno, FileStatus(FileType::not_found));
  FileType type = FileType::unknown;
  if(S_ISREG(stt.st_mode))
    type = FileType::regular;
  else if(S_ISDIR(stt.st_mode))
    type = FileType::directory;
  else if(S_ISLNK(stt.st_mode))
    type = FileType::symlink;
  else if(S_ISCHR(stt.st_mode))
    type = FileType::character;
  else if(S_ISBLK(stt.st_mode))
    type = FileType::block;
  else if(S_ISFIFO(stt.st_mode))
    type = FileType::fifo;
  else if(S_ISSOCK(stt.st_mode))
    type = FileType::socket;

  auto perms = static_cast<Perms>(stt.st_mode & 07777);
  return FilesystemResult<FileStatus>::ok(FileStatus(type, perms));
}
#endif

// ---------------- public status helpers -----------------------
LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::FileStatus>
Lumex::Filesystem::status(Path const &path)
{
#if LUMEX_OS_WINDOWS
  return get_file_status_windows(path, true);
#else
  return get_file_status_posix(path, true);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::FileStatus>
Lumex::Filesystem::symlink_status(Path const &path)
{
#if LUMEX_OS_WINDOWS
  return get_file_status_windows(path, false);
#else
  return get_file_status_posix(path, false);
#endif
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_symlink(Path const &path)
{
  return symlink_status(path).value().type() == FileType::symlink;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_block_file(Path const &path)
{
  return status(path).value().type() == FileType::block;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_character_file(Path const &path)
{
  return status(path).value().type() == FileType::character;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_fifo(Path const &path)
{
  return status(path).value().type() == FileType::fifo;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_socket(Path const &path)
{
  return status(path).value().type() == FileType::socket;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_other(Path const &path)
{
  FileType fType = status(path).value().type();
  return fType != FileType::none && fType != FileType::not_found && fType != FileType::regular
         && fType != FileType::directory;
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_empty(Path const &path)
{
  if(!exists(path)) return false;
  if(is_directory(path))
  {
    DirectoryIterator iter(path);
    return iter == DirectoryIterator();
  }
  FilesystemResult<std::uintmax_t> fsRes = file_size(path);
  return fsRes.success() && fsRes.value() == 0;
}

// ---------------- copy operations -----------------------------
LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::copy(Path const &from, Path const &to_)
{
  // Step 1: Check if the source 'from' is a directory.
  if(is_directory(from))
  {
    // Step 2 (Recursive case - if 'from' is a directory):
    // Try to create the target directory 'to_'.
    FilesystemResult<bool> created = create_directory(to_);

    // If creation fails and the error is not 'already exists', return the
    // error.
    if(!created && created.error_code() != 0) return FilesystemResult<void>::err(created.error_code());

    // Get all entries (files and subdirectories) within the source directory.
    std::vector<DirectoryEntry> entries = directory_contents(from);

    // Iterate over each entry in the source directory.
    for(DirectoryEntry const &entry : entries)
    {
      // Form the new source path for the current entry.
      Path new_from = entry.path();

      // Form the new target path for the current entry within the destination.
      Path new_to = to_ / entry.path().filename();

      // Recursively call 'copy' for the current entry.
      FilesystemResult<void> res = copy(new_from, new_to);

      // If the recursive copy fails, propagate the error up.
      if(!res) return res;
    }
    // If all entries were successfully copied, return success.
    return FilesystemResult<void>::ok();
  }
  // Step 3 (Non-recursive case - if 'from' is a regular file):
  // Directly copy the file using 'copy_file' function.
  // The result (success or error) of 'copy_file' is returned.
  return copy_file(from, to_);
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::copy_file(Path const &from, Path const &to_)
{
#if LUMEX_OS_WINDOWS
  if(CopyFileA(from.c_str(), to_.c_str(), FALSE) != 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  std::size_t const bufSize = 16384;
  char buffer[bufSize];
  int inFd = open(from.c_str(), O_RDONLY);
  if(inFd < 0) return FilesystemResult<void>::err(errno);
  int outFd = open(to_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if(outFd < 0)
  {
    close(inFd);
    return FilesystemResult<void>::err(errno);
  }
  ssize_t r;
  while((r = read(inFd, buffer, bufSize)) > 0)
  {
    ssize_t w = write(outFd, buffer, static_cast<size_t>(r));
    if(w != r)
    {
      close(inFd);
      close(outFd);
      return FilesystemResult<void>::err(errno);
    }
  }
  close(inFd);
  close(outFd);
  if(r < 0) return FilesystemResult<void>::err(errno);
  return FilesystemResult<void>::ok();
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::copy_symlink(Path const &from, Path const &to_)
{
#if LUMEX_OS_WINDOWS
  // Windows requires knowing if link is file or dir
  DWORD attrs = GetFileAttributesA(from.c_str());
  if(attrs == INVALID_FILE_ATTRIBUTES) return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
  bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if(CreateSymbolicLinkA(to_.c_str(), from.c_str(), isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) != 0)
    return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  if(symlink(from.c_str(), to_.c_str()) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

// ---------------- directory helpers ---------------------------
LUMEX_PUBLIC_API
Lumex::FilesystemResult<bool>
Lumex::Filesystem::create_directories(Path const &path)
{
  if(path.empty()) return FilesystemResult<bool>::ok(false);
  if(exists(path)) return FilesystemResult<bool>::ok(false);
  Path parent = path.parent_path();
  if(!parent.empty())
  {
    FilesystemResult<bool> res = create_directories(parent);
    if(!res && res.error_code() != 0) return FilesystemResult<bool>::err(res.error_code(), false);
  }
  return create_directory(path);
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<bool>
Lumex::Filesystem::remove(Path const &path)
{
  // If file doesn't exist, return success with false
  if(!exists(path)) return FilesystemResult<bool>::ok(false);

#if LUMEX_OS_WINDOWS
  if(is_directory(path))
  {
    if(RemoveDirectoryA(path.c_str()) != 0) return FilesystemResult<bool>::ok(true);
  }
  else
  {
    if(DeleteFileA(path.c_str()) != 0) return FilesystemResult<bool>::ok(true);
  }
  // Return false with the last Windows error if removal failed.
  return FilesystemResult<bool>::err(static_cast<int>(GetLastError()), false);
#else
  if(is_directory(path))
  {
    if(rmdir(path.c_str()) == 0) return FilesystemResult<bool>::ok(true);
  }
  else
  {
    if(unlink(path.c_str()) == 0) return FilesystemResult<bool>::ok(true);
  }
  // Return false with the errno if removal failed.
  return FilesystemResult<bool>::err(errno, false);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<std::uintmax_t>
Lumex::Filesystem::remove_all(Path const &path)
{
  if(!exists(path)) return FilesystemResult<std::uintmax_t>::ok(static_cast<std::uintmax_t>(0));
  std::uintmax_t count = 0;
  if(is_directory(path))
  {
    for(DirectoryEntry const &dirEntry : directory_contents(path))
    {
      FilesystemResult<std::uintmax_t> sub = remove_all(dirEntry.path());
      if(!sub) return sub;
      count += sub.value();
    }
  }
  FilesystemResult<bool> self = remove(path);
  if(!self) return FilesystemResult<std::uintmax_t>::err(self.error_code(), count);
  return FilesystemResult<std::uintmax_t>::ok(count + 1);
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<std::uintmax_t>
Lumex::Filesystem::file_size(Path const &path)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile
    = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) return FilesystemResult<std::uintmax_t>::err(static_cast<int>(GetLastError()), 0);
  LARGE_INTEGER size;
  if(GetFileSizeEx(hFile, std::addressof(size)) == 0)
  {
    DWORD err = GetLastError();
    CloseHandle(hFile);
    return FilesystemResult<std::uintmax_t>::err(static_cast<int>(err), 0);
  }
  CloseHandle(hFile);
  return FilesystemResult<std::uintmax_t>::ok(static_cast<std::uintmax_t>(size.QuadPart));
#else
  struct stat stt;
  if(stat(path.c_str(), std::addressof(stt)) != 0) return FilesystemResult<std::uintmax_t>::err(errno, 0);

  return FilesystemResult<std::uintmax_t>::ok(static_cast<std::uintmax_t>(stt.st_size));
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<std::time_t>
Lumex::Filesystem::last_write_time(Path const &path)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile
    = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) return FilesystemResult<std::time_t>::err(static_cast<int>(GetLastError()), 0);
  FILETIME fTime;
  if(GetFileTime(hFile, nullptr, nullptr, std::addressof(fTime)) == 0)
  {
    DWORD err = GetLastError();
    CloseHandle(hFile);
    return FilesystemResult<std::time_t>::err(static_cast<int>(err), 0);
  }
  CloseHandle(hFile);
  ULARGE_INTEGER ulInt;
  ulInt.LowPart   = fTime.dwLowDateTime;
  ulInt.HighPart  = fTime.dwHighDateTime;
  auto timeStruct = static_cast<std::time_t>((ulInt.QuadPart - KDEFAULT_WINDOWS_FILETIME_TO_UNIX_EPOCH_INTERVALS)
                                             / KDEFAULT_HUNDRED_NANOSECONDS_PER_SECOND);
  return FilesystemResult<std::time_t>::ok(timeStruct);
#else
  struct stat stt;
  if(stat(path.c_str(), std::addressof(stt)) != 0) return FilesystemResult<std::time_t>::err(errno, 0);

  return FilesystemResult<std::time_t>::ok(stt.st_mtime);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::last_write_time(Path const &path, std::time_t new_time)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
  ULARGE_INTEGER ulInt;
  ulInt.QuadPart = (static_cast<unsigned long long>(new_time) * KDEFAULT_HUNDRED_NANOSECONDS_PER_SECOND)
                   + KDEFAULT_WINDOWS_FILETIME_TO_UNIX_EPOCH_INTERVALS;
  FILETIME fTime;
  fTime.dwLowDateTime  = ulInt.LowPart;
  fTime.dwHighDateTime = ulInt.HighPart;
  BOOL ok_             = SetFileTime(hFile, nullptr, nullptr, std::addressof(fTime));
  DWORD err            = (ok_ != 0) ? 0 : GetLastError();
  CloseHandle(hFile);
  return (ok_ != 0) ? FilesystemResult<void>::ok() : FilesystemResult<void>::err(static_cast<int>(err));
#else
  struct utimbuf buf;
  buf.actime  = new_time;
  buf.modtime = new_time;
  if(utime(path.c_str(), std::addressof(buf)) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::permissions(Path const &path, Perms prms)
{
#if LUMEX_OS_WINDOWS
  DWORD attrs = GetFileAttributesA(path.c_str());
  if(attrs == INVALID_FILE_ATTRIBUTES) return FilesystemResult<void>::err(static_cast<int>(GetLastError()));

  DWORD new_attrs = detail::apply_perms_to_windows_attributes(attrs, prms);

  if(SetFileAttributesA(path.c_str(), new_attrs) != 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  mode_t mode = detail::perms_to_posix_mode(prms);
  if(chmod(path.c_str(), mode) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::Path>
Lumex::Filesystem::read_symlink(Path const &path)
{
#if LUMEX_OS_WINDOWS
  std::vector<char> buf(MAX_PATH);
  DWORD len = GetFinalPathNameByHandleA(
    CreateFileA(path.c_str(), 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr), buf.data(), MAX_PATH,
    FILE_NAME_NORMALIZED);
  if(len == 0) return FilesystemResult<Path>::err(static_cast<int>(GetLastError()), Path());
  return FilesystemResult<Path>::ok(Path(std::string(buf.data(), len)));
#else
  std::vector<char> buf(PATH_MAX);
  ssize_t len = readlink(path.c_str(), buf.data(), buf.size() - 1);
  if(len < 0) return FilesystemResult<Path>::err(errno, Path());
  buf[len] = 0;
  return FilesystemResult<Path>::ok(Path(buf.data()));
#endif
}

// --------- space info & temp directory -----------------
LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::SpaceInfo>
Lumex::Filesystem::space(Path const &path)
{
#if LUMEX_OS_WINDOWS
  ULARGE_INTEGER freeBytesAvailable;
  ULARGE_INTEGER totalNumberOfBytes;
  ULARGE_INTEGER totalNumberOfFreeBytes;

  if(GetDiskFreeSpaceExA(path.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes) == 0)
    return FilesystemResult<SpaceInfo>::err(static_cast<int>(GetLastError()), SpaceInfo());
  SpaceInfo info;
  info.available = static_cast<std::uintmax_t>(freeBytesAvailable.QuadPart);
  info.free      = static_cast<std::uintmax_t>(totalNumberOfFreeBytes.QuadPart);
  info.capacity  = static_cast<std::uintmax_t>(totalNumberOfBytes.QuadPart);
  return FilesystemResult<SpaceInfo>::ok(info);
#else
  struct statvfs vfs;
  if(statvfs(path.c_str(), std::addressof(vfs)) != 0) return FilesystemResult<SpaceInfo>::err(errno, SpaceInfo());
  SpaceInfo info;
  info.available = static_cast<std::uintmax_t>(vfs.f_bavail) * vfs.f_frsize;
  info.free      = static_cast<std::uintmax_t>(vfs.f_bfree) * vfs.f_frsize;
  info.capacity  = static_cast<std::uintmax_t>(vfs.f_blocks) * vfs.f_frsize;
  return FilesystemResult<SpaceInfo>::ok(info);
#endif
}

// -------------- directory listing ----------------------
class Lumex::DirectoryIterator::Impl
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
  Path base;
  DirectoryEntry current;
};

LUMEX_PUBLIC_API
Lumex::DirectoryIterator::DirectoryIterator(Path const &path) : m_impl(new Impl)
{
  m_impl->base = path;
#if LUMEX_OS_WINDOWS
  std::string pattern = path.string();
  if(!pattern.empty() && pattern.back() != '/')
    pattern += "/*";
  else
    pattern += "*";
  m_impl->handle = FindFirstFileA(pattern.c_str(), &m_impl->data);
  m_impl->first  = true;
  if(m_impl->handle == INVALID_HANDLE_VALUE) m_impl.reset();
#else
  m_impl->dirp  = opendir(path.c_str());
  m_impl->entry = nullptr;
  if(!m_impl->dirp) m_impl.reset();
#endif
  operator++();
}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator::DirectoryIterator(DirectoryIterator const &other) : m_impl(other.m_impl) {}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator &
Lumex::DirectoryIterator::operator=(DirectoryIterator const &other)
{
  m_impl = other.m_impl;
  return *this;
}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator::DirectoryIterator(DirectoryIterator &&other) noexcept : m_impl(std::move(other.m_impl)) {}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator &
Lumex::DirectoryIterator::operator=(DirectoryIterator &&other) noexcept
{
  m_impl = std::move(other.m_impl);
  return *this;
}

LUMEX_PUBLIC_API Lumex::DirectoryIterator::reference
Lumex::DirectoryIterator::operator*() const
{
  return m_impl->current;
}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator::pointer
Lumex::DirectoryIterator::operator->() const
{
  return &m_impl->current;
}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator &
Lumex::DirectoryIterator::operator++()
{
  if(!m_impl) return *this;
#if LUMEX_OS_WINDOWS
  BOOL ok_;
  if(m_impl->first)
  {
    ok_           = TRUE;
    m_impl->first = false;
  }
  else { ok_ = FindNextFileA(m_impl->handle, &m_impl->data); }
  while(ok_ != 0)
  {
    std::string name(m_impl->data.cFileName);
    if(name != "." && name != "..")
    {
      m_impl->current = DirectoryEntry(m_impl->base / Path(name));
      return *this;
    }
    ok_ = FindNextFileA(m_impl->handle, &m_impl->data);
  }
  FindClose(m_impl->handle);
  m_impl.reset();
#else
  while((m_impl->entry = readdir(m_impl->dirp)) != nullptr)
  {
    std::string name(m_impl->entry->d_name);
    if(name != "." && name != "..")
    {
      m_impl->current = DirectoryEntry(m_impl->base / Path(name));
      return *this;
    }
  }
  closedir(m_impl->dirp);
  m_impl.reset();
#endif
  return *this;
}

LUMEX_PUBLIC_API
Lumex::DirectoryIterator
Lumex::DirectoryIterator::operator++(int)
{
  Lumex::DirectoryIterator tmp(*this);
  ++*this;
  return tmp;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryIterator::operator==(DirectoryIterator const &rhs) const
{
  return m_impl == rhs.m_impl;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryIterator::operator!=(DirectoryIterator const &rhs) const
{
  return !(*this == rhs);
}

// DirectoryEntry simple wrappers
LUMEX_PUBLIC_API
Lumex::DirectoryEntry::DirectoryEntry(Path const &path) : m_path(path), m_status_known(false) {}

LUMEX_PUBLIC_API
void
Lumex::DirectoryEntry::refresh_status() const
{
  m_status       = LumexFilesystem::status(m_path).value();
  m_status_known = true;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::exists() const
{
  return LumexFilesystem::exists(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_regular_file() const
{
  return LumexFilesystem::is_regular_file(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_directory() const
{
  return LumexFilesystem::is_directory(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_symlink() const
{
  return LumexFilesystem::is_symlink(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_block_file() const
{
  return LumexFilesystem::is_block_file(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_character_file() const
{
  return LumexFilesystem::is_character_file(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_fifo() const
{
  return LumexFilesystem::is_fifo(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_socket() const
{
  return LumexFilesystem::is_socket(m_path);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::is_other() const
{
  return LumexFilesystem::is_other(m_path);
}

LUMEX_PUBLIC_API
std::uintmax_t
Lumex::DirectoryEntry::file_size() const
{
  return LumexFilesystem::file_size(m_path).value();
}

LUMEX_PUBLIC_API
Lumex::FileStatus
Lumex::DirectoryEntry::status() const
{
  return LumexFilesystem::status(m_path).value();
}

LUMEX_PUBLIC_API
Lumex::FileStatus
Lumex::DirectoryEntry::symlink_status() const
{
  return LumexFilesystem::symlink_status(m_path).value();
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator==(DirectoryEntry const &rhs) const
{
  return m_path == rhs.m_path;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator!=(DirectoryEntry const &rhs) const
{
  return !(*this == rhs);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator<(DirectoryEntry const &rhs) const
{
  return m_path < rhs.m_path;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator<=(DirectoryEntry const &rhs) const
{
  return !(rhs < *this);
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator>(DirectoryEntry const &rhs) const
{
  return rhs < *this;
}

LUMEX_PUBLIC_API
bool
Lumex::DirectoryEntry::operator>=(DirectoryEntry const &rhs) const
{
  return !(*this < rhs);
}

LUMEX_PUBLIC_API
std::vector<Lumex::DirectoryEntry>
Lumex::Filesystem::directory_contents(Path const &path)
{
  std::vector<Lumex::DirectoryEntry> out;
  for(DirectoryIterator it(path); it != DirectoryIterator(); ++it) out.push_back(*it);
  return out;
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<std::vector<Lumex::Path>>
Lumex::Filesystem::directory_paths(Path const &path)
{
  std::vector<Lumex::Path> paths;

  // IMPORTANT FIX(Test: Filesystem_DirectoryPaths_PathIsFile):
  // Ensure the path exists and is a directory BEFORE iterating.
  // The original logic was flawed because directory_iterator might not immediately
  // set errno or throw for non-directory paths, leading to an empty 'paths'
  // vector that then incorrectly passes the `if(!paths.empty() || LumexFilesystem::exists(path))` check.
  if(!LumexFilesystem::exists(path))
    return FilesystemResult<std::vector<Path>>::err(ENOENT, {}); // No such file or directory

  if(!LumexFilesystem::is_directory(path))
  {
    // If it exists but is not a directory, return an error like ENOTDIR (Not a directory).
    // On Windows, the equivalent might be different, but a non-zero error is expected.
    return FilesystemResult<std::vector<Path>>::err(ENOTDIR, {});
  }

  // Use directory_contents to get entries, then extract paths
  // If the path is a valid directory, the iterator should work.
  for(DirectoryIterator it(path); it != DirectoryIterator(); ++it) paths.push_back(it->path());

  // If we reach here, the path exists and is a directory.
  // The result should always be successful, even if the directory is empty.
  return FilesystemResult<std::vector<Path>>::ok(paths);
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::equivalent(Path const &path1, Path const &path2)
{
  return status(path1).value().type() == status(path2).value().type()
         && file_size(path1).value() == file_size(path2).value();
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::absolute(Path const &path)
{
  if(path.is_absolute()) return path;
  Path base = current_path().value();
  return base / path;
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::create_symlink(Path const &target, Path const &link)
{
#if LUMEX_OS_WINDOWS
  // On Windows, CreateSymbolicLinkA requires admin rights for files, and needs
  // to know if target is a dir
  DWORD attrs = GetFileAttributesA(target.c_str());
  if(attrs == INVALID_FILE_ATTRIBUTES) return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
  bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if(CreateSymbolicLinkA(link.c_str(), target.c_str(), isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) != 0)
    return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  if(symlink(target.c_str(), link.c_str()) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::create_directory_symlink(Path const &target, Path const &link)
{
#if LUMEX_OS_WINDOWS
  // Always create as directory symlink
  if(CreateSymbolicLinkA(link.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY) != 0)
    return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  if(symlink(target.c_str(), link.c_str()) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::canonical(Path const &path)
{
#if LUMEX_OS_WINDOWS
  std::array<char, MAX_PATH> buf;
  DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, buf.data(), nullptr);
  if(len == 0 || len > MAX_PATH) return Path();
  return Path(buf.data());
#else
  std::array<char, PATH_MAX> buf;
  if(realpath(path.c_str(), buf.data()) == nullptr) return Path();
  return Path(buf.data());
#endif
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::weakly_canonical(Path const &path)
{
  // Try canonical, fallback to absolute if fails
  Path can = canonical(path);
  if(!can.empty()) return can;
  return absolute(path);
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::relative(Path const &path, Path const &base)
{
  // Simple implementation: if p is absolute and starts with base, strip base
  Path abs_p       = absolute(path);
  Path abs_base    = absolute(base);
  std::string pstr = abs_p.string();
  std::string bstr = abs_base.string();
#if LUMEX_OS_WINDOWS
  std::transform(pstr.begin(), pstr.end(), pstr.begin(), ::tolower);
  std::transform(bstr.begin(), bstr.end(), bstr.begin(), ::tolower);
#endif
  if(pstr.find(bstr) == 0 && (pstr.size() == bstr.size() || pstr[bstr.size()] == '/' || pstr[bstr.size()] == '\\'))
  {
    std::string rel = pstr.substr(bstr.size());
    while(!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel.erase(0, 1);
    // Ensure forward slashes for cross-platform compatibility
    std::replace(rel.begin(), rel.end(), '\\', '/');
    return Path(rel);
  }
  // Fallback: just return p
  return path;
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::proximate(Path const &path, Path const &base)
{
  // proximate: like relative, but if not possible, return p
  Path rel = relative(path, base);
  if(!rel.empty() && rel != path) return rel;
  return path;
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::rename(Path const &from, Path const &to_)
{
#if LUMEX_OS_WINDOWS
  if(MoveFileExA(from.c_str(), to_.c_str(), MOVEFILE_REPLACE_EXISTING) != 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
#else
  if(::rename(from.c_str(), to_.c_str()) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::resize_file(Path const &path, std::uintmax_t new_size)
{
#if LUMEX_OS_WINDOWS
  HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) return FilesystemResult<void>::err(static_cast<int>(GetLastError()));
  LARGE_INTEGER lInt;
  lInt.QuadPart = static_cast<LONGLONG>(new_size);
  if(SetFilePointerEx(hFile, lInt, nullptr, FILE_BEGIN) == 0 || SetEndOfFile(hFile) == 0)
  {
    DWORD err = GetLastError();
    CloseHandle(hFile);
    return FilesystemResult<void>::err(static_cast<int>(err));
  }
  CloseHandle(hFile);
  return FilesystemResult<void>::ok();
#else
  if(truncate(path.c_str(), static_cast<off_t>(new_size)) == 0) return FilesystemResult<void>::ok();
  return FilesystemResult<void>::err(errno);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::move_file(Path const &from, Path const &to_path)
{
#if LUMEX_OS_WINDOWS
  // Try native MoveFileEx with overwrite
  if(MoveFileExA(from.c_str(), to_path.c_str(),
                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)
     != 0)
    return FilesystemResult<void>::ok();
  DWORD err = GetLastError();
  // ERROR_NOT_SAME_DEVICE -> fallback copy+remove
  if(err == ERROR_NOT_SAME_DEVICE)
  {
    FilesystemResult<void> copy_result = copy_file(from, to_path);
    if(!copy_result) return copy_result;

    FilesystemResult<bool> remove_result = remove(from);
    return remove_result.success() ? FilesystemResult<void>::ok()
                                   : FilesystemResult<void>::err(remove_result.error_code());
  }
  return FilesystemResult<void>::err(static_cast<int>(err));
#else
  // POSIX rename first
  if(::rename(from.c_str(), to_path.c_str()) == 0) return FilesystemResult<void>::ok();
  int err = errno;
  if(err == EXDEV) // cross-device link
  {
    FilesystemResult<void> copy_result = copy_file(from, to_path);
    if(!copy_result) return copy_result;

    FilesystemResult<bool> remove_result = remove(from);
    return remove_result.success() ? FilesystemResult<void>::ok()
                                   : FilesystemResult<void>::err(remove_result.error_code());
  }
  return FilesystemResult<void>::err(err);
#endif
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
Lumex::Filesystem::move_directory(Path const &from, Path const &to_path)
{
  // Basic validation
  if(!is_directory(from)) return FilesystemResult<void>::err(ENOTDIR);
  if(equivalent(from, to_path)) return FilesystemResult<void>::ok();

  // If destination exists, ensure it is empty or remove it
  if(exists(to_path))
  {
    if(!is_directory(to_path)) return FilesystemResult<void>::err(EEXIST);
    FilesystemResult<std::uintmax_t> rem = remove_all(to_path);
    if(!rem) return FilesystemResult<void>::err(rem.error_code());
  }

#if LUMEX_OS_WINDOWS
  if(MoveFileExA(from.c_str(), to_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) != 0)
    return FilesystemResult<void>::ok();
  DWORD err = GetLastError();
  if(err != ERROR_NOT_SAME_DEVICE) return FilesystemResult<void>::err(static_cast<int>(err));
#else
  if(::rename(from.c_str(), to_path.c_str()) == 0) return FilesystemResult<void>::ok();
  if(errno != EXDEV) return FilesystemResult<void>::err(errno);
#endif
  // Cross-device: manual copy then remove
  FilesystemResult<void> copy_result = copy(from, to_path);
  if(!copy_result) return copy_result;
  FilesystemResult<std::uintmax_t> remove_result = remove_all(from);
  return remove_result.success() ? FilesystemResult<void>::ok()
                                 : FilesystemResult<void>::err(remove_result.error_code());
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<Lumex::Path>
Lumex::Filesystem::temp_directory_path()
{
#if LUMEX_OS_WINDOWS
  char buf[MAX_PATH];
  DWORD len = GetTempPathA(MAX_PATH, buf);
  if(len == 0 || len > MAX_PATH) return FilesystemResult<Path>::err(static_cast<int>(GetLastError()), Path());
  return FilesystemResult<Path>::ok(Path(buf));
#else
  const char *tmp = getenv("TMPDIR");
  if(!tmp) tmp = "/tmp";
  return FilesystemResult<Path>::ok(Path(tmp));
#endif
}

LUMEX_PUBLIC_API
std::wstring
Lumex::Filesystem::to_wide_string(std::string const &str)
{
#if LUMEX_OS_WINDOWS
  if(str.empty()) return {};
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
  return wstr;
#else
  if(str.empty()) return {};
  size_t len = mbstowcs(nullptr, str.c_str(), 0);
  if(len == (size_t)-1) return {};
  std::wstring wstr(len, 0);
  mbstowcs(&wstr[0], str.c_str(), len);
  return wstr;
#endif
}

LUMEX_PUBLIC_API
std::string
Lumex::Filesystem::from_wide_string(std::wstring const &wstr)
{
#if LUMEX_OS_WINDOWS
  if(wstr.empty()) return {};
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
  return str;
#else
  if(wstr.empty()) return {};
  size_t len = wcstombs(nullptr, wstr.c_str(), 0);
  if(len == (size_t)-1) return {};
  std::string str(len, 0);
  wcstombs(&str[0], wstr.c_str(), len);
  return str;
#endif
}

LUMEX_PUBLIC_API
Lumex::Path
Lumex::Filesystem::get_exe_path()
{
  try
  {
#if LUMEX_OS_WINDOWS
    std::array<wchar_t, MAX_PATH> buf{};
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
    if(len == 0 || len > buf.size()) return {};
    std::wstring wpath(buf.data(), len);
    std::string spath = from_wide_string(wpath);
    return Path(spath);
#else
    std::array<char, PATH_MAX> buf{};
    ssize_t len            = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    buf[len < 0 ? 0 : len] = '\0';
    return Path(buf.data());
#endif
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Can't get executable path, reason: " << exc.what() << ", falling back on empty path\n";
    return {};
  }
  catch(...)
  {
    std::cerr << "Can't get executable path by unknown reason (non-standard "
                 "exception raised)\n";
    return {};
  }
}

LUMEX_PUBLIC_API
void
Lumex::Filesystem::lock_directory(Lumex::Path const &path)
{
  try
  {
#if LUMEX_OS_WINDOWS
    static HANDLE s_dirHandle = INVALID_HANDLE_VALUE;
    s_dirHandle               = CreateFileW(path.wstring().c_str(), GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE, // without FILE_SHARE_DELETE
                                            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if(s_dirHandle == INVALID_HANDLE_VALUE) std::cerr << "Warning: cannot lock \"" << path << "\" directory\n";
#else
    static int s_lockFd = -1;
    auto lockPath       = path / ".lock";
    s_lockFd            = ::open(lockPath.string().c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(s_lockFd >= 0)
    {
      struct flock fl{};
      fl.l_type   = F_WRLCK;
      fl.l_whence = SEEK_SET;
      fl.l_start  = 0;
      fl.l_len    = 0; // the whole file
      if(fcntl(s_lockFd, F_SETLK, &fl) == -1) std::cerr << "Warning: cannot lock \"" << path << "\" directory\n";
    }
#endif
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Exception while using " << LUMEX_FUNC_NAME << "\n";
    std::cerr << "Reason: " << exc.what() << ". This function done nothing.\n";
  }
  catch(...)
  {
    std::cerr << "Exception while using " << LUMEX_FUNC_NAME << "\n";
    std::cerr << "Reason: unknown (non-standard exception raised). This "
                 "function done nothing.\n";
  }
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_readable(Lumex::Path const &path)
{
  try
  {
    if(!LumexFilesystem::exists(path))
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" does not exist, returning false\n";
      return false;
    }
    FilesystemResult<FileStatus> statusRes = LumexFilesystem::status(path);
    if(!statusRes)
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" status() failed, returning false\n";
      return false;
    }
    Perms perms  = statusRes.value().permissions();
    bool hasRead = (perms & Perms::owner_read) != Perms::none || (perms & Perms::group_read) != Perms::none
                   || (perms & Perms::others_read) != Perms::none;
    if(!hasRead)
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path
                << "\" does not have read permissions, returning false\n";
      return false;
    }
    std::ifstream ifs(path.c_str());
    if(!ifs.is_open() || ifs.fail())
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" cannot be opened, returning false\n";
      return false;
    }
    return true;
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Exception in is_readable: " << exc.what() << ". Returning false.\n";
    return false;
  }
  catch(...)
  {
    std::cerr << "Unknown exception in is_readable. Returning false.\n";
    return false;
  }
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_writable(Lumex::Path const &path)
{
  try
  {
    if(!LumexFilesystem::exists(path))
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" does not exist, returning false\n";
      return false;
    }
    FilesystemResult<FileStatus> statusRes = LumexFilesystem::status(path);
    if(!statusRes)
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" status() failed, returning false\n";
      return false;
    }
    Perms perms   = statusRes.value().permissions();
    bool hasWrite = (perms & Perms::owner_write) != Perms::none || (perms & Perms::group_write) != Perms::none
                    || (perms & Perms::others_write) != Perms::none;
    if(!hasWrite)
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path
                << "\" does not have write permissions, returning false\n";
      return false;
    }
    std::ofstream ofs(path.c_str(), std::ios::in | std::ios::out);
    if(!ofs.is_open() || ofs.fail())
    {
      std::cerr << "Warning (" << LUMEX_FUNC_NAME << "): \"" << path << "\" cannot be opened, returning false\n";
      return false;
    }
    return true;
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Exception in is_writable: " << exc.what() << ". Returning false.\n";
    return false;
  }
  catch(...)
  {
    std::cerr << "Unknown exception in is_writable. Returning false.\n";
    return false;
  }
}

LUMEX_PUBLIC_API
bool
Lumex::Filesystem::is_accessible(Lumex::Path const &path)
{
  std::clog << "Checking if path '" << path
            << "' is accessible by the current "
               "process\n";
  return Lumex::Filesystem::is_readable(path) && Lumex::Filesystem::is_writable(path);
}

LUMEX_PUBLIC_API
inline bool
Lumex::Core::Filesystem::isFileExists(std::string const &path)
{
  std::ifstream ifs(path.c_str());
  return ifs.good();
}

LUMEX_PUBLIC_API
void
Lumex::Core::Filesystem::checkName(std::string const &name)
{
  using namespace Detail;

  if(name.empty()) throw std::invalid_argument("Name of the file or directory cannot be empty");

  // Checking forbidden characters (predicate)
  for(char chr : name)
    if(Detail::isForbidden(chr))
      throw std::invalid_argument("Name of the file contains forbidden character: " + std::string(1, chr));

  // Checking reserved names
  if(Detail::isReservedName(name)) throw std::invalid_argument("Name of the file is reserved by the system: " + name);

  // Checking invalid endings/beginnings
  if(Detail::hasInvalidEnding(name)) throw std::invalid_argument("Name of the file has invalid ending/beginning");
}

LUMEX_PUBLIC_API std::string
Lumex::Core::Filesystem::sanitizeName(std::string const &name, // NOLINT(bugprone-easily-swappable-parameters)
                                      std::string const &defaultValue) noexcept
{
  using namespace Detail;

  if(name.empty()) return defaultValue;

  try
  {
    checkName(name);
    return name; // Already valid
  }
  catch(...)
  {
    std::string sanitized = name;

    // Replacing invalid characters through predicate
    for(char &chr : sanitized)
      if(Detail::isForbidden(chr)) chr = '_';

    // Handling reserved names
    if(Detail::isReservedName(sanitized)) return defaultValue + "_file";

    // Cleaning invalid endings/beginnings
#if LUMEX_OS_WINDOWS
    while(!sanitized.empty() && (sanitized.back() == '.' || sanitized.back() == ' ')) sanitized.pop_back();
#elif LUMEX_OS_LINUX
    if(!sanitized.empty() && sanitized.front() == '-') sanitized.erase(0, 1);
#endif

    // Removing consecutive underscores
    sanitized.erase(
#if __cplusplus >= 202002L
      std::ranges::unique(sanitized, [](char chr1, char chr2) { return chr1 == '_' && chr2 == '_'; }).begin(),
      sanitized.end()
#else
      std::unique(sanitized.begin(), sanitized.end(), [](char chr1, char chr2) { return chr1 == '_' && chr2 == '_'; }),
      sanitized.end()
#endif
    );

    // Removing leading/trailing underscores
    if(!sanitized.empty() && sanitized.front() == '_') sanitized.erase(0, 1);
    if(!sanitized.empty() && sanitized.back() == '_') sanitized.pop_back();

    return sanitized.empty() ? defaultValue : sanitized;
  }
}

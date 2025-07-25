#define LUMEX_IMPLEMENTATION
#include "LumexTemporary.hpp"

#include <fstream>
#include <sstream>

// Convenience using declarations
using LumexFilesystem = Lumex::Filesystem;

#if LUMEX_OS_WINDOWS
  #include <process.h>
  #include <windows.h>

#elif LUMEX_OS_UNIX
  #include <sys/types.h>
  #include <unistd.h>

#endif

// RAII TemporaryDirectory implementation
LUMEX_PUBLIC_API
TemporaryDirectory::TemporaryDirectory(Lumex::Path const &path)
    : m_path(path),
      m_valid(LumexFilesystem::exists(path)
              && LumexFilesystem::is_directory(path))
{}

LUMEX_PUBLIC_API
TemporaryDirectory::~TemporaryDirectory()
{
  if(m_valid) LumexTemporary::remove_temp_directory(m_path);
}

LUMEX_PUBLIC_API
TemporaryDirectory::TemporaryDirectory(TemporaryDirectory &&other) noexcept
    : m_path(std::move(other.m_path)), m_valid(other.m_valid)
{
  other.m_valid = false;
}

LUMEX_PUBLIC_API
TemporaryDirectory &
TemporaryDirectory::operator=(TemporaryDirectory &&other) noexcept
{
  if(this != &other)
    {
      if(m_valid) LumexTemporary::remove_temp_directory(m_path);
      m_path        = std::move(other.m_path);
      m_valid       = other.m_valid;
      other.m_valid = false;
    }
  return *this;
}

LUMEX_PUBLIC_API
void
TemporaryDirectory::release()
{
  m_valid = false;
}

// RAII TemporaryFile implementation
LUMEX_PUBLIC_API
TemporaryFile::TemporaryFile(Lumex::Path const &path)
    : m_path(path),
      m_valid(LumexFilesystem::exists(path)
              && LumexFilesystem::is_regular_file(path))
{}

LUMEX_PUBLIC_API
TemporaryFile::~TemporaryFile()
{
  if(m_valid) LumexTemporary::remove_temp_file(m_path);
}

LUMEX_PUBLIC_API
TemporaryFile::TemporaryFile(TemporaryFile &&other) noexcept
    : m_path(std::move(other.m_path)), m_valid(other.m_valid)
{
  other.m_valid = false;
}

LUMEX_PUBLIC_API
TemporaryFile &
TemporaryFile::operator=(TemporaryFile &&other) noexcept
{
  if(this != &other)
    {
      if(m_valid) LumexTemporary::remove_temp_file(m_path);
      m_path        = std::move(other.m_path);
      m_valid       = other.m_valid;
      other.m_valid = false;
    }
  return *this;
}

LUMEX_PUBLIC_API
void
TemporaryFile::release()
{
  m_valid = false;
}

// Main LumexTemporary implementation
LUMEX_PUBLIC_API
Lumex::Path
LumexTemporary::get_temp_directory_path()
{
#if LUMEX_OS_WINDOWS
  // On Windows, always use system temp directory (original behavior)
  auto tmp = Lumex::Filesystem::temp_directory_path().value() / "Lumex";
  tmp += Lumex::Path::preferred_separator;
  return tmp;
#else
  // On Linux, check if running as AppImage
  std::string appImageMode
    = LumexEnvironment::get("LUMEX_APPIMAGE_MODE").value;
  if(!appImageMode.empty() && appImageMode == "1")
    {
      // If running as AppImage, use the system's default temporary directory
      auto tmp = Lumex::Filesystem::temp_directory_path().value() / "Lumex";
      tmp += Lumex::Path::preferred_separator;
      return tmp;
    }

  // Otherwise, create a temporary directory in the user's home directory
  // This avoids permission issues in /opt/LumReportViewer/bin
  std::string homeDir = LumexEnvironment::get("HOME").value;

  if(homeDir.empty())
    {
      throw std::runtime_error(
        "User home directory environment variable not set.");
    }

  Lumex::Path userTempDir
    = Lumex::Path(homeDir) / ".lumreportviewer" / "Lumex";
  userTempDir += Lumex::Path::preferred_separator;
  return userTempDir;

#endif
}

LUMEX_PUBLIC_API
std::string
LumexTemporary::_generate_random_suffix()
{
  // C++11 compatible random suffix generation
  static bool initialized = false;
  if(!initialized)
    {
      std::srand(static_cast<unsigned int>(std::time(nullptr)));
      initialized = true;
    }

  std::ostringstream oss;

  // Add timestamp component
  std::time_t now = std::time(nullptr);
  oss << std::hex << now;

  // Add process ID if available
#if LUMEX_OS_WINDOWS
  oss << "_" << std::hex << GetCurrentProcessId();
#elif LUMEX_OS_UNIX
  oss << "_" << std::hex << getpid();
#else
  // Fallback for other systems - use additional random component
  oss << "_" << std::hex << (std::rand() % 65536);
#endif

  // Add random component
  int const random_count = 6;
  int const random_base  = 16;
  for(int i = 0; i < random_count; ++i)
    oss << std::hex << (std::rand() % random_base);

  return oss.str();
}

LUMEX_PUBLIC_API
std::string
LumexTemporary::generate_temp_name(std::string const &prefix)
{
  std::string name;
  if(!prefix.empty()) name = prefix + "_";
  name += _generate_random_suffix();
  return name;
}

LUMEX_PUBLIC_API
bool
LumexTemporary::_ensure_temp_directory_exists(Lumex::Path const &temp_dir)
{
  if(LumexFilesystem::exists(temp_dir))
    return LumexFilesystem::is_directory(temp_dir);

  auto result = LumexFilesystem::create_directories(temp_dir);
  return result.success();
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<TemporaryDirectory>
LumexTemporary::create_temp_directory(std::string const &name)
{
  Lumex::Path temp_base = get_temp_directory_path();

  // Ensure base temp directory exists
  if(!_ensure_temp_directory_exists(temp_base))
    {
      return Lumex::FilesystemResult<TemporaryDirectory>(
        -1, TemporaryDirectory(Lumex::Path()));
    }

  // Generate unique directory name
  std::string dir_name = generate_temp_name(name.empty() ? "tmp_dir" : name);
  Lumex::Path temp_dir_path = temp_base / dir_name;

  // Try to create directory (retry with different names if exists)
  int attempts           = 0;
  int const max_attempts = 100;

  while(attempts < max_attempts)
    {
      if(!LumexFilesystem::exists(temp_dir_path))
        {
          auto result = LumexFilesystem::create_directory(temp_dir_path);
          if(result.success())
            {
              return Lumex::FilesystemResult<TemporaryDirectory>(
                TemporaryDirectory(temp_dir_path));
            }
          return Lumex::FilesystemResult<TemporaryDirectory>(
            result.error_code(), TemporaryDirectory(Lumex::Path()));
        }

      // Name collision, try with new suffix
      ++attempts;
      dir_name      = generate_temp_name(name.empty() ? "tmp_dir" : name);
      temp_dir_path = temp_base / dir_name;
    }

  // Too many collisions
  return Lumex::FilesystemResult<TemporaryDirectory>(
    -2, TemporaryDirectory(Lumex::Path()));
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
LumexTemporary::remove_temp_directory(Lumex::Path const &path)
{
  if(!LumexFilesystem::exists(path))
    return Lumex::FilesystemResult<void>(0); // Already removed, success

  if(!LumexFilesystem::is_directory(path))
    return Lumex::FilesystemResult<void>(-1); // Not a directory

  // Remove directory and all contents
  auto result = LumexFilesystem::remove_all(path);
  if(result.success()) return Lumex::FilesystemResult<void>();
  return Lumex::FilesystemResult<void>(result.error_code());
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<TemporaryFile>
LumexTemporary::create_temp_file(std::string const &name)
{
  Lumex::Path temp_base = get_temp_directory_path();

  // Ensure base temp directory exists
  if(!_ensure_temp_directory_exists(temp_base))
    {
      return Lumex::FilesystemResult<TemporaryFile>(
        -1, TemporaryFile(Lumex::Path()));
    }

  // Generate unique file name
  std::string file_name = generate_temp_name(name.empty() ? "tmp_file" : name);
  Lumex::Path temp_file_path = temp_base / file_name;

  // Try to create file (retry with different names if exists)
  int attempts           = 0;
  int const max_attempts = 100;

  while(attempts < max_attempts)
    {
      if(!LumexFilesystem::exists(temp_file_path))
        {
          // Create the file
          std::ofstream file(temp_file_path.c_str());
          if(file.is_open())
            {
              file.close();
              if(LumexFilesystem::exists(temp_file_path))
                {
                  return Lumex::FilesystemResult<TemporaryFile>(
                    TemporaryFile(temp_file_path));
                }
            }
          // File creation failed
          return Lumex::FilesystemResult<TemporaryFile>(
            -3, TemporaryFile(Lumex::Path()));
        }

      // Name collision, try with new suffix
      ++attempts;
      file_name      = generate_temp_name(name.empty() ? "tmp_file" : name);
      temp_file_path = temp_base / file_name;
    }

  // Too many collisions
  return Lumex::FilesystemResult<TemporaryFile>(-2,
                                                TemporaryFile(Lumex::Path()));
}

LUMEX_PUBLIC_API
Lumex::FilesystemResult<void>
LumexTemporary::remove_temp_file(Lumex::Path const &path)
{
  if(!LumexFilesystem::exists(path))
    return Lumex::FilesystemResult<void>(0); // Already removed, success

  if(!LumexFilesystem::is_regular_file(path))
    return Lumex::FilesystemResult<void>(-1); // Not a regular file

  // Remove the file
  auto result = LumexFilesystem::remove(path);
  if(result.success()) return Lumex::FilesystemResult<void>();
  return Lumex::FilesystemResult<void>(result.error_code());
}

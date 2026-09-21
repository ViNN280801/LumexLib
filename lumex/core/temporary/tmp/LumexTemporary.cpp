#define LUMEX_IMPLEMENTATION
#include <atomic>
#include <fstream>
#include <random>
#include <sstream>

#include "LumexTemporary.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// Convenience using declarations
namespace lumex
{
namespace core
{
namespace temporary
{
namespace tmp
{
using lumex_filesystem = lumex::filesystem;

#if LUMEX_OS_WINDOWS
#include <Windows.h>
#include <process.h>

#elif LUMEX_OS_UNIX
#include <sys/types.h>
#include <unistd.h>

#endif

// RAII TemporaryDirectory implementation
LUMEX_PUBLIC_API
TemporaryDirectory::TemporaryDirectory (lumex::path const &path)
    : m_path (path), m_valid (lumex_filesystem::exists (path)
                              && lumex_filesystem::is_directory (path))
{
}

LUMEX_PUBLIC_API
TemporaryDirectory::~TemporaryDirectory ()
{
  if (m_valid)
    LumexTemporary::remove_temp_directory (m_path);
}

LUMEX_PUBLIC_API
TemporaryDirectory::TemporaryDirectory (TemporaryDirectory &&other)
    LUMEX_NOEXCEPT : m_path (std::move (other.m_path)),
                     m_valid (other.m_valid)
{
  other.m_valid = false;
}

LUMEX_PUBLIC_API
TemporaryDirectory &
TemporaryDirectory::operator= (TemporaryDirectory &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      // Clean up current directory if valid
      if (m_valid)
        LumexTemporary::remove_temp_directory (m_path);

      // Take ownership of other's path
      m_path = std::move (other.m_path);
      m_valid = other.m_valid;

      // Set other to invalid state
      other.m_valid = false;
    }
  return *this;
}

LUMEX_PUBLIC_API
void
TemporaryDirectory::release ()
{
  m_valid = false;
}

// RAII TemporaryFile implementation
LUMEX_PUBLIC_API
TemporaryFile::TemporaryFile (lumex::path const &path)
    : m_path (path), m_valid (lumex_filesystem::exists (path)
                              && lumex_filesystem::is_regular_file (path))
{
}

LUMEX_PUBLIC_API
TemporaryFile::~TemporaryFile ()
{
  if (m_valid)
    LumexTemporary::remove_temp_file (m_path);
}

LUMEX_PUBLIC_API
TemporaryFile::TemporaryFile (TemporaryFile &&other) LUMEX_NOEXCEPT
    : m_path (std::move (other.m_path)),
      m_valid (other.m_valid)
{
  other.m_valid = false;
}

LUMEX_PUBLIC_API
TemporaryFile &
TemporaryFile::operator= (TemporaryFile &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      // Clean up current file if valid
      if (m_valid)
        LumexTemporary::remove_temp_file (m_path);

      // Take ownership of other's path
      m_path = std::move (other.m_path);
      m_valid = other.m_valid;

      // Set other to invalid state
      other.m_valid = false;
    }
  return *this;
}

LUMEX_PUBLIC_API
void
TemporaryFile::release ()
{
  m_valid = false;
}

// Main LumexTemporary implementation
LUMEX_PUBLIC_API
lumex::path
LumexTemporary::get_temp_directory_path ()
{
#if LUMEX_OS_WINDOWS
  // On Windows, always use system temp directory (original behavior)
  auto tmp
      = lumex::core::filesystem::fs::lumex_filesystem::temp_directory_path ()
            .value ()
        / "Lumex";
  tmp += lumex::path::preferred_separator;
  return tmp;
#else
  // On Linux, check if running as AppImage
  std::string appImagePath = LumexEnvironment::get ("APPIMAGE").value;
  if (!appImagePath.empty ())
    {
      // If running as AppImage, use the system's default temporary directory
      auto tmp = lumex::core::filesystem::fs::lumex_filesystem::
                     temp_directory_path ()
                         .value ()
                 / "Lumex";
      tmp += lumex::path::preferred_separator;
      return tmp;
    }

  // Otherwise, create a temporary directory in the user's home directory
  std::string homeDir = LumexEnvironment::get ("HOME").value;

  if (homeDir.empty ())
    throw std::runtime_error (
        "User home directory environment variable not set.");

  lumex::path userTempDir = lumex::path (homeDir) / "Lumex";
  userTempDir += lumex::path::preferred_separator;
  return userTempDir;

#endif
}

LUMEX_PUBLIC_API
std::string
LumexTemporary::_generate_random_suffix ()
{
  auto const nullHex = 0x00;
  auto const maxHex = 0x0F;

  static thread_local std::mt19937 generator (std::random_device{}());
  static thread_local std::uniform_int_distribution<> distribution (
      nullHex, maxHex); // For hex digits

  static std::atomic<uint64_t> global_counter (0);
  uint64_t counter_value
      = global_counter.fetch_add (1, std::memory_order_relaxed);

  std::ostringstream oss;

  // Add timestamp component
  std::time_t now = std::time (nullptr);
  oss << std::hex << now;

  // Add process ID if available
#if LUMEX_OS_WINDOWS
  oss << "_" << std::hex << GetCurrentProcessId ();
#elif LUMEX_OS_UNIX
  oss << "_" << std::hex << getpid ();
#else
  // Fallback for other systems - use additional random component
  oss << "_" << std::hex
      << distribution (generator); // Using new thread-safe random
#endif

  // Add random component
  int const random_count = 6;
  for (int i = 0; i < random_count; ++i)
    oss << std::hex << distribution (generator);

  // Add counter value
  oss << "_" << std::hex << counter_value;

  return oss.str ();
}

LUMEX_PUBLIC_API
std::string
LumexTemporary::generate_temp_name (std::string const &prefix)
{
  std::string name;
  if (!prefix.empty ())
    name = prefix + "_";
  name += _generate_random_suffix ();
  return name;
}

LUMEX_PUBLIC_API
bool
LumexTemporary::_ensure_temp_directory_exists (lumex::path const &temp_dir)
{
  if (lumex_filesystem::exists (temp_dir))
    return lumex_filesystem::is_directory (temp_dir);

  auto result = lumex_filesystem::create_directories (temp_dir);
  return result.success ();
}

LUMEX_PUBLIC_API
lumex::filesystem_result<TemporaryDirectory>
LumexTemporary::create_temp_directory (std::string const &name)
{
  lumex::path temp_base = get_temp_directory_path ();

  // Ensure base temp directory exists
  if (!_ensure_temp_directory_exists (temp_base))
    return lumex::filesystem_result<TemporaryDirectory>::err (
        -1, TemporaryDirectory (lumex::path ()));

  // Generate unique directory name
  std::string dir_name = generate_temp_name (name.empty () ? "tmp_dir" : name);
  lumex::path temp_dir_path = temp_base / dir_name;

  // Try to create directory (retry with different names if exists)
  int attempts = 0;
  int const max_attempts = 100;

  while (attempts < max_attempts)
    {
      if (!lumex_filesystem::exists (temp_dir_path))
        {
          auto result = lumex_filesystem::create_directory (temp_dir_path);
          if (result.success ())
            return lumex::filesystem_result<TemporaryDirectory>::ok (
                TemporaryDirectory (temp_dir_path));
          return lumex::filesystem_result<TemporaryDirectory>::err (
              result.error_code (), TemporaryDirectory (lumex::path ()));
        }

      // Name collision, try with new suffix
      ++attempts;
      dir_name = generate_temp_name (name.empty () ? "tmp_dir" : name);
      temp_dir_path = temp_base / dir_name;
    }

  // Too many collisions
  return lumex::filesystem_result<TemporaryDirectory>::err (
      -2, TemporaryDirectory (lumex::path ()));
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
LumexTemporary::remove_temp_directory (lumex::path const &path)
{
  if (!lumex_filesystem::exists (path))
    return lumex::filesystem_result<void>::ok (); // Already removed, success

  if (!lumex_filesystem::is_directory (path))
    return lumex::filesystem_result<void>::err (-1); // Not a directory

  // Remove directory and all contents
  auto result = lumex_filesystem::remove_all (path);
  if (result.success ())
    return lumex::filesystem_result<void>::ok ();
  return lumex::filesystem_result<void>::err (result.error_code ());
}

LUMEX_PUBLIC_API
lumex::filesystem_result<TemporaryFile>
LumexTemporary::create_temp_file (std::string const &name)
{
  lumex::path temp_base = get_temp_directory_path ();

  // Ensure base temp directory exists
  if (!_ensure_temp_directory_exists (temp_base))
    return lumex::filesystem_result<TemporaryFile>::err (
        -1, TemporaryFile (lumex::path ()));

  // Generate unique file name
  std::string file_name
      = generate_temp_name (name.empty () ? "tmp_file" : name);
  lumex::path temp_file_path = temp_base / file_name;

  // Try to create file (retry with different names if exists)
  int attempts = 0;
  int const max_attempts = 100;

  while (attempts < max_attempts)
    {
      if (!lumex_filesystem::exists (temp_file_path))
        {
          // Create the file
          std::ofstream file (temp_file_path.c_str ());
          if (file.is_open ())
            {
              file.close ();
              if (lumex_filesystem::exists (temp_file_path))
                return lumex::filesystem_result<TemporaryFile>::ok (
                    TemporaryFile (temp_file_path));
            }
          // File creation failed
          return lumex::filesystem_result<TemporaryFile>::err (
              -3, TemporaryFile (lumex::path ()));
        }

      // Name collision, try with new suffix
      ++attempts;
      file_name = generate_temp_name (name.empty () ? "tmp_file" : name);
      temp_file_path = temp_base / file_name;
    }

  // Too many collisions
  return lumex::filesystem_result<TemporaryFile>::err (
      -2, TemporaryFile (lumex::path ()));
}

LUMEX_PUBLIC_API
lumex::filesystem_result<void>
LumexTemporary::remove_temp_file (lumex::path const &path)
{
  if (!lumex_filesystem::exists (path))
    return lumex::filesystem_result<void>::ok (); // Already removed, success

  if (!lumex_filesystem::is_regular_file (path))
    return lumex::filesystem_result<void>::err (-1); // Not a regular file

  // Remove the file
  auto result = lumex_filesystem::remove (path);
  if (result.success ())
    return lumex::filesystem_result<void>::ok ();
  return lumex::filesystem_result<void>::err (result.error_code ());
}
} // namespace tmp
} // namespace temporary
} // namespace core
} // namespace lumex

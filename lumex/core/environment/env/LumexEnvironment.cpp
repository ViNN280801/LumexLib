#define LUMEX_IMPLEMENTATION
#include <algorithm> // std::transform
#include <cctype>    // std::tolower

#include "LumexEnvironment.hpp"

#if LUMEX_OS_WINDOWS
namespace lumex
{
namespace core
{
namespace environment
{
namespace env
{
LUMEX_PUBLIC_API
LumexEnvironment::EnvResult
LumexEnvironment::WindowsEnvironmentStrategy::get_variable (
    char const *name) const
{
  if (name == nullptr
      || name[0]
             == '\0') // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return { ERROR_INVALID_PARAMETER };

  // First try _dupenv_s for security (preferred on Windows)
  char *buffer = nullptr;
  std::size_t buffer_size = 0;
  errno_t result = _dupenv_s (std::addressof (buffer),
                              std::addressof (buffer_size), name);

  if (result == 0 && buffer != nullptr)
    {
      // RAII wrapper for automatic cleanup
      std::unique_ptr<char, decltype (&free)> smart_buffer (
          buffer, std::addressof (free));
      return EnvResult (string_type (buffer));
    }

  // Fallback to GetEnvironmentVariableA
  DWORD size = GetEnvironmentVariableA (name, nullptr, 0);
  if (size == 0)
    {
      DWORD error = GetLastError ();
      if (error == ERROR_SUCCESS)
        { // Variable exists but is empty
          return EnvResult (
              string_type ("")); // Return successful result with empty string
        }
      return { static_cast<int> (error) }; // True error (e.g., not found)
    }

  if (size > MAX_ENV_BUFFER_SIZE)
    return { ERROR_BUFFER_OVERFLOW };

  std::unique_ptr<char[]> win_buffer (new (
      std::nothrow) char[size]); // NOLINT(cppcoreguidelines-avoid-c-arrays)
  if (!win_buffer)
    return { ERROR_NOT_ENOUGH_MEMORY };

  DWORD actual_size = GetEnvironmentVariableA (name, win_buffer.get (), size);
  if (actual_size == 0 || actual_size >= size)
    return { static_cast<int> (GetLastError ()) };

  return EnvResult (string_type (win_buffer.get (), actual_size));
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::WindowsEnvironmentStrategy::set_variable (
    char const *name, char const *value) const
{
  if (name == nullptr || name[0] == '\0')
    return false; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return SetEnvironmentVariableA (name, value) != 0;
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::WindowsEnvironmentStrategy::unset_variable (
    char const *name) const
{
  if (name == nullptr || name[0] == '\0')
    return false; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return SetEnvironmentVariableA (name, nullptr) != 0;
}

#else

LUMEX_PUBLIC_API
LumexEnvironment::EnvResult
LumexEnvironment::PosixEnvironmentStrategy::get_variable (
    char const *name) const
{
  if (name == nullptr || name[0] == '\0')
    return { -1 }; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // getenv is not thread-safe, but we can't do much about it in C++11
  // without external synchronization
  char const *result = std::getenv (name);
  if (result == nullptr)
    return { -2 }; // Not found

  // Validate result pointer and create safe copy
  try
    {
      size_type len = std::strlen (result);
      if (len > MAX_ENV_BUFFER_SIZE)
        return { -3 }; // Too large
      return EnvResult (string_type (result, len));
    }
  catch (...)
    {
      // Even though we're not supposed to throw, std::string constructor
      // might
      return { -4 }; // Memory allocation failed
    }
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::PosixEnvironmentStrategy::set_variable (
    char const *name, char const *value) const
{
  if (name == nullptr || name[0] == '\0')
    return false;

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  // Use setenv if available (POSIX.1-2001)
  return setenv (name, value ? value : "", value ? 0 : 1) == 0;
#else
  // Fallback to putenv (less safe, but more portable)
  if (value == nullptr)
    return unset_variable (name);

  size_type name_len = std::strlen (name);
  size_type value_len = std::strlen (value);
  size_type total_len = name_len + value_len + 2; // name=value\0

  if (total_len > MAX_ENV_BUFFER_SIZE)
    return false;

  std::unique_ptr<char[]> env_string (new (std::nothrow) char[total_len]);
  if (!env_string)
    return false;

  std::strcpy (env_string.get (), name);
  std::strcat (env_string.get (), "=");
  std::strcat (env_string.get (), value);

  // Note: putenv takes ownership of the string, so we release it
  return putenv (env_string.release ()) == 0;
#endif
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::PosixEnvironmentStrategy::unset_variable (
    char const *name) const
{
  if (name == nullptr || name[0] == '\0')
    return false;

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  return unsetenv (name) == 0;
#else
  // Fallback: set to empty string
  return set_variable (name, "");
#endif
}
#endif

LUMEX_PUBLIC_API
LumexEnvironment &
LumexEnvironment::instance ()
{
  static LumexEnvironment instance;
  return instance;
}

LUMEX_PUBLIC_API
LumexEnvironment::EnvResult
LumexEnvironment::get_environment_variable (char const *name) const
{
  if (name == nullptr)
    return { -1 };

  std::lock_guard<std::mutex> lock (m_mutex);
  return m_strategy->get_variable (name);
}

LUMEX_PUBLIC_API
LumexEnvironment::EnvResult
LumexEnvironment::get_environment_variable (string_type const &name) const
{
  return get_environment_variable (name.c_str ());
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::set_environment_variable (char const *name,
                                            char const *value,
                                            bool overwrite) const
{
  if (name == nullptr)
    return false;

  std::lock_guard<std::mutex> lock (m_mutex);
  if (value == nullptr)
    return m_strategy->unset_variable (name);

  if (!overwrite && m_strategy->get_variable (name).success)
    {
      // Variable already exists and the caller asked not to overwrite it.
      return true;
    }

  return m_strategy->set_variable (name, value);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::set_environment_variable (string_type const &name,
                                            string_type const &value,
                                            bool overwrite) const
{
  return set_environment_variable (name.c_str (), value.c_str (), overwrite);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::unset_environment_variable (char const *name) const
{
  if (name == nullptr)
    return false;

  std::lock_guard<std::mutex> lock (m_mutex);
  return m_strategy->unset_variable (name);
}

LUMEX_PUBLIC_API
LumexEnvironment::string_type
LumexEnvironment::get_environment_variable_or (
    char const *name, string_type const &default_value) const
{
  EnvResult result = get_environment_variable (name);
  return result.get_value_or (default_value);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::has_environment_variable (char const *name) const
{
  return get_environment_variable (name).success;
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::is_environment_variable_truthy (char const *name) const
{
  string_type value = get_environment_variable_or (name, string_type ());

  string_type value_lower = value;
  std::transform (value_lower.begin (), value_lower.end (),
                  value_lower.begin (), [] (unsigned char chr) {
                    return static_cast<char> (std::tolower (chr));
                  });

  return !(value.empty () || value_lower == "false" || value_lower == "0");
}

LUMEX_PUBLIC_API
LumexEnvironment::EnvResult
LumexEnvironment::get (char const *name)
{
  return instance ().get_environment_variable (name);
}

LUMEX_PUBLIC_API
LumexEnvironment::string_type
LumexEnvironment::get_or (char const *name, string_type const &default_value)
{
  return instance ().get_environment_variable_or (name, default_value);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::set (char const *name, char const *value, bool overwrite)
{
  return instance ().set_environment_variable (name, value, overwrite);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::has (char const *name)
{
  return instance ().has_environment_variable (name);
}

LUMEX_PUBLIC_API
bool
LumexEnvironment::is_truthy (char const *name)
{
  return instance ().is_environment_variable_truthy (name);
}

LUMEX_PUBLIC_API
bool
is_env_set (LumexEnvironment::string_type const &name)
{
  return !LumexEnvironment::get_or (name.c_str (),
                                    LumexEnvironment::string_type ())
              .empty ();
}

LUMEX_PUBLIC_API
bool
is_env_truthy (LumexEnvironment::string_type const &name)
{
  return LumexEnvironment::is_truthy (name.c_str ());
}
} // namespace env
} // namespace environment
} // namespace core
} // namespace lumex

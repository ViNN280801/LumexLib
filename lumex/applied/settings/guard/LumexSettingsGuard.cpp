#define LUMEX_IMPLEMENTATION
#include "LumexSettingsGuard.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#include "lumex/applied/logging/LumexLogging"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"

namespace lumex
{
namespace applied
{
namespace settings
{
namespace guard
{
LUMEX_PUBLIC_API
LumexSettingsGuard::LumexSettingsGuard (
    std::shared_ptr<ILumexSettings> settings,
    std::string filename) LUMEX_NOEXCEPT : m_settings (std::move (settings)),
                                           m_filename (std::move (filename))
{
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::backup (std::string const &filename) LUMEX_NOEXCEPT
{
  try
    {
      lumex::path const src (filename);
      if (!lumex::core::filesystem::fs::lumex_filesystem::exists (src)
          || !lumex::core::filesystem::fs::lumex_filesystem::is_regular_file (
              src))
        return false;

      std::string const dst
          = filename + ".bak."
            + LumexTime::get_current_datetime ("%Y%m%d-%H%M%S");

      auto const result
          = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
              src, lumex::path (dst));
      if (!result.success ())
        {
          LumexLogging::warning ("LumexSettingsGuard", "Failed to back up '",
                                 filename, "' to '", dst, "' (error code ",
                                 result.error_code (), ").");
          return false;
        }

      LumexLogging::info ("LumexSettingsGuard", "Backup of '", filename,
                          "' created at '", dst, "'.");
      return true;
    }
  catch (...)
    {
      LumexLogging::warning ("LumexSettingsGuard",
                             "Exception while backing up '", filename, "'.");
      return false;
    }
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::ensureExistsWithDefaults (
    LumexSettingsCreateFn const &createDefault) LUMEX_NOEXCEPT
{
  std::lock_guard<std::recursive_mutex> lock (m_mutex);
  return _ensureOrRepairImpl (createDefault, /*logOnFinalFailure=*/true);
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::repairIfCorrupted (
    LumexSettingsCreateFn const &createDefault) LUMEX_NOEXCEPT
{
  std::lock_guard<std::recursive_mutex> lock (m_mutex);
  return _ensureOrRepairImpl (createDefault, /*logOnFinalFailure=*/false);
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::_ensureOrRepairImpl (
    LumexSettingsCreateFn const &createDefault,
    bool logOnFinalFailure) LUMEX_NOEXCEPT
{
  if (!m_settings)
    return false;

  bool ok = m_settings->load (m_filename);
  if (!ok)
    {
      // Best-effort: back up whatever is currently on disk (no-op if nothing
      // exists there).
      backup (m_filename);

      bool createSucceeded = false;
      try
        {
          createSucceeded
              = static_cast<bool> (createDefault) && createDefault ();
        }
      catch (...)
        {
          createSucceeded = false;
        }

      if (!createSucceeded)
        {
          if (logOnFinalFailure)
            LumexLogging::error ("LumexSettingsGuard",
                                 "Failed to create default settings for '",
                                 m_filename, "'.");
          return false;
        }

      ok = m_settings->load (m_filename);
    }

  if (!ok && logOnFinalFailure)
    LumexLogging::error ("LumexSettingsGuard", "Settings file '", m_filename,
                         "' is still invalid after repair.");

  return ok;
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::ensureKeysWithDefaults (
    std::vector<lumex_settings_key_spec_t> const &specs) LUMEX_NOEXCEPT
{
  std::lock_guard<std::recursive_mutex> lock (m_mutex);
  if (!m_settings)
    return false;

  bool changed = false;

  for (auto const &spec : specs)
    {
      std::string const currentValue
          = m_settings->get (spec.section, spec.key);

      bool needDefault = false;
      if (spec.validate)
        {
          try
            {
              needDefault = !spec.validate (currentValue);
            }
          catch (...)
            {
              needDefault = true;
            }
        }
      else
        {
          needDefault = currentValue.empty ();
        }

      if (needDefault)
        {
          m_settings->add (spec.section, spec.key, spec.default_value);
          changed = true;
        }
    }

  if (!changed)
    return false;

  bool const saved = m_settings->save (m_filename);
  if (!saved)
    LumexLogging::warning ("LumexSettingsGuard",
                           "Failed to persist repaired keys to '", m_filename,
                           "'.");

  return saved;
}
} // namespace guard
} // namespace settings
} // namespace applied
} // namespace lumex

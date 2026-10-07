/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define LUMEX_IMPLEMENTATION
#include <exception>
#include <mutex>
#include <string>

#include "LumexSettingsGuard.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
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
namespace
{
/// Module name of every message the guard logs.
LUMEX_CONST_STR GUARD_LOG_MODULE = "LumexSettingsGuard";

/// Logged in place of `what ()` for an exception that does not derive from
/// `std::exception`.
LUMEX_CONST_STR UNKNOWN_EXCEPTION = "unknown exception";

/// Operation name logged for `ensure_keys_with_defaults`.
LUMEX_CONST_STR ENSURE_KEYS_NAME = "ensure_keys_with_defaults";

/// Operation name logged for `ensure_exists_with_defaults`.
LUMEX_CONST_STR ENSURE_EXISTS_NAME = "ensure_exists_with_defaults";

/// Operation name logged for `repair_if_corrupted`.
LUMEX_CONST_STR REPAIR_NAME = "repair_if_corrupted";

/**
 * @brief Logs that an operation of the guard ended with an exception.
 * @details Called from the catch handlers of the `noexcept` members, so it
 * swallows anything the logging itself throws (for example `std::bad_alloc`
 * while formatting the message): an exception leaving the handler would end
 * the program in `std::terminate()`.
 * @param as_error `true` to log at error level, `false` at warning level.
 * @param operation The guard member that failed.
 * @param filename The settings file of the guard.
 * @param what The exception message, or `UNKNOWN_EXCEPTION`.
 */
void
log_exception (bool as_error, char const *operation,
               std::string const &filename, char const *what) LUMEX_NOEXCEPT
{
  try
    {
      if (as_error)
        LumexLogging::error (GUARD_LOG_MODULE, operation, " for '", filename,
                             "' failed with an exception: ", what);
      else
        LumexLogging::warning (GUARD_LOG_MODULE, operation, " for '", filename,
                               "' failed with an exception: ", what);
    }
  catch (...)
    {
    }
}
} // namespace

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
            + lumex_time::get_current_datetime ("%Y%m%d-%H%M%S");

      auto const result
          = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
              src, lumex::path (dst));
      if (!result.success ())
        {
          LumexLogging::warning (GUARD_LOG_MODULE, "Failed to back up '",
                                 filename, "' to '", dst, "' (error code ",
                                 result.error_code (), ").");
          return false;
        }

      LumexLogging::info (GUARD_LOG_MODULE, "Backup of '", filename,
                          "' created at '", dst, "'.");
      return true;
    }
  catch (...)
    {
      // The warning itself may throw (for example `std::bad_alloc`); it must
      // not leave this `noexcept` function.
      try
        {
          LumexLogging::warning (GUARD_LOG_MODULE,
                                 "Exception while backing up '", filename,
                                 "'.");
        }
      catch (...)
        {
        }
      return false;
    }
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::ensure_exists_with_defaults (
    LumexSettingsCreateFn const &createDefault) LUMEX_NOEXCEPT
{
  return _ensure_or_repair_impl (createDefault, /*logOnFinalFailure=*/true);
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::repair_if_corrupted (
    LumexSettingsCreateFn const &createDefault) LUMEX_NOEXCEPT
{
  return _ensure_or_repair_impl (createDefault, /*logOnFinalFailure=*/false);
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::_ensure_or_repair_impl (
    LumexSettingsCreateFn const &createDefault,
    bool logOnFinalFailure) LUMEX_NOEXCEPT
{
  // The guarded settings object may throw although its interface reports
  // failure through return values. An exception must not leave this
  // `noexcept` function, so it ends the call as a failure. The file is not
  // regenerated then: the exception does not mean that the file is corrupted.
  try
    {
      std::lock_guard<std::recursive_mutex> lock (m_mutex);
      if (!m_settings)
        return false;

      bool ok = m_settings->load (m_filename);
      if (!ok)
        {
          // Best-effort: back up whatever is currently on disk (no-op if
          // nothing exists there).
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
                LumexLogging::error (GUARD_LOG_MODULE,
                                     "Failed to create default settings for '",
                                     m_filename, "'.");
              return false;
            }

          ok = m_settings->load (m_filename);
        }

      if (!ok && logOnFinalFailure)
        LumexLogging::error (GUARD_LOG_MODULE, "Settings file '", m_filename,
                             "' is still invalid after repair.");

      return ok;
    }
  catch (std::exception const &exc)
    {
      log_exception (logOnFinalFailure,
                     logOnFinalFailure ? ENSURE_EXISTS_NAME : REPAIR_NAME,
                     m_filename, exc.what ());
    }
  catch (...)
    {
      log_exception (logOnFinalFailure,
                     logOnFinalFailure ? ENSURE_EXISTS_NAME : REPAIR_NAME,
                     m_filename, UNKNOWN_EXCEPTION);
    }
  return false;
}

LUMEX_PUBLIC_API
bool
LumexSettingsGuard::ensure_keys_with_defaults (
    std::vector<lumex_settings_key_spec_t> const &specs) LUMEX_NOEXCEPT
{
  // As in `_ensure_or_repair_impl`: an exception from the guarded settings
  // object
  // (`get`, `add` or `save`) ends the call as a failure. Defaults already
  // added in memory stay there; nothing is saved after the exception.
  try
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
        LumexLogging::warning (GUARD_LOG_MODULE,
                               "Failed to persist repaired keys to '",
                               m_filename, "'.");

      return saved;
    }
  catch (std::exception const &exc)
    {
      log_exception (false, ENSURE_KEYS_NAME, m_filename, exc.what ());
    }
  catch (...)
    {
      log_exception (false, ENSURE_KEYS_NAME, m_filename, UNKNOWN_EXCEPTION);
    }
  return false;
}
} // namespace guard
} // namespace settings
} // namespace applied
} // namespace lumex

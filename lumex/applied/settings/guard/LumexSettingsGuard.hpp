/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexSettingsGuard.hpp
 * @brief Format-agnostic backup/repair/validation layer built on top of
 * `ILumexSettings`.
 * @details This header ports the "keep a settings file alive" behavior of
 * PeakExpertWeb's JSON-specific `BaseConfiguration` (default-key restoration,
 * corruption backup, and per-key validation) to a form that works with *any*
 * `ILumexSettings` implementation (INI today, JSON or others tomorrow), by
 * driving everything through the interface's `load`/`save`/`get`/`add`
 * contract instead of a concrete parser.
 */
#ifndef LUMEX_APPLIED_SETTINGS_GUARD_HPP
#define LUMEX_APPLIED_SETTINGS_GUARD_HPP

#include "lumex/LumexExport.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "lumex/applied/settings/interface/ILumexSettings.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace settings
{
namespace guard
{
/**
 * @brief Validation callback for a single settings key.
 * @details Receives the key's current raw string value, exactly as returned by
 *          `ILumexSettings::get()`, and returns `true` if that value is
 * acceptable as-is, `false` if it must be overwritten with the key's
 * configured default.
 * @note Must not throw; if it does, `LumexSettingsGuard` catches the exception
 * and treats it the same as returning `false` (value rejected, default
 * restored).
 */
using LumexSettingsValidateFn = std::function<bool (std::string const &)>;

/**
 * @brief Callback that (re)creates a settings file's default content from
 * scratch.
 * @details Invoked when the guarded file is missing, unreadable, or fails the
 * underlying `ILumexSettings::load()` call. A typical implementation populates
 * a fresh `ILumexSettings` instance with an application's default key/value
 * pairs and persists them via `save()` to the same path the guard was
 * constructed with.
 * @return `true` if defaults were written successfully, `false` otherwise.
 */
using LumexSettingsCreateFn = std::function<bool (void)>;

/**
 * @brief Describes a single settings key that
 * `LumexSettingsGuard::ensureKeysWithDefaults` must keep present and valid.
 * @details Mirrors the intent of `BaseConfiguration::key_spec_t`
 * (section/key/default/validate), adapted to `ILumexSettings`'s flat
 * string-value model. Unlike a JSON tree, the base interface has no way to
 * distinguish "key absent" from "key present with an empty value" - `get()`
 * returns an empty string for both - so both states are treated identically
 * here: a key is considered to need its default whenever `validate` rejects
 * its current value, or, when no `validate` is supplied, whenever that value
 *          is an empty string. This is not a loss of behavior versus the JSON
 * original: an empty string is already `BaseConfiguration`'s own fallback
 * definition of "no real value" for the no-validator case.
 */
struct lumex_settings_key_spec_t
{
  std::string section; ///< Section name; empty is implementation-defined (e.g.
                       ///< "root").
  std::string key;     ///< Key name within the section.
  std::string default_value; ///< Value written when the key is missing, empty,
                             ///< or invalid.
  LumexSettingsValidateFn
      validate; ///< Optional; `nullptr` => validated only against "not empty".
};

/**
 * @brief Format-agnostic backup/repair/validation guard for an
 * `ILumexSettings` instance.
 * @details Adds the three behaviors PeakExpertWeb's `BaseConfiguration`
 * provided for its JSON configuration files, generalized to work through
 * `ILumexSettings` alone:
 *            - `ensureExistsWithDefaults` / `repairIfCorrupted`: if the
 * guarded file is missing or fails to `load()`, back it up (when it exists)
 * and regenerate it via a caller-supplied `LumexSettingsCreateFn`, then
 * re-validate.
 *            - `backup`: a static, format-independent file-level copy to
 *              `<filename>.bak.<YYYYMMDD-HHMMSS>`, implemented purely against
 *              `lumex::filesystem`/`lumex::path` (no dependency on any
 * concrete `ILumexSettings` implementation).
 *            - `ensureKeysWithDefaults`: walks a list of
 * `lumex_settings_key_spec_t` and restores each key's default value whenever
 * it is missing, empty, or fails its own `validate` callback. The class owns
 * no settings storage itself - it composes an existing
 *          `std::shared_ptr<ILumexSettings>` (INI today; any future
 * JSON/YAML/XML implementation of the same interface works unchanged) plus the
 * filesystem path that instance loads from and saves to.
 * @note Thread-safety: an internal `std::recursive_mutex` serializes
 * concurrent calls made through the *same* `LumexSettingsGuard` instance. This
 * is narrower than `BaseConfiguration`'s single process-wide mutex (shared
 * with its JSON file helper)
 *       - by design, since a `LumexSettingsGuard` composes a caller-owned
 *       `ILumexSettings` instance rather than a global file-access chokepoint.
 * If the same underlying file is also touched by other `ILumexSettings`
 * instances, other `LumexSettingsGuard`s, or unrelated code, callers remain
 * responsible for their own synchronization, exactly as `ILumexSettings`'s own
 * contract already requires of its implementations.
 * @note Exception safety: no public member function throws. Exceptions raised
 * by a caller-supplied `LumexSettingsValidateFn`/`LumexSettingsCreateFn` are
 * caught and treated as failure (validation rejected / default-creation
 * failed).
 */
class LUMEX_API LumexSettingsGuard final
{
public:
  /**
   * @brief Constructs a guard around an existing settings object and the path
   * it persists to.
   * @param settings The `ILumexSettings` instance to guard. May be any
   * concrete implementation (`LumexSettingsINI` today). A `nullptr` is
   * accepted defensively - every operation below then simply fails (returns
   * `false`) instead of crashing.
   * @param filename The filesystem path this guard loads from, saves to, and
   * backs up.
   */
  explicit LumexSettingsGuard (std::shared_ptr<ILumexSettings> settings,
                               std::string filename) LUMEX_NOEXCEPT;

  /**
   * @brief Ensures the guarded file exists and is loadable; repairs it via
   * `createDefault` otherwise.
   * @details If `ILumexSettings::load(filename)` fails (file missing,
   * unreadable, or fails the implementation's own parse/validity check), the
   * current file is backed up first (when present), `createDefault` is invoked
   * to regenerate it, and `load()` is retried once. Logs at error level if the
   * file is still not loadable afterward.
   * @param createDefault Functor that (re)writes default content to
   * `filename`. A `nullptr` or a callback returning `false` is treated as
   * repair failure.
   * @return `true` if the file exists and loads successfully after this call,
   * `false` otherwise.
   */
  bool ensureExistsWithDefaults (LumexSettingsCreateFn const &createDefault)
      LUMEX_NOEXCEPT;

  /**
   * @brief Identical repair behavior to `ensureExistsWithDefaults`, without
   * the final error-level log.
   * @details Provided as a separate entry point - mirroring
   * `BaseConfiguration::repairIfCorrupted`
   *          - for callers that want to probe/repair a file without that
   * failure being logged as an application error (e.g. a caller that will
   * itself report a more specific message).
   * @param createDefault Functor that (re)writes default content to
   * `filename`.
   * @return `true` if the file exists and loads successfully after this call,
   * `false` otherwise.
   */
  bool repairIfCorrupted (LumexSettingsCreateFn const &createDefault)
      LUMEX_NOEXCEPT;

  /**
   * @brief Ensures every key in `specs` is present and valid, restoring
   * `default_value` otherwise.
   * @details Loads no file itself - it validates/repairs whatever is currently
   * held by the guarded `ILumexSettings` instance (typically populated by a
   * prior `load()` or by `ensureExistsWithDefaults`), then persists the result
   * via `save(filename)` if, and only if, at least one key was changed.
   * @param specs The key specifications to enforce, in order.
   * @return `true` if at least one key was changed and the save succeeded;
   * `false` if nothing needed changing, or if saving the changes failed.
   */
  bool ensureKeysWithDefaults (
      std::vector<lumex_settings_key_spec_t> const &specs) LUMEX_NOEXCEPT;

  /**
   * @brief Copies `filename` to a sibling `<filename>.bak.<YYYYMMDD-HHMMSS>`
   * backup file.
   * @details Pure filesystem operation - independent of any `ILumexSettings`
   * implementation, so it works identically for INI, JSON, or any future
   * format. Does nothing (and returns `false`) if `filename` does not exist or
   * is not a regular file.
   * @param filename The path to back up.
   * @return `true` if the backup file was created successfully, `false`
   * otherwise.
   */
  static bool backup (std::string const &filename) LUMEX_NOEXCEPT;

  /// @return The guarded `ILumexSettings` instance (may be `nullptr` if
  /// constructed as such).
  std::shared_ptr<ILumexSettings> const &
  settings () const LUMEX_NOEXCEPT
  {
    return m_settings;
  }

  /// @return The filesystem path this guard loads from, saves to, and backs
  /// up.
  std::string const &
  filename () const LUMEX_NOEXCEPT
  {
    return m_filename;
  }

private:
  /**
   * @brief Shared implementation for
   * `ensureExistsWithDefaults`/`repairIfCorrupted`.
   * @param createDefault Functor that (re)writes default content to
   * `m_filename`.
   * @param logOnFinalFailure Whether to emit an error-level log if the file is
   * still not loadable after the repair attempt.
   * @return `true` if the file loads successfully by the end of this call,
   * `false` otherwise.
   */
  bool _ensureOrRepairImpl (LumexSettingsCreateFn const &createDefault,
                            bool logOnFinalFailure) LUMEX_NOEXCEPT;

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251) // Suppress C4251 for STL/shared_ptr members in
                                // DLL interface
#endif
  std::shared_ptr<ILumexSettings>
      m_settings;         ///< The guarded settings object; may be null.
  std::string m_filename; ///< Path this guard loads/saves/backs up.
  std::recursive_mutex
      m_mutex; ///< Serializes calls made through this instance.
#ifdef _WIN32
#pragma warning(pop)
#endif
};
} // namespace guard
} // namespace settings
} // namespace applied
} // namespace lumex

using LumexSettingsGuard = lumex::applied::settings::guard::LumexSettingsGuard;
using lumex_settings_key_spec_t
    = lumex::applied::settings::guard::lumex_settings_key_spec_t;
using LumexSettingsValidateFn
    = lumex::applied::settings::guard::LumexSettingsValidateFn;
using LumexSettingsCreateFn
    = lumex::applied::settings::guard::LumexSettingsCreateFn;

#endif // !LUMEX_APPLIED_SETTINGS_GUARD_HPP

/**
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

/**
 * @file LumexEnvironment.hpp
 * @brief Defines the LumexEnvironment class for cross-platform environment
 * variable management.
 * @details This header provides the declaration for the LumexEnvironment
 * class, which offers a unified, thread-safe, and exception-safe interface for
 * interacting with system environment variables across different operating
 * systems (Windows, Linux, macOS). It utilizes design patterns like Singleton
 * and Strategy to ensure a robust and flexible architecture.
 */
#ifndef LUMEX_CORE_ENVIRONMENT_ENV_HPP
#define LUMEX_CORE_ENVIRONMENT_ENV_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cstring> // std::strlen, std::strcpy
#include <memory>  // std::unique_ptr, std::addressof
#include <mutex>   // std::mutex, std::lock_guard
#include <string>  // std::string

#include "lumex/core/utility/LumexUtility"

#if LUMEX_OS_WINDOWS
#include <Windows.h> // GetEnvironmentVariableA/W
#include <cstdlib>   // _dupenv_s, free
#include <utility>
#endif

#include "lumex/LumexExport.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Contains classes and utilities for environment variable interaction.
 * @details This namespace groups all components related to environment
 * variable management within the LumexCore library, providing a clear
 *          separation of concerns.
 */
namespace environment
{
namespace env
{
/**
 * @brief Provides a cross-platform, and thread-safe way to manage
 *        environment variables.
 * @details This class implements the Singleton pattern to ensure a single,
 *          globally accessible instance for environment variable operations.
 *          It leverages the Strategy pattern internally to handle OS-specific
 *          details (Windows, Linux, macOS) while presenting a unified API.
 *          All operations are designed to be exception-safe, returning a
 *          dedicated result structure (EnvResult) with status and error codes
 *          instead of throwing exceptions. Resource management is handled
 * using RAII principles, particularly for dynamically allocated buffers. This
 * class is non-copyable and non-movable to maintain the integrity of the
 * singleton instance.
 *
 * @note This class adheres to C++11 standard and uses appropriate
 * platform-specific APIs for optimal performance and security (e.g., _dupenv_s
 * on Windows).
 *
 * @example
 * // Basic usage:
 * LumexEnvironment::EnvResult path_result = LumexEnvironment::get("PATH");
 * if (path_result.success) {
 *     std::cout << "PATH: " << path_result.value << std::endl;
 * } else {
 *     std::cerr << "Failed to get PATH. Error code: " <<
 * path_result.error_code
 * << std::endl;
 * }
 *
 * // Getting with a default fallback value:
 * std::string home_dir = LumexEnvironment::get_or("HOME", "/default/home");
 * std::cout << "Home directory: " << home_dir << std::endl;
 *
 * // Setting an environment variable:
 * if (LumexEnvironment::set("MY_APP_VAR", "my_value")) {
 *     std::cout << "MY_APP_VAR set successfully." << std::endl;
 * } else {
 *     std::cerr << "Failed to set MY_APP_VAR." << std::endl;
 * }
 *
 * // Checking if an environment variable exists:
 * if (LumexEnvironment::has("TEMP")) {
 *     std::cout << "TEMP variable exists." << std::endl;
 * }
 */
class LUMEX_API LumexEnvironment
{
public:
  /// @brief Type alias for std::size_t, used for buffer sizes and lengths.
  using size_type = std::size_t;
  /// @brief Type alias for std::string, used for environment variable names
  /// and values.
  using string_type = std::string;

  /// @brief Maximum recommended buffer size for environment variables.
  /// @note This value is chosen to be compatible with Windows' maximum path
  /// length multiplied by 16.
  static size_type const MAX_ENV_BUFFER_SIZE = 32767; // Windows MAX_PATH * 16

  /**
   * @brief Represents the result of an environment variable operation.
   * @details This structure encapsulates the variable's value, a success flag,
   *          and an error code (if applicable). It implements the Null Object
   *          pattern, providing a safe default state and methods to retrieve
   *          values with fallbacks, preventing null pointer dereferences or
   *          exceptions.
   *
   * @example
   * LumexEnvironment::EnvResult result =
   * LumexEnvironment::get("NON_EXISTENT_VAR"); if (!result) { // Implicit
   * conversion to bool for convenience std::cerr << "Variable not found. Error
   * code: " << result.error_code << std::endl; std::string fallback =
   * result.get_value_or("DEFAULT_FALLBACK"); std::cout << "Using fallback
   * value: " << fallback << std::endl;
   * }
   */
  struct EnvResult
  {
    /// @brief The value of the environment variable. Empty if not found or
    /// error.
    string_type value; // NOLINT(misc-non-private-member-variables-in-classes)

    /// @brief True if the operation was successful, false otherwise.
    bool success; // NOLINT(misc-non-private-member-variables-in-classes)

    /// @brief System-specific error code if `success` is false.
    int error_code; // NOLINT(misc-non-private-member-variables-in-classes)

    /**
     * @brief Constructs an EnvResult in a default (unsuccessful) state.
     * @details Initializes `success` to `false` and `error_code` to `0`.
     */
    EnvResult () : success (false), error_code (0) {}

    /**
     * @brief Constructs a successful EnvResult with a given value.
     * @param val The successful string value of the environment variable.
     * @note The value is moved to `this->value` for efficiency.
     */
    explicit EnvResult (string_type val)
        : value (std::move (val)), success (true), error_code (0)
    {
    }

    /**
     * @brief Constructs an unsuccessful EnvResult with an error code and an
     * optional fallback value.
     * @param err_code The error code indicating the reason for failure.
     * @param fallback An optional string to use as a fallback value if the
     * operation failed. This value will be stored in `this->value`.
     * @note The `fallback` value is moved to `this->value` for efficiency.
     */
    EnvResult (int err_code, string_type fallback = string_type ())
        : value (std::move (fallback)), success (false), error_code (err_code)
    {
    }

    /**
     * @brief Retrieves the variable's value or a specified fallback value if
     * the operation failed.
     * @param fallback The string to return if the operation was not
     * successful.
     * @return The actual value if the `success` flag is true, otherwise the
     * provided fallback value.
     */
    string_type
    get_value_or (string_type const &fallback) const
    {
      return success ? value : fallback;
    }

    /**
     * @brief Allows implicit conversion to bool to easily check the success
     * status.
     * @return True if the operation was successful (`success` is true), false
     * otherwise.
     * @note This enables convenient usage in conditional statements like `if
     * (result)`.
     */
    operator bool () const { return success; }
  };

private:
  /**
   * @brief Abstract base class for environment variable access strategies.
   * @details This interface defines the contract for OS-specific
   * implementations of environment variable operations. It is a key component
   * of the Strategy pattern, allowing LumexEnvironment to delegate
   * platform-dependent behavior without exposing implementation details.
   * @note This class handles basic C-string arguments and does not throw
   * exceptions.
   */
  class LUMEX_API
      EnvironmentStrategy // NOLINT(cppcoreguidelines-special-member-functions)
  {
  public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived strategy
     * classes.
     * @note Adheres to the Rule of Zero/Five for polymorphic base classes.
     */
    virtual ~EnvironmentStrategy () = default;
    /**
     * @brief Pure virtual method to retrieve the value of an environment
     * variable.
     * @param name The null-terminated C-string name of the environment
     * variable.
     * @return An EnvResult object containing the variable's value and
     * operation status.
     */
    virtual EnvResult get_variable (char const *name) const = 0;
    /**
     * @brief Pure virtual method to set or update an environment variable.
     * @param name The null-terminated C-string name of the environment
     * variable.
     * @param value The null-terminated C-string value to assign. If `nullptr`,
     * the variable should be unset.
     * @return True if the variable was successfully set/updated, false
     * otherwise.
     */
    virtual bool set_variable (char const *name, char const *value) const = 0;
    /**
     * @brief Pure virtual method to unset (remove) an environment variable.
     * @param name The null-terminated C-string name of the environment
     * variable to unset.
     * @return True if the variable was successfully unset, false otherwise.
     */
    virtual bool unset_variable (char const *name) const = 0;
  };

#if LUMEX_OS_WINDOWS
  /**
   * @brief Windows-specific implementation of the EnvironmentStrategy.
   * @details This class provides the concrete implementation for environment
   *          variable operations on Windows, utilizing Win32 API functions
   *          like `_dupenv_s` and `SetEnvironmentVariableA`.
   * @note This class inherits from `EnvironmentStrategy` to provide a
   * polymorphic interface.
   */
  class LUMEX_API WindowsEnvironmentStrategy : public EnvironmentStrategy
  {
  public:
    /**
     * @brief Retrieves the value of a specified environment variable on
     * Windows.
     * @param name The null-terminated C-string name of the environment
     * variable to retrieve.
     * @return An EnvResult object containing the variable's value and
     * operation status.
     * @throws Nothing, all errors are encapsulated in the EnvResult's
     * error_code.
     * @note Prioritizes `_dupenv_s` for security, falls back to
     * `GetEnvironmentVariableA`.
     */
    EnvResult get_variable (char const *name) const override;

    /**
     * @brief Sets or updates the value of a specified environment variable on
     * Windows.
     * @param name The null-terminated C-string name of the environment
     * variable to set.
     * @param value The null-terminated C-string value to assign. If `nullptr`,
     * the variable will be unset.
     * @return True if the variable was successfully set/updated, false
     * otherwise.
     * @note Does not throw.
     * @note Internally uses `SetEnvironmentVariableA`.
     */
    bool set_variable (char const *name, char const *value) const override;

    /**
     * @brief Unsets (removes) a specified environment variable on Windows.
     * @param name The null-terminated C-string name of the environment
     * variable to unset.
     * @return True if the variable was successfully unset, false otherwise.
     * @note Does not throw.
     * @note Internally calls `SetEnvironmentVariableA` with a `nullptr` value.
     */
    bool unset_variable (char const *name) const override;
  };

#else
  /**
   * @brief POSIX-compliant implementation of the EnvironmentStrategy.
   * @details This class provides the concrete implementation for environment
   *          variable operations on POSIX-compliant systems (Linux, macOS,
   * Unix), utilizing standard C library functions like `getenv`, `setenv`, and
   * `unsetenv`.
   * @note This class inherits from `EnvironmentStrategy` to provide a
   * polymorphic interface.
   */
  class LUMEX_API PosixEnvironmentStrategy : public EnvironmentStrategy
  {
  public:
    /**
     * @brief Retrieves the value of a specified environment variable on POSIX
     * systems.
     * @param name The null-terminated C-string name of the environment
     * variable to retrieve.
     * @return An EnvResult object containing the variable's value and
     * operation status.
     * @throws Nothing, all errors are encapsulated in the EnvResult's
     * error_code.
     * @note Internally uses `getenv`. Be aware that `getenv` is not
     * thread-safe in some older standards; this class uses a mutex to mitigate
     * this for its public methods.
     */
    EnvResult get_variable (char const *name) const override;

    /**
     * @brief Sets or updates the value of a specified environment variable on
     * POSIX systems.
     * @param name The null-terminated C-string name of the environment
     * variable to set.
     * @param value The null-terminated C-string value to assign. If `nullptr`,
     * the variable will be unset.
     * @return True if the variable was successfully set/updated, false
     * otherwise.
     * @note Does not throw.
     * @note Prioritizes `setenv` (POSIX.1-2001) if available, falls back to
     * `putenv`. When using `putenv`, this method manages memory for the
     * environment string.
     */
    bool set_variable (char const *name, char const *value) const override;

    /**
     * @brief Unsets (removes) a specified environment variable on POSIX
     * systems.
     * @param name The null-terminated C-string name of the environment
     * variable to unset.
     * @return True if the variable was successfully unset, false otherwise.
     * @note Does not throw.
     * @note Prioritizes `unsetenv` (POSIX.1-2001) if available, falls back to
     * setting the variable to an empty string.
     */
    bool unset_variable (char const *name) const override;
  };
#endif

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  /// @brief Mutex for ensuring thread-safe access to environment variables.
  /// @details Declared `mutable` to allow `const` member functions to acquire
  /// locks,
  ///          as locking does not logically modify the object's observable
  ///          state.
  mutable std::mutex m_mutex;
  /// @brief Unique pointer to the concrete `EnvironmentStrategy`
  /// implementation.
  /// @details This member holds the OS-specific strategy, ensuring proper
  /// lifetime
  ///          management through `std::unique_ptr` (RAII). It is initialized
  ///          once during the `LumexEnvironment` singleton's creation.
  std::unique_ptr<EnvironmentStrategy> m_strategy;
#ifdef _WIN32
#pragma warning(pop)
#endif

  /**
   * @brief Private constructor for the `LumexEnvironment` class.
   * @details This constructor is private to enforce the Singleton pattern,
   *          ensuring that only the `instance()` method can create a
   * `LumexEnvironment` object. It initializes the appropriate
   * `EnvironmentStrategy` based on the operating system.
   */
  LumexEnvironment () : m_strategy (create_strategy ()) {}

  /**
   * @brief Factory method for creating the appropriate `EnvironmentStrategy`
   * based on the operating system.
   * @details This static method determines whether to instantiate
   * `WindowsEnvironmentStrategy` or `PosixEnvironmentStrategy` at compile
   * time, based on the `LUMEX_OS_WINDOWS` macro.
   * @return A `std::unique_ptr` to the newly created `EnvironmentStrategy`
   * instance.
   */
  static std::unique_ptr<EnvironmentStrategy>
  create_strategy ()
  {
#if LUMEX_OS_WINDOWS
    return std::unique_ptr<EnvironmentStrategy> (
        new WindowsEnvironmentStrategy ());
#else
    return std::unique_ptr<EnvironmentStrategy> (
        new PosixEnvironmentStrategy ());
#endif
  }

public:
  /**
   * @brief Provides the single, globally accessible instance of
   * LumexEnvironment.
   * @details This method implements the thread-safe Singleton pattern (using
   *          C++11 static initialization magic) to ensure only one instance
   *          of the environment manager exists throughout the application's
   * lifetime.
   * @return A reference to the singleton LumexEnvironment instance.
   * @note The instance is lazily initialized upon the first call to this
   * method.
   *
   * @example
   * LumexEnvironment& env_instance = LumexEnvironment::instance();
   * // Now use env_instance to call non-static methods if preferred
   * LumexEnvironment::EnvResult path_res =
   * env_instance.get_environment_variable("PATH");
   */
  static LumexEnvironment &instance ();

  /**
   * @brief Deleted copy constructor.
   * @details Ensures that LumexEnvironment objects cannot be copied,
   *          maintaining the integrity of the singleton pattern.
   */
  LumexEnvironment (LumexEnvironment const &) = delete;

  /**
   * @brief Deleted copy assignment operator.
   * @details Ensures that LumexEnvironment objects cannot be assigned,
   *          maintaining the integrity of the singleton pattern.
   */
  LumexEnvironment &operator= (LumexEnvironment const &) = delete;

  /**
   * @brief Default destructor.
   * @details Explicitly defined to ensure proper resource management and
   *          adherence to the Rule of Five. It will automatically deallocate
   *          the `m_strategy` unique pointer.
   */
  ~LumexEnvironment () = default;

  /**
   * @brief Deleted move constructor.
   * @details Ensures that LumexEnvironment objects cannot be moved,
   *          maintaining the integrity of the singleton pattern.
   */
  LumexEnvironment (LumexEnvironment &&) = delete;

  /**
   * @brief Deleted move assignment operator.
   * @details Ensures that LumexEnvironment objects cannot be move-assigned,
   *          maintaining the integrity of the singleton pattern.
   */
  LumexEnvironment &operator= (LumexEnvironment &&) = delete;

  /**
   * @brief Retrieves the value of a specified environment variable in a
   * thread-safe manner.
   * @details This method provides a cross-platform way to get environment
   * variable values. It handles different OS-specific APIs internally and
   * returns an EnvResult structure, which indicates success or failure and
   * contains the value or an error code, without throwing exceptions.
   * A mutex ensures thread-safety during the underlying system call.
   * @param name The null-terminated C-string name of the environment variable
   * to retrieve. Must not be `nullptr` or an empty string.
   * @return An EnvResult object containing the variable's value and operation
   * status. If `name` is invalid (`nullptr`), returns an unsuccessful
   * EnvResult with an error code (-1).
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * LumexEnvironment::EnvResult user_result =
   * LumexEnvironment::instance().get_environment_variable("USER"); if
   * (user_result.success) { std::cout << "Current user: " << user_result.value
   * << std::endl; } else { std::cerr << "Could not get USER variable. Error: "
   * << user_result.error_code << std::endl;
   * }
   */
  EnvResult get_environment_variable (char const *name) const;

  /**
   * @brief Convenience overload for `get_environment_variable` using
   * `std::string`.
   * @details This method converts the `std::string` name to a C-string
   *          and delegates to the `char const *` overload.
   * @param name The `std::string` name of the environment variable to
   * retrieve.
   * @return An EnvResult object containing the variable's value and operation
   * status.
   * @note This method is thread-safe due to the underlying `char const *`
   * overload.
   * @throws Nothing (delegates exception safety to the underlying C-string
   * overload).
   */
  EnvResult get_environment_variable (string_type const &name) const;

  /**
   * @brief Sets or updates the value of a specified environment variable in a
   * thread-safe manner.
   * @details This method allows setting new environment variables or modifying
   * existing ones. On POSIX systems, it attempts to use `setenv` if available
   * (C++11 + POSIX.1-2001), falling back to `putenv` for broader
   * compatibility. On Windows, it uses `SetEnvironmentVariableA`. A mutex
   * ensures thread-safety.
   * @param name The null-terminated C-string name of the environment variable
   * to set. Must not be `nullptr` or an empty string.
   * @param value The null-terminated C-string value to assign to the variable.
   *              If `nullptr`, the variable will be unset (removed).
   * @param overwrite If `false` and the variable already exists (regardless of
   *                  its current value), it is left untouched and this method
   *                  returns `true` without modifying it. If `true` (the
   *                  default, preserving prior behavior), an existing variable
   *                  is always overwritten. Ignored when `value` is `nullptr`.
   * @return True if the variable was successfully set/unset, or already
   * existed with `overwrite` set to `false`; false otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::instance().set_environment_variable("LOG_LEVEL",
   * "DEBUG")) { std::cout << "LOG_LEVEL set to DEBUG." << std::endl; } else {
   *     std::cerr << "Failed to set LOG_LEVEL." << std::endl;
   * }
   *
   * // Unsetting a variable:
   * if (LumexEnvironment::instance().set_environment_variable("OLD_VAR",
   * nullptr)) { std::cout << "OLD_VAR unset successfully." << std::endl;
   * }
   *
   * // Setting only if not already present:
   * LumexEnvironment::instance().set_environment_variable("CONFIG_PATH",
   * "/default/path", false);
   */
  bool set_environment_variable (char const *name, char const *value,
                                 bool overwrite = true) const;

  /**
   * @brief Convenience overload for `set_environment_variable` using
   * `std::string`.
   * @details This method converts the `std::string` name and value to
   * C-strings and delegates to the `char const *` overloads.
   * @param name The `std::string` name of the environment variable to set.
   * @param value The `std::string` value to assign to the variable.
   * @param overwrite If `false`, an already-existing variable is left
   * untouched. Defaults to `true`, preserving prior always-overwrite behavior.
   * @return True if the variable was successfully set, false otherwise.
   * @note This method is thread-safe due to the underlying `char const *`
   * overload.
   * @throws Nothing (delegates exception safety to the underlying C-string
   * overload).
   */
  bool set_environment_variable (string_type const &name,
                                 string_type const &value,
                                 bool overwrite = true) const;

  /**
   * @brief Unsets (removes) a specified environment variable in a thread-safe
   * manner.
   * @details This method removes an environment variable. On Windows, it sets
   * the variable's value to `nullptr`. On POSIX systems, it uses `unsetenv` if
   * available, or a fallback of setting the variable to an empty string.
   * A mutex ensures thread-safety.
   * @param name The null-terminated C-string name of the environment variable
   * to unset. Must not be `nullptr` or an empty string.
   * @return True if the variable was successfully unset, false otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::instance().unset_environment_variable("TEMP_VAR")) {
   *     std::cout << "TEMP_VAR unset successfully." << std::endl;
   * } else {
   *     std::cerr << "Failed to unset TEMP_VAR." << std::endl;
   * }
   */
  bool unset_environment_variable (char const *name) const;

  /**
   * @brief Retrieves the value of an environment variable, providing a default
   * fallback.
   * @details This is a convenience method that returns the variable's value if
   * found, or a specified `default_value` if the variable does not exist or
   *          could not be retrieved. It simplifies usage by avoiding manual
   *          `EnvResult` checking for simple retrieval scenarios.
   * @param name The null-terminated C-string name of the environment variable.
   * @param default_value The `std::string` value to return if the variable is
   * not found or retrieval failed.
   * @return The value of the environment variable if found and successful, or
   * `default_value` otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * std::string editor =
   * LumexEnvironment::instance().get_environment_variable_or("EDITOR", "vim");
   * std::cout << "Preferred editor: " << editor << std::endl;
   */
  string_type
  get_environment_variable_or (char const *name,
                               string_type const &default_value) const;

  /**
   * @brief Checks if a specified environment variable exists.
   * @details This is a convenience method that quickly determines the presence
   *          of an environment variable without retrieving its value. It
   *          internally calls `get_environment_variable` and checks its
   * success status.
   * @param name The null-terminated C-string name of the environment variable
   * to check. Must not be `nullptr` or an empty string.
   * @return True if the environment variable exists and was successfully
   * retrieved (even if its value is empty), false otherwise (e.g., not found,
   * or invalid name).
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::instance().has_environment_variable("PROGRAMFILES"))
   * { std::cout << "PROGRAMFILES variable is present." << std::endl;
   * }
   */
  bool has_environment_variable (char const *name) const;

  /**
   * @brief Checks whether an environment variable is "truthy" (enabled).
   * @details A variable is considered truthy unless it is unset, empty, or its
   *          value is equal to "0" or "false" once lower-cased (so "FALSE",
   *          "False", and "false" are all treated as disabled). Any other
   *          non-empty value, including "1", "yes", or "true", is truthy.
   * @param name The null-terminated C-string name of the environment variable
   * to check. Must not be `nullptr` or an empty string.
   * @return True if the variable is set to a truthy value, false otherwise
   * (including when the variable does not exist).
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if
   * (LumexEnvironment::instance().is_environment_variable_truthy("FEATURE_X"))
   * { std::cout << "Feature X is enabled." << std::endl;
   * }
   */
  bool is_environment_variable_truthy (char const *name) const;

  // Static convenience methods for easier usage
  /**
   * @brief Static convenience method to retrieve an environment variable.
   * @details This is a shortcut for
   * `LumexEnvironment::instance().get_environment_variable(name)`. It provides
   * a global access point for reading environment variables without needing to
   * explicitly get the `LumexEnvironment` singleton instance.
   * @param name The null-terminated C-string name of the environment variable.
   * @return An EnvResult object containing the variable's value and operation
   * status.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * auto locale_result = LumexEnvironment::get("LANG");
   * if (locale_result) {
   *     std::cout << "System language: " << locale_result.value << std::endl;
   * }
   */
  static EnvResult get (char const *name);

  /**
   * @brief Static convenience method to retrieve an environment variable with
   * a default fallback.
   * @details This is a shortcut for
   * `LumexEnvironment::instance().get_environment_variable_or(name,
   * default_value)`. It provides a simple way to read an environment variable
   * with a default value if the variable is not found or cannot be retrieved.
   * @param name The null-terminated C-string name of the environment variable.
   * @param default_value The default value to return if the variable is not
   * found.
   * @return The value of the environment variable if found, or `default_value`
   * if not found.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * std::string temp_path = LumexEnvironment::get_or("TMP", "/tmp");
   * std::cout << "Temporary path: " << temp_path << std::endl;
   */
  static string_type get_or (char const *name,
                             string_type const &default_value);

  /**
   * @brief Static convenience method to set an environment variable.
   * @details This is a shortcut for
   * `LumexEnvironment::instance().set_environment_variable(name, value,
   * overwrite)`. It provides a global access point for setting environment
   * variables.
   * @param name The null-terminated C-string name of the environment variable.
   * @param value The null-terminated C-string value to assign (`nullptr` to
   * unset).
   * @param overwrite If `false`, an already-existing variable is left
   * untouched. Defaults to `true`, preserving prior always-overwrite behavior.
   * @return True if successful, false otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::set("APP_MODE", "PRODUCTION")) {
   *     std::cout << "App mode set to PRODUCTION." << std::endl;
   * }
   */
  static bool set (char const *name, char const *value, bool overwrite = true);

  /**
   * @brief Static convenience method to check if an environment variable
   * exists.
   * @details This is a shortcut for
   * `LumexEnvironment::instance().has_environment_variable(name)`. It provides
   * a quick global check for the existence of an environment variable.
   * @param name The null-terminated C-string name of the environment variable.
   * @return True if the variable exists, false otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::has("HOME")) {
   *     std::cout << "HOME variable is defined." << std::endl;
   * }
   */
  static bool has (char const *name);

  /**
   * @brief Static convenience method to check whether an environment variable
   * is truthy (enabled).
   * @details This is a shortcut for
   * `LumexEnvironment::instance().is_environment_variable_truthy(name)`. See
   * that method for the exact truthy/falsy value semantics.
   * @param name The null-terminated C-string name of the environment variable.
   * @return True if the variable is set to a truthy value, false otherwise.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * if (LumexEnvironment::is_truthy("FEATURE_X")) {
   *     std::cout << "Feature X is enabled." << std::endl;
   * }
   */
  static bool is_truthy (char const *name);
};

/**
 * @brief Checks whether an environment variable is set to a non-empty value.
 * @details A small free-function helper, equivalent to
 *          `!LumexEnvironment::get_or(name, {}).empty()`: true only when the
 *          variable exists and its value is not an empty string.
 * @param name Name of the environment variable to check.
 * @return True if the variable exists and is non-empty, false otherwise.
 * @note This function is thread-safe (delegates to `LumexEnvironment`).
 * @note Does not throw.
 */
LUMEX_API bool is_env_set (LumexEnvironment::string_type const &name);

/**
 * @brief Checks whether an environment variable is "truthy" (enabled).
 * @details A small free-function helper, equivalent to
 * `LumexEnvironment::is_truthy(name)`. A variable is truthy unless it is
 * unset, empty, or equal to "0"/"false" once lower-cased.
 * @param name Name of the environment variable to check.
 * @return True if the variable is truthy, false otherwise.
 * @note This function is thread-safe (delegates to `LumexEnvironment`).
 * @note Does not throw.
 */
LUMEX_API bool is_env_truthy (LumexEnvironment::string_type const &name);
} // namespace env
} // namespace environment
} // namespace core
} // namespace lumex

/**
 * @brief Alias for lumex::core::environment::LumexEnvironment.
 * @details This using declaration simplifies the usage of the LumexEnvironment
 * class by allowing it to be referred to without its full namespace
 * qualification.
 */
using LumexEnvironment = lumex::core::environment::env::LumexEnvironment;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_ENVIRONMENT_ENV_HPP

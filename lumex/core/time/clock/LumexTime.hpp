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
 * @file LumexTime.hpp
 * @brief Provides cross-platform utilities for time and date manipulation.
 * @details This header defines the `LumexTime` class, offering a set of static
 * methods for retrieving current date and time in various formats, as well as
 *          timestamps in different time units (nanoseconds, microseconds,
 * milliseconds, seconds, minutes, hours, days, weeks, months, years). It also
 * includes a `Constants` namespace with frequently used time-related numerical
 * constants. The implementation aims for cross-platform compatibility and ease
 * of use.
 */
#ifndef LUMEX_CORE_TIME_CLOCK_HPP
#define LUMEX_CORE_TIME_CLOCK_HPP

#include "lumex/LumexExport.hpp" // For LUMEX_API and LUMEX_EXTERN_C_BEGIN/END

#include <chrono> // For std::chrono::system_clock::time_point
#include <ctime>  // For std::time_t, std::time
#include <string> // For std::string

#include "lumex/core/utility/LumexUtility" // Potentially for LUMEX_ATTRIBUTE_NODISCARD, etc.
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

LUMEX_EXTERN_C_BEGIN

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Core library components.
 * @details This namespace encapsulates fundamental, low-level utilities
 *          and data structures used across the Lumex Core library.
 */
namespace time
{
namespace clock
{
/**
 * @brief Contains compile-time constants related to time calculations.
 * @details This namespace provides a collection of `constexpr` unsigned long
 * and long long constants that represent various time unit conversions and
 * buffer sizes. These are used internally by `LumexTime` for precision and
 * efficiency in timestamp calculations and formatting.
 */
namespace Constants
{
/**
 * @brief Default buffer size for date and time strings.
 * @details Specifies the recommended buffer size for C-style character arrays
 *          used in `strftime` or similar functions when formatting date and
 * time.
 */
LUMEX_CONSTEXPR unsigned long KDEFAULT_DATETIME_BUF_SIZE = 128UL;

/**
 * @brief Number of microseconds in one millisecond.
 * @details Used for converting between microseconds and milliseconds.
 */
LUMEX_CONSTEXPR int K_MICROSECONDS_IN_MILLISECOND = 1000;

/**
 * @brief Number of nanoseconds in one microsecond.
 */
LUMEX_CONSTEXPR long long NS_IN_MCS = 1000LL;

/**
 * @brief Number of nanoseconds in one millisecond.
 * @details Calculated as `NS_IN_MCS * 1000LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_MS = 1000LL * NS_IN_MCS;

/**
 * @brief Number of nanoseconds in one second.
 * @details Calculated as `NS_IN_MS * 1000LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_S = 1000LL * NS_IN_MS;

/**
 * @brief Number of nanoseconds in one minute.
 * @details Calculated as `NS_IN_S * 60LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_MIN = 60LL * NS_IN_S;

/**
 * @brief Number of nanoseconds in one hour.
 * @details Calculated as `NS_IN_MIN * 60LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_H = 60LL * NS_IN_MIN;

/**
 * @brief Number of nanoseconds in one day.
 * @details Calculated as `NS_IN_H * 24LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_D = 24LL * NS_IN_H;

/**
 * @brief Number of nanoseconds in one week.
 * @details Calculated as `NS_IN_D * 7LL`.
 */
LUMEX_CONSTEXPR long long NS_IN_W = 7LL * NS_IN_D;

/**
 * @brief Approximate number of nanoseconds in one month.
 * @details Approximation based on 30 days per month: `NS_IN_D * 30LL`.
 * @warning This is an approximation and does not account for varying month
 * lengths.
 */
LUMEX_CONSTEXPR long long NS_IN_M = 30LL * NS_IN_D; // Approximation

/**
 * @brief Approximate number of nanoseconds in one year.
 * @details Approximation based on 365 days per year: `NS_IN_D * 365LL`.
 * @warning This is an approximation and does not account for leap years.
 */
LUMEX_CONSTEXPR long long NS_IN_Y = 365LL * NS_IN_D; // Approximation
} // namespace Constants

/**
 * @brief Provides static utility methods for time and date handling.
 * @details The `LumexTime` class offers a convenient, static interface
 *          for common time-related operations. It abstracts away
 *          platform-specific details and provides a consistent API
 *          for retrieving current date/time strings and timestamps
 *          in various granularities. All methods are designed to be
 *          thread-safe as they primarily involve reading system time
 *          and performing calculations without shared mutable state.
 */
class LUMEX_API LumexTime
{
public:
  /**
   * @brief Retrieves the current date and time formatted as a string.
   * @details This method captures the current system time and formats it
   *          according to the specified format string, similar to `strftime`.
   *          It is useful for logging, UI display, or generating timestamped
   * filenames.
   * @param format A C-style string specifying the desired date and time
   * format. Uses `strftime` format codes (e.g., "%Y-%m-%d %H:%M:%S"). Default
   * is Russian format: "%d.%m.%Y_%H:%M:%S".
   * @return A `std::string` containing the current date and time in the
   * specified format.
   * @warning Ensure the format string is valid for `strftime` on your target
   * platform.
   * @note Complexity: Dependent on the length of the formatted string and
   * underlying system calls.
   */
  static std::string get_current_datetime (char const *format
                                           = "%d.%m.%Y_%H:%M:%S");

  /**
   * @brief Gettters for the time since epoch in different time units.
   * @details These static methods provide the current system time
   *          as a string, representing the duration since the Unix epoch
   *          (January 1, 1970, 00:00:00 UTC) in various time granularities.
   *          They are suitable for high-resolution timing, unique ID
   * generation, or performance measurement.
   * @return A `std::string` representation of the timestamp.
   * @note Complexity: O(1) for time acquisition, O(log N) or O(N) for string
   * conversion depending on implementation.
   */
  static std::string
  get_timestamp_ns (); ///< @brief Gets the current timestamp in nanoseconds
                       ///< since epoch.
  static std::string
  get_timestamp_mcs (); ///< @brief Gets the current timestamp in microseconds
                        ///< since epoch.
  static std::string
  get_timestamp_ms (); ///< @brief Gets the current timestamp in milliseconds
                       ///< since epoch.
  static std::string get_timestamp_s (); ///< @brief Gets the current timestamp
                                         ///< in seconds since epoch.
  static std::string
  get_timestamp_min (); ///< @brief Gets the current timestamp in minutes since
                        ///< epoch.
  static std::string get_timestamp_h (); ///< @brief Gets the current timestamp
                                         ///< in hours since epoch.
  static std::string get_timestamp_d (); ///< @brief Gets the current timestamp
                                         ///< in days since epoch.
  static std::string get_timestamp_w (); ///< @brief Gets the current timestamp
                                         ///< in weeks since epoch.
  static std::string get_timestamp_m (); ///< @brief Gets the current timestamp
                                         ///< in months (approx.) since epoch.
  static std::string get_timestamp_y (); ///< @brief Gets the current timestamp
                                         ///< in years (approx.) since epoch.

  /**
   * @brief Formats a point in time as a local-time string, with a robust
   * fallback chain.
   * @details Attempts to format `time_` as local time (`localtime_s` on
   * Windows, `localtime_r` on POSIX). If that fails, falls back to UTC
   * (`gmtime_s`/`gmtime_r`). If that also fails, returns the raw number of
   * seconds since the Unix epoch as a decimal string, so this method never
   * throws and never returns an empty string.
   * @param time_ The `std::time_t` value to format. Defaults to the current
   * system time.
   * @param fmt A `strftime`-compatible format string. Defaults to
   * "%Y%m%d-%H%M%S".
   * @return The formatted timestamp string, or a raw epoch-seconds string as a
   * last-resort fallback.
   * @note This method is thread-safe (uses only thread-safe `_r`/`_s` time
   * functions and local state).
   * @note Does not throw.
   *
   * @example
   * std::string log_stamp = LumexTime::timestamp();
   * std::string custom = LumexTime::timestamp(std::time(nullptr), "%Y-%m-%d");
   */
  static std::string timestamp (std::time_t time_ = std::time (nullptr),
                                std::string const &fmt = "%Y%m%d-%H%M%S");

  /**
   * @brief Formats a point in time as a local-time string with a millisecond
   * suffix.
   * @details Uses the same fallback chain as `timestamp()` (local time -> UTC
   * -> raw epoch seconds) for the seconds portion of `tp`, then appends a
   * ".mmm" millisecond suffix derived from `tp` itself. If the fallback chain
   * has to resort to the raw epoch-seconds string, no millisecond suffix is
   * appended.
   * @param tp The `std::chrono::system_clock::time_point` to format. Defaults
   * to the current time.
   * @param fmt A `strftime`-compatible format string, applied to the seconds
   * portion of `tp`. Defaults to "%Y-%m-%d %H:%M:%S".
   * @return The formatted timestamp string with a millisecond suffix, or a raw
   * epoch-seconds string as a last-resort fallback.
   * @note This method is thread-safe.
   * @note Does not throw.
   *
   * @example
   * std::string log_stamp = LumexTime::timestamp_ms();
   */
  static std::string timestamp_ms (std::chrono::system_clock::time_point tp
                                   = std::chrono::system_clock::now (),
                                   std::string const &fmt
                                   = "%Y-%m-%d %H:%M:%S");

private:
  /**
   * @brief Internal helper function to get a timestamp in a specified unit.
   * @details This private static method calculates the current time since the
   *          Unix epoch and divides it by a given divisor to convert it into
   *          the desired time unit. It forms the backbone for all public
   * `get_timestamp_` methods.
   * @param divisor The value by which to divide the total nanoseconds since
   * epoch to convert to the target unit (e.g., `Constants::NS_IN_MS` for
   * milliseconds).
   * @return A `std::string` representation of the calculated timestamp.
   * @note Complexity: O(1) for time acquisition, O(log N) or O(N) for string
   * conversion.
   */
  static std::string _get_timestamp (long long divisor);
};
} // namespace clock
} // namespace time
} // namespace core
} // namespace lumex

/**
 * @brief Global type alias for `lumex::core::time::LumexTime`.
 * @details This `using` declaration brings `LumexTime` into the global
 * namespace (or enclosing namespace where it's included), allowing for more
 * convenient usage without full namespace qualification.
 */
using LumexTime = lumex::core::time::clock::LumexTime;

LUMEX_EXTERN_C_END

#endif // !LUMEX_CORE_TIME_CLOCK_HPP

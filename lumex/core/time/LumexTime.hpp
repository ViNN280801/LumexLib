/**
 * @file LumexTime.hpp
 * @brief Provides cross-platform utilities for time and date manipulation.
 * @details This header defines the `LumexTime` class, offering a set of static methods
 *          for retrieving current date and time in various formats, as well as
 *          timestamps in different time units (nanoseconds, microseconds, milliseconds,
 *          seconds, minutes, hours, days, weeks, months, years). It also includes
 *          a `Constants` namespace with frequently used time-related numerical constants.
 *          The implementation aims for cross-platform compatibility and ease of use.
 */
#ifndef LUMEX_TIME_HPP
#define LUMEX_TIME_HPP

#include "lumex/LumexExport.hpp"           // For LUMEX_API and LUMEX_EXTERN_C_BEGIN/END
#include "lumex/core/utility/LumexUtility" // Potentially for LUMEX_ATTRIBUTE_NODISCARD, etc.

#include <string> // For std::string

LUMEX_EXTERN_C_BEGIN

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    /**
     * @brief Core library components.
     * @details This namespace encapsulates fundamental, low-level utilities
     *          and data structures used across the Lumex Core library.
     */
    namespace Time
    {
      /**
       * @brief Contains compile-time constants related to time calculations.
       * @details This namespace provides a collection of `constexpr` unsigned long and long long
       *          constants that represent various time unit conversions and buffer sizes.
       *          These are used internally by `LumexTime` for precision and efficiency in
       *          timestamp calculations and formatting.
       */
      namespace Constants
      {
        /**
         * @brief Default buffer size for date and time strings.
         * @details Specifies the recommended buffer size for C-style character arrays
         *          used in `strftime` or similar functions when formatting date and time.
         */
        constexpr unsigned long KDEFAULT_DATETIME_BUF_SIZE = 128UL;

        /**
         * @brief Number of microseconds in one millisecond.
         * @details Used for converting between microseconds and milliseconds.
         */
        constexpr int K_MICROSECONDS_IN_MILLISECOND = 1000;

        /**
         * @brief Number of nanoseconds in one microsecond.
         */
        constexpr long long NS_IN_MCS = 1000LL;

        /**
         * @brief Number of nanoseconds in one millisecond.
         * @details Calculated as `NS_IN_MCS * 1000LL`.
         */
        constexpr long long NS_IN_MS = 1000LL * NS_IN_MCS;

        /**
         * @brief Number of nanoseconds in one second.
         * @details Calculated as `NS_IN_MS * 1000LL`.
         */
        constexpr long long NS_IN_S = 1000LL * NS_IN_MS;

        /**
         * @brief Number of nanoseconds in one minute.
         * @details Calculated as `NS_IN_S * 60LL`.
         */
        constexpr long long NS_IN_MIN = 60LL * NS_IN_S;

        /**
         * @brief Number of nanoseconds in one hour.
         * @details Calculated as `NS_IN_MIN * 60LL`.
         */
        constexpr long long NS_IN_H = 60LL * NS_IN_MIN;

        /**
         * @brief Number of nanoseconds in one day.
         * @details Calculated as `NS_IN_H * 24LL`.
         */
        constexpr long long NS_IN_D = 24LL * NS_IN_H;

        /**
         * @brief Number of nanoseconds in one week.
         * @details Calculated as `NS_IN_D * 7LL`.
         */
        constexpr long long NS_IN_W = 7LL * NS_IN_D;

        /**
         * @brief Approximate number of nanoseconds in one month.
         * @details Approximation based on 30 days per month: `NS_IN_D * 30LL`.
         * @warning This is an approximation and does not account for varying month lengths.
         */
        constexpr long long NS_IN_M = 30LL * NS_IN_D; // Approximation

        /**
         * @brief Approximate number of nanoseconds in one year.
         * @details Approximation based on 365 days per year: `NS_IN_D * 365LL`.
         * @warning This is an approximation and does not account for leap years.
         */
        constexpr long long NS_IN_Y = 365LL * NS_IN_D; // Approximation
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
         *          It is useful for logging, UI display, or generating timestamped filenames.
         * @param format A C-style string specifying the desired date and time format.
         *               Uses `strftime` format codes (e.g., "%Y-%m-%d %H:%M:%S").
         *               Default is Russian format: "%d.%m.%Y_%H:%M:%S".
         * @return A `std::string` containing the current date and time in the specified format.
         * @warning Ensure the format string is valid for `strftime` on your target platform.
         * @complexity Dependent on the length of the formatted string and underlying system calls.
         */
        static std::string get_current_datetime(char const *format = "%d.%m.%Y_%H:%M:%S");

        /**
         * @brief Gettters for the time since epoch in different time units.
         * @details These static methods provide the current system time
         *          as a string, representing the duration since the Unix epoch
         *          (January 1, 1970, 00:00:00 UTC) in various time granularities.
         *          They are suitable for high-resolution timing, unique ID generation,
         *          or performance measurement.
         * @return A `std::string` representation of the timestamp.
         * @complexity O(1) for time acquisition, O(log N) or O(N) for string conversion depending on implementation.
         */
        static std::string get_timestamp_ns();  ///< @brief Gets the current timestamp in nanoseconds since epoch.
        static std::string get_timestamp_mcs(); ///< @brief Gets the current timestamp in microseconds since epoch.
        static std::string get_timestamp_ms();  ///< @brief Gets the current timestamp in milliseconds since epoch.
        static std::string get_timestamp_s();   ///< @brief Gets the current timestamp in seconds since epoch.
        static std::string get_timestamp_min(); ///< @brief Gets the current timestamp in minutes since epoch.
        static std::string get_timestamp_h();   ///< @brief Gets the current timestamp in hours since epoch.
        static std::string get_timestamp_d();   ///< @brief Gets the current timestamp in days since epoch.
        static std::string get_timestamp_w();   ///< @brief Gets the current timestamp in weeks since epoch.
        static std::string get_timestamp_m();   ///< @brief Gets the current timestamp in months (approx.) since epoch.
        static std::string get_timestamp_y();   ///< @brief Gets the current timestamp in years (approx.) since epoch.

      private:
        /**
         * @brief Internal helper function to get a timestamp in a specified unit.
         * @details This private static method calculates the current time since the
         *          Unix epoch and divides it by a given divisor to convert it into
         *          the desired time unit. It forms the backbone for all public `get_timestamp_` methods.
         * @param divisor The value by which to divide the total nanoseconds since epoch
         *                to convert to the target unit (e.g., `Constants::NS_IN_MS` for milliseconds).
         * @return A `std::string` representation of the calculated timestamp.
         * @complexity O(1) for time acquisition, O(log N) or O(N) for string conversion.
         */
        static std::string _get_timestamp(long long divisor);
      };
    } // namespace Time
  } // namespace Core
} // namespace Lumex

/**
 * @brief Global type alias for `Lumex::Core::Time::LumexTime`.
 * @details This `using` declaration brings `LumexTime` into the global namespace
 *          (or enclosing namespace where it's included), allowing for more convenient
 *          usage without full namespace qualification.
 */
using LumexTime = Lumex::Core::Time::LumexTime;

LUMEX_EXTERN_C_END

#endif // !LUMEX_TIME_HPP

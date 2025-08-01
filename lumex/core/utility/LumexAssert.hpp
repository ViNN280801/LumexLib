#ifndef LUMEX_ASSERT_HPP
#define LUMEX_ASSERT_HPP

#include "lumex/LumexExport.hpp"

LUMEX_EXTERN_C_BEGIN
/**
 * @brief Handles failed assertions within the Lumex codebase.
 *
 * This function serves as the central handler for assertion failures. It is
 * typically invoked by an assertion macro when a specified condition evaluates
 * to false. The handler receives details about the failed assertion, including
 * the assertion string itself, the source file where the failure occurred, and
 * the exact line number.
 *
 * Its primary responsibility is to process the assertion failure, which might
 * involve logging the details, breaking into a debugger, or terminating the
 * application. Implementations should provide clear diagnostic information
 * to aid in debugging.
 *
 * @param[in] assertion A null-terminated string representing the failed assertion expression.
 *                      This pointer is caller-owned and must remain valid for the duration of the call.
 * @param[in] file A null-terminated string representing the name of the source file
 *                 where the assertion failed. This pointer is caller-owned.
 * @param[in] line The line number within the `file` where the assertion failed.
 * @return This function does not return to the caller under normal assertion failure scenarios,
 *         as it is expected to either terminate the program or enter a debugger.
 * @note This function is declared `noexcept`, indicating that it does not throw C++ exceptions.
 *       Any internal failures should be handled without propagating exceptions.
 * @warning Calling this function directly is typically not recommended; it is intended
 *          to be used by internal assertion macros or mechanisms.
 */
LUMEX_PUBLIC_API
void lumex_assert_handler(char const *assertion, char const *file, int line) noexcept;
LUMEX_EXTERN_C_END

#define LUMEX_ASSERT(cond) ((cond) ? static_cast<void>(0) : lumex_assert_handler(#cond, __FILE__, __LINE__))

#endif // !LUMEX_ASSERT_HPP

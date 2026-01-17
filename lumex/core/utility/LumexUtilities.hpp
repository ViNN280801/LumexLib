#ifndef LUMEX_UTILITY_UTILITIES_HPP
#define LUMEX_UTILITY_UTILITIES_HPP

#include <cstdint>

#include "lumex/core/utility/LumexKeywords.hpp"

/**
 * @brief Safe cast from POSIX file descriptor (int) to void* for cross-platform storage.
 * @details
 * Problem:
 * - Windows: native handles are `HANDLE` (void*)
 * - POSIX/Linux: native handles are `int` (file descriptor)
 * - For uniform storage in the base class, a common type is needed -> `void*` is chosen
 *
 * Why direct reinterpret_cast<void*>(int) is dangerous:
 * On 64-bit systems:
 * - `int` takes 32 bits (signed: -2,147,483,648 .. 2,147,483,647)
 * - `void*` takes 64 bits
 * - Direct `reinterpret_cast<void*>(int)` is undefined behavior according to C++ standard:
 *   * Standard allows `reinterpret_cast` only for types of the same size
 *   * Behavior with different sizes is implementation-defined (not guaranteed)
 *
 * Why safe through intptr_t:
 * 1. `intptr_t` (from <cstdint>) guaranteed to have pointer size (32/64 bits)
 * 2. `static_cast<intptr_t>(fd)`:
 *    - POSIX file descriptors are always >= 0 (non-negative)
 *    - int (32 bits) extends to intptr_t (64 bits) with sign preservation:
 *      * fd >= 0: highest 32 bits = 0x00000000 (extension by zeros)
 *      * fd < 0:  highest 32 bits = 0xFFFFFFFF (extension by sign) - but POSIX fd is always >= 0!
 * 3. `reinterpret_cast<void*>(intptr_t)`:
 *    - Both types of the same size (64 bits) -> safe bitwise copy
 *    - Guaranteed by C++ standard for intptr_t ↔ void*
 *
 * Reverse conversion (Lumex::Utility::ptr_to_fd):
 * - `reinterpret_cast<intptr_t>(ptr)` -> get 64-bit value
 * - `static_cast<int>(intptr_t)` -> truncate to 32 bits (safe, since highest bits = 0)
 * - Get original file descriptor
 *
 * Architectural portability:
 * - 32-bit systems: int (32) -> intptr_t (32) -> void* (32)
 * - 64-bit systems: int (32) -> intptr_t (64) -> void* (64)
 * - ARM32/ARM64/x86/x64/RISC-V: all support intptr_t
 *
 * @param fd POSIX file descriptor (int >= 0)
 * @return void* representation of the descriptor for cross-platform storage
 *
 * @note Use ONLY for POSIX file descriptors (non-negative integers)!
 *       For Windows HANDLE directly assign to void* (already a pointer).
 *
 * @example Typical usage in a serial port:
 * @code
 * #if LUMEX_OS_WINDOWS
 *   m_hInputFile = m_serialPort.native_handle(); // Windows: HANDLE -> void*
 * #else
 *   m_hInputFile = Lumex::Utility::fd_to_ptr(m_serialPort.native_handle()); // POSIX: int -> void*
 * #endif
 *
 * // Reverse conversion:
 * #if !LUMEX_OS_WINDOWS
 *   int fd = Lumex::Utility::ptr_to_fd(m_hInputFile);
 *   ::close(fd);
 * #endif
 * @endcode
 *
 * @warning DO NOT use for arbitrary int values! Only for file descriptors!
 */
static inline void *
fd_to_ptr(int fileDescriptor) LUMEX_NOEXCEPT_FUNCTION
{
  return reinterpret_cast< // NOLINT(performance-no-int-to-ptr)
    void *>(static_cast<intptr_t>(fileDescriptor));
}

/**
 * @brief Safe reverse conversion of void* to POSIX file descriptor (int).
 * @details Reverse operation for Lumex::Utility::fd_to_ptr. Restores the original
 *          file descriptor from void* representation.
 *
 * Mechanism of work:
 * 1. `reinterpret_cast<intptr_t>(ptr)`:
 *    - void* (64 bits) -> intptr_t (64 bits) bitwise
 *    - Highest 32 bits = 0x00000000 (from the original FD_TO_PTR conversion)
 *    - Lowest 32 bits = original file descriptor
 * 2. `static_cast<int>(intptr_t)`:
 *    - Truncates to 32 bits (takes lowest bits)
 *    - Get original fd
 *
 * @param ptr void* representation of the descriptor (from Lumex::Utility::fd_to_ptr)
 * @return int POSIX file descriptor
 *
 * @example
 * @code
 * void* stored_ptr = Lumex::Utility::fd_to_ptr(5); // fd=5 -> void*
 * int fd = Lumex::Utility::ptr_to_fd(stored_ptr);  // void* -> fd=5
 * assert(fd == 5); // Restored original descriptor
 * @endcode
 */
inline static int
ptr_to_fd(void *pointer) LUMEX_NOEXCEPT_FUNCTION
{
  return static_cast<int>(reinterpret_cast<intptr_t>(pointer));
}

#endif // !LUMEX_UTILITY_UTILITIES_HPP

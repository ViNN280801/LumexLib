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

// NOLINTBEGIN(readability-simplify-boolean-expr)
#ifndef LUMEX_CORE_UTILITY_NUMERIC_HPP
#define LUMEX_CORE_UTILITY_NUMERIC_HPP

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

#include <atomic>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if __cplusplus >= 202002L
#include <compare>
#endif

// Cast mixed-signedness compares on every ISA. The old x86-only gate left
// x64/x86_64 on the uncast branch (MSVC C4018).
#define LUMEXLUMEX_SFNC_ARCH_X86 1

namespace lumex
{
namespace core
{
namespace utility
{
namespace numeric
{

/**
 * @file SafeNumericComparator.hpp
 * @brief Universal thread-safe comparator for safe comparison of arithmetic
 * types
 * @details Prevents overflow and incorrect comparisons between types with
 * different ranges. Supports all arithmetic types (integral and
 * floating-point) with compile-time optimizations.
 * @since C++11
 * @note Thread-safe, exception-safe, no-throw where possible
 *
 * @section problem Problem: Unsafe comparison of different types
 *
 * In C++, comparing variables of different types can cause serious problems:
 *
 * @subsection overflow Overflow problem
 * Comparing types with different value ranges triggers implicit type
 * conversion, which can overflow and cause undefined behavior (UB):
 *
 * @code
 * unsigned char block_size = 200;  // Range: 0-255
 * int packet_size = 300;           // Range: -2,147,483,648 to 2,147,483,647
 *
 * // UNSAFE! May produce an incorrect result
 * if (block_size >= packet_size) {
 *   // Intended: 200 >= 300 should be false
 *   // Actual: unsigned char(300) = 44 (300 % 256)
 *   // Result: 200 >= 44 = true (WRONG!)
 * }
 * @endcode
 *
 * @subsection floating_point Floating-point problem
 * Comparing integers to floating-point values is also problematic:
 *
 * @code
 * int counter = 3;
 * float threshold = 3.14f;
 *
 * // UNSAFE! NaN and infinities are not handled
 * if (counter >= threshold) {
 *   // If threshold is NaN the result is indeterminate
 *   // If threshold is +infinity the result is always false
 *   // If threshold is -infinity the result is always true
 * }
 * @endcode
 *
 * @subsection consequences Consequences
 * - Incorrect program logic
 * - Undefined behavior (UB)
 * - Potential security vulnerabilities
 * - Difficult debugging
 *
 * @section solution Solution: SafeComparator
 *
 * SafeComparator addresses these problems by:
 *
 * @subsection range_checking Range checking
 * Before comparing, checks whether the value fits the target type range:
 *
 * @code
 * // SAFE with SafeComparator
 * SafeUCharComparator safe_size(200);
 * if (safe_size.safe_compare(300)) {
 *   // Algorithm:
 *   // 1. Check: 300 > 255 (max unsigned char) = true
 *   // 2. Return: false (correct!)
 * }
 * @endcode
 *
 * @subsection floating_handling Floating-point handling
 * Handles special values correctly:
 *
 * @code
 * SafeIntComparator safe_counter(3);
 * float threshold = std::numeric_limits<float>::quiet_NaN();
 *
 * if (safe_counter.safe_compare(threshold)) {
 *   // Algorithm:
 *   // 1. Check: threshold is NaN = true
 *   // 2. Return: false (correct!)
 * }
 * @endcode
 *
 * @subsection compile_time Compile-time optimizations
 * Uses SFINAE to select the optimal comparison algorithm:
 * - Same types: direct comparison (fastest)
 * - Integer types: range check plus conversion
 * - Floating-point types: NaN/infinity handling plus conversion
 * - Mixed types: combined handling
 *
 * @section examples Practical examples
 *
 * @subsection bad_example Example WITHOUT SafeComparator (unsafe)
 *
 * @code
 * // Problematic code
 * struct DataBlock {
 *     unsigned char m_Size;  // 0-255
 *     // ...
 * };
 *
 * void processData(const DataBlock& block) {
 *     int maxPacketSize = 1000;  // May be any value
 *
 *     // CRITICAL ERROR!
 *     if (block.m_Size >= maxPacketSize) {
 *         // Issue: unsigned char(1000) = 232 (1000 % 256)
 *         // Result: any m_Size >= 232 yields true
 *         // For example: m_Size = 200, but 200 >= 232 = false (correct)
 *         // But: m_Size = 250, and 250 >= 232 = true (incorrect!)
 *         processLargePacket();
 *     } else {
 *         processSmallPacket();
 *     }
 * }
 * @endcode
 *
 * @subsection good_example Example WITH SafeComparator (safe)
 *
 * @code
 * // Safe code
 * struct DataBlock {
 *     unsigned char m_Size;  // 0-255
 *     // ...
 * };
 *
 * void processData(const DataBlock& block) {
 *     int maxPacketSize = 1000;  // May be any value
 *
 *     // SAFE!
 *     SafeUCharComparator safeSize(block.m_Size);
 *     if (safeSize.safe_compare(maxPacketSize)) {
 *         // SafeComparator algorithm:
 *         // 1. Check: maxPacketSize(1000) > 255 (max unsigned char) = true
 *         // 2. Return: false (correct!)
 *         // Result: processSmallPacket() is called correctly
 *         processLargePacket();
 *     } else {
 *         processSmallPacket();  // Correct choice!
 *     }
 * }
 * @endcode
 *
 * @subsection comprehensive_example Complete example of all comparison
 * operations
 *
 * @code
 * // Demonstration of all comparison operations
 * void demonstrateAllComparisons() {
 *     unsigned char size = 200;
 *     int threshold = 300;
 *     float precision = 3.14f;
 *
 *     SafeUCharComparator safeSize(size);
 *
 *     // All comparison kinds
 *     if (safeSize.safe_equal(threshold)) {
 *         // Safe comparison ==
 *     }
 *
 *     if (safeSize.safe_not_equal(threshold)) {
 *         // Safe comparison !=
 *     }
 *
 *     if (safeSize.safe_less(threshold)) {
 *         // Safe comparison <
 *     }
 *
 *     if (safeSize.safe_greater(threshold)) {
 *         // Safe comparison >
 *     }
 *
 *     if (safeSize.safe_less_equal(threshold)) {
 *         // Safe comparison <=
 *     }
 *
 *     if (safeSize.safe_greater_equal(threshold)) {
 *         // Safe comparison >=
 *     }
 *
 * #if __cplusplus >= 202002L
 *     // C++20 three-way comparison
 *     auto result = safeSize.safe_three_way_compare(threshold);
 *     if (is_equal(result)) {
 *         // Values are equal
 *     } else if (is_less(result)) {
 *         // First value is less
 *     } else if (is_greater(result)) {
 *         // First value is greater
 *     }
 * #endif
 *
 *     // Utility functions
 *     if (safe_equal(size, threshold)) {
 *         // Direct comparison without constructing an object
 *     }
 *
 *     if (safe_less(size, precision)) {
 *         // Comparison with floating-point
 *     }
 * }
 * @endcode
 *
 * @subsection why_it_works Why this works
 *
 * SafeComparator uses the following algorithm for unsigned char >= int:
 *
 * @code
 * bool safe_compare(unsigned char current, int other) {
 *     // 1. Check the range: other > max(unsigned char) = 255
 *     if (other > 255) return false;  // other is too large
 *
 *     // 2. Check the range: other < min(unsigned char) = 0
 *     if (other < 0) return true;      // other is negative
 *
 *     // 3. Safe comparison: convert to a common type
 *     return current >= static_cast<int>(other);
 * }
 * @endcode
 *
 * This approach guarantees:
 * - Correct comparison results
 * - No overflow
 * - Edge-case handling
 * - Compile-time type checks
 * - Thread-safety when required
 *
 * @section usage When to use
 *
 * Use SafeComparator when:
 * - comparing variables of different types
 * - handling user input
 * - building critical systems
 * - you need guaranteed-correct comparisons
 * - thread-safety is required
 *
 * @section performance Performance
 *
 * SafeComparator is optimized for performance:
 * - Compile-time dispatch (zero-cost abstractions)
 * - Same-type specializations (direct comparison)
 * - Minimal overhead for the checks
 * - Lock-free operations for atomic versions
 *
 * @section algorithm Full SafeComparator algorithm
 *
 * @subsection algorithm_overview Algorithm overview
 * SafeComparator uses a deterministic algorithm for safe comparison of any
 * arithmetic types. The algorithm runs in a fixed order :
 *
 * **INPUT**: current (type T), other (type U)
 * **OUTPUT**: bool (comparison result current >= other)
 *
 * @subsection step_by_step_algorithm Step-by-step algorithm
 *
 * **STEP 1: Type classification**
 * ```
 * ACTION 1.1: Determine type T
 * ACTION 1.2: Determine type U
 * ACTION 1.3: Compute same_type = (T == U)
 * ACTION 1.4: Compute both_integral = (T ∈ integral) AND (U ∈ integral)
 * ACTION 1.5: Compute both_floating = (T ∈ floating) AND (U ∈ floating)
 * ACTION 1.6: Compute mixed_types = NOT(same_type) AND NOT(both_integral) AND
 * NOT(both_floating)
 * ```
 *
 * **STEP 2: Choose comparison algorithm**
 * ```
 * IF same_type == true:
 *     GO TO ALGORITHM A (same types)
 * ELSE IF both_integral == true:
 *     GO TO ALGORITHM B (integer types)
 * ELSE IF both_floating == true:
 *     GO TO ALGORITHM C (floating-point types)
 * ELSE IF mixed_types == true:
 *     GO TO ALGORITHM D (mixed types)
 * ELSE:
 *     RAISE A COMPILE ERROR
 * ```
 *
 * **ALGORITHM A: Same-type comparison (T == U)**
 * ```
 * ACTION A.1: Perform a direct comparison: result = (current >= other)
 * ACTION A.2: Return result
 * ```
 *
 * **ALGORITHM B: Integer comparison (T != U, both integral)**
 * ```
 * ACTION B.1: Get max_T = std::numeric_limits<T>::max()
 * ACTION B.2: Get min_T = std::numeric_limits<T>::min()
 * ACTION B.3: IF other > max_T:
 *     ACTION B.3.1: Return false
 * ACTION B.4: IF other < min_T:
 *     ACTION B.4.1: Return true
 * ACTION B.5: Get CommonType = std::common_type<T, U>::type
 * ACTION B.6: Convert: current_common = static_cast<CommonType>(current)
 * ACTION B.7: Convert: other_common = static_cast<CommonType>(other)
 * ACTION B.8: Compare: result = (current_common >= other_common)
 * ACTION B.9: Return result
 * ```
 *
 * **ALGORITHM C: Floating-point comparison**
 * ```
 * ACTION C.1: IF std::isnan(current) == true OR std::isnan(other) == true:
 *     ACTION C.1.1: Return false
 * ACTION C.2: IF std::isinf(current) == true OR std::isinf(other) == true:
 *     ACTION C.2.1: Compare: result = (current == other)
 *     ACTION C.2.2: Return result
 * ACTION C.3: Get CommonType = std::common_type<T, U>::type
 * ACTION C.4: Convert: current_common = static_cast<CommonType>(current)
 * ACTION C.5: Convert: other_common = static_cast<CommonType>(other)
 * ACTION C.6: Compare: result = (current_common >= other_common)
 * ACTION C.7: Return result
 * ```
 *
 * **ALGORITHM D: Mixed-type comparison (integral + floating)**
 * ```
 * ACTION D.1: IF T ∈ integral AND U ∈ floating:
 *     GO TO SUB-ALGORITHM D1 (integral T, floating U)
 * ACTION D.2: IF T ∈ floating AND U ∈ integral:
 *     GO TO SUB-ALGORITHM D2 (floating T, integral U)
 * ACTION D.3: ELSE:
 *     RAISE A COMPILE ERROR
 * ```
 *
 * **SUB-ALGORITHM D1: integral T, floating U**
 * ```
 * ACTION D1.1: IF std::isnan(other) == true:
 *     ACTION D1.1.1: Return false
 * ACTION D1.2: IF std::isinf(other) == true:
 *     ACTION D1.2.1: IF other < 0:
 *         ACTION D1.2.1.1: Return true
 *     ACTION D1.2.2: ELSE:
 *         ACTION D1.2.2.1: Return false
 * ACTION D1.3: Get max_T = std::numeric_limits<T>::max()
 * ACTION D1.4: Get min_T = std::numeric_limits<T>::min()
 * ACTION D1.5: IF other > max_T:
 *     ACTION D1.5.1: Return false
 * ACTION D1.6: IF other < min_T:
 *     ACTION D1.6.1: Return true
 * ACTION D1.7: Get CommonType = std::common_type<T, U>::type
 * ACTION D1.8: Convert: current_common = static_cast<CommonType>(current)
 * ACTION D1.9: Convert: other_common = static_cast<CommonType>(other)
 * ACTION D1.10: Compare: result = (current_common >= other_common)
 * ACTION D1.11: Return result
 * ```
 *
 * **SUB-ALGORITHM D2: floating T, integral U**
 * ```
 * ACTION D2.1: IF std::isnan(current) == true:
 *     ACTION D2.1.1: Return false
 * ACTION D2.2: IF std::isinf(current) == true:
 *     ACTION D2.2.1: IF current > 0:
 *         ACTION D2.2.1.1: Return true
 *     ACTION D2.2.2: ELSE:
 *         ACTION D2.2.2.1: Return false
 * ACTION D2.3: Get CommonType = std::common_type<T, U>::type
 * ACTION D2.4: Convert: current_common = static_cast<CommonType>(current)
 * ACTION D2.5: Convert: other_common = static_cast<CommonType>(other)
 * ACTION D2.6: Compare: result = (current_common >= other_common)
 * ACTION D2.7: Return result
 * ```
 *
 * @subsection algorithm_example Worked example of the algorithm
 *
 * **INPUT**: current = 252 (unsigned char), other = 1652 (int)
 *
 * **STEP 1: Type classification**
 * - T = unsigned char, U = int
 * - same_type = false (unsigned char != int)
 * - both_integral = true (both integer)
 * - both_floating = false
 * - mixed_types = false
 *
 * **STEP 2: Algorithm selection**
 * - both_integral == true -> ALGORITHM B
 *
 * **ALGORITHM B: Integer comparison**
 * - ACTION B.1: max_T = 255 (maximum unsigned char)
 * - ACTION B.2: min_T = 0 (minimum unsigned char)
 * - ACTION B.3: other(1652) > max_T(255) -> true -> Return false
 *
 * **RESULT**: false (252 >= 1652 correctly evaluated as false)
 *
 * @subsection practical_examples Practical examples with code
 *
 * **EXAMPLE 1: WITHOUT SafeComparator (unsafe)**
 * @code
 * unsigned char block_size = 252;  // Range: 0-255
 * int packet_size = 1652;          // Range: -2,147,483,648 to 2,147,483,647
 *
 * // UNSAFE! Implicit type conversion
 * if (block_size >= packet_size) {
 *     // Issue: unsigned char(1652) = 116 (1652 % 256)
 *     // Result: 252 >= 116 = true (WRONG!)
 *     // Logically: 252 >= 1652 should be false
 *     processLargePacket();  // Called incorrectly!
 * } else {
 *     processSmallPacket();
 * }
 * @endcode
 *
 * **Problem**: unsigned char(1652) = 116 due to overflow (1652 % 256 = 116)
 * **Result**: 252 >= 116 = true (incorrect!)
 * **Consequence**: Incorrect program logic
 *
 * **EXAMPLE 2: WITH SafeComparator (safe)**
 * @code
 * unsigned char block_size = 252;  // Range: 0-255
 * int packet_size = 1652;          // Range: -2,147,483,648 to 2,147,483,647
 *
 * // SAFE! Use SafeComparator
 * SafeUCharComparator safe_size(block_size);
 * if (safe_size.safe_compare(packet_size)) {
 *     // SafeComparator algorithm:
 *     // 1. Classification: unsigned char vs int -> both_integral = true
 *     // 2. Choice: ALGORITHM B (integer types)
 *     // 3. ACTION B.1: max_T = 255
 *     // 4. ACTION B.3: packet_size(1652) > max_T(255) -> true
 *     // 5. RESULT: false (correct!)
 *     processLargePacket();
 * } else {
 *     processSmallPacket();  // Correct choice!
 * }
 * @endcode
 *
 * **SafeComparator algorithm**:
 * 1. **Type classification**: unsigned char vs int -> both_integral = true
 * 2. **Algorithm choice**: ALGORITHM B (integer types)
 * 3. **ACTION B.1**: max_T = 255 (maximum unsigned char)
 * 4. **ACTION B.3**: packet_size(1652) > max_T(255) -> true
 * 5. **RESULT**: false (252 >= 1652 correctly evaluated as false)
 *
 * **Result**: processSmallPacket() is called correctly!
 *
 * **Result comparison**:
 * - **Without SafeComparator**: 252 >= 116 = true (incorrect)
 * - **With SafeComparator**: 252 >= 1652 = false (correct)
 * - **Difference**: SafeComparator prevents overflow and yields the correct
 * result
 *
 * @subsection algorithm_properties Algorithm properties
 *
 * **Determinism**: The algorithm always yields the same result for the same
 * inputs
 * **Correctness**: The comparison result is mathematically correct
 * **Safety**: No undefined behavior (UB)
 * **Efficiency**: O(1) time and space
 * **Completeness**: Handles all arithmetic type combinations
 *
 */

// === Helper metafunctions ===
template <typename TypeToClean> struct clean_type_t
{
  using type = typename std::remove_cv<
      typename std::remove_reference<TypeToClean>::type>::type;
};

// === Type compatibility checks ===

/**
 * @brief Metafunction: whether two types can be compared safely
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @details Checks that both types are arithmetic and have specialized
 * numeric_limits. Required for correct value ranges and overflow prevention.
 * @note Compile-time check; no runtime cost
 */
template <typename T, typename U> struct is_safe_comparable
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;
  static bool const value = std::is_arithmetic<clean_T>::value
                            && std::is_arithmetic<clean_U>::value
                            && std::numeric_limits<clean_T>::is_specialized
                            && std::numeric_limits<clean_U>::is_specialized;
};

// === Concepts for C++20 compatibility ===
#if __cplusplus >= 202002L
/**
 * @brief Concept for arithmetic types with specialized numeric_limits
 * @tparam T Type to check
 * @details C++20 alternative to SFINAE for checking arithmetic types.
 *          Gives clearer compile errors and better integration with modern
 * C++.
 * @since C++20
 */
template <typename T>
concept ArithmeticType
    = std::is_arithmetic_v<T> && std::numeric_limits<T>::is_specialized;

/**
 * @brief Concept for safely comparable types
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @details Checks that both types are arithmetic and can be safely compared.
 *          Used for stricter typing in C++20 code.
 * @since C++20
 */
template <typename T, typename U>
concept SafeComparable = ArithmeticType<T> && ArithmeticType<U>;
#endif

// === Traits for optimization ===
/**
 * @brief Metafunction describing comparison traits of two types
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @details Provides compile-time type information used to optimize comparison
 * algorithms. Uses STL common_type to pick a common conversion type.
 * @note All computation is compile-time and does not affect runtime
 * performance
 */
template <typename T, typename U> struct comparison_traits
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;

  static bool const same_type
      = std::is_same<clean_T,
                     clean_U>::value; ///< true if the types are identical
  static bool const both_integral
      = std::is_integral<clean_T>::value
        && std::is_integral<clean_U>::value; ///< true if both types are
                                             ///< integral
  static bool const both_floating
      = std::is_floating_point<clean_T>::value
        && std::is_floating_point<clean_U>::value; ///< true if both types are
                                                   ///< floating-point
  static bool const mixed_types
      = !same_type && !both_integral
        && !both_floating; ///< true if the types are mixed (integral +
                           ///< floating)

  // Uses STL common_type directly - simple, reliable, efficient
  using common_type =
      typename std::common_type<clean_T, clean_U>::type; ///< Common type for
                                                         ///< safe conversion
};

// === SFINAE helper for C++11 compatibility ===

/**
 * @brief Primary template for the safe-comparison implementation
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @tparam Enable SFINAE parameter for specialization
 * @details Uses SFINAE to pick the optimal comparison implementation depending
 * on the types. Keeps C++11 compatibility without if constexpr.
 */
template <typename T, typename U, typename Enable = void>
struct safe_compare_impl_helper;

/**
 * @brief Specialization for identical types (maximum optimization)
 * @tparam T Type to compare
 * @details Direct comparison without conversions for maximum performance.
 *          Used when both types are identical, so overflow cannot occur.
 * @note No-throw, maximum performance
 */
template <typename T>
struct safe_compare_impl_helper<
    T, T,
    typename std::enable_if<
        std::is_integral<typename clean_type_t<T>::type>::value>::type>
{
  using clean_T = typename clean_type_t<T>::type;
  /**
   * @brief Safe comparison >= for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current >= other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  compare (T current, T other) LUMEX_NOEXCEPT
  {
    return current >= other; // Direct comparison without conversions
  }

  /**
   * @brief Safe comparison >= for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current >= other
   */
  static bool
  greater_equal (T current, T other) LUMEX_NOEXCEPT
  {
    return current >= other;
  }

  /**
   * @brief Safe comparison <= for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current <= other
   */
  static bool
  less_equal (T current, T other) LUMEX_NOEXCEPT
  {
    return current <= other;
  }

  /**
   * @brief Safe comparison < for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current < other
   */
  static bool
  less (T current, T other) LUMEX_NOEXCEPT
  {
    return current < other;
  }

  /**
   * @brief Safe comparison > for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current > other
   */
  static bool
  greater (T current, T other) LUMEX_NOEXCEPT
  {
    return current > other;
  }

  /**
   * @brief Safe comparison == for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current == other
   */
  static bool
  equal (T current, T other) LUMEX_NOEXCEPT
  {
    return current == other;
  }

  /**
   * @brief Safe comparison != for identical types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current != other
   */
  static bool
  not_equal (T current, T other) LUMEX_NOEXCEPT
  {
    return current != other;
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison for identical types (C++20 spaceship)
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return std::strong_ordering comparison result
   * @details Returns strong_ordering for an exact comparison of identical
   * types. Guarantees a deterministic order without information loss.
   * @since C++20
   */
  static std::strong_ordering
  three_way_compare (T current, T other) LUMEX_NOEXCEPT
  {
    if (current < other)
      return std::strong_ordering::less;
    if (current > other)
      return std::strong_ordering::greater;
    return std::strong_ordering::equal;
  }
#endif
};

/**
 * @brief Specialization for identical floating-point types
 * @tparam T Floating-point type to compare
 * @details Direct comparison without conversions for maximum performance.
 *          Used when both types are identical and are floating-point types.
 * @note No-throw, maximum performance for floating-point types
 */
template <typename T>
struct safe_compare_impl_helper<
    T, T,
    typename std::enable_if<
        std::is_floating_point<typename clean_type_t<T>::type>::value>::type>
{
  using clean_T = typename clean_type_t<T>::type;
  /**
   * @brief Safe comparison >= for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current >= other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  compare (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current >= other;
  }

  /**
   * @brief Safe comparison == for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current == other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  equal (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current == other;
  }

  /**
   * @brief Safe comparison != for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current != other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  not_equal (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return true;
    return current != other;
  }

  /**
   * @brief Safe comparison < for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current < other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  less (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current < other;
  }

  /**
   * @brief Safe comparison > for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current > other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  greater (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current > other;
  }

  /**
   * @brief Safe comparison <= for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current <= other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  less_equal (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current <= other;
  }

  /**
   * @brief Safe comparison >= for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return true if current >= other
   * @note Direct comparison without conversions; maximum performance
   */
  static bool
  greater_equal (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    return current >= other;
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison for identical floating-point types
   * @param[in] current Current value
   * @param[in] other Value to compare
   * @return std::partial_ordering comparison result
   * @details Returns partial_ordering for floating-point types because of NaN.
   *          NaN always yields unordered.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare (T current, T other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return std::partial_ordering::unordered;
    if (current < other)
      return std::partial_ordering::less;
    if (current > other)
      return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
#endif
};

/**
 * @brief Specialization for integer types
 * @tparam T First integer type
 * @tparam U Second integer type
 * @details Implements safe comparison between different integer types.
 *          Checks value ranges to prevent overflow during type conversion.
 * @note Checks type bounds before converting; prevents undefined behavior
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4018)
#endif
template <typename T, typename U>
struct safe_compare_impl_helper<
    T, U,
    typename std::enable_if<
        std::is_integral<typename clean_type_t<T>::type>::value
        && std::is_integral<typename clean_type_t<U>::type>::value
        && !std::is_same<typename clean_type_t<T>::type,
                         typename clean_type_t<U>::type>::value>::type>
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;
  using CommonType =
      typename std::common_type<clean_T, clean_U>::type; ///< Common type for
                                                         ///< safe conversion

  /**
   * @brief Safe comparison >= for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other exceeds the maximum value of T, returns false.
   *          If other is less than the minimum value of T, returns true.
   */
  static bool
  compare (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges to prevent overflow
    // Signedness is taken into account so the comparison is correct
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current >= other is never
        // false
        if (current < 0)
          return false;
        // other is always >= 0; only the upper bound is checked
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        // T unsigned, U signed: if other < 0 then current >= other is always
        // true true
        if (other < 0)
          return true;
        // Otherwise check the upper bound
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else
      {
        // Both signed or both unsigned - safe comparison
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return false;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return true;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return false;
        if (other < std::numeric_limits<clean_T>::min ())
          return true;
#endif
      }
    return static_cast<CommonType> (current)
           >= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison >= for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   */
  static bool
  greater_equal (T current, U other) LUMEX_NOEXCEPT
  {
    return compare (current, other);
  }

  /**
   * @brief Safe comparison <= for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current <= other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other exceeds the maximum value of T, returns true.
   *          If other is less than the minimum value of T, returns false.
   */
  static bool
  less_equal (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current <= other is always
        // true true
        if (current < 0)
          return true;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return false;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else
      {
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return true;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return false;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return true;
        if (other < std::numeric_limits<clean_T>::min ())
          return false;
#endif
      }
    return static_cast<CommonType> (current)
           <= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison < for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current < other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other exceeds the maximum value of T, returns true.
   *          If other is less than the minimum value of T, returns false.
   */
  static bool
  less (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current < other is always
        // true true
        if (current < 0)
          return true;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return false;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else
      {
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return true;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return false;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return true;
        if (other < std::numeric_limits<clean_T>::min ())
          return false;
#endif
      }
    return static_cast<CommonType> (current) < static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison > for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current > other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other exceeds the maximum value of T, returns false.
   *          If other is less than the minimum value of T, returns true.
   */
  static bool
  greater (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current > other is always
        // false
        if (current < 0)
          return false;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return true;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else
      {
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return false;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return true;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return false;
        if (other < std::numeric_limits<clean_T>::min ())
          return true;
#endif
      }
    return static_cast<CommonType> (current) > static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison == for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current == other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other does not fit in T, returns false.
   */
  static bool
  equal (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current == other is always
        // false
        if (current < 0)
          return false;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return false;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return false;
#endif
      }
    else
      {
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return false;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return false;
      }
    return static_cast<CommonType> (current)
           == static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison != for integer types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current != other
   * @details Checks value ranges before converting to prevent overflow.
   *          If other does not fit in T, returns true.
   */
  static bool
  not_equal (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current != other is always
        // true
        if (current < 0)
          return true;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return true;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return true;
#endif
      }
    else
      {
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return true;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return true;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return true;
        if (other < std::numeric_limits<clean_T>::min ())
          return true;
#endif
      }
    return static_cast<CommonType> (current)
           != static_cast<CommonType> (other);
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison for integer types (C++20 spaceship)
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return std::strong_ordering comparison result
   * @details Checks value ranges before converting to prevent overflow.
   *          Returns strong_ordering for an exact integer comparison.
   * @since C++20
   */
  static std::strong_ordering
  three_way_compare (T current, U other) LUMEX_NOEXCEPT
  {
    // Check ranges taking signedness into account
    if ((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value)
            ? true
            : false)
      {
        // T signed, U unsigned: if current < 0 then current < other
        if (current < 0)
          return std::strong_ordering::less;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedT = typename std::make_unsigned<clean_T>::type;
        if (static_cast<UnsignedT> (other)
            > static_cast<UnsignedT> (std::numeric_limits<clean_T>::max ()))
          return std::strong_ordering::less;
#else
        if (other > static_cast<typename std::make_unsigned<clean_T>::type> (
                std::numeric_limits<clean_T>::max ()))
          return std::strong_ordering::less;
#endif
      }
    else if ((std::is_unsigned<clean_T>::value
              && std::is_signed<clean_U>::value)
                 ? true
                 : false)
      {
        if (other < 0)
          return std::strong_ordering::greater;
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return std::strong_ordering::less;
#else
        using UnsignedU = typename std::make_unsigned<clean_U>::type;
        if (static_cast<UnsignedU> (other)
            > static_cast<UnsignedU> (std::numeric_limits<clean_T>::max ()))
          return std::strong_ordering::less;
#endif
      }
    else
      {
#ifdef LUMEXLUMEX_SFNC_ARCH_X86
        // On 32-bit architectures use explicit casts to silence C4018
        if (static_cast<CommonType> (other)
            > static_cast<CommonType> (std::numeric_limits<clean_T>::max ()))
          return std::strong_ordering::less;
        if (static_cast<CommonType> (other)
            < static_cast<CommonType> (std::numeric_limits<clean_T>::min ()))
          return std::strong_ordering::greater;
#else
        if (other > std::numeric_limits<clean_T>::max ())
          return std::strong_ordering::less;
        if (other < std::numeric_limits<clean_T>::min ())
          return std::strong_ordering::greater;
#endif
      }

    auto const current_common = static_cast<CommonType> (current);
    auto const other_common = static_cast<CommonType> (other);

    if (current_common < other_common)
      return std::strong_ordering::less;
    if (current_common > other_common)
      return std::strong_ordering::greater;
    return std::strong_ordering::equal;
  }
#endif
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Specialization for floating-point types
 * @tparam T First floating-point type
 * @tparam U Second floating-point type
 * @details Implements safe comparison between different floating-point types.
 *          Handles special values correctly: NaN and infinities.
 * @note NaN always returns false for ordered comparisons; infinities compare
 * by sign
 */
template <typename T, typename U>
struct safe_compare_impl_helper<
    T, U,
    typename std::enable_if<
        std::is_floating_point<typename clean_type_t<T>::type>::value
        && std::is_floating_point<typename clean_type_t<U>::type>::value
        && !std::is_same<typename clean_type_t<T>::type,
                         typename clean_type_t<U>::type>::value>::type>
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;
  using CommonType =
      typename std::common_type<clean_T, clean_U>::type; ///< Common type for
                                                         ///< safe conversion

  /**
   * @brief Safe comparison >= for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   * @details Handles NaN correctly (returns false) and infinities (compares by
   * sign). Prevents undefined behavior when comparing special values.
   */
  static bool
  compare (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    if (std::isinf (current) || std::isinf (other))
      return current == other;
    return static_cast<CommonType> (current)
           >= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison >= for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   */
  static bool
  greater_equal (T current, U other) LUMEX_NOEXCEPT
  {
    return compare (current, other);
  }

  /**
   * @brief Safe comparison <= for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current <= other
   * @details Handles NaN correctly (returns false) and infinities (compares by
   * sign).
   */
  static bool
  less_equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    if (std::isinf (current) || std::isinf (other))
      return current == other;
    return static_cast<CommonType> (current)
           <= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison < for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current < other
   * @details Handles NaN correctly (returns false) and infinities (compares by
   * sign).
   */
  static bool
  less (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    if (std::isinf (current) || std::isinf (other))
      return false; // infinity == infinity, not <
    return static_cast<CommonType> (current) < static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison > for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current > other
   * @details Handles NaN correctly (returns false) and infinities (compares by
   * sign).
   */
  static bool
  greater (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    if (std::isinf (current) || std::isinf (other))
      return false; // infinity == infinity, not >
    return static_cast<CommonType> (current) > static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison == for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current == other
   * @details Handles NaN correctly (returns false) and infinities (compares by
   * sign).
   */
  static bool
  equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return false;
    if (std::isinf (current) || std::isinf (other))
      return current == other;
    return static_cast<CommonType> (current)
           == static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison != for floating-point types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current != other
   * @details Handles NaN correctly (returns true) and infinities (compares by
   * sign).
   */
  static bool
  not_equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return true;
    if (std::isinf (current) || std::isinf (other))
      return current != other;
    return static_cast<CommonType> (current)
           != static_cast<CommonType> (other);
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison for floating-point types (C++20 spaceship)
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return std::partial_ordering comparison result
   * @details Handles NaN correctly (returns unordered) and infinities.
   *          Returns partial_ordering for floating-point types because of NaN.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::isnan (current) || std::isnan (other))
      return std::partial_ordering::unordered;
    if (std::isinf (current) || std::isinf (other))
      {
        if (current == other)
          return std::partial_ordering::equivalent;
        return std::partial_ordering::unordered;
      }

    auto const current_common = static_cast<CommonType> (current);
    auto const other_common = static_cast<CommonType> (other);

    if (current_common < other_common)
      return std::partial_ordering::less;
    if (current_common > other_common)
      return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
#endif
};

/**
 * @brief Specialization for mixed types (integer + floating-point)
 * @tparam T First type (integer or floating-point)
 * @tparam U Second type (floating-point or integer)
 * @details Implements safe comparison between integer types and floating-point
 * types. Handles floating-point special values correctly.
 * @note The hardest comparison case; needs special NaN and infinities
 */
template <typename T, typename U>
struct safe_compare_impl_helper<
    T, U,
    typename std::enable_if<
        (std::is_integral<typename clean_type_t<T>::type>::value
         && std::is_floating_point<typename clean_type_t<U>::type>::value)
        || (std::is_floating_point<typename clean_type_t<T>::type>::value
            && std::is_integral<typename clean_type_t<U>::type>::value)>::type>
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;
  using CommonType =
      typename std::common_type<clean_T, clean_U>::type; ///< Common type for
                                                         ///< safe conversion

  /**
   * @brief Safe comparison >= for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns false; infinities compare
   * by sign.
   */
  static bool
  compare (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return false;
        if (std::isinf (other))
          return other < 0;
        return static_cast<CommonType> (current)
               >= static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return false;
    if (std::isinf (current))
      return current > 0;
    return static_cast<CommonType> (current)
           >= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison >= for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current >= other
   */
  static bool
  greater_equal (T current, U other) LUMEX_NOEXCEPT
  {
    return compare (current, other);
  }

  /**
   * @brief Safe comparison <= for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current <= other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns false; infinities compare
   * by sign.
   */
  static bool
  less_equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return false;
        if (std::isinf (other))
          return other > 0;
        return static_cast<CommonType> (current)
               <= static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return false;
    if (std::isinf (current))
      return current < 0;
    return static_cast<CommonType> (current)
           <= static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison < for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current < other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns false; infinities compare
   * by sign.
   */
  static bool
  less (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return false;
        if (std::isinf (other))
          return other > 0;
        return static_cast<CommonType> (current)
               < static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return false;
    if (std::isinf (current))
      return current < 0;
    return static_cast<CommonType> (current) < static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison > for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current > other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns false; infinities compare
   * by sign.
   */
  static bool
  greater (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return false;
        if (std::isinf (other))
          return other < 0;
        return static_cast<CommonType> (current)
               > static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return false;
    if (std::isinf (current))
      return current > 0;
    return static_cast<CommonType> (current) > static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison == for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current == other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns false; infinities compare
   * by sign.
   */
  static bool
  equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return false;
        if (std::isinf (other))
          return false;
        return static_cast<CommonType> (current)
               == static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return false;
    if (std::isinf (current))
      return false;
    return static_cast<CommonType> (current)
           == static_cast<CommonType> (other);
  }

  /**
   * @brief Safe comparison != for mixed types
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return true if current != other
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns true; infinities compare by
   * sign.
   */
  static bool
  not_equal (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return true;
        if (std::isinf (other))
          return true;
        return static_cast<CommonType> (current)
               != static_cast<CommonType> (other);
      }
    if (std::isnan (current))
      return true;
    if (std::isinf (current))
      return true;
    return static_cast<CommonType> (current)
           != static_cast<CommonType> (other);
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison for mixed types (C++20 spaceship)
   * @param[in] current Current value of type T
   * @param[in] other Value to compare of type U
   * @return std::partial_ordering comparison result
   * @details Handles all combinations: integral vs floating-point and
   * floating-point vs integral. NaN always returns unordered; infinities
   * compare by sign. Returns partial_ordering because NaN may be present.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare (T current, U other) LUMEX_NOEXCEPT
  {
    if (std::is_integral<clean_T>::value
        && std::is_floating_point<clean_U>::value)
      {
        if (std::isnan (other))
          return std::partial_ordering::unordered;
        if (std::isinf (other))
          return std::partial_ordering::unordered;

        auto const current_common = static_cast<CommonType> (current);
        auto const other_common = static_cast<CommonType> (other);

        if (current_common < other_common)
          return std::partial_ordering::less;
        if (current_common > other_common)
          return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
      }

    if (std::isnan (current))
      return std::partial_ordering::unordered;
    if (std::isinf (current))
      return std::partial_ordering::unordered;

    auto const current_common = static_cast<CommonType> (current);
    auto const other_common = static_cast<CommonType> (other);

    if (current_common < other_common)
      return std::partial_ordering::less;
    if (current_common > other_common)
      return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
#endif
};

/**
 * @brief Universal thread-safe comparator for safe comparison of arithmetic
 * types
 * @tparam T Arithmetic type to compare (integral or floating-point)
 * @tparam Atomic If true, uses std::atomic<T> for thread-safety, otherwise
 * plain T for performance
 * @details Prevents overflow and incorrect comparisons between types with
 * different ranges. Supports all arithmetic types with compile-time
 * optimizations and SFINAE dispatch.
 *
 * @section invariants Class invariants
 * - All operations are thread-safe when Atomic=true
 * - Overflow is prevented when comparing different types
 * - Edge cases are handled (NaN, infinities, type bounds)
 * - No-throw guarantees for every operation
 *
 * @section thread_safety Thread Safety
 * - When Atomic=true: every operation is lock-free thread-safe
 * - When Atomic=false: maximum single-threaded performance
 * - Memory-ordering guarantees for atomic operations
 * - Compare-and-swap for atomic updates
 *
 * @section exception_safety Exception Safety
 * - Strong guarantee for every operation
 * - No-throw where possible (every method is noexcept)
 * - RAII resource management
 *
 * @section performance Performance
 * - Compile-time SFINAE dispatch
 * - Specializations that optimize identical types
 * - Zero-cost abstractions where possible
 * - Lock-free operations for atomic versions
 *
 * @example
 * // Thread-safe version for multithreading
 * SafeComparator<int, true> atomic_counter(0);
 * atomic_counter.update(100);
 * if (atomic_counter.safe_compare(50)) {
 *   // Safe comparison int >= int
 * }
 *
 * // High-performance single-threaded version
 * SafeComparator<unsigned char, false> fast_size(200);
 * if (fast_size.safe_compare(300)) {
 *   // Safe comparison unsigned char >= int without overflow
 * }
 *
 * @since C++11
 * @note Fully C++11 compatible; uses C++20 concepts when available
 */
template <typename T, bool Atomic = false> class SafeComparator
{
  using clean_T = typename clean_type_t<T>::type;
  LUMEX_STATIC_ASSERT_MSG (std::is_arithmetic<clean_T>::value,
                           "T must be arithmetic type");
  LUMEX_STATIC_ASSERT_MSG (std::numeric_limits<clean_T>::is_specialized,
                           "T must have specialized numeric_limits");

private:
  // Compile-time implementation choice
  typename std::conditional<Atomic, std::atomic<clean_T>, clean_T>::type
      m_value; ///< Comparator value (atomic or plain)

  /**
   * @brief SFINAE dispatcher for atomic store
   * @tparam A SFINAE parameter (equals Atomic)
   * @param[in] value Value to store
   * @param[in] order Memory order for atomic operations
   * @note Specialization for Atomic=true: uses std::atomic::store
   */
  template <bool A = Atomic>
  typename std::enable_if<A, void>::type
  atomic_store (clean_T value, std::memory_order order
                               = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    m_value.store (value, order);
  }

  /**
   * @brief SFINAE dispatcher for a plain store
   * @tparam A SFINAE parameter (equals Atomic)
   * @param[in] value Value to store
   * @param[in] order Memory order (ignored for non-atomic operations)
   * @note Specialization for Atomic=false: direct assignment for maximum
   * performance
   */
  template <bool A = Atomic>
  typename std::enable_if<!A, void>::type
  atomic_store (clean_T value,
                std::memory_order /* unused */ = std::memory_order_seq_cst)
      LUMEX_NOEXCEPT
  {
    m_value = value;
  }

  /**
   * @brief SFINAE dispatcher for atomic load
   * @tparam A SFINAE parameter (equals Atomic)
   * @param[in] order Memory order for atomic operations
   * @return Current value
   * @note Specialization for Atomic=true: uses std::atomic::load
   */
  template <bool A = Atomic>
  typename std::enable_if<A, clean_T>::type
  atomic_load (std::memory_order order
               = std::memory_order_seq_cst) const LUMEX_NOEXCEPT
  {
    return m_value.load (order);
  }

  /**
   * @brief SFINAE dispatcher for a plain load
   * @tparam A SFINAE parameter (equals Atomic)
   * @param[in] order Memory order (ignored for non-atomic operations)
   * @return Current value
   * @note Specialization for Atomic=false: direct read for maximum performance
   */
  template <bool A = Atomic>
  typename std::enable_if<!A, clean_T>::type
  atomic_load (LUMEX_ATTRIBUTE_MAYBE_UNUSED std::memory_order order
               = std::memory_order_seq_cst) const LUMEX_NOEXCEPT
  {
    return m_value;
  }

public:
  // === Constructors ===

  /**
   * @brief Default constructor
   * @details Initializes the value to T{}.
   *          For atomic types this is a thread-safe operation.
   * @note No-throw; thread-safe when Atomic=true
   */
  SafeComparator () LUMEX_NOEXCEPT : m_value{} {}

  /**
   * @brief Constructor with an initial value
   * @param[in] value Initial value
   * @details Creates a comparator with the given initial value.
   *          For atomic types, initialization is atomic.
   * @note No-throw; thread-safe when Atomic=true
   */
  explicit SafeComparator (T value) LUMEX_NOEXCEPT
      : m_value (static_cast<clean_T> (value))
  {
  }

  /**
   * @brief Copy constructor
   * @param[in] other Source object to copy
   * @details Creates a copy of an existing comparator.
   *          For atomic types, copy uses an atomic load.
   * @note No-throw; thread-safe when Atomic=true
   */
  SafeComparator (SafeComparator const &other) LUMEX_NOEXCEPT
      : m_value (other.atomic_load ())
  {
  }

  /**
   * @brief Move constructor
   * @param[in] other Source object to move from
   * @details Moves the value from another comparator and zeros the source
   * object. Uses std::exchange for a safe move.
   * @note No-throw, maximum performance
   */
  SafeComparator (SafeComparator &&other) LUMEX_NOEXCEPT
      : m_value (std::exchange (other.m_value, clean_T{}))
  {
  }

  /**
   * @brief Copy assignment operator
   * @param[in] other Source object to copy
   * @return Reference to this object
   * @details Assigns the value from another comparator.
   *          Guards against self-assignment.
   * @note No-throw; thread-safe when Atomic=true
   */
  SafeComparator &
  operator= (SafeComparator const &other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      atomic_store (other.atomic_load ());
    return *this;
  }

  /**
   * @brief Move assignment operator
   * @param[in] other Source object to move from
   * @return Reference to this object
   * @details Moves the value from another comparator and zeros the source
   * object. Guards against self-assignment.
   * @note No-throw, maximum performance
   */
  SafeComparator &
  operator= (SafeComparator &&other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      atomic_store (std::exchange (other.m_value, clean_T{}));
    return *this;
  }

  /**
   * @brief Destructor
   * @details Implicitly generated by the compiler.
   *          For atomic types, destruction is thread-safe.
   * @note No-throw; implicitly generated by the compiler
   */
  ~SafeComparator () = default;

  // === Safe comparison operations ===

  /**
   * @brief Safe >= comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value >= other
   * @details Performs a safe comparison between the current T value and a
   * value of type U. Checks value ranges to prevent overflow. Uses specialized
   * algorithms for each type combination.
   * @note Thread-safe when Atomic=true; no-throw
   * @warning T and U must be safely comparable (checked at compile-time)
   */
  template <typename U>
  bool
  safe_compare (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::compare (atomic_load (),
                                                          other);
  }

  /**
   * @brief Safe >= comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value >= other
   * @details Alias for safe_compare() for readability.
   *          Performs the same safe range check.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_greater_equal (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::greater_equal (atomic_load (),
                                                                other);
  }

  /**
   * @brief Safe <= comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value <= other
   * @details Performs a safe less-or-equal comparison between the current
   * value of type T and a value of type U. Checks value ranges to prevent
   * overflow.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_less_equal (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::less_equal (atomic_load (),
                                                             other);
  }

  /**
   * @brief Safe < comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value < other
   * @details Performs a safe less-than comparison of the current value of type
   * T and a value of type U. Checks value ranges to prevent overflow.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_less (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::less (atomic_load (), other);
  }

  /**
   * @brief Safe > comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value > other
   * @details Performs a safe greater-than comparison of the current value of
   * type T and a value of type U. Checks value ranges to prevent overflow.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_greater (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::greater (atomic_load (),
                                                          other);
  }

  /**
   * @brief Safe == comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value == other
   * @details Performs a safe equality comparison of the current value of type
   * T and a value of type U. Checks value ranges to prevent overflow.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_equal (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::equal (atomic_load (), other);
  }

  /**
   * @brief Safe != comparison with another type
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return true if the current value != other
   * @details Performs a safe inequality comparison of the current value of
   * type T and a value of type U. Checks value ranges to prevent overflow.
   * @note Thread-safe when Atomic=true; no-throw
   */
  template <typename U>
  bool
  safe_not_equal (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::not_equal (atomic_load (),
                                                            other);
  }

#if __cplusplus >= 202002L
  /**
   * @brief Three-way comparison with another type (C++20 spaceship)
   * @tparam U Type of the value to compare
   * @param[in] other Value to compare
   * @return Three-way comparison result
   * @details Performs a three-way comparison between the current T value and a
   * value of type U. Returns strong_ordering for integer types,
   * partial_ordering for floating-point. Checks value ranges to prevent
   * overflow.
   * @note Thread-safe when Atomic=true; no-throw
   * @since C++20
   */
  template <typename U>
  auto
  safe_three_way_compare (U other) const LUMEX_NOEXCEPT
  {
    LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                             "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::three_way_compare (
        atomic_load (), other);
  }
#endif

  // === State operations ===

  /**
   * @brief Update the comparator value
   * @param[in] new_value New value to set
   * @details Stores a new value in the comparator.
   *          For atomic types the operation is thread-safe with memory
   * ordering.
   * @note Thread-safe when Atomic=true; no-throw
   */
  void
  update (T new_value) LUMEX_NOEXCEPT
  {
    atomic_store (static_cast<clean_T> (new_value));
  }

  /**
   * @brief Get the current comparator value
   * @return Current value of type T
   * @details Returns the current comparator value.
   *          For atomic types the operation is thread-safe with memory
   * ordering.
   * @note Thread-safe when Atomic=true; no-throw
   */
  T
  get () const LUMEX_NOEXCEPT
  {
    return static_cast<T> (atomic_load ());
  }

  /**
   * @brief Atomic compare-and-set
   * @param[in] expected_value Expected current value
   * @param[in] desired_value Desired new value
   * @return true if the value was changed, false if expected_value does not
   * match the current value
   * @details Performs an atomic compare-and-set.
   *          For atomic types uses compare_exchange_strong with memory
   * ordering. For non-atomic types performs a plain check-and-set.
   * @note Thread-safe when Atomic=true; no-throw
   * @warning expected_value may be updated on a failed compare (for atomic
   * types)
   */
  bool
  compare_and_set (T expected_value, T desired_value) LUMEX_NOEXCEPT
  {
    return compare_and_set_impl (static_cast<clean_T> (expected_value),
                                 static_cast<clean_T> (desired_value),
                                 std::integral_constant<bool, Atomic>{});
  }

private:
  /**
   * @brief SFINAE compare-and-set for atomic types
   * @param[in] expected_value Expected value (may be updated)
   * @param[in] desired_value Desired value
   * @param[in] atomic_tag SFINAE tag (std::true_type)
   * @return true if the value was changed
   * @details Uses std::atomic::compare_exchange_strong with memory ordering.
   *          expected_value is updated to the current value on a failed
   * compare.
   * @note Thread-safe, no-throw
   */
  bool
  compare_and_set_impl (clean_T expected_value, clean_T desired_value,
                        std::true_type /* atomic */) LUMEX_NOEXCEPT
  {
    return m_value.compare_exchange_strong (expected_value, desired_value,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire);
  }

  /**
   * @brief SFINAE compare-and-set for non-atomic types
   * @param[in] expected_value Expected value
   * @param[in] desired_value Desired value
   * @param[in] non_atomic_tag SFINAE tag (std::false_type)
   * @return true if the value was changed
   * @details Performs a plain check-and-set for maximum performance.
   *          Does not change expected_value on a failed compare.
   * @note No-throw, maximum performance
   */
  bool
  compare_and_set_impl (
      clean_T expected_value,
      clean_T desired_value, // NOLINT(bugprone-easily-swappable-parameters)
      std::false_type /* non-atomic */) LUMEX_NOEXCEPT
  {
    if (m_value == expected_value)
      {
        m_value = desired_value;
        return true;
      }
    return false;
  }

public:
  // === Convenience operators ===

  /**
   * @brief Implicit conversion operator to T
   * @return Current value of type T
   * @details Lets the comparator be used where a value of type T.
   *          For atomic types this is an atomic load.
   * @note Thread-safe when Atomic=true; no-throw
   * @warning Implicit conversion can hide atomic operations
   */
  operator T () const LUMEX_NOEXCEPT
  {
    return static_cast<T> (atomic_load ());
  }

  /**
   * @brief Value assignment operator
   * @param[in] value New value to set
   * @return Reference to this object
   * @details Stores a new value in the comparator.
   *          For atomic types the operation is thread-safe.
   * @note Thread-safe when Atomic=true; no-throw
   */
  SafeComparator &
  operator= (T value) LUMEX_NOEXCEPT
  {
    atomic_store (static_cast<clean_T> (value));
    return *this;
  }
};

// === Convenience aliases for common types ===

/**
 * @brief Aliases for atomic (thread-safe) versions
 * @details Ready-made types for multithreaded use.
 *          Every operation is lock-free thread-safe.
 * @note Use in multithreaded applications
 */
using AtomicCharComparator
    = SafeComparator<char, true>; ///< Atomic comparator for char
using AtomicUCharComparator
    = SafeComparator<unsigned char,
                     true>; ///< Atomic comparator for unsigned char
using AtomicShortComparator
    = SafeComparator<short, true>; ///< Atomic comparator for short
using AtomicUShortComparator
    = SafeComparator<unsigned short,
                     true>; ///< Atomic comparator for unsigned short
using AtomicIntComparator
    = SafeComparator<int, true>; ///< Atomic comparator for int
using AtomicUIntComparator
    = SafeComparator<unsigned int,
                     true>; ///< Atomic comparator for unsigned int
using AtomicLongComparator
    = SafeComparator<long, true>; ///< Atomic comparator for long
using AtomicULongComparator
    = SafeComparator<unsigned long,
                     true>; ///< Atomic comparator for unsigned long
using AtomicLongLongComparator
    = SafeComparator<long long, true>; ///< Atomic comparator for long long
using AtomicULongLongComparator
    = SafeComparator<unsigned long long,
                     true>; ///< Atomic comparator for unsigned long long
using AtomicFloatComparator
    = SafeComparator<float, true>; ///< Atomic comparator for float
using AtomicDoubleComparator
    = SafeComparator<double, true>; ///< Atomic comparator for double
using AtomicLongDoubleComparator
    = SafeComparator<long double, true>; ///< Atomic comparator for long double

/**
 * @brief Aliases for non-atomic versions (higher performance)
 * @details Ready-made types for single-threaded use.
 *          Maximum performance with no synchronization overhead.
 * @note Use only in a single-threaded context
 */
using FastCharComparator
    = SafeComparator<char, false>; ///< Fast comparator for char
using FastUCharComparator
    = SafeComparator<unsigned char,
                     false>; ///< Fast comparator for unsigned char
using FastShortComparator
    = SafeComparator<short, false>; ///< Fast comparator for short
using FastUShortComparator
    = SafeComparator<unsigned short,
                     false>; ///< Fast comparator for unsigned short
using FastIntComparator
    = SafeComparator<int, false>; ///< Fast comparator for int
using FastUIntComparator
    = SafeComparator<unsigned int,
                     false>; ///< Fast comparator for unsigned int
using FastLongComparator
    = SafeComparator<long, false>; ///< Fast comparator for long
using FastULongComparator
    = SafeComparator<unsigned long,
                     false>; ///< Fast comparator for unsigned long
using FastLongLongComparator
    = SafeComparator<long long, false>; ///< Fast comparator for long long
using FastULongLongComparator
    = SafeComparator<unsigned long long,
                     false>; ///< Fast comparator for unsigned long long
using FastFloatComparator
    = SafeComparator<float, false>; ///< Fast comparator for float
using FastDoubleComparator
    = SafeComparator<double, false>; ///< Fast comparator for double
using FastLongDoubleComparator
    = SafeComparator<long double, false>; ///< Fast comparator for long double

/**
 * @brief Default aliases (non-atomic for better performance)
 * @details Ready-made types with optimal defaults.
 *          Use non-atomic versions for maximum performance.
 * @note Recommended for most uses
 */
using SafeCharComparator = SafeComparator<char>; ///< Safe comparator for char
using SafeUCharComparator
    = SafeComparator<unsigned char>; ///< Safe comparator for unsigned char
using SafeShortComparator
    = SafeComparator<short>; ///< Safe comparator for short
using SafeUShortComparator
    = SafeComparator<unsigned short>; ///< Safe comparator for unsigned short
using SafeIntComparator = SafeComparator<int>; ///< Safe comparator for int
using SafeUIntComparator
    = SafeComparator<unsigned int>; ///< Safe comparator for unsigned int
using SafeLongComparator = SafeComparator<long>; ///< Safe comparator for long
using SafeULongComparator
    = SafeComparator<unsigned long>; ///< Safe comparator for unsigned long
using SafeLongLongComparator
    = SafeComparator<long long>; ///< Safe comparator for long long
using SafeULongLongComparator
    = SafeComparator<unsigned long long>; ///< Safe comparator for unsigned
                                          ///< long long
using SafeFloatComparator
    = SafeComparator<float>; ///< Safe comparator for float
using SafeDoubleComparator
    = SafeComparator<double>; ///< Safe comparator for double
using SafeLongDoubleComparator
    = SafeComparator<long double>; ///< Safe comparator for long double

// === Three-way comparison and utilities ===

#if __cplusplus >= 202002L
/**
 * @brief Metafunction for the three-way comparison result type
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @details Picks the most suitable ordering type for three-way comparison.
 *          Returns strong_ordering for integer types, partial_ordering for
 * floating-point.
 * @since C++20
 */
template <typename T, typename U> struct three_way_comparison_result
{
  using clean_T = typename clean_type_t<T>::type;
  using clean_U = typename clean_type_t<U>::type;

  LUMEX_CONST_NUM bool both_integral
      = std::is_integral<clean_T>::value && std::is_integral<clean_U>::value;
  LUMEX_CONST_NUM bool both_floating
      = std::is_floating_point<clean_T>::value
        && std::is_floating_point<clean_U>::value;
  LUMEX_CONST_NUM bool mixed_types = !both_integral && !both_floating;

  using type
      = std::conditional_t<both_integral, std::strong_ordering,
                           std::conditional_t<both_floating || mixed_types,
                                              std::partial_ordering, void>>;
};

/**
 * @brief Alias for the three-way comparison result type
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @since C++20
 */
template <typename T, typename U>
using three_way_comparison_result_t =
    typename three_way_comparison_result<T, U>::type;

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means equal
 * @details Converts a three-way comparison result to bool equality.
 *          Useful for compatibility with existing code.
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_equal (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering == std::strong_ordering::equal;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::equivalent;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering == std::weak_ordering::equivalent;
  }
}

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means less
 * @details Converts a three-way comparison result to bool "less".
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_less (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering == std::strong_ordering::less;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::less;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering == std::weak_ordering::less;
  }
}

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means greater
 * @details Converts a three-way comparison result to bool "greater".
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_greater (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering == std::strong_ordering::greater;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::greater;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering == std::weak_ordering::greater;
  }
}

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means less or equal
 * @details Converts a three-way comparison result to bool "less or equal".
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_less_equal (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering != std::strong_ordering::greater;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::less
           || ordering == std::partial_ordering::equivalent;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering != std::weak_ordering::greater;
  }
}

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means greater or equal
 * @details Converts a three-way comparison result to bool "greater or equal".
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_greater_equal (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering != std::strong_ordering::less;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::greater
           || ordering == std::partial_ordering::equivalent;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering != std::weak_ordering::less;
  }
}

/**
 * @brief Utility that converts a comparison result to bool
 * @tparam Ordering Comparison result type
 * @param[in] ordering Three-way comparison result
 * @return true if ordering means not equal
 * @details Converts a three-way comparison result to bool "not equal".
 * @since C++20
 */
template <typename Ordering>
LUMEX_CONSTEXPR bool
is_not_equal (Ordering const &ordering) LUMEX_NOEXCEPT
{
  LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::strong_ordering>)
  {
    return ordering != std::strong_ordering::equal;
  }
  else LUMEX_CONSTEXPR_IF (std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering != std::partial_ordering::equivalent;
  }
  else
  {
    LUMEX_STATIC_ASSERT ((std::is_same_v<Ordering, std::weak_ordering>));
    return ordering != std::weak_ordering::equivalent;
  }
}
#endif

// === Utility functions for convenience ===

/**
 * @brief Safe comparison of two values of different types
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 >= value2
 * @details Performs a safe comparison of two values of different types.
 *          Uses specialized algorithms to prevent overflow.
 *          Does not require a comparator object.
 * @note Thread-safe, no-throw
 * @warning T and U must be safely comparable (checked at compile-time)
 * @example
 * unsigned char size = 200;
 * int limit = 300;
 * if (safe_compare(size, limit)) {
 *   // Safe comparison unsigned char >= int
 * }
 */
template <typename T, typename U>
bool
safe_compare (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::compare (value1, value2);
}

/**
 * @brief Safe greater-or-equal comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 >= value2
 * @details Alias for safe_compare() for readability.
 *          Performs the same safe range check.
 * @note Thread-safe, no-throw
 * @example
 * float f = 3.14f;
 * int i = 3;
 * if (safe_greater_equal(f, i)) {
 *   // Safe comparison float >= int
 * }
 */
template <typename T, typename U>
bool
safe_greater_equal (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::greater_equal (value1, value2);
}

/**
 * @brief Safe less-or-equal comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 <= value2
 * @details Performs a safe less-or-equal comparison of two values of different
 * types. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @example
 * int i = 3;
 * float f = 3.14f;
 * if (safe_less_equal(i, f)) {
 *   // Safe comparison int <= float
 * }
 */
template <typename T, typename U>
bool
safe_less_equal (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::less_equal (value1, value2);
}

/**
 * @brief Safe less-than comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 < value2
 * @details Performs a safe less-than comparison of two values of different
 * types. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @example
 * unsigned char size = 200;
 * int limit = 300;
 * if (safe_less(size, limit)) {
 *   // Safe comparison unsigned char < int
 * }
 */
template <typename T, typename U>
bool
safe_less (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::less (value1, value2);
}

/**
 * @brief Safe greater-than comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 > value2
 * @details Performs a safe greater-than comparison of two values of different
 * types. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @example
 * int threshold = 100;
 * unsigned char count = 150;
 * if (safe_greater(count, threshold)) {
 *   // Safe comparison unsigned char > int
 * }
 */
template <typename T, typename U>
bool
safe_greater (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::greater (value1, value2);
}

/**
 * @brief Safe equality comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 == value2
 * @details Performs a safe equality comparison of two values of different
 * types. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @example
 * int expected = 42;
 * float actual = 42.0f;
 * if (safe_equal(expected, actual)) {
 *   // Safe comparison int == float
 * }
 */
template <typename T, typename U>
bool
safe_equal (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::equal (value1, value2);
}

/**
 * @brief Safe inequality comparison
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return true if value1 != value2
 * @details Performs a safe inequality comparison of two values of different
 * types. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @example
 * int threshold = 100;
 * unsigned char count = 150;
 * if (safe_not_equal(count, threshold)) {
 *   // Safe comparison unsigned char != int
 * }
 */
template <typename T, typename U>
bool
safe_not_equal (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::not_equal (value1, value2);
}

#if __cplusplus >= 202002L
/**
 * @brief Safe three-way comparison (C++20 spaceship)
 * @tparam T First type to compare
 * @tparam U Second type to compare
 * @param[in] value1 First value of type T
 * @param[in] value2 Second value of type U
 * @return Three-way comparison result
 * @details Performs a safe three-way comparison of two values of different
 * types. Returns strong_ordering for integer types, partial_ordering for
 * floating-point. Checks value ranges to prevent overflow.
 * @note Thread-safe, no-throw
 * @since C++20
 * @example
 * int a = 42;
 * float b = 42.0f;
 * auto result = safe_three_way_compare(a, b);
 * if (result == std::strong_ordering::equal) {
 *   // Safe three-way comparison of int <=> float
 * }
 */
template <typename T, typename U>
auto
safe_three_way_compare (T value1, U value2) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<T, U>::value,
                           "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::three_way_compare (value1, value2);
}
#endif

/**
 * @brief Check whether a value fits the target type
 * @tparam TargetType Target type
 * @tparam SourceType Source value type
 * @param[in] value Value of type SourceType to check
 * @return true if the value fits TargetType without loss of data
 * @details Checks whether a SourceType value can be converted safely to
 * TargetType. For integers, checks value ranges. For floating-point types,
 * rejects NaN and infinities.
 * @note No-throw; compile-time type check
 * @example
 * if (fits_in_type<unsigned char>(300)) {
 *   // 300 does not fit in unsigned char (0-255)
 * }
 *
 * if (fits_in_type<int>(3.14f)) {
 *   // 3.14f can be converted to int safely
 * }
 */
template <typename TargetType, typename SourceType>
bool
fits_in_type (SourceType value) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (is_safe_comparable<TargetType, SourceType>::value,
                           "Both types must be safely comparable");

  using clean_target = typename std::remove_cv<
      typename std::remove_reference<TargetType>::type>::type;
  using clean_source = typename std::remove_cv<
      typename std::remove_reference<SourceType>::type>::type;

  if (std::is_integral<clean_target>::value
      && std::is_integral<clean_source>::value)
    return value >= std::numeric_limits<clean_target>::min ()
           && value <= std::numeric_limits<clean_target>::max ();
  if (std::is_floating_point<clean_target>::value
      && std::is_floating_point<clean_source>::value)
    return !std::isnan (value) && !std::isinf (value);

  // Mixed types - use STL common_type
  if (std::is_integral<clean_target>::value
      && std::is_floating_point<clean_source>::value)
    {
      if (std::isnan (value) || std::isinf (value))
        return false;
      return value >= std::numeric_limits<clean_target>::min ()
             && value <= std::numeric_limits<clean_target>::max ();
    }

  // floating-point to integral
  return !std::isnan (value) && !std::isinf (value);
}

// === Usage examples ===

/**
 * @example SafeNumericComparator_Examples.cpp
 * @brief SafeComparator usage examples for various scenarios
 * @details Shows the main capabilities of the safe comparison:
 *          - Safe comparison of different types
 *          - Thread-safe operations
 *          - High-performance operations
 *          - Range checking of values
 *          - Mixed types
 */

/*
// Example 1: Safe comparison unsigned char with int
unsigned char block_size = 200;
int packet_size = 300;

// Unsafe: may overflow
// if (block_size >= packet_size) // Problem!

// Safe: use SafeComparator
SafeUCharComparator safe_size(block_size);
if (safe_size.safe_compare(packet_size)) {
  // Safe comparison done
}

// Or use the utility function
if (safe_compare(block_size, packet_size)) {
  // Safe comparison done
}

// Example 2: All comparison kinds
SafeUCharComparator safe_size(200);
int threshold = 300;

// Every comparison operator
if (safe_size.safe_equal(threshold)) { // ==
}
if (safe_size.safe_not_equal(threshold)) { // !=
}
if (safe_size.safe_less(threshold)) { // <
}
if (safe_size.safe_greater(threshold)) { // >
}
if (safe_size.safe_less_equal(threshold)) { // <=
}
if (safe_size.safe_greater_equal(threshold)) { // >=
}

// Utility functions
if (safe_equal(block_size, threshold)) { // ==
}
if (safe_not_equal(block_size, threshold)) { // !=
}
if (safe_less(block_size, threshold)) { // <
}
if (safe_greater(block_size, threshold)) { // >
}
if (safe_less_equal(block_size, threshold)) { // <=
}
if (safe_greater_equal(block_size, threshold)) { // >=
}

// Example 3: C++20 three-way comparison
#if __cplusplus >= 202002L
auto result = safe_size.safe_three_way_compare(threshold);
if (is_equal(result)) { // ==
}
else if (is_less(result)) { // <
}
else if (is_greater(result)) { // >
}

// Direct three-way comparison
auto direct_result = safe_three_way_compare(block_size, threshold);
if (is_equal(direct_result)) { // ==
}
#endif

// Example 4: Thread-safe operations
AtomicIntComparator atomic_counter(0);
atomic_counter.update(100);
if (atomic_counter.compare_and_set(100, 200)) {
  // The value was updated
}

// Example 5: High single-threaded performance
FastIntComparator fast_counter(0);
fast_counter.update(100);
int current_value = fast_counter.get();

// Example 6: Range checking
if (fits_in_type<unsigned char>(300)) {
  // 300 does not fit in unsigned char (0-255)
}

// Example 7: Mixed types
float float_value = 3.14f;
int int_value = 3;
if (safe_greater_equal(float_value, int_value)) {
  // Safe comparison float >= int
}

// Example 8: Floating-point with NaN and infinity
float nan_value = std::numeric_limits<float>::quiet_NaN();
float inf_value = std::numeric_limits<float>::infinity();
int normal_value = 42;

SafeFloatComparator safe_float(nan_value);
if (safe_float.safe_equal(normal_value)) { // false - NaN != anything
}
if (safe_float.safe_not_equal(normal_value)) { // true - NaN != anything
}

SafeFloatComparator safe_inf(inf_value);
if (safe_inf.safe_greater(normal_value)) { // true - +inf > any finite
}

// Example 9: Complex scenario
void processDataPacket(unsigned char packet_size, int max_size, float
threshold) { SafeUCharComparator safe_packet_size(packet_size);

    // Check the packet size
    if (safe_packet_size.safe_greater(max_size)) {
        // Packet is too large
        return;
    }

    // Check the threshold
    if (safe_packet_size.safe_less_equal(threshold)) {
        // Small packet
        processSmallPacket();
    } else {
        // Large packet
        processLargePacket();
    }

#if __cplusplus >= 202002L
    // Use three-way comparison for sorting
    auto comparison = safe_packet_size.safe_three_way_compare(max_size);
    if (is_less(comparison)) {
        // Packet is below the maximum size
    }
#endif
}
*/

} // namespace numeric
} // namespace utility
} // namespace core
} // namespace lumex

using lumex::core::utility::numeric::SafeComparator;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_NUMERIC_HPP
// NOLINTEND(readability-simplify-boolean-expr)

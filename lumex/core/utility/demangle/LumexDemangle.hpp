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

#ifndef LUMEX_CORE_UTILITY_DEMANGLE_HPP
#define LUMEX_CORE_UTILITY_DEMANGLE_HPP

/**
 * @brief Demangles a C++ type name for human-readable output.
 *
 * This method takes a mangled C++ type name (as returned by typeid) and
 * attempts to demangle it into a human-readable format. On GCC/Clang
 * platforms, it uses the ABI's demangling function. On other platforms, it
 * returns the original name.
 *
 * @param[in] name The mangled type name to demangle (typically from
 * typeid().name())
 * @return std::string The demangled name if successful, or the original name
 * if:
 *         - Platform doesn't support demangling (non-GNU)
 *         - Demangling failed
 *         - Input was nullptr
 *
 * @note On GNU-compatible compilers (GCC/Clang), this uses abi::__cxa_demangle
 *       which handles:
 *       - Template instantiations
 *       - Namespace qualifications
 *       - CV-qualifiers
 *       - Calling conventions
 *
 * @warning The caller must ensure the input pointer is valid (not dangling).
 *          The method makes no ownership claims on the input string.
 *
 * @example
 *   std::cout << _demangle(typeid(std::vector<int>).name());
 *   // Outputs: "std::vector<int, std::allocator<int>>" instead of
 * "St6vectorIiSaIiEE"
 */
#ifdef __GNUG__
#include <cstdlib>
#include <cxxabi.h>
#include <string>
#include <typeinfo>

#define lumDemangle(type)                                                     \
  ([&] () -> std::string {                                                    \
    std::string name = typeid (type).name ();                                 \
    int status = 0;                                                           \
    char *demangled                                                           \
        = abi::__cxa_demangle (name.c_str (), nullptr, nullptr, &status);     \
    std::string result = (status == 0 && demangled) ? demangled : name;       \
    free (demangled);                                                         \
    return result;                                                            \
  }())
#else
#include <string>
#include <typeinfo>

#define lumDemangle(type) std::string (typeid (type).name ())
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace demangle
{
/**
 * @brief Demangles an already-mangled C++ name (e.g. one captured from
 *        a backtrace/stack unwinder), as opposed to the `lumDemangle`
 *        macro above, which mangles a type/expression itself (via
 *        `typeid`) before demangling it.
 * @details Reuses the exact same low-level mechanic `lumDemangle`
 *          already uses on this platform: `abi::__cxa_demangle` on
 *          GCC/Clang (`__GNUG__`); on every other compiler (MSVC
 *          included) `lumDemangle` itself does not attempt any
 *          demangling either - it falls back to returning the name
 *          unchanged - so this function mirrors that same fallback for
 *          consistency, rather than inventing a separate mechanism
 *          (e.g. `UnDecorateSymbolName`) that the macro above does not
 *          actually use.
 * @param[in] mangled The already-mangled name to demangle (typically
 *        obtained from a backtrace symbol, not from `typeid(...).name()`
 *        computed in-place - for that case, use `lumDemangle` instead).
 * @return The demangled name if successful, or `mangled` unchanged if:
 *         - the platform does not support demangling (non-GNU), or
 *         - demangling failed, or
 *         - `mangled` was `nullptr`.
 * @note On GNU-compatible compilers (GCC/Clang), this uses
 *       `abi::__cxa_demangle`, which handles template instantiations,
 *       namespace qualifications, cv-qualifiers, and calling conventions.
 * @warning The caller must ensure `mangled` is a valid, null-terminated
 *          string when non-null. This function makes no ownership
 *          claims on the input string.
 */
inline std::string
demangle_type_name (char const *mangled)
{
  if (mangled == nullptr)
    return std::string ();

#ifdef __GNUG__
  int status = 0;
  char *demangled = abi::__cxa_demangle (mangled, nullptr, nullptr, &status);
  std::string result = (status == 0 && demangled != nullptr)
                           ? std::string (demangled)
                           : std::string (mangled);
  free (demangled);
  return result;
#else
  return std::string (mangled);
#endif
}

/**
 * @brief Convenience overload of @ref demangle_type_name for
 *        `std::string` input.
 * @param[in] mangled The already-mangled name to demangle.
 * @return See the `char const *` overload.
 */
inline std::string
demangle_type_name (std::string const &mangled)
{
  return demangle_type_name (mangled.c_str ());
}
} // namespace demangle
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_DEMANGLE_HPP

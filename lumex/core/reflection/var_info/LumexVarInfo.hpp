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
 * @file LumexVarInfo.hpp
 * @brief Debug diagnostic printer for an arbitrary expression: name, value
 *        (if printable), demangled type, cv-qualifiers, size, address,
 *        noexcept-status, and call site.
 * @details `LUMEX_VARINFO(expr)` expands to a call into
 * `lumex::core::reflection::VarInfo`, capturing `expr`'s source text, value,
 * and `noexcept(expr)` at the call site.
 *
 * @note Philosophy difference from
 * `lumex/core/string/utility/LumexStringify.hpp`'s `stringify()`:
 * `stringify()` uses a hard `static_assert` to reject any non-streamable type
 * at compile time - the absence of `operator<<` is treated as a caller bug
 * that should fail the build. `FormatValue` below instead degrades gracefully
 * to a `"<no operator<<>"` placeholder for a non-streamable type, because
 * `LUMEX_VARINFO`/`VarInfo` is a debug-printing tool meant to be dropped onto
 * *any* expression while investigating something - failing to compile because
 * one field along the way happens to lack `operator<<` would defeat that
 * purpose. These two headers intentionally do not share one policy; do not try
 * to unify them.
 */
#ifndef LUMEX_CORE_REFLECTION_VAR_INFO_HPP
#define LUMEX_CORE_REFLECTION_VAR_INFO_HPP

#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "lumex/core/utility/demangle/LumexDemangle.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if __cplusplus >= 202002L
#include <source_location>
#endif

namespace lumex
{
namespace core
{
namespace reflection
{
namespace var_info
{
namespace VarInfoDetail
{
/**
 * @brief Detects whether `std::ostream::operator<<(T)` exists for a type T.
 * @note Classic expression-SFINAE (trailing decltype), not a concept -
 *       a single detector for every standard. The C++20 branch below
 *       uses a real concept (`Streamable`) instead, but this version
 *       remains the only one for C++11/14/17 (no concepts there) and
 *       is reused by the `if constexpr` branch on C++17-19 (concepts
 *       are not yet available there, but `if constexpr` already is).
 * @warning "Printable" here means literally "os << v compiles", not
 *          "T has a meaningful operator<<". A function pointer, and a
 *          non-capturing lambda (which implicitly converts to one,
 *          [expr.prim.lambda.closure]), have no dedicated ostream
 *          overload in the standard itself: object pointers get
 *          `operator<<(ostream&, void const*)` ([conv.ptr] - object
 *          pointer -> void* applies only to object types, a function
 *          is not one), but ANY pointer, including a function
 *          pointer, has an implicit conversion to bool ([conv.bool])
 *          - one user-defined conversion followed by one standard
 *          conversion, a single legal implicit conversion sequence -
 *          so `os << v` still compiles via `operator<<(ostream&, bool)`.
 * @warning This produces a platform-divergent result, confirmed by
 *          actually compiling it, not just by reading the standard:
 *          on libstdc++ (g++) and libc++ (clang), a function pointer
 *          prints "1"/"0" (whether it is null) via that bool path
 *          above; on the MSVC STL it prints a real hex address
 *          instead - its own templated ostream overloads intercept
 *          the pointer before the bool conversion is reached. The
 *          standard only guarantees "something gets printed", not
 *          which value - do not assert on the literal printed value
 *          for this case in tests, only that something non-empty was
 *          printed and it differs from the no-operator<< placeholder.
 */
template <typename T> class HasOstreamOperator
{
  template <typename U>
  static auto Test (int) -> decltype (std::declval<std::ostream &> ()
                                          << std::declval<U const &> (),
                                      std::true_type{});

  template <typename U> static std::false_type Test (...);

public:
  static bool const value = decltype (Test<T> (0))::value;
};

#if __cplusplus >= 202002L
/**
 * @brief C++20: same detector as HasOstreamOperator, expressed as a
 *        concept/requires clause - reads directly as a condition,
 *        without the trailing-decltype trick. Used only by the
 *        C++20 branch of FormatValue below; HasOstreamOperator
 *        remains the shared one for older standards and is not removed.
 */
template <typename T>
concept Streamable = requires (std::ostream &os, T const &v) { os << v; };
#endif

#if __cplusplus >= 201703L
// C++17+: if constexpr instead of a pair of enable_if overloads - one
// function instead of two, with the condition visible right in the
// body instead of split across two signatures.
template <typename T>
std::string
FormatValue (T const &value)
{
#if __cplusplus >= 202002L
  LUMEX_CONSTEXPR_IF (Streamable<typename std::decay<T>::type>)
#else
  LUMEX_CONSTEXPR_IF (HasOstreamOperator<typename std::decay<T>::type>::value)
#endif
  {
    std::ostringstream oss;
    oss << value;
    return oss.str ();
  }
  else
  {
    // The type is visible separately (typeid+demangle always work),
    // so a value with no operator<< only needs a placeholder,
    // without failing the build.
    return "<no operator<<>";
  }
}
#else
// C++11/14: if constexpr is unavailable (C++17) - classic
// enable_if dispatch between two overloads, selected via SFINAE.
template <typename T>
typename std::enable_if<
    HasOstreamOperator<typename std::decay<T>::type>::value, std::string>::type
FormatValue (T const &value)
{
  std::ostringstream oss;
  oss << value;
  return oss.str ();
}

/// @brief The type is visible separately (typeid+demangle always
///        work), so a value with no operator<< only needs a
///        placeholder, without failing the build.
template <typename T>
typename std::enable_if<
    !HasOstreamOperator<typename std::decay<T>::type>::value,
    std::string>::type
FormatValue (T const &)
{
  return "<no operator<<>";
}
#endif
} // namespace VarInfoDetail

/**
 * @brief Builds a diagnostic string about an arbitrary expression:
 *        name, value (if the type supports operator<<), demangled
 *        type, cv-qualifiers, size, address, noexcept-status of the
 *        original expression, and call site.
 * @tparam T Deduced by the compiler. A forwarding reference (T&&)
 *           accepts both an lvalue (named variable) and an rvalue/
 *           temporary the same way - for an rvalue, T is deduced
 *           without a reference and the parameter becomes T&&, binding
 *           directly to the temporary (its lifetime is extended for
 *           the call), so `std::addressof(value)` is safe in both
 *           cases without manual materialization tricks.
 * @param exprText Source text of the expression (from LUMEX_VARINFO via
 * `#expr`).
 * @param value The expression itself.
 * @param isExprNoexcept Result of `noexcept(expr)`, computed in the
 *        macro itself, not here. `noexcept(value)` inside this
 *        function would check trivial access to the already-bound
 *        named parameter `value`, not the original expression - that
 *        is always noexcept regardless of whether the original call
 *        was, so such a check inside the function would always be
 *        true and meaningless.
 * @param file `__FILE__` of the call site (not used directly by the
 *        macro on C++20 - see the `std::source_location` overload below).
 * @param line `__LINE__` of the call site.
 * @warning Bit-fields: `LUMEX_VARINFO(s.bitField)` never compiles, on
 *          any standard - not a bug in this macro, but a direct
 *          consequence of [class.bit]: "The address-of operator & shall
 *          not be applied to a bit-field" and, separately, "A
 *          non-const reference shall not bind to a bit-field" - T&& is
 *          deduced as a reference to the field's (non-bit-field) type,
 *          and nothing can bind that to the bit-field itself. Copy the
 *          value into a plain variable first:
 *          `auto tmp = s.bitField; LUMEX_VARINFO(tmp);`.
 * @note This function is not noexcept: besides exceptions from the
 *       type's own operator<< (if any), std::ostringstream/std::string
 *       can throw bad_alloc/length_error on allocation failure, like
 *       any ordinary string handling in this codebase.
 * @note Requires only the standard library; on GCC/Clang additionally
 *       cxxabi.h (via `lumex/core/utility/demangle/LumexDemangle.hpp`).
 */
template <typename T>
std::string
VarInfo (char const *exprText, T &&value, bool isExprNoexcept,
         char const *file, int line)
{
  // The address reuses exactly the pointer type addressof returned
  // (via decltype), not one rebuilt from decay<T> - for an
  // array/string literal, addressof yields a pointer TO THE ARRAY
  // (e.g. int(*)[5]), while decay<T>* would give int** - a completely
  // different, incompatible pointer structure (const_cast between them
  // is ill-formed, not merely imprecise). remove_pointer+remove_cv
  // strips the qualification while keeping the original (possibly
  // array) structure, and that structure - not the decay-simplified
  // one - is the one valid for a static_cast to an object pointer ->
  // void* ([conv.ptr] requires a pointer-to-object-type, which
  // pointer-to-array is, unlike the incompatible "pointer to element").
  // typename ...::type, not remove_cv_t/remove_pointer_t: this function
  // (unlike VarInfoDetail::FormatValue above) is the common C++11
  // baseline, and the _t aliases only appeared in C++14.
  using AddressPointerType = decltype (std::addressof (value));
  using RawPointerType = typename std::remove_cv<
      typename std::remove_pointer<AddressPointerType>::type>::type *;
  void const *addr = static_cast<void const *> (
      const_cast<RawPointerType> (std::addressof (value)));

  // sizeof(value), not sizeof(decay<T>::type): for an array, decay
  // would collapse the type into a pointer and sizeof would return the
  // pointer's size (8 bytes), not the array's real size - sizeof on a
  // reference parameter transparently gives the size of what it refers to.
  std::ostringstream oss;
  oss << exprText << " = " << VarInfoDetail::FormatValue (value) << " [type="
      << ::lumex::core::utility::demangle::demangle_type_name (
             typeid (value).name ())
      << (std::is_const<typename std::remove_reference<T>::type>::value
              ? " const"
              : "")
      << (std::is_volatile<typename std::remove_reference<T>::type>::value
              ? " volatile"
              : "")
      << ", size=" << sizeof (value) << " bytes, addr=" << addr
      << ", noexcept=" << (isExprNoexcept ? "true" : "false") << ", at "
      << file << ":" << line << "]";
  return oss.str ();
}

#if __cplusplus >= 202002L
/**
 * @brief C++20: same as above, but the call site comes from
 *        `std::source_location::current()` instead of `__FILE__`/
 *        `__LINE__` passed explicitly by the macro - it is filled in
 *        by the compiler as a default argument value, evaluated at the
 *        call site, not at the function's declaration site. A separate
 *        overload, not a replacement: both must exist at the same
 *        time, the macro below picks the right one via `__cplusplus`.
 */
template <typename T>
std::string
VarInfo (char const *exprText, T &&value, bool isExprNoexcept,
         std::source_location const &loc = std::source_location::current ())
{
  return VarInfo (exprText, std::forward<T> (value), isExprNoexcept,
                  loc.file_name (), static_cast<int> (loc.line ()));
}
#endif
} // namespace var_info
} // namespace reflection
} // namespace core
} // namespace lumex

/**
 * @brief Diagnostic information about an arbitrary expression: name, value,
 *        type, cv-qualifiers, size, address, noexcept-status, call site.
 * @note `noexcept(expr)` is evaluated here, in the macro itself, over the
 *       original expression text - `noexcept()` does not evaluate its
 *       operand, so `expr` is not executed twice. Evaluating it inside
 *       `VarInfo()` would be too late: by the time it gets there, `expr` has
 *       already become just a named parameter.
 * @note Variadic (`...`/`__VA_ARGS__`), not a single `expr` parameter: the
 *       preprocessor does not understand template angle brackets - only
 *       round/square/curly ones. `LUMEX_VARINFO(std::pair<int,int>(1,2))`
 *       with a single parameter would see the comma inside `<int,int>` as a
 *       macro-argument separator ("passed 2 arguments, but takes just 1",
 *       confirmed by real compilation) and would require extra parentheses
 *       from the caller (`LUMEX_VARINFO((std::pair<int,int>(1,2)))`).
 *       `__VA_ARGS__` re-joins everything back into one token sequence
 *       before the expression is parsed again by the compiler (which does
 *       understand angle brackets), so
 * `LUMEX_VARINFO(std::pair<int,int>(1,2))` works without a wrapper.
 * @see lumex::core::reflection::VarInfo
 */
#if __cplusplus >= 202002L
  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LUMEX_VARINFO(...)                                                    \
  ::lumex::core::reflection::var_info::VarInfo (                              \
      #__VA_ARGS__, (__VA_ARGS__), LUMEX_NOEXCEPT_IF (__VA_ARGS__))
#else
  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LUMEX_VARINFO(...)                                                    \
  ::lumex::core::reflection::var_info::VarInfo (                              \
      #__VA_ARGS__, (__VA_ARGS__), LUMEX_NOEXCEPT_IF (__VA_ARGS__), __FILE__, \
      __LINE__)
#endif

#endif // !LUMEX_CORE_REFLECTION_VAR_INFO_HPP

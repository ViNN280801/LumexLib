/*
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

#ifndef LUMEX_TESTS_SUPPORT_OSTREAM_PROBE_HPP
#define LUMEX_TESTS_SUPPORT_OSTREAM_PROBE_HPP

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

/*
 * What the standard library of the build says about writing a value to a
 * narrow stream, found without any trait of LumexLib: the tests of the stream
 * traits, of stringify, join and quote compare the library's own answer with
 * theirs (for wchar_t, char16_t, char32_t and char8_t the answer changes with
 * the standard and the library).
 */

/*
 * LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS is 1 where the library is known
 * (measured) to delete the inserters of a narrow stream for wchar_t, char8_t,
 * char16_t, char32_t and the pointers to them (P1423R3, [ostream.syn]): from
 * C++20 with libstdc++ 13 and libc++ 23. For the other libraries and for
 * libstdc++ 8 at -std=c++2a (no deleted inserters) it is 0 and the tests only
 * compare with the stream itself.
 */
#if __cplusplus > 201703L                                                     \
    && ((defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 13)                 \
        || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 230000))
#define LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS 1
#else
#define LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS 0
#endif

namespace lumex_tests_support
{
template <typename...> struct probe_make_void
{
  typedef void type;
};

/**
 * @brief `std::declval<std::ostream &> () << std::declval<T> ()` is
 * well-formed.
 */
template <typename T, typename Enable = void>
struct ostream_accepts : std::false_type
{
};

template <typename T>
struct ostream_accepts<
    T, typename probe_make_void<decltype (std::declval<std::ostream &> ()
                                          << std::declval<T> ())>::type>
    : std::true_type
{
};

/** @brief The text `std::ostringstream << value` produces. */
template <typename T>
std::string
streamed (T const &value)
{
  std::ostringstream stream;
  stream << value;
  return stream.str ();
}
} // namespace lumex_tests_support

#endif // !LUMEX_TESTS_SUPPORT_OSTREAM_PROBE_HPP

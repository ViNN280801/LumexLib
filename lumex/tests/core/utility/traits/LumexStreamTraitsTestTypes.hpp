/*
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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

#ifndef LUMEX_TESTS_CORE_UTILITY_TRAITS_HPP
#define LUMEX_TESTS_CORE_UTILITY_TRAITS_HPP

#include <ostream>

// Types of the stream trait tests. The test sources of several standards
// (LumexStreamTraits.cxx11.tests.cpp, .cxx14 and .cxx20) check the same
// types, so they live here. The namespace is named, not anonymous, so the
// operator<< below is one inline function in every test source; argument
// dependent lookup finds it there, as it would for a user type.

namespace lumex_stream_traits_test
{
struct custom_streamable_t
{
  int value;
};

inline std::ostream &
operator<< (std::ostream &os, custom_streamable_t const &item)
{
  return os << item.value;
}

struct not_streamable_t
{
  int value;
};

enum plain_enum_e
{
  plain_value = 1
};

enum class scoped_enum_e
{
  scoped_value = 1
};

struct derived_streamable_t : custom_streamable_t
{
};
} // namespace lumex_stream_traits_test

#endif // !LUMEX_TESTS_CORE_UTILITY_TRAITS_HPP

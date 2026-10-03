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

#ifndef LUMEX_TESTS_CORE_EXPECTED_EXPECTED_TEST_TYPES_HPP
#define LUMEX_TESTS_CORE_EXPECTED_EXPECTED_TEST_TYPES_HPP

#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

// Error types of the expected tests and the per-type steps their typed tests
// share. The typed tests run over int, std::string, SimpleError and
// ComplexError; a step that only some of those types have is an overload for
// them plus a template for the rest. Overload resolution prefers the
// non-template, so this is the C++11 form of an if constexpr chain on the
// type, and every expected suite (C++11, C++17, C++20) compiles it.

// Simple error type
enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

// Complex type of error with resource ownership (for checking move and copy)
struct ComplexError
{
  std::string message;
  int code;
  std::unique_ptr<int> resource;

  explicit ComplexError (std::string msg = "Default Error", int c = 100)
      : message (std::move (msg)), code (c), resource (new int (c))
  {
  }

  ComplexError (ComplexError const &other)
      : message (other.message), code (other.code),
        resource (other.resource ? new int (*other.resource) : nullptr)
  {
  }

  ComplexError &
  operator= (ComplexError const &other)
  {
    if (this != &other)
      {
        message = other.message;
        code = other.code;
        resource.reset (other.resource ? new int (*other.resource) : nullptr);
      }
    return *this;
  }

  ComplexError (ComplexError &&other) noexcept
      : message (std::move (other.message)), code (other.code),
        resource (std::move (other.resource))
  {
    other.code = 0; // Clear the source resource
  }

  ComplexError &
  operator= (ComplexError &&other) noexcept
  {
    if (this != &other)
      {
        message = std::move (other.message);
        code = other.code;
        resource = std::move (other.resource);
        other.code = 0; // Clear the source resource
      }
    return *this;
  }

  bool
  operator== (ComplexError const &other) const
  {
    return message == other.message && code == other.code
           && ((!resource && !other.resource)
               || (resource && other.resource
                   && *resource == *other.resource));
  }

  bool
  operator!= (ComplexError const &other) const
  {
    return !(*this == other);
  }
};

// Selects an overload by type when no value of the type is at hand.
template <typename T> struct TypeTag
{
};

// Two distinct known error values for a fixture: SimpleError and
// ComplexError get their own, the other types the default constructor.
inline void
InitErrorPair (SimpleError &first, SimpleError &second)
{
  first = SimpleError::InvalidInput;
  second = SimpleError::NetworkFailure;
}

inline void
InitErrorPair (ComplexError &first, ComplexError &second)
{
  first = ComplexError ("Test Error 1", 101);
  second = ComplexError ("Test Error 2", 102);
}

template <typename T>
void
InitErrorPair (T &first, T &second)
{
  first = T ();
  second = T ();
}

// A moved-from ComplexError has code 0 and no resource; the other types make
// no promise about their moved-from state.
inline void
ExpectMovedFrom (ComplexError const &error)
{
  EXPECT_EQ (error.code, 0);
  EXPECT_EQ (error.resource, nullptr);
}

template <typename T>
void
ExpectMovedFrom (T const &)
{
}

// Only ComplexError is compared: the other types are not checked at the call
// sites that use this step.
inline void
ExpectEqualComplex (ComplexError const &actual, ComplexError const &expected)
{
  EXPECT_EQ (actual, expected);
}

template <typename T>
void
ExpectEqualComplex (T const &, T const &)
{
}

// Payload of one iteration of a Perf_ loop.
inline void
AssignPerfError (int &error, int i)
{
  error = i;
}

inline void
AssignPerfError (std::string &error, int i)
{
  error = "Error" + std::to_string (i);
}

inline void
AssignPerfError (SimpleError &error, int i)
{
  error = static_cast<SimpleError> (i % 3 + 1);
}

inline void
AssignPerfError (ComplexError &error, int i)
{
  error = ComplexError ("Perf Error", i);
}

template <typename T>
void
AssignPerfError (T &, int)
{
}

// Wall-clock budget of a Perf_ loop over the error type, in milliseconds:
// ComplexError is much slower because of unique_ptr and std::string.
inline long long
PerfThresholdMs (TypeTag<ComplexError>)
{
  return 1000;
}

inline long long
PerfThresholdMs (TypeTag<std::string>)
{
  return 200;
}

template <typename T>
long long
PerfThresholdMs (TypeTag<T>)
{
  return 100;
}

// Name of the error type in failure messages.
inline char const *
TypeLabel (TypeTag<int>)
{
  return "int";
}

inline char const *
TypeLabel (TypeTag<std::string>)
{
  return "string";
}

inline char const *
TypeLabel (TypeTag<SimpleError>)
{
  return "SimpleError";
}

inline char const *
TypeLabel (TypeTag<ComplexError>)
{
  return "ComplexError";
}

template <typename T>
char const *
TypeLabel (TypeTag<T>)
{
  return "Unknown";
}

#endif // !LUMEX_TESTS_CORE_EXPECTED_EXPECTED_TEST_TYPES_HPP

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

#ifndef LUMEX_XML_UTILITY_XML_CLEANER_HPP
#define LUMEX_XML_UTILITY_XML_CLEANER_HPP

#include "lumex/LumexExport.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace utility
{
/**
 * @brief A simple RAII (Resource Acquisition Is Initialization) wrapper for
 * managing dynamically allocated memory.
 * @details `XmlCleaner` ensures that a provided memory block is deallocated
 * using a specified deleter when the `XmlCleaner` object goes out of scope,
 * unless `release()` is called. This is particularly useful for managing
 * memory that might be conditionally owned or passed between functions.
 * @tparam T The type of the pointer to be managed.
 */
template <typename T> struct LUMEX_API XmlCleaner
{ // NOLINT(cppcoreguidelines-special-member-functions)
  /// @brief Type alias for the deleter function pointer.
  using D = void (*) (T *);

  /// @brief Pointer to the managed data.
  T *data{}; // NOLINT(misc-non-private-member-variables-in-classes)
  /// @brief Function pointer to the deleter (e.g., `free`, `delete[]`).
  D deleter{}; // NOLINT(misc-non-private-member-variables-in-classes)

  /**
   * @brief Constructs an `XmlCleaner` object, taking ownership of the provided
   * data.
   * @param[in] data_ A pointer to the data to be managed.
   * @param[in] deleter_ A function pointer to the deleter responsible for
   * freeing `data_`.
   */
  XmlCleaner (T *data_, D deleter_) : data (data_), deleter (deleter_) {}

  /**
   * @brief Destructor. Deallocates the managed data using the provided deleter
   * if `release()` was not called.
   */
  ~XmlCleaner ()
  {
    if (data)
      deleter (data);
  }

  /**
   * @brief Releases ownership of the managed data.
   * @return A pointer to the previously managed data. The caller is now
   * responsible for its deallocation.
   * @details After calling `release()`, the `XmlCleaner` object will no longer
   * deallocate the memory.
   */
  T *
  release ()
  {
    T *result = data;
    data = nullptr;
    return result;
  }
};
} // namespace utility
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_UTILITY_XML_CLEANER_HPP

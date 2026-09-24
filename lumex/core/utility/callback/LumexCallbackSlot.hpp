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

/**
 * @file LumexCallbackSlot.hpp
 * @brief Process-wide slot for one consumer-installed function pointer.
 *
 * @details A library that must not depend on a logging facility (or on any
 * other consumer policy) exposes a hook instead: the consumer installs a
 * plain function pointer, the library calls it, and falls back to its own
 * default when nothing is installed or the installed function throws.
 * `LumexCallbackSlot` is that hook, written once:
 *
 * @code
 * struct my_reporter_tag_t;
 * using MyReporterSlot
 *     = lumex::core::utility::callback::LumexCallbackSlot<
 *         my_reporter_tag_t, void (char const *)>;
 *
 * MyReporterSlot::set (&write_to_my_log);
 * MyReporterSlot::invoke_or (&write_to_stderr, "message");
 * @endcode
 *
 * `Tag` keeps two slots with the same signature apart; it may stay an
 * incomplete type. The stored pointer is a `std::atomic`, so `set` and
 * `invoke_or` may race freely.
 *
 * Storage is a static data member of the class template. Each binary
 * (executable or shared library) that instantiates a slot has its own copy
 * on Windows. For one slot shared by an executable and a shared library,
 * touch the slot only from a compiled source file of that library and export
 * plain `set` / `get` wrapper functions from it.
 */
#ifndef LUMEX_CORE_UTILITY_CALLBACK_HPP
#define LUMEX_CORE_UTILITY_CALLBACK_HPP

#include <atomic>
#include <exception>
#include <iostream>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace callback
{
/**
 * @brief Primary template; only the function-type specialization below is
 * defined.
 * @tparam Tag Any type, used only to tell slots apart.
 * @tparam Signature A function type such as `void (char const *)`.
 */
template <typename Tag, typename Signature> class LumexCallbackSlot;

template <typename Tag, typename Result, typename... Args>
class LumexCallbackSlot<Tag, Result (Args...)>
{
public:
  /** @brief The stored pointer type. */
  using function_type = Result (*) (Args...);

  LumexCallbackSlot () = delete;

  /** @brief Installs `function`; `nullptr` clears the slot. */
  static void
  set (function_type function) LUMEX_NOEXCEPT
  {
    m_slot.store (function, std::memory_order_release);
  }

  /** @brief Returns the installed function, or `nullptr`. */
  LUMEX_ATTRIBUTE_NODISCARD ("the installed function is the result")
  static function_type
  get () LUMEX_NOEXCEPT
  {
    return m_slot.load (std::memory_order_acquire);
  }

  /** @brief Installs `function` and returns the previous one. */
  static function_type
  exchange (function_type function) LUMEX_NOEXCEPT
  {
    return m_slot.exchange (function, std::memory_order_acq_rel);
  }

  /** @brief Clears the slot. */
  static void
  reset () LUMEX_NOEXCEPT
  {
    set (nullptr);
  }

  /** @brief Whether a function is installed. */
  LUMEX_ATTRIBUTE_NODISCARD ("the state is the point of the call")
  static bool
  is_set () LUMEX_NOEXCEPT
  {
    return get () != nullptr;
  }

  /**
   * @brief Calls the installed function with `args`, or `fallback` with the
   * same `args` when the slot is empty or the installed function throws.
   * @param fallback Any callable taking `Args...` and returning something
   * convertible to `Result`. Its exceptions propagate to the caller.
   * @return What the called function returned.
   */
  template <typename Fallback>
  static Result
  invoke_or (Fallback &&fallback, Args... args)
  {
    function_type const function = get ();
    if (function != nullptr)
      {
        try
          {
            return function (args...);
          }
        catch (std::exception const &e)
          {
            std::cerr << "Exception thrown by callback: " << e.what () << '\n';
          }
        catch (...)
          {
            std::cerr << "Unknown exception thrown by callback" << '\n';
          }
        std::endl (std::cerr);
      }
    return fallback (args...);
  }

  /**
   * @class Scoped
   * @brief Installs a function for the lifetime of the object and restores
   *        the previous one on destruction.
   */
  class Scoped
  {
  public:
    explicit Scoped (function_type function) LUMEX_NOEXCEPT
        : _previous (exchange (function))
    {
    }

    ~Scoped () { set (_previous); }

    Scoped (Scoped const &) = delete;
    Scoped &operator= (Scoped const &) = delete;

  private:
    function_type _previous;
  };

private:
  static std::atomic<function_type> m_slot;
};

template <typename Tag, typename Result, typename... Args>
std::atomic<Result (*) (Args...)>
    LumexCallbackSlot<Tag, Result (Args...)>::m_slot{ nullptr };
} // namespace callback
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_CALLBACK_HPP

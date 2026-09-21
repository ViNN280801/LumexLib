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
 * @file LumexTimer.hpp
 * @brief High-resolution stopwatch, callable timing helpers, and a thin
 * `LUMEX_MEASURE_TIME` macro.
 * @details Defines `LumexTimer` (start/stop/elapsed), `extract_function_name`,
 * `measure_execution_time`, gated `measure_time` (ostream sink, optional env
 * gate), and `LUMEX_MEASURE_TIME` which expands to that C++ API.
 */
#ifndef LUMEX_CORE_TIME_TIMER_HPP
#define LUMEX_CORE_TIME_TIMER_HPP

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

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace time
{
namespace timer
{
/**
 * @brief Default environment variable that enables `measure_time` reporting
 * when env gating is on.
 */
inline char const *
default_measure_time_env_name () LUMEX_NOEXCEPT
{
  return "LUMEX_ENABLE_MEASURE_TIME_LOG";
}

/**
 * @brief A high-resolution stopwatch for measuring elapsed wall-clock time.
 * @details Wraps `std::chrono::high_resolution_clock` behind a small
 * start/stop/elapsed interface. A single `LumexTimer` instance is meant to be
 * started and stopped from one thread at a time; it is not internally
 * synchronized.
 *
 * @example
 * LumexTimer timer;
 * timer.start_timer();
 * doSomeWork();
 * timer.stop_timer();
 * std::cout << "Elapsed: " << timer.elapsed_time_ms() << "ms" << std::endl;
 */
class LUMEX_API LumexTimer
{
public:
  /// @brief Defaulted constructor; the timer measures zero elapsed time until
  /// started/stopped.
  LumexTimer () = default;

  /**
   * @brief Starts (or restarts) the timer.
   * @details Records the current high-resolution time as the timer's start
   * point.
   */
  void start_timer ();

  /**
   * @brief Stops the timer.
   * @details Records the current high-resolution time as the timer's end
   * point. Call this before `elapsed_time_ms()` for a meaningful, stable
   * result.
   */
  void stop_timer ();

  /**
   * @brief Returns the elapsed time between the last
   * `start_timer()`/`stop_timer()` pair.
   * @return Elapsed time in milliseconds. Zero if the timer was never
   * started/stopped.
   * @note Call `stop_timer()` before calling this function for a stable
   * result; calling it while the timer is still running returns the elapsed
   * time up to the previous `stop_timer()` call (or zero, if it was never
   * stopped).
   */
  long long elapsed_time_ms () const;

private:
  std::chrono::high_resolution_clock::time_point m_start_tp{};
  std::chrono::high_resolution_clock::time_point m_end_tp{};
  bool m_is_started{ false };
};

/**
 * @brief Extracts a short, human-readable name from a stringified call
 * expression.
 * @details Given the textual source of an expression (e.g. produced by
 * stringifying it with the preprocessor `#` operator), this trims
 * leading/trailing whitespace, discards everything from the first `(` onward,
 * and then keeps only the part after the last
 *          `->` or `.` (i.e. strips any object/pointer member-access prefix).
 * Useful for building short diagnostic labels from an arbitrary call
 * expression's source text.
 * @param expr_str The stringified expression, e.g. "myObject->DoSomething(1,
 * 2)".
 * @return The extracted name (e.g. "DoSomething"), or `expr_str` itself if
 * extraction left nothing (e.g. the input was empty or contained no
 * identifier-like prefix).
 *
 * @example
 * extract_function_name("obj->DoSomething(1, 2)"); // -> "DoSomething"
 * extract_function_name("obj.Method()");            // -> "Method"
 * extract_function_name("justName");                // -> "justName"
 */
LUMEX_API std::string extract_function_name (std::string const &expr_str);

/**
 * @brief Writes a measure-time report line to `out`.
 * @param out Destination stream (caller-owned; typically `std::clog`).
 * @param message Label prefix; when empty, a generic "Time: N [ms]" line is
 * written.
 * @param elapsed_ms Elapsed wall-clock time in milliseconds.
 */
LUMEX_API void write_measure_time_report (std::ostream &out,
                                          std::string const &message,
                                          long long elapsed_ms);

/**
 * @brief Invokes `callable` with `args` and returns elapsed wall-clock time
 * in milliseconds.
 * @tparam Callable Invocable type.
 * @tparam Args Argument types forwarded to `callable`.
 * @param callable Callable to time.
 * @param args Arguments forwarded to `callable`.
 * @return Elapsed milliseconds (non-negative).
 */
template <typename Callable, typename... Args>
long long
measure_execution_time (Callable &&callable, Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::is_callable_v<Callable, Args...>,
      "Callable must be invocable with Args");

  LumexTimer timer;
  timer.start_timer ();
  std::forward<Callable> (callable) (std::forward<Args> (args)...);
  timer.stop_timer ();
  return timer.elapsed_time_ms ();
}

/**
 * @brief Optionally gates timing/reporting via an environment variable, then
 * measures `callable` and writes the result to `out`.
 * @details When `need_to_gate_via_env` is true, timing and reporting run only
 * if `env_name` is set (non-empty via `is_env_set`); otherwise `callable` is
 * invoked without timing. When `need_to_gate_via_env` is false, `env_name` is
 * unused and timing/reporting always run.
 * @tparam Callable Nullary invocable (typically a capturing lambda).
 * @param callable Work to run (and optionally time).
 * @param message Report label (may be empty).
 * @param out Output stream; defaults to `std::clog`.
 * @param need_to_gate_via_env When true, require `env_name` to be set.
 * @param env_name Environment variable name; ignored when gating is off.
 * @return `true` if timing and reporting ran; `false` if the env gate skipped
 * them (callable still ran).
 */
template <typename Callable>
bool
measure_time (Callable &&callable, std::string const &message,
              std::ostream &out = std::clog, bool need_to_gate_via_env = true,
              LUMEX_ATTRIBUTE_MAYBE_UNUSED char const *env_name
              = default_measure_time_env_name ())
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::is_callable_v<Callable>,
      "Callable must be nullary-invocable");

  if (need_to_gate_via_env)
    {
      if (env_name == nullptr
          || !lumex::core::environment::env::is_env_set (env_name))
        {
          std::forward<Callable> (callable) ();
          return false;
        }
    }
  else
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (env_name);
    }

  long long const elapsed_ms
      = measure_execution_time (std::forward<Callable> (callable));
  write_measure_time_report (out, message, elapsed_ms);
  return true;
}
} // namespace timer
} // namespace time
} // namespace core
} // namespace lumex

/**
 * @brief Alias for lumex::core::time::timer::LumexTimer.
 */
using LumexTimer = lumex::core::time::timer::LumexTimer;

// NOLINTBEGIN(cppcoreguidelines-macro-usage) - thin wrappers over the C++ API;
// argument count dispatch and `#expr` stringify require the preprocessor.

/**
 * @brief Times `expr` and reports to `std::clog` when the default env gate
 * allows it.
 * @details One-argument form auto-builds the message from `#expr` via
 * `extract_function_name`. Two-argument form uses `msg` as the report label.
 * Full control (custom stream, gate flag, env name) is on `measure_time`.
 *
 * Usage:
 *   LUMEX_MEASURE_TIME(doWork());
 *   LUMEX_MEASURE_TIME(doWork(), "custom label");
 */
#define LUMEX_MEASURE_TIME(...) LUMEX_MEASURE_TIME_DISPATCH (__VA_ARGS__)

#define LUMEX_MEASURE_TIME_DISPATCH(...)                                      \
  LUMEX_MEASURE_TIME_GET_OVERLOAD (__VA_ARGS__, LUMEX_MEASURE_TIME_2,         \
                                   LUMEX_MEASURE_TIME_1)                      \
  (__VA_ARGS__)

#define LUMEX_MEASURE_TIME_GET_OVERLOAD(_1, _2, NAME, ...) NAME

#define LUMEX_MEASURE_TIME_1(expr)                                            \
  ::lumex::core::time::timer::measure_time (                                  \
      [&] () -> decltype (expr) { return expr; },                             \
      "'" + ::lumex::core::time::timer::extract_function_name (#expr)         \
          + "' command was processed.")

#define LUMEX_MEASURE_TIME_2(expr, msg)                                       \
  ::lumex::core::time::timer::measure_time (                                  \
      [&] () -> decltype (expr) { return expr; }, (msg))

// NOLINTEND(cppcoreguidelines-macro-usage)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_TIME_TIMER_HPP

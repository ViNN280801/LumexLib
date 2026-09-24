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
 * @file LumexJsonDiagnostics.hpp
 * @brief Process-wide report sink for the non-throwing JSON helpers.
 *
 * @details `LumexJsonHelper` never throws: it returns `false`, an empty
 * document or a default value, and describes what went wrong through this
 * sink. The library does not depend on any logging facility. A consumer
 * installs one function pointer (`set_diagnostic_reporter`) to route the
 * messages into its own logger. Without a reporter, or when the reporter
 * throws, `warning` and `error` messages go to `std::cerr` and `debug` and
 * `info` messages are dropped.
 *
 * The module is header-only, so the reporter lives in a
 * `LumexCallbackSlot` instantiated by every binary that includes this
 * header. On Windows each executable or shared library has its own slot:
 * install the reporter from the binary that calls the helpers.
 */
#ifndef LUMEX_APPLIED_JSON_DIAGNOSTICS_HPP
#define LUMEX_APPLIED_JSON_DIAGNOSTICS_HPP

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace json
{
namespace diagnostics
{
/**
 * @brief Severity of a diagnostic message.
 * @details `toString()` returns the enumerator name (`"warning"`).
 */
LUMEX_DEFINE_REFLECTED_ENUM (LumexJsonDiagnosticLevel, std::uint8_t, (debug),
                             (info), (warning), (error))

/**
 * @brief Consumer report sink for one diagnostic message.
 * @param level Severity of the message.
 * @param message Non-owning, null-terminated text. Valid only for the call.
 */
using json_diagnostic_fn
    = void (*) (LumexJsonDiagnosticLevel level, char const *message);

namespace Detail
{
struct json_diagnostic_tag_t;
} // namespace Detail

/** @brief The slot that holds the installed reporter (one per binary). */
using LumexJsonDiagnosticSlot
    = lumex::core::utility::callback::LumexCallbackSlot<
        Detail::json_diagnostic_tag_t,
        void (LumexJsonDiagnosticLevel, char const *)>;

/**
 * @brief Installs the report sink. `nullptr` restores the default
 * (`warning` and `error` to `std::cerr`, the rest dropped).
 */
inline void
set_diagnostic_reporter (json_diagnostic_fn reporter) LUMEX_NOEXCEPT
{
  LumexJsonDiagnosticSlot::set (reporter);
}

/** @brief Returns the installed sink, or `nullptr` for the default one. */
inline json_diagnostic_fn
get_diagnostic_reporter () LUMEX_NOEXCEPT
{
  return LumexJsonDiagnosticSlot::get ();
}

namespace Detail
{
inline void
append_parts (std::ostringstream &)
{
}

template <typename Head, typename... Tail>
inline void
append_parts (std::ostringstream &stream, Head const &head,
              Tail const &...tail)
{
  stream << head;
  append_parts (stream, tail...);
}

/**
 * @brief Default sink: writes `[level] message` to `std::cerr` for
 * `warning` and `error`, drops `debug` and `info`. Never throws.
 */
inline void
write_to_stderr (LumexJsonDiagnosticLevel level,
                 char const *message) LUMEX_NOEXCEPT
{
  if (level != LumexJsonDiagnosticLevel::warning
      && level != LumexJsonDiagnosticLevel::error)
    return;
  try
    {
      std::cerr << '[' << toString (level) << "] " << message << '\n';
      std::cerr.flush ();
    }
  catch (...)
    {
    }
}

/**
 * @brief Concatenates `parts` with `operator<<` and sends the text to the
 * installed sink, or to the default one when the slot is empty or the sink
 * throws. Never throws: a failure while building the text drops the message.
 */
template <typename... Parts>
inline void
report (LumexJsonDiagnosticLevel level, Parts const &...parts) LUMEX_NOEXCEPT
{
  try
    {
      std::ostringstream stream;
      append_parts (stream, parts...);
      std::string const message = stream.str ();
      LumexJsonDiagnosticSlot::invoke_or (&write_to_stderr, level,
                                          message.c_str ());
    }
  catch (...)
    {
    }
}
} // namespace Detail
} // namespace diagnostics
} // namespace json
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_JSON_DIAGNOSTICS_HPP

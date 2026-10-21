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
 * @file LumexTransportLog.hpp
 * @brief The log of the transports: one sink slot, a level threshold, a
 * component filter and the byte dump format.
 * @details The library writes log records to a sink, a plain function
 * `void (log_record_t const &)` set with `set_log_sink`; without one, records
 * of level `warning` and above go to `std::cerr`. A record below the
 * threshold (`set_log_level`, default `warning`) or of a filtered component
 * costs one atomic load and formats nothing. `LUMEX_TRANSPORT_LOG_MIN_LEVEL`
 * (0 trace .. 4 error, default 0, the CMake option of the same name) removes
 * the calls below it at compile time.
 *
 * The slot, the threshold and the filter live in a compiled source of the
 * transport library, which is always static: each binary that links it (an
 * executable, every DLL) has its own; set the sink in each.
 *
 * Levels (the transport's level map): `error` an open or write that failed, a
 * session-stopping fault; `warning` a dropped frame, a timeout before a retry,
 * a buffer limit; `info` open, close, reconnect; `debug` one line per frame or
 * operation; `trace` raw chunks with their bytes.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_LOG_LOG_HPP
#define LUMEX_APPLIED_TRANSPORT_LOG_LOG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/// The lowest level that is compiled in: 0 trace, 1 debug, 2 info, 3 warning,
/// 4 error.
#ifndef LUMEX_TRANSPORT_LOG_MIN_LEVEL
#define LUMEX_TRANSPORT_LOG_MIN_LEVEL 0
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace log
{
/// @brief The level of a record; `off` as a threshold disables the log.
enum class log_level : int
{
  trace = 0,
  debug = 1,
  info = 2,
  warning = 3,
  error = 4,
  off = 5
};

/// @brief The bytes a record may carry.
typedef lumex::core::span::view::span<std::uint8_t const> log_bytes_t;

/// @brief One log record; every pointer is valid only during the sink call.
struct log_record_t
{
  log_level level = log_level::info;
  /// Where it comes from: "transport.serial", "transport.tcp", ...
  char const *component = "";
  /// Which transport: "COM3", "192.0.2.10:4001"; may be empty.
  char const *instance = "";
  /// The text.
  char const *message = "";
  /// Bytes sent or received; may be empty.
  log_bytes_t bytes;
};

/// @brief A log sink.
typedef void (*log_sink_t) (log_record_t const &);

/// @brief Sets the sink of this binary; `nullptr` restores the default sink
/// (`warning` and above to `std::cerr`). Thread-safe.
void set_log_sink (log_sink_t sink) LUMEX_NOEXCEPT;

/// @brief The sink of this binary; `nullptr` when the default sink is used.
log_sink_t get_log_sink () LUMEX_NOEXCEPT;

/// @brief Sets the threshold (default `warning`). Thread-safe.
void set_log_level (log_level level) LUMEX_NOEXCEPT;

/// @brief The threshold.
log_level get_log_level () LUMEX_NOEXCEPT;

/**
 * @brief Restricts the components: a record passes when its component starts
 * with one of `allow` (all pass when `allow` is empty) and with none of
 * `deny`. Two empty lists remove the filter. Thread-safe.
 */
void set_component_filter (std::vector<std::string> const &allow,
                           std::vector<std::string> const &deny);

/// @brief The most bytes a dump shows (default 256); the rest is counted.
void set_dump_limit (std::size_t bytes) LUMEX_NOEXCEPT;

/// @brief The dump limit.
std::size_t get_dump_limit () LUMEX_NOEXCEPT;

/// @brief Shows every byte of a dump, whatever the limit (default false).
void set_dump_unlimited (bool unlimited) LUMEX_NOEXCEPT;

/// @brief True when the dump limit is ignored.
bool is_dump_unlimited () LUMEX_NOEXCEPT;

/**
 * @brief The text of a byte dump: the bytes in hex, then the same bytes
 * between bars as text, control bytes by name (`<CR>`, `<LF>`, `<SUB>`,
 * `<ACK>`, `<NAK>`, `<STX>`, `<ETX>`), other unprintable bytes as `.`; past
 * the limit ` (+N B)`. Example: `41 0D 0A |A<CR><LF>|`.
 */
std::string format_dump (log_bytes_t bytes);

/// @brief True when a record of this level and component would be written.
bool log_enabled (log_level level, char const *component) LUMEX_NOEXCEPT;

/// @brief Writes a record to the sink (the caller checked `log_enabled`).
void log_write (log_level level, char const *component, char const *instance,
                std::string const &message,
                log_bytes_t bytes = log_bytes_t ());
} // namespace log
} // namespace transport
} // namespace applied
} // namespace lumex

/**
 * @brief Writes a transport log record. `message` (a `std::string`
 * expression) is evaluated only when the level is compiled in, reaches the
 * threshold and the component passes the filter.
 * @code
 * LUMEX_TRANSPORT_LOG (warning, "transport.serial", name_.c_str (),
 *                      "read timed out after " + std::to_string (ms) + " ms",
 *                      log_bytes_t ());
 * @endcode
 */
#define LUMEX_TRANSPORT_LOG(level, component, instance, message, bytes)       \
  do                                                                          \
    {                                                                         \
      if (static_cast<int> (                                                  \
              ::lumex::applied::transport::log::log_level::level)             \
              >= LUMEX_TRANSPORT_LOG_MIN_LEVEL                                \
          && ::lumex::applied::transport::log::log_enabled (                  \
              ::lumex::applied::transport::log::log_level::level,             \
              (component)))                                                   \
        ::lumex::applied::transport::log::log_write (                         \
            ::lumex::applied::transport::log::log_level::level, (component),  \
            (instance), (message), (bytes));                                  \
    }                                                                         \
  while (false)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_LOG_LOG_HPP

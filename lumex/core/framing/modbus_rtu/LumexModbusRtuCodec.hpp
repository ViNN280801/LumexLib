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
 * @file LumexModbusRtuCodec.hpp
 * @brief `modbus_rtu_codec`: Modbus RTU frames found in a byte stream by
 * their function code.
 * @details A frame is `[address][function][data][CRC-16/MODBUS]`; the
 * payload of `encode` and of the sink is the frame without the CRC. Modbus
 * RTU marks the end of a frame with a silent interval, which a byte stream
 * (TCP, a buffered serial port) does not keep, so the decoder takes the length
 * from the function code: a table gives, per function, either a fixed frame
 * size or the offset of a byte count (`modbus_rtu_responses ()` for the master
 * side, `modbus_rtu_requests ()` for the device side). An exception reply
 * (function | 0x80) is always 5 bytes.
 *
 * The CRC goes low byte first, as the Modbus specification says;
 * `crc_order = byte_order::big` reads and writes it high byte first, as some
 * devices do. A function missing from the table is `bad_header`, a frame
 * longer than `max_frame_size` (default 256, the Modbus maximum)
 * `frame_too_large`, a wrong CRC `bad_checksum`; after each the decoder drops
 * one byte and tries again. A frame for another address is dropped without a
 * report when `filter_address` is set.
 */
#ifndef LUMEX_CORE_FRAMING_MODBUS_RTU_CODEC_HPP
#define LUMEX_CORE_FRAMING_MODBUS_RTU_CODEC_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own push_back, insert or reserve (a false positive: only references into
// the container are invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <cstddef>
#include <cstdint>
#include <vector>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/crc/LumexCrc"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace modbus_rtu
{
namespace Constants
{
/// @brief The largest Modbus RTU frame (application data unit).
LUMEX_CONSTEXPR std::size_t KMAX_ADU_SIZE = 256;
/// @brief The bit of the function code that marks an exception reply.
LUMEX_CONSTEXPR codec::byte_t KEXCEPTION_BIT = 0x80;
/// @brief The size of an exception reply: address, function, code, CRC.
LUMEX_CONSTEXPR std::size_t KEXCEPTION_SIZE = 5;
} // namespace Constants

/// @brief How long a frame of one function code is.
struct modbus_function_t
{
  /// The function code.
  codec::byte_t code = 0;
  /// The whole frame size with the CRC, or 0 when a byte count follows.
  std::size_t fixed_size = 0;
  /// Where the byte count is (from the address byte) when `fixed_size` is 0;
  /// the frame is then `count_offset + 1 + count + 2` bytes.
  std::size_t count_offset = 2;
};

/// @brief The function table of the master side: the replies to functions
/// 1-6, 15 and 16.
inline std::vector<modbus_function_t>
modbus_rtu_responses ()
{
  std::vector<modbus_function_t> table;
  codec::byte_t const counted[] = { 0x01, 0x02, 0x03, 0x04 };
  codec::byte_t const fixed[] = { 0x05, 0x06, 0x0F, 0x10 };
  for (codec::byte_t code : counted)
    {
      modbus_function_t entry;
      entry.code = code;
      entry.count_offset = 2;
      table.push_back (entry);
    }
  for (codec::byte_t code : fixed)
    {
      modbus_function_t entry;
      entry.code = code;
      entry.fixed_size = 8;
      table.push_back (entry);
    }
  return table;
}

/// @brief The function table of the device side: the requests of functions
/// 1-6, 15 and 16.
inline std::vector<modbus_function_t>
modbus_rtu_requests ()
{
  std::vector<modbus_function_t> table;
  codec::byte_t const fixed[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
  codec::byte_t const counted[] = { 0x0F, 0x10 };
  for (codec::byte_t code : fixed)
    {
      modbus_function_t entry;
      entry.code = code;
      entry.fixed_size = 8;
      table.push_back (entry);
    }
  for (codec::byte_t code : counted)
    {
      modbus_function_t entry;
      entry.code = code;
      entry.count_offset = 6;
      table.push_back (entry);
    }
  return table;
}

/// @brief The options of `modbus_rtu_codec`.
struct modbus_rtu_options_t : codec::codec_options_t
{
  modbus_rtu_options_t () { max_frame_size = Constants::KMAX_ADU_SIZE; }

  /// The length rules per function code.
  std::vector<modbus_function_t> functions = modbus_rtu_responses ();
  /// The byte order of the CRC on the wire.
  codec::byte_order crc_order = codec::byte_order::little;
  /// Deliver only the frames of `address`.
  bool filter_address = false;
  /// The address `filter_address` accepts.
  codec::byte_t address = 0;
};

/// @brief Modbus RTU framing (see the file description).
class modbus_rtu_codec
{
public:
  using options_type = modbus_rtu_options_t;

  /// @pre `options.max_frame_size >= 5` (the shortest frame).
  explicit modbus_rtu_codec (options_type const &options = options_type ())
      : options_ (options)
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size
                           >= Constants::KEXCEPTION_SIZE);
  }

  /// @brief Appends the payload (address, function, data) and its CRC.
  /// @return `truncated` for a payload of less than two bytes,
  /// `payload_too_large` for a frame above `max_frame_size`.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    if (payload.size () < 2)
      return codec::encode_failure (codec::framing_errc::truncated);
    if (payload.size () + 2 > options_.max_frame_size)
      return codec::encode_failure (codec::framing_errc::payload_too_large);
    out.insert (out.end (), payload.begin (), payload.end ());
    codec::detail::put_uint (crc_of (payload), 2, options_.crc_order, out);
    return codec::encode_result_t ();
  }

  /**
   * @brief Consumes a chunk of the stream and delivers every frame it
   * completes.
   * @return `chunk.size ()`: the whole chunk is always consumed.
   */
  template <typename Sink>
  std::size_t
  decode (codec::bytes_view_t chunk, Sink &&sink)
  {
    buffer_.insert (buffer_.end (), chunk.begin (), chunk.end ());
    while (next_frame (sink))
      {
      }
    return chunk.size ();
  }

  /// @brief Forgets the received bytes.
  void
  reset () LUMEX_NOEXCEPT
  {
    buffer_.clear ();
  }

private:
  static std::uint64_t
  crc_of (codec::bytes_view_t data) LUMEX_NOEXCEPT
  {
    return lumex::core::crc::parametric::crc_parametric<
        lumex::core::crc::parametric::crc16_modbus_spec_t>::
        calculate (lumex::core::span::view::span<std::uint8_t const> (
            data.data (), data.size ()));
  }

  modbus_function_t const *
  find (codec::byte_t code) const LUMEX_NOEXCEPT
  {
    for (modbus_function_t const &entry : options_.functions)
      if (entry.code == code)
        return &entry;
    return nullptr;
  }

  template <typename Sink>
  bool
  reject (codec::framing_errc error, Sink &sink)
  {
    codec::detail::deliver (sink, error);
    buffer_.erase (buffer_.begin ());
    return true;
  }

  // The size of the frame at the front, 0 when more bytes are needed to know
  // it, or a rejection.
  template <typename Sink>
  bool
  next_frame (Sink &sink)
  {
    if (buffer_.size () < 2)
      return false;
    codec::byte_t const function = buffer_[1];
    std::size_t size = 0;
    if ((function & Constants::KEXCEPTION_BIT) != 0)
      size = Constants::KEXCEPTION_SIZE;
    else
      {
        modbus_function_t const *const entry = find (function);
        if (entry == nullptr)
          return reject (codec::framing_errc::bad_header, sink);
        if (entry->fixed_size != 0)
          size = entry->fixed_size;
        else
          {
            if (buffer_.size () <= entry->count_offset)
              return false;
            size = entry->count_offset + 1 + buffer_[entry->count_offset] + 2;
          }
      }
    if (size > options_.max_frame_size)
      return reject (codec::framing_errc::frame_too_large, sink);
    if (buffer_.size () < size)
      return false;

    codec::bytes_view_t const body (buffer_.data (), size - 2);
    if (codec::detail::get_uint (buffer_.data () + size - 2, 2,
                                 options_.crc_order)
        != crc_of (body))
      return reject (codec::framing_errc::bad_checksum, sink);

    if (!options_.filter_address || buffer_[0] == options_.address)
      {
        frame_.assign (buffer_.begin (),
                       buffer_.begin ()
                           + static_cast<std::ptrdiff_t> (size - 2));
        codec::detail::deliver (
            sink, codec::bytes_view_t (frame_.data (), frame_.size ()));
      }
    buffer_.erase (buffer_.begin (),
                   buffer_.begin () + static_cast<std::ptrdiff_t> (size));
    return true;
  }

  options_type options_;
  codec::bytes_t buffer_;
  codec::bytes_t frame_;
};
} // namespace modbus_rtu
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_MODBUS_RTU_CODEC_HPP

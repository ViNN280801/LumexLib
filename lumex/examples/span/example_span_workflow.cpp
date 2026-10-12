#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::span;

// A workflow: decode frames from one received byte buffer without copying.
// Frame layout (little-endian): magic u16 = 0xA55A, channel u8, count u8,
// count samples of u16, checksum u8 (the sum of every earlier byte).
// Every step below passes a span of the same buffer; a frame is parsed from
// the bytes it covers, and the rest of the buffer is handed on.
namespace
{
typedef span<std::uint8_t const> bytes_view;

struct frame_t
{
  std::uint8_t channel;
  std::vector<std::uint16_t> samples;
};

std::uint8_t
checksum (bytes_view bytes)
{
  std::uint8_t sum = 0;
  for (std::uint8_t value : bytes)
    {
      sum = static_cast<std::uint8_t> (sum + value);
    }
  return sum;
}

std::uint16_t
read_u16 (bytes_view bytes)
{
  return static_cast<std::uint16_t> (bytes[0] | (bytes[1] << 8));
}

// Parses one frame at the start of `input`. On success returns the number of
// bytes it used, and 0 when the bytes do not hold a whole valid frame.
std::size_t
parse_frame (bytes_view input, frame_t &frame)
{
  if (input.size () < 5 || read_u16 (input.first (2)) != 0xA55A)
    {
      return 0;
    }
  bytes_view const header = input.first (4);
  std::size_t const count = header[3];
  std::size_t const length = header.size () + count * 2 + 1;
  if (input.size () < length)
    {
      return 0;
    }
  bytes_view const body = input.subspan (header.size (), count * 2);
  if (checksum (input.first (length - 1)) != input[length - 1])
    {
      return 0;
    }
  frame.channel = header[2];
  frame.samples.clear ();
  for (std::size_t index = 0; index < count; ++index)
    {
      frame.samples.push_back (read_u16 (body.subspan (index * 2, 2)));
    }
  return length;
}

// Appends one frame to the buffer.
void
append_frame (std::vector<std::uint8_t> &buffer, std::uint8_t channel,
              std::vector<std::uint16_t> const &samples)
{
  std::size_t const begin = buffer.size ();
  buffer.push_back (0x5A);
  buffer.push_back (0xA5);
  buffer.push_back (channel);
  buffer.push_back (static_cast<std::uint8_t> (samples.size ()));
  for (std::uint16_t sample : samples)
    {
      buffer.push_back (static_cast<std::uint8_t> (sample & 0xFF));
      buffer.push_back (static_cast<std::uint8_t> (sample >> 8));
    }
  buffer.push_back (checksum (bytes_view (buffer).subspan (begin)));
}

// Mean of every window of `width` samples, sliding by one: each window is a
// subview, no sample is copied.
std::vector<double>
moving_average (span<std::uint16_t const> samples, std::size_t width)
{
  std::vector<double> averages;
  for (std::size_t first = 0; first + width <= samples.size (); ++first)
    {
      double sum = 0;
      for (std::uint16_t value : samples.subspan (first, width))
        {
          sum += value;
        }
      averages.push_back (sum / static_cast<double> (width));
    }
  return averages;
}

// Splits a large run of samples into blocks of at most `block` and reports
// the maximum of each; the last block may be shorter.
void
report_block_maxima (span<std::uint16_t const> samples, std::size_t block)
{
  std::size_t index = 0;
  while (!samples.empty ())
    {
      span<std::uint16_t const> const part
          = samples.first (samples.size () < block ? samples.size () : block);
      std::uint16_t maximum = part.front ();
      for (std::uint16_t value : part)
        {
          maximum = value > maximum ? value : maximum;
        }
      std::cout << "  block " << index++ << " (" << part.size ()
                << " samples) max=" << maximum << '\n';
      samples = samples.subspan (part.size ());
    }
}
} // namespace

int
main ()
{
  std::cout << "=== span workflow: decoding frames from one buffer ===\n\n";

  std::cout << "--- 1. Build a receive buffer with three frames ---\n";
  std::vector<std::uint8_t> buffer;
  append_frame (buffer, 1, std::vector<std::uint16_t> (3, 100));
  append_frame (buffer, 2,
                std::vector<std::uint16_t>{ 10, 20, 30, 40, 50, 60 });
  append_frame (buffer, 3, std::vector<std::uint16_t>{ 7 });
  buffer.push_back (0x00); // trailing noise
  std::cout << "bytes received: " << buffer.size () << '\n';

  std::cout << "\n--- 2. Walk the buffer frame by frame ---\n";
  bytes_view rest (buffer);
  frame_t frame = { 0, std::vector<std::uint16_t> () };
  std::size_t frames = 0;
  std::vector<std::uint16_t> channel_two;
  while (std::size_t const used = parse_frame (rest, frame))
    {
      std::cout << "frame " << frames++ << ": channel=" << int (frame.channel)
                << " samples=" << frame.samples.size () << " used=" << used
                << " bytes\n";
      if (frame.channel == 2)
        {
          channel_two = frame.samples;
        }
      rest = rest.subspan (used);
    }
  std::cout << "bytes left over: " << rest.size () << " (not a whole frame)\n";

  std::cout << "\n--- 3. A corrupted frame is rejected ---\n";
  std::vector<std::uint8_t> damaged (buffer.begin (), buffer.begin () + 11);
  damaged[5] ^= 0xFF;
  std::cout << "parse of the damaged frame returns "
            << parse_frame (bytes_view (damaged), frame) << '\n';

  std::cout << "\n--- 4. Windows over the samples of channel 2 ---\n";
  std::vector<double> const averages
      = moving_average (span<std::uint16_t const> (channel_two), 3);
  for (std::size_t index = 0; index < averages.size (); ++index)
    {
      std::cout << "  window " << index << " mean=" << averages[index] << '\n';
    }

  std::cout << "\n--- 5. Blocks of a long run ---\n";
  std::vector<std::uint16_t> run;
  for (std::uint16_t value = 0; value < 10; ++value)
    {
      run.push_back (static_cast<std::uint16_t> ((value * 37) % 11));
    }
  report_block_maxima (span<std::uint16_t const> (run), 4);

  std::cout << "\n=== Span workflow finished ===\n";
  return 0;
}

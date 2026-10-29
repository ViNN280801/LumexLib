// The encoders of whole messages and the key writers
// (lumex/core/protocol/layout/LumexProtocolMessage.hpp,
// index/LumexProtocolKeyReader.hpp) leave the buffer unchanged when an
// allocation fails, at every allocation they make. This test binary replaces
// the global operator new: it fails with std::bad_alloc after the number of
// allocations the test allows and passes every allocation through otherwise.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
// -1: every allocation succeeds; n >= 0: n more succeed, then one fails.
int g_allowed_allocations = -1;
} // namespace

void *
operator new (std::size_t size)
{
  if (g_allowed_allocations == 0)
    throw std::bad_alloc ();
  if (g_allowed_allocations > 0)
    --g_allowed_allocations;
  void *const memory = std::malloc (size == 0 ? 1 : size);
  if (memory == nullptr)
    throw std::bad_alloc ();
  return memory;
}

void
operator delete (void *memory) noexcept
{
  std::free (memory);
}

void
operator delete (void *memory, std::size_t) noexcept
{
  std::free (memory);
}

namespace alloc_tables
{
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;

struct codec
{
};

// Three header bytes, then a key of two bytes.
struct header
{
  using key_reader = index::key_at<3, 2>;
};

struct text_header
{
  using layout_type = layout::tokenized_text_layout<','>;
};

// clang-format off
#define ALLOC_ROWS(X, X_AS) \
  X (SET, 0x0121, "Set", (u16_be_, u8_, u32_le_), ("a", "b", "c"), (u32_be_), \
     ())
#define NO_ROWS(X, X_AS)
// clang-format on
LUMEX_PROTOCOL_DEFINE (alloc_protocol, codec, header, ALLOC_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (alloc_text, alloc_protocol, codec, text_header,
                              NO_ROWS);
} // namespace alloc_tables

namespace
{
namespace tables = alloc_tables;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;

using set_row = tables::alloc_protocol::SET_t;

// A buffer of one byte without spare capacity: the next byte allocates.
layout::bytes_t
tight_buffer ()
{
  layout::bytes_t out (1, 0xEE);
  out.shrink_to_fit ();
  return out;
}

// Runs `encode` with 0, 1, 2, ... allocations allowed until it no longer
// fails; after every failure the buffer must be as it was. Returns the number
// of failures seen.
template <typename Encode>
int
failures_leave_the_buffer (Encode encode)
{
  int failures = 0;
  for (int allowed = 0; allowed < 32; ++allowed)
    {
      layout::bytes_t out = tight_buffer ();
      bool threw = false;
      g_allowed_allocations = allowed;
      try
        {
          encode (out);
        }
      catch (std::bad_alloc const &)
        {
          threw = true;
        }
      g_allowed_allocations = -1;
      if (!threw)
        return failures;
      ++failures;
      EXPECT_EQ (out, (layout::bytes_t{ 0xEE }))
          << "after " << allowed << " allocations";
    }
  ADD_FAILURE () << "the encoder never finished";
  return failures;
}

void
encode_request (layout::bytes_t &out)
{
  EXPECT_TRUE (
      (layout::encode_message<tables::alloc_protocol, set_row> (out, 1, 2, 3)
           .has_value ()));
}

void
encode_reply (layout::bytes_t &out)
{
  EXPECT_TRUE (
      (layout::encode_reply_message<tables::alloc_protocol, set_row> (out, 7u)
           .has_value ()));
}

void
encode_text_request (layout::bytes_t &out)
{
  EXPECT_TRUE (
      (layout::encode_message<tables::alloc_text, set_row> (out, 1, 2, 3)
           .has_value ()));
}

void
write_key (layout::bytes_t &out)
{
  dsl::key_value_t const key
      = { dsl::key_kind::integer, 2, 0x0121, nullptr, 0 };
  EXPECT_TRUE ((index::key_at<3, 2>::write (out, key)));
}

void
write_digits (layout::bytes_t &out)
{
  dsl::key_value_t const key
      = { dsl::key_kind::integer, 2, 0x0121, nullptr, 0 };
  EXPECT_TRUE ((index::key_at_digits<3, 4, 16>::write (out, key)));
}
} // namespace

TEST (
    LumexProtocolMessageAllocTest,
    GivenAnAllocationThatFails_WhenARequestOrAReplyMessageIsEncoded_ThenTheBufferIsUnchanged)
{
  EXPECT_GE (failures_leave_the_buffer (&encode_request), 2);
  EXPECT_GE (failures_leave_the_buffer (&encode_reply), 2);
  EXPECT_GE (failures_leave_the_buffer (&encode_text_request), 2);
}

TEST (LumexProtocolMessageAllocTest,
      GivenAnAllocationThatFails_WhenAKeyIsWritten_ThenTheBufferIsUnchanged)
{
  EXPECT_GE (failures_leave_the_buffer (&write_key), 1);
  EXPECT_GE (failures_leave_the_buffer (&write_digits), 1);
}

TEST (LumexProtocolMessageAllocTest,
      GivenNoFailingAllocation_WhenEncoded_ThenTheMessageIsAppended)
{
  layout::bytes_t out = tight_buffer ();
  encode_request (out);
  EXPECT_EQ (out, (layout::bytes_t{ 0xEE, 0x00, 0x00, 0x00, 0x01, 0x21, 0x00,
                                    0x01, 0x02, 0x03, 0x00, 0x00, 0x00 }));
}

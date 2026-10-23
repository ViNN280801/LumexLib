// The umbrella of the protocol module (lumex/core/protocol/LumexProtocol):
// one include gives the table macros, the vocabulary, the entries and the
// error codes, and a table written with them works end to end.

#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace dsl = lumex::core::protocol::dsl;

struct codec
{
};

struct header
{
};

// A synthetic two-row table of an invented device.
#define UMBRELLA_ROWS(X, X_AS)                                                \
  X (HELLO, 0x10, "Greeting", (u8_<>), ("version"), (), (idempotent))         \
  X_AS (BYE, "bye!", 0x11, "Farewell", (), (), (), (timeout (50)))

LUMEX_PROTOCOL_DEFINE (umbrella_protocol, codec, header, UMBRELLA_ROWS);
} // namespace

TEST (LumexProtocolTest,
      GivenTheUmbrella_WhenATableIsDefined_ThenTypesEntriesAndErrorsWork)
{
  EXPECT_EQ (dsl::command_count<umbrella_protocol>::value, 2u);
  auto const entries = dsl::command_entries<umbrella_protocol> ();
  ASSERT_EQ (entries.size (), 2u);
  EXPECT_EQ (std::string (entries[1].specification_name), "bye!");
  EXPECT_EQ (entries[1].options.timeout_ms, 50u);
  std::error_code const code
      = lumex::core::protocol::error::protocol_errc::unknown_key;
  EXPECT_STREQ (code.category ().name (), "lumex.protocol");
}

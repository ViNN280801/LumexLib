// Every macro, option, field and query of lumex/core/protocol in one
// translation unit of a strict consumer: a synthetic table of an invented
// device (no real protocol) with a layout, a layer, a framed layer, every row
// option, the entries, the client methods and the error codes. Exit code 0
// when every value read back is the one the table wrote.

#include <cstdio>
#include <ratio>
#include <string>
#include <system_error>

#include "lumex/core/protocol/LumexProtocol"

namespace fixture
{
namespace dsl = lumex::core::protocol::dsl;

enum class mode
{
  idle,
  busy
};

struct codec
{
};

struct other_codec
{
};

struct header
{
};

LUMEX_PROTOCOL_LAYOUT (state_t,
                       (u8_<>, u16_le_<scale<std::ratio<1, 10>>>, u16_be_<>),
                       ("mode", "level", "counter"));

// clang-format off
#define FIXTURE_ROWS(X, X_AS)                                                 \
  X (PING, 0x01, "Liveness", (), (), (), (idempotent))                        \
  X (SET_LEVEL, 0x02, "Set the level",                                        \
     (u16_le_<scale<std::ratio<1, 100> > >, u8_<>), ("level", "channel"),     \
     (state_t),                                                               \
     (timeout (100), retries (2), accepted_in (mode::idle, mode::busy),       \
      log_level (level::info)))                                               \
  X_AS (ERASE, "DELETE", 0x03, "Erase", (), (), (state_t),                    \
        (dangerous, firmware (1, 9)))                                         \
  X_AS (READ_NAME, "read-name", "RN", "Read the name", (), (), (),            \
        (model ("A", "B"), deprecated))                                       \
  X (STREAM, 'S', "Stream", (), (), (state_t), (stream (50)))

#define FIXTURE_LAYER_ROWS(X, X_AS)                                           \
  X (SET_LEVEL, 0x12, "Set the level (wide)", (u16_be_<>), ("level"),         \
     (state_t), ())                                                           \
  X (RESET, 0x20, "Reset", (), (), (), (dangerous))
// clang-format on

LUMEX_PROTOCOL_DEFINE (fixture_protocol, codec, header, FIXTURE_ROWS);
LUMEX_PROTOCOL_EXTEND (fixture_layer, fixture_protocol, FIXTURE_LAYER_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (fixture_framed, fixture_layer, other_codec,
                              header, FIXTURE_LAYER_ROWS);
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (FIXTURE_ROWS);

struct client : fixture_layer::lumex_methods<client>
{
  template <typename Row, typename... Args>
  int
  lumex_invoke (Args... args)
  {
    int sum = static_cast<int> (dsl::make_entry<Row> ().key.integer);
    int const values[] = { 0, args... };
    for (int value : values)
      sum += value;
    return sum;
  }
};

namespace
{
// A failed check counts one; a function, so that the compiler does not see
// a check whose result is known at compile time as dead code.
int
failed (bool ok)
{
  return ok ? 0 : 1;
}

int
check ()
{
  int failures = 0;
  typedef fixture_protocol::SET_LEVEL_t set_level;
  failures += failed (
      dsl::get_option<dsl::kind::timeout> (set_level::options ()).milliseconds
      == 100);
  failures += failed (
      dsl::get_option<dsl::kind::retries> (set_level::options ()).count == 2);
  failures += failed (
      dsl::get_option<dsl::kind::accepted_in> (set_level::options ()).states[1]
      == mode::busy);
  failures += failed (
      dsl::get_option<dsl::kind::log_level> (set_level::options ()).value
      == dsl::level::info);
  failures += failed ((dsl::flags_of (fixture_protocol::ERASE_t::options ())
                       & dsl::row_flags::dangerous)
                      != 0);
  failures += failed (!dsl::has_option<dsl::kind::stream> (
      fixture_protocol::PING_t::options ()));
  failures
      += failed (std::string (fixture_protocol::ERASE_t::specification_name ())
                 == "DELETE");
  failures += failed (fixture_protocol::READ_NAME_t::key ().size == 2);
  failures += failed (std::string (state_t::field_names ()[2]) == "counter");

  failures += failed (dsl::command_count<fixture_layer>::value == 6
                      && dsl::command_count<fixture_framed>::value == 6);
  lumex::core::span::view::span<dsl::command_entry_t const> const entries
      = dsl::command_entries<fixture_layer> ();
  failures
      += failed (entries.size () == 6 && entries[1].key.integer == 0x12
                 && std::string (entries[5].identifier) == "RESET"
                 && entries[3].options.models.size () == 2
                 && entries[0].options.flags == dsl::row_flags::idempotent);

  client user;
  failures += failed (user.PING () == 0x01 && user.SET_LEVEL (3) == 0x12 + 3
                      && user.RESET () == 0x20);

  std::error_code const code
      = lumex::core::protocol::error::protocol_errc::truncated;
  failures += failed (!code.message ().empty ());
  return failures;
}
} // namespace
} // namespace fixture

int
main ()
{
  int const failures = fixture::check ();
  std::printf ("protocol_warnings: %d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
